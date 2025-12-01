#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <curl/curl.h>
#include <gmp.h>
#include <stdatomic.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <getopt.h>
#include <ctype.h>
#include "miner_core.h"

// --- Global State ---
atomic_bool block_found = ATOMIC_VAR_INIT(false);
atomic_long total_hashes = ATOMIC_VAR_INIT(0);
atomic_int total_submits = ATOMIC_VAR_INIT(0);
atomic_int total_accepted = ATOMIC_VAR_INIT(0);
atomic_int total_rejected = ATOMIC_VAR_INIT(0);
atomic_int total_dropped = ATOMIC_VAR_INIT(0);
pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;
solution_t* found_solution = NULL; // Will hold the solution
pthread_mutex_t solution_mutex = PTHREAD_MUTEX_INITIALIZER;


// --- Data Structures ---

// To hold the JSON response from the node
struct memory {
    char *response;
    size_t size;
};


thread_stats_t* mining_stats = NULL;

// To pass data to each mining thread
typedef struct {
    int thread_id;
    char* address;
    char* node;
    long height;
    long block_date;
    mpz_t difficulty;
    int cpu_usage;
    thread_stats_t* stats;
} thread_data_t;



// --- Config Parsing ---

// Helper to trim whitespace from a string in-place
void trim(char *str) {
    char *start, *end;
    if (str == NULL || *str == '\0') return;

    // Trim leading space
    for (start = str; *start != '\0' && isspace((unsigned char)*start); start++);

    // Trim trailing space
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = '\0';

    // Move trimmed string to the beginning of the buffer
    if (str != start) memmove(str, start, strlen(start) + 1);
}

// Parses miner.conf and sets the config variables
void parse_config(const char* filename, char** node, char** address, int* num_threads, int* cpu_usage, int* report_interval) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return; // File not found, do nothing
    }

    char line[256];
    char* key;
    char* value;
    char* separator;

    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        separator = strchr(line, '=');
        if (!separator) continue;

        *separator = '\0'; // Split key and value
        key = line;
        value = separator + 1;

        trim(key);
        trim(value);

        if (*value == '\0') continue; // Skip empty values

        if (strcmp(key, "node") == 0) {
            *node = strdup(value);
        } else if (strcmp(key, "address") == 0) {
            *address = strdup(value);
        } else if (strcmp(key, "threads") == 0) {
            *num_threads = atoi(value);
        } else if (strcmp(key, "cpu") == 0) {
            *cpu_usage = atoi(value);
        } else if (strcmp(key, "report-interval") == 0) {
            *report_interval = atoi(value);
        }
    }
    fclose(file);
}


// --- Networking (libcurl) ---

// Callback function to write curl response to our memory struct
static size_t write_callback(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)userp;

    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if (ptr == NULL) return 0; // out of memory

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

// Super basic JSON parser to extract values. Not robust, but avoids a library dependency.
char* json_extract(const char* json, const char* key) {
    char* key_ptr = strstr(json, key);
    if (!key_ptr) return NULL;

    char* colon_ptr = strchr(key_ptr, ':');
    if (!colon_ptr) return NULL;

    char* start_ptr = colon_ptr + 1;
    while (*start_ptr == ' ' || *start_ptr == '"') start_ptr++;

    char* end_ptr = start_ptr;
    while (*end_ptr != '"' && *end_ptr != ',' && *end_ptr != '}') end_ptr++;

    int len = end_ptr - start_ptr;
    char* value = malloc(len + 1);
    strncpy(value, start_ptr, len);
    value[len] = '\0';
    return value;
}


int get_mining_info(const char* node, long* height, mpz_t difficulty, long* date) {
    CURL *curl;
    CURLcode res;
    struct memory chunk = {0};
    char url[256];
    snprintf(url, sizeof(url), "%s/mine.php?q=info", node);

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 second timeout

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(chunk.response);
            return 0;
        }

        char *height_str = json_extract(chunk.response, "\"height\"");
        char *difficulty_str = json_extract(chunk.response, "\"difficulty\"");
        char *date_str = json_extract(chunk.response, "\"date\"");

        if (!height_str || !difficulty_str || !date_str) {
             fprintf(stderr, "Error: Could not parse mining info from node.\n");
             curl_easy_cleanup(curl);
             free(chunk.response);
             return 0;
        }

        *height = atol(height_str) + 1;
        mpz_set_str(difficulty, difficulty_str, 10);
        *date = atol(date_str);

        free(height_str);
        free(difficulty_str);
        free(date_str);
        free(chunk.response);
        curl_easy_cleanup(curl);
        return 1;
    }
    return 0;
}

int submit_block(const char* node, const char* address, const solution_t* solution) {
    atomic_fetch_add(&total_submits, 1);
    CURL *curl;
    CURLcode res;
    struct memory chunk = {0};
    char url[256];
    char post_fields[1024];

    char* hit_str = mpz_get_str(NULL, 10, solution->hit);
    char* target_str = mpz_get_str(NULL, 10, solution->target);
    char* difficulty_str = mpz_get_str(NULL, 10, solution->difficulty);

    snprintf(url, sizeof(url), "%s/mine.php?q=submitHash", node);
    snprintf(post_fields, sizeof(post_fields),
        "argon=%s&nonce=%s&height=%ld&difficulty=%s&address=%s&hit=%s&target=%s&date=%ld&elapsed=%d&minerInfo=phpcoin-c-miner&version=1.6.8",
        solution->argon, solution->nonce, solution->height, difficulty_str, address,
        hit_str, target_str, solution->date, solution->elapsed);

    free(hit_str);
    free(target_str);
    free(difficulty_str);

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(chunk.response);
            return 0;
        }

        pthread_mutex_lock(&console_mutex);
        printf("\nSubmission response: %s\n", chunk.response);
        pthread_mutex_unlock(&console_mutex);

        // Basic check for "ok" status in response
        int success = (strstr(chunk.response, "\"status\":\"ok\"") != NULL);
        if (success) {
            atomic_fetch_add(&total_accepted, 1);
        } else {
            atomic_fetch_add(&total_rejected, 1);
        }

        free(chunk.response);
        curl_easy_cleanup(curl);
        return success;
    }
    return 0;
}


// --- Mining Thread ---

void* miner_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    thread_stats_t* stats = data->stats;
    stats->pid = syscall(SYS_gettid);

    mpz_t hit, target;
    mpz_inits(hit, target, NULL);

    long sleep_time = (100 - data->cpu_usage) * 500;
    uint64_t thread_nonce = 0;


    while (!block_found) {
        if (data->cpu_usage < 100) {
            usleep(sleep_time);
        }
        long current_time = time(NULL);
        int elapsed = current_time - data->block_date;
        if (elapsed < 0) elapsed = 0;

        stats->height = data->height;
        stats->elapsed = elapsed;

        char* argon = calculate_argon_hash(data->address, data->block_date, elapsed, data->height, stats, thread_nonce);
        if (!argon) continue;

        char* nonce = calculate_nonce(data->address, data->block_date, elapsed, argon);
        if (!nonce) {
            free(argon);
            continue;
        }

        calculate_hit(hit, data->address, nonce, data->height, data->difficulty);
        calculate_target(target, elapsed, data->difficulty);

        thread_nonce++;

        // --- Update stats ---
        pthread_mutex_lock(&stats->stat_mutex);
        mpz_set(stats->hit, hit);
        mpz_set(stats->target, target);
        if (mpz_cmp(hit, stats->best_hit) > 0) {
            mpz_set(stats->best_hit, hit);
        }
        pthread_mutex_unlock(&stats->stat_mutex);
        // --- End of stats update ---

        if (mpz_cmp(hit, target) > 0) {
            // Use a mutex to ensure only one thread can set the solution
            pthread_mutex_lock(&solution_mutex);
            if (!block_found) { // Double check after acquiring the lock
                block_found = true;
                found_solution = malloc(sizeof(solution_t));
                mpz_inits(found_solution->difficulty, found_solution->hit, found_solution->target, NULL);
                found_solution->argon = argon; // Transfer ownership of the memory
                found_solution->nonce = nonce; // Transfer ownership of the memory
                found_solution->height = data->height;
                mpz_set(found_solution->difficulty, data->difficulty);
                found_solution->date = data->block_date + elapsed;
                mpz_set(found_solution->hit, hit);
                mpz_set(found_solution->target, target);
                found_solution->elapsed = elapsed;

                pthread_mutex_lock(&console_mutex);
                printf("\n\n!!! BLOCK FOUND BY THREAD %d !!!\n", data->thread_id);
                gmp_printf("Height: %ld\nNonce: %s\nHit: %Zd\nTarget: %Zd\n\n",
                    found_solution->height, found_solution->nonce, found_solution->hit, found_solution->target);
                pthread_mutex_unlock(&console_mutex);
            }
             pthread_mutex_unlock(&solution_mutex);
        }

        if(!block_found) {
            free(argon);
            free(nonce);
        }

        // Check for a new block on the network every 10 attempts
        if (atomic_load(&stats->hashes) % 10 == 0) {
            long current_network_height;
            mpz_t temp_difficulty;
            mpz_init(temp_difficulty);
            long temp_date;
            if (get_mining_info(data->node, &current_network_height, temp_difficulty, &temp_date)) {
                if (current_network_height > data->height) {
                    if(!block_found) { // prevent multiple dropped messages
                        atomic_fetch_add(&total_dropped, 1);
                        pthread_mutex_lock(&console_mutex);
                        printf("\nNew block detected on the network. Restarting miner...\n");
                        pthread_mutex_unlock(&console_mutex);
                        block_found = true; // Signal main loop to restart
                    }
                }
            }
            mpz_clear(temp_difficulty);
        }
    }

    mpz_clears(hit, target, NULL);
    free(data); // Free the thread-specific data
    return NULL;
}

// --- Main Function ---

void print_usage(const char* prog_name) {
    fprintf(stderr, "Usage: %s --node <node_url> --address <address> [--threads <threads>] [--cpu <cpu>] [--report-interval <interval>] [--flat-log]\n", prog_name);
}

int main(int argc, char** argv) {
    // --- Configurable parameters ---
    // 1. Set hardcoded defaults
    char* node = NULL;
    char* address = NULL;
    int num_threads = 4;
    int cpu_usage = 100;
    int report_interval = 30;
    bool flat_log = false;
    int opt;

    // 2. Load from miner.conf, overriding defaults
    parse_config("miner.conf", &node, &address, &num_threads, &cpu_usage, &report_interval);
    char* conf_node_ptr = node; // Keep track of pointers from config to free them later if needed
    char* conf_address_ptr = address;


    // 3. Parse command-line arguments, overriding both defaults and config file values
    static struct option long_options[] = {
        {"node", required_argument, 0, 'n'},
        {"address", required_argument, 0, 'a'},
        {"threads", required_argument, 0, 't'},
        {"cpu", required_argument, 0, 'c'},
        {"report-interval", required_argument, 0, 'i'},
        {"flat-log", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "n:a:t:c:i:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 0:
                if (strcmp(long_options[option_index].name, "flat-log") == 0) {
                    flat_log = true;
                }
                break;
            case 'n':
                node = optarg;
                break;
            case 'a':
                address = optarg;
                break;
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'c':
                cpu_usage = atoi(optarg);
                break;
            case 'i':
                report_interval = atoi(optarg);
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Free memory from config file if overridden by command line
    if (node != conf_node_ptr) {
        free(conf_node_ptr);
    }
    if (address != conf_address_ptr) {
        free(conf_address_ptr);
    }

    if (!node || !address) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (num_threads <= 0) num_threads = 1;
    if (cpu_usage <=0 || cpu_usage > 100) cpu_usage = 100;
    if (report_interval <= 0) report_interval = 1;


    long height, block_date;
    mpz_t difficulty;
    mpz_init(difficulty);

    while(1) {
        printf("Fetching initial mining info from %s...\n", node);
        if (!get_mining_info(node, &height, difficulty, &block_date)) {
            fprintf(stderr, "Failed to get mining info. Retrying in 10 seconds.\n");
            sleep(10);
            continue;
        }

        gmp_printf("Starting miner for address %s\nHeight: %ld\nDifficulty: %Zd\nThreads: %d\nCPU: %d%%\nReport Interval: %ds\n",
            address, height, difficulty, num_threads, cpu_usage, report_interval);
        printf("---------------------------------------------------\n");


        pthread_t* threads = malloc(sizeof(pthread_t) * num_threads);
        mining_stats = malloc(sizeof(thread_stats_t) * num_threads);

        block_found = false;
        if(found_solution) {
            mpz_clears(found_solution->difficulty, found_solution->hit, found_solution->target, NULL);
            free(found_solution->argon);
            free(found_solution->nonce);
            free(found_solution);
            found_solution = NULL;
        }


        for (int i = 0; i < num_threads; i++) {
            thread_data_t* data = malloc(sizeof(thread_data_t));
            mining_stats[i].id = i + 1;
            atomic_init(&mining_stats[i].hashes, 0);
            mpz_inits(mining_stats[i].hit, mining_stats[i].best_hit, mining_stats[i].target, NULL);
            pthread_mutex_init(&mining_stats[i].stat_mutex, NULL);

            data->thread_id = i + 1;
            data->address = address;
            data->node = node;
            data->height = height;
            data->block_date = block_date;
            data->cpu_usage = cpu_usage;
            mpz_init_set(data->difficulty, difficulty);
            data->stats = &mining_stats[i];

            pthread_create(&threads[i], NULL, miner_thread, data);
        }

        struct timespec last_report_time;
        clock_gettime(CLOCK_MONOTONIC, &last_report_time);
        bool header_printed = false;

        while (!block_found) {
            sleep(1);
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double interval = (now.tv_sec - last_report_time.tv_sec) + (now.tv_nsec - last_report_time.tv_nsec) / 1e9;

            if (interval >= report_interval) {
                if (!flat_log && header_printed) {
                     // Move cursor up by num_threads lines
                    printf("\033[%dA", num_threads);
                }

                pthread_mutex_lock(&console_mutex);

                if (!header_printed) {
                    printf("%-6s %-7s %-5s %-8s %-10s %-10s %-10s %-5s %-5s %-5s %-5s\n",
                           "PID", "Height", "Elapsed", "Speed", "Hit", "Best", "Target", "Submits", "Accepted", "Rejected", "Dropped");
                    header_printed = true;
                }

                if (interval < 1) interval = 1;

                for (int i = 0; i < num_threads; i++) {
                    long thread_hashes = atomic_exchange(&mining_stats[i].hashes, 0);
                    mining_stats[i].speed = (double)thread_hashes / interval;
                    char speed_str[16];
                    snprintf(speed_str, sizeof(speed_str), "%.1f H/s", mining_stats[i].speed);

                    char hit_str[32], best_hit_str[32], target_str[32];
                    pthread_mutex_lock(&mining_stats[i].stat_mutex);
                    gmp_sprintf(hit_str, "%Zd", mining_stats[i].hit);
                    gmp_sprintf(best_hit_str, "%Zd", mining_stats[i].best_hit);
                    gmp_sprintf(target_str, "%Zd", mining_stats[i].target);
                    pthread_mutex_unlock(&mining_stats[i].stat_mutex);


                    printf("%-6d %-7ld %-5d %-8s %-10s %-10s %-10s %-5d %-5d %-5d %-5d\n",
                        mining_stats[i].pid,
                        mining_stats[i].height,
                        mining_stats[i].elapsed,
                        speed_str,
                        hit_str,
                        best_hit_str,
                        target_str,
                        atomic_load(&total_submits),
                        atomic_load(&total_accepted),
                        atomic_load(&total_rejected),
                        atomic_load(&total_dropped)
                    );
                }

                pthread_mutex_unlock(&console_mutex);
                last_report_time = now;
            }
        }

        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
            mpz_clears(mining_stats[i].hit, mining_stats[i].best_hit, mining_stats[i].target, NULL);
            pthread_mutex_destroy(&mining_stats[i].stat_mutex);
        }

        if(found_solution) {
            submit_block(node, address, found_solution);
            printf("Submission attempted. Waiting 5 seconds before starting next block...\n");
            sleep(5);
        }

        free(threads);
        free(mining_stats);
        mining_stats = NULL;
    }
    mpz_clear(difficulty);
    return 0;
}
