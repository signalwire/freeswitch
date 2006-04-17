/*******************************************************
 * jitterbuffer: 
 * an application-independent jitterbuffer, which tries 
 * to achieve the maximum user perception during a call.
 * For more information look at:
 * http://www.speakup.nl/opensource/jitterbuffer/
 *
 * Copyright on this file is held by:
 * - Jesse Kaijen <jesse@speakup.nl>  
 * - SpeakUp <info@speakup.nl>
 *
 * Contributors:
 * Jesse Kaijen <jesse@speakup.nl>
 *
 * Version: 1.1
 * 
 * Changelog:
* 1.0 => 1.1 (2006-03-24) (thanks to Micheal Jerris, freeswitch.org)
 * - added MSVC 2005 project files
 * - added JB_NOJB as return value
 *
 *
 * This program is free software, distributed under the terms of:
 * - the GNU Lesser (Library) General Public License
 * - the Mozilla Public License
 * 
 * if you are interested in an different licence type, please contact us.
 *
 * How to use the jitterbuffer, please look at the comments 
 * in the headerfile.
 *
 * Further details on specific implementations, 
 * please look at the comments in the code file.
 */

#ifndef _JITTERBUFFER_H_
#define _JITTERBUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

/***********
 * The header file consists of four parts.
 * - configuration constants, structs and parameter definitions
 * - functions
 * - How to use the jitterbuffer and 
 *   which responsibilities do YOU have
 * - debug messages explained
 */


// configuration constants
/* Number of historical timestamps to use in calculating jitter and jitterbuffer size */ #define JB_HISTORY_SIZE 500
/* minimum jitterbuffer size, disabled if 0 */ #define JB_MIN_SIZE 0
/* maximum jitterbuffer size, disabled if 0 */ #define JB_MAX_SIZE 0
 /* maximum successive interpolating frames, disabled if 0 */ #define JB_MAX_SUCCESSIVE_INTERP 0
/* amount of extra delay allowed before shrinking */
#define JB_ALLOW_EXTRA_DELAY 30																														
/* ms between growing */
#define JB_WAIT_GROW 60
/* ms between shrinking */
#define JB_WAIT_SHRINK 250
/* ms that the JB max may be off */
#define JB_MAX_DIFF 6000 //in a RTP stream the max_diff may be 3000 packets (most packets are 20ms)

//structs
typedef struct jb_info {
	long frames_received;       /* Number of frames received by the jitterbuffer */
  long frames_late;           /* Number of frames that were late */
  long frames_lost;           /* Number of frames that were lost */
  long frames_ooo;            /* Number of frames that were Out Of Order */
  long frames_dropped;        /* Number of frames that were dropped due shrinkage of the jitterbuffer */
  long frames_dropped_twice;  /* Number of frames that were dropped because this timestamp was already in the jitterbuffer */
                              
  long delay;     /* Current delay due the jitterbuffer */
	long jitter; 		/* jitter measured within current history interval*/
	long losspct; 	/* recent lost frame percentage (network and jitterbuffer loss) */
	                            
	long delay_target;   /* The delay where we want to grow to */
	long losspct_jb;     /* recent lost percentage due the jitterbuffer */
	long last_voice_ms;	 /* the duration of the last voice frame */
  short silence;       /* If we are in silence 1-yes 0-no */
  long iqr;            /* Inter Quartile Range of current history, if the squareroot is taken it is a good estimate of jitter */
} jb_info;

typedef struct jb_frame {
	void *data;		                /* the frame data */
	long ts;	                    /* the senders timestamp */
	long ms;	                    /* length of this frame in ms */
	int  type;	                  /* the type of frame */
	int codec;                    /* codec of this frame, undefined if nonvoice */
	struct jb_frame *next, *prev; /* pointers to the next and previous frames in the queue */ } jb_frame;

typedef struct jb_hist_element {
	long delay; /* difference between time of arrival and senders timestamp */
	long ts;    /* senders timestamp */
	long ms;    /* length of this frame in ms */
	int codec;  /* wich codec this frame has */
} jb_hist_element;								

typedef struct jb_settings {
  /* settings */
	long min_jb;	              /* defines a hard clamp to use in setting the jitterbuffer delay */
  long max_jb;	              /* defines a hard clamp to use in setting the jitterbuffer delay */
  long max_successive_interp; /* the maximum count of successive interpolations before assuming silence */
  long extra_delay;           /* amount of extra delay allowed before shrinking */
  long wait_grow;             /* ms between growing */
  long wait_shrink;           /* ms between shrinking */
  long max_diff;              /* maximum number of milliseconds the jitterbuffer may be off */
} jb_settings;

typedef struct jitterbuffer {
	struct jb_hist_element hist[JB_HISTORY_SIZE]; /* the history of the last received frames */
	long hist_sorted_delay[JB_HISTORY_SIZE];      /* a sorted buffer of the delays (lowest first) */
	long hist_sorted_timestamp[JB_HISTORY_SIZE];  /* a sorted buffer of the timestamps (lowest first) */
	
	int  hist_pointer;          /* points to index in history for next entry */
	long last_adjustment;       /* the time of the last adjustment (growing or shrinking) */
  long next_voice_time;	      /* the next ts is to be read from the jb (senders timestamp) */
	long cnt_successive_interp; /* the count of consecutive interpolation frames */	
	long silence_begin_ts;      /* the time of the last CNG frame, when in silence */
	long min;		                /* the clock difference within current history interval */
	long current; 		          /* the present jitterbuffer adjustment */
	long target; 		            /* the target jitterbuffer adjustment */
	long last_delay;            /* the delay of the last packet, used for calc. jitter */
	                            
	jb_frame *voiceframes; 	 /* queued voiceframes */
	jb_frame *controlframes; /* queued controlframes */
	jb_settings settings;    /* the settings of the jitterbuffer */
	jb_info info;            /* the statistics of the jitterbuffer */
} jitterbuffer;

//parameter definitions
/* return codes */
#define JB_OK		0
#define JB_EMPTY	1
#define JB_NOFRAME	2
#define JB_INTERP	3
#define JB_NOJB		4


/* frame types */
#define JB_TYPE_CONTROL	1
#define JB_TYPE_VOICE	2
#define JB_TYPE_SILENCE	3

/* the jitterbuffer behaives different for each codec. */
/* Look in the code if a codec has his function defined */
/* default is g711x behaiviour */
#define JB_CODEC_SPEEX 10       //NOT defined
#define JB_CODEC_ILBC 9         //NOT defined
#define JB_CODEC_GSM_EFR 8
#define JB_CODEC_GSM_FR 7       //NOT defined
#define JB_CODEC_G723_1 6
#define JB_CODEC_G729A 5
#define JB_CODEC_G729 4
#define JB_CODEC_G711x_PLC 3
#define JB_CODEC_G711x 2
#define JB_CODEC_OTHER 1        //NOT defined


/*
 * Creates a new jitterbuffer and sets the default settings.
 * Always use this function for creating a new jitterbuffer. 
 */
jitterbuffer *jb_new();

/*
 * The control frames and possible personal settings are kept. 
 * History and voice/silence frames are destroyed. 
 */
void jb_reset(jitterbuffer *jb);

/*
 * Resets the jitterbuffer totally, all the control/voice/silence frames are destroyed
 * default settings are put as well. 
 */
void jb_reset_all(jitterbuffer *jb);

/*
 * Destroy the jitterbuffer and any frame within. 
 * Always use this function for destroying a jitterbuffer,
 * otherwise there is a chance of memory leaking.
 */
void jb_destroy(jitterbuffer *jb);

/*
 * Define your own settings for the jitterbuffer. Only settings !=0
 * are put in the jitterbuffer.
 */
void jb_set_settings(jitterbuffer *jb, jb_settings *settings);

/*
 * Get the statistics for the jitterbuffer. 
 * Copying the statistics directly for the jitterbuffer won't work because
 * The statistics are only calculated when calling this function.
 */
void jb_get_info(jitterbuffer *jb, jb_info *stats);

/*
 * Get the current settings of the jitterbuffer.
 */
void jb_get_settings(jitterbuffer *jb, jb_settings *settings);

/*
 * Gives an estimation of the MOS of a call given the
 * packetloss p, delay d, and wich codec is used.
 * The assumption is made that the echo cancelation is around 37dB.
 */
float jb_guess_mos(float p, long d, int codec);

/*
 * returns JB_OK if there are still frames left in the jitterbuffer
 * otherwise JB_EMPTY is returned.
 */
int jb_has_frames(jitterbuffer *jb);

/*
 * put a packet(frame) into the jitterbuffer.
 * *data - points to the packet
 * type - type of packet, JB_CONTROL|JB_VOICE|JB_SILENCE
 * ms - duration of frame (only voice)
 * ts - timestamp sender
 * now - current timestamp (timestamp of arrival)
 * codec - which codec the frame holds (only voice), if not defined, g711x will be used
 *
 * if type==control @REQUIRE: *data, type, ts, now
 * if type==voice   @REQUIRE: *data, type, ms, ts, now @OPTIONAL: codec
 * if type==silence @REQUIRE: *data, type, ts, now
 * on return *data is undefined
 */
void jb_put(jitterbuffer *jb, void *data, int type, long ms, long ts, long now, int codec);

/*
 * Get a packet from the jitterbuffer if it's available.
 * control packets have a higher priority above voice and silence packets
 * they are always delivered as fast as possible. The delay of the jitterbuffer
 * doesn't work for these packets. 
 * @REQUIRE 1<interpl <= jb->settings->extra_delay (=default JB_ALLOW_EXTRA_DELAY)
 *
 * return will be:
 * JB_OK, *data points to the packet
 * JB_INTERP, please interpolate for interpl milliseconds
 * JB_NOFRAME, no frame scheduled
 * JB_EMPTY, the jitterbuffer is empty
 */
int jb_get(jitterbuffer *jb, void **data, long now, long interpl);

/* debug functions */
typedef 		void (*jb_output_function_t)(const char *fmt, ...);
void 			jb_setoutput(jb_output_function_t warn, jb_output_function_t err, jb_output_function_t dbg);


/*******************************
 * The use of the jitterbuffer *
 *******************************
 * Always create a new jitterbuffer with jb_new().
 * Always destroy a jitterbuffer with jb_destroy().
 *
 * There is no lock(mutex) mechanism, that your responsibility.
 * The reason for this is that different environments require
 * different ways of implementing a lock.
 *
 * The following functions require a lock on the jitterbuffer:
 * jb_reset(), jb_reset_all(), jb_destroy(), jb_set_settings(),
 * jb_get_info(), jb_get_settings(), jb_has_frames(), jb_put(),
 * jb_get()
 *
 * The following functions do NOT require a lock on the jitterbuffer:
 * jb_new(), jb_guess_mos()
 *
 * Since control packets have a higher priority above any other packet
 * a call may already be ended while there is audio left to play. We
 * advice that you poll the jitterbuffer if there are frames left.
 *
 * If the audiopath is oneway (eg. voicemailbox) and the latency doesn't
 * matter, we advice to set a minimum jitterbuffer size. Then there is
 * less loss and the quality is better.
 */


/****************************
 * debug messages explained *
 ****************************
 * N  - jb_new()
 * R  - jb_reset()
 * r  - jb_reset_all()
 * D  - jb_destroy()
 * S  - jb_set_settings()
 * H  - jb_has_frames()
 * I  - jb_get_info()
 * S  - jb_get_settings()
 * pC - jb_put() put Control packet
 * pT - jb_put() Timestamp was already in the queue
 * pV - jb_put() put Voice packet
 * pS - jb_put() put Silence packet
 *
 * A  - jb_get()
 * // below are all the possible debug info when trying to get a packet
 * gC - get_control() - there is a control message
 * gs - get_voice() - there is a silence frame
 * gS - get_voice() - we are in silence
 * gL - get_voice() - are in silence, frame is late
 * gP - get_voice() - are in silence, play frame (end of silence)
 * ag - get_voicecase() - grow little bit (diff < interpl/2)
 * aG - get_voicecase() - grow interpl
 * as - get_voicecase() - shrink by voiceframe we throw out
 * aS - get_voicecase() - shrink by interpl
 * aN - get_voicecase() - no time yet
 * aL - get_voicecase() - frame is late
 * aP - get_voicecase() - play frame
 * aI - get_voicecase() - interpolate
 */

#ifdef __cplusplus
}
#endif


#endif

