#include "SourceList.h"
#define BROADCAST_PREFIX ("/ndn/broadcast/confernece")

static SourceList *gSourceList;
static enum ccn_upcall_res handle_source_info_content(struct ccn_closure *selfp,
												   enum ccn_upcall_kind kind,
												   struct ccn_upcall_info *info);
SourceList::SourceList() {
	gSourceList = this;
	readNdnParams();
	nh = new NdnHandler();
	acceptStaleData = true;
	discoverClosure = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	discoverClosure->p = &handle_source_info_content;
	enumerate();
	bRunning = true;
	start();
}

SourceList::~SourceList() {
	bRunning = false;
	if (isRunning())
		wait();
	if (nh != NULL) {
		delete nh;
		nh = NULL;
	}
	if (discoverClosure != NULL)
		free(discoverClosure);
	
	QHash<QString, MediaSource *>::const_iterator it = list.constBegin();
	for(; it != list.constEnd(); it++) {
		MediaSource *ms = NULL;
		ms = it.value();
		if (ms != NULL)
			delete ms;
	}
}

void SourceList::readNdnParams() {
	QDomDocument settings;
	QString configFileName = QDir::homePath() + "/.actd/.config";
	QFile config(configFileName);
	if (!config.exists()) {
		fprintf(stderr, "Config file does not exist\n");
		abort();
	}
	if (!config.open(QIODevice::ReadOnly)) {
		fprintf(stderr, "Can not open config file\n");
		abort();
	}
	if (!settings.setContent(&config)) {
		config.close();
		fprintf(stderr, "Can not parse config file\n");
		abort();
	}

	QDomElement docElem = settings.documentElement();
	QDomNode n = docElem.firstChild();
	while (!n.isNull()) {
		if (n.nodeName() == "confName") {
			confName = n.toElement().text();
		}
		n = n.nextSibling();
	}

	if (confName == "") {
		fprintf(stderr, "Null conference name\n");
		abort();
	}
}

void SourceList::alivenessTimerExpired(QString username) {
	MediaSource *ms = list[username];	
	if (ms != NULL) {
		delete ms;
	}
	list.remove(username);
	emit mediaSourceLeft(username);
}


void SourceList::enumerate() {
    ccn_charbuf *path = NULL;
    path = ccn_charbuf_create();
	ccn_name_from_uri(path, BROADCAST_PREFIX);
    ccn_name_append_str(path, confName.toLocal8Bit().constData());
	ccn_name_append_str(path, "video-list");

    QHash<QString, MediaSource *>::const_iterator it = list.constBegin();
	QList<QString> toExclude;
    while (it != list.constEnd())
    {
		MediaSource *ms = it.value();
		if (ms && ms->getNeedExclude()) {
			toExclude.append(ms->getUsername());
		}
		++it;
    }

	expressEnumInterest(path, toExclude);
}


void SourceList::expressEnumInterest(struct ccn_charbuf *interest, QList<QString> &toExclude) {

	if (toExclude.size() == 0) {
		struct ccn_charbuf *templ = NULL;

		if(acceptStaleData) {
			templ = ccn_charbuf_create();
            ccn_charbuf_append_tt(templ, CCN_DTAG_Interest, CCN_DTAG); // <interest>
            ccn_charbuf_append_tt(templ, CCN_DTAG_Name, CCN_DTAG); // <name>
            ccn_charbuf_append_closer(templ); // </name>
			// accept stale data
			ccnb_tagged_putf(templ, CCN_DTAG_AnswerOriginKind, "%d", 4); 
			ccn_charbuf_append_closer(templ); //</interest>
		}
		int res = ccn_express_interest(nh->h, interest, discoverClosure, templ);
		if (res < 0) {
			fprintf(stderr, "express interest failed!");
		}
		ccn_charbuf_destroy(&interest);
		if (templ != NULL)
			ccn_charbuf_destroy(&templ);
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
		qsort(excl, toExclude.size(), sizeof(excl[0]), NdnHandler::nameCompare);
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
			NdnHandler::excludeAll(templ);
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
			NdnHandler::excludeAll(templ);
		}
		ccn_charbuf_append_closer(templ); // </Exclude>
		// accept stale data
		ccnb_tagged_putf(templ, CCN_DTAG_AnswerOriginKind, "%d", 4); 

		ccn_charbuf_append_closer(templ); // </Interest> 
		int res = ccn_express_interest(nh->h, interest, discoverClosure, templ);
		if (res < 0) {
			fprintf(stderr, "express interest failed!");
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

void SourceList::handleSourceInfoContent(struct ccn_upcall_info *info) {
	size_t len = 0;
	const unsigned char *value = NULL;
	ccn_content_get_value(info->content_ccnb, info->pco->offset[CCN_PCO_E], info->pco, &value, &len);
	if (parseSourceInfoContent(value, len) < 0) {
		fprintf(stderr, "Invalid source info received.\n");
	}
	
	enumerate();
}

int SourceList::parseSourceInfoContent(const unsigned char *value, size_t len) {

    QByteArray buffer((const char *)(value));
    QDomDocument doc;
    if (!doc.setContent(buffer)) {
        return -1;
    }
   
    QString username, qPrefix;
	bool leaving = false;
    QDomElement docElem = doc.documentElement();  // <user> 
    QDomNode node = docElem.firstChild();  // <username>
    while (!node.isNull()) {
        if (node.nodeName() == "username") {
			username = node.toElement().text();
		} else
        if (node.nodeName() == "prefix") {
			qPrefix = node.toElement().text();
		} else 
		if (node.nodeName() == "leave") {
			leaving = true;
		}
        node = node.nextSibling();
    }

	if (username == "")
		return -1;
	
	if (!leaving && qPrefix == "")
		return -1;
	
	if (leaving) 
		removeMediaSource(username);
	else
		addMediaSource(username, qPrefix);

    return 0;
}

int SourceList::addMediaSource(QString username, QString prefix) {

	QHash<QString, MediaSource *>::const_iterator it = list.find(username);
	if (it != list.constEnd()) { // username exists
		MediaSource *ms = it.value();
		ms->refreshReceived();
		if (prefix != ms->getPrefix()) { // user updated prefix
			ms->setPrefix(prefix);
		}
		return 0;
	}
	
	MediaSource *ms = new MediaSource(this, username, prefix);
	list.insert(username, ms);
    emit mediaSourceAdded(username); 
    return 0;
}

int SourceList::removeMediaSource(QString username) {
	MediaSource *ms = list[username];
	if (ms == NULL)
		return -1;
	
	list.remove(username);

	emit mediaSourceLeft(username);
	
	delete ms;
}

void SourceList::run() {
	int res = 0;
	while(res >= 0 && bRunning) {
		res = ccn_run(nh->h, 0);
		usleep(20);
	}
}

MediaSource::MediaSource(QObject *parent, QString prefix, QString username) {
	needExclude = false;
	streaming = false;
	consecutiveTimeouts = 0;
	namePrefix = prefix;
	this->username = username;
	freshnessTimer = new QTimer(this);
	freshnessTimer->setInterval(FRESHNESS * 1000);
	freshnessTimer->setSingleShot(true);
	alivenessTimer = new QTimer(this);
	alivenessTimer->setInterval(ALIVE_PERIOD * 1000);
	alivenessTimer->setSingleShot(true);
	connect(freshnessTimer, SIGNAL(timeout()), this, SLOT(excludeNotNeeded()));
	connect(alivenessTimer, SIGNAL(timeout()), this, SLOT(noLongerActive()));
	connect(this, SIGNAL(alivenessTimeout(QString)), parent, SLOT(alivenessTimerExpired(QString)));
	fetchClosure = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	fetchClosure->data = this;
	fetchClosure->p = NULL;
	fetchPipelineClosure = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	fetchPipelineClosure->data = this;
	fetchPipelineClosure->p = NULL;
}

MediaSource::~MediaSource() {
	if (fetchClosure != NULL)
		free(fetchClosure);
}

void MediaSource::refreshReceived() {
	needExclude = true;
	freshnessTimer->stop();
	freshnessTimer->start();
	alivenessTimer->stop();
	alivenessTimer->start();
}

void MediaSource::excludeNotNeeded() {
	needExclude = false;
}

void MediaSource::noLongerActive() {
	emit alivenessTimeout(username);	
}

static enum ccn_upcall_res handle_source_info_content(struct ccn_closure *selfp,
												   enum ccn_upcall_kind kind,
												   struct ccn_upcall_info *info) {
	switch (kind) {
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		if (gSourceList != NULL)  {
			// stop accepting stale data after first timeout
			gSourceList->stopAcceptingStaleData();
			gSourceList->enumerate();
		}

		break;

	case CCN_UPCALL_CONTENT_UNVERIFIED:
		break;
	case CCN_UPCALL_CONTENT:
		gSourceList->handleSourceInfoContent(info);
		break;
	default:
		break;
	}
	return CCN_UPCALL_RESULT_OK;

}
