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
 * on Receiver Side
 * */


#include "packet-format.h"
#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <inttypes.h>

int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file> <port> <window size>\n", argv[0]);
        exit(1);
    }

    char *file_name = argv[1];
    int port = atoi(argv[2]);
    int window_size = atoi(argv[3]);

    if(window_size > MAX_WINDOW_SIZE) {
        fprintf(stderr, "window size must be less than 32\n");
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(file_name, "w");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Prepare server socket.
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Allow address reuse so we can rebind to the same port,
    // after restarting the server.
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) <
        0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in srv_addr = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = htonl(INADDR_ANY),
            .sin_port = htons(port),
    };

    if (bind(sockfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr))) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Receiving on port: %d\n", port);

    ssize_t len;
    int seq_num = 0;
    int end = 0;

    int client_port = -1;
    int client_id = -1;

    if (window_size == 1) { // STOP-AND-WAIT    &&      GO-BACK-N
        do { // Iterate over segments, until last the segment is detected.
            // Receive segment.
            struct sockaddr_in src_addr;
            data_pkt_t data_pkt;
            ack_pkt_t send_pkt;

            len =
                    recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                             (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});

            if( client_port == -1 && client_id == -1){
                client_port = src_addr.sin_port;
                client_id = src_addr.sin_addr.s_addr;
            } else if(client_port != src_addr.sin_port || client_id != src_addr.sin_addr.s_addr){
                fprintf(stderr, "ACK received from wrong address.\n");
                exit(EXIT_FAILURE);
            }

            if (len >= 0){
                printf("Received segment %d.\n", ntohl(data_pkt.seq_num));

                if (ntohl(data_pkt.seq_num) == seq_num) {
                    // Write data to file.
                    fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);
                    seq_num++;

                    if(len != sizeof(data_pkt_t)){
                        end = 1;
                    }

                } else {
                    printf("Segment out of window.\n");
                }
            }

            send_pkt.seq_num = htonl(seq_num);
            send_pkt.selective_acks = htonl(0b00);

            ssize_t sent_len =
                    sendto(sockfd, &send_pkt, sizeof(send_pkt), 0,
                           (struct sockaddr *) &src_addr, sizeof(src_addr));
            if (sent_len > 0) {
                printf("Sending ACK %d / %08" PRIu32 ".\n", ntohl(send_pkt.seq_num), send_pkt.selective_acks);
            }

            if(end == 1) break;

        } while (true);

    } else if ( window_size > 1 ) { // Selective Repeat

        uint32_t sltv_acks = 0b00;

        do {
            struct sockaddr_in src_addr;
            data_pkt_t data_pkt;
            ack_pkt_t send_pkt;

            len =
                    recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                             (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});

            if( client_port == -1 && client_id == -1){
                client_port = src_addr.sin_port;
                client_id = src_addr.sin_addr.s_addr;
            } else if(client_port != src_addr.sin_port || client_id != src_addr.sin_addr.s_addr){
                fprintf(stderr, "ACK received from wrong address.\n");
                exit(EXIT_FAILURE);
            }

            if (len >= 0){
                printf("Received segment %d.\n", ntohl(data_pkt.seq_num));

                int rcv_seq = ntohl(data_pkt.seq_num);

                // Make sure that it writes on exact place
                fseek(file, ( rcv_seq * 1000 ), SEEK_SET);

                // Write data to file.
                fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);

                if (rcv_seq == seq_num) {

                    if (sltv_acks != 0b00) {

                        uint32_t temp = sltv_acks;
                        int reset = -1; // everything was sent?

                        while (temp > 0 ) {

                            seq_num++;
                            sltv_acks >>= 1; // shift right window


                            if(!(temp & 1)) {
                                reset = 1; // there's some acks left to send
                                break;
                            }
                            temp = temp >> 1;
                        }

                        if(reset != 1){ // if everything in the window was sent reset it
                            seq_num++;
                            sltv_acks = 0b00;
                        }

                        if(end == 2){
                            end = 1;
                        }
                    } else {
                        seq_num++;
                    }

                    if(len != sizeof(data_pkt_t)){ // last segment was received, shut down
                        end = 1;
                    }

                } else if( rcv_seq > seq_num && rcv_seq < seq_num + window_size ) {
                    sltv_acks |= 1 << (rcv_seq - seq_num - 1);

                    if(len != sizeof(data_pkt_t)){ // last segment was received out of order
                        end = 2;
                    }

                } else {
                    printf("Segment out of window.\n");
                }
            }

            send_pkt.selective_acks = htonl(sltv_acks);
            send_pkt.seq_num = htonl(seq_num);

            ssize_t sent_len =
                    sendto(sockfd, &send_pkt, sizeof(send_pkt), 0,
                           (struct sockaddr *) &src_addr, sizeof(src_addr));
            if (sent_len > 0) {
                printf("Sending ACK %d / %08" PRIu32 ".\n", ntohl(send_pkt.seq_num), ntohl(send_pkt.selective_acks));
            }

            if(end == 1) break;

        } while (true);

    }

    // Clean up and exit.
    close(sockfd);
    fclose(file);

    exit(EXIT_SUCCESS);
}

