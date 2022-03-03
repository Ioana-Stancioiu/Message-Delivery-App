#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <math.h>
#include <iostream>
#include <vector>
using namespace std;

#include "helpers.h"

int main(int argc, char const *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc < 2, "not enough arguments");

    // udp socket file descriptor
    int udpfd;
    // listen socket
    int sockfd;
    // tcp socket;
    int newsockfd;

    fd_set read_fds;
    fd_set tmp_fds;
    int fdmax;

    struct sockaddr_in udpcli_addr, tcpcli_addr, serv_addr;

    // packet from udp client
    udp_packet payload_udp;

    FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

    // open udp socket
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udpfd < 0, "failed opening udp socket");

    // server port
    int port = atoi(argv[1]);
    DIE(port == 0, "atoi");

    // create struct sockaddr_in for udp
    udpcli_addr.sin_family = AF_INET;
    udpcli_addr.sin_port = htons(port);
    udpcli_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t udpcli_addr_size = sizeof(udpcli_addr);

    // bind to udp socket
    int ret = bind(udpfd, (struct sockaddr*)&udpcli_addr, sizeof(udpcli_addr));
    DIE(ret < 0, "bind");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

    // deactivate nagle
    int set_nodelay = 1;
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
                    (char*)&set_nodelay, sizeof(int));
    DIE(ret < 0, "tcp_nodelay failed");

    // create struct sockaddr_in for tcp socket
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

    // bind to server socket
	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

    // listen for tcp connections
	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

    // add sockets to read set
    FD_SET(sockfd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(udpfd, &read_fds);

    fdmax = sockfd;

    // clients connected to server
    vector<client*> tcp_clients;
    // store socket to client link
    unordered_map<int, client*> client_socket;
    // store list of clients subscribed to each topic
    unordered_map<string, list<client*>> topics;
    // store socket status
    vector<bool> active_sockets;
    active_sockets.assign(10, false);

    // SF messages to be deallocated
    vector<msg*> SFallocated_messages;

    // message for tcp client
    msg message;

    while (1) {
        tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == sockfd) {
                    // received connection request from client

                    socklen_t tcp_len = sizeof(tcpcli_addr);
                    newsockfd = accept(sockfd, (struct sockaddr *)&tcpcli_addr, &tcp_len);
					DIE(newsockfd < 0, "accept");

                    // deactivate nagle
                    set_nodelay = 1;
                    ret = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY,
                                    (char*)&set_nodelay, sizeof(int));
                    DIE(ret < 0, "tcp_nodelay failed");

                    FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) {
						fdmax = newsockfd;
                    }

                    if (static_cast<int>(active_sockets.capacity()) >= fdmax) {
                        active_sockets.resize(fdmax * 2);
                    }
                } else if (i == udpfd) {
                    // received message from udp client

                    memset(&payload_udp, 0, sizeof(udp_packet));
                    ret = recvfrom(udpfd, &payload_udp, sizeof(payload_udp), 0,
                                   (struct sockaddr*)&udpcli_addr, &udpcli_addr_size);
                    DIE(ret < 0, "recvfrom");

                    auto cli_list = topics.find(payload_udp.topic);
                    if (cli_list == topics.end()) {
                        // add new topic to existent topics
                        list<client*> clients;
                        topics[payload_udp.topic] = clients;
                    } else {
                        // create new message for tcp client
                        msg* message = new msg;
                        message->len = ret + 2 * sizeof(int);
                        message->msg_type = UDP_PACKET;

                        // copy info from udp message
                        tcp_packet payload_tcp;
                        memset(&payload_tcp, 0, sizeof(payload_tcp));
                        payload_tcp.sender_address = udpcli_addr.sin_addr.s_addr;
                        payload_tcp.sender_port = udpcli_addr.sin_port;
                        memcpy(&payload_tcp.packet, &payload_udp, ret);

                        memcpy(message->payload, &payload_tcp, message->len);

                        // flag to check if message can be send
                        bool message_pending = false;

                        // send message to all subscribers
                        for (auto cli = cli_list->second.begin(); cli != cli_list->second.end(); cli++) {
                            // if client is connected send message
                            if (active_sockets[(*cli)->socket]) {
                                ret = send((*cli)->socket, message, message->len + MSG_HEADER_SIZE, 0);
                                DIE(ret < 0, "send message udp");
                            } else {
                                auto topic_cli = (*cli)->topics.find(payload_udp.topic);
                                // store message if SF is set
                                if (topic_cli->second == 1) {
                                    (*cli)->SFmessages.push(message);
                                    message_pending = true;
                                }
                            }
                        }

                        if (!message_pending) {
                            // delete message if send
                            delete message;
                        } else {
                            // store to delete in the future
                            SFallocated_messages.push_back(message);
                        }
                    }

                } else if (i == STDIN_FILENO) {
                    char buffer[10];
                    memset(buffer, 0, sizeof(buffer));
                    fgets(buffer, sizeof(buffer) - 1, stdin);

                    // received exit command
                    if (strncmp(buffer, "exit", 4) == 0) {
                        // close all connections
                        for (int j = sockfd + 1; j <= fdmax; j++) {
                            if (active_sockets[j]) {
                                // tell client connection will be closed
                                memset(&message, 0, sizeof(msg));
                                message.msg_type = EXIT;
                                ret = send(j, &message, MSG_HEADER_SIZE, 0);
                                DIE(ret < 0, "send");
                            }
                            close(j);
                        }

                        // deallocate clients
                        for (size_t j = 0; j < tcp_clients.size(); j++) {
                            delete tcp_clients[j];
                        }

                        // deallocate messages
                        for (size_t j = 0; j < SFallocated_messages.size(); j++) {
                            delete SFallocated_messages[j];
                        }

				        close(sockfd);
                        return 0;
			        }
                } else {
                    // received message from tcp client
                    memset(&message, 0, sizeof(message));
                    ret = recv(i, &message, sizeof(message), 0);
                    DIE(ret < 0, "recv");

                    if (ret == 0) {
                        // client disconnected
                        auto cli = client_socket.find(i);
                        printf("Client %s disconnected.\n", cli->second->id);
                        client_socket.erase(cli);

                        // mark socket as innactive and close it
                        active_sockets[i] = false;
                        close(i);
                        FD_CLR(i, &read_fds);
                    } else {
                        if (message.len != static_cast<int> (strlen(message.payload))) {
                            fprintf(stderr, "Error receiving message from client\n");
                        }

                        // receive client id
                        if (message.msg_type == NEW_CONNECTION) {
                            size_t j;
                            for (j = 0; j < tcp_clients.size(); j++) {
                                // client id already exists
                                if (strcmp(tcp_clients[j]->id, message.payload) == 0) {
                                    // close new socket and disconnect client
                                    auto client = client_socket.find(tcp_clients[j]->socket);
                                    if (client != client_socket.end() &&
                                        strcmp(client->second->id, message.payload) == 0) {

                                        // send exit command
                                        memset(&message, 0, sizeof(msg));
                                        message.msg_type = EXIT;
                                        ret = send(i, &message, MSG_HEADER_SIZE, 0);
                                        DIE(ret < 0, "send");

                                        printf("Client %s already connected.\n", tcp_clients[j]->id);
                                        active_sockets[i] = false;
                                        close(i);
                                        FD_CLR(i, &read_fds);
                                    } else {
                                        // client back online
                                        printf("New client %s connected from %s:%d.\n", message.payload,
                                                                                inet_ntoa(tcpcli_addr.sin_addr),
                                                                                ntohs(tcpcli_addr.sin_port));
                                        tcp_clients[j]->socket = i;
                                        client_socket[i] = tcp_clients[j];
                                        // mark the socket as active
                                        active_sockets[newsockfd] = true;

                                        // send SF messages
                                        while (!tcp_clients[j]->SFmessages.empty()) {
                                            msg* SFmessage = (msg*)tcp_clients[j]->SFmessages.front();
                                            ret = send(i, SFmessage, SFmessage->len + MSG_HEADER_SIZE, 0);
                                            DIE(ret < 0, "recv");
                                            tcp_clients[j]->SFmessages.pop();
                                        }
                                    }
                                    break;
                                }
                            }

                            // new client connected
                            if (j >= tcp_clients.size()) {
                                printf("New client %s connected from %s:%d.\n", message.payload,
                                                                                inet_ntoa(tcpcli_addr.sin_addr),
                                                                                ntohs(tcpcli_addr.sin_port));

                                client* newclient = new client;
                                strcpy(newclient->id, message.payload);
                                newclient->socket = i;
                                // add client to client list
                                tcp_clients.push_back(newclient);
                                // create socket-client link
                                client_socket[i] = newclient;
                                // mark the socket as active
                                active_sockets[newsockfd] = true;
                            }
                        }

                        // received subscribe commnad
                        if (message.msg_type == SUBSCRIBE) {
                            char topic[50];
                            int SF;
                            ret = sscanf(message.payload, "%s %d", topic, &SF);
                            DIE(ret <= 0, "sscanf");

                            auto cli = client_socket.find(i);
                            auto cli_topic = cli->second->topics.find(topic);
                            if (cli_topic == cli->second->topics.end()) {
                                // add new subscription to client list of subscriptions
                                cli->second->topics[topic] = SF;

                                // add new subcribtion to server list of topics
                                auto iter = topics.find(topic);
                                if (iter == topics.end()) {
                                    list<client*> client_list;
                                    client_list.push_back(cli->second);
                                    topics[topic] = client_list;
                                } else {
                                    iter->second.push_back(cli->second);
                                }
                            } else {
                                // modify SF flag
                                cli_topic->second = SF;
                            }
                        }

                        // received unsubscribe command
                        if (message.msg_type == UNSUBSCRIBE) {
                            auto cli_list = topics.find(message.payload);
                            if (cli_list == topics.end()) {
                                fprintf(stderr, "Can't unsubscribe from nonexitant topic\n");
                            } else {
                                // find and erase client
                                for (auto cli = cli_list->second.begin(); cli != cli_list->second.end(); cli++) {
                                    if ((*cli)->socket == i) {
                                        (*cli)->topics.erase(message.payload);
                                        cli_list->second.erase(cli);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
