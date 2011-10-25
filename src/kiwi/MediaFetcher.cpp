#include "MediaFetcher.h"
#include "SourceList.h"

MediaFetcher::MediaFetcher (SourceList *sourceList) {
	this->sourceList = sourceList;
	nh = new NdnHandler();
	staleOk = true;
	fetchTimer = new QTimer(this);
	fetchTimer->setInterval(40);
	connect(fetchTimer, timeout(), this, fetch());
	fetchTimer->start();
	start();
}

MediaFetcher::~MediaFetcher() {
	if (nh != NULL)
		delete nh;
}

void MediaFetcher::fetch() {
}

void MediaFetcher::run() {
}
