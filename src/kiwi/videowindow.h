#ifndef VIDEOWINDOW_H
#define VIDEOWINDOW_H
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QMap>

#include "VideoStreamSource.h"
#include "VideoStreamSink.h"

#define MAX_USER_NUM 9

struct coordinates {
	int row;
	int col;
};

class QNamedFrame;

class VideoWindow: public QDialog
{
	Q_OBJECT

public:
	VideoWindow();
	~VideoWindow();

public slots:
	void refreshImage(unsigned char *buf, size_t len);
	void alterDisplayNumber(QString name, int addOrDel);

private:
	QImage IplImage2QImage(const IplImage *iplImage);
	
private:
	VideoStreamSource *source;
	VideoStreamSink *sink;
	QGridLayout *layout;
	QMap<QString, QNamedFrame *> displays;
	QNamedFrame *last;
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
