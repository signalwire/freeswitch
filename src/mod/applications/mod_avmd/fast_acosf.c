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

#define ACOS_TABLE_LENGTH (1<<25)
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

extern void compute_table(void)
{
    uint32_t i;
    float f;
    FILE *acos_table_file;
    size_t ret;

    acos_table_file = fopen(ACOS_TABLE_FILENAME, "w");


    for(i = 0; i < (1 << 25); i++){
	f = acosf(float_from_index(i));
	ret = fwrite(&f, sizeof(f), 1, acos_table_file);
	assert(ret != 0);
    }


    ret = fclose(acos_table_file);
    assert(ret != EOF);
}


extern void init_fast_acosf(void)
{
    int ret;

    if(acos_table == NULL){
	ret = access(ACOS_TABLE_FILENAME, F_OK);
	if(ret == 0) compute_table();

        acos_fd = open(ACOS_TABLE_FILENAME, O_RDONLY);
	if(acos_fd == -1) perror("Could not open file " ACOS_TABLE_FILENAME);
	assert(acos_fd != -1);
        acos_table = (float *)mmap(
            NULL,
            ACOS_TABLE_LENGTH * sizeof(float),
            PROT_READ,
            MAP_SHARED | MAP_POPULATE,
            acos_fd,
            0
        );
    }
}

extern void destroy_fast_acosf(void)
{
    int ret;

    ret = munmap(acos_table, ACOS_TABLE_LENGTH);
    assert(ret != -1);
    ret = close(acos_fd);
    assert(ret != -1);
    acos_table = NULL;
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


