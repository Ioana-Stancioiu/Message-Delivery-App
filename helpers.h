#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <unordered_map>
#include <list>
#include <iterator>
#include <queue>
using namespace std;

/*
* Macro de verificare a erorilor
* Exemplu:
* 		int fd = open (file_name , O_RDONLY);
* 		DIE( fd == -1, "open failed");
*/

#define DIE(assertion, call_description)				\
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(EXIT_FAILURE);				\
		}							\
	} while(0)

#define STDIN_BUFLEN 100
#define MAX_CLIENTS 10
#define MAX_BUF_SIZE 1559
#define MSG_HEADER_SIZE 5

// message types
#define SUBSCRIBE 0
#define UNSUBSCRIBE 1
#define UDP_PACKET 2
#define EXIT 3
#define NEW_CONNECTION 4

typedef struct __attribute__((packed)) {
	char topic[50];
	uint8_t data_type;
	char payload[1500];
}  udp_packet;

typedef struct __attribute__((packed)) {
	uint32_t sender_address;
	int sender_port;
	udp_packet packet;
} tcp_packet;

typedef struct __attribute__((packed)) {
	int len;
	char msg_type;
	char payload[MAX_BUF_SIZE];
} msg;

typedef struct {
	char id[16];
	int socket;
	unordered_map<string, int> topics;
	queue<msg*> SFmessages;
} client;


/**
 * @return int from an array of bytes
 * in network byte order
 */
int32_t byte_string_to_int(char* payload) {
    uint32_t number = ((uint8_t)payload[1] << 24) +
                      ((uint8_t)payload[2] << 16) +
                      ((uint8_t)payload[3] << 8) +
                      (uint8_t)payload[4];
    if (payload[0]) {
        return (0 - number);
    }

    return number;
}

/**
 * @return short_real from an array of bytes
 * in network byte order
 */
double byte_string_to_short_real(char* payload) {
    uint16_t number =  ((uint8_t)payload[0] << 8) +
                       (uint8_t)payload[1];

    return (double)number / 100;
}

/**
 * @param decimals number of floating point decimal places
 * @return float from an array of bytes
 * in newtwork byte order
 */
double byte_string_to_float(char* payload, uint8_t* decimals) {
	int32_t number = byte_string_to_int(payload);
	*decimals = payload[5];
	return (double) number / pow(10, *decimals);
}

#endif
