#include "VideoStreamSink.h"
#define BUF_SIZE 10000000

VideoStreamSink::VideoStreamSink() {
	decoder = new VideoStreamDecoder(BUF_SIZE);
}

VideoStreamSink::~VideoStreamSink() {
}

IplImage *VideoStreamSink::getNextFrame(unsigned char *buf, int len) {
	IplImage *decodedImage = decoder->decodeVideoFrame(buf, len);
	return decodedImage;
}
