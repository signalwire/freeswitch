/*****************************************************************************
 * libsangoma.c	AFT T1/E1: HDLC API Code Library
 *
 * Author(s):	Anthony Minessale II <anthmct@yahoo.com>
 *              Nenad Corbic <ncorbic@sangoma.com>
 *
 * Copyright:	(c) 2005 Anthony Minessale II
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 * ============================================================================
 */

#ifndef _LIBSNAGOMA_H
#define _LIBSNAGOMA_H
#include <stdio.h>

#define WANPIPE_TDM_API 1

#ifdef WIN32
#ifndef __WINDOWS__
#define __WINDOWS__
#endif
#include <windows.h>
#include <winioctl.h>
#include <conio.h>
#include <stddef.h>				//for offsetof()
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
#include <wanpipe_defines.h>	//for 'wan_udp_hdr_t'
#include <wanpipe_cfg.h>
#ifdef WANPIPE_TDM_API
#include <wanpipe_tdm_api.h>	//for TDMV API
#endif
#include <sang_status_defines.h>//return codes
#include <sang_api.h>			//for IOCTL codes
#include <sdla_te1_pmc.h>		//RBS definitions
#include <sdla_te1.h>			//TE1 macros
#include <sdla_56k.h>			//56k macros
#include <sdla_remora.h>		//Analog card
#include <sdla_te3.h>			//T3 card
#include <sdla_front_end.h>		//front-end (T1/E1/56k) commands
#include <sdla_aft_te1.h>		//for Wanpipe API

#define _MYDEBUG
#define PROGRAM_NAME "LIBSANGOMA: "
#include <DebugOut.h>

typedef HANDLE sng_fd_t;
#else
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if.h>
#include <poll.h>
#include <signal.h>

//typedef int sng_fd_t;
#include <linux/wanpipe_defines.h>
#include <linux/wanpipe_cfg.h>
#include <linux/wanpipe.h>
#ifdef WANPIPE_TDM_API
# include <linux/wanpipe_tdm_api.h>
#endif
#endif

#define FNAME_LEN	50
#define FUNC_DBG(x)		if(0)printf("%s:%d\n", x, __LINE__)
#define DBG_PRINT		if(1)printf

typedef wp_tdm_api_rx_hdr_t sangoma_api_hdr_t;

/* Decodec Span/Chan from interface name */
int sangoma_span_chan_toif(int span, int chan, char *interface_name);
int sangoma_span_chan_fromif(char *interface_name, int *span, int *chan);
int sangoma_interface_toi(char *interface_name, int *span, int *chan);

sng_fd_t sangoma_create_socket_by_name(char *device, char *card);

/* Open Span/Chan devices
 * open_tdmapi_span_chan: open device based on span chan values 
 * sangoma_open_tdmapi_span: open first available device on span
 */     

sng_fd_t sangoma_open_tdmapi_span_chan(int span, int chan);
sng_fd_t sangoma_open_tdmapi_span(int span);

#define sangoma_create_socket_intr sangoma_open_tdmapi_span_chan

/* Device Rx/Tx functions 
 * writemsg_tdm: 	tx header + data from separate buffers 
 * readmsg_tdm: 	rx header + data to separate buffers
 */    
int sangoma_writemsg_tdm(sng_fd_t fd, void *hdrbuf, int hdrlen, 
						 void *databuf, unsigned short datalen, int flag);
int sangoma_readmsg_tdm(sng_fd_t fd, void *hdrbuf, int hdrlen, 
						void *databuf, int datalen, int flag);

#define sangoma_readmsg_socket sangoma_readmsg_tdm
#define sangoma_sendmsg_socket sangoma_writemsg_tdm

#ifdef WANPIPE_TDM_API

void sangoma_socket_close(sng_fd_t *sp);
int sangoma_socket_waitfor(sng_fd_t fd, int timeout, int flags);

/* Get Full TDM API configuration per chan */
int sangoma_get_full_cfg(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

/* Get/Set TDM Codec per chan */
int sangoma_tdm_set_codec(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api, int codec);
int sangoma_tdm_get_codec(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

/* Get/Set USR Tx/Rx Period in milliseconds */
int sangoma_tdm_set_usr_period(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api, int period);
int sangoma_tdm_get_usr_period(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

/* Get user MTU/MRU values in bytes */
int sangoma_tdm_get_usr_mtu_mru(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

/* Not supported yet */
int sangoma_tdm_set_power_level(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api, int power);
int sangoma_tdm_get_power_level(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

/* Flush buffers from current channel */
int sangoma_tdm_flush_bufs(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

int sangoma_tdm_enable_rbs_events(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api, int poll_in_sec);
int sangoma_tdm_disable_rbs_events(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

int sangoma_tdm_write_rbs(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api, unsigned char rbs);

int sangoma_tdm_read_event(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

/* DTMF Detection on Octasic chip */
int sangoma_tdm_enable_dtmf_events(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);
int sangoma_tdm_disable_dtmf_events(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

/* DTMF Detection on A200 (SLIC) chip */
int sangoma_tdm_enable_rm_dtmf_events(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);
int sangoma_tdm_disable_rm_dtmf_events(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

/* On/Off hook events on A200 (Analog) card */
int sangoma_tdm_enable_rxhook_events(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);
int sangoma_tdm_disable_rxhook_events(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

int sangoma_tdm_get_fe_alarms(sng_fd_t fd, wanpipe_tdm_api_t *tdm_api);

#ifndef LIBSANGOMA_GET_HWCODING
#define LIBSANGOMA_GET_HWCODING 1
#endif
int sangoma_tdm_get_hw_coding(int fd, wanpipe_tdm_api_t *tdm_api);

#endif 	/* WANPIPE_TDM_API */

#endif
