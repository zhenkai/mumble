#include "VideoStreamSource.h"
#define BUF_SIZE 10000000
#define FRAME_PER_SECOND 25 
#define MAX_EDGE_LENGTH 640
#define FRESHNESS 2

CameraVideoInput::CameraVideoInput() {
	initialized = false;
	initCamera();
}

CameraVideoInput::~CameraVideoInput() {
	if (cap != NULL) {
		cvReleaseCapture(&cap);
		cap = NULL;
		initialized = false;
	}
}

bool CameraVideoInput::initCamera() {
	if (initialized == true)
		return true;

	cap = cvCaptureFromCAM(0);
	if (cap == NULL)
		return false;
	initialized = true;
}

IplImage *CameraVideoInput::getNextFrame() {
	if (!initialized)
		initCamera();
	
	if (!cvGrabFrame(cap))
		return NULL;

	IplImage *capImage = cvRetrieveFrame(cap);
	int x = capImage->width;
	int y = capImage->height;
	int max = x > y ? x : y;
	float scale = (float)((float) max / MAX_EDGE_LENGTH);
	int newX = int (x/scale);
	int newY = int (y/scale);
	if (newX % 2 != 0)
		newX++;
	if (newY % 2 != 0)
		newY++;
	CvSize size = cvSize(newX, newY);
	IplImage *retImage = cvCreateImage(size, 8, 3);
	cvResize(capImage, retImage);
	return retImage;
}

VideoStreamSource::VideoStreamSource() {
	seq = 0;
	initialized = false;
	cam = NULL;
	encoder = NULL;
	cam = new CameraVideoInput();
	if (cam == NULL)
		return;
	
	IplImage *image = cam->getNextFrame();
	if (image == NULL)
		return;
	
	encoder = new VideoStreamEncoder(BUF_SIZE, image->width, image->height, FRAME_PER_SECOND);

	if (encoder == NULL)
		return;
	
	nh = new NdnHandler();

	initialized = true;
	cvReleaseImage(&image);
	captureTimer = new QTimer(this);
	connect(captureTimer, SIGNAL(timeout()), this, SLOT(processFrame()));
	captureTimer->start(1000 / FRAME_PER_SECOND);

	start();

}

VideoStreamSource::~VideoStreamSource() {
	if (cam != NULL)
		delete cam;
	if (encoder != NULL)
		delete encoder;
	if (nh != NULL)
		delete nh;
}

void VideoStreamSource::processFrame() {
	IplImage *currentFrame = cam->getNextFrame();
	int frameSize = 0;
	const unsigned char *encodedFrame = encoder->encodeVideoFrame(currentFrame, &frameSize);
	emit frameProcessed((unsigned char *)encodedFrame, frameSize);
	cvReleaseImage(&currentFrame);

	generateNdnContent(encodedFrame, frameSize);

	seq++;
}

void VideoStreamSource::generateNdnContent(const unsigned char *buffer, int len) {
	struct ccn_charbuf *signed_info = ccn_charbuf_create();
	int res = ccn_signed_info_create(signed_info, nh->getPublicKeyDigest(), nh->getPublicKeyDigestLength(), NULL, CCN_CONTENT_DATA, FRESHNESS, NULL, nh->keylocator);
	if (res < 0) {
		fprintf(stderr, "Failed to create signed_info\n");
		abort();
	}
	
	struct ccn_charbuf *path = ccn_charbuf_create();
	ccn_name_from_uri(path, namePrefix.toStdString().c_str());
	ccn_name_append_str(path, "video");
	struct ccn_charbuf *seqBuf = ccn_charbuf_create();
	ccn_charbuf_putf(seqBuf, "%ld", seq);
	ccn_name_append(path, seqBuf->buf, seqBuf->length);
	struct ccn_charbuf *content = ccn_charbuf_create();
	res = ccn_encode_ContentObject(content, path, signed_info, buffer, len, NULL, nh->getPrivateKey());
	if (res) {
		fprintf(stderr, "Failed to create video content\n");
		abort();
	}
	ccn_put(nh->h, content->buf, content->length);

	ccn_charbuf_destroy(&path);
	ccn_charbuf_destroy(&seqBuf);
	ccn_charbuf_destroy(&signed_info);
	ccn_charbuf_destroy(&content);
}

void VideoStreamSource::run() {
	while(true) {
		int res = ccn_run(nh->h, 0);
		usleep(100);
	}
}


