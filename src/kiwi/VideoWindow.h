#ifndef VIDEOWINDOW_H
#define VIDEOWINDOW_H
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QHash>

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
	void refreshImage(QString name, IplImage *image);
	void alterDisplayNumber(QString name, int addOrDel);

private:
	QImage IplImage2QImage(const IplImage *iplImage);
	
private:
	VideoStreamSource *source;
	VideoStreamSink *sink;
	QGridLayout *layout;
	QHash<QString, QNamedFrame *> displays;
};

class QNamedFrame : public QWidget {
	private:
		QLabel *imageLabel;
		QVBoxLayout *layout;
		int pos;
		QString name;
	public:
		QNamedFrame();
		~QNamedFrame();
		void setName(QString name);
		void setImage(QImage image);
		void setPosition(int pos) {this->pos = pos;}
		int getPosition() { return pos;}
};


#endif
