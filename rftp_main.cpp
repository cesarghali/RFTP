#include "rftp_protocol.h"


int print_menu();


int main()
{
    printf("\nWelcome to the RFTP program\n");
    printf("---------------------------\n");

    rftp_init();

    print_default_settings();

    int choice = print_menu();
    while(choice != 12)
    {
        switch(choice)
        {
            case 1:
                listen_to_default_port();
                break;

            case 2:
                stop_listening_to_default_port();
                break;

            case 3:
                change_window_size();
                break;

            case 4:
                change_logging_status();
                break;

            case 5:
                change_sequence_number_bits();
                break;

            case 6:
                send_message();
                break;

            case 7:
                send_file();
                break;

            case 8:
                change_listening_default_port();
                break;

            case 9:
                change_receive_buffer();
                break;

            case 10:
                work_in_check_sum_mode();
                break;

            case 11:
                work_in_crc_mode();
                break;

            default:
                break;
        }

        choice = print_menu();
    }

    printf("\n---------------------\n");
    printf("Thanks for using RFTP\n");
    printf("---------------------\n\n");

    return 0;
}

int print_menu()
{
    printf("\n 1. Listen to the [default port]\n");
    printf(" 2. Stop listening to the [default port]\n");
    printf(" 3. Change the window size\n");
    printf(" 4. Change logging status\n");
    printf(" 5. Change sequence number bits\n");
    printf(" 6. Send a message\n");
    printf(" 7. Send a file\n");
    printf(" 8. Change the listening [default port]\n");
    printf(" 9. Change the receive buffer size\n");
    printf("10. Work in Checksum mode\n");
    printf("11. Work in CRC mode\n");
    printf("12. Exit\n");
    printf("\nPlease write the option number and press [enter]: ");

    char input[1000];
    scanf("%s", input);

    int choice = atoi(input);

    if (choice < 1 | choice > 12)
    {
        printf("Bad option\n");
    }

    return choice;
}


