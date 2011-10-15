#include "VideoStreamDecoder.h"

VideoStreamDecoder::VideoStreamDecoder(int bufSize, int fps, PixelFormat pixelFormat) {
	this->bufSize = bufSize;
	this->fps = fps;
	this->openCVPixelFormat = pixelFormat;
	
	initialized = false;

	openCVReadyFrame = NULL;

	if (bufSize <= 1) {
		fprintf(stderr, "buffer size is too small\n");
		return;
	}
	
	buffer = new unsigned char[bufSize];

	if (initDecoder())
		initialized = true;

}

VideoStreamDecoder::~VideoStreamDecoder() {
	if (buffer != NULL)
		delete buffer;
}

AVFrame *VideoStreamDecoder::createFrame(int pixelFormat, int frameWidth, int frameHeight) {
	AVFrame *frame = avcodec_alloc_frame();
	if (!frame)
		return NULL;
	
	int picSize = avpicture_get_size(pixelFormat, frameWidth, frameHeight);
	unsigned char *picBuffer = new unsigned char[picSize];

	if (!picBuffer) {
		av_free(frame);
		return NULL;
	}

	avpicture_fill((AVPicture *)frame, picBuffer, pixelFormat, frameWidth, frameHeight);
	return frame;
}

bool VideoStreamDecoder::initDecoder() {
	av_register_all();

	decodeCodec = avcodec_find_decoder(CODEC_ID_H264);

	if(!decodeCodec) {
		fprintf(stderr, "Can not find decode codec\n");
		return false;
	}

	codecContext = avcodec_alloc_context();
	decodeFrame = avcodec_alloc_frame();

	if (avcodec_open(codecContext, decodeCodec) < 0) {
		fprintf(stderr, "could not open codec\n");
		return false;
	}
	
	return true;
}

IplImage *VideoStreamDecoder::decodeVideoFrame(unsigned char *buf, int size) {
	IplImage *image = NULL;
	int decodedFrameSize = 0;

	int res = avcodec_decode_video(codecContext, decodeFrame, &decodedFrameSize, buf, size);
	if (res < 0) {
		fprintf(stderr, "Can not decode buffer\n");
		return NULL;
	}
	
	if (openCVFrame == NULL) {
		opevCVFrame = createAVFrame(openCVPixelFormt, codecContext->width, codecContext->height);
	}

	if(decodedFrameSize > 0) {
		img_convert((AVPicture *)openCVFrame, openCVPixelFormat, (AVPicture *)decodeFrame, PIX_FMT_YUV420P, codecContext->width, codecContext->height);
	}

	image = cvCreateImage(cvSize(codecContext->width, codecContext->height), IPL_DEPTH_8U, 3);
	memcpy(image->imageData, openCVFrame->data[0], image->imageSize);
	image->widthStep = openCVFrame->lineSize[0];

	return image;
}
