#ifndef NDNHANDLER_H
#define NDNHANDLER_H
extern "C" {
#include <ccn/ccn.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#include <ccn/uri.h>
#include <ccn/bloom.h>
#include <stdio.h>
#include <stdlib.h>
}

class NdnHandler {
public:
	NdnHandler();
	~NdnHandler();
	void createKeylocator();
	void initKeystore();
	const struct ccn_pkey *getPrivateKey();
	const unsigned char *getPublicKeyDigest();
	ssize_t getPublicKeyDigestLength();
public:
	static int nameCompare(const void *a, const void *b);
	static void excludeAll(struct ccn_charbuf *c);

	struct ccn *h;
	struct ccn_keystore *keystore;
	struct ccn_charbuf *keylocator;

};
#endif
