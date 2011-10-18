#ifndef VIDEOWINDOW_H
#define VIDEOWINDOW_H
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>

#include <opencv2/opencv.hpp>
#include "VideoStreamSource.h"
#include "VideoStreamSink.h"

#define MAX_USER_NUM 12

class QNamedFrame;

class VideoWindow: public QDialog
{
	Q_OBJECT

public:
	VideoWindow();
	~VideoWindow();
	int openCamera(int device);
	int openOutputFile(char *file);
	int openInputFile(char *file);

private slots:
	void refreshImage(unsigned char *buf, size_t len);

private:
	VideoStreamSource *source;
	VideoStreamSink *sink;
	QVBoxLayout *layout;
	QNamedFrame *selfImage;
};

class QNamedFrame : public QWidget {
	private:
		QLabel *nameLabel;
		QLabel *imageLabel;
		QVBoxLayout *layout;
	public:
		QNamedFrame();
		~QNamedFrame();
		void setName(QString name);
		void setImage(QImage image);
};

#endif
