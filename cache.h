#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

struct cache_item
{
    uint16_t sequence;
    uint8_t packet[1024];
    uint16_t length;
    timeval time_stamp;
    struct cache_item* next_item;
} __attribute__ ((packed)) ;


void cache_push(cache_item* pFirstItem, cache_item* pNewItem);
cache_item* cache_pop(cache_item* pFirstItem);
cache_item* cache_create_item(uint16_t sequence, uint8_t data[1024], uint16_t length, timeval time_stamp);
void cache_search(cache_item* pFirstItem, uint16_t sequence, uint16_t window_size, uint8_t data[8]);
long check_cache(cache_item* pFirstItem, int socket, sockaddr_in to_addr, uint16_t window_size);
void remove_cache_item(cache_item* previous_item);
long long timeval_diff(timeval* difference, timeval* start_time, timeval* end_time);


#endif	//CACHE_H
