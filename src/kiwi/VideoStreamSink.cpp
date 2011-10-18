#include "VideoStreamSink.h"
#define BUF_SIZE 1000000

VideoStreamSink::VideoStreamSink() {
	decoder = new VideoStreamDecoder(BUF_SIZE);
}

IplImage *VideoStreamSink::getNextFrame() {
	IplImage *decodedImage = decoder->decodeVideoFrame(unsigned char *buffer, int frameSize);
}
