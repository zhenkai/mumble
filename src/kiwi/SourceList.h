#ifndef SOURCELIST_H
#define SOURCELIST_H
#include "NdnHandler.h"
#include <QThread>
#include <QTimer>
#include <QHash>
#include <QDir>
#include <QFile>
#include <QtXml>
#define FRESHNESS 2
#define ALIVE_PERIOD 600

class MediaSource;
class MediaFetcher;
class SourceList: public QThread {
	Q_OBJECT
public:
	SourceList();
	virtual ~SourceList();

	void run();
	friend class MediaFetcher;
	void stopAcceptingStaleData() {acceptStaleData = false;}
	void handleSourceInfoContent(struct ccn_upcall_info *);
	void enumerate();

private:
	int parseSourceInfoContent(const unsigned char *value, size_t len);
	int addMediaSource(QString username, QString prefix);
	int removeMediaSource(QString username);
	void readNdnParams();
	void expressEnumInterest(struct ccn_charbuf *interest, QList<QString> &toExclude);

signals:
	void mediaSourceLeft(QString fullName);
	void mediaSourceAdded(QString fullName);
public slots:
	void alivenessTimerExpired(QString);
private:
	NdnHandler *nh;
	QString confName;
	QHash<QString, MediaSource *> list;
	struct ccn_closure *discoverClosure;
	bool acceptStaleData;
};

class MediaSource: public QObject {
	Q_OBJECT
public:
	MediaSource(QObject *parent, QString prefix, QString username);
	void refreshReceived();
	bool getNeedExclude() {return needExclude; }
	QString getUsername() {return username; }
	QString getPrefix() {return namePrefix;}
	void setPrefix(QString prefix) {namePrefix = prefix; }
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

static SourceList *gSourceList;
static enum ccn_upcall_res handle_source_info_content(struct ccn_closure *selfp,
												   enum ccn_upcall_kind kind,
												   struct ccn_upcall_info *info);
#endif
