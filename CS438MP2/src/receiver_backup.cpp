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
#include <string>
#include <iostream>
#include <queue>

using namespace std;

struct sockaddr_in si_me, si_other;
int s, slen;
unsigned short int sender_port = 4950;

void diep(string s) {
    perror(s.c_str());
    exit(1);
}



void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    /* Open destination file */
    FILE *fp;
    fp = fopen(destinationFile, "wb");
    if (fp == NULL) {
        printf("Could not open destination file.");
        exit(1);
    }

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

    memset((char *)&si_other, 0, sizeof (si_other));

	/* Now receive data and send acknowledgements */

    /* Setup timeout of the socket */
    // struct timeval tv;
    // tv.tv_sec = 0;
    // tv.tv_usec = 100000;
    // if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    //     perror("Error");
    // }
    
    int recv_size = 1025;
    int sent_size = 5;
    char sendBuf[sent_size];
    char recvBuf[recv_size];
    char writeBuf[recv_size];
    size_t numBytesReceived = 0;
    size_t numBytesSent = 0;
    socklen_t addr_len = sizeof(si_other);
    int ack = 0;
    int seq = 0;
    queue<int> ack_queue;

    // Receive total length now
    numBytesReceived = recvfrom(s, recvBuf, 20, 0, (struct sockaddr *)&si_other, &addr_len);
    if(numBytesReceived <= 0) {
        diep("recvfrom");
    }
    int totalBytesExpect = atoi(recvBuf);
    cout << "Expected to receive " << totalBytesExpect << " bytes" << endl;
    int numPacketsExpect = totalBytesExpect/recv_size;
    int lastPacketSize = totalBytesExpect%recv_size;
    if(lastPacketSize > 0){
        numPacketsExpect++;
    }
    sprintf(sendBuf, "Initiating Transfer");
    numBytesSent = sendto(s, sendBuf, strlen(sendBuf), 0, (struct sockaddr *)&si_other, sizeof(si_other));
    if(numBytesSent < 0){
        diep("initiation");
    }

    for(int i = 1;i <= numPacketsExpect;i++){
        ack_queue.push(i);
    }

    while(true){
        cout << "-------- Now recv --------" << endl;
        numBytesReceived = recvfrom(s, recvBuf, recv_size, 0, (struct sockaddr *)&si_other, &addr_len);
        seq = atoi(&recvBuf[0]);
        
        if(numBytesReceived <= 0){
            printf("-------- Receiving complete --------\n");
            break;
        }
        cout << "Received seq# " << seq << " with packet size " << numBytesReceived << " bytes" << endl;
        // Write to file
        memcpy(writeBuf, recvBuf+1, numBytesReceived);
        fwrite(writeBuf, 1, numBytesReceived-1, fp);

        // pop new ack number only when correct seq number is received 
        if(seq == ack_queue.front()){
            ack = ack_queue.front();
            ack_queue.pop();
        }
        cout << "-------- Send ACK " << ack << " --------" << endl;
        sprintf(sendBuf, "ACK %d", ack);
        numBytesSent = sendto(s, sendBuf, strlen(sendBuf), 0, (struct sockaddr *)&si_other, sizeof(si_other));
        if(numBytesSent < 0){
            diep("ACK");
        }
        cout << endl;
    }

    fclose(fp);
    close(s);
	printf("%s received.\n", destinationFile);
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
}

