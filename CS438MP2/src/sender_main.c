/* 
 * File:   sender_main.c
 * Author: 
 *
 * Created on 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define min(a, b) a < b ? a : b
#define max(a, b) a > b ? a : b
#define CACHE_SIZE 5000
#define MAXDATASIZE 1024
#define PACKETNUM 12

struct Packet
{
  int seq;
  long double time;
};

unsigned long long int total;
struct sockaddr_in si_other;
int s, slen;
int packet_num = 0;
unsigned long long int num_bytes;
struct Packet cache[CACHE_SIZE];
int retransmit = 0;
double CW;
double minCW = 32;
int SST = 128;
int dupACK = 0;
int T_base = 0;
int T_tail = 0;
int slowStart = 1;
int fastRecovery = 0;
FILE *fp;
int readsize = MAXDATASIZE - PACKETNUM - 1;
unsigned long long int real_size;

int end = 0;
long double TIMEOUT = 0.5;
int retransmit_p = -1;
long double start_time = 0;

void diep(char *s)
{
  perror(s);
  exit(1);
}
double floor(double num)
{
  int res = (int)num;

  return (double)(res);
}
void *timerThread()
{
  printf("Timer starts working!\n");
  long double current_time_out = TIMEOUT;
  long double start_time1;
  long double current_time;
  struct timespec start_t;

  int base = 0;

  while (1)
  {
    if (cache[0].time != 0)
    {
      start_time1 = cache[0].time;
      cache[0].seq = 0;
      cache[0].time = 0;
      break;
    }
  }

  //clock_t time_diff = clock() - start_time;

  while (1)
  {
    if (base < T_base)
    {
      printf("current_time_out: %Lf\n", current_time_out);
      int i = base;
      while (i < T_base && T_base <= total)
      {
        current_time_out += cache[(i + 1) % CACHE_SIZE].time;
        i++;
      }
      base = T_base;
    }

    clock_gettime(CLOCK_MONOTONIC, &start_t);
    current_time = start_t.tv_sec + start_t.tv_nsec / 1000000000.0;
    //current_time = finish_t.tv_sec + finish_t.tv_nsec / 1000000000.0;

    if ((current_time - start_time1) >= current_time_out)
    {
      //printf("Timeout! \n");
      //printf("T_base: %d \n", T_base);
      //SST = (int)(CW / 2);
      CW = minCW;
      dupACK = 0;
      slowStart = 1;
      fastRecovery = 0;
      T_tail = T_base;
      //retransmit = 1;
      clock_gettime(CLOCK_MONOTONIC, &start_t);
      start_time1 = start_t.tv_sec + start_t.tv_nsec / 1000000000.0;
      current_time_out = TIMEOUT;
    }
    if (end == 1)
    {
      printf("timer ends");
      break;
    }
  }
  return NULL;
}

void *receiverThread()
{
  int ack_num = 0;
  int num;
  char ack[PACKETNUM];

  printf("Receiver starts working!\n");
  while (1)
  {
    if ((num = recvfrom(s, ack, PACKETNUM, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen)) > 0)
    {
      //printf("ACK num: %s\n", ack);

      if (strcmp(ack, "end") == 0)
      {
        end = 1;
        break;
      }
      int received_ack = atoi(ack);
      if (received_ack == total)
      {
        end = 1;
        char end[MAXDATASIZE] = "end";
        sendto(s, end, strlen(end), 0, (struct sockaddr *)&si_other, ((socklen_t)slen));
        break;
      }
      memset(ack, 0, sizeof(ack));
      //printf("ack: %d\n", ack_num);
      //printf("received_ack: %d\n", received_ack);
      //printf("CW: %f\n", CW);
      //printf("SST: %d\n", SST);
      //printf("dupAck: %d\n", dupACK);

      if (slowStart)
      {
        //printf("Slow Start begins\n");
        /* A new ACK come */
        if (ack_num < received_ack)
        {
          double new_CW = CW + 2 * ((double)received_ack - (double)ack_num);
          //printf("new_CW: %f\n", new_CW);
          //printf("ack: %d\n", ack_num);
          //printf("received_ack: %d\n", received_ack);

          if (new_CW > (double)SST)
          {
            CW = (double)CW + ((double)received_ack - (double)ack_num) * (double)(1 / floor(CW));
            //printf("Conflict Avoid: %f\n", CW);
          }
          else
          {

            CW = new_CW;
            //printf("Not Conflict Avoid: %f\n", CW);
          }
          dupACK = 0;
          // TODO: send packets based on CW
          T_base += received_ack - ack_num;
          T_tail = T_base + (int)floor(CW) - 1;
          //printf("CW: %f\n", CW);
          //printf("Not Conflict Avoid: %f\n", T_base);
          //printf("Not Conflict Avoid: %f\n", CW);
          ack_num = received_ack;
        }
        else if (ack_num == received_ack)
        {
          dupACK++;
          //retransmit = 1;
          //retransmit_p = received_ack+1;
        }
      }

      if (fastRecovery)
      {
        if (ack_num == received_ack)
        {
          CW += 1;
          dupACK++;
          retransmit = 1;
          retransmit_p = received_ack + 1;
          // TODO: send one new packet
          T_tail += 1;
        }
        else if (ack_num < received_ack)
        {
          CW = (double)SST;
          dupACK = 0;

          // TODO: send packets based on CW
          T_base += received_ack - ack_num;
          T_tail = T_base + (int)floor(CW) - 1;

          ack_num = received_ack;
          slowStart = 1;
          fastRecovery = 0;
        }
      }
      if (dupACK == 3)
      {
        fastRecovery = 1;
        slowStart = 0;
        CW = minCW;
        // SST = (int)(CW / 2);
        // CW = (double)SST + 3;
        // TODO: send packets based on CW
        T_tail = T_base + (int)floor(CW) - 1;
        retransmit = 1;
        retransmit_p = received_ack + 1;
      }
    }
  }
  return NULL;
}

unsigned long long int findSize(FILE *fp)
{
  // checking if the file exist or not
  if (fp == NULL)
  {
    printf("File Not Found!\n");
    return -1;
  }

  fseek(fp, 0L, SEEK_END);

  // calculating the size of the file
  unsigned long long int res = (unsigned long long int)ftell(fp);

  return res;
}
/*
  TODO:
  sendFrame will send frame[index] to receiver
*/
void send_file(FILE *fp, int readsize, char buf[], int packet_num)
{
  struct timespec start;
  //long double elapsed;

  int numbytes, num;
  char header[PACKETNUM];
  snprintf(header, PACKETNUM, "%d", packet_num);
  //pad(header,PACKETNUM);
  int length = strlen(header);
  while (length < PACKETNUM)
  {
    strcat(header, " ");
    length += 1;
  }
  num = strlen(header);
  memcpy(buf, header, PACKETNUM); //change to memcpy

  if ((numbytes = fread(buf + num, sizeof(char), readsize, fp)) > 0)
  {
    buf[num + numbytes] = '\0';
    //printf("%s\n",buf);
    //printf("send %d\n",packet_num);
    //printf("%d\n", numbytes);
    if ((sendto(s, buf, strlen(buf), 0, (struct sockaddr *)&si_other, (socklen_t)slen)) == -1)
    {
      perror("send");
      printf("send error");
      exit(1);
    }
    //printf("send length: %d\n", strlen(buf));

    if (packet_num == 1)
    {
      cache[(packet_num - 1) % CACHE_SIZE].seq = packet_num;
      clock_gettime(CLOCK_MONOTONIC, &start);
      start_time = start.tv_sec + start.tv_nsec / 1000000000.0;
      cache[(packet_num - 1) % CACHE_SIZE].time = start_time;

      printf("cache 1 packet: %d\n", cache[(packet_num - 1) % CACHE_SIZE].seq);
      printf("cache 1 time: %Lf\n", cache[(packet_num - 1) % CACHE_SIZE].time);
    }
    else
    {
      //printf("dump1\n");
      if (cache[(packet_num - 1) % CACHE_SIZE].seq != packet_num)
      {
        //printf("dump1\n");
        cache[(packet_num - 1) % CACHE_SIZE].seq = packet_num;
        //printf("dump1\n");
        clock_gettime(CLOCK_MONOTONIC, &start);
        long double time = start.tv_sec + start.tv_nsec / 1000000000.0;
        //printf("dump1\n");
        cache[(packet_num - 1) % CACHE_SIZE].time = time - start_time;
        printf("time: %Lf\n", time);
        printf("time: %Lf\n", start_time);
        printf("cache packet: %d\n", cache[(packet_num - 1) % CACHE_SIZE].seq);
        printf("cache time: %Lf\n", cache[(packet_num - 1) % CACHE_SIZE].time);
        clock_gettime(CLOCK_MONOTONIC, &start);
        start_time = start.tv_sec + start.tv_nsec / 1000000000.0;
      }
    }

    memset(buf, 0, MAXDATASIZE);
  }
}

void *senderThread()
{
  int current_tail = -1;
  int packet_size = MAXDATASIZE - PACKETNUM - 1;

  char buf[MAXDATASIZE];

  while (end == 0)
  {
    //printf("T_base: %d\n", T_base);
    //printf("T_base: %d\n", (int)total);
    if (current_tail > T_tail)
    {
      current_tail = T_base - 1;
    }
    if (retransmit == 1)
    {
      // printf("retransmit starts\n");
      // printf("retransmit_packet: %d\n", retransmit_p);
      // printf("T_base: %d\n", T_base);
      // printf("T_tail: %d\n", T_tail);
      // printf("current tail: %d\n", current_tail);
      int offset = T_base;
      if (retransmit_p != -1)
      {
        offset = retransmit_p - 1;
        retransmit_p = -1;
      }
      if ((unsigned long long int)(offset) >= total)
      {
        continue;
      }
      fseek(fp, (offset)*packet_size, SEEK_SET);
      packet_num = offset + 1;
      //printf("Packet num: %d\n", packet_num);
      send_file(fp, readsize, buf, packet_num);
      retransmit = 0;
    }
    else
    {

      if (current_tail < T_tail)
      {
        //printf("normal send starts\n");
        int i = current_tail + 1;
        while (i <= T_tail)
        {
          if ((unsigned long long int)i >= total)
          {
            break;
          }

          fseek(fp, i * packet_size, SEEK_SET);
          packet_num = i + 1;
          send_file(fp, readsize, buf, packet_num);
          //printf("Packet num: %d\n", packet_num);
          //printf("CW: %f\n", CW);
          //printf("SST: %d\n", SST);
          //printf("T_base: %d\n", T_base);
          current_tail = i;
          i++;
        }
      }
    }
    if (end == 1 || T_base >= total)
    {
      printf("sender ends");
    }
  }
  return NULL;
}
/*
  hostUDPport: receiver's UDP port
*/
void reliablyTransfer(char *hostname, unsigned short int hostUDPport, char *filename, unsigned long long int bytesToTransfer)
{
  //Open the file
  num_bytes = bytesToTransfer;
  fp = fopen(filename, "rb");
  if (fp == NULL)
  {
    printf("Could not open file to send.");
    exit(1);
  }

  real_size = findSize(fp);
  unsigned long long int filesize = min(num_bytes, real_size);

  if (filesize % (unsigned long long int)(readsize) > 0)
  {
    total = filesize / (unsigned long long int)(readsize) + 1;
  }
  else
  {
    total = filesize / (unsigned long long int)(readsize);
  }
  // printf("real_size: %llu\n", real_size);
  // printf("num_bytes: %llu\n", num_bytes);
  // printf("filesize: %llu\n", filesize);
  // printf("total: %llu\n", total);
  //return;
  /* Determine how many bytes to transfer */
  slen = sizeof(si_other);

  /* initailize sender socket */
  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    diep("socket");

  memset((char *)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(hostUDPport);
  if (inet_aton(hostname, &si_other.sin_addr) == 0)
  {
    fprintf(stderr, "inet_aton() failed\n");
    exit(1);
  }

  if (connect(s, (struct sockaddr *)&si_other, sizeof(si_other)) < 0)
  {
    printf("\n Error : Connect Failed \n");
    exit(0);
  }
  /* initialize variables*/
  memset(&cache, 0, sizeof(cache));

  pthread_t p_sender;
  pthread_create(&p_sender, NULL, &senderThread, NULL);

  pthread_t p_reciever;
  pthread_create(&p_reciever, NULL, &receiverThread, NULL);

  pthread_t p_timer;
  pthread_create(&p_timer, NULL, &timerThread, NULL);

  pthread_join(p_sender, NULL);
  pthread_join(p_reciever, NULL);
  pthread_join(p_timer, NULL);

  printf("Closing the socket\n");
  fclose(fp);
  close(s);
  return;
}

/*
 * 
 */
int main(int argc, char **argv)
{

  unsigned short int udpPort;
  unsigned long long int numBytes;

  if (argc != 5)
  {
    fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
    exit(1);
  }
  udpPort = (unsigned short int)atoi(argv[2]);
  numBytes = atoll(argv[4]);

  reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

  return (EXIT_SUCCESS);
}
