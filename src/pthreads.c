#include "openssl/md5.h"
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/queue.h>
#include <errno.h>
#include <omp.h>

// Some important definitions
#define smooth_yoink calloc
#define yeet free
#define PRINT_DIVIDER printf("============================================================\n");

#define ARGS_LEN (4)
#define THREAD_PRINT (0)

// parameteres we will pass to the pThreads
struct params
{
    int index;

    int *nonce_len;

    bool *solution_found;
    bool *live;

    u_int8_t **solved_nonce; // Pointer to the solution (in HEX)

    u_int8_t prefix;      // Prefix of the thread (index in bytes) calculated in the main thread
    u_int8_t upper_limit; // Upper limit calculated in the main thread. Used in the [prefix, upper_limit] problem space
    MD5_CTX *ctx;         // MD5 context set by the main thread

    ulong *hash_count;
};

// Main coordinator
void coordinate(int index, char *hex_word, int *nonce_len, bool *solution_found, MD5_CTX *ctx, unsigned char **solved_nonce);

// Pthread worker
void *worker(void *arg);

void value_to_string(char *str, unsigned char *hash, int lenHash);
int num_of_trailing_zeroes(char *hash_str);
void print_hash_count(long long unsigned hash_count, double time);

// Stdout helper functions
void print_bits(size_t const size, void const *const ptr);

// Locks
pthread_mutex_t solution_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t utility_lock = PTHREAD_MUTEX_INITIALIZER;

// Barriers
pthread_barrier_t cord_barrier;

// Globals
int g_num_of_zeroes;
int g_max_nonce_len;

long long unsigned g_hash_count;
long long unsigned g_total_of_hash_count;

double start_program;

int main(int argc, char **argv)
{

    if (argc != ARGS_LEN + 1)
    {
        errno = EINVAL;
        perror("Invalid number of arguments! \n");
        printf("./program [input_file] [num_of_zeroes] [thread_count] [max_nonce_len] \n");
        return -1;
    }

    /** Read CLI arguments */
    char *in_file_name = argv[1];
    g_num_of_zeroes = atoi(argv[2]);
    int num_of_threads = atoi(argv[3]);
    g_max_nonce_len = atoi(argv[4]);

    if (num_of_threads < 2)
    {
        errno = EINVAL;
        perror("Specify a thread count higher than 2 \n");
        return -1;
    }

    if (!(ceil(log2(num_of_threads)) == floor(log2(num_of_threads))))
    {
        errno = EINVAL;
        perror("Thread count isn't of the form 2^n \n");
        printf("Did you perhaps mean %d? \n", (int)pow(2, floor(log2(num_of_threads))));
        return -1;
    }

    PRINT_DIVIDER
    printf("> File in: %s\n> Number of zeroes: %d\n> Thread count: %d\n> Max nonce length: %d \n", in_file_name, g_num_of_zeroes, num_of_threads, g_max_nonce_len);
    PRINT_DIVIDER

    start_program = omp_get_wtime();

    FILE *in_file = fopen(in_file_name, "r");
    char line[256];

    /** Prepare the threads */
    bool live = true;

    int b_threads = (int)log2(num_of_threads);

    // only allocate pointers, these are all values we will set later
    int nonce_len;
    unsigned char *solved_nonce;
    bool solution_found;

    pthread_barrier_init(&cord_barrier, NULL, num_of_threads + 1);

    struct params threads_params[num_of_threads];
    pthread_t threads[num_of_threads];

    MD5_CTX ctx;
    MD5_Init(&ctx);

    for (int i = 0; i < num_of_threads; i++)
    {
        u_int8_t prefix = (i << (8 - b_threads));
        u_char upper_limit = (0xFF >> b_threads) | prefix;

        threads_params[i].index = i;
        threads_params[i].prefix = prefix;
        threads_params[i].upper_limit = upper_limit;

        threads_params[i].live = &live;
        threads_params[i].ctx = &ctx;
        threads_params[i].nonce_len = &nonce_len;
        threads_params[i].solution_found = &solution_found;
        threads_params[i].solved_nonce = &solved_nonce;

        pthread_create(&threads[i], NULL, worker, &threads_params[i]);
    }

    /** Read the input file */

    int index = 0;

    while (fgets(line, sizeof(line), in_file))
    {
        line[strcspn(line, "\n")] = 0;

        nonce_len = 1;
        solved_nonce = (u_int8_t *)smooth_yoink(nonce_len, sizeof(u_int8_t));
        solution_found = false;

        if (line[0] != '#')
        {
            coordinate(index, line, &nonce_len, &solution_found, &ctx, &solved_nonce);
            index++;
        }
    }

    fclose(in_file);

    live = false;

    pthread_barrier_wait(&cord_barrier);

    for (int i = 0; i < num_of_threads; i++)
        pthread_join(threads[i], NULL);

    double runtime = omp_get_wtime() - start_program;

    printf("[?] The program finished in %.2fs \n", runtime);
    print_hash_count(g_total_of_hash_count, runtime);

    return 0;
}

void coordinate(int index, char *hex_word, int *nonce_len, bool *solution_found, MD5_CTX *ctx, unsigned char **solved_nonce)
{
    int byte_len = strlen(hex_word) / 2;
    const char *pos = hex_word;
    unsigned char val[byte_len];

    for (size_t count = 0; count < byte_len; count++)
    {
        sscanf(pos, "%2hhx", &val[count]);
        pos += 2;
    }

    double start_hash = omp_get_wtime();

    // process MD5 hashing on initial data
    MD5_CTX new_ctx;
    MD5_Init(&new_ctx);
    MD5_Update(&new_ctx, val, byte_len);

    // Update the context in the threads
    *ctx = new_ctx;

    pthread_mutex_lock(&utility_lock);
    g_hash_count = 0;
    pthread_mutex_unlock(&utility_lock);

    // Either Signal the threads to start, or to continue
    pthread_barrier_wait(&cord_barrier);

    PRINT_DIVIDER
    printf("[?] Started coordination on hex word %s \n", hex_word);
    PRINT_DIVIDER

    while (*nonce_len <= g_max_nonce_len)
    {
        // Wait for the threads to finish their work
        pthread_barrier_wait(&cord_barrier);

        // If a solution was found, stop
        if (*solution_found)
        {
            double current_time = omp_get_wtime();

            PRINT_DIVIDER
            printf("[!] Solution has been found [%s] \n", hex_word);
            printf("[*] Found hash in:  %.2fs; program run time: %.2fs \n", current_time - start_hash, current_time - start_program);
            print_hash_count(g_hash_count, current_time - start_hash);
            printf("[?] Nonce found:  ");

            for (int i = 0; i < *nonce_len; i++)
                printf("%02x", (*solved_nonce)[i]);

            printf("\n");
            PRINT_DIVIDER

            break;
        }

        // If a solution wasn't found, increment the nonce by 1
        // and tell the threads to hash on
        else
        {
            // increase the depth (nonce length) of threads to search (nonce_len is a pointer in threads!)
            pthread_mutex_lock(&solution_lock);
            (*nonce_len)++;
            pthread_mutex_unlock(&solution_lock);

            double current_time = omp_get_wtime();

            PRINT_DIVIDER
            printf("[?] Solution has not been found in this iteration [%s] \n", hex_word);
            printf("[*] Current search time:  %.2fs; program run time: %.2fs \n", current_time - start_hash, current_time - start_program);

            if (*nonce_len > g_max_nonce_len)
            {
                printf("[!] Hash couldn't be found, exceeded max nonce length!\n");
                print_hash_count(g_hash_count, current_time - start_hash);
            }
            else
            {
                printf("[?] Increasing nonce_len from %d -> %d\n", *nonce_len - 1, *nonce_len);
            }

            PRINT_DIVIDER

            if (*nonce_len <= g_max_nonce_len)
            {
                // Resize the (solution) nonce
                free(*solved_nonce);
                *solved_nonce = (u_int8_t *)smooth_yoink(*nonce_len, sizeof(u_int8_t));

                // Signal the threads to start work on a new iteration
                pthread_barrier_wait(&cord_barrier);
            }
        }
    }

    g_total_of_hash_count += g_hash_count;

    return;
}

void *worker(void *arg)
{
    /** Initial instructions on thread creation */
    struct params *pargs = (struct params *)arg;

    int index = pargs->index;
    int *nonce_len_p = pargs->nonce_len;

    bool *solution_found = pargs->solution_found;

    u_int8_t prefix = pargs->prefix;
    u_int8_t upper_limit = pargs->upper_limit;

    // We don't really need to lock things here, but it does lead to a much nicer stdout print
    pthread_mutex_lock(&utility_lock);

    printf("> [t %3d] created : prefix: ", index);
    print_bits(1, &prefix);

    pthread_mutex_unlock(&utility_lock);
    /** End of inital instructions */

    // Wait for all the threads to finish initialization
    pthread_barrier_wait(&cord_barrier);

    long long unsigned local_hash_count;

    while (*(pargs->live))
    {
        /** Start of the dynamic part */

        // extremely important, because we update the pointer in the main thread
        int nonce_len = *nonce_len_p;
        MD5_CTX *ctx = pargs->ctx;

        local_hash_count = 0;

        unsigned char *nonce = (u_int8_t *)smooth_yoink(nonce_len, sizeof(u_int8_t));

        // set nonce_len byte prefix for thread
        nonce[nonce_len - 1] = prefix;

        MD5_CTX ctxNext;

#if THREAD_PRINT
        printf("> [t %3d] started hashing : nonce_len %2d \n", index, nonce_len);
#endif

        while (!(*solution_found))
        {
            // increment nonce by one
            for (int r = 0; r <= nonce_len - 1; r++)
            {
                if (++nonce[r])
                    break;
            }

            if (nonce[nonce_len - 1] > upper_limit)
                break;

            // special case for the last thread in the pool ->
            // it will always overflow, so we must check if value
            // equals to 0x00
            else if (nonce[nonce_len - 1] == 0x00 && index != 0)
                break;

            ctxNext = *ctx;

            MD5_Update(&ctxNext, nonce, nonce_len);

            unsigned char hash[16];
            char hashStr[33];

            MD5_Final(hash, &ctxNext);
            value_to_string(hashStr, hash, 16);

            local_hash_count++;

            bool found = num_of_trailing_zeroes(hashStr) == g_num_of_zeroes;

            if (found)
            {
                pthread_mutex_lock(&solution_lock);
                *solution_found = true;
                *pargs->solved_nonce = nonce;
                pthread_mutex_unlock(&solution_lock);
            }
        }

#if THREAD_PRINT
        printf("> [t %3d] finished the main hashing loop \n", index);
#endif

        // Increase the hashcount
        pthread_mutex_lock(&utility_lock);
        g_hash_count += local_hash_count;
        pthread_mutex_unlock(&utility_lock);

        // Signal to the main thread
        pthread_barrier_wait(&cord_barrier);

        // Wait for the main thread to give new instruction
        pthread_barrier_wait(&cord_barrier);

        // must be at the end, because we share the adress with the main thread and don't wish to free it twice
        yeet(nonce);
    }

    printf("> [t %3d] exited from the pool \n", index);

    return NULL;
}

// returs number of trailing zeros in string hash_str
int num_of_trailing_zeroes(char *hash_str)
{
    int i = 31;
    while (i >= 0 && hash_str[i--] == '0')
        ;
    return 30 - i;
}

void print_bits(size_t const size, void const *const ptr)
{
    unsigned char *b = (unsigned char *)ptr;
    unsigned char byte;
    int i, j;

    for (i = size - 1; i >= 0; i--)
    {
        for (j = 7; j >= 0; j--)
        {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}

// converts hash of length lenHash to a string, representing hash with hex numbers
//      str - result
//      hash - address of byte array
//      lenHash - number of data bytes
void value_to_string(char *str, unsigned char *hash, int lenHash)
{
    char sb[3];

    for (int i = 0; i < lenHash; i++)
    {
        sprintf(sb, "%02x", hash[i]);
        str[2 * i] = sb[0];
        str[2 * i + 1] = sb[1];
    }
    str[2 * lenHash] = 0;
}

void print_hash_count(long long unsigned hash_count, double time)
{
    printf("[*] Calculated %llu hashes in total (%lu hashes/ second) \n", hash_count, (ulong)((double)hash_count / ((double)time)));
}
