/* 
 * File:   receiver_main.c
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
#define FRAME_SIZE 1024
#define CACHE_SIZE 5000

#define PACKETNUM 12
#define MAXDATASIZE 1024
struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

typedef struct Packet
{
    int seq;
    char frame[FRAME_SIZE];
} packet;


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


    char buf[MAXDATASIZE];
    int num;
	/* Now receive data and send acknowledgements */
    FILE *fd;
    fd = fopen(destinationFile, "w+"); //opening file

    memset(buf, 0, MAXDATASIZE);

    int current_packet = 1;
    struct Packet cache[CACHE_SIZE];
    int i = 0;
    while (i < CACHE_SIZE) {
        cache[i].seq = 0;
        i ++;
    }
    int max_pnum = 0;
    while (1) {
        if ((num = recvfrom(s, buf, MAXDATASIZE - 1, 0, (struct sockaddr *) &si_other, (socklen_t * ) & slen)) > 0) {
            printf("%d\n", num);
            printf("buffer: %s\n", buf);

            if (strcmp(buf, "end") == 0) {
                while (current_packet < max_pnum) {
                    if (cache[current_packet%CACHE_SIZE].seq != current_packet) {
                        printf("error for end");
                        break;
                    } else {
                        fprintf(fd, "%s",  cache[current_packet%CACHE_SIZE].frame + PACKETNUM);
                    }
                    current_packet++;
                }
                break;
            }

            char* token;
            token = strtok(buf, " ");

            buf[num] = '\0';
            char packet_num[PACKETNUM];
            memcpy(packet_num, token, PACKETNUM);
            int p_num = atoi(packet_num);
            printf("%s\n", packet_num);
            //printf("%s\n", buf + strlen(packet_num)+1);

            if (p_num > max_pnum) {
                max_pnum = p_num;
            }

            printf("dump1\n");
            if (p_num == current_packet) {
                printf("dump4\n");
                
                fprintf(fd, "%s", buf + PACKETNUM);
                //memcpy(fd, buf + PACKETNUM, MAXDATASIZE-PACKETNUM);
                //fd += MAXDATASIZE-PACKETNUM;
                printf("dump2\n");
                current_packet ++;
                while (cache[current_packet%CACHE_SIZE].seq == current_packet) {
                    printf("dump3\n");
                    fprintf(fd, "%s",  cache[current_packet%CACHE_SIZE].frame + PACKETNUM);
                     
                    //memcpy(fd, cache[current_packet%CACHE_SIZE].frame + PACKETNUM, MAXDATASIZE-PACKETNUM);
                    //fd += MAXDATASIZE-PACKETNUM;
                    current_packet ++;
                    cache[current_packet%CACHE_SIZE].seq = 0;
                }
            }
            printf("dump5\n");
            if (p_num > current_packet) {
                if (cache[p_num%CACHE_SIZE].seq != p_num) {
                    cache[p_num%CACHE_SIZE].seq = p_num;
                    memcpy(cache[p_num%CACHE_SIZE].frame, buf, MAXDATASIZE);
                }
            }
            

//            if (p_num < current_packet) {
//                char re_ack[PACKETNUM];
//                snprintf(re_ack,PACKETNUM,"%d",p_num);
//                sendto(s, re_ack, strlen(re_ack), 0, (struct sockaddr *) &si_other, ((socklen_t)slen));
//                continue;
//            }

            memset(buf, 0, MAXDATASIZE);
            printf("dump6\n");
            char ack[PACKETNUM];
            snprintf(ack,PACKETNUM,"%d",current_packet-1);
            printf("dump7\n");
            sendto(s, ack, strlen(ack), 0, (struct sockaddr *) &si_other, ((socklen_t)slen));

            }
//            if (num < MAXDATASIZE-1) { // need to fix
//                break;
//            }
        }
    char end[MAXDATASIZE] = "end";
    sendto(s, end, strlen(end), 0, (struct sockaddr *) &si_other, ((socklen_t)slen));

    printf("dump8\n");
    fclose(fd);                    //closing file
    printf("dump9\n");
    close(s);
	printf("%s received.", destinationFile);

    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
    printf("end");
    return 0;
}

