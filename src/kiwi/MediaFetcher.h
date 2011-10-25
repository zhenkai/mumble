#ifndef MEDIAFETCHER_H
#define MEDIAFETCHER_H
#include <QThread>
#include <QTimer>
#include <QHash>

class MediaFetcher: public QThread {
	Q_OBJECT
public:
	MediaFetcher(SourceList *sourceList);
	~MediaFetcher();
	void run();
private slots:
	void fetch();

private:
	NdnHandler *nh;
	SourceList *sourceList;
	QTimer *fetchTimer;
	bool staleOk;
};

static MediaFetcher *pMediaFetcher;
#endif
