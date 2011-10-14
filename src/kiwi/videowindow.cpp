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

static void encodeVideo(const IplImage *iplImage) {

	AVCodec *codec = avcodec_find_encoder(CODEC_ID_H264);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}
	
	AVCodecContext *context = avcodec_alloc_context();
	AVFrame *picture = avcodec_alloc_frame();

	context->bit_rate = 400000;
	context->width = 352;
	context->height = 288;
	context->time_base = (AVRational){1, 25};
	// emit one intra frame every 10 frames (??)
	context->gop_size = 10; 
	context->max_b_frames = 1;
	context->pix_fmt = PIX_FMT_BGR24;

	if (avcodec_open(context, codec) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	int nBytes = 100000;
	uchar *buffer = new uchar[nBytes];
	avpicture_fill((AVPicture *)picture, iplImage->imageData, PIX_FMT_BGR24, 352, 288);
	int outputSize = avcodec_encode_video(context, buffer, nBytes, picture);
	// now the encoded stuff is in buffer with length outputSize
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
