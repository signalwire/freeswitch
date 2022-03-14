
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>
#include <string.h>
#include <limits.h>

#include "dtmf.h"


// powf(10.0f, i/10.0f) *10
int32_t db_tbl[] = {
	10,//0db 1.0000 
	12,//1db 1.2589 
	15,//2db 1.5849 
	19,//3db 1.9953 
	25,//4db 2.5119 
	31,//5db 3.1623 
	39,//6db 3.9811 
	50,//7db 5.0119 
	63,//8db 6.3096 
	79,//9db 7.9433 
	100,//10db 10.0000  
	125,//11db 12.5893  
	158,//12db 15.8489  
	199,//13db 19.9526  
	251,//14db 25.1189  
	316,//15db 31.6228  
	398,//16db 39.8107  
	501,//17db 50.1187  
	630,//18db 63.0957  
	794,//19db 79.4328  
	1000,//20db 100.0000  
	1258,//21db 125.8925  
	1584,//22db 158.4893  
	1995,//23db 199.5262  
	2511,//24db 251.1887  
};

// (102*32768.0f/(128.0f*1.4142f))*powf(10.0f, (dbm0 - (3.14f))/20.0f)
static int32_t threshold_tbl[] = {
	165446895,//0dbm0  
	131419153,//-1dbm0 
	104389942,//-2dbm0 
	82919876, //-3dbm0 
	65865596, //-4dbm0 
	52318902, //-5dbm0 
	41558382, //-6dbm0 
	33010994, //-7dbm0 
	26221565, //-8dbm0 
	20828531, //-9dbm0 
	16544691, //-10dbm0 
	13141913, //-11dbm0 
	10438993, //-12dbm0 
	8291988,  //-13dbm0 
	6586560,  //-14dbm0 
	5231890,  //-15dbm0 
	4155838,  //-16dbm0 
	3301099,  //-17dbm0 
	2622156,  //-18dbm0 
	2082853,  //-19dbm0 
	1654468,  //-20dbm0 
	1314191,  //-21dbm0 
	1043899,  //-22dbm0 
	829198,   //-23dbm0 
	658656,   //-24dbm0 
	523188,   //-25dbm0 
	415583,   //-26dbm0 
	330110,   //-27dbm0 
	262215,   //-28dbm0 
	208285,   //-29dbm0 
	165446,   //-30dbm0 
	131419,   //-31dbm0 
	104389,   //-32dbm0 
	82919,    //-33dbm0 
	65865,    //-34dbm0 
	52318,    //-35dbm0 
	41558,    //-36dbm0 
	33011,    //-37dbm0 
	26221,    //-38dbm0 
	20828,    //-39dbm0 
	16544,    //-40dbm0 
	13141,    //-41dbm0 
	10438,    //-42dbm0 
};



goertzel_state_t * goertzel_init(goertzel_state_t *s,
                                               goertzel_descriptor_t *t);
int goertzel_release(goertzel_state_t *s);

int goertzel_free(goertzel_state_t *s);

void goertzel_reset(goertzel_state_t *s);

int goertzel_update(goertzel_state_t *s,
                                  const int16_t amp[],
                                  int samples);
int32_t goertzel_result(goertzel_state_t *s);

DtmfPara gPara = {5, 9, 23, 32, 838, 10, 50, 60, -21, -15, -3, -3, 0};

static __inline__ void goertzel_sample(goertzel_state_t *s, int16_t amp)
{

    int16_t x;
    int16_t v1;

    v1 = s->v2;
    s->v2 = s->v3;

    x = (((int32_t) s->fac*s->v2) >> 14);
    s->v3 = x - v1 + (amp >> 7);
    s->current_sample++;
}

static __inline__ void goertzel_samplex(goertzel_state_t *s, int16_t amp)
{

    int16_t x;
    int16_t v1;

    v1 = s->v2;
    s->v2 = s->v3;

    x = (((int32_t) s->fac*s->v2) >> 14);
    s->v3 = x - v1 + amp;
}

goertzel_state_t * goertzel_init(goertzel_state_t *s,
                                               goertzel_descriptor_t *t)
{
    if (s == ((void *)0))
    {
        if ((s = (goertzel_state_t *) malloc(sizeof(*s))) == ((void *)0))
            return ((void *)0);
    }

    s->v2 =
    s->v3 = 0;
    s->fac = t->fac;
    s->samples = t->samples;
    s->current_sample = 0;
    return s;
}


int goertzel_release(goertzel_state_t *s)
{
    return 0;
}


int goertzel_free(goertzel_state_t *s)
{
    if (s)
        free(s);
    return 0;
}


void goertzel_reset(goertzel_state_t *s)
{

    s->v2 =
    s->v3 = 0;
    s->current_sample = 0;
}


int goertzel_update(goertzel_state_t *s,
                                  const int16_t amp[],
                                  int samples)
{
    int i;

    int16_t x;
    int16_t v1;

    if (samples > s->samples - s->current_sample)
        samples = s->samples - s->current_sample;
    for (i = 0; i < samples; i++)
    {
        v1 = s->v2;
        s->v2 = s->v3;

        x = (((int32_t) s->fac*s->v2) >> 14);
        s->v3 = x - v1 + (amp[i] >> 7);
    }
    s->current_sample += samples;
    return samples;
}



int32_t goertzel_result(goertzel_state_t *s)
{

    int16_t v1;
    int32_t x;
    int32_t y;

    v1 = s->v2;
    s->v2 = s->v3;

    x = (((int32_t) s->fac*s->v2) >> 14);
    s->v3 = x - v1;

    x = (int32_t) s->v3*s->v3;
    y = (int32_t) s->v2*s->v2;
    x += y;
    y = ((int32_t) s->v3*s->fac) >> 14;
    y *= s->v2;
    x -= y;
    x <<= 1;
    goertzel_reset(s);

    return x;
}


int dtmf_rx(dtmf_rx_state_t *s, const int16_t amp[], int samples);
int dtmf_rx_status(dtmf_rx_state_t *s);
size_t dtmf_rx_get(dtmf_rx_state_t *s, char *digits, int max);
dtmf_rx_state_t * dtmf_rx_init(dtmf_rx_state_t *s,
                                             dtmf_report_func_t callback,
                                             void *user_data);

int dtmf_rx_release(dtmf_rx_state_t *s);
int dtmf_rx_free(dtmf_rx_state_t *s);

static const char dtmf_positions[] = "123A" "456B" "789C" "*0#D";

static goertzel_descriptor_t dtmf_detect_row[4] =
{
	{27977,102},
	{26954,102},
	{25699,102},
	{24217,102}
};
static goertzel_descriptor_t dtmf_detect_col[4] =
{
	{19071,102},
	{16323,102},
	{13083,102},
	{9314,102}
};




static int dtmf_hits_to_begin = 3;		/* How many successive hits needed to consider begin of a digit */
static int dtmf_misses_to_end = 3;		/* How many successive misses needed to consider end of a digit */
int dtmf_rx(dtmf_rx_state_t *s, const int16_t amp[], int samples)
{

    int32_t row_energy[4];
    int32_t col_energy[4];
    int16_t xamp;

    int i;
    int j;
    int sample;
    int best_row;
    int best_col;
    int limit;
    uint8_t hit;

    for (sample = 0; sample < samples; sample = limit)
    {

        if ((samples - sample) >= (102 - s->current_sample))
            limit = sample + (102 - s->current_sample);
        else
            limit = samples;


        for (j = sample; j < limit; j++)
        {
            xamp = amp[j];
            xamp = (((int16_t) xamp) >> 7);

            s->energy += ((int32_t) xamp*xamp);

            goertzel_samplex(&s->row_out[0], xamp);
            goertzel_samplex(&s->col_out[0], xamp);
            goertzel_samplex(&s->row_out[1], xamp);
            goertzel_samplex(&s->col_out[1], xamp);
            goertzel_samplex(&s->row_out[2], xamp);
            goertzel_samplex(&s->col_out[2], xamp);
            goertzel_samplex(&s->row_out[3], xamp);
            goertzel_samplex(&s->col_out[3], xamp);
        }
        if (s->duration < 2147483647 - (limit - sample))
            s->duration += (limit - sample);
        s->current_sample += (limit - sample);
        if (s->current_sample < 102)
            continue;

        row_energy[0] = goertzel_result(&s->row_out[0]);
        best_row = 0;
        col_energy[0] = goertzel_result(&s->col_out[0]);
        best_col = 0;
        for (i = 1; i < 4; i++)
        {
            row_energy[i] = goertzel_result(&s->row_out[i]);
            if (row_energy[i] > row_energy[best_row])
                best_row = i;
            col_energy[i] = goertzel_result(&s->col_out[i]);
            if (col_energy[i] > col_energy[best_col])
                best_col = i;
        }
        hit = 0;

        if (row_energy[best_row] >= s->threshold
            &&
            col_energy[best_col] >= s->threshold)
        {
            if (col_energy[best_col]/s->reverse_twist < row_energy[best_row]/10
                &&
                col_energy[best_col]/10 > row_energy[best_row]/s->normal_twist)
            {

                for (i = 0; i < 4; i++)
                {
                    if ((i != best_col && col_energy[i]/10 > col_energy[best_col]/63)
                        ||
                        (i != best_row && row_energy[i]/10 > row_energy[best_row]/63))
                    {
                        break;
                    }
                }

                if (i >= 4
                    &&
                    (row_energy[best_row] + col_energy[best_col])/s->nEnergyRatio > s->energy/10)
                {

                    hit = dtmf_positions[(best_row << 2) + best_col];
                }
            }
            {
                /*printf(
                         "Potentially '%c' - total %.2fdB, row %.2fdB, col %.2fdB, duration %d - %s\n",
                         dtmf_positions[(best_row << 2) + best_col],
                         log10f(s->energy)*10.0f - 68.251f + (3.14f + 3.02f),
                         log10f(row_energy[best_row]/83.868f)*10.0f - 68.251f + (3.14f + 3.02f),
                         log10f(col_energy[best_col]/83.868f)*10.0f - 68.251f + (3.14f + 3.02f),
                         s->duration,
                         (hit) ? "hit" : "miss");*/
            }
        }
		//if (hit) printf("%c",hit);
		//else printf("-");
#if 0
        if (hit != s->in_digit && s->last_hit != s->in_digit)
        {
            hit = (hit && hit == s->last_hit) ? hit : 0;
            {
                if (hit)
                {
                    s->digits_callback(s->ch, hit, 0);
                    /*if (s->current_digits < 128)
                    {
                        s->digits[s->current_digits++] = (char) hit;
                        s->digits[s->current_digits] = '\0';
                        if (s->digits_callback)
                        {
                            s->digits_callback(s->ch, s->digits, s->current_digits);
                            s->current_digits = 0;
                        }
                    }
                    else
                    {
                        s->lost_digits++;
                    }*/
                }
            }
            s->in_digit = hit;
        }
        s->last_hit = hit;
        s->energy = 0;//((int16_t) (0.0f/128.0 + ((0.0f >= 0.0) ? 0.5 : -0.5)));
        s->current_sample = 0;
#endif

#if 0 // best match.
/*
 * Adapted from ETSI ES 201 235-3 V1.3.1 (2006-03)
 * (40ms reference is tunable with hits_to_begin and misses_to_end)
 * each hit/miss is 12.75ms with DTMF_GSIZE at 102
 *
 * Character recognition: When not DRC *(1) and then
 *      Shall exist VSC > 40 ms (hits_to_begin)
 *      May exist 20 ms <= VSC <= 40 ms
 *      Shall not exist VSC < 20 ms
 *
 * Character recognition: When DRC and then
 *      Shall cease Not VSC > 40 ms (misses_to_end)
 *      May cease 20 ms >= Not VSC >= 40 ms
 *      Shall not cease Not VSC < 20 ms
 *
 * *(1) or optionally a different digit recognition condition
 *
 * Legend: VSC The continuous existence of a valid signal condition.
 *      Not VSC The continuous non-existence of valid signal condition.
 *      DRC The existence of digit recognition condition.
 *      Not DRC The non-existence of digit recognition condition.
 */

/*
 * Example: hits_to_begin=2 misses_to_end=3
 * -------A last_hit=A hits=0&1
 * ------AA hits=2 current_hit=A misses=0       BEGIN A
 * -----AA- misses=1 last_hit=' ' hits=0
 * ----AA-- misses=2
 * ---AA--- misses=3 current_hit=' '            END A
 * --AA---B last_hit=B hits=0&1
 * -AA---BC last_hit=C hits=0&1
 * AA---BCC hits=2 current_hit=C misses=0       BEGIN C
 * A---BCC- misses=1 last_hit=' ' hits=0
 * ---BCC-C misses=0 last_hit=C hits=0&1
 * --BCC-CC misses=0
 *
 * Example: hits_to_begin=3 misses_to_end=2
 * -------A last_hit=A hits=0&1
 * ------AA hits=2
 * -----AAA hits=3 current_hit=A misses=0       BEGIN A
 * ----AAAB misses=1 last_hit=B hits=0&1
 * ---AAABB misses=2 current_hit=' ' hits=2     END A
 * --AAABBB hits=3 current_hit=B misses=0       BEGIN B
 * -AAABBBB misses=0
 *
 * Example: hits_to_begin=2 misses_to_end=2
 * -------A last_hit=A hits=0&1
 * ------AA hits=2 current_hit=A misses=0       BEGIN A
 * -----AAB misses=1 hits=0&1
 * ----AABB misses=2 current_hit=' ' hits=2 current_hit=B misses=0 BEGIN B
 * ---AABBB misses=0
 */

		if (s->current_hit) {
			/* We are in the middle of a digit already */
			if (hit != s->current_hit) {
				s->misses++;
				if (s->misses == dtmf_misses_to_end) {
					/* There were enough misses to consider digit ended */
					s->current_hit = 0;
				}
			} else {
				s->misses = 0;
				/* Current hit was same as last, so increment digit duration (of last digit) */
				s->digitlen[s->current_digits - 1] += 102;
			}
		}

		/* Look for a start of a new digit no matter if we are already in the middle of some
		   digit or not. This is because hits_to_begin may be smaller than misses_to_end
		   and we may find begin of new digit before we consider last one ended. */

		if (hit != s->lasthit) {
			s->lasthit = hit;
			s->hits = 0;
		}
		if (hit && hit != s->current_hit) {
			s->hits++;
			if (s->hits == s->minPositiveduration) {
				store_digit(s, hit);
				s->digitlen[s->current_digits - 1] = s->minPositiveduration * 102;
				s->current_hit = hit;
				s->misses = 0;
			}
		}
#endif

		/* Reinitialise the detector for the next block */
		for (i = 0; i < 4; i++) {
			goertzel_reset(&s->row_out[i]);
			goertzel_reset(&s->col_out[i]);
		}

		if (hit != s->lasthit) {
			s->lasthit = hit;
			//s->hits = 0;
		}
		s->hit_bit <<= 1;
		if (hit) {
			int cur = (best_row << 2) + best_col;
			s->hit_bit += 1;
			s->hit_s[cur]++;
			if (s->hit_s[cur] > s->hit_s[s->hit_max])
				s->hit_max = cur;
			
		}
		if (s->hit_bit > (1<<s->minNegativeduration) && !(s->hit_bit & ((1<<s->minNegativeduration)-1))){
			if (s->hit_s[s->hit_max] >= s->minPositiveduration)	{

                if (s->current_digits < MAX_DTMF_DIGITS){
                    s->digits[s->current_digits++] = dtmf_positions[s->hit_max];
                    s->digits[s->current_digits] = '\0';
                    if (s->digits_callback){
                        s->digits_callback(s->ch, dtmf_positions[s->hit_max], s->hit_s[s->hit_max]);
                        s->current_digits = 0;
                    }
                }
                else{
				    s->lost_digits++;
			    }
                
			}else{
				s->lost_digits++;
			}
			for (i=0;i<16;i++) s->hit_s[i] = 0;
			s->hit_bit = 0;			
		}
		s->energy = 0.0;
		s->current_sample = 0;
	}

	return (s->lasthit);	/* return the debounced hit */
/*
    }
    return 0;*/
}

size_t dtmf_rx_get(dtmf_rx_state_t *s, char *buf, int max)
{
    if (max > s->current_digits)
        max = s->current_digits;
    if (max > 0)
    {
        memcpy(buf, s->digits, max);
        memmove(s->digits, s->digits + max, s->current_digits - max);
        s->current_digits -= max;
    }
    buf[max] = '\0';
    return max;
}

dtmf_rx_state_t * dtmf_rx_init(dtmf_rx_state_t *s,
                                             dtmf_report_func_t callback,
                                             void *user_data)
{
    int i;
	

    if (s == ((void *)0))
    {
        if ((s = (dtmf_rx_state_t *) malloc(sizeof(*s))) == ((void *)0))
            return ((void *)0);
    }
    memset(s, 0, sizeof(*s));
    //span_log_init(&s->logging, SPAN_LOG_NONE, ((void *)0));
    //span_log_set_protocol(&s->logging, "DTMF");
    s->digits_callback = callback;
    s->digits_callback_data = user_data;
    //s->realtime_callback = ((void *)0);
    //s->realtime_callback_data = ((void *)0);
    //s->filter_dialtone = 0;
    s->normal_twist = 63;
    s->reverse_twist = 25;
    s->threshold = 165446; //-30dbm0
    s->minPositiveduration = dtmf_hits_to_begin;
    s->minNegativeduration = dtmf_misses_to_end;
	s->nEnergyRatio = 838;
    s->in_digit = 0;
    s->last_hit = 0;
	s->lasthit = 0;
	s->current_hit = 0;
	//s->energy = 0.0;
	//s->current_sample = 0;
	s->hits = 0;
	s->misses = 0;
	
    for (i = 0; i < 4; i++)
    {
        goertzel_init(&s->row_out[i], &dtmf_detect_row[i]);
        goertzel_init(&s->col_out[i], &dtmf_detect_col[i]);
    }
    s->energy = 0;//((int16_t) (0.0f/128.0 + ((0.0f >= 0.0) ? 0.5 : -0.5)));
	//printf("%s energy=%hd \n", __func__, s->energy);
    s->current_sample = 0;
    //s->lost_digits = 0;
    //s->current_digits = 0;
    //s->digits[0] = '\0';
	s->current_digits = 0;
	s->detected_digits = 0;
	s->lost_digits = 0;
	s->digits[0] = '\0';
	s->hit_bit = 0;
	s->hit_max = 0;
	for (i=0; i<16; i++)
		s->hit_s[i] = 0;
    return s;
}


int dtmf_rx_release(dtmf_rx_state_t *s)
{
    return 0;
}


int dtmf_rx_free(dtmf_rx_state_t *s)
{
    free(s);
    return 0;
}


#if 1
#ifdef __cplusplus
extern "C" {
#endif

void dtmf_detect_init(dtmf_rx_state_t *s, dtmf_report_func_t func, void * user_data)
{
    dtmf_rx_init(s, func, user_data);  
}

int dtmf_detect(dtmf_rx_state_t *s, short sample_buffer[], int samples)
{
    dtmf_rx(s, sample_buffer, samples);
    if (s->lasthit == 0) return 0;
    else return 1;
}


unsigned int dtmf_detect_lost(dtmf_rx_state_t *s)
{
    return s->lost_digits;
}

void dtmf_detect_setgpara(DtmfPara* para)
{
    memcpy(&gPara, para, sizeof(gPara));
}

void dtmf_detect_setpara(dtmf_rx_state_t *s, DtmfPara* para)
{
	int32_t positivetwist;
	int32_t negativetwist;
	int32_t levelminin;
	int32_t minPositiveduration,minNegativeduration;
	int32_t nEnergyRatio;
	positivetwist = (para->nPositiveTwist>=0 && para->nPositiveTwist<=24)?para->nPositiveTwist:5;
	negativetwist = (para->nNegativeTwist>=0 && para->nNegativeTwist<=24)?para->nNegativeTwist:9;
	levelminin = (para->nLevelMinIn<=0 && para->nLevelMinIn>= -42)?(-para->nLevelMinIn):20;
	minPositiveduration = (para->nMinPositiveDurationMs>=12 && para->nMinPositiveDurationMs<= 2000)?(para->nMinPositiveDurationMs):12;
	minPositiveduration /= 12;
	minNegativeduration = (para->nMinNegativeDurationMs>=12 && para->nMinNegativeDurationMs<= 2000)?(para->nMinNegativeDurationMs):12;
	minNegativeduration /= 12;
	nEnergyRatio = (para->nEnergyRatio>=10 && para->nEnergyRatio<= 1000)?(para->nEnergyRatio):838;
	
    s->reverse_twist = db_tbl[positivetwist];
    s->normal_twist = db_tbl[negativetwist];
    s->threshold = threshold_tbl[levelminin];	
	s->minPositiveduration = minPositiveduration;
	s->minNegativeduration = minNegativeduration;
	s->nEnergyRatio = nEnergyRatio;
		
	return;

}

int  dtmf_detect_get(dtmf_rx_state_t *s, char *buf, int max)
{
    int len=0;
    len =   (int)dtmf_rx_get(s, buf, max);
    return len;
}

#ifdef __cplusplus
};
#endif
#endif
