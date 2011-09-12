#include "announcement.h"
#include "debugbox.h"
#include <QtXml>
#define REFRESH_INTERVAL 30 
#define REMOVE_INTERVAL (2 * REFRESH_INTERVAL + 5) 

Announcement::Announcement()
{
	own = false;
	confName = QString("");
	organizer = QString("");
	email = QString("");
	audio = false;
	video = false;
	desc = QString("");
	uuid = QString("");
	memset(digest, 0, SHA_DIGEST_LENGTH);
	memset(conferenceKey, 0, KEY_LENGTH);
	memset(audioSessionKey, 0, KEY_LENGTH);
}

void Announcement::setDigest(unsigned char *hash) {
	if (hash == NULL)
		return;
	
	
	memcpy(digest, hash, SHA_DIGEST_LENGTH);
	QByteArray d((char *)digest, SHA_DIGEST_LENGTH);
	QString ds(d.toBase64());

	QByteArray h((char *)hash, SHA_DIGEST_LENGTH);
	QString hs(h.toBase64());

	debug("digest is " + ds);
	debug("hash is " + hs);
}

bool Announcement::equalDigest(unsigned char *hash) {

	if (hash == NULL)
		return false;
	
	QByteArray d((char *)digest, SHA_DIGEST_LENGTH);
	QString ds(d.toBase64());

	QByteArray h((char *)hash, SHA_DIGEST_LENGTH);
	QString hs(h.toBase64());

	debug("digest is " + ds);
	debug("hash is " + hs);
	int res = memcmp(digest, hash, SHA_DIGEST_LENGTH);
	if (res == 0) {
		debug("Announcement " + confName + " : yes, this is equal digest!");
		return true;
	}
	
	debug(QString("memcmp result is %1").arg(res));
	return false;
}

void Announcement::copy(Announcement *a) {
	this->own = a->own;
	this->confName = a->confName;
	this->organizer = a->organizer;
	this->email = a->email;
	this->audio = a->audio;
	this->video = a->video;
	this->desc = a->desc;
	this->date = a->date;
	this->time = a->time;
	this->hours = a->hours;
	this->minutes = a->minutes;
	this->uuid = a->uuid;
	this->isPrivate = a->isPrivate;
	this->certs = a->certs;
	this->opaqueName = a->opaqueName;
	this->out = a->out;
	memcpy(this->conferenceKey, a->conferenceKey, KEY_LENGTH);
	memcpy(this->audioSessionKey, a->audioSessionKey, KEY_LENGTH);
	memcpy(this->digest, a->digest, SHA_DIGEST_LENGTH);
}

void Announcement::initConferenceKey() {
	int res = 0;
	while(res == 0) {
		res = RAND_bytes(conferenceKey, KEY_LENGTH);
	}
}

void Announcement::initAudioSessionKey() {
	int res = 0;
	while(res == 0) {
		res = RAND_bytes(audioSessionKey, KEY_LENGTH);
	}
}

QString &operator<<(QString &out, Announcement *a) {
	
	bool audio = a->getAudio();
	bool video = a->getVideo();
	QString confName = a->getConfName();
	QString organizer = a->getOrganizer();
	QString email = a->getEmail();
	QString desc = a->getDesc();
	QDate date = a->getDate();
	QTime time = a->getTime();
	int hours = a->getHours();
	int minutes = a->getMinutes();
	QString uuid = a->getUuid();

	out.append("<conference>");
	out.append("<audio>");
	if (audio) 
		out.append("true");
	else
		out.append("false");
	out.append("</audio>");

	out.append("<video>");
	if (video) 
		out.append("true");
	else
		out.append("false");
	out.append("</video>");

	out.append("<confName>");
	out.append(confName);
	out.append("</confName>");
		
	out.append("<organizer>");
	out.append(organizer);
	out.append("</organizer>");

	out.append("<email>");
	out.append(email);
	out.append("</email>");

	out.append("<desc>");
	out.append(desc);
	out.append("</desc>");

	out.append("<date>");
	QString qsDate = date.toString(Qt::TextDate);
	out.append(qsDate);
	out.append("</date>");

	out.append("<time>");
	QString qsTime = time.toString(Qt::TextDate);
	out.append(qsTime);
	out.append("</time>");

	out.append("<hours>");
	QString qsHours = QString("%1").arg(hours);
	out.append(qsHours);
	out.append("</hours>");

	out.append("<minutes>");
	QString qsMinutes = QString("%1").arg(minutes);
	out.append(qsMinutes);
	out.append("</minutes>");

	out.append("<uuid>");
	out.append(uuid);
	out.append("</uuid>");

	out.append("</conference>");
	return out;
}

QDomDocument &operator>>(QDomDocument &in, Announcement *a) {
	
	QDomElement docElem = in.documentElement(); // <conference>
	QDomNode node = docElem.firstChild(); // <audio>

	while(!node.isNull()) {
		QString attr = node.nodeName();
		if (attr == "audio") {
			QString bAudio = node.toElement().text();
			if (bAudio == "true") 
				a->setAudio(true);
			else
				a->setAudio(false);
		} else
		if (attr == "video") {
			QString bVideo = node.toElement().text();
			if (bVideo == "true") 
				a->setVideo(true);
			else
				a->setVideo(false);
		} else
		if (attr == "confName") {
			QString confName = node.toElement().text();
			a->setConfName(confName);
		} else
		if (attr == "organizer") {
			QString organizer = node.toElement().text();
			a->setOrganizer(organizer);
		} else
		if (attr == "email") {
			QString email = node.toElement().text();
			a->setEmail(email);
		} else
		if (attr == "date") {
			QString qsDate = node.toElement().text();
			QDate date = QDate::fromString(qsDate, Qt::TextDate);
			a->setDate(date);
		} else
		if (attr == "time") {
			QString qsTime = node.toElement().text();
			QTime time = QTime::fromString(qsTime, Qt::TextDate);
			a->setTime(time);
		} else
		if (attr == "hours") {
			QString qsHours = node.toElement().text();
			int hours = qsHours.toInt();
			a->setHours(hours);
		} else
		if (attr == "minutes") {
			QString qsMinutes = node.toElement().text();
			int minutes = qsMinutes.toInt();
			a->setMinutes(minutes);
		} else 
		if (attr == "desc" ) {
			QString desc = node.toElement().text();
			a->setDesc(desc);
		} else
		if (attr == "uuid" ) {
			QString uuid = node.toElement().text();
			a->setUuid(uuid);
		}
		else
		{
			critical(QString("Unknown xml attribute: %1").arg(attr));
		}

		node = node.nextSibling();
	}

	return in;
}



FetchedAnnouncement::FetchedAnnouncement()
{
	Announcement();
	timestamp = QDateTime::currentDateTime();
	dismissed = false;
}

void FetchedAnnouncement::refreshReceived() {
	timestamp = QDateTime::currentDateTime();	
}

bool FetchedAnnouncement::needRefresh() {
	QDateTime now = QDateTime::currentDateTime();
	if (timestamp.secsTo(now) > REFRESH_INTERVAL) {
		return true;
	}
	return false;
}

bool FetchedAnnouncement::isStaled() {
	QDateTime now = QDateTime::currentDateTime();
	if (timestamp.secsTo(now) > REMOVE_INTERVAL) {
		return true;
	}
	return false;
}

