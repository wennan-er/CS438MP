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
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <queue>
#include <atomic>

using namespace std;

struct sockaddr_in si_other, si_me;
int s, slen;
unsigned short int my_port = 4950;
size_t window_size = 1;
vector<string> packets;

void diep(string s) {
    perror(s.c_str());
    exit(1);
}

atomic<int> packetSent;
void increment_packet(){
   ++packetSent;
}

atomic<int> dupACK;
void increment_dupACK(){
    dupACK++;
}

/* An atomic data structure for threads to access */
template <typename T>
class atomic_vector
{
    public:
        void push_back(const T &value){
            lock_guard<mutex> lock(m_mutex);
            at_vector.push_back(value);
        }
        void pop_back(){
            lock_guard<mutex> lock(m_mutex);
            at_vector.pop_back();
        }
        T operator[](size_t n){
            lock_guard<mutex> lock(m_mutex);
            return at_vector[n];
        }
        bool empty(){
            lock_guard<mutex> lock(m_mutex);
            return at_vector.empty();
        }
        void erase(vector<int>::iterator position){
            lock_guard<mutex> lock(m_mutex);
            at_vector.erase(position);
        }
        vector<int>::iterator begin(){
            lock_guard<mutex> lock(m_mutex);
            return at_vector.begin();
        }
        
    private:
        vector<T> at_vector;
        mutable mutex m_mutex;
};


struct _arg {
    int sent_size;
};

void *transmit(void* args){
    struct _arg* _args = (struct _arg*) args;
    int sent_size = _args->sent_size;
    int recv_size = 5;
    char sendBuf[sent_size];
    char recvBuf[recv_size];
    socklen_t addr_len;
    strcpy(sendBuf, (packets[packetSent]).c_str());
    cout << "-------- Send --------" << endl;
    // Write file to sendBuf
    size_t numBytesSent;
    size_t numBytesReceived;
    cout << "Now send with length " << strlen(sendBuf) << "bytes " << endl;
    // Repeatedly send packet to server until we succeed
    numBytesSent = 0;
    while(numBytesSent != strlen(sendBuf)){
        numBytesSent = sendto(s, sendBuf, strlen(sendBuf), 0, (struct sockaddr *)&si_other, slen);
        cout << "Actual sent " << numBytesSent << " bytes" << endl;
        if(numBytesSent == -1){
            diep("sendto");
        }
    }   
    increment_packet();

    cout << "-------- Now recv --------" << endl;
    numBytesReceived = recvfrom(s, recvBuf, recv_size, 0, (struct sockaddr *)&si_other, &addr_len);
    if(numBytesReceived == -1){
        diep("recvfrom");
    }
    int expect_ack = packetSent;
    int ack = atoi(&recvBuf[4]);
    cout << "Received " << recvBuf << " expect ACK " << expect_ack << endl;
    cout << endl;
    if(expect_ack != ack){
        close(s);
        diep("ACK");
    }
    return NULL;
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

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(my_port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1){
        diep("bind");
        exit(1);
    }

	/* Send data and receive acknowledgements on s*/

    /* Setup timeout of the socket */
    struct timeval tv;
    tv.tv_sec = 3; // seconds
    tv.tv_usec = 0; // microseconds
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }

    /* Declare Variables here */
    int sent_size = min(1025, int(bytesToTransfer));
    int recv_size = 5;
    char sendBuf[sent_size];
    char recvBuf[recv_size];
    size_t numBytesSent = 0;
    size_t numBytesReceived = 0;
    socklen_t addr_len;
    int tmp[] = {1,2,3,4};
    // vector<int> sender_window(1,1);
    // int ack = 0;
    packetSent = 0; // initialize the atomic<int> packetSent
    // Sender Window declared here 
    vector<int> sender_window(tmp, tmp+sizeof(tmp)/sizeof(int));
    // ACK queue
    // Read files into a vector of string
    char tmpBuf[sent_size];

    int numPackets = bytesToTransfer/1024;
    for(int i = 0; i <= numPackets; i++){
        int read_size = fread(tmpBuf, 1, 1024, fp);
        if(read_size == 0){
            break;
        }
        // cout << read_size << endl;
        string packet = to_string(i+1) + string(tmpBuf, read_size);
        cout << packet.size() << endl;
        packets.push_back(packet);
    }   

    /* First we need to send number of bytes to transfer to receiver */
    cout << "-------- INIT --------" << endl;
    while(numBytesReceived == 0){
        char initBuf[20];
        sprintf(initBuf, "%llu", bytesToTransfer);
        numBytesSent = sendto(s, initBuf, 20, 0, (struct sockaddr *)&si_other, slen);
        if(numBytesSent <= 0){
            diep("initiation");
        }
        numBytesReceived = recvfrom(s, recvBuf, 5, 0, (struct sockaddr *)&si_other, &addr_len);
        if(numBytesReceived == -1){
            diep("initiation");
        }
    }

    /* Now begin file transfer */
    cout << "-------- Begin Tranfer --------" << endl;
    while(true){
        for(int i : sender_window){
            pthread_t transmit_thread;
            struct _arg args;
            args.sent_size = sent_size;

            if (pthread_create(&transmit_thread, NULL, transmit, (void *)&args) != 0){
                cout << "Create transmit thread failed" << endl;
                exit(1);
            }

            pthread_join(transmit_thread, NULL);
            if(packetSent == packets.size()){
                break;
            }
        }
        if(packetSent == packets.size()){
            break;
        }
    }

    /* Send an empty string indicating end of transmission */
    sprintf(sendBuf, "");
    while(numBytesSent != strlen(sendBuf)){
        numBytesSent = sendto(s, sendBuf, strlen(sendBuf), 0, (struct sockaddr *)&si_other, slen);
        if(numBytesSent == -1){
            diep("sendto");
        }
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
