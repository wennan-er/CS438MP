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

#define PACKETNUM 12
#define MAXDATASIZE 1024
struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}



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
    while (1) {
        if ((num = recvfrom(s, buf, MAXDATASIZE - 1, 0, (struct sockaddr *) &si_other, (socklen_t * ) & slen)) > 0) {
            printf("%d\n", num);
            char* token;
            token = strtok(buf, " ");

            buf[num] = '\0';
            char packet_num[PACKETNUM];
            strcpy(packet_num, token);
            printf("%s\n", packet_num);
            //printf("%s\n", buf + strlen(packet_num)+1);
            fprintf(fd, "%s", buf + PACKETNUM);              //writing data into file
            memset(buf, 0, MAXDATASIZE);
            sendto(s, packet_num, strlen(packet_num), 0, (struct sockaddr *) &si_other, ((socklen_t)slen));

            }
            if (num < MAXDATASIZE-1) {
                break;
            }
        }
    char end[MAXDATASIZE] = "end";
    sendto(s, end, strlen(end), 0, (struct sockaddr *) &si_other, ((socklen_t)slen));


    fclose(fd);                    //closing file

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

