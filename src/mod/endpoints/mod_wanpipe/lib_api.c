/*****************************************************************************
* lib_api.c	Common API library
*
* Author(s):	Nenad Corbic <ncorbic@sangoma.com>
*
* Copyright:	(c) 2003 Sangoma Technologies Inc.
*
*		This program is free software; you can redistribute it and/or
*		modify it under the terms of the GNU General Public License
*		as published by the Free Software Foundation; either version
*		2 of the License, or (at your option) any later version.
* ============================================================================
*/


#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if_wanpipe.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <signal.h>
#include <linux/if.h>

#include <linux/wanpipe_defines.h>
#include <linux/wanpipe_cfg.h>
#include <linux/wanpipe.h>

#include "lib_api.h"

#define SINGLE_CHANNEL	0x2
#define RANGE_CHANNEL	0x1


char	read_enable=0;
char 	write_enable=0;
char 	primary_enable=0;
int 	tx_cnt=1;
int	rx_cnt=0;
int	tx_size=10;
int	tx_delay=0;
int	tx_data=-1;
int	tx_ss7_type=0;
int	rx_ss7_timer=0;

unsigned char card_name[WAN_IFNAME_SZ];
unsigned char if_name[WAN_IFNAME_SZ];

unsigned char sw_if_name[WAN_IFNAME_SZ];
unsigned char sw_card_name[WAN_IFNAME_SZ];

unsigned char tx_file[WAN_IFNAME_SZ];
unsigned char rx_file[WAN_IFNAME_SZ];

unsigned char daddr[TX_ADDR_STR_SZ];
unsigned char saddr[TX_ADDR_STR_SZ];
unsigned char udata[TX_ADDR_STR_SZ];

int	files_used=0;
int	verbose=0;

int	tx_connections;

int	ds_prot=0;
int	ds_prot_opt=0;
int	ds_max_mult_cnt=0;
unsigned int ds_active_ch=0;
int	ds_7bit_hdlc=0;
int 	direction=-1;

int 	tx_channels=1;
int	cause=0;
int 	diagn=0;

int 	card_cnt=0;
int 	i_cnt=0;

unsigned long parse_active_channel(char* val);

int init_args(int argc, char *argv[])
{
	int i;
	int c_cnt=0;

	sprintf(daddr,"111");
	sprintf(saddr,"222");
	sprintf(udata,"C9");
	
	for (i = 0; i < argc; i++){
		
		if (!strcmp(argv[i],"-i")){

			if (i+1 > argc-1){
				printf("ERROR: Invalid Interface Name!\n");
				return WAN_FALSE;
			}

			strncpy(if_name, argv[i+1],WAN_IFNAME_SZ);
			i_cnt=1;
		
		}else if (!strcmp(argv[i],"-si")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid Switch Interface Name!\n");
				return WAN_FALSE;
			}
			
			strncpy(sw_if_name, argv[i+1], WAN_IFNAME_SZ);

		}else if (!strcmp(argv[i],"-c")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid Card Name!\n");
				return WAN_FALSE;
			}
			
			strncpy(card_name, argv[i+1], WAN_IFNAME_SZ);
			card_cnt=1;
			
		}else if (!strcmp(argv[i],"-sc")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid Switch Card Name!\n");
				return WAN_FALSE;
			}
			
			strncpy(sw_card_name, argv[i+1], WAN_IFNAME_SZ);

		}else if (!strcmp(argv[i],"-r")){
			read_enable=1;
			c_cnt=1;
	
		}else if (!strcmp(argv[i],"-w")){
			write_enable=1;
			c_cnt=1;

		}else if (!strcmp(argv[i],"-pri")){
			primary_enable=1;
		
		}else if (!strcmp(argv[i],"-txcnt")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid tx cnt!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				tx_cnt = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid tx cnt!\n");
				return WAN_FALSE;
			}

		}else if (!strcmp(argv[i],"-rxcnt")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid rx cnt!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				rx_cnt = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid rx cnt!\n");
				return WAN_FALSE;
			}

		}else if (!strcmp(argv[i],"-txsize")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid tx size!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				tx_size = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid tx size, must be a digit!\n");
				return WAN_FALSE;
			}
		}else if (!strcmp(argv[i],"-txdelay")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid tx delay!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				tx_delay = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid tx delay, must be a digit!\n");
				return WAN_FALSE;
			}
		}else if (!strcmp(argv[i],"-txdata")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid tx data!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				tx_data = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid tx data, must be a digit!\n");
				return WAN_FALSE;
			}
		}else if (!strcmp(argv[i],"-tx_ss7_type")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid tx ss7 type!\n");
				return WAN_FALSE;
			}
	
			if(isdigit(argv[i+1][0])){
				tx_ss7_type = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid tx ss7 type, must be a digit!\n");
				return WAN_FALSE;
			}
		 
		}else if (!strcmp(argv[i],"-rx_ss7_timer")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid rx ss7 timer!\n");
				return WAN_FALSE;
			}
	
			if(isdigit(argv[i+1][0])){
				rx_ss7_timer = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid tx ss7 type, must be a digit!\n");
				return WAN_FALSE;
			}
		 

		}else if (!strcmp(argv[i],"-txfile")){

			if (i+1 > argc-1){
				printf("ERROR: Invalid Tx File Name!\n");
				return WAN_FALSE;
			}

			strncpy(tx_file, argv[i+1],WAN_IFNAME_SZ);
			files_used |= TX_FILE_USED;
			
		}else if (!strcmp(argv[i],"-rxfile")){

			if (i+1 > argc-1){
				printf("ERROR: Invalid Rx File Name!\n");
				return WAN_FALSE;
			}

			strncpy(rx_file, argv[i+1],WAN_IFNAME_SZ);
			files_used |= RX_FILE_USED;

		}else if (!strcmp(argv[i],"-daddr")){

			if (i+1 > argc-1){
				printf("ERROR: Invalid daddr str!\n");
				return WAN_FALSE;
			}

			strncpy(daddr, argv[i+1],TX_ADDR_STR_SZ);
		
		}else if (!strcmp(argv[i],"-saddr")){

			if (i+1 > argc-1){
				printf("ERROR: Invalid saddr str!\n");
				return WAN_FALSE;
			}

			strncpy(saddr, argv[i+1],TX_ADDR_STR_SZ);
		
		}else if (!strcmp(argv[i],"-udata")){

			if (i+1 > argc-1){
				printf("ERROR: Invalid udata str!\n");
				return WAN_FALSE;
			}

			strncpy(udata, argv[i+1],TX_ADDR_STR_SZ);

		}else if (!strcmp(argv[i],"-verbose")){
			verbose=1;

		}else if (!strcmp(argv[i],"-prot")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid prot!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				ds_prot = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid prot, must be a digit!\n");
				return WAN_FALSE;
			}

			
		}else if (!strcmp(argv[i],"-prot_opt")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid prot_opt!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				ds_prot_opt = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid prot_opt, must be a digit!\n");
				return WAN_FALSE;
			}
		}else if (!strcmp(argv[i],"-max_mult_cnt")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid max_mult_cnt!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				ds_max_mult_cnt = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid max_mult_cnt, must be a digit!\n");
				return WAN_FALSE;
			}
		}else if (!strcmp(argv[i],"-active_ch")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid active ch!\n");
				return WAN_FALSE;
			}		

			ds_active_ch = parse_active_channel(argv[i+1]);
		}else if (!strcmp(argv[i],"-txchan")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid channels!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				tx_channels = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid channels, must be a digit!\n");
				return WAN_FALSE;
			}
		}else if (!strcmp(argv[i],"-diagn")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid diagn!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				diagn = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid diagn, must be a digit!\n");
				return WAN_FALSE;
			}
			
		}else if (!strcmp(argv[i],"-cause")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid cause!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				cause = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid cause, must be a digit!\n");
				return WAN_FALSE;
			}

		}else if (!strcmp(argv[i],"-7bit_hdlc")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid 7bit hdlc value!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				ds_7bit_hdlc = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid 7bit hdlc, must be a digit!\n");
				return WAN_FALSE;
			}

		}else if (!strcmp(argv[i],"-dir")){
			if (i+1 > argc-1){
				printf("ERROR: Invalid direction value!\n");
				return WAN_FALSE;
			}		

			if(isdigit(argv[i+1][0])){
				direction = atoi(argv[i+1]);
			}else{
				printf("ERROR: Invalid direction, must be a digit!\n");
				return WAN_FALSE;
			}
		}
	}

	if (!i_cnt){
		printf("ERROR: No Interface Name!\n");
		return WAN_FALSE;
	}
	if (!card_cnt){
		printf("ERROR: No Card name!\n");
		return WAN_FALSE;
	}
	if (!c_cnt){
		printf("ERROR: No Read or Write Command!\n");
		return WAN_FALSE;
	}

	return WAN_TRUE;
}

static unsigned char api_usage[]="\n"
"\n"
"<options>:\n"
"	-i  <ifname>     #interface name\n"
"	-c  <card name>  #card name\n"
"	-r               #read enable\n"
"	-w               #write eable\n"
"\n"
"<extra options>\n"
"	-txcnt   <digit>  #number of tx packets  (Dflt: 1)\n"
"	-txsize  <digit>  #tx packet size        (Dflt: 10)\n"
"	-txdelay <digit>  #delay in sec after each tx packet (Dflt: 0)\n"
"	-txdata  <digit>  #data to tx <1-255>\n"
"\n"
"	-txfile  <file>   #Use file to tx instead\n"
"	-rxfile  <file>   #Save all rx data to a file\n"
"	\n"
"\n"
"	-tx_ss7_type  <digit> # 1=FISU   2=LSSU (repeating)\n"
"	-rx_ss7_timer <digit> #Force receive timeout value \n"
"\n"
"	-rxcnt   <digit>  #number of rx packets before exit\n"
"			  #this number overwrites the txcnt\n"
"	                  #Thus, app will only exit after it\n"
"	                  #receives the rxcnt number of packets.\n"
"	\n"
"	-verbose	  #Enable verbose mode\n"
"\n"
"<datascope options>\n"
"\n"
"	-prot		<digit>	  #Protocol Bit map: \n"
"				  #1=FISU, 2=LSSU, 4=MSU, 8=RAW HDLC\n"
"	\n"
"	-prot_opt	<digit>	  #Protocol bit map\n"
"			          #0=None, 1=Delta, 2=Max Multiple\n"		  
"\n"
"	-active_ch	<digit>   #Active channel\n" 
"                                  #ALL = all channels \n"
"                                  #1 24 = 1 to 24 \n"
"                                  #1.24 = 1 and 24 \n"
"                                  #1-4.7-15 = 1 to 4 and 7 to 15\n"
"	\n"
"	-max_mult_cnt 	<digit>   #If Prot_opt == 2 \n"
"                                  #max_mult_cnt is the number of \n"
"                                  #consecutive duplicate frames \n"
"                                  #received before pass up the stack.\n"
"	\n"
"	-7bit_hdlc	<digit>   #Enable 7 Bit Hdlc Engine\n"
"	                          #1=Enable 0=Disable\n"
"	\n"
"	-dir		<digit>	  #Direction 0: Rx  1: Tx  none: All\n"
"\n"
"<x25 protocol options>\n"
"\n"
"	-txchan		<digit>  #Number of channels    (dflt=1)\n"
"	-cause		<digit>  #disconnect cause      (dflt=0)\n"
"	-diagn		<digit>  #disconnect diagnostic (dflt=0)\n"
"\n";

void usage(unsigned char *api_name)
{
printf ("\n\nAPI %s USAGE:\n\n%s <options> <extra options>\n\n%s\n",
		api_name,api_name,api_usage);
}


/*============================================================================
 * TE1
 */
unsigned long get_active_channels(int channel_flag, int start_channel, int stop_channel)
{
	int i = 0;
	unsigned long tmp = 0, mask = 0;

	if ((channel_flag & (SINGLE_CHANNEL | RANGE_CHANNEL)) == 0)
		return tmp;
	if (channel_flag & RANGE_CHANNEL) { /* Range of channels */
		for(i = start_channel; i <= stop_channel; i++) {
			mask = 1 << (i - 1);
			tmp |=mask;
		}
	} else { /* Single channel */ 
		mask = 1 << (stop_channel - 1);
		tmp |= mask; 
	}
	return tmp;
}


unsigned long parse_active_channel(char* val)
{
	int channel_flag = 0;
	char* ptr = val;
	int channel = 0, start_channel = 0;
	unsigned long tmp = 0;

	if (strcmp(val,"ALL") == 0)
		return ENABLE_ALL_CHANNELS;

	while(*ptr != '\0') {
		if (isdigit(*ptr)) {
			channel = strtoul(ptr, &ptr, 10);
			channel_flag |= SINGLE_CHANNEL;
		} else {
			if (*ptr == '-') {
				channel_flag |= RANGE_CHANNEL;
				start_channel = channel;
			} else {
				tmp |= get_active_channels(channel_flag, start_channel, channel);
				channel_flag = 0;
			}
			ptr++;
		}
	}
	if (channel_flag){
		tmp |= get_active_channels(channel_flag, start_channel, channel);
	}
	return tmp;
}

void u_delay(int usec)
{
	struct timeval tv;
	tv.tv_usec = usec;
	tv.tv_sec=0;

	select(0,NULL,NULL, NULL, &tv);
}
