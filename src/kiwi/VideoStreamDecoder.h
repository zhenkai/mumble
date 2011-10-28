#ifndef VIDEOSTREAMDECODER_H
#define VIDEOSTREAMDECODER_H

#include <opencv/cv.h>
#include <opencv/highgui.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#include "Params.h"

/**
 * Video Decoder
 * Decodes encoded data
 */
class VideoStreamDecoder{
public:
	/***
	 * @param bufSize: buf size of internal buffer size
	 * @param fps: frame per second
	 * @param pixelFormat: the pixel format of opencv IplImage; usually it's BGR24
	 */
	 VideoStreamDecoder(int bufSize, int fps = FRAME_PER_SECOND, PixelFormat pixelFormat = PIX_FMT_BGR24);
	 virtual ~VideoStreamDecoder();

	 /**
	  * Decode a buffer to an IplImage
	  * @param buf: encoded data
	  * @param size: buffer size of encoded data
	  * @return: an IplImage, which is the next image in the decoded video stream.
	  */
	  IplImage *decodeVideoFrame(const unsigned char *buf, int size);

private:
	bool initDecoder();
	AVFrame *createAVFrame(PixelFormat pixelFormat, int frameWidth, int frameHeight);

private:
	bool initialized;
	unsigned char *buffer;
	int bufSize;
	int fps;
	AVCodec *decodeCodec;
	AVCodecContext *codecContext;
	AVFrame *decodeFrame;
	AVFrame *openCVFrame;
	PixelFormat openCVPixelFormat;
};
#endif
