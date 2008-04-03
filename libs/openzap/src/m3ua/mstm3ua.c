/* WARNING WORK IN PROGRESS
 *  mstm3ua.c
 *  mstss7d port
 *
 *  Created by Shane Burrell on 2/2/08.
 *  Copyright 2008 Shane Burrell. All rights reserved.
 *
 */

#include "mstm3ua.h"


int build_route_context(unsigned char *opc, unsigned char *dpc, unsigned char *bytemsg, unsigned char len)
{
//Routing Context
	bytemsg[8] = 0x10;  //Tag  0x210  Protocol Data
	bytemsg[9] = 0x02;  
	
	bytemsg[10] = len;  //Len
	bytemsg[11] = 0x00;


	bytemsg[12] = opc[2];  //OPC Member
	bytemsg[13] = opc[1];  //OPC Cluster
	bytemsg[14] = opc[0];  //OPC Network
	
	bytemsg[15] = 0x00;
	
	bytemsg[16] = dpc[2];//DPC Member
	bytemsg[17] = dpc[1];//DPC Cluster
	bytemsg[18] = dpc[0];//DPC Network
	
	bytemsg[19] = 0x00;
	return 0;
}



int build_m3ua_hdr(unsigned char len,unsigned char *bytemsg)

{

	bytemsg[0] = M_VERSION_REL1;  // 1 Verison
	//bytemsg[1] = 0x00;  // 2 RESERVED
	//bytemsg[2] = M_CLASS_XFER;  // 3 Msg Class
    //SS7 BOX Kludge
	bytemsg[1] = 0x01;  // 2 RESERVED
	bytemsg[2] = 0x00;  // 2 RESERVED				
	
	bytemsg[3] = M_TYPE_DATA	;  // 4 Msg Type

	bytemsg[4] = len;  // 5 Msg LENGTH  81  32bit field
	bytemsg[5] = 0x00;  // 6
	bytemsg[6] = 0x00;  // 7
	bytemsg[7] = 0x00;  // 8
	return(0);

};