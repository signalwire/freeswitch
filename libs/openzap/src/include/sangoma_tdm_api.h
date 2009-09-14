/*****************************************************************************
 * sangoma_tdm_api.h	Sangoma TDM API Portability functions
 *
 * Author(s):	Anthony Minessale II <anthmct@yahoo.com>
 *              Nenad Corbic <ncorbic@sangoma.com>
 *				Michael Jerris <mike@jerris.com>
 *				David Rokhvarg <davidr@sangoma.com>
 *
 * Copyright:	(c) 2006 Nenad Corbic <ncorbic@sangoma.com>
 *                       Anthony Minessale II
 *				(c) 1984-2007 Sangoma Technologies Inc.
 *
 * ============================================================================
 */

#ifndef _SANGOMA_TDM_API_H
#define _SANGOMA_TDM_API_H

/* This entire block of defines and includes from this line, through #define FNAME_LEN probably dont belong here */
/* most of them probably belong in wanpipe_defines.h, then each header file listed included below properly included */
/* in the header files that depend on them, leaving only the include for wanpipe_tdm_api.h left in this file or */
/* possibly integrating the rest of this file diretly into wanpipe_tdm_api.h */
#ifndef __WINDOWS__
#if defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32)
#define __WINDOWS__
#endif /* defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32) */
#endif /* ndef __WINDOWS__ */

#if defined(__WINDOWS__)
#if defined(_MSC_VER)
/* disable some warnings caused by wanpipe headers that will need to be fixed in those headers */
#pragma warning(disable:4201 4214)

/* sang_api.h(74) : warning C4201: nonstandard extension used : nameless struct/union */

/* wanpipe_defines.h(219) : warning C4214: nonstandard extension used : bit field types other than int */
/* wanpipe_defines.h(220) : warning C4214: nonstandard extension used : bit field types other than int */
/* this will break for any compilers that are strict ansi or strict c99 */

/* The following definition for that struct should resolve this warning and work for 32 and 64 bit */
#if 0
struct iphdr {
	
#if defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned    ihl:4,
		version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	unsigned    version:4,
		ihl:4;
#else
# error  "unknown byteorder!"
#endif
	unsigned    tos:8;
	unsigned		tot_len:16;
	unsigned		id:16;
	unsigned		frag_off:16;
	__u8    ttl;
	__u8    protocol;
	__u16   check;
	__u32   saddr;
	__u32   daddr;
	/*The options start here. */
};
#endif /* #if 0 */

#define __inline__ __inline
#endif /* defined(_MSC_VER) */
#include <windows.h>
/* do we like the name WP_INVALID_SOCKET or should it be changed? */
#define WP_INVALID_SOCKET INVALID_HANDLE_VALUE
#else /* defined(__WINDOWS__) */ 
#define WP_INVALID_SOCKET -1
#include <stropts.h>
#include <poll.h>
#include <sys/socket.h>
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


#if defined(__WINDOWS__)
/* This might be broken on windows, as POLL_EVENT_TELEPHONY seems to be commented out in sang_api.h.. it should be added to POLLPRI */
#define POLLPRI (POLL_EVENT_LINK_STATE | POLL_EVENT_LINK_CONNECT | POLL_EVENT_LINK_DISCONNECT)
#endif

/* return -1 for error, 0 for timeout or 1 for success. *flags is set to the poll evetns POLLIN | POLLOUT | POLLPRI based on the result of the poll */
/* on windows we actually have POLLPRI defined with several events, so we could theoretically poll */
/* for specific events.  Is there any way to do this on *nix as well? */ 

/* a cross platform way to poll on an actual pollset (span and/or list of spans) will probably also be needed for analog */
/* so we can have one analong handler thread that will deal with all the idle analog channels for events */
/* the alternative would be for the driver to provide one socket for all of the oob events for all analog channels */
static __inline__ int tdmv_api_wait_socket(sng_fd_t fd, int timeout, int *flags)
{
#if defined(__WINDOWS__)
	DWORD ln;
	API_POLL_STRUCT	api_poll;

	memset(&api_poll, 0x00, sizeof(API_POLL_STRUCT));
	
	api_poll.user_flags_bitmap = *flags;
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

	*flags = 0;

	switch(api_poll.operation_status)
		{
		case SANG_STATUS_RX_DATA_AVAILABLE:
			break;

		case SANG_STATUS_RX_DATA_TIMEOUT:
			return 0;

		default:
			return -1;
		}

	if (api_poll.poll_events_bitmap == 0){
		return -1;
	}

	if (api_poll.poll_events_bitmap & POLL_EVENT_TIMEOUT) {
		return 0;
	}

	*flags = api_poll.poll_events_bitmap;

	return 1;
#else
    struct pollfd pfds[1];
    int res;

    memset(&pfds[0], 0, sizeof(pfds[0]));
    pfds[0].fd = fd;
    pfds[0].events = *flags;
    res = poll(pfds, 1, timeout);
	*flags = 0;

	if (pfds[0].revents & POLLERR) {
		res = -1;
	}

	if (res > 0) {
		*flags = pfds[0].revents;
	}

    return res;
#endif
}

/* on windows right now, there is no way to specify if we want to read events here or not, we allways get them here */
/* we need some what to select if we are reading regular tdm msgs or events */
/* need to either have 2 functions, 1 for events, 1 for regural read, or a flag on this function to choose */
/* 2 functions preferred.  Need implementation for the event function for both nix and windows that is threadsafe */
static __inline__ int tdmv_api_readmsg_tdm(sng_fd_t fd, void *hdrbuf, int hdrlen, void *databuf, int datalen)
{
	/* What do we need to do here to avoid having to do all */
	/* the memcpy's on windows and still maintain api compat with nix */
	int rx_len=0;
#if defined(__WINDOWS__)
	static RX_DATA_STRUCT	rx_data;
	api_header_t			*pri;
	wp_tdm_api_rx_hdr_t		*tdm_api_rx_hdr;
	wp_tdm_api_rx_hdr_t		*user_buf = (wp_tdm_api_rx_hdr_t*)hdrbuf;
	DWORD ln;

	if (hdrlen != sizeof(wp_tdm_api_rx_hdr_t)){
		return -1;
	}

	if (!DeviceIoControl(
						 fd,
						 IoctlReadCommand,
						 (LPVOID)NULL,
						 0L,
						 (LPVOID)&rx_data,
						 sizeof(RX_DATA_STRUCT),
						 (LPDWORD)(&ln),
						 (LPOVERLAPPED)NULL
						 )){
		return -1;
	}

	pri = &rx_data.api_header;
	tdm_api_rx_hdr = (wp_tdm_api_rx_hdr_t*)rx_data.data;

	user_buf->wp_tdm_api_event_type = pri->operation_status;

	switch(pri->operation_status)
		{
		case SANG_STATUS_RX_DATA_AVAILABLE:
			if (pri->data_length > datalen){
				break;
			}
			memcpy(databuf, rx_data.data, pri->data_length);
			rx_len = pri->data_length;
			break;

		default:
			break;
		}

#else
	struct msghdr msg;
	struct iovec iov[2];

	memset(&msg,0,sizeof(struct msghdr));

	iov[0].iov_len=hdrlen;
	iov[0].iov_base=hdrbuf;

	iov[1].iov_len=datalen;
	iov[1].iov_base=databuf;

	msg.msg_iovlen=2;
	msg.msg_iov=iov;

	rx_len = read(fd,&msg,datalen+hdrlen);

	if (rx_len <= sizeof(wp_tdm_api_rx_hdr_t)){
		return -EINVAL;
	}

	rx_len-=sizeof(wp_tdm_api_rx_hdr_t);
#endif
    return rx_len;
}                    

static __inline__ int tdmv_api_writemsg_tdm(sng_fd_t fd, void *hdrbuf, int hdrlen, void *databuf, unsigned short datalen)
{
	/* What do we need to do here to avoid having to do all */
	/* the memcpy's on windows and still maintain api compat with nix */
	int bsent = 0;
#if defined(__WINDOWS__)
	static TX_DATA_STRUCT	local_tx_data;
	api_header_t			*pri;
	DWORD ln;

	/* Are these really not needed or used???  What about for nix?? */
	(void)hdrbuf;
	(void)hdrlen;

	pri = &local_tx_data.api_header;

	pri->data_length = datalen;
	memcpy(local_tx_data.data, databuf, pri->data_length);

	if (!DeviceIoControl(
						 fd,
						 IoctlWriteCommand,
						 (LPVOID)&local_tx_data,
						 (ULONG)sizeof(TX_DATA_STRUCT),
						 (LPVOID)&local_tx_data,
						 sizeof(TX_DATA_STRUCT),
						 (LPDWORD)(&ln),
						 (LPOVERLAPPED)NULL
						 )){
		return -1;
	}

	if (local_tx_data.api_header.operation_status == SANG_STATUS_SUCCESS) {
		bsent = datalen;
	}
#else
	struct msghdr msg;
	struct iovec iov[2];

	memset(&msg,0,sizeof(struct msghdr));

	iov[0].iov_len = hdrlen;
	iov[0].iov_base = hdrbuf;

	iov[1].iov_len = datalen;
	iov[1].iov_base = databuf;

	msg.msg_iovlen = 2;
	msg.msg_iov = iov;

	bsent = write(fd, &msg, datalen + hdrlen);
	if (bsent > 0){
		bsent -= sizeof(wp_tdm_api_tx_hdr_t);
	}
#endif
	return bsent;
}

#endif /* _SANGOMA_TDM_API_H */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

