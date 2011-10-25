#ifndef SOURCELIST_H
#define SOURCELIST_H
#include "NdnHandler.h"
#include <QThread>
#include <QTimer>
#include <QHash>
#define FRESHNESS 2
#define ALIVE_PERIOD 600

class MediaFetcher;
class SourceList: public QThread {
	Q_OBJECT
public:
	SourceList();
	virtual ~SourceList();
	void run();
	friend class MediaFetcher;

signals:
	void mediaSourceLeft(QString fullName);
public slots:
	void alivenessTimerExpired(QString);
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
	void alivenessTimeout(QString);
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

static SourceList *pSourceList;

#endif
