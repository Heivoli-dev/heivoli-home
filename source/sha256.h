#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    uint32_t datalen;
} Sha256;

void sha256_init(Sha256 *c);
void sha256_update(Sha256 *c, const uint8_t *data, size_t len);
void sha256_final(Sha256 *c, uint8_t out[32]);

// Calcule le SHA-256 d'un fichier et l'écrit en hexadécimal minuscule (65 octets).
int sha256_file_hex(const char *path, char hex[65]);
