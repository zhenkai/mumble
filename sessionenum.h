#ifndef SESSIONENUM_H
#define SESSIONENUM_H

#ifdef __cplusplus
extern "C" {
#endif
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>
#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <ccn/ccn.h>
#include <ccn/bloom.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#include <ccn/uri.h>
#include <errno.h>
#ifdef __cplusplus
}
#endif

#include "announcement.h"
#include <QThread>
#include <QObject>
#include <QTimer>
#include <QUuid>
#include <QHash>

class SessionEnum: public QThread {
	Q_OBJECT

public:
	void run();
	void startThread();
	void stopThread();
	SessionEnum();
	~SessionEnum();
	void removeFromMyConferences(Announcement *a);
	void addToMyConferences(Announcement *a);
	void addToConferences(Announcement *a, bool pub);
	void handleDismissEvent(struct ccn_upcall_info *info);
	void handleEnumInterest(struct ccn_upcall_info *info);
	void handleEnumPrivateInterest(struct ccn_upcall_info *info);
	bool isConferenceRefresh(unsigned char *hash, bool pub);
	void sendDismissSignal(Announcement *a);
	void setListPrivate(bool b);
	void expressEnumInterest(struct ccn_charbuf *interest, QList<QString> &toExclude, bool privateConf);
	void encodeAnnouncement(struct ccn_charbuf *name, char *buffer, size_t total_len, struct ccn_upcall_info *info);
	bool isFinalBlock(struct ccn_upcall_info *info);
	void fetchRemainingBlocks(struct ccn_closure *selfp, struct ccn_upcall_info *info);
	void decodeAnnouncement(struct ccn_upcall_info *info, bool privateConf);
	void handleEnumContent(const unsigned char *value, size_t len);
	void handleEnumPrivateContent(const unsigned char *value, size_t len, struct ccn_upcall_info *info);

private slots:
	void enumerate();
	void enumeratePriConf();
	void enumeratePubConf();
	void checkAlive();

signals:
	void expired(QString, QString);
	void add(Announcement *);

private:
	void ccnConnect();
	void initKeystoreAndKeylocator();
	void loadPublicAndPrivateKeys();
	static int pubKeyEncrypt(EVP_PKEY *public_key, const unsigned char *data, size_t data_length,
					  unsigned char **encrypted_output, size_t *encrypted_output_length);
	static int priKeyDecrypt(EVP_PKEY *private_key, const unsigned char *ciphertext, size_t ciphertext_length,
					  unsigned char **decrypted_output, size_t *decrypted_output_length);

	static int symDecrypt(const unsigned char *key, const unsigned char *iv, const unsigned char *ciphertext, 
						   size_t ciphertext_length, unsigned char **plaintext, size_t *plaintext_length, 
						   size_t plaintext_padding);

	static int symEncrypt(const unsigned char *key, const unsigned char *iv, const unsigned char *plaintext, 
						   size_t plaintext_length, unsigned char **ciphertext, size_t *ciphertext_length,
						   size_t padding); 

private:
	QList<Announcement *> myConferences;
	QList<Announcement *> myPrivateConferences;
	QList<FetchedAnnouncement *> pubConferences;
	QList<FetchedAnnouncement *> priConferences;
	QHash<QString, bool> unfinishedFetches;
	bool bRunning;
	struct ccn *ccn;
	struct ccn_closure *to_announce;
	struct ccn_closure *to_announce_private;
	struct ccn_closure *fetch_announce;
	struct ccn_closure *fetch_private;
	struct ccn_closure *handle_dismiss;
	struct ccn_keystore *keystore;
	struct ccn_keystore *actd_keystore;
	struct ccn_charbuf *keylocator;
	QTimer *enumTimer;
	QTimer *aliveTimer;
	QString uuid;
	bool listPrivate;
	QList<struct ccn_pkey *>publicKeys;
};


#endif
