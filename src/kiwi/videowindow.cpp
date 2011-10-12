#include <QMessageBox>
#include <QPixmap>
#include <QTimer>
#include "videowindow.h"

#define MAX_NAME_LEN 30

VideoWindow::VideoWindow(): QDialog(0) {
	layout = new QVBoxLayout(this);
	selfImage = new QNamedFrame();
	selfImage->setName("/usr/local");
	layout->addWidget(selfImage);
	setLayout(layout);
	openCamera(0);

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

void VideoWindow::refreshImage() {
	cv::Mat frame;
	cap >> frame;
	int width = frame.cols;
	int height = frame.rows;
	int size = width * height;
	QImage image = QImage((const uchar *)frame.ptr<uchar>(0), frame.cols, frame.rows, QImage::Format_RGB888).rgbSwapped();
	selfImage->setImage(image);
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
