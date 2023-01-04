#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h> // gettimeofday()
#include <netdb.h>
#include <sys/ioctl.h>
#include <fcntl.h>

// IPv4 header len without options
#define IP4_HDRLEN 20

// ICMP header len for echo req
#define ICMP_HDRLEN 8

#define SERVER_PORT 3000
// run 2 programs using fork + exec
// command: make clean && make all && ./partb

unsigned short calculate_checksum(unsigned short *paddress, int len);

int main(int argc, char *argv[])
{
    int counter = 1;          // counter for the sequence number
    char *hostname = argv[1]; // The destination adress

    //=================================================================================================================|
    //                                                TCP SOCKET (server)                                              |
    //=================================================================================================================|
    // Open the listening (server) socket
    int listeningSocket = -1;

    if ((listeningSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Could not create listening socket : %d", errno);
    }

    // Reuse the address if the server socket on was closed
    // and remains for 45 seconds in TIME-WAIT state till the final removal.
    //
    int enableReuse = 1;
    if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(int)) < 0)
    {
        printf("setsockopt() failed with error code : %d", errno);
    }

    // "sockaddr_in" is the "derived" from sockaddr structure
    // used for IPv4 communication. For IPv6, use sockaddr_in6
    //
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT); // network order

    // Bind the socket to the port with any IP at this port
    if (bind(listeningSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        printf("Bind failed with error code : %d", errno);
        // TODO: close the socket
        return -1;
    }

    printf("Bind() success\n");

    // Make the socket listening; actually mother of all client sockets.
    if (listen(listeningSocket, 500) == -1) // 500 is a Maximum size of queue connection requests
                                            // number of concurrent connections
    {
        printf("listen() failed with error code : %d", errno);
        // TODO: close the socket
        return -1;
    }

    // Accept and incoming connection
    printf("Waiting for incoming TCP-connections...\n");

    struct sockaddr_in clientAddress; //
    socklen_t clientAddressLen = sizeof(clientAddress);

    //=================================================================================================================

    //=================================================================================================================|
    //                                                RAW SOCKET                                                       |
    //=================================================================================================================|

    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;

    // The port is irrelant for Networking and therefore was zeroed.
    // dest_in.sin_addr.s_addr = iphdr.ip_dst.s_addr;
    dest_in.sin_addr.s_addr = inet_addr(hostname);
    // inet_pton(AF_INET, DESTINATION_IP, &(dest_in.sin_addr.s_addr));

    // Create raw socket for IP-RAW (make IP-header by yourself)
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }

    //============================================Non-Blocking Socket===================================================
    int flags = fcntl(sock, F_GETFL, 0);
    flags = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    //==================================================================================================================
    while (1)
    {
        char *args[2];
        // compiled watchdog.c by makefile
        args[0] = "./watchdog";
        args[1] = NULL;
        int pid = fork();
        if (pid == 0)
        {
            execvp(args[0], args);
        }
        else
        {
            struct icmp icmphdr; // ICMP-header
            char data[IP_MAXPACKET] = "This is the ping.\n";
            int datalen = strlen(data) + 1;

            //=================================================================================================================|
            //                                  ICMP header                                                                    |
            //=================================================================================================================|

            // Message Type (8 bits): ICMP_ECHO_REQUEST
            icmphdr.icmp_type = ICMP_ECHO;
            // Message Code (8 bits): echo request
            icmphdr.icmp_code = 0;
            // Identifier (16 bits): some number to trace the response.
            // It will be copied to the response packet and used to map response to the request sent earlier.
            // Thus, it serves as a Transaction-ID when we need to make "ping"
            icmphdr.icmp_id = 18;
            // Sequence Number (16 bits): starts at 0
            icmphdr.icmp_seq = counter;
            // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
            icmphdr.icmp_cksum = 0;
            // Combine the packet
            char packet[IP_MAXPACKET];
            // Next, ICMP header
            memcpy((packet), &icmphdr, ICMP_HDRLEN);
            // After ICMP header, add the ICMP data.
            memcpy(packet + ICMP_HDRLEN, data, datalen);
            // Calculate the ICMP header checksum
            icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
            memcpy((packet), &icmphdr, ICMP_HDRLEN);

            // ================================================================================================================|
            // Variable to mesuring the time
            struct timeval start, end;
            gettimeofday(&start, 0);
            //=======================================TCP Accept ================================================================
            memset(&clientAddress, 0, sizeof(clientAddress));
            clientAddressLen = sizeof(clientAddress);
            int clientSocket = accept(listeningSocket, (struct sockaddr *)&clientAddress, &clientAddressLen);
            if (clientSocket == -1)
            {
                printf("listen failed with error code : %d", errno);
                // TODO: close the sockets
                return -1;
            }
            //===================================================================================================================
            // Send the packet using sendto() for sending datagrams.
            int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));
            if (bytes_sent == -1)
            {
                fprintf(stderr, "sendto() failed with error: %d", errno);
                return -1;
            }
            if (counter == 1)
            {
                printf("PING %s (%s): %d data bytes\n", argv[1], argv[1], bytes_sent);
            }
            // Get the ping response
            socklen_t len = sizeof(dest_in);
            int bytes_received = 0;
            int seq = 0;
            int ttl = 0;
            ioctl(sock, FIONREAD, &bytes_received);
            while (bytes_received <= 0)
            {   
                int Timercount = 0;
                // Trying to receive an ICMP ECHO REPLAY packet.
                bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len);

                if(bytes_received == -1){
                    recv(clientSocket, &Timercount, sizeof(int),0);     
                }
                if(Timercount == 1){
                        break;
                    }
            }

            if (bytes_received > 0)
            {
                // Check the IP header
                struct iphdr *iphdr = (struct iphdr *)packet;
                struct icmphdr *icmphdr = (struct icmphdr *)(packet + (iphdr->ihl * 4));
                // printf("%d bytes from %s\n", bytes_received, inet_ntoa(dest_in.sin_addr));
                //  icmphdr->type
                seq = icmphdr->un.echo.sequence;
                ttl = iphdr->ttl;
                // printf("%d bytes from : data length : %d , icmp header : %d , ip header : %d, Seq number: %d \n", bytes_received, datalen, ICMP_HDRLEN, IP4_HDRLEN, icmphdr->un.echo.sequence);
                gettimeofday(&end, 0);
                float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
                printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%f ms\n", bytes_received, argv[1], seq, ttl, milliseconds);
                bzero(packet, IP_MAXPACKET);
                counter++;
                kill(pid, SIGKILL);
                printf("------------------------------------------------------------------------\n");
            }
            //=================================================================================================================|
            //                                                TCP SOCKET : accept                                              |
            //=================================================================================================================|

            int flag = 0;

            // try to receive some data, this is a blocking call
            if (recv(clientSocket, &flag, sizeof(int), 0) == -1)
            {
                printf("recvfrom() failed with error code  : %d", errno);
                return -1;
            }
            if (flag == 1)
            {
                printf("server %s cannot be reached.\n", argv[1]);
                close(listeningSocket);
                exit(1);
            }

            sleep(2);
        }
    }
    return 0;
}

unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}
