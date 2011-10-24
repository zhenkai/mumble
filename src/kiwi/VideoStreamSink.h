#ifndef VIDEOSTREAMSINK_H
#define VIDEOSTREAMSINK_H
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include "VideoStreamDecoder.h"
#include "NdnHandler.h"
#include <QThread>
#include <QTimer>
#include <QHash>

#define FRESHNESS 2
#define ALIVE_PERIOD 600

class MediaSource;
class SourceList;

class VideoStreamSink: public QThread {
	Q_OBJECT
public:
	VideoStreamSink();
	virtual ~VideoStreamSink();
	void run();
	//IplImage *getNextFrame(unsigned char *buf, int len);

signals:
	void nextFrameFetched(IplImage *decodedImage, QString name);

private:
	VideoStreamDecoder *decoder;
	NdnHandler *nh;
	SourceList *sourceList;
};

class SourceList: public QThread {
	Q_OBJECT
public:
	SourceList();
	virtual ~SourceList();
	void run();
	friend class VideoStreamSink;
private:
	NdnHandler *nh;
	QHash<QString, MediaSource *> list;
};

class MediaSource: public QObject {
	Q_OBJECT
public:
	MediaSource(QObject *parent, QString prefix, QString username);
	void refreshReceived();
	bool getNeedExclude() {return needExclude; }
signals:
	void alivenessTimeout();
private slots:
	void excludeNotNeeded();
	void noLongerActive();
private:
	QString namePrefix;
	QString username;
	long seq;
	bool needExclude;
	QTimer *freshnessTimer;
	QTimer *alivenessTimer;
};
#endif
