#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>

#include "cache.h"
#include "crc32.h"


void rftp_init();
void print_default_settings();
void* sent_packets_time_stamp_increase(void*);
void listen_to_default_port();
void* listen_to_receive(void*);
void stop_listening_to_default_port();
void change_window_size();
void change_logging_status();
void change_sequence_number_bits();
void send_message();
void send_file();
void change_listening_default_port();
void change_receive_buffer();
void work_in_check_sum_mode();
void work_in_crc_mode();
uint16_t calc_cksum(uint8_t*, int);
void print_timestamp();


#define ACK_FLAG    0x01
#define EOF_FLAG    0x04
#define NAM_FLAG    0x10
#define TXT_FLAG    0x20
#define FIL_FLAG    0x40


struct rftp_hdr
{
    uint16_t   sequence_number;
    uint16_t   ack_number;
    uint16_t   window_size;
    uint16_t   data_length;
    uint16_t   check_sum;
    uint32_t   crc;
    uint8_t    flag;               // 0FTN0E0A
    uint8_t    padding_1;
} __attribute__ ((packed)) ;

struct retransmit_parameters
{
    int        socket;
    sockaddr_in   to_addr;
} __attribute__ ((packed)) ;
