#ifndef MEDIAFETCHER_H
#define MEDIAFETCHER_H
#include <QThread>
#include <QTimer>
#include <QHash>
#include "NdnHandler.h"
#include "SourceList.h"

class MediaFetcher: public QThread {
	Q_OBJECT
public:
	MediaFetcher(SourceList *sourceList);
	~MediaFetcher();
	void run();
	void initPipe(struct ccn_closure *selfp, struct ccn_upcall_info *info);
	void processContent(struct ccn_closure *selfp, struct ccn_upcall_info *info);
private:
	void initStream();

signals:
	void contentArrived(QString, const unsigned char *content, size_t len);

private slots:
	void fetch();

private:
	NdnHandler *nh;
	SourceList *sourceList;
	QTimer *fetchTimer;
	bool staleOk;
};


#endif
