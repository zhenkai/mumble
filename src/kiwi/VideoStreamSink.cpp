#include "VideoStreamSink.h"
#include "Params.h"

VideoStreamSink::VideoStreamSink() {
	sourceList = new SourceList();
	fetcher = new MediaFetcher(sourceList);
	connect(sourceList, SIGNAL(mediaSourceLeft(QString)), this, SLOT(sourceLeft(QString)));
	connect(sourceList, SIGNAL(mediaSourceAdded(QString)), this, SLOT(sourceAdded(QString)));
}

VideoStreamSink::~VideoStreamSink() {
	if (sourceList != NULL)
		delete sourceList;
	if (fetcher != NULL)
		delete fetcher;
}

void VideoStreamSink::sourceLeft(QString name) {
	emit sourceNumChanged(name, 0);
}

void VideoStreamSink::sourceAdded(QString name) {
	emit sourceNumChanged(name, 1);
}



