/* 
 * File:   receiver_main.c
 * Author: Minghao Jiang
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
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>


using namespace std;

/* Define macros here */
#define PACKET_DATA_LEN 1472
#define TIMEOUT 22000

// Message Type
#define SYN 1
#define ACK 2
#define DATA 3
#define FIN 4
#define SYN_ACK 5

#define DEBUG 0

#if DEBUG == 1
int dropSeq = 5;
int dropFlag = 1;
int dropSeq2 = 6;
#endif


/* Struct for initialization data */
struct sockaddr_in si_me, si_other;
int s, slen;


typedef struct init_data_t {
    int numOfPackets;
    int lastPacketSize;
} init_data_t;

/* Struct for tcp packet */
typedef struct tcp_packet_t {
    int seq_num;
    int ack_num;
    int type;
    struct init_data_t init_data;
    char data[PACKET_DATA_LEN];
} tcp_packet_t;

const int TCP_PACKET_SIZE = sizeof(tcp_packet_t);

void diep(string s) {
    perror(s.c_str());
    exit(1);
}

void configure_packet(tcp_packet_t* packet, int ack_num, int type){
    packet->type = type;
    packet->ack_num = ack_num;
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


	/* Now receive data and send acknowledgements */ 
    tcp_packet_t recvPacket;
    tcp_packet_t sendPacket;
    int ack_num = 0;
    int numOfPackets = 0;
    int lastPacketSize = 0;
    vector<string> packets;
    string recvBuf;
    int dupACKcount = 0;
    priority_queue<int, vector<int>, greater<int> > seqBuf;
    unordered_map<int, bool> packetCommit;

    cout << "-------- Initialize communication --------" << endl;
    while(true){
        // Receive SYN
        if(recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
            diep("recvfrom");
        }
        if(recvPacket.type != SYN){
            continue;
        }
        numOfPackets = recvPacket.init_data.numOfPackets;
        lastPacketSize = recvPacket.init_data.lastPacketSize;

        // Send SYN_ACK
        configure_packet(&sendPacket, 0, SYN_ACK);
        if(sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) != TCP_PACKET_SIZE){
            diep("sendto");
        }

        // Receive ACK
        if(recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
            diep("recvfrom");
        }
        if(recvPacket.type != ACK){
            continue;
        }
        break;
    }
    packets.resize(numOfPackets);
    cout << numOfPackets << endl;
    ack_num = -1;
    cout << "-------- Begin file transmission --------" << endl;
    while(true){
        // cout << "-------- Receive --------" << endl;
        if(recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
            diep("recvfrom");
        }
        cout << "-------- Received Seq" << recvPacket.seq_num << ", expect Seq" << ack_num+1 << " --------" << endl;
        if (recvPacket.type == FIN){
            cout << "-------- Receive FIN" << recvPacket.seq_num << " --------" << endl;
            ack_num = recvPacket.seq_num;
            break;
        }

        #if DEBUG == 1
        if ((recvPacket.seq_num == dropSeq || recvPacket.seq_num == dropSeq2) && dropFlag == 1){
            cout << "drop" << endl;
            dropSeq += 20;
            dropSeq2 += 20;
            continue;
        }
        #endif

        if(recvPacket.seq_num == numOfPackets-1){
            recvBuf = string(recvPacket.data, lastPacketSize);
        }
        else{
            recvBuf = string(recvPacket.data, PACKET_DATA_LEN);
        }
        packets[recvPacket.seq_num] = recvBuf;

        if(dupACKcount == 2 && recvPacket.seq_num == ack_num+1){
            ack_num = recvPacket.seq_num;
            cout << "-------- Send ACK " << ack_num << " --------" << endl;
            configure_packet(&sendPacket, ack_num, ACK);
            if(sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) != TCP_PACKET_SIZE){
                diep("sendto");
            }
            packetCommit[ack_num] = true;
            dupACKcount = 0;
        }
        else if(packetCommit.find(recvPacket.seq_num) != packetCommit.end() && packetCommit[recvPacket.seq_num] == true) {
            // for(int commitACK = recvPacket.seq_num; commitACK <= ack_num; commitACK++){
            //     if(packetCommit[recvPacket.seq_num] == true){
            //         cout << "-------- Send commit ACK " << commitACK << " --------" << endl;
            //         configure_packet(&sendPacket, commitACK, ACK);
            //         if(sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) != TCP_PACKET_SIZE){
            //             diep("sendto");
            //         }
            //     }
            // }
            continue;
        }
        else{
            seqBuf.push(recvPacket.seq_num);
        }
        

        while(!seqBuf.empty()){
            if (dupACKcount == 2){
                break;
            }
            if(seqBuf.top() == ack_num+1){
                ack_num = seqBuf.top();
                seqBuf.pop();
            }
            else if(seqBuf.top() < ack_num){
                seqBuf.pop();
                break;
            }
            else{
                dupACKcount++;
            }
            cout << "-------- Send ACK " << ack_num << " --------" << endl;
            configure_packet(&sendPacket, ack_num, ACK);
            if(sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) != TCP_PACKET_SIZE){
                diep("sendto");
            }
            packetCommit[ack_num] = true;
        }
    }

    cout << "-------- End Transmission --------" << endl;
    while(true){
        cout << "-------- Send FIN ACK " << ack_num << " --------" << endl;
        configure_packet(&sendPacket, ack_num, ACK);
        if(sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) != TCP_PACKET_SIZE){
            diep("sendto");
        }
        // Send FIN
        cout << "-------- Send FIN " << ack_num+1 << " --------" << endl;
        configure_packet(&sendPacket, ack_num+1, FIN);
        if(sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) != TCP_PACKET_SIZE){
            diep("sendto");
        }
        // Receive ACK
        if(recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
            diep("recvfrom");
        }
        cout << "Received ACK" << recvPacket.seq_num << endl;
        break;
    }

    for(int i = 0;i < packets.size();i++){
        fwrite(packets[i].c_str(), 1, packets[i].size(), fp);
    }

    fclose(fp);
    cout << "Closing the socket" << endl;
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