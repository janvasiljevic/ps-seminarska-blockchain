// http: // www.physics.rutgers.edu/~haule/509/MPI_Guide_C++.pdf

#include "openssl/md5.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include <math.h>
#include <mpi.h>

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
int world_size;
int my_id;
int iter = 0;

// int *sendbuff, *recvbuff;
MPI::Datatype mpi_payload_type;

void coordinate(std::string hex_word);
void worker(std::string payload);
void pritnByteArr(unsigned char *buff, int len);
void value_to_string(char *str, unsigned char *hash, int lenHash);
int num_of_trailing_zeroes(char *hashStr);

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

  MPI::Init(argc, argv);
  world_size = MPI::COMM_WORLD.Get_size();
  my_id = MPI::COMM_WORLD.Get_rank();

  if (my_id == 0)
  {
    // i'm more equal then the others

    // log basic info
    std::cout << ">Mater here!" << std::endl
              << "File in: " << in_file_name << std::endl
              << "num of zeroes: " << g_num_of_zeroes << std::endl;

    // read file and start processing
    std::ifstream file(in_file_name);
    if (file.is_open())
    {
      for (std::string line; std::getline(file, line);)
      {
        // std::cout << line << std::endl;
        if (line[0] != '#')
        {
          std::cout << "> Job init" << std::endl;
          coordinate(line);
        }
      }
    }
    else
    {
      std::cout << "Can't open file!" << std::endl;
      exit(1);
    }
  }
  else
  {
    // we are slaves,... oh well... life ig
    // wait for the MPI message for us to start working
    std::cout << "> Slave " << my_id << ", reporting for duty" << std::endl;
    while (true)
    {
      MPI::Status status;
      char *payload = (char *)yoink(str_size * sizeof(char));
      MPI::COMM_WORLD.Recv(payload, sizeof(payload), MPI::CHAR, 0, PAYLOAD_TAG, status);
      worker(payload);
      yeet(payload);
    }
  }
  MPI::Finalize(); // clean after yourself MPI!
  return 0;
}

// #define P(x) std::cout << x << std::endl;

void purge()
{
}

void coordinate(std::string hex_word)
{
  bool solution_found = false;

  char *payload = (char *)yoink(str_size * sizeof(char));
  for (int i = 0; i < world_size; i++)
  {
    strcpy(payload, hex_word.c_str());
    MPI::COMM_WORLD.Send(&payload, sizeof(payload), MPI::CHAR, i, PAYLOAD_TAG);
  }

  unsigned char *val = (unsigned char *)yoink(NONCE_LEN * sizeof(unsigned char));
  MPI::COMM_WORLD.Recv(val, NONCE_LEN, MPI::UNSIGNED_CHAR, MPI::ANY_SOURCE, HASH_TAG);
  std::cout << "received from slave " << val << std::endl;

  // send completed
  int done = 1;
  for (int i = 1; i < world_size; i++)
  {
    MPI::COMM_WORLD.Send(&done, 1, MPI::INT, i, COMPLETED_TAG);
  }
  iter++;
  yeet(payload);
  yeet(val);
  MPI::COMM_WORLD.Barrier();
}

void worker(std::string payload)
{
  // this is our worker https://www.youtube.com/watch?v=YQqLejog8m8
  std::cout << "> slave job init : " << my_id << std::endl;

  // wait for job alert

  int prefix_len = ceil(log2(world_size));
  int byte_len = payload.length() / 2;
  uint8_t *val = (uint8_t *)yoink(byte_len * sizeof(uint8_t));
  const char *pos = payload.c_str();
  uint8_t prefix = my_id;
  uint8_t prefix_overflow = world_size >= 256 ? (my_id + 1) % 255 : my_id + 1; // tkae care potential overflow at large process counts

  // convert string hey to byte array
  // 01 02 03 04 C2 -> 0x01 0x02...
  for (int i = 0; i < byte_len; i++)
  {
    sscanf(pos, "%2hhx", &val[i]);
    pos += 2;
  }

  // init MD5 context
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, val, byte_len);

  uint8_t *nonce = (uint8_t *)smooth_yoink(NONCE_LEN, sizeof(uint8_t));
  // Set nonce_len byte prefix for thread
  nonce[NONCE_LEN - 1] = prefix;

  MD5_CTX ctxNext;
  int done = 0;
  auto req = MPI::COMM_WORLD.Irecv(&done, 1, MPI::INT, 0, COMPLETED_TAG);

  while (true)
  {
    req.Test();
    if (done)
    {
      std::cout << ">completed by done" << std::endl;
      break;
    }

    // increment nonce by one
    for (int r = 0; r <= NONCE_LEN - 1; r++)
    {
      if (++nonce[r])
        break;
    }
    // check if we've overflown the problem space
    if (nonce[NONCE_LEN - 1] == prefix_overflow)
    {
      std::cout << "> completed by queue drained." << std::endl;
      break;
    }
    ctxNext = ctx;

    MD5_Update(&ctxNext, nonce, NONCE_LEN);

    unsigned char hash[16];
    char hashStr[33];

    MD5_Final(hash, &ctxNext);
    value_to_string(hashStr, hash, 16);

    int c = num_of_trailing_zeroes(hashStr);
    if (c == g_num_of_zeroes)
    {
      // HANDLE HASH FOUND
      std::cout << "------------------------ FOUND HASH ---------------------" << std::endl
                << "hash : " << hashStr << std::endl;
      pritnByteArr(nonce, NONCE_LEN);
      MPI::COMM_WORLD.Send(&nonce, NONCE_LEN, MPI::UNSIGNED_CHAR, 0, HASH_TAG);
      // break and go into "wait" loop
      req.Free();
      break;
    }
  }
  yeet(val);
  // yeet(&pos);
  MPI::COMM_WORLD.Barrier();
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
