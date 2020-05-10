#if defined(_OPENSSL_)
#include "openssl/aes.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#pragma comment(lib, "../../3rd/openssl/libssl.lib")
#pragma comment(lib, "../../3rd/openssl/libcrypto.lib")

void ts_encrypt_test(const char* keyfile, const char* file, const char* outfile)
{
    uint8_t key[AES_BLOCK_SIZE]; // AES-128
    FILE* keyfp = fopen(keyfile, "rb");
    assert(sizeof(key) == fread(key, 1, sizeof(key), keyfp));
    fclose(keyfp);

    AES_KEY aes;
    assert(0 == AES_set_encrypt_key(key, 128, &aes));

    uint8_t iv[AES_BLOCK_SIZE] = { 0 };
    uint8_t in[AES_BLOCK_SIZE];
    uint8_t out[AES_BLOCK_SIZE];
    FILE* infp = fopen(file, "rb");
    FILE* outfp = fopen(outfile, "wb");
    while (1 == fread(in, sizeof(in), 1, infp))
    {
        AES_cbc_encrypt(in, out, sizeof(out), &aes, iv, AES_ENCRYPT);
        assert(1 == fwrite(out, sizeof(out), 1, outfp));
    }
    fclose(infp);
    fclose(outfp);
}

void ts_decrypt_test(const char* keyfile, const char* file, const char* outfile)
{
    uint8_t key[AES_BLOCK_SIZE]; // AES-128
    FILE* keyfp = fopen(keyfile, "rb");
    assert(sizeof(key) == fread(key, 1, sizeof(key), keyfp));
    fclose(keyfp);

    AES_KEY aes;
    assert(0 == AES_set_decrypt_key(key, 128, &aes));

    uint8_t iv[AES_BLOCK_SIZE] = { 0 };
    uint8_t in[AES_BLOCK_SIZE];
    uint8_t out[AES_BLOCK_SIZE];
    FILE* infp = fopen(file, "rb");
    FILE* outfp = fopen(outfile, "wb");
    while (1 == fread(in, sizeof(in), 1, infp))
    {
        AES_cbc_encrypt(in, out, sizeof(out), &aes, iv, AES_DECRYPT);
        assert(1 == fwrite(out, sizeof(out), 1, outfp));
    }
    fclose(infp);
    fclose(outfp);
}
#endif
