#ifndef VIDEOSTREAMENCODER_H
#define VIDEOSTREAMENCODER_H
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#define ENCODING_STRING ("h264")

/**
 * Encoder for video
 * Encode an IplImage to encoded data [h.264 or other formats]
 */
class VideoStreamEncoder{
public:
	VideoStreamEncoder(size_t bufSize, int videoWidth, int videoHeight, 
					   int fps = 25, PixelFormat pixelFormat = PIX_FMT_BGR24);
	virtual ~VideoStreamEncoder();

	/**
	 * Encode an IplImage to a buffer
	 * @param image: this is a "next" frame of a video stream
	 * @param outSize: the size of encode buffer
	 * @return: A buffer which is the next packet of an encode stream
	 * @note: do not free the returned buffer
	 */
	 const unsigned char *encodeVideoFrame(IplImage *image, int *outSize);

private:
	bool initEncoder();
	AVFrame *createAVFrame(int pix_format);
	AVStream *createVideoStream(AVFormatContext *fmtContext);
	bool openCodec(AVStream *stream);

private:
	bool initialized;
	AVFrame *readyToEncodeFrame, *openCVFrame;
	AVOutputFormat *outputFormat;
	AVFormatContext *fmtContext;
	AVStream *videoStream;
	PixelFormat openCVPixelFormat;
	unsigned char *buffer;
	size_t bufSize;
	int videoWidth;
	int videoHeight;
	int fps;
};
#endif
