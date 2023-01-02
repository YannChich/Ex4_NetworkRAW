#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>
// IPv4 header len without options
#define IP4_HDRLEN 20

// ICMP header len for echo req
#define ICMP_HDRLEN 8

// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);
// Ip machine
#define SOURCE_IP "127.0.0.1"

int main(int argc, char *argv[])
{
    int counter = 1;          // counter for the sequence number
    char *hostname = argv[1]; // The destination adress

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
    //==================================================================================================================
    while (1)
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

        // ====================================================================================================================|

        // Variable to mesuring the time
        struct timeval start, end;
        gettimeofday(&start, 0);

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
        int bytes_received = -1;
        int seq = 0;
        int ttl = 0;
        while ((bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len)))
        {
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

                break;
            }
        }
        gettimeofday(&end, 0);
        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%f ms\n", bytes_received, argv[1], seq, ttl, milliseconds);
        bzero(packet, IP_MAXPACKET);
        counter++;
        printf("------------------------------------------------------------------------\n");
        sleep(2);
    }
    close(sock);
    return 0;
}

// Compute checksum (RFC 1071).
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