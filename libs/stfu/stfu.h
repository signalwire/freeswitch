/*
 * STFU (S)ort (T)ransportable (F)ramed (U)tterances
 * Copyright (c) 2007 Anthony Minessale II <anthm@freeswitch.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE. 
 *
 * THOSE WHO DISAGREE MAY CERTIANLY STFU
 */

#ifndef STFU_H
#define STFU_H
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __STUPIDFORMATBUG__
}
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef  _MSC_VER
#ifndef uint32_t
typedef unsigned __int8     uint8_t;
typedef unsigned __int16    uint16_t;
typedef unsigned __int32    uint32_t;
typedef unsigned __int64    uint64_t;
typedef __int8      int8_t;
typedef __int16     int16_t;
typedef __int32     int32_t;
typedef __int64     int64_t;
typedef unsigned long   in_addr_t;
#endif
#define snprintf _snprintf
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <inttypes.h>
#endif
#include <assert.h>

#define STFU_DATALEN 16384
#define STFU_QLEN 300
#define STFU_MAX_TRACK 256

typedef enum {
	STFU_IT_FAILED,
	STFU_IT_WORKED,
	STFU_IM_DONE
} stfu_status_t;

struct stfu_frame {
	uint32_t ts;
	uint32_t pt;
	uint8_t data[STFU_DATALEN];
	size_t dlen;
	uint8_t was_read;
	uint8_t plc;
};
typedef struct stfu_frame stfu_frame_t;

struct stfu_instance;
typedef struct stfu_instance stfu_instance_t;

typedef struct {
	uint32_t in_len;
	uint32_t in_size;
	uint32_t out_len;
	uint32_t out_size;

} stfu_report_t;


void stfu_n_report(stfu_instance_t *i, stfu_report_t *r);
void stfu_n_destroy(stfu_instance_t **i);
stfu_instance_t *stfu_n_init(uint32_t qlen);
stfu_status_t stfu_n_resize(stfu_instance_t *i, uint32_t qlen);
stfu_status_t stfu_n_add_data(stfu_instance_t *i, uint32_t ts, uint32_t pt, void *data, size_t datalen, int last);
stfu_frame_t *stfu_n_read_a_frame(stfu_instance_t *i);
void stfu_n_reset(stfu_instance_t *i);

#define stfu_im_done(i) stfu_n_add_data(i, 0, NULL, 0, 1)
#define stfu_n_eat(i,t,p,d,l) stfu_n_add_data(i, t, p, d, l, 0)

#ifdef __cplusplus
}
#endif
#endif /*STFU_H*/

