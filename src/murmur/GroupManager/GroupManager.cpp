/* Copyright (C) 2010-present, Zhenkai Zhu <zhenkai@cs.ucla.edu>, Yang Xu <yangxu.5661@yahoo.com.cn>

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  - Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  - Neither the name of the Mumble Developers nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <cstring>
#include <ctime>
#include <QString>
#include "murmur_pch.h"
#include "GroupManager.h"
#include "Server.h"
#include <QSettings>
#include "debugbox.h"
#include <poll.h>
#include <pthread.h>

#define  BROADCAST_PREFIX ("/ndn/broadcast/conference")

#define DEBUG

#define FRESHNESS 10 

#ifdef DEBUG
#define DPRINT(fmt, ...)                                          \
                                                                 \
   fprintf(stderr, "[%s:%d:%s] ", __FILE__, __LINE__, __func__); \
   fprintf(stderr, fmt, ##__VA_ARGS__);                          \
   fprintf(stderr, "\n");                                        \

#else
#define DPRINT(fmt, ...)                                          \
                                                                 \
   fprintf(stderr, fmt, ##__VA_ARGS__);                          \
   fprintf(stderr, "\n");                                        \

#endif

static pthread_mutex_t gm_mutex;
static pthread_mutexattr_t attr;
static pollfd pfds[1];

static int namecompare(const void *a, const void *b);
static void append_bf_all(struct ccn_charbuf *c);
/*
 * This appends a tagged, valid, fully-saturated Bloom filter, useful for
 * excluding everything between two 'fenceposts' in an Exclude construct.
 */
static void
append_bf_all(struct ccn_charbuf *c)
{
    unsigned char bf_all[9] = { 3, 1, 'A', 0, 0, 0, 0, 0, 0xFF };
    const struct ccn_bloom_wire *b = ccn_bloom_validate_wire(bf_all, sizeof(bf_all));
    if (b == NULL) abort();
    ccn_charbuf_append_tt(c, CCN_DTAG_Bloom, CCN_DTAG);
    ccn_charbuf_append_tt(c, sizeof(bf_all), CCN_BLOB);
    ccn_charbuf_append(c, bf_all, sizeof(bf_all));
    ccn_charbuf_append_closer(c);
}

static ccn_keystore *cached_keystore = NULL;
static GroupManager *pGroupManager = NULL;

static enum ccn_upcall_res incoming_content(ccn_closure *selfp, 
                           ccn_upcall_kind kind, 
                           ccn_upcall_info *info);
static enum ccn_upcall_res incoming_interest(ccn_closure *selfp, 
                           ccn_upcall_kind kind, 
                           ccn_upcall_info *info);
static enum ccn_upcall_res handle_leave(ccn_closure *selfp,
										ccn_upcall_kind kind,
										ccn_upcall_info *info);

static char *ccn_name_comp_to_str(const unsigned char *ccnb,
								  const struct ccn_indexbuf *comps,
								  int index);

static void init_cached_keystore();
static const ccn_pkey *get_my_private_key();
static const ccn_pkey *get_my_public_key();
static const unsigned char *get_my_publisher_key_id();
static ssize_t get_my_publisher_key_id_length();

static int ccn_create_keylocator(ccn_charbuf *c, const ccn_pkey *k);

static void mutex_trylock() {
	int c = 0;
	while(pthread_mutex_trylock(&gm_mutex) != 0) {
		usleep(200);
		c++;
		if (c> 10000) {
			fprintf(stderr, "cannot obtain lock %s: %d \n", __FILE__, __LINE__);
			abort();
		}
	}
}

static void mutex_unlock() {
	pthread_mutex_unlock(&gm_mutex);
}

/*
 * Comparison operator for sorting the excl list with qsort.
 * For convenience, the items in the excl array are
 * charbufs containing ccnb-encoded Names of one component each.
 * (This is not the most efficient representation.)
 */
static int /* for qsort */
namecompare(const void *a, const void *b)
{
    const struct ccn_charbuf *aa = *(const struct ccn_charbuf **)a;
    const struct ccn_charbuf *bb = *(const struct ccn_charbuf **)b;
    int ans = ccn_compare_names(aa->buf, aa->length, bb->buf, bb->length);
    if (ans == 0)
        fprintf(stderr, "wassat? %d\n", __LINE__);
    return (ans);
}

static char *ccn_name_comp_to_str(const unsigned char *ccnb,
								  const struct ccn_indexbuf *comps,
								  int index) {
	size_t comp_size;
	const unsigned char *comp_ptr;
	char *str;
	if (ccn_name_comp_get(ccnb, comps, index, &comp_ptr, &comp_size) == 0) {
		str = (char *)malloc(sizeof(char) *(comp_size + 1));
		strncpy(str, (const char *)comp_ptr, comp_size);
		str[comp_size] = '\0';
		return str;
	}
	else {
		debug("can not get name comp");
		return NULL;
	}
}


static void init_cached_keystore() {
    ccn_keystore *keystore = cached_keystore;
    int res;
    if (keystore == NULL) {
	ccn_charbuf *temp = ccn_charbuf_create();
	keystore = ccn_keystore_create();
	ccn_charbuf_putf(temp, "%s/.ccnx/.ccnx_keystore", getenv("HOME"));
	res = ccn_keystore_init(keystore,
				ccn_charbuf_as_string(temp),
				(char *)"Th1s1sn0t8g00dp8ssw0rd.");
	if (res != 0) {
	    printf("Failed to initialize keystore %s\n", ccn_charbuf_as_string(temp));
	    exit(1);
	}
	ccn_charbuf_destroy(&temp);
	cached_keystore = keystore;
    }
}


GroupManager::GroupManager(NdnMediaProcess *pNdnMediaPro) {

	ruMutex = new QMutex(QMutex::Recursive);
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&gm_mutex, &attr);
    pGroupManager = this;

    this->pNdnMediaPro = pNdnMediaPro;

	QDomDocument settings;
	QString configFileName = QDir::homePath() + "/" + \
							".actd" + "/" + ".config";
	QFile config(configFileName);
	if (!config.exists()) 
		critical("Config file does not exist!");
	
	if (!config.open(QIODevice::ReadOnly))
		critical("Can not open config file!");
	
	if (!settings.setContent(&config)) {
		config.close();
		critical("can not parse config file!");
	}
	config.close();
	
	QDomElement docElem = settings.documentElement(); //<config>
	QDomNode n = docElem.firstChild();

	isPrivate = false;
	QString qsConfKey;
	QString qsSessionKey;
	QByteArray qbaSK;
	while(!n.isNull()) {
		if (n.nodeName() == "prefix") {
			prefix = n.toElement().text();
		} else if (n.nodeName() == "confName") {
			confName = n.toElement().text();
		} else if (n.nodeName() == "channelName") {
		} else if (n.nodeName() == "private") {
			QString p = n.toElement().text();
			if (p == "true")
				isPrivate = true;
			else
				isPrivate = false;
		} else if (n.nodeName() == "confKey" && isPrivate) {
			qsConfKey = n.toElement().text();
			QByteArray qbaCK = \
				QByteArray::fromBase64(qsConfKey.toLocal8Bit()); 
			memcpy(conferenceKey, qbaCK.data(), qbaCK.size());
		} else if (n.nodeName() == "sessionKey" && isPrivate) {
			qsSessionKey = n.toElement().text();
			qbaSK = \
				QByteArray::fromBase64(qsSessionKey.toLocal8Bit());

		} else {
			debug("unknown atrribute in config file!");
		}
		n = n.nextSibling();

	}

	if (confName.isEmpty())
		critical("no confName in config!");
	
	if (isPrivate && (qsConfKey.isEmpty() || qsSessionKey.isEmpty()))
		isPrivate = false;
	
	if (isPrivate) {
		pNdnMediaPro->setPrivate();
		pNdnMediaPro->setSK(qbaSK);
	}

	ndnContext();
	
    localUser = NULL;

    // ui_session id for remoteUsers
	// 5000 at maximum
    for (int i = 5001; i < 10000; ++i)
        qqRSesIds.enqueue(i);

}

GroupManager::~GroupManager() {

	ccn_free();
    pGroupManager = NULL;
    StopThread();
}

void GroupManager::StopThread() {
    bRunning = false;
    if (isRunning()) {
		debug("Ending Userlist Handling thread");
		wait();
    }
}

void GroupManager::StartThread() {
	if (! isRunning()) {
		debug("Starting Userlist Handling thread");
		bRunning = true;

		pfds[0].fd = ccn_get_connection_fd(ccn);
		pfds[0].events = POLLIN;
		start(QThread::HighestPriority);
#ifdef Q_OS_LINUX
		// QThread::HighestPriority == Same as everything else...
		int policy;
		sched_param param;
		if (pthread_getschedparam(pthread_self(), &policy, &param) == 0) {
			if (policy == SCHED_OTHER) {
				policy = SCHED_FIFO;
				param.sched_priority = 1;
				pthread_setschedparam(pthread_self(), policy, &param);
			}
		}
#endif
	}
}

void GroupManager::run() {
    int res = 0;
	res = ccn_run(ccn, 0);
    while (bRunning) {
        if (res >= 0) {
			int ret = poll(pfds, 1, 100);	
			if (ret >= 0) {
				mutex_trylock();
				res = ccn_run(ccn, 0);
				mutex_unlock();
			}
        }
    }
}

void GroupManager::setLocalUser(ServerUser *u) {
    localUser = u;
	userName = u->qsName;
    QString temp = prefix + "/" + userName;
    pNdnMediaPro->addLocalUser(temp);

    enumTimer = new QTimer(this);
    connect(enumTimer, SIGNAL(timeout()), this, SLOT(enumerate()));  
	enumTimer->start(4000);

	aliveTimer = new QTimer(this);
	connect(aliveTimer, SIGNAL(timeout()), this, SLOT(checkAlive()));
	aliveTimer->start(25000);

    StartThread();
	//need lock
	enumerate();
	pNdnMediaPro->startThread();

}

int GroupManager::addRemoteUser(QString qPrefix, QString username) {

	QHash<QString, RemoteUser *>::const_iterator it = qhRemoteUsers.find(username);
	if (it != qhRemoteUsers.constEnd()) { // username exists
		RemoteUser *u = it.value();
		u->refreshReceived();
		if (qPrefix != u->getPrefix()) { // user updated prefix
			QString temp = u->getPrefix() + "/" + username;	
			pNdnMediaPro->deleteRemoteUser(temp); // delete old prefix+name in mediapro
			temp =qPrefix + "/" + username;
			pNdnMediaPro->addRemoteUser(temp);
			u->setPrefix(qPrefix);
		}
		return 0;
	}
	
    QString temp =qPrefix + "/" + username;
    pNdnMediaPro->addRemoteUser(temp);

    debug(QString("addRemoteUser(%1, %2)").arg(qPrefix).arg(username));
    RemoteUser *u = new RemoteUser(qPrefix, username);
    u->uiSession = qqRSesIds.dequeue();
    u->cChannel = localUser->cChannel;
    qhRemoteUsers.insert(username, u);
    // fire a signal to notify other modules that a new user joins the call
    emit remoteUserJoin(u);
    return 0;
}


void GroupManager::deleteRemoteUser(QString remoteUser) {
    debug(QString("deleteRemoteUser(%1)").arg(remoteUser));
    RemoteUser *u = NULL;
    u = qhRemoteUsers[remoteUser];
    if (u == NULL) {
        return; 
    }
    qhRemoteUsers.remove(remoteUser);


    QString temp = QString(u->getPrefix()) + "/" + u->qsName;
    pNdnMediaPro->deleteRemoteUser(temp);

    qqRSesIds.enqueue(u->uiSession);
    emit remoteUserLeave(u->uiSession);
    delete u;
}

void GroupManager::userLeft(RemoteUser *ru) {
	QString temp = QString(ru->getPrefix()) + "/" + ru->qsName;
	pNdnMediaPro->deleteRemoteUser(temp);
	qqRSesIds.enqueue(ru->uiSession);
	emit remoteUserLeave(ru->uiSession);
	debug("check alive: remote user left:" + temp);
}

void GroupManager::checkAlive() {
	QHash<QString, RemoteUser *>::iterator i = qhRemoteUsers.begin();
	debug("check alive timer triggered");
	while (i != qhRemoteUsers.end()) {
		RemoteUser * ru = i.value();
		if (ru != NULL && ru->isStaled()) {
			if (!ru->hasLeft()) {
				userLeft(ru);
			}
			delete ru;
			i = qhRemoteUsers.erase(i); 
			debug("check alive: remote user staled:");

		} else {
			debug("check alive: nothing yet");
			i++;
		}

	}
}


static inline void xmlEncoding(QString &qsData, const QString username, const QString qPrefix) {
    qsData.append("<user>");
    qsData.append("<username>");
    qsData.append(username);
    qsData.append("</username>");
    qsData.append("<prefix>");
    qsData.append(qPrefix);
    qsData.append("</prefix>");
    qsData.append("</user>");
}

int GroupManager::userListtoXml(const char **data) {
    QString qsData;
	xmlEncoding(qsData, userName, prefix);
    QByteArray qba = qsData.toLocal8Bit();
    char *buffer = static_cast<char *>(calloc(qba.size() + 1, sizeof(char)));
    memcpy(buffer, qba.constData(), qba.size());
    buffer[qba.size()] = '\0';
	if (isPrivate) {
		unsigned char *enc_data = NULL;
		size_t enc_len = 0;
		int res = symEncrypt(conferenceKey, NULL, (const unsigned char *) buffer, strlen(buffer), &enc_data, &enc_len, AES_BLOCK_SIZE);
		if (res != 0)
			critical("can not encrypt speaker info");
		
		free(buffer);
		buffer = NULL;

		*data = (const char *)enc_data;
		return enc_len;

	} else {
		*data = buffer;
		return strlen(buffer);
	}
}

int GroupManager::parseXmlUserList(const unsigned char *incoming_data, size_t len) {
	unsigned char *list = NULL;
	size_t ulen = 0;
	if (isPrivate) {
		int res = symDecrypt(conferenceKey, NULL, incoming_data, len, &list, &ulen, AES_BLOCK_SIZE);
		if (res != 0 )
			critical("can not decryption speaker info");
	}else {
		list = (unsigned char *) incoming_data;
	}

    QByteArray buffer((const char *)(list));
    QDomDocument doc;
    if (!doc.setContent(buffer)) {
        debug("Cannot convert data to xml\n");
        return -1;
    }
   
    QString username, qPrefix;
    QDomElement docElem = doc.documentElement();  // <user> 
    QDomNode node = docElem.firstChild();  // <username>
    while (!node.isNull()) {
        if (node.nodeName() == "username") {
			username = node.toElement().text();
		} else
        if (node.nodeName() == "prefix") {
			qPrefix = node.toElement().text();
		} else {
			critical("Unknown xml attribute");
		}

        node = node.nextSibling();
    }

	if (username == "" || prefix == "")
		return -1;
	
	if (isPrivate && list != NULL) {
		free(list);
		list = NULL;
	}

	addRemoteUser(qPrefix, username);
    return 0;
}


void GroupManager::ndnContext() { 
    ccn_init();
    ccn_open();
    assert(ccn);
}

int GroupManager::ccn_init() {
    ccn = NULL;
    ccn = ccn_create();
    if (ccn == NULL || ccn_connect(ccn, NULL) == -1) {
        DPRINT("Failed to initialize ccn agent connection");
        return -1;
    }
    req_closure = NULL;
    join_closure = NULL;
    cached_keystore = NULL;
	init_cached_keystore();
	leave_closure = NULL;

	leave_closure = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	leave_closure->p = &handle_leave;

	join_closure = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	join_closure->p = &incoming_content;

	req_closure = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	req_closure->p = &incoming_interest;

    return 0;
}    

int GroupManager::ccn_free() {

    if (ccn != NULL) {
        ccn_disconnect(ccn);
        ccn_destroy(&ccn);
    }
    if (cached_keystore != NULL) ccn_keystore_destroy(&cached_keystore);
    if (join_closure != NULL) {
        free(join_closure);
        join_closure = NULL;
    }
    if (req_closure != NULL) {
        free(req_closure);
        req_closure = NULL;
    }
	if (leave_closure != NULL) {
		free(leave_closure);
		leave_closure = NULL;
	}
    return 0;
}

int GroupManager::ccn_open() {
    ccn_charbuf *interest_filter_path;
    interest_filter_path = ccn_charbuf_create();
    if (interest_filter_path == NULL ) {
        DPRINT("Failed to allocate or initialize interest filter path");
        return -1;
    }
	ccn_name_from_uri(interest_filter_path, (const char *) BROADCAST_PREFIX);
	ccn_name_append_str(interest_filter_path, confName.toLocal8Bit().constData());
	ccn_name_append_str(interest_filter_path, "speaker-list");
    ccn_set_interest_filter(ccn, interest_filter_path, req_closure);
    ccn_charbuf_destroy(&interest_filter_path);

	ccn_charbuf *leave_filter = ccn_charbuf_create();
	ccn_name_from_uri(leave_filter, (const char *) BROADCAST_PREFIX);
	ccn_name_append_str(leave_filter, confName.toLocal8Bit().constData());
	ccn_name_append_str(leave_filter, "leave");
    ccn_set_interest_filter(ccn, leave_filter, leave_closure);
    ccn_charbuf_destroy(&leave_filter);
    return 0;
}

void GroupManager::expressEnumInterest(struct ccn_charbuf *interest, QList<QString> &toExclude) {

	if (toExclude.size() == 0) {
		mutex_trylock();	
		int res = ccn_express_interest(ccn, interest, join_closure, NULL);
		mutex_unlock();
		if (res < 0) {
			critical("express interest failed!");
		}
		ccn_charbuf_destroy(&interest);
		return;
	}

	struct ccn_charbuf **excl = NULL;
	if (toExclude.size() > 0) {
		excl = new ccn_charbuf *[toExclude.size()];
		for (int i = 0; i < toExclude.size(); i ++) {
			QString compName = toExclude.at(i);
			struct ccn_charbuf *comp = ccn_charbuf_create();
			ccn_name_init(comp);
			ccn_name_append_str(comp, compName.toStdString().c_str());
			excl[i] = comp;
			comp = NULL;
		}
		qsort(excl, toExclude.size(), sizeof(excl[0]), &namecompare);
	}

	int begin = 0;
	bool excludeLow = false;
	bool excludeHigh = true;
	while (begin < toExclude.size()) {
		if (begin != 0) {
			excludeLow = true;
		}
		struct ccn_charbuf *templ = ccn_charbuf_create();
		ccn_charbuf_append_tt(templ, CCN_DTAG_Interest, CCN_DTAG); // <Interest>
		ccn_charbuf_append_tt(templ, CCN_DTAG_Name, CCN_DTAG); // <Name>
		ccn_charbuf_append_closer(templ); // </Name> 
		ccn_charbuf_append_tt(templ, CCN_DTAG_Exclude, CCN_DTAG); // <Exclude>
		if (excludeLow) {
			append_bf_all(templ);
		}
		for (; begin < toExclude.size(); begin++) {
			struct ccn_charbuf *comp = excl[begin];
			if (comp->length < 4) abort();
			// we are being conservative here
			if (interest->length + templ->length + comp->length > 1350) {
				break;
			}
			ccn_charbuf_append(templ, comp->buf + 1, comp->length - 2);
		}
		if (begin == toExclude.size()) {
			excludeHigh = false;
		}
		if (excludeHigh) {
			append_bf_all(templ);
		}
		ccn_charbuf_append_closer(templ); // </Exclude>

		ccn_charbuf_append_closer(templ); // </Interest> 
		mutex_trylock();	
		int res = ccn_express_interest(ccn, interest, join_closure, templ);
		mutex_unlock();
		if (res < 0) {
			critical("express interest failed!");
		}
		ccn_charbuf_destroy(&templ);
	}
	ccn_charbuf_destroy(&interest);
	for (int i = 0; i < toExclude.size(); i++) {
		ccn_charbuf_destroy(&excl[i]);
	}
	if (excl != NULL) {
		delete []excl;
	}
}

void GroupManager::enumerate() {
    ccn_charbuf *interest_path = NULL;
    ccn_charbuf *templ = NULL;
    interest_path = ccn_charbuf_create();
    if (interest_path == NULL ) {
        return;
    }
	ccn_name_from_uri(interest_path, BROADCAST_PREFIX);
    ccn_name_append_str(interest_path, confName.toLocal8Bit().constData());
	ccn_name_append_str(interest_path, "speaker-list");

    // update exclusive filter according to recently known remote users
    QHash<QString, RemoteUser *>::const_iterator it = (pGroupManager->qhRemoteUsers).constBegin();
	QList<QString> toExclude;
    while (it != (pGroupManager->qhRemoteUsers).constEnd())
    {
		RemoteUser *ru = it.value();
		if (ru && !ru->needRefresh()) {
			toExclude.append(ru->getName());
		}
		++it;
    }

	// TODO: do not exclude local user (when staleness comes to play)
	// always exclude localuser by now
	toExclude.append(userName);

	expressEnumInterest(interest_path, toExclude);
}

/* send optional leave notification before actually leaves
 */
void GroupManager::sendLeaveInterest() {
	ccn_charbuf *interest_path = ccn_charbuf_create();
	if (interest_path == NULL ) {
		return;
	}
	ccn_name_from_uri(interest_path, (const char*)BROADCAST_PREFIX);
	ccn_name_append_str(interest_path, confName.toLocal8Bit().constData());
	ccn_name_append_str(interest_path, "leave");
	ccn_name_append_str(interest_path, userName.toLocal8Bit().constData());
	static ccn_charbuf *templ = NULL;
	mutex_trylock();
    ccn_express_interest(ccn, interest_path, leave_closure, NULL);
	mutex_unlock();
    ccn_charbuf_destroy(&interest_path);
	templ = NULL;
}

void GroupManager::handleLeave(ccn_upcall_info *info) {
	debug("leave interest received");
	// /ndn/broadcast/conference/conference-name/leave/username
	 char *leaver = NULL;
	 leaver = ccn_name_comp_to_str(info->interest_ccnb, info->interest_comps, 5);
	 // get leaver
	 QString user = leaver;
	 //pGroupManager->deleteRemoteUser(user);
	 RemoteUser *rmUser = qhRemoteUsers[user];
	 if (rmUser == NULL)
		 return;
	 rmUser->setLeft();
	 userLeft(rmUser);
	 debug(QString("remote user %1 left\n").arg(user));
	 if (leaver != NULL) {
		 free((void *)leaver);
		 leaver = NULL;
	 }
}

void GroupManager::incomingInterest(ccn_upcall_info *info) {
    int res;

    const char *data = NULL;
    const unsigned char *requester = NULL;
    const unsigned char *refresher = NULL;
    const unsigned char *filter = NULL;
    size_t filter_len = 0;

    ccn_charbuf *signed_info = NULL;
    ccn_charbuf *name = NULL;
    ccn_charbuf *content = NULL;

    RemoteUser *refreshUser = NULL;

    // requesterPrefix starts from index 4 to (info->interest_comps->n - 2)
    int nameEnd = 0;
	nameEnd = (info->interest_comps)->n - 2;

	/* construct reply data
	 * name format:
	 *   /ndn/broadcast/conference/conference-name/speaker-list/username
	 */
	signed_info = ccn_charbuf_create();
	struct ccn_charbuf *keylocator = ccn_charbuf_create();
	ccn_create_keylocator(keylocator, ccn_keystore_public_key(cached_keystore));
	res = ccn_signed_info_create(signed_info,
			/*pubkeyid*/ get_my_publisher_key_id(),
			/*publisher_key_id_size*/ get_my_publisher_key_id_length(),
			/*datetime*/ NULL,
			/*type*/ CCN_CONTENT_DATA,
			///*freshness*/ -1,
			/*freshness*/ FRESHNESS,
			/*finalblockid*/ NULL,
			/*keylocator*/ keylocator);
	if (res < 0) {
		DPRINT("FAILED TO CREATE signed_info (res == %d)", res);
	}

	name = ccn_charbuf_create();
	content = ccn_charbuf_create();
	ccn_name_init(name);
	ccn_name_append_components(name, info->interest_ccnb,
			info->interest_comps->buf[0], info->interest_comps->buf[nameEnd + 1]);
	// append own  username
	ccn_name_append_str(name, userName.toLocal8Bit().constData());
	// get user list, the caller need to free the data buffer allocated 
	int dlen = userListtoXml(&data);
	ccn_encode_ContentObject(content, name, signed_info,
			data, dlen, 
			NULL, get_my_private_key());
	// already have the lock, no need to trylock
	ccn_put(info->h, content->buf, content->length);
	ccn_charbuf_destroy(&signed_info);
	ccn_charbuf_destroy(&name);
	ccn_charbuf_destroy(&content);
	if (data != NULL) {
		free((void *)data);
		data = NULL;
	}
}

void GroupManager::incomingContent(ccn_upcall_info *info) {
    size_t len = 0;
    unsigned char *valuep = NULL;
    ccn_content_get_value(info->content_ccnb, info->pco->offset[CCN_PCO_E],
                          info->pco,
                          (const unsigned char **)&valuep, &len);

    if (-1 == parseXmlUserList(valuep, len)) {
        debug("Invalid user list format received"); 
    }
	
	if (valuep != NULL) {
		valuep = NULL;
	}

	// already have the lock
	enumerate();
}

static enum ccn_upcall_res handle_leave(ccn_closure *selfp,
										ccn_upcall_kind kind,
										ccn_upcall_info *info) {
	switch(kind) {
    case CCN_UPCALL_FINAL:
        return (CCN_UPCALL_RESULT_OK);

    case CCN_UPCALL_INTEREST_TIMED_OUT:

        return (CCN_UPCALL_RESULT_OK);

    case CCN_UPCALL_INTEREST: {
		pGroupManager->handleLeave(info);
		return (CCN_UPCALL_RESULT_OK);
	}

	default:
		return (CCN_UPCALL_RESULT_OK);
			
	}
}

static enum ccn_upcall_res incoming_interest(ccn_closure *selfp,
                                ccn_upcall_kind kind,
                                ccn_upcall_info *info)
{
    assert(pGroupManager);
    switch (kind) {
    case CCN_UPCALL_FINAL:
        return (CCN_UPCALL_RESULT_OK);

    case CCN_UPCALL_INTEREST_TIMED_OUT:

        return (CCN_UPCALL_RESULT_OK);

    case CCN_UPCALL_INTEREST: {
		pGroupManager->incomingInterest(info);
        return (CCN_UPCALL_RESULT_OK);
	}

    default:
        return (CCN_UPCALL_RESULT_OK);
    }
}


static enum ccn_upcall_res incoming_content(ccn_closure *selfp,
                                  ccn_upcall_kind kind,
                                  ccn_upcall_info *info)
{
    assert(pGroupManager);

    switch (kind) {
    case CCN_UPCALL_FINAL:
    case CCN_UPCALL_INTEREST_TIMED_OUT:
    case CCN_UPCALL_CONTENT_UNVERIFIED:
        return (CCN_UPCALL_RESULT_OK);

    case CCN_UPCALL_CONTENT:
        /* Incoming content responses to previously expressed interests */ 
        break;
    default:
        return (CCN_UPCALL_RESULT_ERR);
    }

	pGroupManager->incomingContent(info);
    return (CCN_UPCALL_RESULT_OK);   
}



static const ccn_pkey *get_my_private_key() {
    if (cached_keystore == NULL) init_cached_keystore();
    return (ccn_keystore_private_key(cached_keystore));
}

static const ccn_pkey *get_my_public_key() {
    if (cached_keystore == NULL) init_cached_keystore();
    return (ccn_keystore_public_key(cached_keystore));
}

static const unsigned char *get_my_publisher_key_id() {
    if (cached_keystore == NULL) init_cached_keystore();
    return (ccn_keystore_public_key_digest(cached_keystore));
}

static ssize_t get_my_publisher_key_id_length() {
    if (cached_keystore == NULL) init_cached_keystore();
    return (ccn_keystore_public_key_digest_length(cached_keystore));
}

static int
ccn_create_keylocator(ccn_charbuf *c, const ccn_pkey *k)
{
    int res;
    ccn_charbuf_append_tt(c, CCN_DTAG_KeyLocator, CCN_DTAG);
    ccn_charbuf_append_tt(c, CCN_DTAG_Key, CCN_DTAG);
    res = ccn_append_pubkey_blob(c, k);
    if (res < 0)
        return (res);
    else {
        ccn_charbuf_append_closer(c); /* </Key> */
        ccn_charbuf_append_closer(c); /* </KeyLocator> */
    }
    return (0);
}

