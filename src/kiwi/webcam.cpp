/**
 * Display video from webcam
 * Author Nash
 * License GPL
 */
#include <stdio.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

void cvShowMultipleImages(char *title, int nArgs, ...) {
	IplImage *img;

	IplImage *DispImage;

	int size;
	int width, height;

	if (nArgs <= 0) {
		fprintf(stderr, "Number of arguments too small.\n");
		return;
	}
	else if (nArgs > 12) {
		fprintf(stderr, "Number of arguments too large.\n");
		return;
	}
	switch (nArgs) {
	case 1: width = 1; height = 1; size = 300; break;
	case 2: width = 2; height = 1; size = 300; break;
	case 3: 
	case 4: width = 2; height = 2; size = 300; break;
	case 5:
	case 6: width = 3; height = 2; size = 200; break;
	case 7:
	case 8: width = 4; height = 2; size = 200; break;
	case 9:
	case 10:
	case 11:
	case 12: width = 4; height = 3; size = 150; break;
	default: break;
	}

	DispImage = cvCreateImage(cvSize(100 + size * width, 60 + size * height), 8, 3);

	va_list args;
	va_start(args, nArgs);
	for (int i = 0, m = 20, n = 20; i < nArgs; i++, m += (20 + size)) {
		img = va_arg(args, IplImage*);

		if(img == NULL) {
			fprintf(stderr, "Invalid arguments");
			cvReleaseImage(&DispImage);
			return;
		}

		int x = img->width;
		int y = img->height;

		int max = (x > y) ? x : y;

		float scale = (float)((float) max / size);
		if (i % width == 0 && m != 20) {
			m = 20;
			n += 20 + size;
		}
		
		cvSetImageROI(DispImage, cvRect(m, n, (int) (x/scale), int (y/scale)));
		cvResize(img, DispImage);
		cvResetImageROI(DispImage);
	}

	cvShowImage(title, DispImage);
	va_end(args);
	cvReleaseImage(&DispImage);
}

int main(int argc, char **argv) {
	CvCapture *capture = 0;
	IplImage *frame = 0;
	int key = 0;

	/* initialize camera */
	capture = cvCaptureFromCAM(0);

	/* always check */
	if (!capture) {
		fprintf(stderr, "Cannot initialize webcam!\n");
		return 1;
	}

	while (key != 'q') {
		/* get a frame */
		frame = cvQueryFrame(capture);

		/* always check */
		if (!frame) break;

		/* display current frame */
	//	cvShowImage("result", frame);
		cvShowMultipleImages("result", 4, frame, frame, frame, frame);

		/* exit if user press 'q' */
		key = cvWaitKey(1);
	}

	/* free memory */
	cvDestroyWindow("result");
	cvReleaseCapture(&capture);
	return 0;
}
