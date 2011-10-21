#ifndef NDNHANDLER_H
#define NDNHANDLER_H
extern "C" {
#endif
#include <ccn/ccn.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#include <ccn/uri.h>
#ifdef __cplusplus
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

	struct ccn *h;
	struct ccn_keystore *keystore;
	struct ccn_charbuf *keylocator;
};
#endif
