#ifndef _fsk_detect_H_
#define _fsk_detect_H_

#ifndef MAX_CH
#define MAX_CH                      64
#endif


#define SPANDSP_USE_FIXED_POINT

/* The longest window will probably be 106 for 75 baud */
#define FSK_MAX_WINDOW_LEN          128

/* This is based on A-law, but u-law is only 0.03dB different */
#ifndef SPANDSP_USE_FIXED_POINT
#define DBM0_MAX_POWER              (3.14f + 3.02f)
#endif
/* This is based on the ITU definition of dbOv in G.100.1 */
#ifndef SAMPLE_RATE
#define SAMPLE_RATE                 8000
#endif

#define SLENK                       8
#define DDS_STEPS                   (1 << SLENK)
#define DDS_SHIFT                   (32 - 2 - SLENK)

/*!
    Complex 32 bit integer type.
*/
typedef struct
{
    /*! \brief Real part. */
    int32_t re;
    /*! \brief Imaginary part. */
    int32_t im;
} complexi32_t;


/*!
    Complex integer type.
*/
typedef struct
{
    /*! \brief Real part. */
    int re;
    /*! \brief Imaginary part. */
    int im;
} complexi_t;


typedef struct
{
    /*! Short text name for the modem. */
    const char *name;
    /*! The frequency of the zero bit state, in Hz */
    int freq_zero;
    /*! The frequency of the one bit state, in Hz */
    int freq_one;
    /*! The transmit power level, in dBm0 */
    int tx_level;
    /*! The minimum acceptable receive power level, in dBm0 */
    int min_level;
    /*! The bit rate of the modem, in units of 1/100th bps */
    int baud_rate;
} fsk_spec_t;


/*!
    Power meter descriptor. This defines the working state for a
    single instance of a power measurement device.
*/
struct power_meter_s
{
    /*! The shift factor, which controls the damping of the power meter. */
    int shift;

    /*! The current power reading. */
    int32_t reading;
};

typedef void (*fsk_report_func_t)(void *user_data, int bit);
typedef void (*fsk_report_func_ex_t)(void *user_data, unsigned char fsk);

typedef struct power_meter_s power_meter_t;

typedef void (*fsk_status_func_t)(int ch, int status);

typedef enum { fskIdle, fskPre11, fskPre10, fskPre20, fskPre2, fskData0, fskData1, fskData, fskEnd0, fskEnd1 } fsk_detect_state_t;

/*!
    FSK modem receive descriptor. This defines the state of a single working
    instance of an FSK modem receiver.
*/
struct fsk_rx_state_s
{
    int baud_rate;
    /*! \brief Synchronous/asynchronous framing control */
    int framing_mode;
    /*! \brief The callback function used to put each bit received. */
    fsk_report_func_t put_bit;
    /*! \brief A user specified opaque pointer passed to the put_bit routine. */
    void *put_bit_user_data;

    /*! \brief The callback function used to report modem status changes. */
    fsk_status_func_t status_handler;
    /*! \brief A user specified opaque pointer passed to the status function. */
    int status_user_data;

    int32_t carrier_on_power;
    int32_t carrier_off_power;
    power_meter_t power;
    /*! \brief The value of the last signal sample, using the a simple HPF for signal power estimation. */
    int16_t last_sample;
    /*! \brief >0 if a signal above the minimum is present. It may or may not be a V.29 signal. */
    int signal_present;

    int32_t phase_rate[2];
    uint32_t phase_acc[2];

    int correlation_span;

    complexi32_t window[2][FSK_MAX_WINDOW_LEN];
    complexi32_t dot[2];
    int buf_ptr;

    int frame_state;
    unsigned int frame_bits;
    int baud_phase;
    int last_bit;
    int scaling_shift;
    
    fsk_detect_state_t     state;
    unsigned char   fsk;
    int             fsk_idx;
    int             pre1_count;
    int             pre2_count;
    int             end_count;
    unsigned char   fsks[256];
    int             fsk_count;
    int             fsk_count_old;
    int             fskcurintcnt;
    int             fskintcnt0;
    int             c_min_pre1_count;
    int             c_min_pre2_count;
    int             c_min_end_count;    
    fsk_report_func_ex_t    funcex;  
    void *funcex_user_data;  
    int             fsk_hdr;
    int             fsk_hdr_len;
    int             fsk_data_len;
};

typedef struct
{
	int nFSK_FREQ_ZERO;
	int nFSK_FREQ_ONE;
	int nFSK_MIN_LEVEL;
	int nFSK_BAUD_RATE;
	int nFSK_PRE1_LEN;
	int nFSK_PRE2_LEN;
	int nFSK_END_LEN;
} FskPara;

/*!
    FSK modem receive descriptor. This defines the state of a single working
    instance of an FSK modem receiver.
*/
typedef struct fsk_rx_state_s fsk_rx_state_t;


#ifdef __cplusplus
extern "C" {
#endif

void    fsk_detect_init(fsk_rx_state_t *s, fsk_spec_t *spec, int freq_zero, int freq_one, int min_level, int baud_rate, fsk_report_func_t func);
int     fsk_detect(fsk_rx_state_t *s, short sample_buffer[], int samples);
void    fsk_detect_reset(fsk_rx_state_t *s, fsk_spec_t *spec);

void    fsk_detect_set_param(fsk_rx_state_t *s, int min_pre1, int min_pre2, int min_end, fsk_report_func_ex_t func, void *user_data);
int     fsk_detect_get_data(fsk_rx_state_t *s, fsk_spec_t *spec, unsigned char *fsk_buf);
int     fsk_detect_data_analyze(unsigned char *  fsk_buf, int fsk_len, unsigned char * caller_id, unsigned char * date_time, unsigned char * caller_name);

#ifdef __cplusplus
};
#endif

#endif
