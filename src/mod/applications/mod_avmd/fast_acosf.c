#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <stdint.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef _MSC_VER
#include <sys/mman.h>
#endif
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include "fast_acosf.h"
#include "options.h"

#ifdef FASTMATH

#define SIGN_MASK (0x80000000)
#define DATA_MASK (0x07FFFFF8)

#define SIGN_UNPACK_MASK (0x01000000)
#define DATA_UNPACK_MASK (0x00FFFFFF)

#define VARIA_DATA_MASK (0x87FFFFF8)
#define CONST_DATA_MASK (0x38000000)

#define ACOS_TABLE_LENGTH (1 << 25)
#define ACOS_TABLE_FILENAME "/tmp/acos_table.dat"

typedef union {
    uint32_t i;
    float f;
} float_conv_t;

#ifdef FAST_ACOSF_TESTING
static float strip_float(float f);
#endif
static uint32_t index_from_float(float f);
static float float_from_index(uint32_t d);
static float *acos_table = NULL;
static int acos_fd = -1;

#ifdef FAST_ACOSF_TESTING
static float strip_float(float f)
{
    float_conv_t d;

    d.i = d.i & (VARIA_DATA_MASK | CONST_DATA_MASK);

    return d.i;
}
#endif

extern int compute_table(void)
{
    uint32_t i;
    float   f;
    FILE    *acos_table_file;
    size_t  res;

    acos_table_file = fopen(ACOS_TABLE_FILENAME, "w");

    for (i = 0; i < ACOS_TABLE_LENGTH; i++) {
        f = acosf(float_from_index(i));
        res = fwrite(&f, sizeof(f), 1, acos_table_file);
        if (res != 1) {
            goto fail;
        }
    }

    res = fclose(acos_table_file);
    if (res != 0) {
        return -2;
    }
    return 0;

fail:
    fclose(acos_table_file);
    return -1;
}

extern int init_fast_acosf(void)
{
    int     ret, errsv;
    FILE    *acos_fp;
    char    err[150];

    if (acos_table == NULL) {
        ret = access(ACOS_TABLE_FILENAME, F_OK);
        if (ret == -1) {
            /* file doesn't exist, bad permissions,
             * or some other error occured */
            errsv = errno;
            strerror_r(errsv, err, 150);
            if (errsv != ENOENT) return -1;
            else {
	            switch_log_printf(
		            SWITCH_CHANNEL_LOG,
		            SWITCH_LOG_NOTICE,
		            "File [%s] doesn't exist. Creating file...\n", ACOS_TABLE_FILENAME
		        );
                ret = compute_table();
                if (ret != 0) return -2;
            }
        } else {
	        switch_log_printf(
	            SWITCH_CHANNEL_LOG,
		        SWITCH_LOG_INFO,
		        "Using previously created file [%s]\n", ACOS_TABLE_FILENAME
		    );
        }
    }

    acos_fp = fopen(ACOS_TABLE_FILENAME, "r");
    if (acos_fp == NULL) return -3;
    /* can't fail */
    acos_fd = fileno(acos_fp);
    acos_table = (float *) mmap(
            NULL,                               /* kernel chooses the address at which to create the mapping */
            ACOS_TABLE_LENGTH * sizeof(float),
            PROT_READ,
            MAP_SHARED | MAP_POPULATE,          /* read-ahead on the file.  Later accesses  to  the  mapping
                                                 * will not be blocked by page faults */
            acos_fd,
            0
            );
    if (acos_table == MAP_FAILED) return -4;

    return 0;
}

extern int destroy_fast_acosf(void)
{
    if (munmap(acos_table, ACOS_TABLE_LENGTH) == -1) return -1;
    if (acos_fd != -1) {
        if (close(acos_fd) == -1) return -2;
    }
    /* disable use of fast arc cosine file */
    acos_table = NULL;

    return 0;
}

extern float fast_acosf(float x)
{
    return acos_table[index_from_float(x)];
}


static uint32_t index_from_float(float f)
{
    float_conv_t d;

    d.f = f;
    return ((d.i & SIGN_MASK) >> 7) | ((d.i & DATA_MASK) >> 3);
}


static float float_from_index(uint32_t d)
{
    float_conv_t f;

    f.i = ((d & SIGN_UNPACK_MASK) << 7) | ((d & DATA_UNPACK_MASK) << 3) | CONST_DATA_MASK;
    return f.f;
}


#endif
