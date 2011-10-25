#include "SourceList.h"

SourceList::SourceList() {
	nh = new NdnHandler();
}

SourceList::~SourceList() {
	if (nh != NULL)
		delete nh;
}

void SourceList::alivenessTimerExpired(QString fullName) {
	MedieSource *ms = list[fullName];	
	if (ms != NULL) {
		delete ms;
	}
	list.remove(fullName);
	emit mediaSourceLeft(fullName);
}

MediaSource::MediaSource(QObject *parent, QString prefix, QString username) {
	needExclude = false;
	namePrefix = prefix;
	this->username = username;
	freshnessTimer = new QTimer(this);
	freshnessTimer->setInterval(FRESHNESS * 1000);
	freshnessTimer->setSingleShot(true);
	alivenessTimer = new QTimer(this);
	alivenessTimer->setInterval(ALIVE_PERIOD * 1000);
	alivenessTimer->setSingleShot(true);
	connect(freshnessTimer, SIGNAL(timeout()), this, SLOT(excludeNotNeeded()));
	connect(alivenessTimer, SIGNAL(timeout()), this, SLOT(noLongerActive()));
	connect(this, SIGNAL(alivenessTimeout(QString)), parent, SLOT(alivenessTimerExpired(QString)));
}

void MediaSource::refreshReceived() {
	needExclude = true;
	freshnessTimer->stop();
	freshnessTimer->start();
	alivenessTimer->stop();
	alivenessTimer->start();
}

void MediaSource::excludeNotNeeded() {
	needExclude = false;
}

void MediaSource::noLongerActive() {
	emit alivenessTimeout(namePrefix + username);	
}
