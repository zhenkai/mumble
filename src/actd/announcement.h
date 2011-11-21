#ifndef ANNOUNCEMENT_H
#define ANNOUNCEMENT_H

#include <QUuid>
#include <QDateTime>
#include <QDomDocument>
#include <QStringList>

#ifdef __cplusplus
extern "C" {
#endif
#include <openssl/rand.h>
#ifdef __cplusplus
}
#endif

#define KEY_LENGTH 512/8
#define SHA_DIGEST_LENGTH 20

class Announcement
{
	private:

		QString confName;
		QString organizer;
		QString email;
		bool own;
		bool audio;
		bool video;
		bool text;
		QString desc;
		QDate date;
		QTime time;
		int hours;
		int minutes;
		QString uuid;
		bool isPrivate;
		QStringList certs;
		QString opaqueName;
		QString out;


	public:
		Announcement();
		void copy(Announcement *a);

		QString getConfName() { return confName; }
		QString getOrganizer() { return organizer; }
		QString getEmail() { return email; }
		QString getUuid() {return uuid; }
		bool getAudio() { return audio; }
		bool getVideo() { return video; }
		bool getText() { return text; }
		bool getOwner() { return own; }
		QString getDesc() { return desc; }
		QDate getDate() { return date; }
		QTime getTime() { return time; }
		int getHours() { return hours; }
		int getMinutes() { return minutes; }
		bool getIsPrivate() { return isPrivate; }
		bool equalDigest(unsigned char *hash);
		QStringList &getCerts() { return certs; }
		QString getOpaqueName() { return opaqueName; }
		QString getXmlOut() { return out; }


		void setConfName(QString confName) { this->confName = confName; }
		void setOrganizer(QString organizer) { this->organizer = organizer; }
		void setEmail(QString email) { this->email = email; }
		void setUuid(QString uuid) { this->uuid = uuid; }
		void setOwner(bool own) { this->own = own; }
		void setAudio(bool audio) { this->audio = audio; }
		void setVideo(bool video) { this->video = video; }
		void setText(bool text) { this->text = text; }
		void setDesc(QString desc) { this->desc = desc; }
		void setDate(QDate date) { this->date = date; }
		void setTime(QTime time) { this->time = time; }
		void setHours(int hours) { this->hours = hours; }
		void setMinutes(int minutes) { this->minutes = minutes; }
		void setIsPrivate(bool b) { isPrivate = b; }
		void setCerts(QStringList &certs) { this->certs = certs; }
		void setOpaqueName(QString opaqueName) { this->opaqueName = opaqueName; }
		void setXmlOut(QString out) { this->out = out; }
		void setDigest(unsigned char *hash);
		void initConferenceKey();
		void initAudioSessionKey();

		unsigned char conferenceKey[KEY_LENGTH];
		unsigned char audioSessionKey[KEY_LENGTH];
		unsigned char digest[SHA_DIGEST_LENGTH];

};

QDataStream &operator<<(QDataStream &out, Announcement *a);



class FetchedAnnouncement : public Announcement
{
	public:
		FetchedAnnouncement();
		void refreshReceived();
		bool isStaled();
		bool needRefresh();
		bool isDismissed() { return dismissed; }
		void setDismissed(bool dismissed) { this->dismissed = dismissed; }
		bool getIsEligible() { return isEligible; }
		void setIsEligible(bool b) { isEligible = b; }
	
	private:
		QDateTime timestamp;
		bool dismissed;
		bool isEligible;


};

QString &operator<<(QString &out, Announcement *a);
QDomDocument &operator>>(QDomDocument &in, Announcement *a);

#endif
