#ifndef WIN32   /* currently we support fast acosf computation only on UNIX/Linux */


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
#include "avmd_fast_acosf.h"
#include "avmd_options.h"


typedef union {
    uint32_t i;
    float f;
} float_conv_t;

/* 
 * Manipulate these parameters to change
 * mapping's resolution. The sine tone
 * of 1600Hz is detected even with 20
 * bits discarded in float integer representation
 * with only slightly increased amount of false
 * positives (keeping variance threshold on 0.0001).
 * 12 bits seem to be good choice when there is
 * a need to compute faster and/or decrease mapped file
 * size on disk while keeping false positives low.
 */
#define ACOS_TABLE_CONST_EXPONENT (0x70)
#define ACOS_TABLE_CONST_EXPONENT_BITS (3)
#define ACOS_TABLE_DISCARDED_BITS (3)
/* rosolution:
    3: 15 728 640 indices spreading range [0.0, 1.0], table size on disk 134 217 728 bytes (default)
    4:  7 364 320 indices spreading range [0.0, 1.0], table size on disk  67 108 864 bytes
    5:  3 932 160 indices spreading range [0.0, 1.0], table size on disk  33 554 432 bytes
    12:    30 720 indices spreading range [0.0, 1.0], table size on disk     262 144 bytes
    16:     1 920 indices spreading range [0.0, 1.0], table size on disk      16 384 bytes
    20:       120 indices spreading range [0.0, 1.0], table size on disk       1 024 bytes
    24:         7 indices spreading range [0.0, 1.0], table size on disk          64 bytes
    26:         1 indices spreading range [0.0, 1.0], table size on disk          16 bytes
*/
#define ACOS_TABLE_FREE_EXPONENT_BITS (7 - ACOS_TABLE_CONST_EXPONENT_BITS)
#define ACOS_TABLE_DATA_BITS (31 - ACOS_TABLE_CONST_EXPONENT_BITS - ACOS_TABLE_DISCARDED_BITS)
#define ACOS_TABLE_LENGTH (1 << (31 - ACOS_TABLE_CONST_EXPONENT_BITS - ACOS_TABLE_DISCARDED_BITS))

#define VARIA_DATA_MASK (0x87FFFFFF & ~((1 << ACOS_TABLE_DISCARDED_BITS) - 1))
#define CONST_DATA_MASK (((1 << ACOS_TABLE_CONST_EXPONENT_BITS) - 1) \
                                    << (ACOS_TABLE_DATA_BITS - 1 + ACOS_TABLE_DISCARDED_BITS))

#define SIGN_UNPACK_MASK (1 << (ACOS_TABLE_DATA_BITS - 1))
#define DATA_UNPACK_MASK ((1 << (ACOS_TABLE_DATA_BITS - 1)) - 1)

#define SIGN_MASK  (0x80000000)
#define DATA_MASK (DATA_UNPACK_MASK << ACOS_TABLE_DISCARDED_BITS)

#define ACOS_TABLE_FILENAME "/tmp/acos_table.dat"

static uint32_t index_from_float(float f);
static float float_from_index(uint32_t d);
static float *acos_table = NULL;
static int acos_fd = -1;


#ifdef FAST_ACOSF_TESTING

#define INF(x) printf("[%s] [%u]\n", #x, x)
#define INFX(x) printf("[%s] [%08x]\n", #x, x)

static void
debug_print(void);

static void
dump_table_summary(void);

#endif /* FAST_ACOSF_TESTING */


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
    return ((d.i & SIGN_MASK) >> (32 - ACOS_TABLE_DATA_BITS)) |
        ((d.i & DATA_MASK) >> ACOS_TABLE_DISCARDED_BITS);
}

static float float_from_index(uint32_t d)
{
    float_conv_t f;
    f.i = ((d & SIGN_UNPACK_MASK) << (32 - ACOS_TABLE_DATA_BITS)) |
        ((d & DATA_UNPACK_MASK) << ACOS_TABLE_DISCARDED_BITS) | CONST_DATA_MASK;
    return f.f;
}

#ifdef FAST_ACOSF_TESTING

#define INF(x) printf("[%s] [%u]\n", #x, x)
#define INFX(x) printf("[%s] [%08x]\n", #x, x)

static void
debug_print(void)
{
    INF(ACOS_TABLE_CONST_EXPONENT);
    INF(ACOS_TABLE_CONST_EXPONENT_BITS);
    INF(ACOS_TABLE_FREE_EXPONENT_BITS);
    INF(ACOS_TABLE_DISCARDED_BITS);
    INF(ACOS_TABLE_DATA_BITS);
    INF(ACOS_TABLE_LENGTH);
    INFX(VARIA_DATA_MASK);
    INFX(CONST_DATA_MASK);
    INFX(SIGN_UNPACK_MASK);
    INFX(DATA_UNPACK_MASK);
    INFX(SIGN_MASK);
    INFX(DATA_MASK);
}

static void
dump_table_summary(void)
{
    uint32_t i, i_0, i_1, di;
    float f;

    i = 1;
    i_0 = index_from_float(0.0);
    i_1 = index_from_float(1.0);
    di = (i_1 - i_0)/100;
    if (di == 0)  di = 1;
 
    for (; i < ACOS_TABLE_LENGTH; i += di )
    {
        f = float_from_index(i);
        printf("-01i[%.10u] : ffi[%f] fa[%f] acos[%f]\n",
                i, f, fast_acosf(f), acos(f));
    }

    i = 1;
    for (; i < ACOS_TABLE_LENGTH; i = (i << 1))
    {
        f = fast_acosf(float_from_index(i));
        printf("--i[%.10u] : fa[%f] ffi[%f]\n",
                i, f, float_from_index(i));
    }

    f = 0.0;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.1;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.2;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.3;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.4;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.5;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.6;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.7;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 7.5;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.8;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.9;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.95;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.99;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 1.0;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 1.1;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 1.2;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = 0.0;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.1;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.2;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.3;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.4;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.5;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.6;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.7;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -7.5;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.8;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.9;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.95;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -0.99;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -1.0;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -1.1;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
    f = -1.2;
    printf("i [%d] from float [%f]\n", index_from_float(f), f);
}

#endif  /* FAST_ACOSF_TESTING */
#endif  /* WIN32 */
