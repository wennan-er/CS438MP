/* 
 * File:   sender_main.c
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <vector>
#include <string>
#include <iostream>
#include <errno.h>

using namespace std;

/* Define macros here */
#define PACKET_DATA_LEN 1472
#define TIMEOUT 40000
#define MSS 1

// Message Type
#define SYN 1
#define ACK 2
#define DATA 3
#define FIN 4
#define SYN_ACK 5

// Send Mode
#define INIT 0
#define SEND 1
// #define ACK 2

#define SS 0
#define CC 1
#define FR 2

struct sockaddr_in si_other;
int s, slen;
vector<string> packets;

/* Struct for initialization data */
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

void configure_packet(tcp_packet_t* packet, int seq_num, int mode, int type, struct init_data_t* init_data){
    
    packet->type = type;
    if (mode == INIT){
        packet->seq_num = seq_num;
        packet->init_data.lastPacketSize = init_data->lastPacketSize;
        packet->init_data.numOfPackets = init_data->numOfPackets;
    }
    else if (mode == SEND){
        packet->seq_num = seq_num;
        memcpy(packet->data, packets[seq_num].c_str(), packets[seq_num].size());
    }
    else {
        packet->seq_num = seq_num;
        packet->ack_num = seq_num;
    }
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    char tmpBuf[PACKET_DATA_LEN];
    int numPackets = bytesToTransfer/PACKET_DATA_LEN;
    for(int i = 0; i <= numPackets; i++){
        int read_size = fread(tmpBuf, 1, PACKET_DATA_LEN, fp);
        if(read_size == 0){
            break;
        }
        // cout << read_size << endl;
        string packet = string(tmpBuf, read_size);
        packets.push_back(packet);
    } 
    
    numPackets = packets.size();

	/* Send data and receive acknowledgements on s*/
    tcp_packet_t recvPacket;
    tcp_packet_t sendPacket;
    int state = SS;
    int seq_num = 0;
    int numBytesSent = 0;
    int seq_base = 0;
    int expectACK;

    float cwnd = MSS;
    int ssthresh = 10;
    int dupACKcount = 0;
    int prevACK = -1;
    bool sending = true;

    int retransmit = 0;

    int windowStart = 0;
    int windowEnd = 1;

    struct init_data_t init_data;
    init_data.numOfPackets = numPackets;
    init_data.lastPacketSize = packets.back().size();

    cout << "-------- Initialize communication --------" << endl;
    /* 3 way handshake */
    while(true){
        // Send SYN 
        configure_packet(&sendPacket, 0, INIT, SYN, &init_data);
        if(sendto(s, (void*)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) !=  TCP_PACKET_SIZE){
            diep("sendto");
        }
        // Receive SYN_ACK
        if(recvfrom(s, (void*)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
            diep("recvfrom");
        }
        if(recvPacket.type != SYN_ACK){
            continue;
        }
        // Send ACK
        configure_packet(&sendPacket, 1, ACK, ACK, NULL);
        if(sendto(s, (void*)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) !=  TCP_PACKET_SIZE){
            diep("sendto");
        }
        break;
    }

    /* Setup timeout of the socket */
    struct timeval tv;
    tv.tv_sec = 0; // seconds
    tv.tv_usec = TIMEOUT; // microseconds
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }
    
    cout << "-------- Begin file transmission --------" << endl;
    while (sending){
        if(seq_num == packets.size() and prevACK == packets.size()-1){
            sending = false;
            break;
        }

        /* Slow Start */
        if (state == SS){
            cout << "-------- SLOW START --------" << endl;

            if(retransmit){
                int idx = seq_base;
                while(idx < windowEnd){
                    cout << "-------- Send seq " << idx << " --------" << endl;
                    configure_packet(&sendPacket, idx, SEND, DATA, NULL);
                    numBytesSent = 0;
                    while(numBytesSent != TCP_PACKET_SIZE){
                        numBytesSent = sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen);
                        cout << "Actual sent " << numBytesSent << " bytes" << endl;
                        if(numBytesSent == -1){
                            diep("sendto");
                        }
                    }
                    idx++;
                }
                retransmit = 0;
            }
            else{
                // Send packets.
                cout << "cwnd is " << cwnd << endl;
                for(seq_num = windowStart; seq_num < windowStart+cwnd;seq_num++){
                    if(seq_num == packets.size())
                        break;
                    cout << "-------- Send seq " << seq_num << " --------" << endl;
                    configure_packet(&sendPacket, seq_num, SEND, DATA, NULL);
                    numBytesSent = 0;
                    while(numBytesSent != TCP_PACKET_SIZE){
                        numBytesSent = sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen);
                        cout << "Actual sent " << numBytesSent << " bytes" << endl;
                        if(numBytesSent == -1){
                            diep("sendto");
                        }
                    }
                }
                windowEnd = seq_num;
            }

            cout << "Expect "<< windowStart << " to " << windowEnd-1 << endl;
            for(expectACK = windowStart; expectACK < windowEnd;expectACK++){
                /* Break conditions*/   
                // Go to CC
                if(cwnd >= ssthresh){
                    state = CC;
                }

                cout << "-------- Receive --------" << endl;
                if (recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
                    cout << "Timeout" << endl;
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        ssthresh = cwnd/2;
                        cwnd = MSS;
                        dupACKcount = 0;
                        state = SS;
                        retransmit = 1;
                        break;
                    }
                    diep("recvfrom");
                }

                cout << "Received ACK " << recvPacket.ack_num << endl;
                // cout << "DUPLICATE ACK ACCOUNT " << dupACKcount << endl;
                
                // New ACK
                if(recvPacket.ack_num > prevACK){
                    cwnd = cwnd+MSS;
                    dupACKcount = 0;
                    prevACK = recvPacket.ack_num;
                    seq_base = recvPacket.ack_num+1;
                    windowStart++;
                }
                // Duplicate ACK
                else if(recvPacket.ack_num == prevACK){
                    dupACKcount++;
                    // Go to FR
                    if(dupACKcount == 2){
                        ssthresh = cwnd/2;
                        cwnd = ssthresh+3;
                        state = FR;
                        retransmit = 1;
                        break;
                    }
                    continue;
                }
                else{
                    continue;
                }
            }
        }

        /* Congestin Control */
        if(state == CC){
            cout << "-------- CONGESTION CONTROL --------" << endl;
            // Send packets.
            for(seq_num = windowStart; seq_num < windowStart+cwnd;seq_num++){
                if(seq_num == packets.size())
                    break;
                cout << "-------- Send seq " << seq_num << " --------" << endl;
                configure_packet(&sendPacket, seq_num, SEND, DATA, NULL);
                numBytesSent = 0;
                while(numBytesSent != TCP_PACKET_SIZE){
                    numBytesSent = sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen);
                    cout << "Actual sent " << numBytesSent << " bytes" << endl;
                    if(numBytesSent == -1){
                        diep("sendto");
                    }
                }
            }
            windowEnd = seq_num;
            
            if (cwnd == 0)
                cwnd = MSS;
            if (windowStart == windowEnd)
                windowEnd += cwnd;

            cout << "Expect "<< windowStart << " to " << windowEnd-1 << endl;
            for(expectACK = windowStart; expectACK < windowEnd;expectACK++){

                cout << "-------- Receive --------" << endl;
                if (recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
                    cout << "Timeout" << endl;
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        ssthresh = cwnd/2;
                        cwnd = MSS;
                        dupACKcount = 0;
                        state = SS;
                        retransmit = 1;
                        break;
                    }
                    diep("recvfrom");
                }

                cout << "Received ACK " << recvPacket.ack_num << endl;
                // cout << "DUPLICATE ACK ACCOUNT " << dupACKcount << endl;
                
                // New ACK
                if(recvPacket.ack_num > prevACK){
                    cwnd = cwnd + MSS*(MSS/cwnd);
                    dupACKcount = 0;
                    prevACK = recvPacket.ack_num;
                    seq_base = recvPacket.ack_num+1;
                    windowStart++;
                }
                // Duplicate ACK
                else if(recvPacket.ack_num == prevACK){
                    dupACKcount++;
                    // Go to FR
                    if(dupACKcount == 2){
                        ssthresh = cwnd/2;
                        cwnd = ssthresh+3;
                        state = FR;
                        retransmit = 1;
                        break;
                    }
                    continue;
                }
                else{
                    continue;
                }
            }
        }

        if(state == FR){
            cout << "-------- FAST RECOVERY --------" << endl;
            /* Retransmit missing packet */
            if(retransmit){
                cout << "-------- Send seq " << seq_base << " --------" << endl;
                configure_packet(&sendPacket, seq_base, SEND, DATA, NULL);
                numBytesSent = 0;
                while(numBytesSent != TCP_PACKET_SIZE){
                    numBytesSent = sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen);
                    cout << "Actual sent " << numBytesSent << " bytes" << endl;
                    if(numBytesSent == -1){
                        diep("sendto");
                    }
                }
                cout << "-------- Receive --------" << endl;
                if (recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
                    cout << "Timeout" << endl;
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        ssthresh = cwnd/2;
                        cwnd = MSS;
                        dupACKcount = 0;
                        state = SS;
                        retransmit = 1;
                    }
                }
                cout << "Received ACK " << recvPacket.ack_num << endl;
                if(recvPacket.ack_num == seq_base){
                    retransmit = 0;
                    windowStart++;
                    cwnd = ssthresh;
                    dupACKcount = 0;
                    prevACK = recvPacket.ack_num;
                    seq_base = recvPacket.ack_num+1;
                }
                    
                
            }
            else{
                // Send packets.
                for(seq_num = windowStart; seq_num < windowStart+cwnd;seq_num++){
                    if(seq_num == packets.size())
                        break;
                    cout << "-------- Send seq " << seq_num << " --------" << endl;
                    configure_packet(&sendPacket, seq_num, SEND, DATA, NULL);
                    numBytesSent = 0;
                    while(numBytesSent != TCP_PACKET_SIZE){
                        numBytesSent = sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen);
                        cout << "Actual sent " << numBytesSent << " bytes" << endl;
                        if(numBytesSent == -1){
                            diep("sendto");
                        }
                    }
                }
                windowEnd = seq_num;
            }

            if (cwnd == 0)
                cwnd = MSS;
            if (windowStart == windowEnd)
                windowEnd += cwnd;
            cout << "Expect "<< windowStart << " to " << windowEnd-1 << endl;
            for(expectACK = windowStart; expectACK < windowEnd;expectACK++){
                cout << "-------- Receive --------" << endl;
                if (recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
                    cout << "Timeout" << endl;
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        ssthresh = cwnd/2;
                        cwnd = MSS;
                        dupACKcount = 0;
                        state = SS;
                        retransmit = 1;
                        break;
                    }
                    diep("recvfrom");
                }

                cout << "Received ACK " << recvPacket.ack_num << endl;
                // cout << "DUPLICATE ACK ACCOUNT " << dupACKcount << endl;

                // New ACK
                if(recvPacket.ack_num > prevACK){
                    cwnd = ssthresh;
                    dupACKcount = 0;
                    prevACK = recvPacket.ack_num;
                    seq_base = recvPacket.ack_num+1;
                    state = CC;
                    windowStart++;
                }
                // Duplicate ACK
                else if(recvPacket.ack_num == prevACK){
                    cwnd++;
                    retransmit = 1;
                    break;
                }
                else{
                    continue;
                }
            }
        }
    }

    cout << "-------- End Transmission --------" << endl;
    while(true){
        // Send FIN
        cout << "-------- Send FIN" << seq_num << " --------" << endl;
        configure_packet(&sendPacket, seq_num, FIN, FIN, NULL);
        if(sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) != TCP_PACKET_SIZE){
            diep("sendto");
        }
        // Receive ACK
        if(recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
            diep("recvfrom");
        }
        cout << "Received ACK" << recvPacket.ack_num << endl;
        // Receive FIN
        if(recvfrom(s, (void *)&recvPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) != TCP_PACKET_SIZE){
            diep("recvfrom");
        }
        cout << "Received FIN" << recvPacket.ack_num << endl;
        cout << "-------- Send ACK" << recvPacket.ack_num << " --------" << endl;
        configure_packet(&sendPacket, recvPacket.ack_num, ACK, ACK, NULL);
        if(sendto(s, (void *)&sendPacket, TCP_PACKET_SIZE, 0, (struct sockaddr *)&si_other, slen) != TCP_PACKET_SIZE){
            diep("sendto");
        }
        break;
    }

    fclose(fp);
    printf("Closing the socket\n");
    close(s);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;
    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}