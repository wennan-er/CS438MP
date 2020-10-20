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

#define HEADER_SIZE 100
#define PACKET_SIZE 1124 //PACKET_SIZE = FRAME_SIZE + HEADER_SIZE
#define FRAME_SIZE 1024
#define TIME_BOUND
#define SERVER_PORT 8000
#define min(a, b) a < b ? a : b
#define max(a, b) a > b ? a : b

#define MAXDATASIZE 1024
#define PACKETNUM 12
#define MAX(a, b) ((a) < (b) ? (b) : (a))
struct sockaddr_in si_other;

int s, slen;
int packet_num = 0;
int flag = 0;

unsigned long long int num_bytes;

/*
  Varible for TCP sender
     1.CW: congestion window size
     2.numberFrame: file split into how many frames
     3.SST: threshhold for slow start
     4.dupACK: use for fast recovery
     5.ackCount: count how many distinct acks recieved, is ackCount==numberFrame, finish!
     
     6.fileFrame: i: pointer point to start of the frame i file in disk.
     7.T-base: base of congestion window: fileFrame
     8.T_tail
     9.ackFrame: 1 for having recieved ACK, 0 for not.
     10.socket1: send packets to receiver

     11.socket2: receive acks from receiver
  
*/
//int CW;
//int numberFrame;
//int SST;
//int dupACK = 0;
//int timeout;
//
//int fileFrame[numberFrame];
//int T_base = 0;
//int T_tail = 0;
//int ackFrame[numberFrame];
//int timeout;
//int ackCount = 0;
//
//bool slowStart = True;
//bool congAvoidance = False;
//bool fastRecovery = False;

FILE *fp;

void diep(char *s)
{
  perror(s);
  exit(1);
}

/*
  TODO:
  receiverThread will listen on sockfd new coming ACKs:
    1.based on original state: change state, CW, SST etc.
*/
void *receiverThread()
{
  int ack_num = -1;
  int num;
  char ack[PACKETNUM];
  while (1)
  {
    if ((num = recvfrom(s, ack, MAXDATASIZE - 1, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen)) > 0)
    {
      printf("%s\n", ack);
      if (strcmp(ack, "end") == 0)
      {
        break;
      }
      ack_num = MAX(ack_num, atoi(ack));
      memset(ack, 0, sizeof(ack));
    }
  }
  return NULL;
}
/*
  TODO:
  senderThread will send packects in congestion window to reciever.
*/

void *senderThread()
{
  char buf[MAXDATASIZE];

  unsigned long long int filesize = num_bytes;
  int numbytes, num, readsize;

  while (filesize > 0)
  {

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

    readsize = MAXDATASIZE - num - 1;
    strcpy(buf, header);

    if (filesize >= (unsigned long long int)readsize)
    {
      filesize -= (unsigned long long int)readsize;
      //printf("if folesize: %d\n",filesize);
    }
    else
    {
      readsize = (int)filesize;
      filesize = 0;
      //printf("else folesize: %d\n",filesize);
    }
    //printf("folesize: %d\n",filesize);
    if ((numbytes = fread(buf + num, sizeof(char), readsize, fp)) > 0)
    {
      buf[num + numbytes] = '\0';
      //printf("%s\n",buf);
      //printf("%s\n",buf+packet_num);
      printf("%d\n", numbytes);
      if ((sendto(s, buf, strlen(buf), 0, (struct sockaddr *)&si_other, (socklen_t)slen)) == -1)
      {
        perror("send");
        printf("send error");
        exit(1);
      }
      memset(buf, 0, sizeof(buf));
      packet_num++;
    }
  }

  flag = 1;
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

  /* Send data and receive acknowledgements on s*/
  /* start senderThread*/
  pthread_t p_sender;
  pthread_create(&p_sender, NULL, &senderThread, NULL);
  /* start recieverThread*/
  pthread_t p_reciever;
  pthread_create(&p_reciever, NULL, &receiverThread, NULL);
  /* join 2 threads */
  pthread_join(p_sender, NULL);
  pthread_join(p_reciever, NULL);

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
