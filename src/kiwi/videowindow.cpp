#include <QMessageBox>
#include <QPixmap>
#include <QTimer>
#include <QDebug>
#include <QTime>
#include "VideoWindow.h"

#define MAX_NAME_LEN 30

VideoWindow::VideoWindow(): QDialog(0) {
	layout = new QVBoxLayout(this);
	selfImage = new QNamedFrame();
	selfImage->setName("/usr/local");
	layout->addWidget(selfImage);
	setLayout(layout);
	source = new VideoStreamSource();
	sink = new VideoStreamSink();

	connect(source, SIGNAL(frameProcessed(unsigned char *, size_t)), this, SLOT(refreshImage(unsigned char *, size_t)));
//	QTimer *refresh = new QTimer(this);
//	connect(refresh, SIGNAL(timeout()), this, SLOT(refreshImage()));
//	refresh->start(25);
}

VideoWindow::~VideoWindow() {
}

static QImage IplImage2QImage(const IplImage *iplImage) {
	if (iplImage == NULL)
		return QImage();

	int height = iplImage->height;
	int width = iplImage->width;

	if (iplImage->depth == IPL_DEPTH_8U && iplImage->nChannels == 3) {
		// Must be const to achieve faster QImage construction.
		const uchar *qImageBuffer = (const uchar *)iplImage->imageData;
		// Must specify byesPerLine; otherwise Qt assumes data is 32-bit aligned and each line of data in image is also 32-bit aligned.
		// The default interpretation without bytesPerLine will produces 3 vauge images (red, green blue) instead of a clear one
		QImage img(qImageBuffer, width, height, iplImage->widthStep, QImage::Format_RGB888);
		return img.rgbSwapped();
	} else {
		qWarning() << QString("Can not convert image, nChannels is %1").arg(iplImage->nChannels);
		return QImage();
	}
}

void VideoWindow::refreshImage(unsigned char *buf, size_t len) {
	IplImage *decodedImage = sink->getNextFrame(buf, len);
	QImage image = IplImage2QImage(decodedImage);
	selfImage->setImage(image);
	//cvReleaseImage(&decodedImage);
}


QNamedFrame::QNamedFrame() {
	nameLabel = new QLabel;
	imageLabel = new QLabel;
	layout = new QVBoxLayout;
	layout->addWidget(nameLabel);
	layout->addWidget(imageLabel);
	setLayout(layout);
}

QNamedFrame::~QNamedFrame() {
	delete nameLabel;
	delete imageLabel;
	delete layout;
}

void QNamedFrame::setName(QString name) {
	if (name.size() > MAX_NAME_LEN) {
		name.truncate(MAX_NAME_LEN);
	}
	nameLabel->setText(name);
}

void QNamedFrame::setImage(QImage image) {
	imageLabel->setPixmap(QPixmap::fromImage(image));
}
