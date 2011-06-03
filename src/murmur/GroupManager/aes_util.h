#ifndef AES_UTIL_H
#define AES_UTIL_H
#ifdef __cplusplus
extern "C" {
#endif
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
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
int symDecrypt(const unsigned char *key, const unsigned char *iv, const unsigned char *ciphertext, 
						   size_t ciphertext_length, unsigned char **plaintext, size_t *plaintext_length, 
						   size_t plaintext_padding);

int symEncrypt(const unsigned char *key, const unsigned char *iv, const unsigned char *plaintext, 
						   size_t plaintext_length, unsigned char **ciphertext, size_t *ciphertext_length,
						   size_t padding); 
#endif
