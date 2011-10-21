#ifndef VIDEOSTREAMSOURCE_H
#define VIDEOSTREAMSOURCE_H
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <QTimer>
#include <QThread>
extern "C" {
#include <ccn/ccn.h>
#include <ccn/keystore.h>
#include <ccn/uri.h>
}

#include "VideoStreamEncoder.h"
#include "NdnHandler.h"

class CameraVideoInput{
public:
	CameraVideoInput();
	virtual ~CameraVideoInput();
	IplImage *getNextFrame();
private:
	bool initCamera();
private:
	CvCapture *cap;
	bool initialized;
};

class VideoStreamSource: public QThread{
	Q_OBJECT
public:
	void run();	
	VideoStreamSource();
	~VideoStreamSource();
	void setNamePrefix(QString prefix) {namePrefix = prefix;}

signals:
	void frameProcessed(unsigned char *buf, size_t len);


private slots:
	void processFrame();

private:
	void generateNdnContent(const unsigned char *buffer, int len);

private:
	CameraVideoInput *cam;
	VideoStreamEncoder *encoder;
	NdnHandler *nh;
	QTimer *captureTimer;
	struct ccn_closure *publish;	
	long seq;
	bool initialized;
	QString namePrefix;
};
#endif
