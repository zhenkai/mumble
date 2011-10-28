#include "sessionenum.h"
#include <QDomDocument>
#include "debugbox.h"
#include <QIODevice>
#include <QtXml>
#include <signal.h>
#include <execinfo.h>
#include <stdlib.h>
#include <poll.h>


#define BROADCAST_PREFIX ("/ndn/broadcast/conference")
#define FRESHNESS 10

struct fetched_data {
	unsigned char *value;
	size_t len;
	int seq;
	bool privateConf;
};
static struct pollfd pfds[1];
static pthread_mutex_t actd_mutex; 
static pthread_mutexattr_t ccn_attr;


static int namecompare(const void *a, const void *b);

static SessionEnum *gsd = NULL;

static void append_bf_all(struct ccn_charbuf *c);

static void mutex_trylock() {
	int c = 0;
	while(pthread_mutex_trylock(&actd_mutex) != 0) {
		usleep(200);
		c++;
		if (c> 10000) {
			fprintf(stderr, "cannot obtain lock %s: %d \n", __FILE__, __LINE__);
			abort();
		}
	}
}

static void mutex_unlock() {
	pthread_mutex_unlock(&actd_mutex);
}

static char *ccn_name_comp_to_str(const unsigned char *ccnb,
								  const struct ccn_indexbuf *comps,
								  int index);

static enum ccn_upcall_res dismiss_signal(struct ccn_closure *selfp,
										enum ccn_upcall_kind kind,
										struct ccn_upcall_info *info);

static enum ccn_upcall_res incoming_interest(struct ccn_closure *selfp,
											enum ccn_upcall_kind kind,
											struct ccn_upcall_info *info);

static enum ccn_upcall_res incoming_private_interest(struct ccn_closure *selfp,
													enum ccn_upcall_kind kind,
													struct ccn_upcall_info *info);

static enum ccn_upcall_res incoming_content(struct ccn_closure *selfp,
											enum ccn_upcall_kind kind,
											struct ccn_upcall_info *info);

static enum ccn_upcall_res incoming_private_content(struct ccn_closure *selfp,
													enum ccn_upcall_kind kind,
													struct ccn_upcall_info *info);


/*
 * Comparison operator for sorting the excl list with qsort.
 * For convenience, the items in the excl array are
 * charbufs containing ccnb-encoded Names of one component each.
 * (This is not the most efficient representation.)
 */
static int /* for qsort */
namecompare(const void *a, const void *b)
{
    const struct ccn_charbuf *aa = *(const struct ccn_charbuf **)a;
    const struct ccn_charbuf *bb = *(const struct ccn_charbuf **)b;
    int ans = ccn_compare_names(aa->buf, aa->length, bb->buf, bb->length);
    if (ans == 0)
        fprintf(stderr, "wassat? %d\n", __LINE__);
    return (ans);
}


/*
 * This appends a tagged, valid, fully-saturated Bloom filter, useful for
 * excluding everything between two 'fenceposts' in an Exclude construct.
 */
static void
append_bf_all(struct ccn_charbuf *c)
{
    unsigned char bf_all[9] = { 3, 1, 'A', 0, 0, 0, 0, 0, 0xFF };
    const struct ccn_bloom_wire *b = ccn_bloom_validate_wire(bf_all, sizeof(bf_all));
    if (b == NULL) abort();
    ccn_charbuf_append_tt(c, CCN_DTAG_Bloom, CCN_DTAG);
    ccn_charbuf_append_tt(c, sizeof(bf_all), CCN_BLOB);
    ccn_charbuf_append(c, bf_all, sizeof(bf_all));
    ccn_charbuf_append_closer(c);
}

static char *ccn_name_comp_to_str(const unsigned char *ccnb,
								  const struct ccn_indexbuf *comps,
								  int index) {
	size_t comp_size;
	const unsigned char *comp_ptr;
	char *str;
	if (ccn_name_comp_get(ccnb, comps, index, &comp_ptr, &comp_size) == 0) {
		str = (char *)malloc(sizeof(char) *(comp_size + 1));
		strncpy(str, (const char *)comp_ptr, comp_size);
		str[comp_size] = '\0';
		return str;
	}
	else {
		debug("can not get name comp");
		return NULL;
	}
}

void SessionEnum::setListPrivate(bool b) {
	listPrivate = b;
	if (listPrivate) {
		enumeratePriConf();
	}
	if (!listPrivate && priConferences.size() > 0) {
		for (int i = 0; i < priConferences.size(); i ++) {
			Announcement *a = priConferences.at(i);
			if (a != NULL) {
				QString confName = a->getConfName();
				QString organizer = a->getOrganizer();
				emit expired(confName, organizer);
				free(a);
			}

		}
		priConferences.clear();
	}
}

void SessionEnum::initKeystoreAndKeylocator() {
	// prepare for ccnx
	keystore = NULL;
	ccn_charbuf *temp = ccn_charbuf_create();
	keystore = ccn_keystore_create();
	ccn_charbuf_putf(temp, "%s/.ccnx/.ccnx_keystore", getenv("HOME"));
	int res = ccn_keystore_init(keystore,
				ccn_charbuf_as_string(temp),
				(char *)"Th1s1sn0t8g00dp8ssw0rd.");
	if (res != 0) {
	    printf("Failed to initialize keystore %s\n", ccn_charbuf_as_string(temp));
	    exit(1);
	}
	ccn_charbuf_destroy(&temp);
	
	keylocator = ccn_charbuf_create();
	ccn_charbuf_append_tt(keylocator, CCN_DTAG_KeyLocator, CCN_DTAG);
	ccn_charbuf_append_tt(keylocator, CCN_DTAG_Key, CCN_DTAG);
	res = ccn_append_pubkey_blob(keylocator, ccn_keystore_public_key(keystore));
	if (res < 0) {
		ccn_charbuf_destroy(&keylocator);
	}else {
		ccn_charbuf_append_closer(keylocator);
		ccn_charbuf_append_closer(keylocator);
	}



	// public & private key pair for actd
	actd_keystore = NULL;
	QString keystoreFilename = QString("%1/.actd/.actd_keystore").arg(getenv("HOME"));
	QFile keystoreFile(keystoreFilename);
	QDir actdDir(QString("%1/.actd").arg(getenv("HOME")));
	// if no keystore, create one
	if (!actdDir.exists()) {
		QDir homeDir = QDir::home();
		homeDir.mkdir(".actd");
	}
	if (!keystoreFile.exists()) {
		res = ccn_keystore_file_init((char *)keystoreFilename.toStdString().c_str(), (char *)"Th1s1s@p8ssw0rdf0r8ctd.", (char *)getenv("USER"), 1024, 365);
		if (res < 0)
			critical("Failed to initialize keystore file for actd.");
	}
	actd_keystore = ccn_keystore_create();
	temp = ccn_charbuf_create();
	ccn_charbuf_putf(temp, "%s/.actd/.actd_keystore", getenv("HOME"));
	res = ccn_keystore_init(actd_keystore,
								ccn_charbuf_as_string(temp),
								(char *)"Th1s1s@p8ssw0rdf0r8ctd.");
	if (res != 0) 
		critical("Failed to initialze keystore for actd");

	ccn_charbuf_destroy(&temp);
}



static void append_bloom_filter(struct ccn_charbuf *templ, struct ccn_bloom *b) {
    ccn_charbuf_append_tt(templ, CCN_DTAG_Exclude, CCN_DTAG);
    ccn_charbuf_append_tt(templ, CCN_DTAG_Bloom, CCN_DTAG);
    int wireSize = ccn_bloom_wiresize(b);
    ccn_charbuf_append_tt(templ, wireSize, CCN_BLOB);
    ccn_bloom_store_wire(b, ccn_charbuf_reserve(templ, wireSize), wireSize);
    templ->length += wireSize;
    ccn_charbuf_append_closer(templ);
    ccn_charbuf_append_closer(templ);
}


static enum ccn_upcall_res dismiss_signal(struct ccn_closure *selfp,
										enum ccn_upcall_kind kind,
										struct ccn_upcall_info *info) {
	switch (kind) {
	case CCN_UPCALL_FINAL:
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST: {
		// /ndn/broadcast/conference/dismiss/uuid/confName/organizer
		debug("dismiss interest received");
		gsd->handleDismissEvent(info);
		return (CCN_UPCALL_RESULT_OK);
	}
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
}

static enum ccn_upcall_res incoming_interest(struct ccn_closure *selfp,
											enum ccn_upcall_kind kind,
											struct ccn_upcall_info *info) {

	switch (kind) {
	case CCN_UPCALL_FINAL:
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST:
	{

		debug("incoming public interest");
		gsd->handleEnumInterest(info);		
		return (CCN_UPCALL_RESULT_OK);
	}
	
	default:
		return (CCN_UPCALL_RESULT_OK);

	}
}

static enum ccn_upcall_res incoming_private_interest(struct ccn_closure *selfp,
													enum ccn_upcall_kind kind,
													struct ccn_upcall_info *info) {

	switch (kind) {
	case CCN_UPCALL_FINAL:
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST:
	{
		debug("incoming private interest");

		gsd->handleEnumPrivateInterest(info);		
		return (CCN_UPCALL_RESULT_OK);
	}
	
	default:
		return (CCN_UPCALL_RESULT_OK);

	}
}

static enum ccn_upcall_res incoming_content(struct ccn_closure *selfp,
											enum ccn_upcall_kind kind,
											struct ccn_upcall_info *info) {

	switch (kind) {
	case CCN_UPCALL_FINAL:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);

	case CCN_UPCALL_CONTENT: {
		debug("incoming public content");
		gsd->decodeAnnouncement(info, false);
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
	{
		debug("unverified content!");
		return (CCN_UPCALL_RESULT_OK);

	}
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
}


static enum ccn_upcall_res incoming_private_content(struct ccn_closure *selfp,
													enum ccn_upcall_kind kind,
													struct ccn_upcall_info *info) {

	switch (kind) {
	case CCN_UPCALL_FINAL:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);

	case CCN_UPCALL_CONTENT: {
		debug("incoming private content");
		gsd->decodeAnnouncement(info, true);
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
	{
		debug("unverified content!");
		return (CCN_UPCALL_RESULT_OK);

	}
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
}

static enum ccn_upcall_res fetched_block(struct ccn_closure *selfp,
										 enum ccn_upcall_kind kind,
										 struct ccn_upcall_info *info) {
	switch (kind) {
	case CCN_UPCALL_FINAL:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		selfp->intdata ++;
		if (selfp->intdata > 5)
			return (CCN_UPCALL_RESULT_OK);
		return (CCN_UPCALL_RESULT_REEXPRESS);

	case CCN_UPCALL_CONTENT: {
		debug("incoming private content");
		gsd->fetchRemainingBlocks(selfp, info);
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
	{
		debug("unverified content!");
		return (CCN_UPCALL_RESULT_OK);

	}
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
}

void SessionEnum::fetchRemainingBlocks(struct ccn_closure *selfp, struct ccn_upcall_info *info) {
	struct fetched_data *data = (struct fetched_data *)selfp->data;
	const unsigned char *value = NULL;
	size_t len = 0;
	int res =ccn_content_get_value(info->content_ccnb, info->pco->offset[CCN_PCO_E],
						info->pco, &value, &len);
	if (res < 0)
		critical("failed to parse content object");

	data->value = (unsigned char *)realloc(data->value, data->len + len);
	memcpy(data->value + data->len, value, len);
	data->len += len;
	data->seq ++;

	if (isFinalBlock(info)) {
		const unsigned char *cb = info->content_ccnb;
		struct ccn_indexbuf *cc = info->content_comps;
		QString cname = ccn_name_comp_to_str(cb, cc, 4);

		unfinishedFetches.remove(cname);
		const unsigned char *content = data->value;
		if (data->privateConf) {
			handleEnumPrivateContent(content, data->len, info);
		}
		else {
			handleEnumContent(content, data->len);
		}
		free(data);
		free(selfp);
		return;
	}

	const unsigned char *cb = info->content_ccnb;
	struct ccn_indexbuf *cc = info->content_comps;
	struct ccn_charbuf *name = ccn_charbuf_create();
	ccn_name_init(name);
	ccn_name_append_components(name, cb, cc->buf[0], cc->buf[cc->n - 2]);
	struct ccn_charbuf *seq = ccn_charbuf_create();
	ccn_charbuf_putf(seq, "%d", data->seq);
	ccn_name_append(name, seq->buf, seq->length);
	// already have lock
	res = ccn_express_interest(info->h, name, selfp, NULL);
	if (res < 0)
		abort();
	ccn_charbuf_destroy(&name);
	ccn_charbuf_destroy(&seq);
}

bool SessionEnum::isFinalBlock(struct ccn_upcall_info *info) {
	const unsigned char *ccnb;
	size_t ccnb_size;
	ccnb = info->content_ccnb;
	ccnb_size = info->pco->offset[CCN_PCO_E];
	if (info->pco->offset[CCN_PCO_B_FinalBlockID] != info->pco->offset[CCN_PCO_E_FinalBlockID]) {
		const unsigned char *finalid = NULL;
		size_t finalid_size = 0;
		const unsigned char *nameid = NULL;
		size_t nameid_size = 0;
		struct ccn_indexbuf *cc = info->content_comps;
		ccn_ref_tagged_BLOB(CCN_DTAG_FinalBlockID, ccnb, info->pco->offset[CCN_PCO_B_FinalBlockID], info->pco->offset[CCN_PCO_E_FinalBlockID], &finalid, &finalid_size);
		if (cc->n < 2) abort();
		ccn_ref_tagged_BLOB(CCN_DTAG_Component, ccnb, cc->buf[cc->n - 2], cc->buf[cc->n -1], &nameid, &nameid_size);
		if (finalid_size == nameid_size && 0 == memcmp(finalid, nameid, nameid_size))
			return true;
	}
	return false;
}


void SessionEnum::removeFromMyConferences(Announcement *ra) {
	
	if (ra->getIsPrivate()) {
		int loc = -1;
		for (int i = 0; i < myPrivateConferences.size(); i++) {
			Announcement *a = myPrivateConferences.at(i);
			if (a == NULL)
				exit(1);
			if (a->getConfName() == ra->getConfName() && a->getOrganizer() == ra->getOrganizer()) {
				loc = i;
				sendDismissSignal(a);
				delete a;
				break;
			}
		}
		if (loc < 0 )
			exit(1);
		myPrivateConferences.removeAt(loc);

	}
	else {
		int loc = -1;
		for (int i = 0; i < myConferences.size(); i++) {
			Announcement *a = myConferences.at(i);
			if (a == NULL)
				exit(1);
			if (a->getConfName() == ra->getConfName() && a->getOrganizer() == ra->getOrganizer()) {
				loc = i;
				sendDismissSignal(a);
				delete a;
				break;
			}
		}

		if (loc < 0 )
			exit(1);

		myConferences.removeAt(loc);
	}
}


void SessionEnum::handleDismissEvent(struct ccn_upcall_info *info) {

	char *dUuid = NULL;
	char *dConfName = NULL;
	char *dOrganizer = NULL;
	const unsigned char *ccnb = info->interest_ccnb;
	const struct ccn_indexbuf *comps = info->interest_comps;
	dUuid = ccn_name_comp_to_str(ccnb, comps, 4);
	dConfName = ccn_name_comp_to_str(ccnb, comps, 5);
	dOrganizer = ccn_name_comp_to_str(ccnb, comps, 6);
	for (int i = 0; i < pubConferences.size(); i++) {
		FetchedAnnouncement *fa = pubConferences.at(i);
		if (fa == NULL)
			critical("NULL encountered unexpectedly");

		if (fa->getUuid() == dUuid && fa->getConfName() == dConfName && fa->getOrganizer() == dOrganizer) {
			// mark it as dismissed, but leave it to be cleaned by checkAlive,
			// so that this conference is not displayed to the user, but still kept by SessionEnum
			// until it is timed out.
			// this is to avoid fetch the cached information of this conference in the intermediate routers as soon
			// as we remove it from the publist
			// when it times out here, the cached information should also have timed out.
			fa->setDismissed(true);
			emit expired(dConfName, dOrganizer);
			break;
		}
	}
	for (int i = 0; i < priConferences.size(); i++) {
		FetchedAnnouncement *fa = priConferences.at(i);
		if (fa == NULL)
			critical("NULL encountered unexpectedly");

		if (fa->getUuid() == dUuid && fa->getConfName() == dConfName && fa->getOrganizer() == dOrganizer) {
			// mark it as dismissed, but leave it to be cleaned by checkAlive,
			// so that this conference is not displayed to the user, but still kept by SessionEnum
			// until it is timed out.
			// this is to avoid fetch the cached information of this conference in the intermediate routers as soon
			// as we remove it from the publist
			// when it times out here, the cached information should also have timed out.
			fa->setDismissed(true);
			emit expired(dConfName, dOrganizer);
			break;
		}
	}

}

void SessionEnum::encodeAnnouncement(struct ccn_charbuf *name, char *buffer, size_t total_len, struct ccn_upcall_info *info) {

	struct ccn_charbuf *signed_info = NULL;
	struct ccn_charbuf *finalblockid = NULL;
	struct ccn_charbuf *content = NULL;
	struct ccn_charbuf *seq = NULL;
	struct ccn_charbuf *name_with_seq = NULL;
	char *buffer_ptr = buffer;

	// 1024 per segment 
	int block_size = 1024;
	int len;
	for (int i = 0; i < total_len; i += block_size) {
		if (total_len - i < block_size) {
			len = total_len - i;
			seq = ccn_charbuf_create();
			ccn_charbuf_putf(seq, "%d", i/block_size);
			finalblockid = ccn_charbuf_create();
			ccn_charbuf_append_tt(finalblockid, seq->length, CCN_BLOB);
			ccn_charbuf_append(finalblockid, seq->buf, seq->length);
			ccn_charbuf_destroy(&seq);
		}
		else {
			len = block_size;
		}
		
		signed_info = ccn_charbuf_create();
		// create signed info
		int res = ccn_signed_info_create(signed_info,
										ccn_keystore_public_key_digest(keystore),
										ccn_keystore_public_key_digest_length(keystore),
										NULL,
										CCN_CONTENT_DATA,
										FRESHNESS, 
										finalblockid,
										keylocator);
		if (res < 0) {
			critical("Failed to create signed_info");
		}
		name_with_seq = ccn_charbuf_create();
		ccn_charbuf_append(name_with_seq, name->buf, name->length);
		seq = ccn_charbuf_create();
		ccn_charbuf_putf(seq, "%d", i/block_size);
		ccn_name_append(name_with_seq, seq->buf, seq->length);
		content = ccn_charbuf_create();
		res = ccn_encode_ContentObject(content,
										name_with_seq,
										signed_info,
										buffer_ptr,
										len,
										NULL,
										ccn_keystore_private_key(keystore));

		if (res)
			critical("failed to create content");

		buffer_ptr += len;

		// already have the lock
		ccn_put(info->h, content->buf, content->length);
		ccn_charbuf_destroy(&name_with_seq);
		ccn_charbuf_destroy(&content);
		ccn_charbuf_destroy(&signed_info);
		ccn_charbuf_destroy(&seq);
	}
	ccn_charbuf_destroy(&finalblockid);
}

void SessionEnum::handleEnumInterest(struct ccn_upcall_info *info) {
	QString val = "conference-list";
	if (ccn_name_comp_strcmp(info->interest_ccnb, info->interest_comps, 3 , val.toStdString().c_str()) != 0)
	{
		debug("Public Listing: trash interest received");
		return;
	}

	// /ndn/broadcast/conference/conference-list
	
	for (int i = 0; i < myConferences.size(); i++) {
		Announcement *a = myConferences.at(i);
		struct ccn_charbuf *name = NULL;
		struct ccn_charbuf *content = NULL;

		name = ccn_charbuf_create();
		ccn_name_init(name);
		int nameEnd = info->interest_comps->n - 1;
		ccn_name_append_components(name, info->interest_ccnb,
								info->interest_comps->buf[0], info->interest_comps->buf[nameEnd]);
		QString confName = a->getConfName();
		ccn_name_append_str(name, confName.toStdString().c_str());

		QString qsData;
		qsData << a;
		QByteArray qba = qsData.toLocal8Bit();
		char *buffer = static_cast<char *>(calloc(qba.size() + 1, sizeof(char)));
		memcpy(buffer, qba.constData(), qba.size());
		buffer[qba.size()] = '\0';
		encodeAnnouncement(name, buffer, strlen(buffer),  info);
		ccn_charbuf_destroy(&name);
		name = NULL;
		if (buffer != NULL) {
			free((void *)buffer);
			buffer = NULL;
		}
	}
}

void SessionEnum::handleEnumPrivateInterest(struct ccn_upcall_info *info) {
	QString val = "private-list";
	if (ccn_name_comp_strcmp(info->interest_ccnb, info->interest_comps, 3, val.toStdString().c_str()) != 0){
		debug("Private Listing: trash interest received");
		return;
	}


	// /ndn/broadcast/conference/private-list
	for (int i = 0; i < myPrivateConferences.size(); i ++) {
		Announcement *a = myPrivateConferences.at(i);
		QString opaqueName = a->getOpaqueName();

		QString out;
		if (a->getXmlOut().isEmpty()) {
			out.append("<bundle>");

			// <Enc-data></Enc-data><Enc-data></Enc-data>...<SK></SK>..<Desc></Desc>

			QStringList certs = a->getCerts();
			int user_num = certs.size();
			if (user_num == 0)
				continue;
			for (int i = 0; i < user_num; i++) {
				QString path = certs.at(i);
				FILE *fp = fopen(path.toStdString().c_str(), "r");
				if (!fp) {
					//critical("Can not open cert " + path);
					debug("no certs of participants specified");
					return;
				}
				X509 *cert= PEM_read_X509(fp, NULL, NULL, NULL);
				fclose(fp);
				EVP_PKEY *public_key = X509_get_pubkey(cert);
				char *to_enc = (char *)malloc(sizeof(a->conferenceKey) + 5);
				to_enc[0] = 'A'; to_enc[1] = 'T'; to_enc[2] = 'H'; to_enc[3] = 'U'; to_enc[4] ='\0';
				memcpy(to_enc + 5, a->conferenceKey, sizeof(a->conferenceKey));
				char *enc_data = NULL;
				size_t enc_len = 0;

				int res = pubKeyEncrypt(public_key, (const unsigned char *)to_enc, (size_t)(sizeof(a->conferenceKey) + 5), 
										  (unsigned char **)&enc_data, &enc_len);
				if (res != 0) 
					critical("public key encryption failed!");
				QByteArray qba((char *)enc_data, (int)enc_len);	
				QString base64(qba.toBase64());
				out.append("<Enc-Data>");
				out.append(base64);
				out.append("</Enc-Data>");
				if (cert != NULL)
					free(cert);
				if (public_key != NULL)
					free(public_key);
				if (to_enc != NULL)
					free(to_enc);
				if (enc_data != NULL)
					free(enc_data);
					
			}


			char *enc_session_key = NULL;
			size_t enc_key_len = 0;
			char iv[sizeof(a->audioSessionKey)];
			RAND_bytes((unsigned char*)iv, sizeof(iv));

			int res = symEncrypt(a->conferenceKey, (unsigned char *)iv, (const unsigned char *)a->audioSessionKey, sizeof(a->audioSessionKey), (unsigned char **)&enc_session_key, &enc_key_len, (size_t)AES_BLOCK_SIZE);
			if (res != 0) 
				critical("sym encryption by conference key failed");

			QByteArray qbaSK((char *)enc_session_key, (int)enc_key_len);	
			QString base64SK(qbaSK.toBase64());
			QByteArray qbaIV((char *)iv, (int) sizeof(iv));
			QString base64IV(qbaIV.toBase64());
			out.append("<Enc-SK>");
			out.append("<iv>");
			out.append(base64IV);
			out.append("</iv>");
			out.append("<SK>");
			out.append(base64SK);
			out.append("</SK>");
			out.append("</Enc-SK>");
			if (enc_session_key != NULL)
				free(enc_session_key);
			
			QString qsData;
			qsData << a;
			QByteArray qba = qsData.toLocal8Bit();
			char *buffer = (char *)malloc(qba.size() + 1);
			memcpy(buffer, qba.constData(), qba.size());
			buffer[qba.size()] = '\0';
			char *enc_desc = NULL;
			size_t enc_desc_len = 0;
			size_t buf_len = qba.size() + 1;
			res = symEncrypt(a->conferenceKey, NULL, (const unsigned char *)buffer, buf_len, (unsigned char **) &enc_desc, &enc_desc_len, (size_t)AES_BLOCK_SIZE);
			if (res != 0)
				critical("conf desc encryption by conference key failed");
			QByteArray qbaDesc((char *)enc_desc, (int)enc_desc_len);
			QString base64Desc(qbaDesc.toBase64());
			out.append("<Enc-Desc>");
			out.append(base64Desc);
			out.append("</Enc-Desc>");
			if (enc_desc != NULL)
				free(enc_desc);

			out.append("</bundle>");
			a->setXmlOut(out);
		}
		else 
			out = a->getXmlOut();

		QByteArray qbaOut = out.toLocal8Bit();


		struct ccn_charbuf *name = NULL;
		name = ccn_charbuf_create();
		ccn_name_init(name);
		int nameEnd = info->interest_comps->n - 1;
		ccn_name_append_components(name, info->interest_ccnb,
								info->interest_comps->buf[0], info->interest_comps->buf[nameEnd]);
		ccn_name_append_str(name, opaqueName.toStdString().c_str());

		char *secret = (char *)malloc(qbaOut.size());
		memcpy(secret, qbaOut.data(), qbaOut.size());
		encodeAnnouncement(name, secret, qbaOut.size(), info);
		ccn_charbuf_destroy(&name);
		if (secret != NULL)
			free(secret);
	}
}



void SessionEnum::decodeAnnouncement(struct ccn_upcall_info *info, bool privateConf)  {
	const unsigned char *value = NULL;
	size_t len = 0;
	int res =ccn_content_get_value(info->content_ccnb, info->pco->offset[CCN_PCO_E],
						info->pco, &value, &len);
	if (res < 0)
		critical("failed to parse content object");

	if (isFinalBlock(info)) {
		unsigned char *content = (unsigned char *)calloc(1, len);
		memcpy(content, value, len);
		if (privateConf) {
			handleEnumPrivateContent(content, len, info);
		}
		else {
			handleEnumContent(content, len);
		}

		return;
	}

	struct ccn_closure *fetch_remaining_closure = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	struct fetched_data *data = (struct fetched_data *)calloc(1, sizeof(struct fetched_data));
	data->value = (unsigned char *)calloc(1, len);
	memcpy(data->value, value, len);
	data->len = len;
	data->seq = 1;
	data->privateConf = privateConf;
	fetch_remaining_closure->data = data;
	fetch_remaining_closure->p = &fetched_block;
	fetch_remaining_closure->intdata = 0;
	const unsigned char *cb = info->content_ccnb;
	struct ccn_indexbuf *cc = info->content_comps;
	struct ccn_charbuf *name = ccn_charbuf_create();
	ccn_name_init(name);
	ccn_name_append_components(name, cb, cc->buf[0], cc->buf[cc->n - 2]);
	struct ccn_charbuf *seq = ccn_charbuf_create();
	ccn_charbuf_putf(seq, "%d", data->seq);
	ccn_name_append(name, seq->buf, seq->length);
	// already have lock
	res = ccn_express_interest(info->h, name, fetch_remaining_closure, NULL);
	if (res < 0)
		abort();
	ccn_charbuf_destroy(&name);
	ccn_charbuf_destroy(&seq);

	// add to a hash that contains unfinished fetches (to include in exclude filter to avoid fetching the first part again)
	// /ndn/broadcast/conference/conference-list/conf-name
	//  0		 1         2          3            4
    QString cname = ccn_name_comp_to_str(cb, cc, 4);
	if (cname == "")
		debug("Content with short name /ndn/broadcast/conference/conference-list received! Panic!");
	
	unfinishedFetches.insert(cname, true);
	// TODO: should be removed after a period of time even if the thing is never fully fetched
}

void SessionEnum::handleEnumContent(const unsigned char *value, size_t len) {
		
		unsigned char hash[SHA_DIGEST_LENGTH];	
		SHA1(value, len, hash);
		/////
		QByteArray x((char *)hash, (int) SHA_DIGEST_LENGTH);
		QString y(x.toBase64());

		if (isConferenceRefresh(hash, true))
			return;

		QByteArray buffer((const char *)value);
		QDomDocument doc;
		if (!doc.setContent(buffer)) {
			debug("failed to convert content to xml");
			return;
		}
		
		Announcement *a = new Announcement();

		doc >> a;
		a->setIsPrivate(false);
		a->setDigest(hash);

		if (a->getConfName().isEmpty()) {
			debug("Fetched Conference Name is empty");
			return;	
		}

		addToConferences(a, true);
		if (value != NULL)
			free((void *)value);

}


void SessionEnum::handleEnumPrivateContent(const unsigned char *value, size_t len, struct ccn_upcall_info *info) {

		unsigned char hash[SHA_DIGEST_LENGTH];	
		SHA1(value, len, hash);

		if (isConferenceRefresh(hash, false)) 
			return;

		QByteArray bundle((char *)value, (int)len);
		QDomDocument doc;
		if (!doc.setContent(bundle)) {
			debug("failed to convert content to xml");
			return;
		}
		bool eligible = false;
		Announcement *a = NULL;
		QDomElement docElem = doc.documentElement(); // <bundle>
		QDomNode node = docElem.firstChild(); // <Enc-Data>
		while (!node.isNull()) {

			QString attr = node.nodeName();
			if (attr == "Enc-Data") {
				debug("decrypting enc-data");
				char *dout = NULL;
				size_t dout_len = 0;
				struct ccn_pkey *priKey = (struct ccn_pkey *)ccn_keystore_private_key(actd_keystore);
				QString encData = node.toElement().text();
				QByteArray qbaEncData = QByteArray::fromBase64(encData.toLocal8Bit());
				char *enc_data = qbaEncData.data();
				int res = priKeyDecrypt((EVP_PKEY*) priKey, (unsigned char *)enc_data, qbaEncData.size(), 
							   (unsigned char **)&dout, &dout_len);
				if (res != 0) {
					debug("decrypt failed\n");
					node = node.nextSibling();
					continue;
				}

				char jargon[5];
				memcpy(jargon, dout, 5);
				if (strcmp(jargon, "ATHU") == 0) {
					eligible = true;
					a = new Announcement();
					memcpy(a->conferenceKey, dout + 5, dout_len - 5);
					if (dout) {
						free(dout);
						dout = NULL;
					}
				}
				if (dout) {
					free(dout);
					dout = NULL;
				}
			} else
			if (attr == "Enc-SK") { //<Enc-SK>
				if (!eligible)
					return;
				
				QDomNode skNode = node.toElement().firstChild();
				if (skNode.isNull() || skNode.nodeName() != "iv")
					return;

				QString skIV = skNode.toElement().text();
				QByteArray qbaIV = QByteArray::fromBase64(skIV.toLocal8Bit());

				skNode = skNode.nextSibling();
				if (skNode.isNull() || skNode.nodeName() != "SK")
					return;

				QString encSK = skNode.toElement().text();
				QByteArray qbaEncSK = QByteArray::fromBase64(encSK.toLocal8Bit());
				char *enc_sk = qbaEncSK.data();

				char *session_key = NULL;
				size_t session_key_len = 0;
				int res = symDecrypt(a->conferenceKey, (unsigned char *)qbaIV.data(), (unsigned char *)enc_sk, qbaEncSK.size(), (unsigned char **)&session_key,
						   &session_key_len, AES_BLOCK_SIZE);
				if (res != 0)  {
					debug("can not decrypt sessionkey");
					return;
				}

				memcpy(a->audioSessionKey, session_key, session_key_len);
				if (session_key) {
					free(session_key);
					session_key = NULL;
				}
			} else 
			if (attr == "Enc-Desc") {
				if (!eligible)
					return;
				char *desc = NULL;
				size_t desc_len = 0;
				QString encDesc = node.toElement().text();
				QByteArray qbaEncDesc = QByteArray::fromBase64(encDesc.toLocal8Bit());
				char *enc_desc = qbaEncDesc.data();
				int res = symDecrypt(a->conferenceKey, NULL, (unsigned char *)enc_desc, qbaEncDesc.size(), (unsigned char **)&desc, &desc_len, AES_BLOCK_SIZE);
				if (res != 0)  {
					debug("can not decrypt desc");
					return;
				}

				QByteArray buffer((const char *)desc);

				QDomDocument descDoc;
				if (!descDoc.setContent(buffer)) {
					debug("failed to convert content to xml");
					return;
				}
				
				descDoc >> a;
				if (desc) {
					free (desc);
					desc = NULL;
				}
			}
			else {
				debug("Unknown xml attribute");
				return;
			}
			node = node.nextSibling();
		}
		// /ndn/broadcast/conference/private-list/opaque-name	
		char *opaqueName = NULL;
		opaqueName = ccn_name_comp_to_str(info->content_ccnb, info->content_comps, 4);
		if (opaqueName == NULL) {
			debug("can not get opaque name!");
			return;
		}

		a->setOpaqueName(opaqueName);

		a->setIsPrivate(true);
		a->setDigest(hash);

		if (a->getConfName().isEmpty()) {
			debug("Fetched Conference Name is empty");
		}


		addToConferences(a, false);
		if (value != NULL)
			free((void *)value);

		debug("handle private content done");
}

void SessionEnum::addToMyConferences(Announcement *a) {
	a->setUuid(uuid);
	if (a->getIsPrivate()) {
		myPrivateConferences.append(a);
	}
	else {
		myConferences.append(a);
	}
}


bool SessionEnum::isConferenceRefresh(unsigned char *hash, bool pub) {
	if (pub) {
		for (int i = 0; i < pubConferences.size(); i++) {
			FetchedAnnouncement *fa = pubConferences.at(i);
			if (fa->equalDigest(hash)) {
				fa->refreshReceived();
				// already have lock
				enumeratePubConf();
				return true;
			}
		}
	} else {
		for (int i = 0; i < priConferences.size(); i++) {
			FetchedAnnouncement *fa = priConferences.at(i);
			if (fa->equalDigest(hash)) {
				fa->refreshReceived();
				// already have lock
				enumeratePriConf();
				return true;
			}
		}
	}
	return false;
}


void SessionEnum::addToConferences(Announcement *a, bool pub) {
	if (a->getUuid() == uuid)
		return;
	FetchedAnnouncement *fa = new FetchedAnnouncement();
	fa->copy(a);
	if (pub) {
		pubConferences.append(fa);
		emit add(fa);
		// already have lock
		enumeratePubConf();
	}
	else {
		priConferences.append(fa);
		if (listPrivate) {
			emit add(fa);
			// already have lock
			enumeratePriConf();
		}
	}
}


void SessionEnum::ccnConnect() {
    ccn = NULL;

    ccn = ccn_create();
    if (ccn == NULL || ccn_connect(ccn, NULL) == -1) {
		QString qs = ("Failed to connect to ccnd. Act will quit.");
		critical(qs);
    }

	// public conf
    struct ccn_charbuf *enum_interest = ccn_charbuf_create();
    if (enum_interest == NULL) {
        QString qs =("Failed to allocate or initialize interest filter path");
		critical(qs);
    }
	ccn_name_from_uri(enum_interest, (const char *) BROADCAST_PREFIX);
	ccn_name_append_str(enum_interest, "conference-list");
	to_announce = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	to_announce->p = &incoming_interest;
    ccn_set_interest_filter(ccn, enum_interest, to_announce);
    ccn_charbuf_destroy(&enum_interest);

	// private conf
    struct ccn_charbuf *private_interest = ccn_charbuf_create();
    if (private_interest == NULL) {
        QString qs =("Failed to allocate or initialize interest filter path");
		critical(qs);
    }
	ccn_name_from_uri(private_interest, (const char *) BROADCAST_PREFIX);
	ccn_name_append_str(private_interest, "private-list");
	to_announce_private = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	to_announce_private->p = &incoming_private_interest;
    ccn_set_interest_filter(ccn, private_interest, to_announce_private);
    ccn_charbuf_destroy(&private_interest);

	// dismiss interest
    struct ccn_charbuf *dismiss_interest = ccn_charbuf_create();
    if (dismiss_interest == NULL) {
        QString qs =("Failed to allocate or initialize interest filter path");
		critical(qs);
    }
	ccn_name_from_uri(dismiss_interest, (const char *) BROADCAST_PREFIX);
	ccn_name_append_str(dismiss_interest, "dismiss");
	handle_dismiss = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	handle_dismiss->p = &dismiss_signal;
    ccn_set_interest_filter(ccn, dismiss_interest, handle_dismiss);
    ccn_charbuf_destroy(&dismiss_interest);
}


int SessionEnum::pubKeyEncrypt(EVP_PKEY *public_key,
							   const unsigned char *data, size_t data_length,
							   unsigned char **encrypted_output,
							   size_t *encrypted_output_length) {

    int openssl_result = 0;
    unsigned char *eptr = NULL;

    if ((NULL == data) || (0 == data_length) || (NULL == public_key))
        return EINVAL;

    *encrypted_output_length = ccn_pubkey_size((struct ccn_pkey *)public_key);
	eptr = (unsigned char *)malloc(*encrypted_output_length);

    memset(eptr, 0, *encrypted_output_length);

	RSA *trsa = EVP_PKEY_get1_RSA((EVP_PKEY *)public_key);
	openssl_result = RSA_public_encrypt(data_length, data, eptr, trsa, RSA_PKCS1_PADDING);
	RSA_free(trsa);

    if (openssl_result < 0) {
        if (NULL == *encrypted_output) {
            free(eptr);
        }
        return openssl_result;
    }
    *encrypted_output = eptr;
	*encrypted_output_length = openssl_result;
    return 0;
}

int SessionEnum::priKeyDecrypt(
                               EVP_PKEY *private_key,
                               const unsigned char *ciphertext, size_t ciphertext_length,
                               unsigned char **decrypted_output,
                               size_t *decrypted_output_length) {

    unsigned char *dptr = NULL;
	int openssl_result = 0;

    if ((NULL == ciphertext) || (0 == ciphertext_length) || (NULL == private_key))
        return EINVAL;

    *decrypted_output_length = EVP_PKEY_size((EVP_PKEY *)private_key);
	dptr = (unsigned char *)malloc(*decrypted_output_length);
    memset(dptr, 0, *decrypted_output_length);

	RSA *trsa = EVP_PKEY_get1_RSA(private_key);	
	openssl_result = RSA_private_decrypt(ciphertext_length, ciphertext, dptr, trsa, RSA_PKCS1_PADDING);
	RSA_free(trsa);

    if (openssl_result < 0) {
        if (NULL == *decrypted_output) {
            free(dptr);
        }
        return openssl_result;
    }
    *decrypted_output = dptr;
	*decrypted_output_length = openssl_result;
    return 0;
}

int SessionEnum::symDecrypt(const unsigned char *key,
							const unsigned char *iv,
							const unsigned char *ciphertext, 
							size_t ciphertext_length,
							unsigned char **plaintext, 
							size_t *plaintext_length, 
							size_t plaintext_padding) {

    EVP_CIPHER_CTX ctx;
    unsigned char *pptr = *plaintext;
    const unsigned char *dptr = NULL;
    size_t plaintext_buf_len = ciphertext_length + plaintext_padding;
    size_t decrypt_len = 0;

    if ((NULL == ciphertext) || (NULL == plaintext_length) || (NULL == key) || (NULL == plaintext))
        return EINVAL;

    if (NULL == iv) {
        plaintext_buf_len -= AES_BLOCK_SIZE;
    }

    if ((NULL != *plaintext) && (*plaintext_length < plaintext_buf_len))
        return ENOBUFS;

    if (NULL == pptr) {
        pptr = (unsigned char *)calloc(1, plaintext_buf_len);
        if (NULL == pptr)
            return ENOMEM;
    }

    if (NULL == iv) {
        iv = ciphertext;
        dptr = ciphertext + AES_BLOCK_SIZE;
        ciphertext_length -= AES_BLOCK_SIZE;
    } else {
        dptr = ciphertext;
    }

    /*
      print_block("ccn_decrypt: key:", key, AES_BLOCK_SIZE);
      print_block("ccn_decrypt: iv:", iv, AES_BLOCK_SIZE);
      print_block("ccn_decrypt: ciphertext:", dptr, ciphertext_length);
    */
    if (1 != EVP_DecryptInit(&ctx, EVP_aes_128_cbc(),
                             key, iv)) {
        if (NULL == *plaintext)
            free(pptr);
        return -128;
    }

    if (1 != EVP_DecryptUpdate(&ctx, pptr, (int *)&decrypt_len, dptr, ciphertext_length)) {
        if (NULL == *plaintext)
            free(pptr);
        return -127;
    }
    *plaintext_length = decrypt_len + plaintext_padding;
    if (1 != EVP_DecryptFinal(&ctx, pptr+decrypt_len, (int *)&decrypt_len)) {
        if (NULL == *plaintext)
            free(pptr);
        return -126;
    }
    *plaintext_length += decrypt_len;
    *plaintext = pptr;
    /* this is supposed to happen automatically, but sometimes we seem to be running over the end... */
    memset(*plaintext + *plaintext_length - plaintext_padding, 0, plaintext_padding);
    return 0;
}


int SessionEnum::symEncrypt(const unsigned char *key,
							const unsigned char *iv,
							const unsigned char *plaintext, 
							size_t plaintext_length,
							unsigned char **ciphertext, 
							size_t *ciphertext_length,
							size_t ciphertext_padding) {
    EVP_CIPHER_CTX ctx;
    unsigned char *cptr = *ciphertext;
    unsigned char *eptr = NULL;
    /* maximum length of ciphertext plus user-requested extra */
    size_t ciphertext_buf_len = plaintext_length + AES_BLOCK_SIZE-1 + ciphertext_padding;
    size_t encrypt_len = 0;
    size_t alloc_buf_len = ciphertext_buf_len;
    size_t alloc_iv_len = 0;

    if ((NULL == ciphertext) || (NULL == ciphertext_length) || (NULL == key) || (NULL == plaintext))
        return EINVAL;

    if (NULL == iv) {
        alloc_buf_len += AES_BLOCK_SIZE;
    }

    if ((NULL != *ciphertext) && (*ciphertext_length < alloc_buf_len))
        return ENOBUFS;

    if (NULL == cptr) {
        cptr = (unsigned char *)calloc(1, alloc_buf_len);
        if (NULL == cptr)
            return ENOMEM;
    }
    *ciphertext_length = 0;

    if (NULL == iv) {
        iv = cptr;
        eptr = cptr + AES_BLOCK_SIZE; /* put iv at start of block */

        if (1 != RAND_bytes((unsigned char *)iv, AES_BLOCK_SIZE)) {
            if (NULL == *ciphertext)
                free(cptr);
            return -1;
        }

        alloc_iv_len = AES_BLOCK_SIZE;
        fprintf(stderr, "ccn_encrypt: Generated IV\n");
    } else {
        eptr = cptr;
    }

    if (1 != EVP_EncryptInit(&ctx, EVP_aes_128_cbc(),
                             key, iv)) {
        if (NULL == *ciphertext)
            free(cptr);
        return -128;
    }

    if (1 != EVP_EncryptUpdate(&ctx, eptr, (int *)&encrypt_len, plaintext, plaintext_length)) {
        if (NULL == *ciphertext)
            free(cptr);
        return -127;
    }
    *ciphertext_length += encrypt_len;

    if (1 != EVP_EncryptFinal(&ctx, eptr+encrypt_len, (int *)&encrypt_len)) {
        if (NULL == *ciphertext)
            free(cptr);
        return -126;
    }

    /* don't include padding length in ciphertext length, caller knows its there. */
    *ciphertext_length += encrypt_len;
    *ciphertext = cptr;							   

    /*
      print_block("ccn_encrypt: key:", key, AES_BLOCK_SIZE);
      print_block("ccn_encrypt: iv:", iv, AES_BLOCK_SIZE);
      print_block("ccn_encrypt: ciphertext:", eptr, *ciphertext_length);
    */
    /* now add in any generated iv */
    *ciphertext_length += alloc_iv_len;
    return 0;
}

void SessionEnum::sendDismissSignal(Announcement *a) {
	struct ccn_charbuf *interest = ccn_charbuf_create();
	if (interest == NULL ) {
		critical("interest construction failed");
	}
	int res = ccn_name_from_uri(interest, BROADCAST_PREFIX);
	if (res < 0)
		critical("Bad ccn URI");
	
	ccn_name_append_str(interest, "dismiss");
	ccn_name_append_str(interest, a->getUuid().toStdString().c_str());
	ccn_name_append_str(interest, a->getConfName().toStdString().c_str());
	ccn_name_append_str(interest, a->getOrganizer().toStdString().c_str());

	
	// fetch_announce handler should never be triggered in this case
	mutex_trylock();
	res = ccn_express_interest(ccn, interest, fetch_announce, NULL);
	mutex_unlock();
	if (res < 0) {
		critical("express dismiss interest failed!");
	}
	debug("dismiss interest sent");
	ccn_charbuf_destroy(&interest);
}

void SessionEnum::enumerate() {
	enumeratePubConf();
	if (listPrivate) {
		enumeratePriConf();
	}
}


void SessionEnum::expressEnumInterest(struct ccn_charbuf *interest, QList<QString> &toExclude, bool privateConf) {

	if (toExclude.size() == 0) {
		mutex_trylock();	
		int res;
		if (privateConf) {
			res = ccn_express_interest(ccn, interest, fetch_private, NULL);
		} else {
			res = ccn_express_interest(ccn, interest, fetch_announce, NULL);
		}
		mutex_unlock();
		if (res < 0) {
			critical("express interest failed!");
		}
		ccn_charbuf_destroy(&interest);
		return;
	}

	struct ccn_charbuf **excl = NULL;
	if (toExclude.size() > 0) {
		excl = new ccn_charbuf *[toExclude.size()];
		for (int i = 0; i < toExclude.size(); i ++) {
			QString compName = toExclude.at(i);
			struct ccn_charbuf *comp = ccn_charbuf_create();
			ccn_name_init(comp);
			ccn_name_append_str(comp, compName.toStdString().c_str());
			excl[i] = comp;
			comp = NULL;
		}
		qsort(excl, toExclude.size(), sizeof(excl[0]), &namecompare);
	}

	int begin = 0;
	bool excludeLow = false;
	bool excludeHigh = true;
	while (begin < toExclude.size()) {
		if (begin != 0) {
			excludeLow = true;
		}
		struct ccn_charbuf *templ = ccn_charbuf_create();
		ccn_charbuf_append_tt(templ, CCN_DTAG_Interest, CCN_DTAG); // <Interest>
		ccn_charbuf_append_tt(templ, CCN_DTAG_Name, CCN_DTAG); // <Name>
		ccn_charbuf_append_closer(templ); // </Name> 
		ccn_charbuf_append_tt(templ, CCN_DTAG_Exclude, CCN_DTAG); // <Exclude>
		if (excludeLow) {
			append_bf_all(templ);
		}
		for (; begin < toExclude.size(); begin++) {
			struct ccn_charbuf *comp = excl[begin];
			if (comp->length < 4) abort();
			// we are being conservative here
			if (interest->length + templ->length + comp->length > 1350) {
				break;
			}
			ccn_charbuf_append(templ, comp->buf + 1, comp->length - 2);
		}
		if (begin == toExclude.size()) {
			excludeHigh = false;
		}
		if (excludeHigh) {
			append_bf_all(templ);
		}
		ccn_charbuf_append_closer(templ); // </Exclude>

		ccn_charbuf_append_closer(templ); // </Interest> 
		mutex_trylock();	
		int res;
		if (privateConf) {
			res = ccn_express_interest(ccn, interest, fetch_private, templ);
		} else {
			res = ccn_express_interest(ccn, interest, fetch_announce, templ);
		}
		mutex_unlock();
		if (res < 0) {
			critical("express interest failed!");
		}
		ccn_charbuf_destroy(&templ);
	}
	ccn_charbuf_destroy(&interest);
	for (int i = 0; i < toExclude.size(); i++) {
		ccn_charbuf_destroy(&excl[i]);
	}
	if (excl != NULL) {
		delete []excl;
	}
}

void SessionEnum::enumeratePubConf() {
	struct ccn_charbuf *interest = ccn_charbuf_create();
	if (interest == NULL ) {
		critical("interest construction failed");
	}
	int res = ccn_name_from_uri(interest, BROADCAST_PREFIX);
	if (res < 0)
		critical("Bad ccn URI");
	
	ccn_name_append_str(interest, "conference-list");

	QList<QString> toExclude;
	for (int i = 0; i < pubConferences.size(); i++) {
		FetchedAnnouncement *fa = pubConferences.at(i);
		if (fa == NULL) 
			critical("SessionEnum::enumrate");

		if (!fa->needRefresh()) {
			toExclude.append(fa->getConfName());
		}
	}

	// always exclude own conferences
	// TODO: do not do this (when staleness comes to play)
	for (int i = 0; i < myConferences.size(); i++) {
		Announcement *a = myConferences.at(i);
		if (a == NULL) 
			critical("SessionEnum::enumrate");
		toExclude.append(a->getConfName());
	}

	// exclude unfinished fetches
	QHash<QString, bool>::const_iterator it = unfinishedFetches.constBegin();
	while(it != unfinishedFetches.constEnd()) {
		toExclude.append(it.key());
		it++;
	}

	expressEnumInterest(interest, toExclude, false);
	//testtest(interest, toExclude);
	
}

void SessionEnum::enumeratePriConf() {
	struct ccn_charbuf *interest = ccn_charbuf_create();
	if (interest == NULL ) {
		critical("interest construction failed");
	}
	int res = ccn_name_from_uri(interest, BROADCAST_PREFIX);
	if (res < 0)
		critical("Bad ccn URI");
	
	ccn_name_append_str(interest, "private-list");
	
	QList<QString> toExclude;
	for (int i = 0; i < priConferences.size(); i++) {
		FetchedAnnouncement *fa = priConferences.at(i);
		if (fa == NULL) 
			critical("SessionEnum::enumrate");

		if (!fa->needRefresh()) {
			toExclude.append(fa->getOpaqueName());
		}
	}

	// always exclude own conferences
	// TODO: do not do this (when staleness comes to play)
	for (int i = 0; i < myPrivateConferences.size(); i++) {
		Announcement *a = myPrivateConferences.at(i);
		if (a == NULL) 
			critical("SessionEnum::enumrate");
		toExclude.append(a->getOpaqueName());
	}

	expressEnumInterest(interest, toExclude, true);
	//testtest(interest, toExclude);

}

void SessionEnum::startThread() {
	if (! isRunning()) {
		bRunning = true;
		pthread_mutexattr_init(&ccn_attr);
		pthread_mutexattr_settype(&ccn_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&actd_mutex, &ccn_attr);
		pfds[0].fd = ccn_get_connection_fd(ccn);
		pfds[0].events = POLLIN; 
		start(QThread::HighestPriority);
	}
}

void SessionEnum::stopThread() {
	if (isRunning()) {
		bRunning = false;
		wait();
	}
}


void SessionEnum::run() {
	int res = 0;
	int ret;
	while (bRunning) {
		res = ccn_run(ccn, 0);
		if (res >= 0) {
			ret = poll(pfds, 1, 100);
			if (ret >= 0) {
				mutex_trylock();
				res = ccn_run(ccn, 0);
				if (res < 0) {
					fprintf(stderr, "Error from ccn_run: %d\n", res);
					abort();
				}
				mutex_unlock();
			}
		}
	}
}

void SessionEnum::checkAlive() {

	debug("++++++SessionEnum::checkAlive()");
	foreach( FetchedAnnouncement *fa, pubConferences) {
		if (fa == NULL)
			critical("conference announcement is null");

		if (fa->isStaled()) {
			if (!fa->isDismissed()) {
				QString confName = fa->getConfName();
				QString organizer = fa->getOrganizer();
				emit expired(confName, organizer);
			}
			delete fa;
			pubConferences.removeOne(fa);
		}
	}

	foreach( FetchedAnnouncement *fa, priConferences) {
		if (fa == NULL)
			critical("conference announcement is NULL");

		if (fa->isStaled()) {
			if (!fa->isDismissed()) {
				QString confName = fa->getConfName();
				QString organizer = fa->getOrganizer();
				emit expired(confName, organizer);
			}
			delete fa;
			priConferences.removeOne(fa);
		}
	}

}

SessionEnum::SessionEnum() {
	gsd = this;
	listPrivate = false;
	QUuid quuid = QUuid::createUuid();
	uuid = quuid.toString();
	fetch_announce = (struct ccn_closure *) (calloc(1, sizeof(struct ccn_closure)));
	fetch_announce->p = &incoming_content;
	fetch_private = (struct ccn_closure *) (calloc(1, sizeof(struct ccn_closure)));
	fetch_private->p = &incoming_private_content;
	ccnConnect();
	initKeystoreAndKeylocator();

	enumTimer = new QTimer(this);
	connect(enumTimer, SIGNAL(timeout()), this, SLOT(enumerate()));
	enumTimer->start(4000);

	aliveTimer = new QTimer(this);
	connect(aliveTimer, SIGNAL(timeout()), this, SLOT(checkAlive()));
	aliveTimer->start(15000);

	startThread();

	enumerate();

}

SessionEnum::~SessionEnum() {
	stopThread();
	if (ccn != NULL) {
		ccn_disconnect(ccn);
		ccn_destroy(&ccn);
	}
	if (keystore != NULL) {
		ccn_keystore_destroy(&keystore);
	}
	if (actd_keystore != NULL) {
		ccn_keystore_destroy(&actd_keystore);
	}
}
