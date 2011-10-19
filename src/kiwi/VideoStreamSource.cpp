#include "VideoStreamSource.h"
#define BUF_SIZE 10000000
#define FRAME_RATE 25

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
	float scale = (float)((float) max / 640);
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
	
	encoder = new VideoStreamEncoder(BUF_SIZE, image->width, image->height, FRAME_RATE);

	if (encoder == NULL)
		return;
	
	initialized = true;
	cvReleaseImage(&image);
	captureTimer = new QTimer(this);
	connect(captureTimer, SIGNAL(timeout()), this, SLOT(processFrame()));
	captureTimer->start(1000 / FRAME_RATE);

}

void VideoStreamSource::processFrame() {
	IplImage *currentFrame = cam->getNextFrame();
	int frameSize = 0;
	const unsigned char *encodedFrame = encoder->encodeVideoFrame(currentFrame, &frameSize);
	emit frameProcessed((unsigned char *)encodedFrame, frameSize);
	cvReleaseImage(&currentFrame);

}

void VideoStreamSource::run() {
	while(true);
}


