#include "VideoStreamEncoder.h"

VideoStreamEncoder::VideoStreamEncoder(size_t bufSize, int videoWidth, int videoHeight, int fps, PixelFormat pixelFormat){
	this->bufSize = bufSize;
	this->videoWidth = videoWidth;
	this->videoHeight = videoHeight;
	this->fps = 10;
	this->openCVPixelFormat = pixelFormat;

	initialized = false;
	if (bufSize <= 1) {
		return;
	}

	buffer = new unsigned char[bufSize];
	if (initEncoder())
		initialized = true;
}

VideoStreamEncoder::~VideoStreamEncoder() {
	if (buffer != NULL)
		delete buffer;
}

bool VideoStreamEncoder::openCodec(AVStream *stream) {
	AVCodecContext *codecContext = stream->codec;
	AVCodec *codec = avcodec_find_encoder(codecContext->codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
	}
	if (avcodec_open(codecContext, codec) < 0) {
		return false;
	}
	return true;
}

AVFrame *VideoStreamEncoder::createAVFrame(PixelFormat pixel_format) {
	AVFrame *frame = avcodec_alloc_frame();
	if (!frame)
		return NULL;
	
	int picSize = avpicture_get_size(pixel_format, videoWidth, videoHeight);
	unsigned char *picBuffer = new unsigned char[picSize];
	if (!picBuffer) {
		av_free(frame);
		return NULL;
	}

	avpicture_fill((AVPicture *)frame, picBuffer, pixel_format, videoWidth, videoHeight);
	return frame;
}

AVStream *VideoStreamEncoder::createVideoStream(AVFormatContext *fmtContext) {
	AVStream *stream = av_new_stream(fmtContext, 0);
	if (!stream) {
		fprintf(stderr, "Could not alloc stream\n");
		return NULL;
	}

	AVCodecContext *codecContext = stream->codec;
	codecContext->codec_id = (CodecID) (ENCODING_CODEC);
	codecContext->codec_type = CODEC_TYPE_VIDEO;
	codecContext->bit_rate = STREAM_BIT_RATE;
	codecContext->width = videoWidth;
	codecContext->height = videoHeight;
	codecContext->time_base.den = fps;
	codecContext->time_base.num = 1;
	// question: why do we need to send intra frame periodically?
	codecContext->gop_size = fps;
	codecContext->pix_fmt = RAW_STREAM_FORMAT;

	return stream;
}

bool VideoStreamEncoder::initEncoder() {
	av_register_all();
	outputFormat = guess_format(ENCODING_STRING, NULL, NULL);
	if (!outputFormat) {
		fprintf(stderr, "Could not find suitable output format for %s\n", ENCODING_STRING);
		return false;
	}
	outputFormat->video_codec = ENCODING_CODEC;
	fmtContext = av_alloc_format_context();
	if (!fmtContext) {
		fprintf(stderr, "Can not alloc format context\n");
		return false;
	}

	fmtContext->oformat = outputFormat;

	videoStream = createVideoStream(fmtContext);
	if (!videoStream)
		return false;
	
	// must be done even if no parameters are needed
	if(av_set_parameters(fmtContext, NULL) < 0) {
		fprintf(stderr, "Invalid output format parameters\n");
		return false;
	}

	if (!openCodec(videoStream))
		return false;
	
	readyToEncodeFrame = createAVFrame(videoStream->codec->pix_fmt);
	openCVFrame = createAVFrame(openCVPixelFormat);

	return true;
}

const unsigned char *VideoStreamEncoder::encodeVideoFrame(IplImage *image, int *outSize) {
	if (image == NULL)
		return NULL;
	
	if (!initialized) {
		fprintf(stderr, "Initial class failed\n");
		return NULL;
	}

	if (image->imageSize > bufSize) {
		fprintf(stderr, "image size too large.\n");
		return NULL;
	}

	AVCodecContext *codecContext = videoStream->codec;

	// I saw people use this hack: just assign pointer image->imageData t- openCVFrame->data[0] 
	// without using memcpy. This is supposed to speed things up.
	openCVFrame->data[0] = (uint8_t *)image->imageData;
	
	struct SwsContext *imgConvertCtx;
	int width = codecContext->width;
	int height = codecContext->height;
	imgConvertCtx = sws_getContext(width, height, openCVPixelFormat, width, height, codecContext->pix_fmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	sws_scale(imgConvertCtx, openCVFrame->data, openCVFrame->linesize, 0, height, readyToEncodeFrame->data, readyToEncodeFrame->linesize);
	//img_convert((AVPicture *)readToEncodeFrame, codecContext->pix_fmt, (AVPicture *)openCVFrame, openCVPixelFormt, codecContext->width, codecContext->height);

	*outSize = avcodec_encode_video(codecContext, buffer, bufSize, readyToEncodeFrame);
	fprintf(stderr, "Encoded Frame size: %d\n", *outSize);
	return buffer;
}
