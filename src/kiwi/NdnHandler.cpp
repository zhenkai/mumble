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
