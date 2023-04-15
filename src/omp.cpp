// http: // www.physics.rutgers.edu/~haule/509/MPI_Guide_C++.pdf

#include "openssl/md5.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include <math.h>
#include <omp.h>

// well this is just necesarry
#define smooth_yoink calloc
#define yoink malloc
#define yeet free

#define BUFF_SIZE 1000
#define ARGS_LEN 2
#define NONCE_LEN 5

#define PAYLOAD_TAG 2
#define HASH_TAG 3
#define COMPLETED_TAG 3

const int str_size = 100;
int g_num_of_zeroes;

void pritnByteArr(unsigned char *buff, int len);
void value_to_string(char *str, unsigned char *hash, int lenHash);
int num_of_trailing_zeroes(char *hashStr);
void shallow_cpy(uint8_t *source, uint8_t *dest, int len);

int main(int argc, char **argv)
{
  // validate arg input
  if (argc != ARGS_LEN + 1)
  {
    std::cout << "Invalid number of arguments!" << std::endl;
    std::cout << "./program [input_file] [num_of_zeroes] " << std::endl;
    return -1;
  }
  char *in_file_name = argv[1];
  g_num_of_zeroes = atoi(argv[2]);

  // init MD5 context
  MD5_CTX ctx;

  // read file and start processing
  std::ifstream file(in_file_name);
  if (file.is_open())
  {
    for (std::string line; std::getline(file, line);)
    {
      // std::cout << line << std::endl;
      if (line[0] != '#')
      {
        // report
        std::cout << "> Job init" << std::endl;
        // clear md5
        MD5_Init(&ctx);
      }
    }
  }
  else
  {
    std::cout << "Can't open file!" << std::endl;
    exit(1);
  }
  return 0;
}

void pritnByteArr(unsigned char *buff, int len)
{
  for (int i = 0; i < len; i++)
  {
    if (i > 0)
      printf(":");
    printf("%02X", buff[i]);
  }
  printf("\n");
}

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

// returs number of trailing zeros in string hashStr
int num_of_trailing_zeroes(char *hashStr)
{
  int i = 31;
  while (i >= 0 && hashStr[i--] == '0')
    ;
  return 30 - i;
}

// create a copy
void deep_cpy(uint8_t *source, uint8_t *dest, int len)
{
  for (int i = 0; i < len; i++)
  {
    dest[i] = source[i];
  }
}

// Start worker
int find_hash(std::string input, MD5_CTX md5_ctx, uint8_t *nonce, int swith_index)
{
  if (swith_index == NONCE_LEN)
  {
    // we reached the end of nonce
    // quit it
    return (1);
  }

  // mine current nonce
  // assign buffers
  unsigned char hash[16];
  char hashStr[33];
  // update md5 state and get value from it
  MD5_Update(&md5_ctx, nonce, NONCE_LEN);
  MD5_Final(hash, &md5_ctx);
  value_to_string(hashStr, hash, 16);
  // check the hash and count trailing zeros
  int c = num_of_trailing_zeroes(hashStr);
  if (c == g_num_of_zeroes)
  {
    // hash found
    std::cout << "------------------------ FOUND HASH ---------------------" << std::endl
              << "hash : " << hashStr << std::endl;
    pritnByteArr(nonce, NONCE_LEN);
    return (0);
  }

  // create a new nonce since we dont want to acces same memory
  uint8_t *temp_nonce = new uint8_t[NONCE_LEN];
  // cpy values
  shallow_cpy(nonce, temp_nonce, NONCE_LEN);

  // dispatch two recursions with switched bits
  // default version
  find_hash(input, md5_ctx, temp_nonce, swith_index + 1);
  // flipped nonce  at index
  temp_nonce[swith_index] = 1;
  find_hash(input, md5_ctx, temp_nonce, swith_index + 1);

  // free memory
  delete temp_nonce;
  // done
  return (0);
}