/*****************************************************************************
 * win_api_common.h	- Sangoma API for MS Windows. Abstraction of common API calls.
 *
 * Author(s):	David Rokhvarg <davidr@sangoma.com>
 *
 * Copyright:	(c) 1984-2006 Sangoma Technologies Inc.
 *
 * ============================================================================
 */

#ifndef _WIN_API_COMMON_H
#define _WIN_API_COMMON_H

static 
int 
tdmv_api_ioctl(
	HANDLE fd, 
	wanpipe_tdm_api_cmd_t *tdm_api_cmd
	)
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
		prn(1, "Error: tdmv_api_ioctl(): DeviceIoControl failed!!\n");
		return err;
	}else{
		err = 0;
	}

	if(wan_udp.wan_udphdr_return_code != WAN_CMD_OK){
		//ioctl ok, but command failed
		prn(1, "Error: tdmv_api_ioctl(): command failed! Return code: 0x%X.\n",
			wan_udp.wan_udphdr_return_code);
		return 2;
	}

	//copy data from driver's buffer to caller's buffer
	memcpy(	(void*)tdm_api_cmd,
			wan_udp.wan_udphdr_data, 
			sizeof(wanpipe_tdm_api_cmd_t));
	return 0;
}

static 
int 
wanpipe_api_ioctl(
	HANDLE fd,
	wan_cmd_api_t *api_cmd
	)
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
		prn(1, "Error: wanpipe_api_ioctl(): DeviceIoControl failed!!\n");
		return err;
	}else{
		err = 0;
	}

	if(wan_udp.wan_udphdr_return_code != WAN_CMD_OK){
		prn(1, "Error: wanpipe_api_ioctl(): command failed! Return code: 0x%X.\n",
			wan_udp.wan_udphdr_return_code);
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
	HANDLE drv, 
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
		prn(1, "Error: DoReadCommand(): DeviceIoControl failed!\n");
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
	HANDLE drv, 
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
		//check messages log
		prn(1, "Error: DoWriteCommand(): DeviceIoControl failed!\n");
		return 1;
	}else{
		return 0;
	}
}

// Blocking API Poll command.
static
USHORT 
DoApiPollCommand(
	HANDLE drv, 
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
		//check messages log
		prn(1, "Error: DoApiPollCommand(): DeviceIoControl failed!\n");
		return 1;
	}else{
		return 0;
	}
}

static
int 
DoManagementCommand(
	HANDLE drv, 
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
		//check messages log
		prn(1, "Error: DoManagementCommand(): DeviceIoControl failed!\n");
		return 1;
	}else{
		return 0;
	}
}


#endif /* _WIN_API_COMMON_H */
