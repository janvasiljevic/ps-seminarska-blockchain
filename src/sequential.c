#include "openssl/md5.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <omp.h>

// Some important definitions
#define smooth_yoink calloc
#define yeet free

#define ARGS_LEN (3)

int num_of_trailing_zeroes(char *hash_str);
void value_to_string(char *str, unsigned char *hash, int hash_len);
void compute(char *hex_word, unsigned char *ascii_word, int ascii_len, int num_of_zeroes, int max_nonce_len);

void print_hash_count(long long unsigned hash_count, double time);

long long unsigned g_total_of_hash_count;

int main(int argc, char **argv)
{

    if (argc != ARGS_LEN + 1)
    {
        errno = EINVAL;
        perror("Invalid number of arguments! \n");
        printf("./program [input_file] [num_of_zeroes] [max_nonce_len] \n");
        return -1;
    }

    /** Read CLI arguments */
    char *in_file_name = argv[1];
    int num_of_zeroes = atoi(argv[2]);
    int max_nonce_len = atoi(argv[3]);

    printf("============================================================\n");
    printf("> File in: %s\n> Number of zeroes: %d\n> Max nonce length: %d \n", in_file_name, num_of_zeroes, max_nonce_len);
    printf("============================================================\n");

    double start_program = omp_get_wtime();

    /** Read the input file */

    FILE *in_file = fopen(in_file_name, "r");
    char line[256];

    while (fgets(line, sizeof(line), in_file))
    {

        if (line[0] == '#')
            continue;

        line[strcspn(line, "\n")] = 0;

        int byte_len = strlen(line) / 2;
        const char *pos = line;
        unsigned char val[byte_len];

        for (size_t count = 0; count < byte_len; count++)
        {
            sscanf(pos, "%2hhx", &val[count]);
            pos += 2;
        }

        compute(line, val, byte_len, num_of_zeroes, max_nonce_len);
    }

    fclose(in_file);

    double runtime = omp_get_wtime() - start_program;

    printf("[?] The program finished in %.2fs \n", runtime);

    print_hash_count(g_total_of_hash_count, runtime);

    return 0;
}

void compute(char *hex_word, unsigned char *ascii_word, int ascii_len, int num_of_zeroes, int max_nonce_len)
{
    int nonce_len = 1;

    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, ascii_word, ascii_len);

    printf("============================================================\n");
    printf("[?] Started computation on hex word %s \n", hex_word);
    printf("============================================================\n");

    while (nonce_len <= max_nonce_len)
    {

        unsigned char *nonce = (u_int8_t *)smooth_yoink(nonce_len, sizeof(u_int8_t));
        int last_char_pos = nonce_len - 1;

        MD5_CTX ctxNext;

        bool preped = false;

        while (true)
        {
            // increment nonce by one
            for (int r = 0; r <= nonce_len - 1; r++)
            {
                if (++nonce[r])
                    break;
            }

            if (nonce[last_char_pos] == 0xFF)
                preped = true;

            else if (nonce[last_char_pos] == 0x00 && preped)
                break;

            ctxNext = ctx;

            MD5_Update(&ctxNext, nonce, nonce_len);

            g_total_of_hash_count++;

            unsigned char hash[16];
            char hashStr[33];

            MD5_Final(hash, &ctxNext);
            value_to_string(hashStr, hash, 16);

            bool found = num_of_trailing_zeroes(hashStr) == num_of_zeroes;

            if (found)
            {
                printf("============================================================\n");
                printf("[!] Solution has been found [%s] \n", hex_word);

                for (int i = 0; i < nonce_len; i++)
                    printf("%02x", nonce[i]);

                printf("\n");
                printf("============================================================\n");

                return;
            }
        }

        nonce_len++;

        printf("[?] Solution has not been found in this iteration [%s] \n", hex_word);

        if (nonce_len > max_nonce_len)
        {
            printf("============================================================\n");
            printf("[!] Hash couldn't be found, exceeded max nonce length!\n");
            printf("============================================================\n");
        }
        else
        {
            printf("[?] Increasing nonce_len from %d -> %d\n", nonce_len - 1, nonce_len);
        }
    }
}

// returs number of trailing zeros in string hash_str
int num_of_trailing_zeroes(char *hash_str)
{
    int i = 31;
    while (i >= 0 && hash_str[i--] == '0')
        ;
    return 30 - i;
}

// converts hash of length lenHash to a string, representing hash with hex numbers
//      str - result
//      hash - address of byte array
//      lenHash - number of data bytes
void value_to_string(char *str, unsigned char *hash, int hash_len)
{
    char sb[3];

    for (int i = 0; i < hash_len; i++)
    {
        sprintf(sb, "%02x", hash[i]);
        str[2 * i] = sb[0];
        str[2 * i + 1] = sb[1];
    }
    str[2 * hash_len] = 0;
}

void print_hash_count(long long unsigned hash_count, double time)
{
    printf("[*] Calculated %llu hashes in total (%lu hashes/ second) \n", hash_count, (ulong)((double)hash_count / ((double)time)));
}
