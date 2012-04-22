#ifndef __BUFFER_H__
#define __BUFFER_H__
#include <stdlib.h>
#include <assert.h>

#ifndef INT16_MIN
#define INT16_MIN              (-32767-1)
#endif
#ifndef INT16_MAX
#define INT16_MAX              (32767)
#endif

#define BUFF_TYPE double

typedef struct {
    size_t pos;
    size_t lpos;
    BUFF_TYPE *buf;
    size_t buf_len;
    size_t mask;
    size_t i;
    size_t backlog;
} circ_buffer_t;

extern size_t next_power_of_2(size_t v);

#define INC_POS(b) \
    { \
	(b)->pos++; \
	(b)->pos &= (b)->mask; \
	(b)->lpos++; \
	if((b)->backlog < (b)->buf_len) (b)->backlog++; \
    }

#define DEC_POS(b) \
    { \
	(b)->pos--; \
	(b)->pos &= (b)->mask; \
	(b)->lpos--; \
	if(((b)->backlog - 1) < (b)->backlog) (b)->backlog--; \
    }

#define GET_SAMPLE(b, i) ((b)->buf[(i) & (b)->mask])
#define SET_SAMPLE(b, i, v) ((b)->buf[(i) & (b)->mask] = (v))

#define INSERT_FRAME(b, f, l) \
    do{ \
	for((b)->i = 0; (b)->i < (l); (b)->i++){ \
	    SET_SAMPLE((b), ((b)->i + (b)->pos), (f)[(b)->i]); \
	} \
	(b)->pos += (l); \
	(b)->lpos += (l); \
	(b)->pos %= (b)->buf_len; \
	(b)->backlog += (l); \
	if((b)->backlog > (b)->buf_len) (b)->backlog = (b)->buf_len; \
    }while(0)

#define INSERT_INT16_FRAME(b, f, l) \
    { \
	for((b)->i = 0; (b)->i < (l); (b)->i++){ \
	    SET_SAMPLE( \
		(b), \
		((b)->i + (b)->pos), \
		( \
		    ((f)[(b)->i] >= 0) ? \
		    ((BUFF_TYPE)(f)[(b)->i] / (BUFF_TYPE)INT16_MAX): \
		    (0.0 - ((BUFF_TYPE)(f)[(b)->i] / (BUFF_TYPE)INT16_MIN)) \
		) \
	    ); \
	} \
	(b)->pos += (l); \
	(b)->lpos += (l); \
	(b)->pos &= (b)->mask; \
	(b)->backlog += (l); \
	if((b)->backlog > (b)->buf_len) (b)->backlog = (b)->buf_len; \
    }


#define CALC_BUFF_LEN(fl, bl) (((fl) >= (bl))? next_power_of_2((fl) << 1): next_power_of_2((bl) << 1))

#define INIT_CIRC_BUFFER(bf, bl, fl, s)			\
    { \
	(bf)->buf_len = CALC_BUFF_LEN((fl), (bl)); \
	(bf)->mask = (bf)->buf_len - 1; \
	(bf)->buf = (BUFF_TYPE *) switch_core_session_alloc(s, (bf)->buf_len * sizeof(BUFF_TYPE)); \
	assert((bf)->buf != NULL); \
	(bf)->pos = 0; \
	(bf)->lpos = 0; \
	(bf)->backlog = 0; \
    }

//#define DESTROY_CIRC_BUFFER(b) free((b)->buf)
#define GET_BACKLOG_POS(b) ((b)->lpos - (b)->backlog)
#define GET_CURRENT_POS(b) ((b)->lpos)
#define GET_CURRENT_SAMPLE(b) GET_SAMPLE((b), GET_CURRENT_POS((b)))

#define ADD_SAMPLE(b, s) \
    do{ \
	INC_POS((b)); \
	SET_SAMPLE((b), GET_CURRENT_POS((b)), (s)); \
    }while(0)

#endif

