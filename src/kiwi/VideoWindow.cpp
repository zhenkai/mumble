#include <QMessageBox>
#include <QPixmap>
#include <QTimer>
#include <QDebug>
#include <QTime>
#include <QPainter>
#include <QRectF>
#include "VideoWindow.h"

#define MAX_NAME_LEN 30

/***********************************
 * displays at most 9 videos for now
 ***********************************/

VideoWindow::VideoWindow(): QDialog(0) {
	layout = new QGridLayout(this);
	source = new VideoStreamSource();
	sink = new VideoStreamSink();
	connect(sink, SIGNAL(imageDecoded(QString, IplImage *)), this, SLOT(refreshImage(QString, IplImage *)));
	connect(sink, SIGNAL(sourceNumChanged(QString, int)), this, SLOT(alterDisplayNumber(QString, int)));
	connect(source, SIGNAL(imageCaptured(QString, IplImage *)), this, SLOT(refreshImage(QString, IplImage *)));
	alterDisplayNumber("Me", 1);
}

VideoWindow::~VideoWindow() {
	if (source != NULL)
		delete source;
	if (sink != NULL)
		delete sink;

	QHash<QString, QNamedFrame*>::const_iterator it = displays.constBegin();
	for(; it != displays.constEnd(); it++) {
		QNamedFrame *nf = NULL;
		nf = it.value();
		if (nf != NULL)
			delete nf;
	}
}

QImage VideoWindow::IplImage2QImage(const IplImage *iplImage) {
	if (iplImage == NULL)
		return QImage();

	int height = iplImage->height;
	int width = iplImage->width;

	int displayNum = displays.size();
	int scaleHeight;
	switch (displayNum) {
	case 1:  
	case 2: scaleHeight = 480; break;
	case 3: 
	case 4:
	case 5:
	case 6: scaleHeight = 320; break;
	default: scaleHeight = 270; 
	}

	if (iplImage->depth == IPL_DEPTH_8U && iplImage->nChannels == 3) {
		// Must be const to achieve faster QImage construction.
		const uchar *qImageBuffer = (const uchar *)iplImage->imageData;
		// Must specify byesPerLine; otherwise Qt assumes data is 32-bit aligned and each line of data in image is also 32-bit aligned.
		// The default interpretation without bytesPerLine will produces 3 vauge images (red, green blue) instead of a clear one
		QImage img(qImageBuffer, width, height, iplImage->widthStep, QImage::Format_RGB888);
		return img.rgbSwapped().scaledToHeight(scaleHeight);
	} else {
		qWarning() << QString("Can not convert image, nChannels is %1").arg(iplImage->nChannels);
		return QImage();
	}
}



void VideoWindow::refreshImage(QString name, IplImage *iplImage) {
	QImage image = IplImage2QImage(iplImage);
	QNamedFrame *nf = NULL;
	nf = displays[name];
	if (nf != NULL) 
		nf->setImage(image);

	cvReleaseImage(&iplImage);
}

/**************
 * 1 | 2 | 5 |
 * 3 | 4 | 6 |
 * 7 | 8 | 9 |
 *************/
static struct coordinates posTable[9] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}, \
	{0, 2}, {1, 2}, {2, 0}, {2, 1}, {2, 2}};

void VideoWindow::alterDisplayNumber(QString name, int addOrDel) {
	// add
	if (addOrDel > 0) {
		if (displays.contains(name)	) {
			qWarning() << "User " + name + " already exists. No display added.\n";
			return;
		}
		qWarning() << "User " + name + " added.\n";
		QNamedFrame *disp = new QNamedFrame();
		disp->setName(name);
		displays.insert(name, disp);
		int displayNum = displays.size();
		disp->setPosition(displayNum);
		struct coordinates co = posTable[displayNum - 1];
		layout->addWidget(disp, co.row, co.col);
	}
	// del
	else {
		if (!displays.contains(name)) {
			qWarning() << "User " + name + " does not exist. No display deleted.\n";
			return;
		}
		else {
			QNamedFrame *disp = displays[name];
			if (disp != NULL) {
				layout->removeWidget(disp);
				int pos = disp->getPosition();
				delete disp;
				displays.remove(name);
				QHash<QString, QNamedFrame *>::const_iterator i = displays.constBegin();
				while(i != displays.constEnd()) {
					QNamedFrame *qf = i.value();
					int qfPos = qf->getPosition();
					if (qfPos > pos) {
						qfPos--;
						qf->setPosition(qfPos);
						struct coordinates co = posTable[qfPos - 1];
						layout->removeWidget(qf);
						layout->addWidget(qf, co.row, co.col);
					}
					i++;
				}
			} else {
				qWarning() << "User frame was set to NULL: " + name + " \n";
				return;
			}
		}
	}

	adjustSize();
}

QNamedFrame::QNamedFrame() {
	imageLabel = new QLabel;
	layout = new QVBoxLayout;
	layout->addWidget(imageLabel);
	setLayout(layout);
}

QNamedFrame::~QNamedFrame() {
	delete imageLabel;
	delete layout;
}

void QNamedFrame::setName(QString name) {
	if (name.size() > MAX_NAME_LEN) {
		name.truncate(MAX_NAME_LEN);
	}
	this->name = name;
}

void QNamedFrame::setImage(QImage image) {
	QPainter *painter = new QPainter(&image);
	painter->setPen(Qt::red);
	painter->setFont(QFont("Helvetica", 16));
	QRectF br;
	painter->drawText(image.rect(), Qt::AlignLeft | Qt::AlignTop, name, &br);
	painter->fillRect(br.x(), br.y(), br.width() + 3, br.height(), Qt::lightGray);
	painter->drawText(image.rect(), Qt::AlignLeft | Qt::AlignTop, name);
	imageLabel->setPixmap(QPixmap::fromImage(image));
	delete painter;
}

