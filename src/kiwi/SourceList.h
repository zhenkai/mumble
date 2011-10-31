#ifndef SOURCELIST_H
#define SOURCELIST_H
#include "NdnHandler.h"
#include <QThread>
#include <QTimer>
#include <QHash>
#include <QDir>
#include <QFile>
#include <QtXml>
#include <pthread.h>
#include <poll.h>

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
	void mediaSourceLeft(QString);
	void mediaSourceAdded(QString);
public slots:
	void alivenessTimerExpired(QString);
private:
	QString username;
	NdnHandler *nh;
	QString confName;
	QHash<QString, MediaSource *> list;
	struct ccn_closure *discoverClosure;
	bool acceptStaleData;
	bool bRunning;
};

class MediaSource: public QObject {
	Q_OBJECT
public:
	MediaSource(QObject *parent, QString prefix, QString username);
	~MediaSource();
	void refreshReceived();
	bool isStreaming() { return streaming; }
	bool getNeedExclude() {return needExclude; }
	QString getUsername() {return username; }
	QString getPrefix() {return namePrefix;}
	void setPrefix(QString prefix) {namePrefix = prefix; }
	void setStreaming(bool streaming) {this->streaming = streaming;}
	void incSeq() {seq ++;}
	void setSeq(long seq) {this->seq = seq;}
	long getSeq() {return seq;}
	void incTimeouts(){consecutiveTimeouts++;}
	void resetTimeouts() {consecutiveTimeouts = 0;}
	int getTimeouts() {return consecutiveTimeouts;}
	friend class MediaFetcher;
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
	struct ccn_closure *fetchClosure;
	struct ccn_closure *fetchPipelineClosure;
	bool streaming;
	int consecutiveTimeouts;
	QHash<long, unsigned char *> frameBuffers;

};


#endif
