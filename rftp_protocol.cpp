#include "rftp_protocol.h"

int sockfd_listen;
pthread_t listen_thread;
int stop_listening;

int listening_default_port;
int window_size;

char* file_name;
char* file_name_part;
uint16_t receive_sequence_number;
uint16_t ack_number;

timeval start_time;

cache_item* sent_packets;
pthread_t sent_packets_time_stamp_increase_thread;
pthread_t retransmit_packets_thread;
int stop_retransmit;
int start_retransmitting;

long num_received_packets;
long num_retransmitted_packets;
long num_duplicate_packets;
long num_corrupted_packets;
int seq_num_bits;

int last_rx_seq_num;

uint8_t chsum_crc_mode;
int show_log;

int sock_buf_size;


void rftp_init()
{
    listening_default_port = 1803;
    window_size = 1;

    sockfd_listen = 0;
    stop_listening = 1;

    file_name = 0;

    srand(time(0));
    receive_sequence_number = rand();

    timeval nothing;
    uint8_t data[8] = { 0 };
    sent_packets = cache_create_item(0, data, 8, nothing);

    pthread_create(&sent_packets_time_stamp_increase_thread, NULL, sent_packets_time_stamp_increase, NULL);

    num_received_packets = 0;
    num_retransmitted_packets = 0;
    num_duplicate_packets = 0;
    num_corrupted_packets = 0;
    seq_num_bits = 16;

    last_rx_seq_num = 0; //-1;

    chsum_crc_mode = 1;
    show_log = 0;

    sock_buf_size = 0;
}

void print_default_settings()
{
    printf("\nThe default listening port = %d\n", listening_default_port);
    printf("The default window size = %d\n", window_size);
    if (chsum_crc_mode == 1)
    {
        printf("The default mode is Checksum mode\n");
    }
    else
    {
        printf("The default mode is CRC mode\n");
    }
    printf("Show log = %s\n\n", (show_log == 0 ? "false" : "true"));
}

void* sent_packets_time_stamp_increase(void*)
{
    while(1)
    {
        usleep(1000);

        cache_item* ptr = sent_packets->next_item;
        while(ptr != NULL)
        {
            ptr->time_stamp.tv_usec += 1000;
            if (ptr->time_stamp.tv_usec >= 1000000)
            {
                ptr->time_stamp.tv_sec++;
                ptr->time_stamp.tv_usec = ptr->time_stamp.tv_usec % 1000000;
            }

            ptr = ptr->next_item;
        }
    }
}

void* retransmit_packets(void* args)
{
//return NULL;

    retransmit_parameters* retrans_param = ((retransmit_parameters*)(args));

    while(stop_retransmit == 0)
    {
        usleep(1000);

        num_retransmitted_packets += check_cache(sent_packets, retrans_param->socket, retrans_param->to_addr, window_size);
    }
}

void listen_to_default_port()
{
    if (sockfd_listen > 0)
    {
        printf("\n\n-> Already listening to port %d\n\n", listening_default_port);
        return;
    }

    printf("\n\n-> Create a Datagram Socket\n");
    sockfd_listen = socket(AF_INET, SOCK_DGRAM, 0);
    int optval = 1;
    setsockopt(sockfd_listen, SOL_SOCKET, SO_NO_CHECK, (void*)&optval, sizeof(optval));
    if (sock_buf_size != 0)
    {
        setsockopt(sockfd_listen, SOL_SOCKET, SO_RCVBUF, (char*)&sock_buf_size, sizeof(sock_buf_size));
    }


    if (sockfd_listen < 0)
    {
        printf("**** Error while creating the socket ****\n\n");
        return;
    }

    struct sockaddr_in listen_addr;

    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = listening_default_port;
    listen_addr.sin_addr.s_addr = INADDR_ANY;

    printf("-> Listen to port: %d\n\n", listening_default_port);
    bind(sockfd_listen, ((sockaddr*)(&listen_addr)), sizeof(listen_addr));

    stop_listening = 0;
    pthread_create(&listen_thread, NULL, listen_to_receive, NULL);
}

void* listen_to_receive(void*)
{
    struct sockaddr_in receive_addr;
    size_t receive_addr_size = sizeof(receive_addr);

    int buffer_size = 1024;
    char* buffer = ((char*)(malloc(buffer_size)));

    uint8_t out_of_order;

    uint8_t corrupted = 0;

    while(stop_listening == 0)
    {
        int bytes_num = recvfrom(sockfd_listen, buffer, buffer_size, 0, ((sockaddr*)(&receive_addr)), &receive_addr_size);

        num_received_packets++;

        struct rftp_hdr* rx_rftp_hdr = ((rftp_hdr*)(buffer));


        if (chsum_crc_mode == 1)
        {
            /* Checking the Checksum */
            uint16_t check_sum = rx_rftp_hdr->check_sum;

            rx_rftp_hdr->check_sum = 0;
            if (calc_cksum(((uint8_t*)(buffer)), bytes_num) != check_sum)
            {
                num_corrupted_packets++;

                if (show_log == 1)
                {
                    //timeval current_time_stamp;
                    //gettimeofday(&current_time_stamp, NULL);
                    printf("**** ");
                    print_timestamp();
                    printf(": Invalid Checksum: rx_seq = %d\n", rx_rftp_hdr->sequence_number);
                }

                corrupted = 1;
            }
        }
        else if (chsum_crc_mode == 2)
        {
            /* Checking the CRC */
            uint32_t crc = rx_rftp_hdr->crc;

            rx_rftp_hdr->crc = 0;
            if (crc32(((uint8_t*)(buffer)), bytes_num) != crc)
            {
                num_corrupted_packets++;

                if (show_log == 1)
                {
                    //timeval current_time_stamp;
                    //gettimeofday(&current_time_stamp, NULL);
                    printf("**** ");
                    print_timestamp();
                    printf(": Invalid CRC: rx_seq = %d\n", rx_rftp_hdr->sequence_number);
                }

                corrupted = 1;
            }
        }

        if (corrupted == 1)
        {
            corrupted = 0;

            /*struct rftp_hdr* tx_rftp_hdr = ((rftp_hdr*)(malloc(sizeof(rftp_hdr))));
            tx_rftp_hdr->sequence_number = receive_sequence_number;
            tx_rftp_hdr->ack_number = last_rx_seq_num + 1;
            tx_rftp_hdr->window_size = window_size;
            tx_rftp_hdr->data_length = 0;
            tx_rftp_hdr->check_sum = 0;
            tx_rftp_hdr->crc = 0;
            tx_rftp_hdr->flag = FIL_FLAG | ACK_FLAG;
            tx_rftp_hdr->padding_1 = 0;

            sendto(sockfd_listen, ((uint8_t*)(tx_rftp_hdr)), sizeof(rftp_hdr), 0, ((sockaddr*)(&receive_addr)), sizeof(receive_addr));*/

            memset(buffer, 0, buffer_size);
            continue;
        }


        double max_seq_num = pow(2, seq_num_bits) - 1;

//printf("***** %d, %d, %f, %d\n", rx_rftp_hdr->sequence_number, seq_num_bits, max_seq_num, seq_num_bits);
        if (ack_number == 0)
        {
            if (rx_rftp_hdr->sequence_number == 0)
            {
                ack_number = rx_rftp_hdr->sequence_number + 1;
            }
            else
            {
                if (rx_rftp_hdr->sequence_number == ((uint16_t)(max_seq_num)))
                {
                    ack_number = 1;
                }
                else
                {
                    ack_number = rx_rftp_hdr->sequence_number + 1; //rx_rftp_hdr->data_length;
                }
            }

            out_of_order = 0;
        }
        else
        {
            if (rx_rftp_hdr->sequence_number == 0)
            {
                ack_number = rx_rftp_hdr->sequence_number + 1;

                out_of_order = 0;
            }
            else
            {
                if (ack_number == rx_rftp_hdr->sequence_number)
                {
                    if (rx_rftp_hdr->sequence_number == ((uint16_t)(max_seq_num)))
                    {
                        ack_number = 1;
                    }
                    else
                    {
                        ack_number = rx_rftp_hdr->sequence_number + 1; //rx_rftp_hdr->data_length;
                    }

                    out_of_order = 0;
                }
                else
                {
                    out_of_order = 1;
                }
            }
        }


        if (rx_rftp_hdr->sequence_number != 0 & last_rx_seq_num == rx_rftp_hdr->sequence_number)
        {
            if (show_log == 1)
            {
                //timeval current_time_stamp;
                //gettimeofday(&current_time_stamp, NULL);
                printf("**** ");
                print_timestamp();
                printf(": Duplicate packet: rx_seq = %d\n", rx_rftp_hdr->sequence_number);
            }

            num_duplicate_packets++;
        }
        //else
        //{
            last_rx_seq_num = rx_rftp_hdr->sequence_number;

            if (window_size == 1 | (rx_rftp_hdr->flag & NAM_FLAG) == NAM_FLAG | rx_rftp_hdr->sequence_number % window_size == 1 |
                rx_rftp_hdr->sequence_number == 0)
            {
                struct rftp_hdr* tx_rftp_hdr = ((rftp_hdr*)(malloc(sizeof(rftp_hdr))));
                tx_rftp_hdr->sequence_number = receive_sequence_number;
                tx_rftp_hdr->ack_number = ack_number;
                tx_rftp_hdr->window_size = window_size;
                tx_rftp_hdr->data_length = 0;
                tx_rftp_hdr->check_sum = 0;
                tx_rftp_hdr->crc = 0;
                tx_rftp_hdr->flag = FIL_FLAG | ACK_FLAG;
                tx_rftp_hdr->padding_1 = 0;

                if (chsum_crc_mode == 1)
                {
                    tx_rftp_hdr->check_sum = calc_cksum(((uint8_t*)(tx_rftp_hdr)), sizeof(rftp_hdr));
                }
                else if (chsum_crc_mode == 2)
                {
                    tx_rftp_hdr->crc = crc32(((uint8_t*)(tx_rftp_hdr)), sizeof(rftp_hdr));
                }

                sendto(sockfd_listen, ((uint8_t*)(tx_rftp_hdr)), sizeof(rftp_hdr), 0, ((sockaddr*)(&receive_addr)), sizeof(receive_addr));

                if (show_log == 1)
                {
                    //timeval current_time_stamp;
                    //gettimeofday(&current_time_stamp, NULL);
                    printf("**** ");
                    print_timestamp();
                    printf(": rx_seq = %d, tx_ack = %d\n", rx_rftp_hdr->sequence_number, ack_number);
                    /*printf("**** %ld-%ld: rx_seq = %d, tx_ack = %d, out of order = %d\n", current_time_stamp.tv_sec, current_time_stamp.tv_usec,
                        rx_rftp_hdr->sequence_number, ack_number, out_of_order);*/
                }
            }
            else
            {
                if (show_log == 1)
                {
                    //timeval current_time_stamp;
                    //gettimeofday(&current_time_stamp, NULL);
                    printf("**** ");
                    print_timestamp();
                    printf(": rx_seq = %d\n", rx_rftp_hdr->sequence_number);
                    /*printf("**** %ld-%ld: rx_seq = %d, tx_ack = %d, out of order = %d\n", current_time_stamp.tv_sec, current_time_stamp.tv_usec,
                        rx_rftp_hdr->sequence_number, ack_number, out_of_order);*/
                }
            }

/*if (rand() <= 480201534)
{
    printf("**** tx_Duplicate ****\n");
    sendto(sockfd_listen, ((uint8_t*)(tx_rftp_hdr)), sizeof(rftp_hdr), 0, ((sockaddr*)(&receive_addr)), sizeof(receive_addr));
}*/
        //}


        if (rx_rftp_hdr->sequence_number != 0 & out_of_order == 1)
        {
            continue;
        }


        if ((rx_rftp_hdr->flag & TXT_FLAG) == TXT_FLAG)
        {
            if (bytes_num > 0)
            {
                printf("\n\n-> Received message: %s\n", buffer + sizeof(rftp_hdr));
            }

            ack_number = 0;
            num_received_packets = 0;
            num_duplicate_packets = 0;
            num_corrupted_packets = 0;

            last_rx_seq_num = 0; //-1;
        }
        else if ((rx_rftp_hdr->flag & FIL_FLAG) == FIL_FLAG)
        {
            if ((rx_rftp_hdr->flag & NAM_FLAG) == NAM_FLAG)
            {
                file_name = ((char*)(malloc(rx_rftp_hdr->data_length + 1)));
                memset(file_name, 0, rx_rftp_hdr->data_length + 1);
                memcpy(file_name, buffer + sizeof(rftp_hdr), rx_rftp_hdr->data_length);

                file_name_part = ((char*)(malloc(rx_rftp_hdr->data_length + 5 + 1)));
                memset(file_name_part, 0, rx_rftp_hdr->data_length + 5 + 1);
                memcpy(file_name_part, buffer + sizeof(rftp_hdr), rx_rftp_hdr->data_length);
                memcpy(file_name_part + rx_rftp_hdr->data_length, ".part", 5);

                gettimeofday(&start_time, NULL);
            }
            else
            {
                FILE* receive_file = fopen(file_name_part, "ab");

                if (receive_file == NULL)
                {
                    printf("\n\nError while opening the file [%s]\n", file_name_part);
                }
                else
                {
                    fwrite(buffer + sizeof(rftp_hdr), 1, rx_rftp_hdr->data_length, receive_file);
                    fclose(receive_file);

                    if (rx_rftp_hdr->sequence_number == 0/*(rx_rftp_hdr->flag & EOF_FLAG) == EOF_FLAG*/)
                    {
                        timeval end_time;
                        gettimeofday(&end_time, NULL);
                        timeval time_diff;
                        timeval_diff(&time_diff, &start_time, &end_time);

                        rename(file_name_part, file_name);

                        FILE* received_file = fopen(file_name, "rb");
                        if (send_file == NULL)
                        {
                            printf("Error while opening the file [%s]\n", file_name);
                            return NULL;
                        }

                        fseek(received_file, 0, SEEK_END);
                        long received_file_size = ftell(received_file);
                        rewind(received_file);
                        fclose(received_file);

                        float througput = ((float)(received_file_size)) / (((float)(time_diff.tv_sec * 1000000 + time_diff.tv_usec)) / 1000000);

                        float number_of_packets = ((float)(received_file_size)) / (buffer_size - sizeof(rftp_hdr));
                        float througput_p = ((float)(number_of_packets)) / (((float)(time_diff.tv_sec * 1000000 + time_diff.tv_usec)) / 1000000);


                        printf("\n\n-> Received file: [%s]\n", file_name);
                        printf("Transmit duration: %ld [second(s)] and %ld [microsecond(s)]\n", time_diff.tv_sec, time_diff.tv_usec);
                        printf("Througput: %.2f [Pps]\n", througput_p);
                        printf("Througput: %.2f [Bps]\n", througput);
                        printf("Througput: %.2f [bps]\n", througput * 8);
                        printf("Number of received packets: %ld\n", num_received_packets);
                        printf("Number of duplicate packets: %ld\n", num_duplicate_packets);
                        printf("Number of corrupted packets: %ld\n", num_corrupted_packets);

                        free(file_name);
                        free(file_name_part);
                        file_name = 0;
                        file_name_part = 0;

                        ack_number = 0;
                        num_received_packets = 0;
                        num_duplicate_packets = 0;
                        num_corrupted_packets = 0;

                        last_rx_seq_num = 0; //-1;
                    }
                }
            }
        }

        memset(buffer, 0, buffer_size);
    }
}

void stop_listening_to_default_port()
{
    if (sockfd_listen > 0)
    {
        printf("\n\n-> Stop listening to port %d\n\n", listening_default_port);
        close(sockfd_listen);
        stop_listening = 1;
    }
    else
    {
        printf("\n\n-> Already stoped listening to port %d\n\n", listening_default_port);
    }
}

void change_window_size()
{
    printf("\n-> The window size = %d\n", window_size);
    printf("-> Please enter new window size value: ");
    scanf("%d", &window_size);
    printf("\n");
}

void change_logging_status()
{
    printf("\n-> Show log = %s\n", (show_log == 0 ? "false" : "true"));
    printf("-> Please enter new status of logging [0 or 1]: ");
    scanf("%d", &show_log);
    printf("\n");
}

void change_sequence_number_bits()
{
    printf("\n-> The sequence number bit = %d\n", seq_num_bits);
    printf("-> Please enter new sequence number bits: ");
    scanf("%d", &seq_num_bits);
    printf("\n");
}

void send_file()
{
    num_retransmitted_packets = 0;
    int last_rx_ack_num = 0;
    long num_duplicate_acks = 0;
    uint8_t rx_duplicate_acks = 0;
    long num_rx_corrupted_packets = 0;
    uint8_t corrupted = 0;

    double max_seq_num = pow(2, seq_num_bits) - 1;


    char file_path[256];
    memset(file_path, 0, 256);
    printf("Please enter the file path: ");
    getchar();
    gets(file_path);

    char peer_address[16];
    memset(peer_address, 0, 15);
    printf("Please enter the address of the peer computer: ");
    gets(peer_address);


    int receive_buffer_size = 1024;
    char* receive_buffer = ((char*)(malloc(receive_buffer_size)));


    FILE* send_file = fopen(file_path, "rb");
    if (send_file == NULL)
    {
        printf("Error while opening the file [%s]\n", file_path);
        return;
    }

    fseek(send_file, 0, SEEK_END);
    long send_file_size = ftell(send_file);
    rewind(send_file);

    int buffer_size = 1024 - sizeof(rftp_hdr);
    long packets_number = ceil(send_file_size / buffer_size);
    int remaining_size = send_file_size % buffer_size;


    int sockfd_send = socket(AF_INET, SOCK_DGRAM, 0);
    int optval = 1;
    setsockopt(sockfd_send, SOL_SOCKET, SO_NO_CHECK, (void*)&optval, sizeof(optval));


    struct sockaddr_in to_addr;
    size_t to_addr_size = sizeof(to_addr);
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = listening_default_port;
    inet_aton(peer_address, &to_addr.sin_addr);


    struct rftp_hdr* tx_rftp_hdr = ((rftp_hdr*)(malloc(sizeof(rftp_hdr))));

    uint16_t sequence_number = 1; //0;


    uint8_t* tx_name_packet = ((uint8_t*)(malloc(strlen(file_path) + sizeof(rftp_hdr))));
    memset(tx_name_packet, 0, strlen(file_path) + sizeof(rftp_hdr));

    /* Sending the file name */
    tx_rftp_hdr->sequence_number = sequence_number;
    tx_rftp_hdr->ack_number = 0;
    tx_rftp_hdr->window_size = window_size;
    tx_rftp_hdr->data_length = strlen(file_path);
    tx_rftp_hdr->check_sum = 0;
    tx_rftp_hdr->crc = 0;
    tx_rftp_hdr->flag = FIL_FLAG | NAM_FLAG;
    tx_rftp_hdr->padding_1 = 0;

    memcpy(tx_name_packet, tx_rftp_hdr, sizeof(rftp_hdr));
    memcpy(tx_name_packet + sizeof(rftp_hdr), file_path, strlen(file_path));

    if (chsum_crc_mode == 1)
    {
        ((rftp_hdr*)(tx_name_packet))->check_sum = calc_cksum(tx_name_packet, strlen(file_path) + sizeof(rftp_hdr));
    }
    else if (chsum_crc_mode == 2)
    {
        ((rftp_hdr*)(tx_name_packet))->crc = crc32(tx_name_packet, strlen(file_path) + sizeof(rftp_hdr));
    }

    stop_retransmit = 0;
    start_retransmitting = 0;

    uint8_t name_data[8];
    while(true)
    {
//usleep(20000);
        if (corrupted == 0)
        {
            memset(name_data, 0, 8);
            int name_min = 8;
            if (strlen(file_path) < 8)
            {
                name_min = strlen(file_path);
            }
            for (int i = 0; i < name_min; i++)
            {
                name_data[i] = file_path[i];
            }
            timeval name_time_stamp;
            gettimeofday(&name_time_stamp, NULL);

            sendto(sockfd_send, tx_name_packet, strlen(file_path) + sizeof(rftp_hdr), 0, ((sockaddr*)(&to_addr)), sizeof(to_addr));

            cache_push(sent_packets, cache_create_item(tx_rftp_hdr->sequence_number, tx_name_packet, strlen(file_path) + sizeof(rftp_hdr),
                name_time_stamp));

            if (start_retransmitting == 0)
            {
                retransmit_parameters* retrans_param = ((retransmit_parameters*)(malloc(sizeof(retransmit_parameters))));
                retrans_param->socket = sockfd_send;
                retrans_param->to_addr = to_addr;
                pthread_create(&retransmit_packets_thread, NULL, retransmit_packets, retrans_param);

                start_retransmitting = 1;
            }
        }
        else
        {
            corrupted = 0;
        }

        int bytes_num = recvfrom(sockfd_send, receive_buffer, receive_buffer_size, 0, ((sockaddr*)(&to_addr)), &to_addr_size);

        if (chsum_crc_mode == 1)
        {
            /* Checking the Checksum */
            uint16_t check_sum = ((rftp_hdr*)(receive_buffer))->check_sum;

            ((rftp_hdr*)(receive_buffer))->check_sum = 0;
            if (calc_cksum(((uint8_t*)(receive_buffer)), bytes_num) != check_sum)
            {
                num_rx_corrupted_packets++;

                if (show_log == 1)
                {
                    //timeval current_time_stamp;
                    //gettimeofday(&current_time_stamp, NULL);
                    printf("**** ");
                    print_timestamp();
                    printf(": Invalid Checksum: rx_seq = %d\n", ((rftp_hdr*)(receive_buffer))->sequence_number);
                }

                corrupted = 1;
            }
        }
        else if (chsum_crc_mode == 2)
        {
            /* Checking the CRC */
            uint32_t crc = ((rftp_hdr*)(receive_buffer))->crc;

            ((rftp_hdr*)(receive_buffer))->crc = 0;
            if (crc32(((uint8_t*)(receive_buffer)), bytes_num) != crc)
            {
                num_rx_corrupted_packets++;
                
                if (show_log == 1)
                {
                    //timeval current_time_stamp;
                    //gettimeofday(&current_time_stamp, NULL);
                    printf("**** ");
                    print_timestamp();
                    printf(": Invalid CRC: rx_seq = %d\n", ((rftp_hdr*)(receive_buffer))->sequence_number);
                }

                corrupted = 1;
            }
        }


        if (corrupted == 0)
        {
            if (show_log == 1)
            {
                //timeval current_time_stamp;
                //gettimeofday(&current_time_stamp, NULL);
                printf("**** ");
                print_timestamp();
                printf(": rx_ack = %d, tx_seq = %d\n", ((rftp_hdr*)(receive_buffer))->ack_number, sequence_number);
            }

            if (((rftp_hdr*)(receive_buffer))->ack_number == 2/*1*/ /*strlen(file_path)*/)
            {
                cache_search(sent_packets, tx_rftp_hdr->sequence_number, window_size, name_data);

                break;
            }
        }
    }

    corrupted = 0;

    if (sequence_number == ((uint16_t)(max_seq_num)))
    {
        sequence_number = 0;
    }
    else
    {
        sequence_number += 1; //strlen(file_path);
    }
    

    last_rx_ack_num = ((rftp_hdr*)(receive_buffer))->ack_number;

    window_size = ((rftp_hdr*)(receive_buffer))->window_size;


    char* buffer = ((char*)(malloc(buffer_size)));
    memset(buffer, 0, buffer_size);

    uint8_t* tx_packet = ((uint8_t*)(malloc(buffer_size + sizeof(rftp_hdr))));
    memset(tx_packet, 0, buffer_size + sizeof(rftp_hdr));


    uint32_t file_index = 0;
    uint8_t data[8];

    uint16_t packets_in_window = 0;

    //for (int i = 0; i < packets_number; i++)
    while (send_file_size - file_index > buffer_size)
    {
        if (corrupted == 0)
        {
            if (rx_duplicate_acks == 0)
            {
                tx_rftp_hdr->sequence_number = sequence_number;
                tx_rftp_hdr->ack_number = 0;
                tx_rftp_hdr->window_size = window_size;
                tx_rftp_hdr->data_length = buffer_size;
                tx_rftp_hdr->check_sum = 0;
                tx_rftp_hdr->crc = 0;
                tx_rftp_hdr->flag = FIL_FLAG;
                tx_rftp_hdr->padding_1 = 0;

                memcpy(tx_packet, tx_rftp_hdr, sizeof(rftp_hdr));

                fread(buffer, 1, buffer_size, send_file);
                memcpy(tx_packet + sizeof(rftp_hdr), buffer, buffer_size);

                if (chsum_crc_mode == 1)
                {
                    ((rftp_hdr*)(tx_packet))->check_sum = calc_cksum(tx_packet, buffer_size + sizeof(rftp_hdr));
                }
                else if (chsum_crc_mode == 2)
                {
                    ((rftp_hdr*)(tx_packet))->crc = crc32(tx_packet, buffer_size + sizeof(rftp_hdr));
                }

//usleep(20000);
                /*if (strcmp(peer_address, "127.0.0.1") == 0)
                {
                    usleep(1000);
                }*/


                //uint8_t data[8];
                memset(data, 0, 8);
                int min = 8;
                if (buffer_size < 8)
                {
                    min = buffer_size;
                }
                for (int i = 0; i < min; i++)
                {
                    data[i] = buffer[i];
                }
                timeval time_stamp;
                gettimeofday(&time_stamp, NULL);

                sendto(sockfd_send, tx_packet, buffer_size + sizeof(rftp_hdr), 0, ((sockaddr*)(&to_addr)), sizeof(to_addr));
                cache_push(sent_packets, cache_create_item(tx_rftp_hdr->sequence_number, tx_packet, buffer_size + sizeof(rftp_hdr), time_stamp));

                packets_in_window++;
/*if (rand() <= 480201534)
{
    printf("**** Duplicate ****\n");
    sendto(sockfd_send, tx_packet, buffer_size + sizeof(rftp_hdr), 0, ((sockaddr*)(&to_addr)), sizeof(to_addr));
}*/
            }
            else
            {
                rx_duplicate_acks = 0;
            }
        }
        else
        {
            corrupted = 0;
        }

        int bytes_num;

        if (/*sequence_number*/ packets_in_window % window_size == 0 | window_size == 1)
        {
            bytes_num = recvfrom(sockfd_send, receive_buffer, receive_buffer_size, 0, ((sockaddr*)(&to_addr)), &to_addr_size);

            if (chsum_crc_mode == 1)
            {
                /* Checking the Checksum */
                uint16_t check_sum = ((rftp_hdr*)(receive_buffer))->check_sum;

                ((rftp_hdr*)(receive_buffer))->check_sum = 0;
                if (calc_cksum(((uint8_t*)(receive_buffer)), bytes_num) != check_sum)
                {
                    num_rx_corrupted_packets++;

                    if (show_log == 1)
                    {
                        //timeval current_time_stamp;
                        //gettimeofday(&current_time_stamp, NULL);
                        printf("**** ");
                        print_timestamp();
                        printf(": Invalid Checksum: rx_ack = %d\n", ((rftp_hdr*)(receive_buffer))->ack_number);
                    }

                    corrupted = 1;
                }
            }
            else if (chsum_crc_mode == 2)
            {
                /* Checking the CRC */
                uint32_t crc = ((rftp_hdr*)(receive_buffer))->crc;

                ((rftp_hdr*)(receive_buffer))->crc = 0;
                if (crc32(((uint8_t*)(receive_buffer)), bytes_num) != crc)
                {
                    num_rx_corrupted_packets++;

                    if (show_log == 1)
                    {
                        //timeval current_time_stamp;
                        //gettimeofday(&current_time_stamp, NULL);
                        printf("**** ");
                        print_timestamp();
                        printf(": Invalid CRC: rx_ack = %d\n", ((rftp_hdr*)(receive_buffer))->ack_number);
                    }
                
                    corrupted = 1;
                }
            }

            if (corrupted == 0)
            {
//printf("****** %d, %d\n", last_rx_ack_num, ((rftp_hdr*)(receive_buffer))->ack_number);

                if (last_rx_ack_num == ((rftp_hdr*)(receive_buffer))->ack_number)
                {
                    if (show_log == 1)
                    {
                        //timeval current_time_stamp;
                        //gettimeofday(&current_time_stamp, NULL);
                        printf("**** ");
                        print_timestamp();
                        printf(": Duplicate ack: rx_ack = %d\n", ((rftp_hdr*)(receive_buffer))->ack_number);
                    }

                    num_duplicate_acks++;

                    rx_duplicate_acks = 1;
                }
                else
                {
                    last_rx_ack_num = ((rftp_hdr*)(receive_buffer))->ack_number;


                    if (((rftp_hdr*)(receive_buffer))->ack_number % ((uint16_t)(max_seq_num)) ==
                        ((uint16_t)(tx_rftp_hdr->sequence_number + 1) % ((uint16_t)(max_seq_num))))
                    {
                        cache_search(sent_packets, tx_rftp_hdr->sequence_number, window_size, data);

                        file_index += tx_rftp_hdr->data_length;
                        if (sequence_number == ((uint16_t)(max_seq_num)))
                        {
                            sequence_number = 1;
                        }
                        else
                        {
                            sequence_number += 1; //tx_rftp_hdr->data_length;
                        }
                    }
                    else
                    {
//printf("******** ERROR: rx_ack = %d\n", ((rftp_hdr*)(receive_buffer))->ack_number);
                        file_index -= tx_rftp_hdr->data_length * abs(tx_rftp_hdr->sequence_number - ((rftp_hdr*)(receive_buffer))->ack_number);
                        if (sequence_number == 1)
                        {
                            sequence_number = ((uint16_t)(max_seq_num));
                        }
                        else
                        {
                            sequence_number -= abs(tx_rftp_hdr->sequence_number - ((rftp_hdr*)(receive_buffer))->ack_number);
                        }
                    }

                    if (show_log == 1)
                    {
                        //timeval current_time_stamp;
                        //gettimeofday(&current_time_stamp, NULL);
                        /*printf("**** %ld-%ld: rx_ack = %d, tx_seq = %d\n", current_time_stamp.tv_sec, current_time_stamp.tv_usec,
                            ((rftp_hdr*)(receive_buffer))->ack_number, sequence_number);*/
                        printf("**** ");
                        print_timestamp();
                        printf(": rx_ack = %d, tx_seq = %d, file_index = %d\n", ((rftp_hdr*)(receive_buffer))->ack_number,
                            sequence_number, file_index);
                    }


                    fseek(send_file, file_index, SEEK_SET);

                    memset(buffer, 0, buffer_size);
                }
            }
        }
        else
        {
            if (show_log == 1)
            {
                //timeval current_time_stamp;
                //gettimeofday(&current_time_stamp, NULL);
                printf("**** ");
                print_timestamp();
                printf(": tx_seq = %d\n", sequence_number);
                /*printf("**** %ld-%ld: tx_seq = %d, file_index = %d\n", current_time_stamp.tv_sec, current_time_stamp.tv_usec,
                    sequence_number, file_index);*/
            }

            file_index += tx_rftp_hdr->data_length;
            if (sequence_number == ((uint16_t)(max_seq_num)))
            {
                sequence_number = 1;
            }
            else
            {
                sequence_number += 1; //tx_rftp_hdr->data_length;
            }
        }
    }
    free(buffer);


    memset(receive_buffer, 0, receive_buffer_size);

    /* Remaining Size */
    uint8_t* tx_remaining_packet = ((uint8_t*)(malloc(remaining_size + sizeof(rftp_hdr))));
    memset(tx_remaining_packet, 0, remaining_size + sizeof(rftp_hdr));

    tx_rftp_hdr->sequence_number = 0; //sequence_number;
    tx_rftp_hdr->ack_number = 0;
    tx_rftp_hdr->window_size = window_size;
    tx_rftp_hdr->data_length = remaining_size;
    tx_rftp_hdr->check_sum = 0;
    tx_rftp_hdr->crc = 0;
    tx_rftp_hdr->flag = FIL_FLAG | EOF_FLAG;
    tx_rftp_hdr->padding_1 = 0;

    memcpy(tx_remaining_packet, tx_rftp_hdr, sizeof(rftp_hdr));


    char* remaining_buffer = ((char*)(malloc(remaining_size)));
    memset(remaining_buffer, 0, remaining_size);

    fread(remaining_buffer, 1, remaining_size, send_file);
    memcpy(tx_remaining_packet + sizeof(rftp_hdr), remaining_buffer, remaining_size);

    if (chsum_crc_mode == 1)
    {
        ((rftp_hdr*)(tx_remaining_packet))->check_sum = calc_cksum(tx_remaining_packet, remaining_size + sizeof(rftp_hdr));
    }
    else if (chsum_crc_mode == 2)
    {
        ((rftp_hdr*)(tx_remaining_packet))->crc = crc32(tx_remaining_packet, remaining_size + sizeof(rftp_hdr));
    }


    /*if (strcmp(peer_address, "127.0.0.1") == 0)
    {
        usleep(1000);
    }*/

    corrupted = 0;
    uint8_t remaining_data[8];

    while(1)
    {
//usleep(200000);
        if (corrupted == 0)
        {
            memset(name_data, 0, 8);
            int remaining_min = 8;
            if (remaining_size < 8)
            {
                remaining_min = remaining_size;
            }
            for (int i = 0; i < remaining_min; i++)
            {
                remaining_data[i] = remaining_buffer[i];
            }
            timeval remaining_time_stamp;
            gettimeofday(&remaining_time_stamp, NULL);

            sendto(sockfd_send, tx_remaining_packet, remaining_size + sizeof(rftp_hdr), 0, ((sockaddr*)(&to_addr)), sizeof(to_addr));
            cache_push(sent_packets, cache_create_item(tx_rftp_hdr->sequence_number, tx_remaining_packet, remaining_size + sizeof(rftp_hdr),
                remaining_time_stamp));
        }
        else
        {
            corrupted = 0;
        }

        int bytes_num = recvfrom(sockfd_send, receive_buffer, receive_buffer_size, 0, ((sockaddr*)(&to_addr)), &to_addr_size);

        if (chsum_crc_mode == 1)
        {
            /* Checking the Checksum */
            uint16_t check_sum = ((rftp_hdr*)(receive_buffer))->check_sum;

            ((rftp_hdr*)(receive_buffer))->check_sum = 0;
            if (calc_cksum(((uint8_t*)(receive_buffer)), bytes_num) != check_sum)
            {
                num_rx_corrupted_packets++;

                if (show_log == 1)
                {
                    //timeval current_time_stamp;
                    //gettimeofday(&current_time_stamp, NULL);
                    printf("**** ");
                    print_timestamp();
                    printf(": Invalid Checksum: rx_seq = %d\n", ((rftp_hdr*)(receive_buffer))->sequence_number);
                }

                corrupted = 1;
            }
        }
        else if (chsum_crc_mode == 2)
        {
            /* Checking the CRC */
            uint32_t crc = ((rftp_hdr*)(receive_buffer))->crc;

            ((rftp_hdr*)(receive_buffer))->crc = 0;
            if (crc32(((uint8_t*)(receive_buffer)), bytes_num) != crc)
            {
                num_rx_corrupted_packets++;
                
                if (show_log == 1)
                {
                    //timeval current_time_stamp;
                    //gettimeofday(&current_time_stamp, NULL);
                    printf("**** ");
                    print_timestamp();
                    printf(": Invalid CRC: rx_seq = %d\n", ((rftp_hdr*)(receive_buffer))->sequence_number);
                }

                corrupted = 1;
            }
        }


        if (corrupted == 0)
        {
            if (show_log == 1)
            {
                //timeval current_time_stamp;
                //gettimeofday(&current_time_stamp, NULL);
                /*printf("**** %ld-%ld: rx_ack = %d, tx_seq = %d\n", current_time_stamp.tv_sec, current_time_stamp.tv_usec,
                    ((rftp_hdr*)(receive_buffer))->ack_number, 0);*/
                printf("**** ");
                print_timestamp();
                printf(": rx_ack = %d, tx_seq = %d, file_index = %d\n", ((rftp_hdr*)(receive_buffer))->ack_number,
                    tx_rftp_hdr->sequence_number, file_index);
            }

            if (((rftp_hdr*)(receive_buffer))->ack_number == 1 /*strlen(file_path)*/)
            {
                cache_search(sent_packets, tx_rftp_hdr->sequence_number, window_size, remaining_data);

                break;
            }
        }
    }

    free(remaining_buffer);
    
    close(sockfd_send);

    stop_retransmit = 1;

    printf("\n\nNumber of retransmitted packets: %ld\n", num_retransmitted_packets);
    printf("Number of duplicate acks: %ld\n", num_duplicate_acks);
    printf("Number of corrupted packets: %ld\n\n", num_rx_corrupted_packets);


    num_retransmitted_packets = 0;
}

void send_message()
{
    char message[1024];
    printf("Please enter a message: ");
    getchar();
    gets(message);

    char peer_address[16];
    memset(peer_address, 0, 15);
    printf("Please enter the address of the peer computer: ");
    gets(peer_address);


    struct rftp_hdr* tx_rftp_hdr = ((rftp_hdr*)(malloc(sizeof(rftp_hdr))));

    uint16_t sequence_number = 1;


    uint8_t* tx_packet = ((uint8_t*)(malloc(strlen(message) + sizeof(rftp_hdr))));
    memset(tx_packet, 0, strlen(message) + sizeof(rftp_hdr));

    /* Sending the file name */
    tx_rftp_hdr->sequence_number = sequence_number;
    tx_rftp_hdr->ack_number = 0;
    tx_rftp_hdr->window_size = window_size;
    tx_rftp_hdr->data_length = strlen(message);
    tx_rftp_hdr->check_sum = 0;
    tx_rftp_hdr->crc = 0;
    tx_rftp_hdr->flag = TXT_FLAG;
    tx_rftp_hdr->padding_1 = 0;

    memcpy(tx_packet, tx_rftp_hdr, sizeof(rftp_hdr));
    memcpy(tx_packet + sizeof(rftp_hdr), message, strlen(message));


    if (chsum_crc_mode == 1)
    {
        ((rftp_hdr*)(tx_packet))->check_sum = calc_cksum(tx_packet, strlen(message) + sizeof(rftp_hdr));
    }
    else if (chsum_crc_mode == 2)
    {
        ((rftp_hdr*)(tx_packet))->crc = crc32(tx_packet, strlen(message) + sizeof(rftp_hdr));
    }

    int sockfd_send = socket(AF_INET, SOCK_DGRAM, 0);
    int optval = 1;
    setsockopt(sockfd_send, SOL_SOCKET, SO_NO_CHECK, (void*)&optval, sizeof(optval));

    struct sockaddr_in to_addr;

    to_addr.sin_family = AF_INET;
    to_addr.sin_port = listening_default_port;
    inet_aton(peer_address, &to_addr.sin_addr);
    
    sendto(sockfd_send, tx_packet, strlen(message) + sizeof(rftp_hdr), 0, ((sockaddr*)(&to_addr)), sizeof(to_addr));

    close(sockfd_send);
}

void change_listening_default_port()
{
    printf("\n-> The default port = %d\n", listening_default_port);
    printf("-> Please enter new listening default port: ");
    scanf("%d", &listening_default_port);
    printf("\n");
}


void change_receive_buffer()
{
    printf("\n-> Please enter the received buffer size in [bytes]: ");
    scanf("%d", &sock_buf_size);
    printf("\n");
}

void work_in_check_sum_mode()
{
    chsum_crc_mode = 1;
    printf("\nWorking in Checksum mode\n\n");
}

void work_in_crc_mode()
{
    chsum_crc_mode = 2;
    printf("\nWorking in CRC mode\n\n");
}

uint16_t calc_cksum(uint8_t* hdr, int len)
{
    long sum = 0;

    while(len > 1)
    {
        sum += *((unsigned short*)hdr);
        hdr = hdr + 2;
        if(sum & 0x80000000)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        len -= 2;
    }

    if(len)
    {
        sum += (unsigned short) *(unsigned char *)hdr;
    }
          
    while(sum>>16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

void print_timestamp()
{
    struct timeval tv;
    struct tm* ptm;
    char time_string[40];
    long microseconds;

    gettimeofday(&tv, NULL);
    ptm = localtime(&tv.tv_sec);
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", ptm);
    microseconds = tv.tv_usec;
    
    printf("%s.%03ld", time_string, microseconds);
}
