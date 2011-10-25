#include "SourceList.h"

SourceList::SourceList() {
	nh = new NdnHandler();
	start();
}

SourceList::~SourceList() {
	if (nh != NULL)
		delete nh;
}

void SourceList::readNdnParams() {
	QDomDocument settings;
	QString configFileName = QDir::homePath() + "/.actd/.config";
	QFile config(configFileName);
	if (!config.exists()) {
		fprintf(stderr, "Config file does not exist\n");
		abort();
	}
	if (!config.open(QIODevice::ReadOnly)) {
		fprintf(stderr, "Can not open config file\n");
		abort();
	}
	if (!settings.setContent(&config)) {
		config.close();
		fprintf(stderr, "Can not parse config file\n");
		abort();
	}

	QDomElement docElem = settings.documentElement();
	QDomNode n = docElem.firstChild();
	while (!n.isNull()) {
		if (n.nodeName() == "confName") {
			confName = n.toElement().text();
		}
		n = n.nextSibling();
	}

	if (confName == "") {
		fprintf(stderr, "Null conference name\n");
		abort();
	}
}

void SourceList::alivenessTimerExpired(QString fullName) {
	MedieSource *ms = list[fullName];	
	if (ms != NULL) {
		delete ms;
	}
	list.remove(fullName);
	emit mediaSourceLeft(fullName);
}

SourceList::run() {
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
