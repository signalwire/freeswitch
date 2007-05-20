/*****************************************************************************
 * sangoma_tdm_api.h	Sangoma API Code Library
 *
 * Author(s):	David Rokhvarg <davidr@sangoma.com>
 *
 * Copyright:	(c) 1984-2006 Sangoma Technologies Inc.
 *
 * ============================================================================
 */

#ifndef _SANGOMA_TDM_API_H
#define _SANGOMA_TDM_API_H

#ifndef __WINDOWS__
#if defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32)
#define __WINDOWS__
#endif
#endif

#ifdef __WINDOWS__
#ifdef _MSC_VER
/* disable warning for zero length array in a struct */
/* this will cause errors on c99 and ansi compliant compilers and will need to be fixed in the wanpipe header files */
#pragma warning(disable:4200 4201 4214)
#endif
#include <windows.h>
#define WP_INVALID_SOCKET INVALID_HANDLE_VALUE
#else
#define WP_INVALID_SOCKET -1
#include <stropts.h>
#include <poll.h>
#endif

#include <wanpipe_defines.h>
#include <wanpipe_cfg.h>
#include <wanpipe_tdm_api.h>
#include <sdla_te1_pmc.h>
#ifdef __WINDOWS__
#include <sang_status_defines.h>
#include <sang_api.h>
#endif
#include <sdla_aft_te1.h>

#define FNAME_LEN	50

#ifdef __WINDOWS__
static int tdmv_api_ioctl(sng_fd_t fd, wanpipe_tdm_api_cmd_t *tdm_api_cmd)
{
	wan_udp_hdr_t	wan_udp;
	DWORD			ln;
    unsigned char	id = 0;
	int				err = 0;

	wan_udp.wan_udphdr_request_reply = 0x01;
	wan_udp.wan_udphdr_id			 = id;
   	wan_udp.wan_udphdr_return_code	 = WAN_UDP_TIMEOUT_CMD;

	wan_udp.wan_udphdr_command	= WAN_TDMV_API_IOCTL;
	wan_udp.wan_udphdr_data_len	= sizeof(wanpipe_tdm_api_cmd_t);

	//copy data from caller's buffer to driver's buffer
	memcpy(	wan_udp.wan_udphdr_data, 
			(void*)tdm_api_cmd,
			sizeof(wanpipe_tdm_api_cmd_t));

	if(DeviceIoControl(
			fd,
			IoctlManagementCommand,
			(LPVOID)&wan_udp,
			sizeof(wan_udp_hdr_t),
			(LPVOID)&wan_udp,
			sizeof(wan_udp_hdr_t),
			(LPDWORD)(&ln),
			(LPOVERLAPPED)NULL
			) == FALSE){
		//actual ioctl failed
		err = 1;
		return err;
	}else{
		err = 0;
	}

	if(wan_udp.wan_udphdr_return_code != WAN_CMD_OK){
		//ioctl ok, but command failed
		return 2;
	}

	//copy data from driver's buffer to caller's buffer
	memcpy(	(void*)tdm_api_cmd,
			wan_udp.wan_udphdr_data, 
			sizeof(wanpipe_tdm_api_cmd_t));
	return 0;
}

#else
static int tdmv_api_ioctl(sng_fd_t fd, wanpipe_tdm_api_cmd_t *tdm_api_cmd)
{
	return ioctl(fd, SIOC_WANPIPE_TDM_API, tdm_api_cmd);
}
#endif

static sng_fd_t tdmv_api_open_span_chan(int span, int chan) 
{
   	char fname[FNAME_LEN];
#if defined(__WINDOWS__)

	//NOTE: under Windows Interfaces are zero based but 'chan' is 1 based.
	//		Subtract 1 from 'chan'.
	_snprintf(fname , FNAME_LEN, "\\\\.\\WANPIPE%d_IF%d", span, chan - 1);

	//prn(verbose, "Opening device: %s...\n", fname);

	return CreateFile(	fname, 
						GENERIC_READ | GENERIC_WRITE, 
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						(LPSECURITY_ATTRIBUTES)NULL, 
						OPEN_EXISTING,
						FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
						(HANDLE)NULL
						);
#else
  	int fd = WP_INVALID_SOCKET;

	snprintf(fname, FNAME_LEN, "/dev/wptdm_s%dc%d",span,chan);

	fd = open(fname, O_RDWR);

	if (fd < 0) {
		fd = WP_INVALID_SOCKET;
	}

	return fd;  
#endif
}            

void tdmv_api_close_socket(sng_fd_t *sp) 
{
	if(	*sp != WP_INVALID_SOCKET){
#if defined(__WINDOWS__)
		CloseHandle(*sp);
#else
		close(*sp);
#endif
		*sp = WP_INVALID_SOCKET;
	}
}

#ifdef __WINDOWS__

// Blocking read command. If used after DoApiPollCommand(),
// it will return immediatly, without blocking.
static 
USHORT 
DoReadCommand(
	sng_fd_t drv, 
	RX_DATA_STRUCT * pRx
	)
{
	DWORD ln;

	if (DeviceIoControl(
			drv,
			IoctlReadCommand,
			(LPVOID)NULL,
			0L,
			(LPVOID)pRx,
			sizeof(RX_DATA_STRUCT),
			(LPDWORD)(&ln),
			(LPOVERLAPPED)NULL
			) == FALSE){
		//check messages log
		return 1;
	}else{
		return 0;
	}
}

// Blocking write command. If used after DoApiPollCommand(),
// it will return immediatly, without blocking.
static
UCHAR 
DoWriteCommand(
	sng_fd_t drv, 
	TX_DATA_STRUCT * pTx
	)
{
	DWORD ln;

	if(DeviceIoControl(
			drv,
			IoctlWriteCommand,
			(LPVOID)pTx,
			(ULONG)sizeof(TX_DATA_STRUCT),
			(LPVOID)pTx,
			sizeof(TX_DATA_STRUCT),
			(LPDWORD)(&ln),
			(LPOVERLAPPED)NULL
			) == FALSE){
		return 1;
	}else{
		return 0;
	}
}

#endif

#if defined(__WINDOWS__)
/* This is broken, we don't actually get real results for oob messages on windows */
#define POLLPRI (POLL_EVENT_LINK_STATE | POLL_EVENT_LINK_CONNECT | POLL_EVENT_LINK_DISCONNECT)
#endif

/* return -1 for error, 0 for timeout, or POLLIN | POLLOUT | POLLPRI based on the result of the poll */
int tdmv_api_wait_socket(sng_fd_t fd, int timeout, int flags)
{
#if defined(__WINDOWS__)
	DWORD ln;
	API_POLL_STRUCT	api_poll;

	memset(&api_poll, 0x00, sizeof(API_POLL_STRUCT));
	
	api_poll.user_flags_bitmap = flags;
	api_poll.timeout = timeout;

	if (!DeviceIoControl(
			fd,
			IoctlApiPoll,
			(LPVOID)NULL,
			0L,
			(LPVOID)&api_poll,
			sizeof(API_POLL_STRUCT),
			(LPDWORD)(&ln),
			(LPOVERLAPPED)NULL)) {
		return -1;
	}

	switch(api_poll.operation_status)
	{
		case SANG_STATUS_RX_DATA_AVAILABLE:
			break;

		case SANG_STATUS_RX_DATA_TIMEOUT:
			return 0;

		default:
			return -1;
	}

	if(api_poll.poll_events_bitmap == 0){
		return -1;
	}

	if(api_poll.poll_events_bitmap & POLL_EVENT_TIMEOUT){
		return 0;
	}

	return api_poll.poll_events_bitmap;
#else
    struct pollfd pfds[1];
    int res;

    memset(&pfds[0], 0, sizeof(pfds[0]));
    pfds[0].fd = fd;
    pfds[0].events = flags;
    res = poll(pfds, 1, timeout);

	if (res == 0) {
		return 0;
	}

	if (res < 0) {
		return -1;
	}

	if ((pfds[0].revents & POLLERR)) {
		return -1;
	}

    return pfds[0].revents;
#endif
}

#endif /* _SANGOMA_TDM_API_H */
