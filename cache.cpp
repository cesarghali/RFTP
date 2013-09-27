#include "cache.h"

#include "rftp_protocol.h"


void cache_push(cache_item* pFirstItem, cache_item* pNewItem)
{
    if (pFirstItem->next_item != NULL)
    {
        pNewItem->next_item = pFirstItem->next_item;
        pFirstItem->next_item = pNewItem;
    }

    pFirstItem->next_item = pNewItem;
}

cache_item* cache_pop(cache_item* pFirstItem)
{
    if (pFirstItem->next_item == NULL)
    {
        return NULL;
    }
    else
    {
        cache_item* pResult = pFirstItem->next_item;

        pFirstItem->next_item = pFirstItem->next_item->next_item;

        return pResult;
    }
}

cache_item* cache_create_item(uint16_t sequence, uint8_t packet[1024], uint16_t length, timeval time_stamp)
{
    cache_item* cache_new_item = ((cache_item*)(malloc(sizeof(cache_item))));
    cache_new_item->sequence = sequence;
    //for (int i = 0; i < 1024; i++)
    {
        memcpy(cache_new_item->packet, packet, 1024);
    }
    cache_new_item->length = length;
    cache_new_item->time_stamp = time_stamp;
    cache_new_item->next_item = NULL;
    return cache_new_item;
}

void cache_search(cache_item* pFirstItem, uint16_t sequence, uint16_t window_size, uint8_t packet[8])
{
    int hdr_size = sizeof(rftp_hdr);

    cache_item* ptr = pFirstItem;

    while(ptr != NULL)
    {
        if (ptr->next_item == NULL)
        {
            break;
        }

        /*if (ptr->next_item->sequence == sequence & ptr->next_item->packet[0 + hdr_size] == packet[0] &
            ptr->next_item->packet[1 + hdr_size] == packet[1] & ptr->next_item->packet[2 + hdr_size] == packet[2] &
            ptr->next_item->packet[3 + hdr_size] == packet[3] & ptr->next_item->packet[4 + hdr_size] == packet[4] &
            ptr->next_item->packet[5 + hdr_size] == packet[5] & ptr->next_item->packet[6 + hdr_size] == packet[6] &
            ptr->next_item->packet[7 + hdr_size] == packet[7])*/
        if (ptr->next_item->sequence >= sequence - window_size & ptr->next_item->sequence <= sequence)
        {
            remove_cache_item(ptr);

//printf("************** Removing seq = %d\n", sequence);
        }

        ptr = ptr->next_item;
    }
}

long check_cache(cache_item* pFirstItem, int socket, sockaddr_in to_addr, uint16_t window_size)
{
    timeval current_time_stamp;
    gettimeofday(&current_time_stamp, NULL);

    cache_item* ptr = pFirstItem;

    long count = 0;

    while(ptr != NULL)
    {
        if (ptr->next_item == NULL)
        {
            break;
        }

        /*long x = timeval_diff(NULL, &(ptr->next_item->time_stamp), &current_time_stamp);
        printf("********************** %ld\n", x);*/
        if (timeval_diff(NULL, &(ptr->next_item->time_stamp), &current_time_stamp) >= 31500LL * window_size)
        {
            sendto(socket, ptr->next_item->packet, ptr->next_item->length, 0, ((sockaddr*)(&to_addr)), sizeof(to_addr));

//printf("**** tx_seq = %d\n", ((rftp_hdr*)(ptr->next_item->packet))->sequence_number);

            gettimeofday(&(ptr->next_item->time_stamp), NULL);

            count++;
        }

        ptr = ptr->next_item;
    }

    return count;
}

void remove_cache_item(cache_item* previous_item)
{
    cache_item* temp = previous_item->next_item;

    if (previous_item->next_item->next_item != NULL)
    {
        previous_item->next_item = previous_item->next_item->next_item;
    }
    else
    {
        previous_item->next_item = NULL;
    }

    free(temp);
}

long long timeval_diff(timeval* difference, timeval* start_time, timeval* end_time)
{
    timeval time_diff;
    if (difference == NULL)
    {
        difference = &time_diff;
    }

    difference->tv_sec =end_time->tv_sec -start_time->tv_sec ;
    difference->tv_usec=end_time->tv_usec-start_time->tv_usec;

    /* Using while instead of if below makes the code slightly more robust. */

    while(difference->tv_usec<0)
    {
        difference->tv_usec+=1000000;
        difference->tv_sec -=1;
    }

    return 1000000LL * difference->tv_sec + difference->tv_usec;
}
