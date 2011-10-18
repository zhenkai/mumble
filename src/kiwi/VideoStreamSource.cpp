#include "VideoStreamSource.h"
#define BUF_SIZE 1000000
#define FRAME_RATE 25

CameraVideoInput::CameraVideoInput() {
	initialized = false;
	initCamera();
}

CameraVideoInput::~CameraVideoInput() {
	if (cap != NULL) {
		cvCaptureRelease(&cap);
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

IplImage *getNextFrame() {
	if (!initialzed)
		initCamera();
	
	if (!cvGrabFrame(cap))
		return NULL;

	IplImage *capImage = cvRetrieveFrame(cap);
	IplImage *retImage = cvCreateImage(cvGetSize(capImage), 8, 3);
	cvCopy(capImage, retImage, 0);
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
	
	encoder = new VideoStreamEncoder(BUF_SIZE, image->width, image->height, FRAME_RATE);

	if (encoder == NULL)
		return;
	
	initialized = true;
	cvRelease(image);
	captureTimer = new QTimer(this);
	connect(captureTimer, SIGNAL(timeout()), this, SLOT(processFrame()));
	captureTimer->start(1000 / FRAME_RATE);

}

void VideoStreamSource::processFrame() {
	IplImage *currentFrame = cam->getNextFrame();
	int frameSize = 0;
	unsigned char *encodedFrame = encoder->encodeVideoFrame(currentFrame, & frameSize);
	emit fameProcessed(encodedFrame, frameSize);
	cvReleaseImage(&currentFrame);

}

void VideoStreamSource::run() {
	while(true);
}


