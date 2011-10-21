#include <QMessageBox>
#include <QPixmap>
#include <QTimer>
#include <QDebug>
#include <QTime>
#include "VideoWindow.h"

#define MAX_NAME_LEN 30

/***********************************
 * displays at most 9 videos for now
 ***********************************/

VideoWindow::VideoWindow(): QDialog(0) {
	last = NULL;
	layout = new QGridLayout(this, 3, 3);
	source = new VideoStreamSource();
	sink = new VideoStreamSink();
	connect(source, SIGNAL(frameProcessed(unsigned char *, size_t)), this, SLOT(refreshImage(unsigned char *, size_t)));

}

VideoWindow::~VideoWindow() {
}

QImage VideoWindow::IplImage2QImage(const IplImage *iplImage) {
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
	cvReleaseImage(&decodedImage);
}

/**************
 * 1 | 2 | 5 |
 * 3 | 4 | 6 |
 * 7 | 8 | 9 |
 *************/
static struct coordinates displayTable[9] = {{0, 0}, {0, 1}, {1, 0}, {1, 1} \
	{0, 2}, {1, 2}, {2, 0}, {2, 1}, {2, 2}};

void VideoWindow::alterDisplayNumber(QString name, int addOrDel) {
	// add
	if (addOrDel > 0) {
		if (displays.contains(name)	) {
			QWarning() << "User " + name + " already exists. No display added.\n";
			return;
		}
		QNamedFrame *disp = new QNamedFrame();
		disp->setName(name);
		displays.insert(name, disp);
		int displayNum = displays.size();
		struct coordinates co = displayTable[displayNum - 1];
		layout->addWidget(disp, co.row, co.col);
		last = disp;
	}
	// del
	else {
		if (!displays.contains(name)) {
			QWarning() << "User " + name + " does not exist. No display deleted.\n";
			return;
		}
		else {
			QNamedFrame *disp = displays[name];
			if (disp != NULL) {
				struct coordinates co = disp->getCoordinates();
				layout->removeWidget(disp);
				layout->removeWidget(last);
				layout->addWidget(last, co.row, co.col);
				delete disp;
			}
			displays.remove(name);
		}
	}

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

