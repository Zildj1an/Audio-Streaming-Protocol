#ifndef _ASP_H
#define _ASP_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <math.h>
#include <time.h>
#include <assert.h>

/* Simulate  an  unreliable  connection */
#define UNRELIABLE 1

#define COMPRESS_LOW 128
#define COMPRESS_HIGH 256
#define PORT 2000
#define PROTOCOL SOCK_STREAM
#define LOCAL_HOST "127.0.0.1"

#define MAX_BUFFER 3000
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define SENTINEL '<'
#define BLOCK_SIZE 1024

enum quality_level {EXTREME_LOW, LOW, MEDIUM, HIGH, EXTREME_HIGH};

/* Package transmitted between client and server */
#pragma pack(1)
struct package {
	int num_correct_checks;
	int num_incorrect_checks;
	int num_packages;
	int buffer_size;
	int checksum;
	int received_everything;
	uint8_t data[MAX_BUFFER];
	struct tm last_success;
	struct tm last_fail;
	enum quality_level quality;
};
#pragma pack(0)

/* Protocol API */
void ascii_art(int server);
void inline init_package(struct package *the_package);
void inline init_addr(struct sockaddr_in *local_addr);
long compute_checksum(int count, unsigned short *addr);
void errorMsg(char *msg);
void inline compress(uint8_t *data, enum quality_level quality, int size);
void downsamp_reduct(uint8_t *data, int freq, int downsap, int size);
void update_quality(struct package *the_package , struct tm *current_time);
#endif
