
/*  mstm3ua.h
 *  mstss7d
 *
 *  Created by Shane Burrell on 3/2/08.
 *  Copyright 2008 Shane Burrell. All rights reserved.
 *
 */

#ifndef __SS7_M3UA_H__
#define __SS7_M3UA_H__

typedef unsigned long m3ua_ulong;
typedef unsigned short m3ua_ushort;
typedef unsigned char m3ua_uchar;

typedef unsigned char u8;
typedef unsigned short u16;	/* Note: multi-byte values are little-endian */
typedef unsigned long u32;




#define M_TAG_NETWORK_APPEARANCE	1
#define	M_TAG_PROTOCOL_DATA		3
#define M_TAG_INFO_STRING		4
#define M_TAG_AFFECTED_DPC		5
#define M_TAG_ROUTING_CONTEXT		6
#define M_TAG_DIAGNOSTIC_INFORMATION	7
#define M_TAG_HEARTBEAT_DATA		8
#define M_TAG_UNAVAILABILITY_CAUSE	9
#define M_TAG_REASON			10
#define	M_TAG_TRAFFIC_MODE_TYPE		11
#define M_TAG_ERROR_CODE		12
#define	M_TAG_STATUS_TYPE		13
#define M_TAG_CONGESTED_INDICATIONS	14

#define M_VERSION_REL1   1

#define M_CLASS_MGMT	0x00
#define M_CLASS_XFER	0x01
#define	M_CLASS_SSNM	0x02
#define M_CLASS_ASPSM	0x03
#define M_CLASS_ASPTM	0x04
#define M_CLASS_RKM		0x09

#define M_TYPE_ERR		(0|M_CLASS_MGMT

#define M_TYPE_NTFY		(1|M_CLASS_XFER)
#define M_TYPE_DATA		(1|M_CLASS_XFER)

#define M_TYPE_DUNA		(1|M_CLASS_SSNM)
#define M_TYPE_DAVA		(2|M_CLASS_SSNM)
#define M_TYPE_DUAD		(3|M_CLASS_SSNM)
#define M_TYPE_SCON		(4|M_CLASS_SSNM)
#define M_TYPE_DUPU		(5|M_CLASS_SSNM)

#define	M_TYPE_UP		(1|M_CLASS_ASPSM)
#define	M_TYPE_DOWN		(2|M_CLASS_ASPSM)
#define	M_TYPE_BEAT		(3|M_CLASS_ASPSM)
#define	M_TYPE_UP_ACK		(4|M_CLASS_ASPSM)
#define	M_TYPE_DOWN_ACK		(5|M_CLASS_ASPSM)
#define	M_TYPE_BEAT_ACK		(6|M_CLASS_ASPSM)

#define M_TYPE_ACTIVE		(1|M_CLASS_ASPTM)
#define M_TYPE_INACTIVE		(2|M_CLASS_ASPTM)
#define M_TYPE_ACTIVE_ACK	(3|M_CLASS_ASPTM)
#define M_TYPE_INACTIVE_ACK	(4|M_CLASS_ASPTM)

#define M_CLASS_MASK	0xff00
#define	M_TYPE_MASK	0x00ff



#endif			
