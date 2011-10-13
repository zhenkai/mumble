#include <QMessageBox>
#include <QPixmap>
#include <QTimer>
#include <QDebug>
#include <QTime>
#include "videowindow.h"

#define MAX_NAME_LEN 30

VideoWindow::VideoWindow(): QDialog(0) {
	layout = new QVBoxLayout(this);
	selfImage = new QNamedFrame();
	selfImage->setName("/usr/local");
	layout->addWidget(selfImage);
	setLayout(layout);
	openCamera(0);
	openOutputFile("/tmp/kiwi.avi");
	openInputFile("/tmp/kiwi.avi");

	QTimer *refresh = new QTimer(this);
	connect(refresh, SIGNAL(timeout()), this, SLOT(refreshImage()));
	refresh->start(25);
}

VideoWindow::~VideoWindow() {
	if (cap.isOpened()) {
		cap.release();
	}
}

int VideoWindow::openCamera(int device) {
	if (cap.isOpened()) {
		cap.release();
	}

	cap.open(device);

	if (!cap.isOpened()) {
		QMessageBox::warning(this, "Can not open camera", QString("Failed to open camera device %1.").arg(device));
		return -1;
	}

	return 0;
}

int VideoWindow::openOutputFile(char *file) {
	writer.open(file, CV_FOURCC('M','P', '4', '2'), 25, cvSize(640, 480));
	if (!writer.isOpened()) {
		QMessageBox::warning(this, "Can not write video stream", QString("Failed to write video stream to %1").arg(file));
		return -1;
	}
	return 0;
}

int VideoWindow::openInputFile(char *file) {
	reader.open(file);
	if (!reader.isOpened()) {
		QMessageBox::warning(this, "Can not read video stream", QString("Failed to read video stream from %1").arg(file));
		return -1;
	}
	return 0;
}

static QImage IplImage2QImage(const IplImage *iplImage) {
	int height = iplImage->height;
	int width = iplImage->width;

	if (iplImage->depth == IPL_DEPTH_8U && iplImage->nChannels == 3) {
		// Must be const to achieve faster QImage construction.
		const uchar *qImageBuffer = (const uchar *)iplImage->imageData;
		// Must specify byesPerLine; otherwise Qt assumes data is 32-bit aligned and each line of data in image is also 32-bit aligned.
		// The default interpretation without bytesPerLine will produces 3 vauge images (red, green blue) instead of a clear one
		QImage img(qImageBuffer, width, height, iplImage->widthStep, QImage::Format_RGB888);
		qWarning() << QString("Image size is %1").arg(iplImage->imageSize);
		return img.rgbSwapped();
	} else {
		qWarning() << QString("Can not convert image, nChannels is %1").arg(iplImage->nChannels);
		return QImage();
	}
}

void VideoWindow::refreshImage() {
	cv::Mat frame;
	cap >> frame;

	writer << frame;
	cv::Mat rFrame;
	reader >> rFrame;

	IplImage iplImage = IplImage(rFrame);
	QImage image = IplImage2QImage(&iplImage);

	selfImage->setImage(image);
}

static void encodeVideo() {
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
