/*****************************************************************************
* wanpipe_tdm_api_iface.h 
* 		
* 		WANPIPE(tm) AFT TE1 Hardware Support
*
* Authors: 	Nenad Corbic <ncorbic@sangoma.com>
*
* Copyright (c) 2007 - 08, Sangoma Technologies
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the <organization> nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY <copyright holder> ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

* ============================================================================
* Oct 04, 2005	Nenad Corbic	Initial version.
*
* Jul 25, 2006	David Rokhvarg	<davidr@sangoma.com>	Ported to Windows.
*****************************************************************************/

#ifndef __WANPIPE_TDM_API_IFACE_H_
#define __WANPIPE_TDM_API_IFACE_H_ 


#if defined(__WINDOWS__)
typedef HANDLE sng_fd_t;
#else
typedef int sng_fd_t;
#endif

/* Indicate to library that new features exist */
#define WP_TDM_FEATURE_DTMF_EVENTS	1
#define WP_TDM_FEATURE_FE_ALARM		1
#define WP_TDM_FEATURE_EVENTS		1
#define WP_TDM_FEATURE_LINK_STATUS	1

enum wanpipe_tdm_api_cmds {

	SIOC_WP_TDM_GET_USR_MTU_MRU,	/* 0x00 */

	SIOC_WP_TDM_SET_USR_PERIOD,	/* 0x01 */
	SIOC_WP_TDM_GET_USR_PERIOD,	/* 0x02 */
	
	SIOC_WP_TDM_SET_HW_MTU_MRU,	/* 0x03 */
	SIOC_WP_TDM_GET_HW_MTU_MRU,	/* 0x04 */

	SIOC_WP_TDM_SET_CODEC,		/* 0x05 */
	SIOC_WP_TDM_GET_CODEC,		/* 0x06 */

	SIOC_WP_TDM_SET_POWER_LEVEL,	/* 0x07 */
	SIOC_WP_TDM_GET_POWER_LEVEL,	/* 0x08 */

	SIOC_WP_TDM_TOGGLE_RX,		/* 0x09 */
	SIOC_WP_TDM_TOGGLE_TX,		/* 0x0A */

	SIOC_WP_TDM_GET_HW_CODING,	/* 0x0B */
	SIOC_WP_TDM_SET_HW_CODING,	/* 0x0C */

	SIOC_WP_TDM_GET_FULL_CFG,	/* 0x0D */

	SIOC_WP_TDM_SET_EC_TAP,		/* 0x0E */
	SIOC_WP_TDM_GET_EC_TAP,		/* 0x0F */
	
	SIOC_WP_TDM_ENABLE_RBS_EVENTS,	/* 0x10 */
	SIOC_WP_TDM_DISABLE_RBS_EVENTS,	/* 0x11 */
	SIOC_WP_TDM_WRITE_RBS_BITS,	/* 0x12 */
	
	SIOC_WP_TDM_GET_STATS,		/* 0x13 */
	SIOC_WP_TDM_FLUSH_BUFFERS,	/* 0x14 */
	
	SIOC_WP_TDM_READ_EVENT,		/* 0x15 */
	
	SIOC_WP_TDM_SET_EVENT,		/* 0x16 */

	SIOC_WP_TDM_SET_RX_GAINS,	/* 0x17 */
	SIOC_WP_TDM_SET_TX_GAINS,	/* 0x18 */
	SIOC_WP_TDM_CLEAR_RX_GAINS,	/* 0x19 */
	SIOC_WP_TDM_CLEAR_TX_GAINS,	/* 0x1A */

	SIOC_WP_TDM_GET_FE_ALARMS,	/* 0x1B */

	SIOC_WP_TDM_ENABLE_HWEC,	/* 0x1C */
	SIOC_WP_TDM_DISABLE_HWEC,	/* 0x1D */
	
	SIOC_WP_TDM_SET_FE_STATUS,	/* 0x1E */
	SIOC_WP_TDM_GET_FE_STATUS,	/* 0x1F */

	SIOC_WP_TDM_GET_HW_DTMF,	/* 0x20 */

	SIOC_WP_TDM_NOTSUPP		/*  */

};

#define SIOC_WP_TDM_GET_LINK_STATUS SIOC_WP_TDM_GET_FE_STATUS

enum wanpipe_tdm_api_events {
	WP_TDMAPI_EVENT_NONE,
	WP_TDMAPI_EVENT_RBS,
	WP_TDMAPI_EVENT_ALARM,
	WP_TDMAPI_EVENT_DTMF,
	WP_TDMAPI_EVENT_RM_DTMF,
	WP_TDMAPI_EVENT_RXHOOK,
	WP_TDMAPI_EVENT_RING,
	WP_TDMAPI_EVENT_RING_DETECT,
	WP_TDMAPI_EVENT_RING_TRIP_DETECT,
	WP_TDMAPI_EVENT_TONE,
	WP_TDMAPI_EVENT_TXSIG_KEWL,
	WP_TDMAPI_EVENT_TXSIG_START,
	WP_TDMAPI_EVENT_TXSIG_OFFHOOK,
	WP_TDMAPI_EVENT_TXSIG_ONHOOK,
	WP_TDMAPI_EVENT_ONHOOKTRANSFER,
	WP_TDMAPI_EVENT_SETPOLARITY,
	WP_TDMAPI_EVENT_BRI_CHAN_LOOPBACK,
	WP_TDMAPI_EVENT_LINK_STATUS
};

#define WP_TDMAPI_EVENT_FE_ALARM WP_TDMAPI_EVENT_ALARM


#define WP_TDMAPI_EVENT_ENABLE		0x01
#define WP_TDMAPI_EVENT_DISABLE		0x02
#define WP_TDMAPI_EVENT_MODE_DECODE(mode)				\
		((mode) == WP_TDMAPI_EVENT_ENABLE) ? "Enable" :	\
		((mode) == WP_TDMAPI_EVENT_DISABLE) ? "Disable" :	\
						"(Unknown mode)"

#define WPTDM_A_BIT 			WAN_RBS_SIG_A
#define WPTDM_B_BIT 			WAN_RBS_SIG_B
#define WPTDM_C_BIT 			WAN_RBS_SIG_C
#define WPTDM_D_BIT 			WAN_RBS_SIG_D
 
#define WP_TDMAPI_EVENT_RXHOOK_OFF	0x01
#define WP_TDMAPI_EVENT_RXHOOK_ON	0x02
#define WP_TDMAPI_EVENT_RXHOOK_DECODE(state)				\
		((state) == WP_TDMAPI_EVENT_RXHOOK_OFF) ? "Off-hook" :	\
		((state) == WP_TDMAPI_EVENT_RXHOOK_ON) ? "On-hook" :	\
						"(Unknown state)"

#define WP_TDMAPI_EVENT_RING_PRESENT	0x01
#define WP_TDMAPI_EVENT_RING_STOP	0x02
#define WP_TDMAPI_EVENT_RING_DECODE(state)				\
		((state) == WP_TDMAPI_EVENT_RING_PRESENT) ? "Ring Present" :	\
		((state) == WP_TDMAPI_EVENT_RING_STOP) ? "Ring Stop" :	\
						"(Unknown state)"

#define WP_TDMAPI_EVENT_RING_TRIP_PRESENT	0x01
#define WP_TDMAPI_EVENT_RING_TRIP_STOP	0x02
#define WP_TDMAPI_EVENT_RING_TRIP_DECODE(state)				\
		((state) == WP_TDMAPI_EVENT_RING_TRIP_PRESENT) ? "Ring Present" :	\
		((state) == WP_TDMAPI_EVENT_RING_TRIP_STOP) ? "Ring Stop" :	\
						"(Unknown state)"
/*Link Status */
#define WP_TDMAPI_EVENT_LINK_STATUS_CONNECTED		0x01
#define WP_TDMAPI_EVENT_LINK_STATUS_DISCONNECTED	0x02
#define WP_TDMAPI_EVENT_LINK_STATUS_DECODE(status)					\
		((status) == WP_TDMAPI_EVENT_LINK_STATUS_CONNECTED) ? "Connected" :		\
		((status) == WP_TDMAPI_EVENT_LINK_STATUS_DISCONNECTED)  ? "Disconnected" :		\
							"Unknown"
#define	WP_TDMAPI_EVENT_TONE_DIAL	0x01
#define	WP_TDMAPI_EVENT_TONE_BUSY	0x02
#define	WP_TDMAPI_EVENT_TONE_RING	0x03
#define	WP_TDMAPI_EVENT_TONE_CONGESTION	0x04

/* BRI channels list */						
#define	WAN_BRI_BCHAN1		0x01
#define	WAN_BRI_BCHAN2		0x02
#define	WAN_BRI_DCHAN		0x03


typedef struct {

	u_int8_t	type;
	u_int8_t	mode;
	u_int32_t	time_stamp;
	u_int8_t	channel;
	u_int32_t	chan_map;
	u_int8_t	span;
	union {
		struct {
			u_int8_t	alarm;
		} te1_alarm;
		struct {
			u_int8_t	rbs_bits;
		} te1_rbs;
		struct {
			u_int8_t	state;
			u_int8_t	sig;
		} rm_hook;
		struct {
			u_int8_t	state;
		} rm_ring;
		struct {
			u_int8_t	type;
		} rm_tone;
		struct {
			u_int8_t	digit;	/* DTMF: digit  */
			u_int8_t	port;	/* DTMF: SOUT/ROUT */
			u_int8_t	type;	/* DTMF: PRESET/STOP */
		} dtmf;
		struct {
			u_int16_t	polarity;
			u_int16_t	ohttimer;
		} rm_common;
		struct{
			u_int16_t status;
		} linkstatus;
	} wp_tdm_api_event_u;
#define wp_tdm_api_event_type 		type
#define wp_tdm_api_event_mode 		mode
#define wp_tdm_api_event_alarm 		wp_tdm_api_event_u.te1_alarm.alarm
#define wp_tdm_api_event_alarm 		wp_tdm_api_event_u.te1_alarm.alarm
#define wp_tdm_api_event_rbs_bits 	wp_tdm_api_event_u.te1_rbs.rbs_bits
#define wp_tdm_api_event_hook_state 	wp_tdm_api_event_u.rm_hook.state
#define wp_tdm_api_event_hook_sig 	wp_tdm_api_event_u.rm_hook.sig
#define wp_tdm_api_event_ring_state 	wp_tdm_api_event_u.rm_ring.state
#define wp_tdm_api_event_tone_type 	wp_tdm_api_event_u.rm_tone.type
#define wp_tdm_api_event_dtmf_digit 	wp_tdm_api_event_u.dtmf.digit
#define wp_tdm_api_event_dtmf_type 	wp_tdm_api_event_u.dtmf.type
#define wp_tdm_api_event_dtmf_port 	wp_tdm_api_event_u.dtmf.port
#define wp_tdm_api_event_ohttimer 	wp_tdm_api_event_u.rm_common.ohttimer
#define wp_tdm_api_event_polarity 	wp_tdm_api_event_u.rm_common.polarity
#define wp_tdm_api_event_link_status	wp_tdm_api_event_u.linkstatus.status
} wp_tdm_api_event_t;

typedef struct {
	union {
		unsigned char	reserved[16];
	}wp_rx_hdr_u;
} wp_tdm_api_rx_hdr_t;

typedef struct {
        wp_tdm_api_rx_hdr_t	hdr;
        unsigned char  		data[1];
} wp_tdm_api_rx_element_t;

typedef struct {
	union {
		struct {
			unsigned char	_rbs_rx_bits;
			unsigned int	_time_stamp;
		}wp_tx;
		unsigned char	reserved[16];
	}wp_tx_hdr_u;
#define wp_api_time_stamp 	wp_tx_hdr_u.wp_tx._time_stamp
} wp_tdm_api_tx_hdr_t;

typedef struct {
        wp_tdm_api_tx_hdr_t	hdr;
        unsigned char  		data[1];
} wp_tdm_api_tx_element_t;



typedef struct wp_tdm_chan_stats
{
	unsigned int	rx_packets;		/* total packets received	*/
	unsigned int	tx_packets;		/* total packets transmitted	*/
	unsigned int	rx_bytes;		/* total bytes received 	*/
	unsigned int	tx_bytes;		/* total bytes transmitted	*/
	unsigned int	rx_errors;		/* bad packets received		*/
	unsigned int	tx_errors;		/* packet transmit problems	*/
	unsigned int	rx_dropped;		/* no space in linux buffers	*/
	unsigned int	tx_dropped;		/* no space available in linux	*/
	unsigned int	multicast;		/* multicast packets received	*/
#if !defined(__WINDOWS__)
	unsigned int	collisions;
#endif
	/* detailed rx_errors: */
	unsigned int	rx_length_errors;
	unsigned int	rx_over_errors;		/* receiver ring buff overflow	*/
	unsigned int	rx_crc_errors;		/* recved pkt with crc error	*/
	unsigned int	rx_frame_errors;	/* recv'd frame alignment error */
#if !defined(__WINDOWS__)
	unsigned int	rx_fifo_errors;		/* recv'r fifo overrun		*/
#endif
	unsigned int	rx_missed_errors;	/* receiver missed packet	*/

	/* detailed tx_errors */
#if !defined(__WINDOWS__)
	unsigned int	tx_aborted_errors;
	unsigned int	tx_carrier_errors;
#endif
	unsigned int	tx_fifo_errors;
	unsigned int	tx_heartbeat_errors;
	unsigned int	tx_window_errors;
	
}wp_tdm_chan_stats_t;          


 
typedef struct wanpipe_tdm_api_cmd{
	unsigned int cmd;
	unsigned int hw_tdm_coding;	/* Set/Get HW TDM coding: uLaw muLaw */
	unsigned int hw_mtu_mru;	/* Set/Get HW TDM MTU/MRU */
	unsigned int usr_period;	/* Set/Get User Period in ms */
	unsigned int tdm_codec;		/* Set/Get TDM Codec: SLinear */
	unsigned int power_level;	/* Set/Get Power level treshold */
	unsigned int rx_disable;	/* Enable/Disable Rx */
	unsigned int tx_disable;	/* Enable/Disable Tx */		
	unsigned int usr_mtu_mru;	/* Set/Get User TDM MTU/MRU */
	unsigned int ec_tap;		/* Echo Cancellation Tap */
	unsigned int rbs_poll;		/* Enable/Disable RBS Polling */
	unsigned int rbs_rx_bits;	/* Rx RBS Bits */
	unsigned int rbs_tx_bits;	/* Tx RBS Bits */
	unsigned int hdlc;			/* HDLC based device */
	unsigned int idle_flag;		/* IDLE flag to Tx */
	unsigned int fe_alarms;		/* FE Alarms detected */
	wp_tdm_chan_stats_t stats;	/* TDM Statistics */
	/* Do NOT add anything above this! Important for binary backward compatibility. */
	wp_tdm_api_event_t event;	/* TDM Event */
	unsigned int data_len;
        void *data;	
	unsigned char fe_status;	/* FE status - Connected or Disconnected */
	unsigned int hw_dtmf;		/* HW DTMF enabled */
}wanpipe_tdm_api_cmd_t;

typedef struct wanpipe_tdm_api_event{
	int (*wp_rbs_event)(sng_fd_t fd, unsigned char rbs_bits);
	int (*wp_dtmf_event)(sng_fd_t fd, unsigned char dtmf, unsigned char type, unsigned char port);
	int (*wp_rxhook_event)(sng_fd_t fd, unsigned char hook_state);
	int (*wp_ring_detect_event)(sng_fd_t fd, unsigned char ring_state);
	int (*wp_ring_trip_detect_event)(sng_fd_t fd, unsigned char ring_state);
	int (*wp_fe_alarm_event)(sng_fd_t fd, unsigned char fe_alarm_event);
	int (*wp_link_status_event)(sng_fd_t fd, unsigned char link_status_event);
}wanpipe_tdm_api_event_t; 

typedef struct wanpipe_tdm_api{
	wanpipe_tdm_api_cmd_t	wp_tdm_cmd;
	wanpipe_tdm_api_event_t wp_tdm_event;
}wanpipe_tdm_api_t;


#endif
