#ifndef _dtmf_detect_H_
#define _dtmf_detect_H_

#ifndef MAX_CH
#define MAX_CH                      64
#endif

#ifndef MAX_DTMF_DIGITS
#define MAX_DTMF_DIGITS 128
#endif
//#if !defined(_WIN32) && !defined(_WIN64)
//typedef long          int64_t;
//#else
//typedef __int64       int64_t;
//#endif

typedef struct dtmf_rx_state_s dtmf_rx_state_t;
typedef void (*dtmf_report_func_t)(int ch, char digit, unsigned int duration);

struct goertzel_descriptor_s
{
    int16_t fac;
    int samples;
};

struct goertzel_state_s
{

    int16_t v2;
    int16_t v3;
    int16_t fac;

    int samples;
    int current_sample;
};

typedef struct goertzel_descriptor_s goertzel_descriptor_t;
typedef struct goertzel_state_s goertzel_state_t;

struct dtmf_rx_state_s
{

    dtmf_report_func_t digits_callback;

    void *digits_callback_data;

    int32_t normal_twist;

    int32_t reverse_twist;

    int32_t threshold;

    int32_t energy;
    goertzel_state_t row_out[4];

    goertzel_state_t col_out[4];

    uint8_t last_hit;

    uint8_t in_digit;

	int hits;			/* How many successive hits we have seen already */
	int misses;			/* How many successive misses we have seen already */
	int lasthit;
	int current_hit;
    int current_sample;


    int duration;

	char digits[MAX_DTMF_DIGITS + 1];
	int digitlen[MAX_DTMF_DIGITS + 1];
	int current_digits;
	int detected_digits;
	unsigned int lost_digits;
	uint32_t hit_bit;
	int hit_max;
	int hit_s[16];
/*
    int lost_digits;

    int current_digits;

    char digits[MAX_DTMF_DIGITS + 1];*/
	int minPositiveduration;
    int minNegativeduration;
    int nEnergyRatio;
	int ch;
};


typedef struct
{ 
   int  nPositiveTwist;       // 0..24         5
   int  nNegativeTwist;       // 0..24         9
   int  nMinPositiveDurationMs;       // 10..2000      23
   int  nMinNegativeDurationMs;       // 10..2000      60
   int  nEnergyRatio;         //10..1000   838
   int  nMaxInterruptionMs;   // 0..20         10
   int  nDeviationIn;         // 10..50        50
   int  nDeviationOut;        // 10..100       60
   int  nLevelMinIn;          // -40..-9       -40
   int  nLevelMinOut;         // -40..-15      -40
   int  nSnrMinIn;            // -9..0         -3
   int  nSnrMinOut;           // -9..0         -3
   int  nTwist;               // -24..24       0
} DtmfPara;



#ifdef __cplusplus
extern "C" {
#endif

void    dtmf_detect_init(dtmf_rx_state_t *s, dtmf_report_func_t func, void * user_data);
int     dtmf_detect(dtmf_rx_state_t *s, short sample_buffer[], int samples);
void    dtmf_detect_setpara(dtmf_rx_state_t *s, DtmfPara* para);
void    dtmf_detect_setgpara(DtmfPara* para);
unsigned int dtmf_detect_lost(dtmf_rx_state_t *s);
int     dtmf_detect_get(dtmf_rx_state_t *s, char *buf, int max);

#ifdef __cplusplus
};
#endif

#endif
