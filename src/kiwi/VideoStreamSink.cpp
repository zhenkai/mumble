#include "VideoStreamSink.h"
#define BUF_SIZE 1000000

VideoStreamSink::VideoStreamSink() {
	decoder = new VideoStreamDecoder(BUF_SIZE);
}

IplImage *VideoStreamSink::getNextFrame(unsigned char *buf, int len) {
	IplImage *decodedImage = decoder->decodeVideoFrame(buf, int len);
	return decodedImage;
}
