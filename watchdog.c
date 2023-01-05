#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h> // gettimeofday()

#define SERVER_PORT 3000
#define SERVER_IP_ADDRESS "127.0.0.1"

int main()
{
    //=================================================================================================================|
    //                                                TCP SOCKET (client)                                              |
    //=================================================================================================================|

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1)
    {
        printf("Could not create socket : %d", errno);
    }

    // "sockaddr_in" is the "derived" from sockaddr structure used for IPv4 communication.
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    int rval = inet_pton(AF_INET, (const char *)SERVER_IP_ADDRESS, &serverAddress.sin_addr);
    if (rval <= 0)
    {
        printf("inet_pton() failed");
        return -1;
    }

    // Make a connection to the server with socket SendingSocket.
    if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        printf("connect() failed with error code : %d", errno);
    }


    //=================================================================================================================

    //=================================================================================================================
    // Variable to count (10 seconde)
    struct timeval start, end;
    double TimeElapse =0;
    gettimeofday(&start, 0);

    while(1){ // Every loops we gonna check if the timer not equal or taller to 10000ms == 10s
        int ContinueProg = 0;
        gettimeofday(&end,NULL);
        TimeElapse = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        if(TimeElapse >= 10000){ // if the counter of time taller of 10000ms == 10s we gonna send a TCP message
            ContinueProg = 1;
            send(sock, &ContinueProg, sizeof(int), 0);
            break;
        }
        send(sock, &ContinueProg, sizeof(int), 0);
        
    }
    // Sends some data to server
    int flagTime = 1;
    int bytesSent = send(sock, &flagTime, sizeof(int), 0);

    if (-1 == bytesSent)
    {
        printf("send() failed with error code : %d", errno);
    }
    else if (0 == bytesSent)
    {
        printf("peer has closed the TCP connection prior to send().\n");
    }

    close(sock);
    return 0;
}