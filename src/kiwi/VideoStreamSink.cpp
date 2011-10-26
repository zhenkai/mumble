#include "VideoStreamSink.h"
#define BUF_SIZE 10000000
#define BROADCAST_PREFIX ("/ndn/broadcast/conference")

VideoStreamSink::VideoStreamSink() {
	decoder = new VideoStreamDecoder(BUF_SIZE);
	sourceList = new SourceList();
	fetcher = new MediaFetcher(sourceList);
	connect(fetcher, SIGNAL(contentArrived(QString, const unsigned char *, size_t)), this, SLOT(decodeImage(QString, const unsigned char *, size_t)));
	connect(sourceList, SIGNAL(mediaSourceLeft(QString)), this, SLOT(sourceLeft(QString)));
	connect(sourceList, SIGNAL(mediaSourceAdded(QString)), this, SLOT(sourceAdded(QString)));
}

VideoStreamSink::~VideoStreamSink() {
	if (decoder != NULL)
		delete decoder;
	if (sourceList != NULL)
		delete sourceList;
	if (fetcher != NULL)
		delete fetcher;
}

void VideoStreamSink::decodeImage(QString name, const unsigned char *buf, size_t len) {
	IplImage *decodedImage = decoder->decodeVideoFrame(buf, len);
	emit imageDecoded(name, decodedImage);
}

void VideoStreamSink::sourceLeft(QString name) {
	emit sourceNumChanged(name, 0);
}

void VideoStreamSink::sourceAdded(QString name) {
	emit sourceNumChanged(name, 1);
}



