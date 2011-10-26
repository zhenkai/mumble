#include "NdnHandler.h"

NdnHandler::NdnHandler() {
	initKeystore();
	createKeylocator();
	h = ccn_create();
	if (h == NULL || ccn_connect(h, NULL) == -1) {
		fprintf(stderr, "Failed to connect to ccnd.\n");
		abort();
	}
}

NdnHandler::~NdnHandler() {
	if (h != NULL) {
		ccn_disconnect(h);
		ccn_destroy(&h);
	}
	if (keystore != NULL) {
		ccn_keystore_destroy(&keystore);
	}
	if (keylocator != NULL) {
		ccn_charbuf_destroy(&keylocator);
	}
}

void NdnHandler::initKeystore() {
	keystore = ccn_keystore_create();
	struct ccn_charbuf *temp = ccn_charbuf_create();
	ccn_charbuf_putf(temp, "%s/.ccnx/.ccnx_keystore", getenv("HOME"));
	int res = ccn_keystore_init(keystore, ccn_charbuf_as_string(temp), (char *)"Th1s1sn0t8g00dp8ssw0rd.");
	if (res != 0) {
		fprintf(stderr, "Failed to initialized keystore %s\n", ccn_charbuf_as_string(temp));
		abort();
	}
	ccn_charbuf_destroy(&temp);
}

void NdnHandler::createKeylocator() {
	keylocator = ccn_charbuf_create();
	ccn_charbuf_append_tt(keylocator, CCN_DTAG_KeyLocator, CCN_DTAG);
	ccn_charbuf_append_tt(keylocator, CCN_DTAG_Key, CCN_DTAG);
	int res = ccn_append_pubkey_blob(keylocator, ccn_keystore_public_key(keystore));
	if (res < 0) {
		ccn_charbuf_destroy(&keylocator);
	}else {
		ccn_charbuf_append_closer(keylocator);
		ccn_charbuf_append_closer(keylocator);
	}
}

ssize_t NdnHandler::getPublicKeyDigestLength() {
	return ccn_keystore_public_key_digest_length(keystore);
}

const unsigned char *NdnHandler::getPublicKeyDigest() {
	return ccn_keystore_public_key_digest(keystore);
}

const struct ccn_pkey *NdnHandler::getPrivateKey() {
	return ccn_keystore_private_key(keystore);
}

/*
 * Comparison operator for sorting the excl list with qsort.
 * For convenience, the items in the excl array are
 * charbufs containing ccnb-encoded Names of one component each.
 * (This is not the most efficient representation.)
 */
int /* for qsort */
NdnHandler::nameCompare(const void *a, const void *b)
{
    const struct ccn_charbuf *aa = *(const struct ccn_charbuf **)a;
    const struct ccn_charbuf *bb = *(const struct ccn_charbuf **)b;
    int ans = ccn_compare_names(aa->buf, aa->length, bb->buf, bb->length);
    if (ans == 0)
        fprintf(stderr, "wassat? %d\n", __LINE__);
    return (ans);
}

void NdnHandler::excludeAll(struct ccn_charbuf *c)
{
    unsigned char bf_all[9] = { 3, 1, 'A', 0, 0, 0, 0, 0, 0xFF };
    const struct ccn_bloom_wire *b = ccn_bloom_validate_wire(bf_all, sizeof(bf_all));
    if (b == NULL) abort();
    ccn_charbuf_append_tt(c, CCN_DTAG_Bloom, CCN_DTAG);
    ccn_charbuf_append_tt(c, sizeof(bf_all), CCN_BLOB);
    ccn_charbuf_append(c, bf_all, sizeof(bf_all));
    ccn_charbuf_append_closer(c);
}
