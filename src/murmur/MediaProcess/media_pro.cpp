/* Copyright (C) 2010-present, Zhenkai Zhu <zhenkai@cs.ucla.edu>, Sen Wang <swangfly@qq.vip.com>

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
#include "media_pro.h"
#include <sstream>
#include <pthread.h>
#include <poll.h>
#include <ccn/ccnd.h>

#define FRESHNESS 10 

static struct pollfd pfds[1];
static pthread_mutex_t ccn_mutex; 
static pthread_mutexattr_t ccn_attr;

static void append_lifetime(ccn_charbuf *templ) {
	unsigned int nonce = rand() % MAXNONCE;
	unsigned int lifetime = INTEREST_LIFETIME * 4096 + nonce;
	unsigned char buf[3] = {0};
	for (int i = sizeof(buf) - 1; i >= 0; i--, lifetime >>=8) {
		buf[i] = lifetime & 0xff;
	}
	ccnb_append_tagged_blob(templ, CCN_DTAG_InterestLifetime, buf, sizeof(buf));
}

/*
static ccn_charbuf *make_default_templ() {
	ccn_charbuf *templ = ccn_charbuf_create();
	ccn_charbuf_append_tt(templ, CCN_DTAG_Interest, CCN_DTAG); // <interest>
	ccn_charbuf_append_tt(templ, CCN_DTAG_Name, CCN_DTAG); // <name>
	ccn_charbuf_append_closer(templ); // </name>
	append_lifetime(templ); // <lifetime></lifetime>
	ccn_charbuf_append_closer(templ); // </interest>
	return templ;
}
*/


UserDataBuf::UserDataBuf() {
	interested = 0;    
	iNeedDestroy = 0;
	/*allocate memory for ccn_closure, we don't need to free it, for the ccn stuff will do that*/
	data_buf.callback = (struct ccn_closure *)malloc(sizeof(struct ccn_closure));
	data_buf.sync_callback = (struct ccn_closure *)malloc(sizeof(struct ccn_closure));
	seq = -1;
}

UserDataBuf::~UserDataBuf() { 
   struct buf_list *p = NULL, *pBuf = data_buf.buflist;
   while (pBuf != NULL) {
		p = pBuf->link;
		free(pBuf->buf);
		free(pBuf);
		pBuf = p;
   }
};

void need_fresh_interest(UserDataBuf *userBuf)
{
    if (userBuf != NULL)
        userBuf->interested = 0;
}

enum ccn_upcall_res 
seq_sync_handler(struct ccn_closure *selfp,
				enum ccn_upcall_kind kind,
				struct ccn_upcall_info *info)
{
	switch (kind) {
	case CCN_UPCALL_INTEREST_TIMED_OUT: {
		// no need to re-express
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
		fprintf(stderr, "unverified content received\n");
		return CCN_UPCALL_RESULT_OK;
	case CCN_UPCALL_FINAL:
        return CCN_UPCALL_RESULT_OK;
	case CCN_UPCALL_CONTENT:
		break;
	default:
		return CCN_UPCALL_RESULT_OK;
	}

    UserDataBuf *userBuf  = (UserDataBuf *)selfp->data;

    if (userBuf == NULL) {
        return CCN_UPCALL_RESULT_OK;
    }
	
	const unsigned char *ccnb = info->content_ccnb;
	struct ccn_indexbuf *comps = info->content_comps;

	long seq;
	const unsigned char *seqptr = NULL;
	char *endptr = NULL;
	size_t seq_size = 0;
	int k = comps->n - 2;

	seq = ccn_ref_tagged_BLOB(CCN_DTAG_Component, ccnb,
				comps->buf[k], comps->buf[k + 1],
				&seqptr, &seq_size);
	if (seq >= 0) {
		seq = strtol((const char *)seqptr, &endptr, 10);
		if (endptr != ((const char *)seqptr) + seq_size)
			seq = -1;
	}

	if (seq > 0 && userBuf->seq > 0) {
		if (seq > userBuf->seq || userBuf->seq - seq > SEQ_DIFF_THRES) {
			fprintf(stderr, "+++++++++++++++++++++++++++++++++++++++++++++++ userBuf->seq: %d, seq: %d\n", userBuf->seq, seq);
			userBuf->seq = seq;
			NdnMediaProcess::initPipe(selfp, info, userBuf);

		}
	}
	return CCN_UPCALL_RESULT_OK;

}

enum ccn_upcall_res
ccn_content_handler(struct ccn_closure *selfp,
		    enum ccn_upcall_kind kind,
		    struct ccn_upcall_info *info)
{
    UserDataBuf *userBuf  = (UserDataBuf *)selfp->data;
	switch (kind) {
	case CCN_UPCALL_INTEREST_TIMED_OUT: {
		// if it's short Interest without seq, reexpress
		if (userBuf != NULL && userBuf->seq < 0)
			return (CCN_UPCALL_RESULT_REEXPRESS);

		return (CCN_UPCALL_RESULT_OK);
		
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
		fprintf(stderr, "unverified content received\n");
		return CCN_UPCALL_RESULT_OK;
	case CCN_UPCALL_FINAL:
        return CCN_UPCALL_RESULT_OK;
	case CCN_UPCALL_CONTENT:
		break;
	default:
		return CCN_UPCALL_RESULT_OK;

	}

    if (userBuf == NULL || userBuf->iNeedDestroy) {
        if (userBuf != NULL) delete userBuf;
        selfp->data = NULL;
        return CCN_UPCALL_RESULT_OK;
    }

	if (userBuf->seq < 0) {
		NdnMediaProcess::initPipe(selfp, info, userBuf);
		fprintf(stderr, "initializing pipe");
	}

    struct data_buffer *buffer = &userBuf->data_buf;
    const unsigned char *content_value;
    NDNState *state = buffer->state;

	const unsigned char *ccnb = info->content_ccnb;
	size_t ccnb_size = info->pco->offset[CCN_PCO_E];
	struct ccn_indexbuf *comps = info->content_comps;

	/* Append it to the queue */
	struct buf_list *b;
	b = (struct buf_list *)calloc(1, sizeof(*b));
	b->link = NULL;
	ccn_content_get_value(ccnb, ccnb_size, info->pco,
			&content_value, &b->len);
	b->buf = malloc(b->len);
	memcpy(b->buf, content_value, b->len);
	if (buffer->buflist == NULL)
		buffer->buflist = b;
	else {
		struct buf_list *p = buffer->buflist;
		while (p->link != NULL)
			p = p->link;
		p->link = b;
	}
	/*emit the signal for get the data out of the buffer*/
	state->emitSignal(userBuf->user_name);

    return CCN_UPCALL_RESULT_OK;
}

void 
data_buffer_init(NDNState *state, UserDataBuf *userBuf, const char *direction)
{
    struct data_buffer *db = &userBuf->data_buf;
    db->callback->data = userBuf;
	db->sync_callback->data = userBuf;
    db->state = state;
    db->buflist = NULL;
    strncpy(db->direction, direction, sizeof(db->direction));
    if (db->direction[0] == 'r') {
        db->callback->p = &ccn_content_handler;
		db->sync_callback->p = &seq_sync_handler;
	}
    need_fresh_interest(userBuf);
}

static struct ccn_keystore *cached_keystore = NULL;

void
init_cached_keystore(void)
{
    struct ccn_keystore *keystore = cached_keystore;
    int res;

    if (keystore == NULL) {
	struct ccn_charbuf *temp = ccn_charbuf_create();
	keystore = ccn_keystore_create();
	ccn_charbuf_putf(temp, "%s/.ccnx/.ccnx_keystore", getenv("HOME"));
	res = ccn_keystore_init(keystore,
				ccn_charbuf_as_string(temp),
				"Th1s1sn0t8g00dp8ssw0rd.");
	if (res != 0) {
	    printf("Failed to initialize keystore %s\n", ccn_charbuf_as_string(temp));
	    exit(1);
	}
	ccn_charbuf_destroy(&temp);
	cached_keystore = keystore;
    }
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

const struct ccn_pkey *
get_my_private_key(void)
{
    if (cached_keystore == NULL) init_cached_keystore();
    return (ccn_keystore_private_key(cached_keystore));
}

const struct ccn_certificate *
get_my_certificate(void)
{
    if (cached_keystore == NULL) init_cached_keystore();
    return (ccn_keystore_certificate(cached_keystore));
}

const unsigned char *
get_my_publisher_key_id(void)
{
    if (cached_keystore == NULL) init_cached_keystore();
    return (ccn_keystore_public_key_digest(cached_keystore));
}


ssize_t
get_my_publisher_key_id_length(void)
{
    if (cached_keystore == NULL) init_cached_keystore();
    return (ccn_keystore_public_key_digest_length(cached_keystore));
}

NDNState::NDNState() {
	active = false;
	ccn = NULL;
    signed_info = ccn_charbuf_create();
	if (cached_keystore == NULL)
		init_cached_keystore(); 
	ccn_charbuf *keylocator = ccn_charbuf_create();
	ccn_create_keylocator(keylocator, ccn_keystore_public_key(cached_keystore));
    /* Create signed_info */
    int res = ccn_signed_info_create(signed_info,
                                 /* pubkeyid */ get_my_publisher_key_id(),
                                 /* publisher_key_id_size */ get_my_publisher_key_id_length(),
                                 /* datetime */ NULL,
                                 /* type */ CCN_CONTENT_DATA,
                                 /* freshness */ FRESHNESS,
				                 /* finalblockid */ NULL,
                                 /* keylocator */ keylocator);
    if (res != 0) {
	    fprintf(stderr, "signed_info_create failed %d (line %d)\n", res, __LINE__);
    }
}

NdnMediaProcess::NdnMediaProcess()
{

	localSeq = 0;
	isPrivate = false;
}

int NdnMediaProcess::hint_ahead = 100;

void NdnMediaProcess::tick() {
	localSeq++;
	counter ++;
	if (counter % SEQ_SYNC_INTERVAL == 0) {
		publish_local_seq();
		sync_tick();
	}
	// send new interest for every speaker
	ruMutex.lock();
	QHash<QString, UserDataBuf *>::const_iterator it = qhRemoteUser.constBegin(); 	
	while (it != qhRemoteUser.constEnd()) {
		QString userName = it.key();
		UserDataBuf *udb = it.value();
		if (udb != NULL && udb->seq >= 0) {
			udb->seq++;
			struct ccn_charbuf *pathbuf = ccn_charbuf_create();
			ccn_name_from_uri(pathbuf, userName.toLocal8Bit().constData());
			ccn_name_append_str(pathbuf, "audio");
			struct ccn_charbuf *temp = ccn_charbuf_create();
			ccn_charbuf_putf(temp, "%ld", udb->seq);
			ccn_name_append(pathbuf, temp->buf, temp->length);
			int c = 0;
			while (pthread_mutex_trylock(&ccn_mutex) != 0) {
				c++;
				if (c > 10000000) {
					fprintf(stderr, "cannot obtain lock! %s:%d\n", __FILE__, __LINE__);
					std::exit(1);
				}
			}
			int res = ccn_express_interest(ndnState.ccn, pathbuf, udb->data_buf.callback, NULL);
			pthread_mutex_unlock(&ccn_mutex);
			if (res < 0) {
				fprintf(stderr, "Sending interest failed at normal processor\n");
				exit(1);
			}
			ccn_charbuf_destroy(&pathbuf);
			ccn_charbuf_destroy(&temp);
		}
		it++;	
	}
	ruMutex.unlock();
}

void NdnMediaProcess::sync_tick() {
	// sync with every speaker about their seq	
	ruMutex.lock();
	QHash<QString, UserDataBuf *>::const_iterator it = qhRemoteUser.constBegin();
	while(it != qhRemoteUser.constEnd()) {
		QString userName = it.key();
		UserDataBuf *udb = it.value();
		if (udb != NULL) {
			struct ccn_charbuf *pathbuf = ccn_charbuf_create();
			ccn_name_from_uri(pathbuf, userName.toLocal8Bit().constData());
			ccn_name_append_str(pathbuf, "seq_sync");
			ccn_name_append_str(pathbuf, "audio");
			int c = 0;
			while (pthread_mutex_trylock(&ccn_mutex) != 0) {
				c++;
				if (c > 10000000) {
					fprintf(stderr, "cannot obtain lock! %s:%d\n", __FILE__, __LINE__);
					std::exit(1);
				}
			}
			int res = ccn_express_interest(ndnState.ccn, pathbuf, udb->data_buf.sync_callback, NULL);
			pthread_mutex_unlock(&ccn_mutex);
			if (res < 0) {
				fprintf(stderr, "Sending interest failed at sync process\n");
				std::exit(1);
			}
			ccn_charbuf_destroy(&pathbuf);
			fprintf(stderr, "Sending interest sync interest to %s\n", userName.toLocal8Bit().constData());
		}
		it++;
	}
	ruMutex.unlock();
}

void NdnMediaProcess::initPipe(struct ccn_closure *selfp, struct ccn_upcall_info *info, UserDataBuf *userBuf) {
	// get seq
	const unsigned char *ccnb = info->content_ccnb;
	size_t ccnb_size = info->pco->offset[CCN_PCO_E];
	struct ccn_indexbuf *comps = info->content_comps;

	long seq;
	const unsigned char *seqptr = NULL;
	char *endptr = NULL;
	size_t seq_size = 0;
	int k = comps->n - 2;

	// not the case of resetting seq
	if (userBuf->seq < 0) {
		seq = ccn_ref_tagged_BLOB(CCN_DTAG_Component, ccnb,
				comps->buf[k], comps->buf[k + 1],
				&seqptr, &seq_size);
		if (seq >= 0) {
			seq = strtol((const char *)seqptr, &endptr, 10);
			if (endptr != ((const char *)seqptr) + seq_size)
				seq = -1;
		}
		if (seq >= 0) {
			userBuf->seq = seq;
		}
		else {
			return;
		}
	}	

	// send hint-ahead interests
	for (int i = 0; i < hint_ahead; i ++) {
		userBuf->seq++;
		struct ccn_charbuf *pathbuf = ccn_charbuf_create();
		ccn_name_init(pathbuf);
		ccn_name_append_components(pathbuf, ccnb, comps->buf[0], comps->buf[k]);
		struct ccn_charbuf *temp = ccn_charbuf_create();
		ccn_charbuf_putf(temp, "%ld", userBuf->seq);
		ccn_name_append(pathbuf, temp->buf, temp->length);
		
		// no need to trylock as we already have the lock
		int res = ccn_express_interest(info->h, pathbuf, selfp, NULL);
		if (res < 0) {
			fprintf(stderr, "Sending interest failed at normal processor\n");
			std::exit(1);
		}
		ccn_charbuf_destroy(&pathbuf);
		ccn_charbuf_destroy(&temp);
	}
}

NdnMediaProcess::~NdnMediaProcess()
{
    QHash<QString,UserDataBuf *>::iterator it; 
	ruMutex.lock();
    for ( it = qhRemoteUser.begin(); it != qhRemoteUser.end(); ++it ) {
        delete it.value();
    }
	ruMutex.unlock();
}

void NdnMediaProcess::setSK(QByteArray sk) {
	memcpy(sessionKey, sk.data(), sk.size());
}

void NdnMediaProcess::addRemoteUser(QString strUserName)
{
    UserDataBuf *rUser = new UserDataBuf(); 
    data_buffer_init(&ndnState, rUser, "recv");
    rUser->user_name = strUserName;
    rUser->user_type = REMOTE_USER;
	ruMutex.lock();
    qhRemoteUser.insert(strUserName, rUser); 
	ruMutex.unlock();
}

void NdnMediaProcess::deleteRemoteUser(QString strUserName)
{
	ruMutex.lock();
    UserDataBuf *p =  qhRemoteUser.value(strUserName);
    if (p == NULL) {
        return;
    }
    qhRemoteUser.remove(strUserName); 
    if (p->interested == 0) delete p;
    else p->iNeedDestroy = 1;
	
	ruMutex.unlock();
}

void NdnMediaProcess::addLocalUser(QString strUserName)
{

    localUdb = new UserDataBuf; 
    data_buffer_init(&ndnState, localUdb, "send");
    localUdb->user_name = strUserName;
    localUdb->user_type = LOCAL_USER;
}

void NdnMediaProcess::deleteLocalUser(QString strUserName)
{
	delete localUdb;
	localUdb = NULL;
}

void NdnMediaProcess::publish_local_seq() {
	struct ccn_charbuf *pathbuf = ccn_charbuf_create();
	ccn_name_from_uri(pathbuf, localUdb->user_name.toLocal8Bit().constData());
	ccn_name_append_str(pathbuf, "seq_sync");
	ccn_name_append_str(pathbuf, "audio");
	struct ccn_charbuf *seqbuf = ccn_charbuf_create();
    ccn_charbuf_putf(seqbuf, "%ld", localSeq);
    ccn_name_append(pathbuf, seqbuf->buf, seqbuf->length);
    struct ccn_charbuf *message = ccn_charbuf_create();
	struct ccn_charbuf *seq_signed_info = ccn_charbuf_create();
	if (cached_keystore == NULL)
		init_cached_keystore(); 
	ccn_charbuf *keylocator = ccn_charbuf_create();
	ccn_create_keylocator(keylocator, ccn_keystore_public_key(cached_keystore));
    /* Create signed_info */
    int res = ccn_signed_info_create(seq_signed_info,
                                 /* pubkeyid */ get_my_publisher_key_id(),
                                 /* publisher_key_id_size */ get_my_publisher_key_id_length(),
                                 /* datetime */ NULL,
                                 /* type */ CCN_CONTENT_DATA,
                                 /* freshness */ 1,
				                 /* finalblockid */ NULL,
                                 /* keylocator */ keylocator);

	res = ccn_encode_ContentObject( /* out */ message,
				   pathbuf,
				   seq_signed_info,
					seqbuf->buf, seqbuf->length, 
				   /* keyLocator */ NULL, get_my_private_key());
	int c = 0;
	while (pthread_mutex_trylock(&ccn_mutex) != 0) {
		c++;
		if (c > 10000000) {
			fprintf(stderr, "cannot obtain lock! %s:%d\n", __FILE__, __LINE__);
			std::exit(1);
		}
	}
	ccn_put(ndnState.ccn, message->buf, message->length);
	pthread_mutex_unlock(&ccn_mutex);
	ccn_charbuf_destroy(&pathbuf);
	ccn_charbuf_destroy(&seq_signed_info);
	ccn_charbuf_destroy(&keylocator);
	ccn_charbuf_destroy(&message);
	ccn_charbuf_destroy(&seqbuf);
}

int NdnMediaProcess::ndnDataSend(const void *buf, size_t len)
{

#define CHARBUF_DESTROY \
    ccn_charbuf_destroy(&message);\
    ccn_charbuf_destroy(&path); \
    ccn_charbuf_destroy(&seq);

    UserDataBuf *userBuf = localUdb; 
	if (userBuf == NULL)
		return -1;
    int res = 0;
    int seq_num = -1;
    struct ccn_charbuf *message = ccn_charbuf_create();
    struct ccn_charbuf *path = ccn_charbuf_create();

    struct ccn_charbuf *seq = ccn_charbuf_create();
    unsigned char *ccn_msg = NULL;
    size_t ccn_msg_size = 0;
    
    ccn_name_init(path);
    
   // if (ndnState.active){
        seq_num = localSeq;
		ccn_name_from_uri(path, localUdb->user_name.toLocal8Bit().constData());
		ccn_name_append_str(path, "audio");
    //}
    
    if (seq_num < 0) {
        res = -1;
        CHARBUF_DESTROY;
        return res;
    }
    
    ccn_charbuf_putf(seq, "%ld", seq_num);
    ccn_name_append(path, seq->buf, seq->length);


	if (isPrivate) {
		unsigned char *enc_buf = NULL;
		size_t enc_len = 0;
		res = symEncrypt(sessionKey, NULL, (const unsigned char *)buf, len, &enc_buf, &enc_len, AES_BLOCK_SIZE);
		if (res != 0) {
			fprintf(stderr, "can not decrypt audio\n");
			std::exit(1);
		}

		res = ccn_encode_ContentObject( /* out */ message,
					   path,
					   ndnState.signed_info,
					   enc_buf, enc_len,
					   /* keyLocator */ NULL, get_my_private_key());
		if (enc_buf != NULL) {
			free(enc_buf);
			enc_buf = NULL;
		}
		
	} else {
		res = ccn_encode_ContentObject( /* out */ message,
					   path,
					   ndnState.signed_info,
					   buf, len,
					   /* keyLocator */ NULL, get_my_private_key());
	}

    if (res != 0) {
        fprintf(stderr, "encode_ContentObject failed %d (line %d)\n", res, __LINE__);
        CHARBUF_DESTROY;
        return res;
    }
    
    ccn_msg = (unsigned char *)calloc(1, message->length);
    ccn_msg_size = message->length;
    memcpy(ccn_msg, message->buf, message->length);
    { struct ccn_parsed_ContentObject o = {0};
        res = ccn_parse_ContentObject(ccn_msg, ccn_msg_size, &o, NULL);
        if (res < 0) {
            fprintf(stderr, "created bad ContentObject, res = %d\n", res);
            abort();
        }
    }

    
    struct buf_list *p = NULL, *b = userBuf->data_buf.buflist;
    while (b != NULL) { p = b; b = b->link; }
    b = (struct buf_list*)calloc(1, sizeof(struct buf_list));
    if (b == NULL) {
        CHARBUF_DESTROY;
        return -1;
    }
    if (p != NULL)
        p->link = b;
    else userBuf->data_buf.buflist = b;
    
    b->buf = ccn_msg;
    b->len = ccn_msg_size;
    b->link = NULL;

    CHARBUF_DESTROY;
    return res;
}

int NdnMediaProcess::doPendingSend()
{
    int res = 0;

	struct buf_list  *p;
	struct buf_list *b = localUdb->data_buf.buflist;
	if (b != NULL) {
		p = b->link;
		if (b != NULL && b->buf != NULL) {
			int c = 0;
			while (pthread_mutex_trylock(&ccn_mutex) != 0) {
				c++;
				if (c > 10000000) {
					fprintf(stderr, "cannot obtain lock! %s:%d\n", __FILE__, __LINE__);
					std::exit(1);
				}
			}
			res = ccn_put(ndnState.ccn, b->buf, b->len);
			pthread_mutex_unlock(&ccn_mutex);
			free(b->buf);
			b->len = 0;
			b->buf = NULL;
		}
		free(b);
		localUdb->data_buf.buflist = p;
	}
    return res;
}

int NdnMediaProcess::checkInterest()
{
    int res = 0;
    QHash<QString,UserDataBuf *>::iterator it; 
	ruMutex.lock();
    for ( it = qhRemoteUser.begin(); it != qhRemoteUser.end(); ++it ) {
        if (!it.value()->interested) {
            /* Use a template to express our order preference for the first packet. */
            struct ccn_charbuf *templ = ccn_charbuf_create();
            struct ccn_charbuf *path = ccn_charbuf_create();
            ccn_charbuf_append_tt(templ, CCN_DTAG_Interest, CCN_DTAG);
            ccn_charbuf_append_tt(templ, CCN_DTAG_Name, CCN_DTAG);
            ccn_charbuf_append_closer(templ);
            ccn_charbuf_append_tt(templ, CCN_DTAG_ChildSelector, CCN_DTAG);
            ccn_charbuf_append_tt(templ, 1, CCN_UDATA);
            ccn_charbuf_append(templ, "1", 1);	/* low bit 1: rightmost */
            ccn_charbuf_append_closer(templ); /*<ChildSelector>*/
            ccn_charbuf_append_closer(templ);
			ccn_name_from_uri(path, it.key().toLocal8Bit().constData());
			ccn_name_append_str(path, "audio");
            if (res >= 0) {
                if (it.value()->data_buf.callback->p == NULL) {fprintf(stderr, "data_buf.callback is NULL!\n"); exit(1); }
				int c = 0;
				while (pthread_mutex_trylock(&ccn_mutex) != 0) {
					c++;
					if (c > 10000000) {
						fprintf(stderr, "cannot obtain lock! %s:%d\n", __FILE__, __LINE__);
						std::exit(1);
					}
				}
                res = ccn_express_interest(ndnState.ccn, path, it.value()->data_buf.callback, templ);
				pthread_mutex_unlock(&ccn_mutex);
                it.value()->interested = 1;
            }
            if (res < 0) {
				fprintf(stderr, "sending the first interest failed\n");
				exit(1);
			}
            ccn_charbuf_destroy(&path);
            ccn_charbuf_destroy(&templ);
        }
    }
	ruMutex.unlock();
    return res;
}

int NdnMediaProcess::ndn_wait_message(UserDataBuf *userBuf, char *buf, int len)
{
    if (ndnState.ccn == NULL || userBuf == NULL) {
        errno = 9; //EBADFD;
        return(-1);
    }
    if (userBuf->data_buf.buflist == NULL) {
        errno = EAGAIN;
        len = -1;
    }
    else {
        struct buf_list *b = userBuf->data_buf.buflist;
        userBuf->data_buf.buflist = b->link;
		if (isPrivate) {
			unsigned char *plain_data = NULL;
			size_t plain_len = 0;
			int res = symDecrypt(sessionKey, NULL, (const unsigned char *) b->buf, b->len, &plain_data, &plain_len, AES_BLOCK_SIZE);
			if (res != 0)  {
				fprintf(stderr, "can not decrypt audio\n");
				std::exit(1);
			}
			len = plain_len;
			memcpy(buf, plain_data, len);
			if (plain_data != NULL) {
				free(plain_data);
				plain_data = NULL;
			}
		} else
		{
			if (b->len < len)
				len = b->len;
			memcpy(buf, b->buf, len);
		}
        free(b->buf);
        b->buf = NULL;
        b->len = 0;
        free(b);
        b = NULL;
    }
    return(len);
}

int NdnMediaProcess::startThread() {
    struct ccn *h;

    /* Shut down any lingering session */
    while (ndnState.ccn != NULL) {
        ndnState.active = 0;
        printf("waiting for old session to die\n");
    }
    h = ccn_create();
    if (ccn_connect(h, NULL) == -1) {
        ccn_perror(h, "Failed to contact ccnd");
        ccn_destroy(&h);
        return (-1);
    }
	pthread_mutexattr_init(&ccn_attr);
	pthread_mutexattr_settype(&ccn_attr, PTHREAD_MUTEX_RECURSIVE);     
	pthread_mutex_init(&ccn_mutex, &ccn_attr);

    ndnState.ccn = h; 
	pfds[0].fd = ccn_get_connection_fd(ndnState.ccn);
	pfds[0].events = POLLIN | POLLOUT | POLLWRBAND;
	clock = new QTimer(this);
	connect(clock, SIGNAL(timeout()), this, SLOT(tick()));
	clock->start(PER_PACKET_LEN);
	
	counter = 0;

    if (! isRunning()) {
        fprintf(stderr, "Starting voice thread in media_pro\n"); 
        ndnState.active = true;

        start(QThread::HighestPriority);
#ifdef Q_OS_LINUX
        int policy;
        struct sched_param param;
        if (pthread_getschedparam(pthread_self(), &policy, &param) == 0) {
            if (policy == SCHED_OTHER) {
                policy = SCHED_FIFO;
                param.sched_priority = 1;
                pthread_setschedparam(pthread_self(), policy, &param);
            }
        }
#endif
    }
	else
		fprintf(stderr, "what the hell\n");
    return 0;
}

int NdnMediaProcess::stopThread() {

    /* Shut down any lingering session */
    while (ndnState.ccn != NULL) {
        ndnState.active = false;
        printf("waiting for old session to die\n");
    }

    ndnState.active = false;
    if (isRunning()) {
        printf("Ending voice thread");

    }
}

void NdnMediaProcess::run() {
    int res = 0;
	int ret;

    for(;;) {
        if (ndnState.active != 0) {
            
            /* check each local user's buffer to find out if there are some packets 
             * to be sent.If have, send it. */ 
            doPendingSend();

            /* find the new or resetted remote user in the remote user list,send the 
             * first interest for the user.*/
            checkInterest();
        }
        else /* other module has stopped this thread */
            res = -1;

        if (res >= 0) {
			ret = poll(pfds, 1, 10);	
			if (ret > 0) {
				int c = 0;
				while(pthread_mutex_trylock(&ccn_mutex) != 0) {
					c++;
					if (c> 10000000) {
						fprintf(stderr, "cannot obtain lock at ccn_run\n");
						std::exit(1);
					}
				}
				res = ccn_run(ndnState.ccn, 0);
				pthread_mutex_unlock(&ccn_mutex);
			}
        }
        if (res < 0)
            break;
    }
    
    ccn_destroy(&ndnState.ccn);
    ndnState.ccn = NULL;
}

int NdnMediaProcess::sendLocalMedia(char *msg, int msg_len)
{
    int len, res;

    res = ndnDataSend(msg, msg_len);

    if (res >= 0) {
        res = len;
    }
    else {
        errno = EAGAIN;
        res = -1;
    }
    return(res);
}

int NdnMediaProcess::getRemoteMedia(QString strUserName,char* msg, int msg_len)
{
    int res;

	ruMutex.lock();
    UserDataBuf *p = qhRemoteUser.value(strUserName); 
    res = ndn_wait_message(p, msg, msg_len);
	ruMutex.unlock();

    if (res < 0) {
        errno = EAGAIN;
    } 

    return(res);
}


