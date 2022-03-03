#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "helpers.h"

int main(int argc, char const *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(argc < 4, "not enough arguments");

    // client id
    char id[strlen(argv[1]) + 1];
    strcpy(id, argv[1]);

    // client file descriptor
    int sockfd;
    struct sockaddr_in serv_addr;

    fd_set read_fds;
    fd_set tmp_fds;

    int fdmax;

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "can't open socket");

    // deactivate nagle
    int set_nodelay = 1;
    int ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&set_nodelay, sizeof(int));
    DIE(ret < 0, "tcp_nodelay failed");

    // add client and stdin sockets to reading set
    FD_SET(sockfd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    // server port
    int port = atoi(argv[3]);
    DIE(port == 0, "atoi");

    // create struct sockaddr_in
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "failed at inet_aton");

    // connect to server
    ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "couldn't connect to sever");

    // send server message with id
    msg message;
    memset(&message, 0, sizeof(msg));
    message.len = strlen(id);
    message.msg_type = NEW_CONNECTION;
    strcpy(message.payload, id);

    ret = send(sockfd, &message, message.len + MSG_HEADER_SIZE, 0);
    DIE(ret < 0, "send");

    fdmax = sockfd;

    while (1) {
        tmp_fds = read_fds;
        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "failed on select");

        // read from stdin
        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            char buffer[STDIN_BUFLEN];
            memset(buffer, 0, STDIN_BUFLEN);

            fgets(buffer, STDIN_BUFLEN - 1, stdin);

            // close client
            if (strncmp(buffer, "exit", 4) == 0) {
                break;
            }

            char command[20];
            ret = sscanf(buffer, "%s", command);
            DIE(ret <= 0, "sscanf command");

            memset(&message, 0, sizeof(msg));

            // send server subscribe message
            if (strcmp(command, "subscribe") == 0) {
                strcpy(message.payload, buffer + strlen(command) + 1);
                message.payload[strlen(message.payload)] = '\0';
                message.len = strlen(message.payload);
                message.msg_type = SUBSCRIBE;

                ret = send(sockfd, &message, message.len + MSG_HEADER_SIZE, 0);
                DIE(ret < 0, "send");

                printf("Subscribed to topic.\n");
            }

            // send server unsubcribe message
            if (strcmp(command, "unsubscribe") == 0) {
                ret = sscanf(buffer, "%*s %s", message.payload);
                DIE(ret <= 0, "sscanf topic");
                message.len = strlen(message.payload);
                message.msg_type = UNSUBSCRIBE;

                ret = send(sockfd, &message, message.len + MSG_HEADER_SIZE, 0);
                DIE(ret < 0, "send");
                printf("Unsubscribed from topic.\n");
            }
        } else if(FD_ISSET(sockfd, &tmp_fds)) {
            // receive message from server
            memset(&message, 0, sizeof(msg));

            // receive header
            ret = recv(sockfd, &message, MSG_HEADER_SIZE, 0);
            DIE(ret < 0, "recv");

            // close client
            if (message.msg_type == EXIT) {
                break;
            }

            if (message.msg_type == UDP_PACKET) {
                ret = recv(sockfd, message.payload, message.len, 0);
                DIE(ret < 0, "recv");

                // received message too small
                if (ret < message.len) {
                    ret = recv(sockfd, message.payload + ret, message.len - ret, 0);
                    DIE(ret < 0, "recv");
                }

                message.payload[message.len] = '\0';

                tcp_packet* payload = (tcp_packet*) message.payload;

                struct in_addr sender_address;
                sender_address.s_addr = payload->sender_address;

                printf("%s:%d - %s - ", inet_ntoa(sender_address), ntohs(payload->sender_port),
                                        payload->packet.topic);

                int data_type = payload->packet.data_type;

                if (data_type == 0) {
                    printf("%s - ", "INT");
                    printf("%d\n", byte_string_to_int(payload->packet.payload));
                } else if (data_type == 1) {
                    printf("%s - ","SHORT_REAL");
                    printf("%.2f\n",byte_string_to_short_real(payload->packet.payload));
                } else if (data_type == 2) {
                    printf("%s - ", "FLOAT");
                    uint8_t decimals;
                    double number;
                    number = byte_string_to_float(payload->packet.payload, &decimals);
                    printf("%.*f\n", decimals, number);
                } else {
                    printf("%s - ", "STRING");
                    printf("%s\n", payload->packet.payload);
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
