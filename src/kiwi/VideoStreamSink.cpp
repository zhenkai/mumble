#include "VideoStreamSink.h"
#define BUF_SIZE 10000000
#define BROADCAST_PREFIX ("/ndn/broadcast/conference")

VideoStreamSink::VideoStreamSink() {
	decoder = new VideoStreamDecoder(BUF_SIZE);
	nh = new NdnHandler();
	sourceList = new SourceList();
}

VideoStreamSink::~VideoStreamSink() {
	if (decoder != NULL)
		delete decoder;
	if (nh != NULL)
		delete nh;
	if (sourceList != NULL)
		delete sourceList;
}

//IplImage *VideoStreamSink::getNextFrame(unsigned char *buf, int len) {
	//IplImage *decodedImage = decoder->decodeVideoFrame(buf, len);
	//return decodedImage;
//}

SourceList::SourceList() {
	nh = new NdnHandler();
}

SourceList::~SourceList() {
	if (nh != NULL)
		delete nh;
}

void MediaSource::MediaSource(QObject *parent, QString prefix, QString username) {
	needExclude = false;
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
	emit alivenessTimeout(nameprefix + username);	
}
