/*
 * Soak test the RNG for exhaustion failures
 */
#include <stdio.h>           /* for printf() */
#include <unistd.h>          /* for getopt() */
#include "crypto_kernel.h"

#define BUF_LEN (MAX_PRINT_STRING_LEN/2)

int main(int argc, char *argv[])
{
    int q;
    extern char *optarg;
    int num_octets = 0;
    err_status_t status;
    uint32_t iterations = 0;
    int print_values = 0;

    if (argc == 1) {
        exit(255);
    }

    status = crypto_kernel_init();
    if (status) {
        printf("error: crypto_kernel init failed\n");
        exit(1);
    }

    while (1) {
        q = getopt(argc, argv, "pvn:");
        if (q == -1) {
            break;
        }
        switch (q) {
        case 'p':
            print_values = 1;
            break;
        case 'n':
            num_octets = atoi(optarg);
            if (num_octets < 0 || num_octets > BUF_LEN) {
                exit(255);
            }
            break;
        case 'v':
            num_octets = 30;
            print_values = 0;
            break;
        default:
            exit(255);
        }
    }

    if (num_octets > 0) {
        while (iterations < 300000) {
            uint8_t buffer[BUF_LEN];

            status = crypto_get_random(buffer, num_octets);
            if (status) {
                printf("iteration %d error: failure in random source\n", iterations);
                exit(255);
            } else if (print_values) {
                printf("%s\n", octet_string_hex_string(buffer, num_octets));
            }
            iterations++;
        }
    }

    status = crypto_kernel_shutdown();
    if (status) {
        printf("error: crypto_kernel shutdown failed\n");
        exit(1);
    }

    return 0;
}

