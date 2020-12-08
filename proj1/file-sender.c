/*
 * Author: Guilherme Saraiva
 * ID: 193717
 * Subject: Computer Networks
 *
 * Reliable Data Transfer
 * Implementation of algorithms
 *      Stop-and-wait
 *      Go-back N
 *      Selective Repeat
 * on Sender Side
 * */

#include "packet-format.h"
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>


long int findSize(char *file_name)
{
    // opening the file in read mode
    FILE* fp = fopen(file_name, "r");

    // checking if the file exist or not
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0L, SEEK_END);

    // calculating the size of the file
    long int res = ftell(fp);

    // closing the file
    fclose(fp);

    return res;
}

int main(int argc, char *argv[]) {

    if (argc != 5) {
        fprintf (stderr,"Usage: %s <file> <host> <port> <window size>\n", argv[0]);
        exit (1);
    }

    char *file_name = argv[1];
    char *host = argv[2];
    int port = atoi(argv[3]);
    int window_size = atoi(argv[4]);

    if(window_size > MAX_WINDOW_SIZE) {
        fprintf(stderr, "window size must be less than 32\n");
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(file_name, "r");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Prepare server host address.
    struct hostent *he;
    if (!(he = gethostbyname(host))) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in srv_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = *((struct in_addr *) he->h_addr),
    };

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    long int file_lenght = findSize(file_name);
    long int total_chunks = file_lenght / 1000;
    total_chunks++;

    int tries = 1;
    uint32_t seq_num=0;

    size_t data_len;
    data_pkt_t data_pkt;
    ack_pkt_t recv_pkt;


    if ( window_size == 1){
        do { // Generate segments from file, until the the end of the file.
            // Prepare data segment.

            data_pkt.seq_num = htonl(seq_num++);

            // Load data from file.
            data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);

            // Send segment.
            ssize_t sent_len =
                    sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                           (struct sockaddr *)&srv_addr, sizeof(srv_addr));
            printf("Sending segment %d.\n", ntohl(data_pkt.seq_num));
            if (sent_len != offsetof(data_pkt_t, data) + data_len) {
                fprintf(stderr, "Truncated packet.\n");
                exit(EXIT_FAILURE);
            }

            while( recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0,
                            (struct sockaddr *) &srv_addr, &(socklen_t) {sizeof(srv_addr)}) < 0 ){

                if(port != htons(srv_addr.sin_port) && ((struct in_addr *) he->h_addr)->s_addr != srv_addr.sin_addr.s_addr){
                    fprintf(stderr, "ACK received from wrong address.\n");
                    exit(EXIT_FAILURE);
                }

                printf("Timed out.\n");

                if(tries == MAX_RETRIES) {
                    fprintf(stderr, "Could not receive server acknowledge\n");
                    exit(EXIT_FAILURE);
                }

                // Resend segment.
                sent_len =
                        sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                               (struct sockaddr *) &srv_addr, sizeof(srv_addr));
                printf("Resending segment %d.\n", ntohl(data_pkt.seq_num));

                if (sent_len != offsetof(data_pkt_t, data) + data_len) {
                    fprintf(stderr, "Truncated packet.\n");
                    exit(EXIT_FAILURE);
                }

                tries++;
            }

            if(port != htons(srv_addr.sin_port) && ((struct in_addr *) he->h_addr)->s_addr != srv_addr.sin_addr.s_addr){
                fprintf(stderr, "ACK received from wrong address.\n");
                exit(EXIT_FAILURE);
            }

            printf("Received ACK %d / %08" PRIu32 ".\n", ntohl(recv_pkt.seq_num), ntohl(recv_pkt.selective_acks));

        } while (!(feof(file) && data_len < sizeof(data_pkt.data)));
    }
    else if (window_size > 1 ) { // GO BACK-N and Selective Repeat

        // Sequence number of the last packet sent (rcvbase)
        int last_sent = 0;

        // Sequence number of the last acked packet
        int last_acked = 0;

        int sr_flag = -1;
        uint32_t prev_sltvno = 0;
        int ackno = 0;

        int f_recv_size;

        while (last_acked < total_chunks) {

            while ( last_sent < last_acked + window_size && last_sent < total_chunks ) {

                data_pkt.seq_num = htonl(last_sent);

                fseek(file, (last_sent) * 1000, SEEK_SET);

                // Load data from file.
                data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);

                // Send segment.
                ssize_t sent_len =
                        sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                               (struct sockaddr *) &srv_addr, sizeof(srv_addr));
                printf("Sending segment %d.\n", ntohl(data_pkt.seq_num));

                if (sent_len != offsetof(data_pkt_t, data) + data_len) {
                    fprintf(stderr, "Truncated packet.\n");
                    exit(EXIT_FAILURE);
                }

                last_sent++;
                tries = 0;
            }

            f_recv_size = recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0,
                                       (struct sockaddr *) &srv_addr, &(socklen_t) {sizeof(srv_addr)});

            if(port != htons(srv_addr.sin_port) && ((struct in_addr *) he->h_addr)->s_addr != srv_addr.sin_addr.s_addr){
                fprintf(stderr, "ACK received from wrong address.\n");
                exit(EXIT_FAILURE);
            }

            if( f_recv_size >= 0 ){

                ackno = ntohl(recv_pkt.seq_num);
                int selectiveno = ntohl(recv_pkt.selective_acks);

                printf("Received ACK %d / %08" PRIu32 ".\n", ackno, selectiveno);

                if(last_acked + 1 == ackno) {
                    last_acked++;

                } else if ( ackno < last_acked + 1 && selectiveno > prev_sltvno) {
                    prev_sltvno = selectiveno;

                } else if (prev_sltvno == -2 ){
                    last_acked = last_sent;
                }

                if( selectiveno != 0)
                    sr_flag = 1;

            } else {
                printf("Timed out.\n");

                if(tries == MAX_RETRIES) {
                    fprintf(stderr, "Could not receive server acknowledge\n");
                    exit(EXIT_FAILURE);
                }
                tries++;

                if (sr_flag == 1) {

                    int seg_to_send = 0 ;
                    uint32_t new_selective = prev_sltvno<<1;

                    while (new_selective > 0) {

                        // If current bit is 0
                        if (!(new_selective & 1)) {

                            data_pkt.seq_num = htonl(seg_to_send + last_acked);

                            fseek(file, (seg_to_send + last_acked) * 1000, SEEK_SET);

                            // Load data from file.
                            data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);

                            // Send segment.
                            ssize_t sent_len =
                                    sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                                           (struct sockaddr *) &srv_addr, sizeof(srv_addr));
                            printf("Resending segment %d.\n", ntohl(data_pkt.seq_num));
                            if (sent_len != offsetof(data_pkt_t, data) + data_len) {
                                fprintf(stderr, "Truncated packet.\n");
                                exit(EXIT_FAILURE);
                            }
                        }
                        seg_to_send++;
                        new_selective = new_selective >> 1;
                    }

                    last_sent = last_sent > seg_to_send + ackno? (seg_to_send + ackno) : last_sent; // in case seg was sent out of window
                    prev_sltvno = -2;
                    sr_flag = -1;
                } else {

                    for (int temp = last_acked; temp < last_sent; temp++) {

                        data_pkt.seq_num = htonl(temp);

                        fseek(file, (temp) * 1000, SEEK_SET);

                        // Load data from file.
                        data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);

                        // Send segment.
                        ssize_t sent_len =
                                sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                                       (struct sockaddr *) &srv_addr, sizeof(srv_addr));
                        printf("Resending segment %d.\n", ntohl(data_pkt.seq_num));
                        if (sent_len != offsetof(data_pkt_t, data) + data_len) {
                            fprintf(stderr, "Truncated packet.\n");
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            }
        }
    }

    // Clean up and exit.
    close(sockfd);
    fclose(file);

    exit(EXIT_SUCCESS);
}

