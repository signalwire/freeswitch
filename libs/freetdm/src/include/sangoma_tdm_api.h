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
#define DEV_NAME_LEN	100
char device_name[DEV_NAME_LEN];


#ifdef __WINDOWS__

/* IOCTL management structures and variables*/
wan_udp_hdr_t	wan_udp;

static wan_cmd_api_t api_cmd;
static api_tx_hdr_t *tx_hdr = (api_tx_hdr_t *)api_cmd.data;

/* keeps the LAST (and single) event received */
static wp_tdm_api_rx_hdr_t last_tdm_api_event_buffer;
#endif

#ifdef __WINDOWS__
static int tdmv_api_ioctl(sng_fd_t fd, wanpipe_tdm_api_cmd_t *tdm_api_cmd)
{
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

	sprintf(fname,"/dev/wptdm_s%dc%d",span,chan);

	fd = open(fname, O_RDWR);

	if (fd < 0) {
		fd = WP_INVALID_SOCKET;
	}

	return fd;  
#endif
}            

void tdmv_api_close_socket(sng_fd_t *sp) 
{
	if(	*sp != INVALID_HANDLE_VALUE){
#if defined(__WINDOWS__)
		CloseHandle(*sp);
#else
		close(*sp);
#endif
		*sp = INVALID_HANDLE_VALUE;
	}
}

#ifdef __WINDOWS__
static int wanpipe_api_ioctl(sng_fd_t fd, wan_cmd_api_t *api_cmd)
{
	DWORD			ln;
    unsigned char	id = 0;
	int				err = 0;

	wan_udp.wan_udphdr_request_reply = 0x01;
	wan_udp.wan_udphdr_id			 = id;
   	wan_udp.wan_udphdr_return_code   = WAN_UDP_TIMEOUT_CMD;

	wan_udp.wan_udphdr_command	= SIOC_WANPIPE_API;
	wan_udp.wan_udphdr_data_len	= sizeof(wan_cmd_api_t);

	//copy data from caller's buffer to driver's buffer
	memcpy(	wan_udp.wan_udphdr_data, 
			(void*)api_cmd,
			sizeof(wan_cmd_api_t));

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
		err = 1;
		return err;
	}else{
		err = 0;
	}

	if(wan_udp.wan_udphdr_return_code != WAN_CMD_OK){
		return 2;
	}

	//copy data from driver's buffer to caller's buffer
	memcpy(	(void*)api_cmd,
			wan_udp.wan_udphdr_data, 
			sizeof(wan_cmd_api_t));
	return 0;
}

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

// Blocking API Poll command.
static
USHORT 
DoApiPollCommand(
	sng_fd_t drv, 
	API_POLL_STRUCT *api_poll_ptr
	)
{
	DWORD ln;

	if (DeviceIoControl(
			drv,
			IoctlApiPoll,
			(LPVOID)NULL,
			0L,
			(LPVOID)api_poll_ptr,
			sizeof(API_POLL_STRUCT),
			(LPDWORD)(&ln),
			(LPOVERLAPPED)NULL
						) == FALSE){
		return 1;
	}else{
		return 0;
	}
}

static
int 
DoManagementCommand(
	sng_fd_t drv, 
	wan_udp_hdr_t* wan_udp
	)
{
	DWORD ln;
    static unsigned char id = 0;

	wan_udp->wan_udphdr_request_reply = 0x01;
	wan_udp->wan_udphdr_id = id++;
   	wan_udp->wan_udphdr_return_code = WAN_UDP_TIMEOUT_CMD;

	if(DeviceIoControl(
			drv,
			IoctlManagementCommand,
			(LPVOID)wan_udp,
			sizeof(wan_udp_hdr_t),
			(LPVOID)wan_udp,
			sizeof(wan_udp_hdr_t),
			(LPDWORD)(&ln),
			(LPOVERLAPPED)NULL
						) == FALSE){
		return 1;
	}else{
		return 0;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//structures and definitions used for queueing data
//
typedef struct
{
	void*				previous;
	TX_RX_DATA_STRUCT	tx_rx_data;
}api_queue_element_t;

#define API_Q_MUTEX_TIMEOUT	1000//1 second
#define API_Q_MAX_SIZE		100//optimal length. for short data may need longer queue

enum API_Q_STATUS{
	API_Q_SUCCESS=0,
	API_Q_GEN_FAILURE,
	API_Q_MEM_ALLOC_FAILURE,
	API_Q_FULL,
	API_Q_EMPTY
};

typedef struct
{
	//number of nodes in the list
	USHORT size;
	//insert at tail
	api_queue_element_t * tail;
	//remove from head
	api_queue_element_t * head;
	//mutex for synchronizing access to the queue
	sng_fd_t api_queue_mutex;
}api_queue_t;


static __inline int api_enqueue(	api_queue_t* api_queue, 
									unsigned char * buffer,
									unsigned short length)
{
	api_queue_element_t *element;
	DWORD				mresult;

	mresult = WaitForSingleObject(api_queue->api_queue_mutex, API_Q_MUTEX_TIMEOUT);
	if (mresult != WAIT_OBJECT_0) {
		return API_Q_GEN_FAILURE;
	}

	if(api_queue->size == API_Q_MAX_SIZE){
		ReleaseMutex(api_queue->api_queue_mutex);
		return API_Q_FULL;
	}

	element = malloc(sizeof(api_queue_element_t));
	if(element == NULL){
		ReleaseMutex(api_queue->api_queue_mutex);
		return API_Q_MEM_ALLOC_FAILURE;
	}

	//now copy everything in to the element
	memcpy(element->tx_rx_data.data, buffer, length);
	
	element->tx_rx_data.api_header.data_length = length;
	element->tx_rx_data.api_header.operation_status = SANG_STATUS_TX_TIMEOUT;

	//insert element at the tail of the queue
	element->previous = NULL;

	if(api_queue->size == 0){
		//special case of a previously empty queue
		api_queue->head = element;
		api_queue->tail = element;
	}else{
		api_queue->tail->previous = element;
		api_queue->tail = element;
	}
	api_queue->size++;
	ReleaseMutex(api_queue->api_queue_mutex);
	return API_Q_SUCCESS;
}

static __inline int api_dequeue(	api_queue_t* api_queue,
											TX_RX_DATA_STRUCT* destination)
{
	api_queue_element_t *element;
	DWORD				mresult;

	mresult = WaitForSingleObject(api_queue->api_queue_mutex, API_Q_MUTEX_TIMEOUT);
	if (mresult != WAIT_OBJECT_0) {
		return API_Q_GEN_FAILURE;
	}

	if(api_queue->size == 0){
		//tx queue is empty
		ReleaseMutex(api_queue->api_queue_mutex);
		return API_Q_EMPTY;
	}

	//remove from the head of the queue
	element = api_queue->head;
	api_queue->head = element->previous;

	//now copy everything in to the user buffer
	memcpy(destination, &element->tx_rx_data, sizeof(TX_DATA_STRUCT));

	free(element);
	api_queue->size--;
	if(api_queue->size == 0){
		api_queue->head = NULL;
		api_queue->tail = NULL;
	}
	ReleaseMutex(api_queue->api_queue_mutex);
	return API_Q_SUCCESS;
}

//remove all elements from the queue
static __inline void empty_api_queue(api_queue_t* api_queue)
{
	TX_DATA_STRUCT tx_rx_data;

	while(api_dequeue(api_queue, &tx_rx_data) == 0){
		;
	}
}

///////////////////////////////////////////////////////////////////////////
#endif

#endif /* _SANGOMA_TDM_API_H */
