#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>
#include <openssl/sha.h>
#include <openssl/rand.h> // For generating the salt
#include <argon2.h>
#include <stdint.h>
#include "miner_core.h"

// Constants
#define SALT_LEN 16

// Helper function to convert a raw SHA256 hash to a hex string
static void sha256_to_hex(const unsigned char* hash, char* hex_string) {
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hex_string + (i * 2), "%02x", hash[i]);
    }
    hex_string[64] = 0;
}

char* calculate_argon_hash(const char* miner_address, long prev_block_date, int elapsed, long height, thread_stats_t* stats, uint64_t nonce) {
    atomic_fetch_add(&stats->hashes, 1);
    char base[256];
    snprintf(base, sizeof(base), "%ld-%d-%llu", prev_block_date, elapsed, (unsigned long long)nonce);

    unsigned char salt[SALT_LEN];
    uint32_t t_cost, m_cost, parallelism;

    long current_block_date = prev_block_date + elapsed;
    if (current_block_date < 1614556800L) { // Legacy hashing for old blocks (UPDATE_3_ARGON_HARD)
        t_cost = 2;
        m_cost = 2048;
        parallelism = 1;
        // Use the first 16 bytes of the address as the salt
        strncpy((char*)salt, miner_address, SALT_LEN);
    } else { // Modern hashing
        t_cost = ARGON2_T_COST;
        m_cost = ARGON2_M_COST;
        parallelism = ARGON2_PARALLELISM;

        // --- New Salt Generation ---
        // We create a deterministic, unique salt for each hash attempt by hashing
        // a combination of the miner's address, the block height, and a per-thread nonce.
        // This ensures that each thread is working on unique data, mirroring the behavior
        // of PHP's password_hash, which generates a random salt for each call.
        uint64_t salt_nonce = nonce / 1000;
        char salt_base[512];
        snprintf(salt_base, sizeof(salt_base), "%s-%ld-%llu", miner_address, height, (unsigned long long)salt_nonce);

        unsigned char salt_hash[SHA256_DIGEST_LENGTH];
        SHA256((unsigned char*)salt_base, strlen(salt_base), salt_hash);

        // The final salt is the first 16 bytes of the SHA256 hash.
        memcpy(salt, salt_hash, SALT_LEN);
    }

    uint32_t hash_len = 32;
    size_t encoded_len = argon2_encodedlen(
        t_cost,
        m_cost,
        parallelism,
        sizeof(salt),
        hash_len,
        Argon2_i
    );

    char *encoded_hash = (char*)malloc(encoded_len);
    if (!encoded_hash) {
        perror("Failed to allocate memory for Argon2 hash");
        return NULL;
    }

    int result = argon2i_hash_encoded(
        t_cost,
        m_cost,
        parallelism,
        base, strlen(base),
        salt, sizeof(salt),
        hash_len,
        encoded_hash,
        encoded_len
    );

    if (result != ARGON2_OK) {
        fprintf(stderr, "Error creating Argon2 hash: %s\n", argon2_error_message(result));
        free(encoded_hash);
        return NULL;
    }

    return encoded_hash;
}

char* calculate_nonce(const char* miner_address, long prev_block_date, int elapsed, const char* argon_hash) {
    char nonce_base[512];
    snprintf(nonce_base, sizeof(nonce_base), "%s%s-%ld-%d-%s",
        CHAIN_ID, miner_address, prev_block_date, elapsed, argon_hash);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)nonce_base, strlen(nonce_base), hash);

    char *hex_hash = (char*)malloc(65);
    if (!hex_hash) {
        perror("Failed to allocate memory for nonce");
        return NULL;
    }
    sha256_to_hex(hash, hex_hash);

    return hex_hash;
}

void calculate_hit(mpz_t result, const char* miner_address, const char* nonce, long height, const mpz_t difficulty) {
    char base[512];
    char *difficulty_str = mpz_get_str(NULL, 10, difficulty);
    snprintf(base, sizeof(base), "%s-%s-%ld-%s",
        miner_address, nonce, height, difficulty_str);
    free(difficulty_str);


    // Double SHA256
    unsigned char hash1[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)base, strlen(base), hash1);
    unsigned char hash2[SHA256_DIGEST_LENGTH];
    SHA256(hash1, sizeof(hash1), hash2);

    // Take the first 4 bytes (32 bits) of the final hash, like php-4
    char hash_part_hex[9];
    for(int i = 0; i < 4; i++) {
        sprintf(hash_part_hex + i * 2, "%02x", hash2[i]);
    }
    hash_part_hex[8] = 0;

    mpz_t value;
    mpz_init_set_str(value, hash_part_hex, 16);

    if (mpz_cmp_ui(value, 0) == 0) {
        mpz_set_ui(value, 1); // Avoid division by zero
    }

    // result = ("ffffffff" * BLOCK_TARGET_MUL) / value
    mpz_t ffffffff;
    mpz_init_set_str(ffffffff, "ffffffff", 16);

    mpz_t numerator;
    mpz_init(numerator);
    mpz_mul_ui(numerator, ffffffff, BLOCK_TARGET_MUL);

    mpz_fdiv_q(result, numerator, value);

    // Clean up
    mpz_clear(value);
    mpz_clear(ffffffff);
    mpz_clear(numerator);
}

void calculate_target(mpz_t result, int elapsed, const mpz_t difficulty) {
    if (elapsed <= 0) {
        mpz_set_ui(result, 0);
        return;
    }

    // result = (difficulty * BLOCK_TIME) / elapsed
    mpz_t numerator;
    mpz_init(numerator);
    mpz_mul_ui(numerator, difficulty, BLOCK_TIME);

    mpz_fdiv_q_ui(result, numerator, elapsed);

    mpz_clear(numerator);
}
