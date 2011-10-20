#ifndef VIDEOSTREAMENCODER_H
#define VIDEOSTREAMENCODER_H

#include <opencv/cv.h>
#include <opencv/highgui.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#define ENCODING_STRING ("mpeg")
#define ENCODING_CODEC (CODEC_ID_MPEG4)
#define RAW_STREAM_FORMAT (PIX_FMT_YUV420P)

//#define ENCODING_STRING ("h263")
//#define ENCODING_CODEC (CODEC_ID_H263P)
//#define RAW_STREAM_FORMAT (PIX_FMT_YUV420P)

//#define ENCODING_STRING ("h264")
//#define ENCODING_CODEC (CODEC_ID_H264)
//#define RAW_STREAM_FORMAT (PIX_FMT_YUV420P)

//#define ENCODING_STRING ("mjpeg")
//#define ENCODING_CODEC (CODEC_ID_MJPEG)
//#define RAW_STREAM_FORMAT (PIX_FMT_YUVJ420P)

#define STREAM_BIT_RATE 512000
//#define STREAM_BIT_RATE 128000
#define FRAME_PER_SECOND 25 
#define GROUP_OF_PICTURES 25

/**
 * Encoder for video
 * Encode an IplImage to encoded data [h.264 or other formats]
 */
class VideoStreamEncoder{
public:
	VideoStreamEncoder(size_t bufSize, int videoWidth, int videoHeight, 
					   int fps = FRAME_PER_SECOND, PixelFormat pixelFormat = PIX_FMT_BGR24);
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
	AVFrame *createAVFrame(PixelFormat pix_format);
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
