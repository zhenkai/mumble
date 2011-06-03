#include "aes_util.h"

int symDecrypt(const unsigned char *key,
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


int symEncrypt(const unsigned char *key,
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
