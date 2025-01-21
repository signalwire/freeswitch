

#include "switch_uuidv7.h"

// #include <assert.h>
// #include <stdio.h>
// #include <string.h>
// #include <time.h>
// #include <unistd.h>
#ifdef __APPLE__
#include <sys/random.h> // for macOS getentropy()
#endif

SWITCH_DECLARE(int) uuidv7_new(uint8_t *uuid_out)
{
    int8_t status;
    // struct timespec tp;
    static uint8_t uuid_prev[16] = {0};
    static uint8_t rand_bytes[256] = {0};
    static size_t n_rand_consumed = sizeof(rand_bytes);

    uint64_t unix_ts_ms ;
    // clock_gettime(CLOCK_REALTIME, &tp);
    // unix_ts_ms = (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
    unix_ts_ms = switch_time_now() / 1000;

    if (n_rand_consumed > sizeof(rand_bytes) - 10)
    {
        getentropy(rand_bytes, n_rand_consumed);
        n_rand_consumed = 0;
    }

    status = uuidv7_generate(uuid_prev, unix_ts_ms,
                                    &rand_bytes[n_rand_consumed], uuid_prev);
    n_rand_consumed += uuidv7_status_n_rand_consumed(status);

    memcpy(uuid_out, uuid_prev, 16);
    return status;

}
