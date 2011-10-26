#include "VideoStreamSink.h"
#define BUF_SIZE 10000000
#define BROADCAST_PREFIX ("/ndn/broadcast/conference")

VideoStreamSink::VideoStreamSink() {
	decoder = new VideoStreamDecoder(BUF_SIZE);
	sourceList = new SourceList();
	fetcher = new MediaFetcher(sourceList);
}

VideoStreamSink::~VideoStreamSink() {
	if (decoder != NULL)
		delete decoder;
	if (sourceList != NULL)
		delete sourceList;
	if (fetcher != NULL)
		delete fetcher;
}

//IplImage *VideoStreamSink::getNextFrame(unsigned char *buf, int len) {
	//IplImage *decodedImage = decoder->decodeVideoFrame(buf, len);
	//return decodedImage;
//}


