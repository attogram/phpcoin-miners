#ifndef MINER_CORE_H
#define MINER_CORE_H

#include <gmp.h>
#include <pthread.h>

// To hold a found block solution
typedef struct {
    char* argon;
    char* nonce;
    long height;
    mpz_t difficulty;
    long date;
    mpz_t hit;
    mpz_t target;
    int elapsed;
} solution_t;

// Constants from the PHPCoin source
#define BLOCK_TIME 60
#define BLOCK_TARGET_MUL 1000
#define CHAIN_ID "00"

// Argon2 parameters (for latest hashingOptions)
#define ARGON2_T_COST 2
#define ARGON2_M_COST 32768 // 32 MiB
#define ARGON2_PARALLELISM 1

/**
 * @brief Calculates the Argon2 hash for a given time delta.
 *
 * This function replicates the behavior of PHPCoin's `calculateArgonHash`.
 *
 * @param prev_block_date The timestamp of the previous block.
 * @param elapsed The number of seconds elapsed since the previous block.
 * @return A dynamically allocated string with the encoded Argon2 hash. The caller must free this string.
 */
#include <stdatomic.h>
#include <sys/types.h>

typedef struct {
    int id;
    pid_t pid;
    long height;
    int elapsed;
    double speed;
    mpz_t hit;
    mpz_t best_hit;
    mpz_t target;
    atomic_long hashes;
    pthread_mutex_t stat_mutex;
} thread_stats_t;

char* calculate_argon_hash(const char* miner_address, long prev_block_date, int elapsed, long height, thread_stats_t* stats);

/**
 * @brief Calculates the nonce for a block attempt.
 *
 * This function replicates PHPCoin's `calculateNonce` using SHA256.
 *
 * @param miner_address The miner's public address.
 * @param prev_block_date The timestamp of the previous block.
 * @param elapsed The seconds elapsed since the previous block.
 * @param argon_hash The Argon2 hash for this attempt.
 * @return A dynamically allocated 65-byte string (64 hex chars + null terminator) for the nonce. The caller must free this string.
 */
char* calculate_nonce(const char* miner_address, long prev_block_date, int elapsed, const char* argon_hash);

/**
 * @brief Calculates the "hit" value for a mining attempt.
 *
 * Replicates PHPCoin's `calculateHit` using double SHA256 and GMP arithmetic.
 *
 * @param result An initialized mpz_t variable to store the resulting hit value.
 * @param miner_address The miner's public address.
 * @param nonce The nonce for this attempt.
 * @param height The current block height.
 * @param difficulty The current block difficulty.
 */
void calculate_hit(mpz_t result, const char* miner_address, const char* nonce, long height, const mpz_t difficulty);

/**
 * @brief Calculates the "target" value for a mining attempt.
 *
 * Replicates PHPCoin's `calculateTarget` using GMP arithmetic.
 *
 * @param result An initialized mpz_t variable to store the resulting target value.
 * @param elapsed The seconds elapsed since the previous block.
 * @param difficulty The current block difficulty.
 */
void calculate_target(mpz_t result, int elapsed, const mpz_t difficulty);


#endif // MINER_CORE_H
