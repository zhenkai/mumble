#ifndef VIDEOSTREAMSINK_H
#define VIDEOSTREAMSINK_H
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include "VideoStreamDecoder.h"
#include "MediaFetcher.h"
#include "SourceList.h"
#include "NdnHandler.h"
#include <QThread>
#include <QTimer>
#include <QHash>

class VideoStreamSink: public QObject {
	Q_OBJECT
public:
	VideoStreamSink();
	virtual ~VideoStreamSink();

private slots:
	void decodeImage(QString, const unsigned char *buf, int len);
	void sourceLeft(QString);
	void sourceAdded(QString);
	
signals:
	void sourceNumChanged(QString, int);
	void imageDecoded(QString, IplImage *);

private:
	// decode fetched media
	VideoStreamDecoder *decoder;
	// knows where to fetch media
	SourceList *sourceList;
	// fetch media from list
	MediaFetcher *fetcher;
};
#endif
