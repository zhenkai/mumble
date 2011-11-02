#include "MediaFetcher.h"
#include "Params.h"
#include "SourceList.h"
#include <poll.h>
#include <pthread.h>
static MediaFetcher *gMediaFetcher;
static struct pollfd pfds[1];
static pthread_mutex_t mutex;
static pthread_mutexattr_t ccn_attr;

static enum ccn_upcall_res handleMediaContent(struct ccn_closure *selfp,
											  enum ccn_upcall_kind kind,
											  struct ccn_upcall_info *info);

static enum ccn_upcall_res handlePipeMediaContent(struct ccn_closure *selfp,
											  enum ccn_upcall_kind kind,
											  struct ccn_upcall_info *info);

MediaFetcher::MediaFetcher (SourceList *sourceList) {
	gMediaFetcher = this;
	this->sourceList = sourceList;
	nh = new NdnHandler();
	staleOk = true;
	fetchTimer = new QTimer(this);
	fetchTimer->setInterval(CHECK_INTERVAL);
	connect(fetchTimer, SIGNAL(timeout()), this, SLOT(fetch()));
	fetchTimer->start();

	pfds[0].fd = ccn_get_connection_fd(nh->h);
	pfds[0].events = POLLIN;

	pthread_mutexattr_init(&ccn_attr);
	pthread_mutexattr_settype(&ccn_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mutex, &ccn_attr);

	bRunning = true;
	start();
}

MediaFetcher::~MediaFetcher() {
	bRunning = false;
	if (isRunning())
		wait();
	if (nh != NULL) {
		delete nh;
		nh = NULL;
	}
}

void MediaFetcher::run() {
	int res = 0;
	res = ccn_run(nh->h, 0);
	while(res >= 0 && bRunning) {
		initStream();
		int ret = poll(pfds, 1, 10);
		if (ret >= 0) {
			pthread_mutex_lock(&mutex);
			res = ccn_run(nh->h, 0);
			pthread_mutex_unlock(&mutex);
		}
	}
}

void MediaFetcher::initStream()
{
    QHash<QString,MediaSource *>::const_iterator it = sourceList->list.constBegin(); 
    for ( ; it != sourceList->list.constEnd(); ++it ) {
		MediaSource *ms = it.value();
        if (ms && !ms->isStreaming()) {
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
			QString fullName = ms->getPrefix() + "/" + ms->getUsername();
			// e.g. /ndn/ucla.edu/kiwi/video
			ccn_name_from_uri(path, fullName.toLocal8Bit().constData());
			ccn_name_append_str(path, "video");
			if (ms->fetchClosure->p == NULL) 
				ms->fetchClosure->p = &handleMediaContent;

			pthread_mutex_lock(&mutex);
			int res = ccn_express_interest(nh->h, path, ms->fetchClosure, templ);
			if (res < 0) {
				fprintf(stderr, "Express short interest failed\n");
			}
			pthread_mutex_unlock(&mutex);
			ms->setStreaming(true);
            ccn_charbuf_destroy(&path);
            ccn_charbuf_destroy(&templ);
        }
    }
}

/*****************************************************
 * The key point is to maintain a certain number of outstanding Interests.
 * Check in a frequency higher than the data packets generation freqency,
 * and if the outstanding Interests number is smaller than the threshold,
 * then send a new Interest. Even though the Interests maybe consumed in
 * a faster rate for a short time period (e.g. when a large frame is generated),
 * overall, we can always keep up with Interests generation to make the pipe
 * almost full. 
 * This, combined with the consecutive timeouts detections,
 * should make our MediaFetcher work with producer of various data packet
 * generation rate (i.e. not strict requirement on data packet generation rate
 * as it is in the audio fetching method TODO: update the audio fetching method),
 * as long as the checking frequency is reasonably higher than the data packets
 * generation frequency (a little higher is better to handle the bursty 
 * data packets generation pattern.
 *****************************************************/
void MediaFetcher::fetch() {
	QHash<QString, MediaSource *>::const_iterator it = sourceList->list.constBegin(); 	
	while (it != sourceList->list.constEnd()) {
		QString userName = it.key();
		MediaSource *ms = it.value();
		if (ms != NULL && ms->isStreaming()) {
			if (ms->needSendInterest()){
				ms->incSeq();
				struct ccn_charbuf *pathbuf = ccn_charbuf_create();
				QString fullName = ms->getPrefix() + "/" + ms->getUsername();
				// e.g. /ndn/ucla.edu/kiwi/video/2
				ccn_name_from_uri(pathbuf, fullName.toLocal8Bit().constData());
				ccn_name_append_str(pathbuf, "video");
				struct ccn_charbuf *temp = ccn_charbuf_create();
				ccn_charbuf_putf(temp, "%ld", ms->getSeq());
				ccn_name_append(pathbuf, temp->buf, temp->length);
				if (ms->fetchPipelineClosure->p == NULL)
					ms->fetchPipelineClosure->p = &handlePipeMediaContent;
				pthread_mutex_lock(&mutex);
				int res = ccn_express_interest(nh->h, pathbuf, ms->fetchPipelineClosure, NULL);
				if (res < 0) {
					fprintf(stderr, "Sending interest failed at normal processor\n");
				}
				pthread_mutex_unlock(&mutex);
				ccn_charbuf_destroy(&pathbuf);
				ccn_charbuf_destroy(&temp);
			}
		}
		it++;	
	}
}


void MediaFetcher::initPipe(struct ccn_closure *selfp, struct ccn_upcall_info *info) {
	MediaSource *ms = (MediaSource *)selfp->data;
	if (!ms)
		return;

	if (ms->isStreaming())
		return;

	const unsigned char *ccnb = info->content_ccnb;
	size_t ccnb_size = info->pco->offset[CCN_PCO_E];
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
	if (seq >= 0) {
		ms->setSeq(seq);
		ms->setStreaming(true);
		ms->largestSeenSeq = seq;
	}
	else {
		return;
	}
	
	// send hint-ahead interests
	for (int i = 0; i < PENDING_INTEREST_NUM; i ++) {
		ms->incSeq();
		struct ccn_charbuf *pathbuf = ccn_charbuf_create();
		ccn_name_init(pathbuf);
		ccn_name_append_components(pathbuf, ccnb, comps->buf[0], comps->buf[k]);
		struct ccn_charbuf *temp = ccn_charbuf_create();
		ccn_charbuf_putf(temp, "%ld", ms->getSeq());
		ccn_name_append(pathbuf, temp->buf, temp->length);
		if (ms->fetchPipelineClosure->p == NULL)
			ms->fetchPipelineClosure->p = &handlePipeMediaContent;
		pthread_mutex_lock(&mutex);
		int res = ccn_express_interest(info->h, pathbuf, ms->fetchPipelineClosure, NULL);
		if (res < 0) {
			fprintf(stderr, "Sending interest failed at normal processor\n");
		}
		pthread_mutex_unlock(&mutex);
		ccn_charbuf_destroy(&pathbuf);
		ccn_charbuf_destroy(&temp);
	}
}

void MediaFetcher::processContent(struct ccn_closure *selfp, struct ccn_upcall_info *info) {
	MediaSource *ms = (MediaSource *)selfp->data;

	const unsigned char *ccnb = info->content_ccnb;
	size_t ccnb_size = info->pco->offset[CCN_PCO_E];
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
	if (seq >= 0) {
		if (seq > ms->largestSeenSeq)
			ms->largestSeenSeq = seq;
	}
	else
		return;
	////////////

	const unsigned char *content = NULL;
	size_t len = 0;

	ccn_content_get_value(info->content_ccnb, info->pco->offset[CCN_PCO_E], info->pco, &content, &len);

	QByteArray qba((const char *)content, len);

	QDataStream qds(qba);
	quint16 dataLen;
	quint64 frameNum;
	quint16 frameSeq; 
	bool EoF;
	char array[MAX_PACKET_SIZE];

	qds >> dataLen;
	qds >> frameNum;
	qds >> frameSeq;
	qds >> EoF;

	int ret = qds.readRawData(array, dataLen);

	if (dataLen != ret) {
		fprintf(stderr, "binary encoding/decoding error! dataLen = %d, ret = %d.\n", dataLen, ret);
		abort();
	}

	QString username;
	// no fragmentation
	if (frameSeq == 0 && EoF) {
		username = ms->getUsername();
		unsigned char *buf = (unsigned char *)calloc(1, sizeof(char) * dataLen);
		memcpy(buf, array, dataLen);
		//emit contentArrived(username, buf, dataLen);
		ms->decodeImage(buf, dataLen);
	}
	else {
		if (!ms->frameBuffers.contains((long)frameNum)) {
			struct buffer_t *buffer = (struct buffer_t *)calloc(1, sizeof (struct buffer_t));
			buffer->buf = (unsigned char *)calloc(1, MAX_PACKET_SIZE * 50);
			buffer->size = 0;
			buffer->targetSize = -1;
			ms->frameBuffers[(long)frameNum] = buffer;
		}
		struct buffer_t *buffer = ms->frameBuffers[(long)frameNum];
		memcpy(buffer->buf + (int)frameSeq * MAX_PACKET_SIZE, array, dataLen);
		buffer->size += dataLen;
		if (EoF) {
			buffer->targetSize = (int)frameSeq * MAX_PACKET_SIZE + dataLen;
		}
		// all pieces collected
		if (buffer->targetSize == buffer->size) {
			username = ms->getUsername();
			unsigned char *buf = (unsigned char *)calloc(1, sizeof(char) * buffer->size);
			memcpy(buf, buffer->buf, buffer->size);
		//	emit contentArrived(username, buf, buffer->size);
			ms->decodeImage(buf, buffer->size);
			free(buffer->buf);
			free(buffer);
			ms->frameBuffers.remove((long)frameNum);
		}
	}
}

enum ccn_upcall_res handleMediaContent(struct ccn_closure *selfp,
											  enum ccn_upcall_kind kind,
											  struct ccn_upcall_info *info) {
    MediaSource *ms  = (MediaSource *)selfp->data;
    if (ms == NULL) {
        return CCN_UPCALL_RESULT_OK;
    }

	switch (kind) {
	case CCN_UPCALL_INTEREST_TIMED_OUT: {
		return (CCN_UPCALL_RESULT_REEXPRESS);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
		fprintf(stderr, "unverified content received\n");
		return CCN_UPCALL_RESULT_OK;
	case CCN_UPCALL_CONTENT:
		break;
	default:
		return CCN_UPCALL_RESULT_OK;
	}

	// got some data, reset consecutiveTimeouts
	ms->resetTimeouts();

	gMediaFetcher->processContent(selfp, info);

	gMediaFetcher->initPipe(selfp, info);

    return CCN_UPCALL_RESULT_OK;
}

enum ccn_upcall_res handlePipeMediaContent(struct ccn_closure *selfp,
											  enum ccn_upcall_kind kind,
											  struct ccn_upcall_info *info) {
    MediaSource *ms  = (MediaSource *)selfp->data;
    if (ms == NULL) {
        return CCN_UPCALL_RESULT_OK;
    }
	switch (kind) {
	case CCN_UPCALL_INTEREST_TIMED_OUT: {
		ms->incTimeouts();
		// too many consecutive timeouts
		// the other end maybe crashed or stopped generating video
		if (ms->getTimeouts() > CONSECUTIVE_LOSS_THRESHOLD && ms->isStreaming()) {
			// reset seq for this party
			ms->setSeq(0);
			ms->setStreaming(false);
			fprintf(stderr, "%d consecutive losses!\n", CONSECUTIVE_LOSS_THRESHOLD);
		}
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
		fprintf(stderr, "unverified content received\n");
		return CCN_UPCALL_RESULT_OK;
	case CCN_UPCALL_CONTENT:
		break;
	default:
		return CCN_UPCALL_RESULT_OK;

	}
	// got some data, reset consecutiveTimeouts
	ms->resetTimeouts();

	gMediaFetcher->processContent(selfp, info);

    return CCN_UPCALL_RESULT_OK;
}
