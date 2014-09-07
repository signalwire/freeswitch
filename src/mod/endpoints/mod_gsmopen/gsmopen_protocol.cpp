/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * This module (mod_gsmopen) has been contributed by:
 *
 * Giovanni Maruzzelli <gmaruzz@gmail.com>
 *
 * Maintainer: Giovanni Maruzzelli <gmaruzz@gmail.com>
 *
 * gsmopen_protocol.cpp -- Low Level Interface for mod_gamopen
 *
 */




#include "gsmopen.h"
#ifdef WIN32
#include "win_iconv.c"
#endif // WIN32
#define WANT_GSMLIB

#ifdef WANT_GSMLIB
#include <gsmlib/gsm_sms.h>


using namespace std;
using namespace gsmlib;
#endif // WANT_GSMLIB


extern int running;				//FIXME
int gsmopen_dir_entry_extension = 1;	//FIXME
int option_debug = 100;			//FIXME

#define gsmopen_sleep switch_sleep
#define gsmopen_strncpy switch_copy_string
extern switch_memory_pool_t *gsmopen_module_pool;
extern switch_endpoint_interface_t *gsmopen_endpoint_interface;

#ifdef WIN32
/***************/
// from http://www.openasthra.com/c-tidbits/gettimeofday-function-for-windows/

#include <time.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else /*  */
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif /*  */
struct sk_timezone {
	int tz_minuteswest;			/* minutes W of Greenwich */
	int tz_dsttime;				/* type of dst correction */
};
int gettimeofday(struct timeval *tv, struct sk_timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;
	if (NULL != tv) {
		GetSystemTimeAsFileTime(&ft);
		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch */
		tmpres /= 10;			/*convert into microseconds */
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long) (tmpres / 1000000UL);
		tv->tv_usec = (long) (tmpres % 1000000UL);
	}
	if (NULL != tz) {
		if (!tzflag) {
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}
	return 0;
}

/***************/
#endif /* WIN32 */

int gsmopen_serial_init(private_t *tech_pvt, int controldevice_speed)
{
	if (!tech_pvt)
		return -1;

	tech_pvt->serialPort_serial_control = new ctb::SerialPort();

	/* windows: com ports above com9 need a special trick, which also works for com ports below com10 ... */
	char devname[512] = "";
	strcpy(devname, tech_pvt->controldevice_name);
#ifdef WIN32
	strcpy(devname,"\\\\.\\");
	strcat(devname, tech_pvt->controldevice_name);
#endif

	if (tech_pvt->serialPort_serial_control->Open(devname, 115200, "8N1", ctb::SerialPort::NoFlowControl) >= 0) {
		DEBUGA_GSMOPEN("port %s, SUCCESS open\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
	} else {
#ifdef WIN32
		LPVOID msg;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
							NULL,
							GetLastError(),
							MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
							(LPTSTR) &msg,
							0,
							NULL);
		ERRORA("port open failed for %s - %s", GSMOPEN_P_LOG, devname, (LPCTSTR) msg);
		LocalFree(msg);
#else
		ERRORA("port %s, NOT open\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
#endif
		return -1;
	}

	return 0;
}

int gsmopen_serial_read(private_t *tech_pvt)
{
	if (tech_pvt && tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_read_AT(tech_pvt, 0, 100000, 0, NULL, 1);	// a 10th of a second timeout
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_read_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_read_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */
	return -1;
}

int gsmopen_serial_sync(private_t *tech_pvt)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_sync_AT(tech_pvt);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_sync_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_sync_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */

	return -1;
}

int gsmopen_serial_config(private_t *tech_pvt)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_config_AT(tech_pvt);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_config_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_config_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */

	return -1;
}

int gsmopen_serial_config_AT(private_t *tech_pvt)
{
	int res;
	char at_command[5];
	int i;

	if (!tech_pvt)
		return 0;

/* initial_pause? */
	if (tech_pvt->at_initial_pause) {
		DEBUGA_GSMOPEN("sleeping for %u usec\n", GSMOPEN_P_LOG, tech_pvt->at_initial_pause);
		gsmopen_sleep(tech_pvt->at_initial_pause);
	}

/* go until first empty preinit string, or last preinit string */
	while (1) {

		char trash[4096];
		res = tech_pvt->serialPort_serial_control->Read(trash, 4096);
		if (res) {
			DEBUGA_GSMOPEN("READ %d on serialport init\n", GSMOPEN_P_LOG, res);
		}

		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CFUN=1");
		if (res) {
			DEBUGA_GSMOPEN("no response to AT+CFUN=1. Continuing\n", GSMOPEN_P_LOG);
		}
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT^CURC=0");
		if (res) {
			DEBUGA_GSMOPEN("no response to AT^CURC=0. Continuing\n", GSMOPEN_P_LOG);
		}

		if (strlen(tech_pvt->at_preinit_1)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_1, tech_pvt->at_preinit_1_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_1, tech_pvt->at_preinit_1_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_preinit_2)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_2, tech_pvt->at_preinit_2_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_2, tech_pvt->at_preinit_2_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_preinit_3)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_3, tech_pvt->at_preinit_3_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_3, tech_pvt->at_preinit_3_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_preinit_4)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_4, tech_pvt->at_preinit_4_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_4, tech_pvt->at_preinit_4_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_preinit_5)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_5, tech_pvt->at_preinit_5_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_5, tech_pvt->at_preinit_5_expect);
			}
		} else {
			break;
		}

		break;
	}

/* after_preinit_pause? */
	if (tech_pvt->at_after_preinit_pause) {
		DEBUGA_GSMOPEN("sleeping for %u usec\n", GSMOPEN_P_LOG, tech_pvt->at_after_preinit_pause);
		gsmopen_sleep(tech_pvt->at_after_preinit_pause);
	}

	/* phone, brother, art you alive? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT");
	if (res) {
		ERRORA("no response to AT\n", GSMOPEN_P_LOG);
		return -1;
	}

	/* for motorola, bring it back to "normal" mode if it happens to be in another mode */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+mode=0");
	if (res) {
		DEBUGA_GSMOPEN("AT+mode=0 didn't get OK from the phone. If it is NOT Motorola," " no problem.\n", GSMOPEN_P_LOG);
	}
	gsmopen_sleep(50000);
	/* for motorola end */

	/* reset AT configuration to phone default */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "ATZ");
	if (res) {
		DEBUGA_GSMOPEN("ATZ failed\n", GSMOPEN_P_LOG);
	}

	/* disable AT command echo */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "ATE0");
	if (res) {
		DEBUGA_GSMOPEN("ATE0 failed\n", GSMOPEN_P_LOG);
	}

	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CFUN=1");
	if (res) {
		DEBUGA_GSMOPEN("no response to AT+CFUN=1. Continuing\n", GSMOPEN_P_LOG);
	}
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT^CURC=0");
	if (res) {
		DEBUGA_GSMOPEN("no response to AT^CURC=0. Continuing\n", GSMOPEN_P_LOG);
	}


	/* disable extended error reporting */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMEE=0");
	if (res) {
		DEBUGA_GSMOPEN("AT+CMEE failed\n", GSMOPEN_P_LOG);
	}

	/* various phone manufacturer identifier */
	for (i = 0; i < 10; i++) {
		memset(at_command, 0, sizeof(at_command));
		sprintf(at_command, "ATI%d", i);
		res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
		if (res) {
			DEBUGA_GSMOPEN("ATI%d command failed, continue\n", GSMOPEN_P_LOG, i);
		}
	}

	/* phone manufacturer */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CGMI");
	if (res) {
		DEBUGA_GSMOPEN("AT+CGMI failed\n", GSMOPEN_P_LOG);
	}

	/* phone model */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CGMM");
	if (res) {
		DEBUGA_GSMOPEN("AT+CGMM failed\n", GSMOPEN_P_LOG);
	}

	/* signal network registration with a +CREG unsolicited msg */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CREG=1");
	if (res) {
		DEBUGA_GSMOPEN("AT+CREG=1 failed\n", GSMOPEN_P_LOG);
		tech_pvt->network_creg_not_supported = 1;
	}
	if (!tech_pvt->network_creg_not_supported) {
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CREG?");
		if (res) {
			DEBUGA_GSMOPEN("AT+CREG? failed\n", GSMOPEN_P_LOG);
		}
	}
	/* query signal strength */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSQ");
	if (res) {
		DEBUGA_GSMOPEN("AT+CSQ failed\n", GSMOPEN_P_LOG);
	}
	/* IMEI */
	tech_pvt->requesting_imei = 1;
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+GSN");
	tech_pvt->requesting_imei = 0;
	if (res) {
		DEBUGA_GSMOPEN("AT+GSN failed\n", GSMOPEN_P_LOG);
		tech_pvt->requesting_imei = 1;
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CGSN");
		tech_pvt->requesting_imei = 0;
		if (res) {
			DEBUGA_GSMOPEN("AT+CGSN failed\n", GSMOPEN_P_LOG);
		}
	}
	/* IMSI */
	tech_pvt->requesting_imsi = 1;
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CIMI");
	tech_pvt->requesting_imsi = 0;
	if (res) {
		DEBUGA_GSMOPEN("AT+CIMI failed\n", GSMOPEN_P_LOG);
	}

	/* signal incoming SMS with a +CMTI unsolicited msg */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CNMI=2,1,0,0,0");
	if (res) {
		DEBUGA_GSMOPEN("AT+CNMI=2,1,0,0,0 failed, continue\n", GSMOPEN_P_LOG);
		tech_pvt->sms_cnmi_not_supported = 1;
		tech_pvt->gsmopen_serial_sync_period = 30;	//FIXME in config
	}
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CPMS=\"ME\",\"ME\",\"ME\"");
	if (res) {
		DEBUGA_GSMOPEN("no response to AT+CPMS=\"ME\",\"ME\",\"ME\". Continuing\n", GSMOPEN_P_LOG);
	}
	/* signal incoming SMS with a +CMTI unsolicited msg */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CNMI=2,1,0,0,0");
	if (res) {
		DEBUGA_GSMOPEN("AT+CNMI=2,1,0,0,0 failed, continue\n", GSMOPEN_P_LOG);
		tech_pvt->sms_cnmi_not_supported = 1;
		tech_pvt->gsmopen_serial_sync_period = 30;	//FIXME in config
	}
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CPMS=\"ME\",\"ME\",\"ME\"");
	if (res) {
		DEBUGA_GSMOPEN("no response to AT+CPMS=\"ME\",\"ME\",\"ME\". Continuing\n", GSMOPEN_P_LOG);
	}
	/* signal incoming SMS with a +CMTI unsolicited msg */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CNMI=2,1,0,0,0");
	if (res) {
		DEBUGA_GSMOPEN("AT+CNMI=2,1,0,0,0 failed, continue\n", GSMOPEN_P_LOG);
		tech_pvt->sms_cnmi_not_supported = 1;
		tech_pvt->gsmopen_serial_sync_period = 30;	//FIXME in config
	}

	/* what is the Message Center address (number) to which the SMS has to be sent? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCA?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CSCA? failed, continue\n", GSMOPEN_P_LOG);
	}
	/* what is the Message Format of SMSs? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CMGF? failed, continue\n", GSMOPEN_P_LOG);
	}
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=1");
	if (res) {
		ERRORA("Error setting SMS sending mode to TEXT on the cellphone, let's hope is TEXT by default. Continuing\n", GSMOPEN_P_LOG);
	}
	tech_pvt->sms_pdu_not_supported = 1;

	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSMP=17,167,0,8");	//unicode, 16 bit message
	if (res) {
		WARNINGA("AT+CSMP didn't get OK from the phone, continuing\n", GSMOPEN_P_LOG);
	}

	/* what is the Charset of SMSs? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CSCS? failed, continue\n", GSMOPEN_P_LOG);
	}

	tech_pvt->no_ucs2 = 0;
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS=\"UCS2\"");
	if (res) {
		WARNINGA("AT+CSCS=\"UCS2\" (set TE messages to ucs2) didn't get OK from the phone, let's try with 'GSM'\n", GSMOPEN_P_LOG);
		tech_pvt->no_ucs2 = 1;
	}
	if (tech_pvt->no_ucs2) {
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS=\"GSM\"");
		if (res) {
			WARNINGA("AT+CSCS=\"GSM\" (set TE messages to GSM) didn't get OK from the phone\n", GSMOPEN_P_LOG);
		}
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSMP=17,167,0,0");	//normal, 7 bit message
		if (res) {
			WARNINGA("AT+CSMP didn't get OK from the phone, continuing\n", GSMOPEN_P_LOG);
		}
	}

	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=0");
	if (res) {
		ERRORA("Error setting SMS sending mode to TEXT on the cellphone, let's hope is TEXT by default. Continuing\n", GSMOPEN_P_LOG);
	}
	tech_pvt->sms_pdu_not_supported = 0;
	tech_pvt->no_ucs2 = 1;

	/* is the unsolicited reporting of mobile equipment event supported? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMER=?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CMER=? failed, continue\n", GSMOPEN_P_LOG);
	}
	/* request unsolicited reporting of mobile equipment indicators' events, to be screened by categories reported by +CIND=? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMER=3,0,0,1");
	if (res) {
		DEBUGA_GSMOPEN("AT+CMER=? failed, continue\n", GSMOPEN_P_LOG);
	}

	/* is the solicited reporting of mobile equipment indications supported? */

	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CIND=?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CIND=? failed, continue\n", GSMOPEN_P_LOG);
	}

	/* is the unsolicited reporting of call monitoring supported? sony-ericsson specific */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT*ECAM=?");
	if (res) {
		DEBUGA_GSMOPEN("AT*ECAM=? failed, continue\n", GSMOPEN_P_LOG);
	}
	/* enable the unsolicited reporting of call monitoring. sony-ericsson specific */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT*ECAM=1");
	if (res) {
		DEBUGA_GSMOPEN("AT*ECAM=1 failed, continue\n", GSMOPEN_P_LOG);
		tech_pvt->at_has_ecam = 0;
	} else {
		tech_pvt->at_has_ecam = 1;
	}

	/* disable unsolicited signaling of call list */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CLCC=0");
	if (res) {
		DEBUGA_GSMOPEN("AT+CLCC=0 failed, continue\n", GSMOPEN_P_LOG);
		tech_pvt->at_has_clcc = 0;
	} else {
		tech_pvt->at_has_clcc = 1;
	}

	/* give unsolicited caller id when incoming call */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CLIP=1");
	if (res) {
		DEBUGA_GSMOPEN("AT+CLIP failed, continue\n", GSMOPEN_P_LOG);
	}
	/* for motorola */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+MCST=1");	/* motorola call control codes
																   (to know when call is disconnected (they
																   don't give you "no carrier") */
	if (res) {
		DEBUGA_GSMOPEN("AT+MCST=1 didn't get OK from the phone. If it is NOT Motorola," " no problem.\n", GSMOPEN_P_LOG);
	}
	/* for motorola end */

/* go until first empty postinit string, or last postinit string */
	while (1) {

		if (strlen(tech_pvt->at_postinit_1)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_1, tech_pvt->at_postinit_1_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_1, tech_pvt->at_postinit_1_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_postinit_2)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_2, tech_pvt->at_postinit_2_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_2, tech_pvt->at_postinit_2_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_postinit_3)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_3, tech_pvt->at_postinit_3_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_3, tech_pvt->at_postinit_3_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_postinit_4)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_4, tech_pvt->at_postinit_4_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_4, tech_pvt->at_postinit_4_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_postinit_5)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_5, tech_pvt->at_postinit_5_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_5, tech_pvt->at_postinit_5_expect);
			}
		} else {
			break;
		}

		break;
	}

	return 0;
}

int gsmopen_serial_sync_AT(private_t *tech_pvt)
{
	gsmopen_sleep(10000);		/* 10msec */
	time(&tech_pvt->gsmopen_serial_synced_timestamp);
	return 0;
}

int gsmopen_serial_read_AT(private_t *tech_pvt, int look_for_ack, int timeout_usec, int timeout_sec, const char *expected_string, int expect_crlf)
{
	int select_err = 1;
	struct timeval timeout;
	char tmp_answer[AT_BUFSIZ];
	char tmp_answer2[AT_BUFSIZ];
	char tmp_answer3[AT_BUFSIZ];
	char *tmp_answer_ptr;
	char *last_line_ptr;
	int i = 0;
	int read_count = 0;
	int la_counter = 0;
	int at_ack = -1;
	int la_read = 0;
	int timeout_in_msec;
	int msecs_passed = 0;

	timeout_in_msec = (timeout_sec * 1000) + (timeout_usec ? (timeout_usec / 1000) : 0);

	if (timeout_in_msec != 100)
		DEBUGA_GSMOPEN("TIMEOUT=%d\n", GSMOPEN_P_LOG, timeout_in_msec);

	if (!running || !tech_pvt || !tech_pvt->running) {
		return -1;
	}

	tmp_answer_ptr = tmp_answer;
	memset(tmp_answer, 0, sizeof(char) * AT_BUFSIZ);
	memset(tmp_answer2, 0, sizeof(char) * AT_BUFSIZ);
	memset(tmp_answer3, 0, sizeof(char) * AT_BUFSIZ);

	timeout.tv_sec = timeout_sec;
	timeout.tv_usec = timeout_usec;
	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);

	while ((!tech_pvt->controldev_dead) && msecs_passed <= timeout_in_msec) {
		char *token_ptr;
		timeout.tv_sec = timeout_sec;	//reset the timeout, linux modify it
		timeout.tv_usec = timeout_usec;	//reset the timeout, linux modify it

	  read:
		switch_sleep(20000);
		msecs_passed += 20;

		if (timeout_in_msec != 100) {
			//ERRORA("TIMEOUT=%d, PASSED=%d\n", GSMOPEN_P_LOG, timeout_in_msec, msecs_passed);
		}
		read_count = tech_pvt->serialPort_serial_control->Read(tmp_answer_ptr, AT_BUFSIZ - (tmp_answer_ptr - tmp_answer));
		memset(tmp_answer3, 0, sizeof(char) * AT_BUFSIZ);
		strcpy(tmp_answer3, tmp_answer_ptr);

		if (read_count == 0) {
			if (msecs_passed <= timeout_in_msec) {
				goto read;
			}
		}
		if (read_count == -1) {
			ERRORA
				("read -1 bytes!!! Nenormalno! Marking this gsmopen_serial_device %s as dead, andif it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, power down or battery exhausted\n",
				 GSMOPEN_P_LOG, tech_pvt->controldevice_name);
			tech_pvt->controldev_dead = 1;
			ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
			tech_pvt->running = 0;
			alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
			tech_pvt->active = 0;
			tech_pvt->name[0] = '\0';

			UNLOCKA(tech_pvt->controldev_lock);
			if (tech_pvt->owner) {
				tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
				gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
			}
			switch_sleep(1000000);
			return -1;
		}

		tmp_answer_ptr = tmp_answer_ptr + read_count;

		la_counter = 0;
		memset(tmp_answer2, 0, sizeof(char) * AT_BUFSIZ);
		strcpy(tmp_answer2, tmp_answer);
		if ((token_ptr = strtok(tmp_answer2, "\n\r"))) {
			last_line_ptr = token_ptr;
			strncpy(tech_pvt->line_array.result[la_counter], token_ptr, AT_MESG_MAX_LENGTH);
			if (strlen(token_ptr) > AT_MESG_MAX_LENGTH) {
				WARNINGA
					("AT mesg longer than buffer, original message was: |%s|, in buffer only: |%s|\n",
					 GSMOPEN_P_LOG, token_ptr, tech_pvt->line_array.result[la_counter]);
			}
			la_counter++;

			while ((token_ptr = strtok(NULL, "\n\r"))) {
				last_line_ptr = token_ptr;
				strncpy(tech_pvt->line_array.result[la_counter], token_ptr, AT_MESG_MAX_LENGTH);
				if (strlen(token_ptr) > AT_MESG_MAX_LENGTH) {
					WARNINGA
						("AT mesg longer than buffer, original message was: |%s|, in buffer only: |%s|\n",
						 GSMOPEN_P_LOG, token_ptr, tech_pvt->line_array.result[la_counter]);
				}
				la_counter++;

				if (la_counter == AT_MESG_MAX_LINES) {
					ERRORA("Too many lines in result (>%d). la_counter=%d. tech_pvt->reading_sms_msg=%d. Stop accumulating lines.\n", GSMOPEN_P_LOG,
						   AT_MESG_MAX_LINES, la_counter, tech_pvt->reading_sms_msg);
					WARNINGA("read was %d bytes, tmp_answer3= --|%s|--\n", GSMOPEN_P_LOG, read_count, tmp_answer3);
					at_ack = AT_ERROR;
					break;
				}

			}
		} else {
			last_line_ptr = tmp_answer;
		}

		if (expected_string && !expect_crlf) {
			DEBUGA_GSMOPEN
				("last_line_ptr=|%s|, expected_string=|%s|, expect_crlf=%d, memcmp(last_line_ptr, expected_string, strlen(expected_string)) = %d\n",
				 GSMOPEN_P_LOG, last_line_ptr, expected_string, expect_crlf, memcmp(last_line_ptr, expected_string, strlen(expected_string)));
		}

		if (expected_string && !expect_crlf && !memcmp(last_line_ptr, expected_string, strlen(expected_string))
			) {
			strncpy(tech_pvt->line_array.result[la_counter], last_line_ptr, AT_MESG_MAX_LENGTH);
			// match expected string -> accept it withtout CRLF
			la_counter++;
			if (la_counter == AT_MESG_MAX_LINES) {
				ERRORA("Too many lines in result (>%d). la_counter=%d. tech_pvt->reading_sms_msg=%d. Stop accumulating lines.\n", GSMOPEN_P_LOG,
					   AT_MESG_MAX_LINES, la_counter, tech_pvt->reading_sms_msg);
				WARNINGA("read was %d bytes, tmp_answer3= --|%s|--\n", GSMOPEN_P_LOG, read_count, tmp_answer3);
				at_ack = AT_ERROR;
				break;
			}
		}
		/* if the last line read was not a complete line, we'll read the rest in the future */
		else if (tmp_answer[strlen(tmp_answer) - 1] != '\r' && tmp_answer[strlen(tmp_answer) - 1] != '\n')
			la_counter--;

		/* let's list the complete lines read so far, without re-listing the lines that have already been listed */
		for (i = la_read; i < la_counter; i++) {
			DEBUGA_GSMOPEN("Read line %d: |%s| la_counter=%d\n", GSMOPEN_P_LOG, i, tech_pvt->line_array.result[i], la_counter);
		}

		if (la_counter == AT_MESG_MAX_LINES) {
			ERRORA("Too many lines in result (>%d). la_counter=%d. tech_pvt->reading_sms_msg=%d. Stop accumulating lines.\n", GSMOPEN_P_LOG,
				   AT_MESG_MAX_LINES, la_counter, tech_pvt->reading_sms_msg);
			WARNINGA("read was %d bytes, tmp_answer3= --|%s|--\n", GSMOPEN_P_LOG, read_count, tmp_answer3);
			at_ack = AT_ERROR;
			break;
		}


		/* let's interpret the complete lines read so far (WITHOUT looking for OK, ERROR, and EXPECTED_STRING), without re-interpreting the lines that has been yet interpreted, so we're sure we don't miss anything */
		for (i = la_read; i < la_counter; i++) {

			if ((strcmp(tech_pvt->line_array.result[i], "RING") == 0)) {
				/* with first RING we wait for callid */
				gettimeofday(&(tech_pvt->ringtime), NULL);
				/* give CALLID (+CLIP) a chance, wait for the next RING before answering */
				if (tech_pvt->phone_callflow == CALLFLOW_INCOMING_RING) {
					/* we're at the second ring, set the interface state, will be answered by gsmopen_do_monitor */
					DEBUGA_GSMOPEN("|%s| got second RING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					tech_pvt->interface_state = GSMOPEN_STATE_RING;
				} else {
					/* we're at the first ring, so there is no CALLID yet thus clean the previous one
					   just in case we don't receive the caller identification in this new call */
					memset(tech_pvt->callid_name, 0, sizeof(tech_pvt->callid_name));
					memset(tech_pvt->callid_number, 0, sizeof(tech_pvt->callid_number));
					/* only send AT+CLCC? if the device previously reported its support */
					if (tech_pvt->at_has_clcc != 0) {
						/* we're at the first ring, try to get CALLID (with +CLCC) */
						DEBUGA_GSMOPEN("|%s| got first RING, sending AT+CLCC?\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						int res = gsmopen_serial_write_AT_noack(tech_pvt, "AT+CLCC?");
						if (res) {
							ERRORA("AT+CLCC? (call list) was not correctly sent to the phone\n", GSMOPEN_P_LOG);
						}
					} else {
						DEBUGA_GSMOPEN("|%s| got first RING, but not sending AT+CLCC? as this device "
									   "seems not to support\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					}
				}
				tech_pvt->phone_callflow = CALLFLOW_INCOMING_RING;
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CLCC", 5) == 0)) {
				int commacount = 0;
				int a = 0;
				int b = 0;
				int c = 0;
				/* with clcc we wait for clip */
				memset(tech_pvt->callid_name, 0, sizeof(tech_pvt->callid_name));
				memset(tech_pvt->callid_number, 0, sizeof(tech_pvt->callid_number));

				for (a = 0; a < (int) strlen(tech_pvt->line_array.result[i]); a++) {

					if (tech_pvt->line_array.result[i][a] == ',') {
						commacount++;
					}
					if (commacount == 5) {
						if (tech_pvt->line_array.result[i][a] != ',' && tech_pvt->line_array.result[i][a] != '"') {
							tech_pvt->callid_number[b] = tech_pvt->line_array.result[i][a];
							b++;
						}
					}
					if (commacount == 7) {
						if (tech_pvt->line_array.result[i][a] != ',' && tech_pvt->line_array.result[i][a] != '"') {
							tech_pvt->callid_name[c] = tech_pvt->line_array.result[i][a];
							c++;
						}
					}
				}

				tech_pvt->phone_callflow = CALLFLOW_INCOMING_RING;
				DEBUGA_GSMOPEN("|%s| CLCC CALLID: name is %s, number is %s\n", GSMOPEN_P_LOG,
							   tech_pvt->line_array.result[i],
							   tech_pvt->callid_name[0] ? tech_pvt->callid_name : "not available",
							   tech_pvt->callid_number[0] ? tech_pvt->callid_number : "not available");
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CLIP", 5) == 0)) {
				int commacount = 0;
				int a = 0;
				int b = 0;
				int c = 0;
				/* with CLIP, we want to answer right away */
				memset(tech_pvt->callid_name, 0, sizeof(tech_pvt->callid_name));
				memset(tech_pvt->callid_number, 0, sizeof(tech_pvt->callid_number));

				for (a = 7; a < (int) strlen(tech_pvt->line_array.result[i]); a++) {
					if (tech_pvt->line_array.result[i][a] == ',') {
						commacount++;
					}
					if (commacount == 0) {
						if (tech_pvt->line_array.result[i][a] != ',' && tech_pvt->line_array.result[i][a] != '"') {
							tech_pvt->callid_number[b] = tech_pvt->line_array.result[i][a];
							b++;
						}
					}
					if (commacount == 4) {
						if (tech_pvt->line_array.result[i][a] != ',' && tech_pvt->line_array.result[i][a] != '"') {
							tech_pvt->callid_name[c] = tech_pvt->line_array.result[i][a];
							c++;
						}
					}
				}

				if (tech_pvt->interface_state != GSMOPEN_STATE_RING) {
					gettimeofday(&(tech_pvt->call_incoming_time), NULL);
					DEBUGA_GSMOPEN("GSMOPEN_STATE_RING call_incoming_time.tv_sec=%ld\n", GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec);

				}

				tech_pvt->interface_state = GSMOPEN_STATE_RING;
				tech_pvt->phone_callflow = CALLFLOW_INCOMING_RING;
				DEBUGA_GSMOPEN("|%s| CLIP INCOMING CALLID: name is %s, number is %s\n", GSMOPEN_P_LOG,
							   tech_pvt->line_array.result[i],
							   (strlen(tech_pvt->callid_name) && tech_pvt->callid_name[0] != 1) ? tech_pvt->callid_name : "not available",
							   strlen(tech_pvt->callid_number) ? tech_pvt->callid_number : "not available");

				if (!strlen(tech_pvt->callid_number)) {
					strcpy(tech_pvt->callid_number, "not available");
				}

				if (!strlen(tech_pvt->callid_name) && tech_pvt->callid_name[0] != 1) {
					strncpy(tech_pvt->callid_name, tech_pvt->callid_number, sizeof(tech_pvt->callid_name));
					snprintf(tech_pvt->callid_name, sizeof(tech_pvt->callid_name), "GSMopen: %s", tech_pvt->callid_number);
				}

				DEBUGA_GSMOPEN("|%s| CLIP INCOMING CALLID: NOW name is %s, number is %s\n", GSMOPEN_P_LOG,
							   tech_pvt->line_array.result[i], tech_pvt->callid_name, tech_pvt->callid_number);
			}

			if ((strcmp(tech_pvt->line_array.result[i], "+CMS ERROR: 500") == 0)) {
				ERRORA("Received: \"%s\", generic error, maybe this account ran OUT OF CREDIT?\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			} else if ((strncmp(tech_pvt->line_array.result[i], "+CMS ERROR:", 11) == 0)) {
				ERRORA("Received: \"%s\", what was this error about?\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}
			if ((strcmp(tech_pvt->line_array.result[i], "BUSY") == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_LINEBUSY;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_LINEBUSY\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->phone_callflow != CALLFLOW_CALL_DOWN) {
					switch_core_session_t *session = NULL;
					switch_channel_t *channel = NULL;

					tech_pvt->interface_state = GSMOPEN_STATE_DOWN;

					session = switch_core_session_locate(tech_pvt->session_uuid_str);
					if (session) {
						channel = switch_core_session_get_channel(session);
						switch_core_session_rwunlock(session);
						switch_channel_hangup(channel, SWITCH_CAUSE_NONE);
					}

				} else {
					ERRORA("Why BUSY now?\n", GSMOPEN_P_LOG);
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "NO ANSWER") == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_NOANSWER;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_NOANSWER\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_NO_ANSWER;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				} else {
					ERRORA("Why NO ANSWER now?\n", GSMOPEN_P_LOG);
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "NO CARRIER") == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_NOCARRIER;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_NOCARRIER\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
					switch_core_session_t *session = NULL;
					switch_channel_t *channel = NULL;

					tech_pvt->interface_state = GSMOPEN_STATE_DOWN;

					session = switch_core_session_locate(tech_pvt->session_uuid_str);
					if (session) {
						channel = switch_core_session_get_channel(session);
						switch_core_session_rwunlock(session);
						switch_channel_hangup(channel, SWITCH_CAUSE_NONE);
					}
				} else {
					ERRORA("Why NO CARRIER now?\n", GSMOPEN_P_LOG);
				}
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CBC:", 5) == 0)) {
				int power_supply, battery_strenght, err;

				power_supply = battery_strenght = 0;

				err = sscanf(&tech_pvt->line_array.result[i][6], "%d,%d", &power_supply, &battery_strenght);
				if (err < 2) {
					DEBUGA_GSMOPEN("|%s| is not formatted as: |+CBC: xx,yy| now trying  |+CBC:xx,yy|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

					err = sscanf(&tech_pvt->line_array.result[i][5], "%d,%d", &power_supply, &battery_strenght);
					DEBUGA_GSMOPEN("|%s| +CBC: Powered by %s, battery strenght=%d\n", GSMOPEN_P_LOG,
								   tech_pvt->line_array.result[i], power_supply ? "power supply" : "battery", battery_strenght);

				}

				if (err < 2) {
					DEBUGA_GSMOPEN("|%s| is not formatted as: |+CBC:xx,yy| giving up\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				}

				else {
					if (option_debug > 1)
						DEBUGA_GSMOPEN("|%s| +CBC: Powered by %s, battery strenght=%d\n", GSMOPEN_P_LOG,
									   tech_pvt->line_array.result[i], power_supply ? "power supply" : "battery", battery_strenght);
					if (!power_supply) {
						if (battery_strenght < 10) {
							ERRORA("|%s| BATTERY ALMOST EXHAUSTED\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						} else if (battery_strenght < 20) {
							WARNINGA("|%s| BATTERY LOW\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

						}

					}
				}

			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CSQ:", 5) == 0)) {
				int signal_quality, ber, err;

				signal_quality = ber = 0;

				err = sscanf(&tech_pvt->line_array.result[i][6], "%d,%d", &signal_quality, &ber);
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| +CSQ: Signal Quality: %d, Error Rate=%d\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i], signal_quality, ber);
				if (err < 2) {
					ERRORA("|%s| is not formatted as: |+CSQ: xx,yy|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				} else {
					if (signal_quality < 9 || signal_quality == 99) {
						ERRORA
							("|%s| CELLPHONE GETS ALMOST NO SIGNAL, consider to move it or use additional antenna\n",
							 GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						tech_pvt->got_signal = 0;
						alarm_event(tech_pvt, ALARM_NETWORK_NO_SIGNAL, "CELLPHONE GETS ALMOST NO SIGNAL, consider to move it or use additional antenna");
					} else if (signal_quality < 11) {
						WARNINGA("|%s| CELLPHONE GETS SIGNAL LOW\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						tech_pvt->got_signal = 1;
						alarm_event(tech_pvt, ALARM_NETWORK_LOW_SIGNAL, "CELLPHONE GETS SIGNAL LOW");
					} else {
						tech_pvt->got_signal = 2;
					}

				}

			}
			if ((strncmp(tech_pvt->line_array.result[i], "+CREG:", 6) == 0)) {
				int n, stat, err;

				n = stat = 0;

				err = sscanf(&tech_pvt->line_array.result[i][6], "%d,%d", &n, &stat);
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| +CREG: Display: %d, Registration=%d\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i], n, stat);
				if (err < 2) {
					DEBUGA_GSMOPEN("|%s| is not formatted as: |+CREG: xx,yy|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				} else {
					if (stat == 0) {
						ERRORA
							("|%s| CELLPHONE is not registered to network, consider to move it or use additional antenna\n",
							 GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						tech_pvt->not_registered = 1;
						tech_pvt->home_network_registered = 0;
						tech_pvt->roaming_registered = 0;
						alarm_event(tech_pvt, ALARM_NO_NETWORK_REGISTRATION,
									"CELLPHONE is not registered to network, consider to move it or use additional antenna");
					} else if (stat == 1) {
						DEBUGA_GSMOPEN("|%s| CELLPHONE is registered to the HOME network\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						tech_pvt->not_registered = 0;
						tech_pvt->home_network_registered = 1;
						tech_pvt->roaming_registered = 0;
					} else {
						ERRORA("|%s| CELLPHONE is registered to a ROAMING network\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						tech_pvt->not_registered = 0;
						tech_pvt->home_network_registered = 0;
						tech_pvt->roaming_registered = 1;
						alarm_event(tech_pvt, ALARM_ROAMING_NETWORK_REGISTRATION, "CELLPHONE is registered to a ROAMING network");
					}
				}

			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CMGW:", 6) == 0)) {
				int err;

				err = sscanf(&tech_pvt->line_array.result[i][7], "%s", tech_pvt->at_cmgw);
				DEBUGA_GSMOPEN("|%s| +CMGW: %s\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i], tech_pvt->at_cmgw);
				if (err < 1) {
					ERRORA("|%s| is not formatted as: |+CMGW: xxxx|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				}

			}

			if ((strncmp(tech_pvt->line_array.result[i], "^CEND:1", 7) == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_NOCARRIER;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_NOCARRIER\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
					switch_core_session_t *session = NULL;
					switch_channel_t *channel = NULL;

					tech_pvt->interface_state = GSMOPEN_STATE_DOWN;

					session = switch_core_session_locate(tech_pvt->session_uuid_str);
					if (session) {
						channel = switch_core_session_get_channel(session);
						switch_core_session_rwunlock(session);
						switch_channel_hangup(channel, SWITCH_CAUSE_NONE);
					}
					tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
					if (option_debug > 1)
						DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_IDLE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
						DEBUGA_GSMOPEN("just received a remote HANGUP\n", GSMOPEN_P_LOG);
						tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_NORMAL;
						gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
						DEBUGA_GSMOPEN("just sent GSMOPEN_CONTROL_HANGUP\n", GSMOPEN_P_LOG);
					}
				} else {
					ERRORA("Why NO CARRIER now?\n", GSMOPEN_P_LOG);
				}
			}

			/* at_call_* are unsolicited messages sent by the modem to signal us about call processing activity and events */
			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_idle) == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_IDLE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					DEBUGA_GSMOPEN("just received a remote HANGUP\n", GSMOPEN_P_LOG);
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_NORMAL;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
					DEBUGA_GSMOPEN("just sent GSMOPEN_CONTROL_HANGUP\n", GSMOPEN_P_LOG);
				}

				tech_pvt->phone_callflow = CALLFLOW_CALL_NOCARRIER;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_NOCARRIER\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
					switch_core_session_t *session = NULL;
					switch_channel_t *channel = NULL;

					tech_pvt->interface_state = GSMOPEN_STATE_DOWN;

					session = switch_core_session_locate(tech_pvt->session_uuid_str);
					if (session) {
						channel = switch_core_session_get_channel(session);
						switch_core_session_rwunlock(session);
						switch_channel_hangup(channel, SWITCH_CAUSE_NONE);
					}
				} else {
					ERRORA("Why NO CARRIER now?\n", GSMOPEN_P_LOG);
				}

			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_incoming) == 0)) {


				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_INCOMING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

				if (tech_pvt->phone_callflow != CALLFLOW_CALL_INCOMING && tech_pvt->phone_callflow != CALLFLOW_INCOMING_RING) {
					//mark the time of CALLFLOW_CALL_INCOMING
					gettimeofday(&(tech_pvt->call_incoming_time), NULL);
					tech_pvt->phone_callflow = CALLFLOW_CALL_INCOMING;
					DEBUGA_GSMOPEN("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld\n", GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec);

				}
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_active) == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_ACTIVE;
				DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_ACTIVE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

				if (tech_pvt->interface_state == CALLFLOW_CALL_DIALING || tech_pvt->interface_state == CALLFLOW_STATUS_EARLYMEDIA) {
					DEBUGA_PBX("just received a remote ANSWER\n", GSMOPEN_P_LOG);
					if (tech_pvt->phone_callflow == GSMOPEN_STATE_UP) {
						DEBUGA_PBX("just sent GSMOPEN_CONTROL_RINGING\n", GSMOPEN_P_LOG);
						DEBUGA_PBX("going to send GSMOPEN_CONTROL_ANSWER\n", GSMOPEN_P_LOG);
						tech_pvt->interface_state = CALLFLOW_CALL_REMOTEANSWER;
						DEBUGA_PBX("just sent GSMOPEN_CONTROL_ANSWER\n", GSMOPEN_P_LOG);
					}
				} else {
					tech_pvt->interface_state = GSMOPEN_STATE_UP;
					DEBUGA_PBX("just interface_state UP\n", GSMOPEN_P_LOG);
				}
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_calling) == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_DIALING;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_DIALING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}
			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_failed) == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_FAILED;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_FAILED\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CSCA:", 6) == 0)) {	//TODO SMS FIXME in config!
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| +CSCA: Message Center Address!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CMGF:", 6) == 0)) {	//TODO SMS FIXME in config!
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| +CMGF: Message Format!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CMTI:", 6) == 0)) {	//TODO SMS FIXME in config!
				int err;
				int pos;

				//FIXME all the following commands in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMTI: Incoming SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

				err = sscanf(&tech_pvt->line_array.result[i][12], "%d", &pos);
				if (err < 1) {
					ERRORA("|%s| is not formatted as: |+CMTI: \"MT\",xx|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				} else {
					DEBUGA_GSMOPEN("|%s| +CMTI: Incoming SMS in position: %d!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i], pos);
					tech_pvt->unread_sms_msg_id = pos;
					gsmopen_sleep(1000);

					char at_command[256];

					if (tech_pvt->no_ucs2 == 0) {
						int res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS=\"UCS2\"");
						if (res) {
							ERRORA("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  didn't get OK from the phone, continuing\n", GSMOPEN_P_LOG);
							//memset(tech_pvt->sms_message, 0, sizeof(tech_pvt->sms_message));
						}
					}

					memset(at_command, 0, sizeof(at_command));
					sprintf(at_command, "AT+CMGR=%d", tech_pvt->unread_sms_msg_id);
					memset(tech_pvt->sms_message, 0, sizeof(tech_pvt->sms_message));

					tech_pvt->reading_sms_msg = 1;
					int res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
					tech_pvt->reading_sms_msg = 0;
					if (res) {
						ERRORA("AT+CMGR (read SMS) didn't get OK from the phone, message sent was:|||%s|||\n", GSMOPEN_P_LOG, at_command);
					}
					memset(at_command, 0, sizeof(at_command));
					sprintf(at_command, "AT+CMGD=%d", tech_pvt->unread_sms_msg_id);	/* delete the message */
					tech_pvt->unread_sms_msg_id = 0;
					res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
					if (res) {
						ERRORA("AT+CMGD (Delete SMS) didn't get OK from the phone, message sent was:|||%s|||\n", GSMOPEN_P_LOG, at_command);
					}

					res = sms_incoming(tech_pvt);

					if (tech_pvt->phone_callflow == CALLFLOW_CALL_IDLE && tech_pvt->interface_state == GSMOPEN_STATE_DOWN && tech_pvt->owner == NULL) {
						/* we're not in a call, neither calling */
						res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CKPD=\"EEE\"");
						if (res) {
							DEBUGA_GSMOPEN("AT+CKPD=\"EEE\" (cellphone screen back to user) didn't get OK from the phone\n", GSMOPEN_P_LOG);
						}
					}

				}				//CMTI well formatted

			}					//CMTI

			if ((strncmp(tech_pvt->line_array.result[i], "+MMGL:", 6) == 0)) {	//TODO MOTOROLA SMS FIXME in config!
				int err = 0;

				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +MMGL: Listing Motorola SMSs!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

				err = sscanf(&tech_pvt->line_array.result[i][7], "%d", &tech_pvt->unread_sms_msg_id);
				if (err < 1) {
					ERRORA("|%s| is not formatted as: |+MMGL: xx|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				}
			}
			if ((strncmp(tech_pvt->line_array.result[i], "+CMGL:", 6) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGL: Listing SMSs!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}
			if ((strncmp(tech_pvt->line_array.result[i], "+MMGR:", 6) == 0)) {	//TODO MOTOROLA SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +MMGR: Reading Motorola SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->reading_sms_msg)
					tech_pvt->reading_sms_msg++;
			}
			if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: \"STO U", 13) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading stored UNSENT SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			} else if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: \"STO S", 13) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading stored SENT SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			} else if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: \"REC R", 13) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading received READ SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			} else if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: \"REC U", 13) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading received UNREAD SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->reading_sms_msg)
					tech_pvt->reading_sms_msg++;
			} else if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: ", 6) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->reading_sms_msg)
					tech_pvt->reading_sms_msg++;
			}

			if ((strcmp(tech_pvt->line_array.result[i], "+MCST: 17") == 0)) {	/* motorola call processing unsolicited messages */
				tech_pvt->phone_callflow = CALLFLOW_CALL_INFLUX;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_INFLUX\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], "+MCST: 68") == 0)) {	/* motorola call processing unsolicited messages */
				tech_pvt->phone_callflow = CALLFLOW_CALL_NOSERVICE;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_NOSERVICE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "+MCST: 70") == 0)) {	/* motorola call processing unsolicited messages */
				tech_pvt->phone_callflow = CALLFLOW_CALL_OUTGOINGRESTRICTED;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_OUTGOINGRESTRICTED\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "+MCST: 72") == 0)) {	/* motorola call processing unsolicited messages */
				tech_pvt->phone_callflow = CALLFLOW_CALL_SECURITYFAIL;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_SECURITYFAIL\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CPBR", 5) == 0)) {	/* phonebook stuff begins */

				if (tech_pvt->phonebook_querying) {	/* probably phonebook struct begins */
					int err, first_entry, last_entry, number_lenght, text_lenght;

					if (option_debug)
						DEBUGA_GSMOPEN("phonebook struct: |%s|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

					err = sscanf(&tech_pvt->line_array.result[i][8], "%d-%d),%d,%d", &first_entry, &last_entry, &number_lenght, &text_lenght);
					if (err < 4) {

						err = sscanf(&tech_pvt->line_array.result[i][7], "%d-%d,%d,%d", &first_entry, &last_entry, &number_lenght, &text_lenght);
					}

					if (err < 4) {
						ERRORA
							("phonebook struct: |%s| is nor formatted as: |+CPBR: (1-750),40,14| neither as: |+CPBR: 1-750,40,14|\n",
							 GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					} else {

						if (option_debug)
							DEBUGA_GSMOPEN
								("First entry: %d, last entry: %d, phone number max lenght: %d, text max lenght: %d\n",
								 GSMOPEN_P_LOG, first_entry, last_entry, number_lenght, text_lenght);
						tech_pvt->phonebook_first_entry = first_entry;
						tech_pvt->phonebook_last_entry = last_entry;
						tech_pvt->phonebook_number_lenght = number_lenght;
						tech_pvt->phonebook_text_lenght = text_lenght;
					}

				} else {		/* probably phonebook entry begins */

					if (tech_pvt->phonebook_listing) {
						int err, entry_id, entry_type;

						char entry_number[256];
						char entry_text[256];

						if (option_debug)
							DEBUGA_GSMOPEN("phonebook entry: |%s|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

						err =
							sscanf(&tech_pvt->line_array.result[i][7], "%d,\"%255[0-9+]\",%d,\"%255[^\"]\"", &entry_id, entry_number, &entry_type,
								   entry_text);
						if (err < 4) {
							ERRORA
								("err=%d, phonebook entry: |%s| is not formatted as: |+CPBR: 504,\"+39025458068\",145,\"ciao a tutti\"|\n",
								 GSMOPEN_P_LOG, err, tech_pvt->line_array.result[i]);
						} else {
							//TODO: sanitize entry_text
							if (option_debug)
								DEBUGA_GSMOPEN("Number: %s, Text: %s, Type: %d\n", GSMOPEN_P_LOG, entry_number, entry_text, entry_type);
							/* write entry in phonebook file */
							if (tech_pvt->phonebook_writing_fp) {
								gsmopen_dir_entry_extension++;

								fprintf(tech_pvt->phonebook_writing_fp,
										"%s  => ,%sSKO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcell=%s|phonebook_entry_owner=%s\n",
										entry_number, entry_text, "no",
										tech_pvt->gsmopen_dir_entry_extension_prefix, "2", gsmopen_dir_entry_extension, "yes", "not_specified");
								fprintf(tech_pvt->phonebook_writing_fp,
										"%s  => ,%sDO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcell=%s|phonebook_entry_owner=%s\n",
										entry_number, entry_text, "no",
										tech_pvt->gsmopen_dir_entry_extension_prefix, "3", gsmopen_dir_entry_extension, "yes", "not_specified");
							}
						}

					}

					if (tech_pvt->phonebook_listing_received_calls) {
						int err, entry_id, entry_type;

						char entry_number[256] = "";
						char entry_text[256] = "";

						if (option_debug)
							DEBUGA_GSMOPEN("phonebook entry: |%s|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

						err =
							sscanf(&tech_pvt->line_array.result[i][7], "%d,\"%255[0-9+]\",%d,\"%255[^\"]\"", &entry_id, entry_number, &entry_type,
								   entry_text);
						if (err < 1) {	//we match only on the progressive id, maybe the remote party has not sent its number, and/or there is no corresponding text entry in the phone directory
							ERRORA
								("err=%d, phonebook entry: |%s| is not formatted as: |+CPBR: 504,\"+39025458068\",145,\"ciao a tutti\"|\n",
								 GSMOPEN_P_LOG, err, tech_pvt->line_array.result[i]);
						} else {
							//TODO: sanitize entry_text

							if (option_debug)
								DEBUGA_GSMOPEN("Number: %s, Text: %s, Type: %d\n", GSMOPEN_P_LOG, entry_number, entry_text, entry_type);
							memset(tech_pvt->callid_name, 0, sizeof(tech_pvt->callid_name));
							memset(tech_pvt->callid_number, 0, sizeof(tech_pvt->callid_number));
							strncpy(tech_pvt->callid_name, entry_text, sizeof(tech_pvt->callid_name));
							strncpy(tech_pvt->callid_number, entry_number, sizeof(tech_pvt->callid_number));
							if (option_debug)
								DEBUGA_GSMOPEN("incoming callid: Text: %s, Number: %s\n", GSMOPEN_P_LOG, tech_pvt->callid_name, tech_pvt->callid_number);

							DEBUGA_GSMOPEN("|%s| CPBR INCOMING CALLID: name is %s, number is %s\n",
										   GSMOPEN_P_LOG, tech_pvt->line_array.result[i],
										   tech_pvt->callid_name[0] != 1 ? tech_pvt->callid_name : "not available",
										   tech_pvt->callid_number[0] ? tech_pvt->callid_number : "not available");

							/* mark the time of RING */
							gettimeofday(&(tech_pvt->ringtime), NULL);
							tech_pvt->interface_state = GSMOPEN_STATE_RING;
							tech_pvt->phone_callflow = CALLFLOW_INCOMING_RING;

						}

					}

					else {
						DEBUGA_GSMOPEN("phonebook entry: |%s|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

					}
				}

			}
			if ((strncmp(tech_pvt->line_array.result[i], "+CUSD:", 6) == 0)) {
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| USSD received\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				int res = 0, status = 0;
				unsigned int dcs = 0;
				char ussd_msg[1024];
				memset(tech_pvt->ussd_message, '\0', sizeof(tech_pvt->ussd_message));
				memset(tech_pvt->ussd_dcs, '\0', sizeof(tech_pvt->ussd_dcs));
				res = sscanf(&tech_pvt->line_array.result[i][6], "%d,\"%1023[0-9A-F]\",%d", &status, ussd_msg, &dcs);
				if (res == 1) {
					NOTICA("received +CUSD with status %d\n", GSMOPEN_P_LOG, status);
					tech_pvt->ussd_received = 1;
					tech_pvt->ussd_status = status;
				} else if (res == 3) {
					tech_pvt->ussd_received = 1;
					tech_pvt->ussd_status = status;

					//identifying dcs alphabet according to GSM 03.38 Cell Broadcast Data Coding Scheme
					//CBDataCodingScheme should be used here, but it appears to be buggy (ucs2 messages are not recognized)
					int alphabet = DCS_RESERVED_ALPHABET;  
					if (dcs == 0x11) { 
						alphabet = DCS_SIXTEEN_BIT_ALPHABET;
					} else if ((dcs & 0xF0) <= 0x30){
						alphabet = DCS_DEFAULT_ALPHABET;
					} else if ((dcs & 0xC0) == 0x40 || (dcs & 0xF0) == 0x90) {
						alphabet = dcs & (3 << 2);
					};

					if ( (tech_pvt->ussd_response_encoding == USSD_ENCODING_AUTO && alphabet == DCS_DEFAULT_ALPHABET)
							|| tech_pvt->ussd_response_encoding == USSD_ENCODING_HEX_7BIT ) {
						SMSDecoder d(ussd_msg);
						d.markSeptet();
						string ussd_dec = gsmToLatin1(d.getString(strlen(ussd_msg) / 2 * 8 / 7));
						iso_8859_1_to_utf8(tech_pvt, (char *) ussd_dec.c_str(), tech_pvt->ussd_message, sizeof(tech_pvt->ussd_message));
						strcpy(tech_pvt->ussd_dcs, "default alphabet");
					} else if ( (tech_pvt->ussd_response_encoding == USSD_ENCODING_AUTO && alphabet == DCS_SIXTEEN_BIT_ALPHABET)
								|| tech_pvt->ussd_response_encoding == USSD_ENCODING_UCS2 ) {
						ucs2_to_utf8(tech_pvt, ussd_msg, tech_pvt->ussd_message, sizeof(tech_pvt->ussd_message));
						strcpy(tech_pvt->ussd_dcs, "16-bit alphabet");
					} else if ( (tech_pvt->ussd_response_encoding == USSD_ENCODING_AUTO && alphabet == DCS_EIGHT_BIT_ALPHABET)
								|| tech_pvt->ussd_response_encoding == USSD_ENCODING_HEX_8BIT ) {
						char ussd_dec[1024];
						memset(ussd_dec, '\0', sizeof(ussd_dec));
						hexToBuf(ussd_msg, (unsigned char*)ussd_dec);
						iso_8859_1_to_utf8(tech_pvt, (char *) ussd_dec, tech_pvt->ussd_message, sizeof(tech_pvt->ussd_message));
						strcpy(tech_pvt->ussd_dcs, "8-bit alphabet");
					} else if ( tech_pvt->ussd_response_encoding == USSD_ENCODING_PLAIN ) {
						string ussd_dec = gsmToLatin1(ussd_msg);
						iso_8859_1_to_utf8(tech_pvt, (char *) ussd_dec.c_str(), tech_pvt->ussd_message, sizeof(tech_pvt->ussd_message));
						strcpy(tech_pvt->ussd_dcs, "default alphabet");
					} else {
						ERRORA("USSD data coding scheme not supported=%d\n", GSMOPEN_P_LOG, dcs);
					}

					NOTICA("USSD received: status=%d, message='%s', dcs='%d'\n", 
						GSMOPEN_P_LOG, tech_pvt->ussd_status, tech_pvt->ussd_message, dcs);

					ussd_incoming(tech_pvt);
				} else {
					ERRORA ("res=%d, +CUSD command has wrong format: %s\n",
								 GSMOPEN_P_LOG, res, tech_pvt->line_array.result[i]);
				}
			}

			if ((strncmp(tech_pvt->line_array.result[i], "*ECAV", 5) == 0) || (strncmp(tech_pvt->line_array.result[i], "*ECAM", 5) == 0)) {	/* sony-ericsson call processing unsolicited messages */
				int res, ccid, ccstatus, calltype, processid, exitcause, number, type;
				res = ccid = ccstatus = calltype = processid = exitcause = number = type = 0;
				res =
					sscanf(&tech_pvt->line_array.result[i][6], "%d,%d,%d,%d,%d,%d,%d", &ccid, &ccstatus, &calltype, &processid, &exitcause, &number,
						   &type);
				/* only changes the phone_callflow if enought parameters were parsed */
				if (res >= 3) {
					switch (ccstatus) {
					case 0:
						if (tech_pvt->owner) {
							ast_setstate(tech_pvt->owner, GSMOPEN_STATE_DOWN);
							tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_NORMAL;
							gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
						}
						tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
						tech_pvt->interface_state = GSMOPEN_STATE_DOWN;
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: IDLE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 1:
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: CALLING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 2:
						if (tech_pvt->owner) {
							ast_setstate(tech_pvt->owner, GSMOPEN_STATE_DIALING);
						}
						tech_pvt->interface_state = CALLFLOW_CALL_DIALING;
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: CONNECTING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 3:
						if (tech_pvt->owner) {
							ast_setstate(tech_pvt->owner, GSMOPEN_STATE_UP);
							gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_ANSWER);
						}
						tech_pvt->phone_callflow = CALLFLOW_CALL_ACTIVE;
						tech_pvt->interface_state = GSMOPEN_STATE_UP;
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: ACTIVE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 4:
						if (option_debug > 1)
							DEBUGA_GSMOPEN
								("|%s| Sony-Ericsson *ECAM/*ECAV: don't know how to handle HOLD event\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 5:
						if (option_debug > 1)
							DEBUGA_GSMOPEN
								("|%s| Sony-Ericsson *ECAM/*ECAV: don't know how to handle WAITING event\n", GSMOPEN_P_LOG,
								 tech_pvt->line_array.result[i]);
						break;
					case 6:
						if (option_debug > 1)
							DEBUGA_GSMOPEN
								("|%s| Sony-Ericsson *ECAM/*ECAV: don't know how to handle ALERTING event\n", GSMOPEN_P_LOG,
								 tech_pvt->line_array.result[i]);
						break;
					case 7:
						if (tech_pvt->owner) {
							ast_setstate(tech_pvt->owner, GSMOPEN_STATE_BUSY);
							gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_BUSY);
						}
						tech_pvt->phone_callflow = CALLFLOW_CALL_LINEBUSY;
						tech_pvt->interface_state = GSMOPEN_STATE_BUSY;
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: BUSY\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					}
				} else {
					if (option_debug > 1)
						DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: could not parse parameters\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				}

			}

			/* at_indicator_* are unsolicited messages sent by the phone to signal us that some of its visual indicators on its screen has changed, based on CIND CMER ETSI docs */
			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_noservice_string) == 0)) {
				ERRORA("|%s| at_indicator_noservice_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				alarm_event(tech_pvt, ALARM_NETWORK_NO_SERVICE, "at_indicator_noservice_string");
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_nosignal_string) == 0)) {
				ERRORA("|%s| at_indicator_nosignal_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				alarm_event(tech_pvt, ALARM_NETWORK_NO_SIGNAL, "at_indicator_nosignal_string");
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_lowsignal_string) == 0)) {
				WARNINGA("|%s| at_indicator_lowsignal_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				alarm_event(tech_pvt, ALARM_NETWORK_LOW_SIGNAL, "at_indicator_lowsignal_string");
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_lowbattchg_string) == 0)) {
				WARNINGA("|%s| at_indicator_lowbattchg_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_nobattchg_string) == 0)) {
				ERRORA("|%s| at_indicator_nobattchg_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_callactive_string) == 0)) {
				DEBUGA_GSMOPEN("|%s| at_indicator_callactive_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_nocallactive_string) == 0)) {
				DEBUGA_GSMOPEN("|%s| at_indicator_nocallactive_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_nocallsetup_string) == 0)) {
				DEBUGA_GSMOPEN("|%s| at_indicator_nocallsetup_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_callsetupincoming_string) == 0)) {
				DEBUGA_GSMOPEN("|%s| at_indicator_callsetupincoming_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_callsetupoutgoing_string) == 0)) {
				DEBUGA_GSMOPEN("|%s| at_indicator_callsetupoutgoing_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_callsetupremoteringing_string)
				 == 0)) {
				DEBUGA_GSMOPEN("|%s| at_indicator_callsetupremoteringing_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

		}

		/* let's look for OK, ERROR and EXPECTED_STRING in the complete lines read so far, without re-looking at the lines that has been yet looked at */
		for (i = la_read; i < la_counter; i++) {
			if (expected_string) {
				if ((strncmp(tech_pvt->line_array.result[i], expected_string, strlen(expected_string))
					 == 0)) {
					if (option_debug > 1)
						DEBUGA_GSMOPEN("|%s| got what EXPECTED\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					at_ack = AT_OK;
				}
			} else {
				if ((strcmp(tech_pvt->line_array.result[i], "OK") == 0)) {
					if (option_debug > 1)
						DEBUGA_GSMOPEN("got OK\n", GSMOPEN_P_LOG);
					at_ack = AT_OK;
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "ERROR") == 0)) {
				if (option_debug > 1)
					DEBUGA_GSMOPEN("got ERROR\n", GSMOPEN_P_LOG);
				at_ack = AT_ERROR;
			}

			/* if we are requesting IMEI, put the line into the imei buffer if the line is not "OK" or "ERROR" */
			if (tech_pvt->requesting_imei && at_ack == -1) {
				if (strlen(tech_pvt->line_array.result[i])) {	/* we are reading the IMEI */
					strncpy(tech_pvt->imei, tech_pvt->line_array.result[i], sizeof(tech_pvt->imei));
				}
			}

			/* if we are requesting IMSI, put the line into the imei buffer if the line is not "OK" or "ERROR" */
			if (tech_pvt->requesting_imsi && at_ack == -1) {
				if (strlen(tech_pvt->line_array.result[i])) {	/* we are reading the IMSI */
					strncpy(tech_pvt->imsi, tech_pvt->line_array.result[i], sizeof(tech_pvt->imsi));
				}
			}


			/* if we are reading an sms message from memory, put the line into the sms buffer if the line is not "OK" or "ERROR" */
			if (tech_pvt->reading_sms_msg > 1 && at_ack == -1) {
				int c;
				char sms_body[16000];
				int err = 0;
				memset(sms_body, '\0', sizeof(sms_body));

				if (strncmp(tech_pvt->line_array.result[i], "+CMGR", 5) == 0) {	/* we are reading the "header" of an SMS */
					char content[512];
					char content2[512];
					int inside_comma = 0;
					int inside_quote = 0;
					int which_field = 0;
					int d = 0;

					DEBUGA_GSMOPEN("HERE\n", GSMOPEN_P_LOG);

					memset(content, '\0', sizeof(content));

					for (c = 0; c < (int) strlen(tech_pvt->line_array.result[i]); c++) {
						if (tech_pvt->line_array.result[i][c] == ',' && tech_pvt->line_array.result[i][c - 1] != '\\' && inside_quote == 0) {
							if (inside_comma) {
								inside_comma = 0;
								DEBUGA_GSMOPEN("inside_comma=%d, inside_quote=%d, we're at=%s\n", GSMOPEN_P_LOG, inside_comma, inside_quote,
											   &tech_pvt->line_array.result[i][c]);
							} else {
								inside_comma = 1;
								DEBUGA_GSMOPEN("inside_comma=%d, inside_quote=%d, we're at=%s\n", GSMOPEN_P_LOG, inside_comma, inside_quote,
											   &tech_pvt->line_array.result[i][c]);
							}
						}
						if (tech_pvt->line_array.result[i][c] == '"' && tech_pvt->line_array.result[i][c - 1] != '\\') {
							if (inside_quote) {
								inside_quote = 0;
								DEBUGA_GSMOPEN("END_CONTENT inside_comma=%d, inside_quote=%d, we're at=%s\n", GSMOPEN_P_LOG, inside_comma, inside_quote,
											   &tech_pvt->line_array.result[i][c]);
								DEBUGA_GSMOPEN("%d content=%s\n", GSMOPEN_P_LOG, which_field, content);

								memset(content2, '\0', sizeof(content2));
								if (which_field == 1) {
									err = ucs2_to_utf8(tech_pvt, content, content2, sizeof(content2));
								} else {
									err = 0;
									strncpy(content2, content, sizeof(content2));
								}
								DEBUGA_GSMOPEN("%d content2=%s\n", GSMOPEN_P_LOG, which_field, content2);
								DEBUGA_GSMOPEN("%d content=%s\n", GSMOPEN_P_LOG, which_field, content2);

								memset(content, '\0', sizeof(content));
								d = 0;
								if (which_field == 1) {
									strncpy(tech_pvt->sms_sender, content2, sizeof(tech_pvt->sms_sender));
									DEBUGA_GSMOPEN("%d content=%s\n", GSMOPEN_P_LOG, which_field, content2);

								} else if (which_field == 2) {
									strncpy(tech_pvt->sms_date, content2, sizeof(tech_pvt->sms_date));
									DEBUGA_GSMOPEN("%d content=%s\n", GSMOPEN_P_LOG, which_field, content2);
								} else if (which_field > 2) {
									WARNINGA("WHY which_field is > 2 ? (which_field is %d)\n", GSMOPEN_P_LOG, which_field);
								}
								which_field++;
							} else {
								inside_quote = 1;
								DEBUGA_GSMOPEN("START_CONTENT inside_comma=%d, inside_quote=%d, we're at=%s\n", GSMOPEN_P_LOG, inside_comma, inside_quote,
											   &tech_pvt->line_array.result[i][c]);
							}
						}
						if (inside_quote && tech_pvt->line_array.result[i][c] != '"') {

							content[d] = tech_pvt->line_array.result[i][c];
							d++;

						}

					}
				}				//it was the +CMGR answer from the cellphone
				else {
					DEBUGA_GSMOPEN("body=%s\n", GSMOPEN_P_LOG, sms_body);
					DEBUGA_GSMOPEN("tech_pvt->line_array.result[i]=%s\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					if (tech_pvt->sms_pdu_not_supported) {
						char content3[1000];
						strncpy(tech_pvt->sms_message, tech_pvt->line_array.result[i], sizeof(tech_pvt->sms_message));


						memset(content3, '\0', sizeof(content3));
						DEBUGA_GSMOPEN("sms_message=%s\n", GSMOPEN_P_LOG, tech_pvt->sms_message);
						ucs2_to_utf8(tech_pvt, tech_pvt->sms_message, content3, sizeof(content3));
						DEBUGA_GSMOPEN("content3=%s\n", GSMOPEN_P_LOG, content3);
						strncpy(tech_pvt->sms_body, content3, sizeof(tech_pvt->sms_body));
						if (tech_pvt->sms_cnmi_not_supported) {
							sms_incoming(tech_pvt);
							DEBUGA_GSMOPEN("2 content3=%s\n", GSMOPEN_P_LOG, content3);
						}
					} else {


						try {
							char content2[1000];
							SMSMessageRef sms;

							DEBUGA_GSMOPEN("about to decode\n", GSMOPEN_P_LOG);
							try {
								sms = SMSMessage::decode(tech_pvt->line_array.result[i]);	// dataCodingScheme = 8 , text=ciao 123 belè новости לק ראת ﺎﻠﺠﻤﻋﺓ 人大
							}
							catch(...) {
								ERRORA("GsmException\n", GSMOPEN_P_LOG);
								gsmopen_sleep(5000);
								return -1;
							}

							DEBUGA_GSMOPEN("after decode\n", GSMOPEN_P_LOG);

#if 0
							char letsee[1024];
							memset(letsee, '\0', sizeof(letsee));
	
							DEBUGA_GSMOPEN("about to letsee\n", GSMOPEN_P_LOG);
							try {
								sprintf(letsee, "|%s|\n", sms->toString().c_str());
							}
							catch(...) {
								ERRORA("GsmException\n", GSMOPEN_P_LOG);
								sleep(5);
								return -1;
							}
							DEBUGA_GSMOPEN("after letsee\n", GSMOPEN_P_LOG);
						
							DEBUGA_GSMOPEN("SMS=\n%s\n", GSMOPEN_P_LOG, letsee);
#endif //0
							memset(content2, '\0', sizeof(content2));
							if (sms->dataCodingScheme().getAlphabet() == DCS_DEFAULT_ALPHABET) {
								iso_8859_1_to_utf8(tech_pvt, (char *) sms->userData().c_str(), content2, sizeof(content2));
							} else if (sms->dataCodingScheme().getAlphabet() == DCS_SIXTEEN_BIT_ALPHABET) {
								ucs2_to_utf8(tech_pvt, (char *) bufToHex((unsigned char *) sms->userData().data(), sms->userData().length()).c_str(),
											 content2, sizeof(content2));
							} else {
								ERRORA("dataCodingScheme not supported=%u\n", GSMOPEN_P_LOG, sms->dataCodingScheme().getAlphabet());

							}
							DEBUGA_GSMOPEN("dataCodingScheme=%u\n", GSMOPEN_P_LOG, sms->dataCodingScheme().getAlphabet());
							DEBUGA_GSMOPEN("dataCodingScheme=%s\n", GSMOPEN_P_LOG, sms->dataCodingScheme().toString().c_str());
							DEBUGA_GSMOPEN("address=%s\n", GSMOPEN_P_LOG, sms->address().toString().c_str());
							DEBUGA_GSMOPEN("serviceCentreAddress=%s\n", GSMOPEN_P_LOG, sms->serviceCentreAddress().toString().c_str());
							DEBUGA_GSMOPEN("serviceCentreTimestamp=%s\n", GSMOPEN_P_LOG, sms->serviceCentreTimestamp().toString().c_str());
							DEBUGA_GSMOPEN("UserDataHeader=%s\n", GSMOPEN_P_LOG, (char *)bufToHex((unsigned char *)
											((string) sms->userDataHeader()).data(), sms->userDataHeader().length()).c_str());
							DEBUGA_GSMOPEN("messageType=%d\n", GSMOPEN_P_LOG, sms->messageType());
							DEBUGA_GSMOPEN("userData= |||%s|||\n", GSMOPEN_P_LOG, content2);


							memset(sms_body, '\0', sizeof(sms_body));
							strncpy(sms_body, content2, sizeof(sms_body));
							DEBUGA_GSMOPEN("body=%s\n", GSMOPEN_P_LOG, sms_body);
							strncpy(tech_pvt->sms_body, sms_body, sizeof(tech_pvt->sms_body));
							strncpy(tech_pvt->sms_sender, sms->address().toString().c_str(), sizeof(tech_pvt->sms_sender));
							strncpy(tech_pvt->sms_date, sms->serviceCentreTimestamp().toString().c_str(), sizeof(tech_pvt->sms_date));
							strncpy(tech_pvt->sms_userdataheader, (char *)
										bufToHex((unsigned char *)((string) sms->userDataHeader()).data(), sms->userDataHeader().length()).c_str(),
										sizeof(tech_pvt->sms_userdataheader));
							strncpy(tech_pvt->sms_datacodingscheme, sms->dataCodingScheme().toString().c_str(), sizeof(tech_pvt->sms_datacodingscheme));
							strncpy(tech_pvt->sms_servicecentreaddress, sms->serviceCentreAddress().toString().c_str(),
									sizeof(tech_pvt->sms_servicecentreaddress));
							tech_pvt->sms_messagetype = sms->messageType();

						}
							catch(...) {
								ERRORA("GsmException\n", GSMOPEN_P_LOG);
								gsmopen_sleep(5000);
								return -1;
						}





					}

				}				//it was the UCS2 from cellphone

			}					//we were reading the SMS

		}

		la_read = la_counter;

		if (look_for_ack && at_ack > -1)
			break;

		if (la_counter == AT_MESG_MAX_LINES) {
			ERRORA("Too many lines in result (>%d). la_counter=%d. tech_pvt->reading_sms_msg=%d. Stop accumulating lines.\n", GSMOPEN_P_LOG,
				   AT_MESG_MAX_LINES, la_counter, tech_pvt->reading_sms_msg);
			WARNINGA("read was %d bytes, tmp_answer3= --|%s|--\n", GSMOPEN_P_LOG, read_count, tmp_answer3);
			at_ack = AT_ERROR;
			break;
		}

	}

	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);
	if (select_err == -1) {
		ERRORA("select returned -1 on %s, setting controldev_dead, error was: %s\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name, strerror(errno));
		tech_pvt->controldev_dead = 1;

		tech_pvt->running = 0;
		alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "select returned -1 on interface, setting controldev_dead");
		tech_pvt->active = 0;
		tech_pvt->name[0] = '\0';
		if (tech_pvt->owner)
			gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
		switch_sleep(1000000);
		return -1;
	}

	if (tech_pvt->phone_callflow == CALLFLOW_CALL_INCOMING && tech_pvt->call_incoming_time.tv_sec) {	//after three sec of CALLFLOW_CALL_INCOMING, we assume the phone is incapable of notifying RING (eg: motorola c350), so we try to answer
		char list_command[64];
		struct timeval call_incoming_timeout;
		gettimeofday(&call_incoming_timeout, NULL);
		call_incoming_timeout.tv_sec -= 3;
		DEBUGA_GSMOPEN
			("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
			 GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
		if (call_incoming_timeout.tv_sec > tech_pvt->call_incoming_time.tv_sec) {

			tech_pvt->call_incoming_time.tv_sec = 0;
			tech_pvt->call_incoming_time.tv_usec = 0;
			DEBUGA_GSMOPEN
				("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
				 GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
			int res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CPBS=RC");
			if (res) {
				ERRORA("AT+CPBS=RC (select memory of received calls) was not answered by the phone\n", GSMOPEN_P_LOG);
			}
			tech_pvt->phonebook_querying = 1;
			res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CPBR=?");
			if (res) {
				ERRORA("AT+CPBS=RC (select memory of received calls) was not answered by the phone\n", GSMOPEN_P_LOG);
			}
			tech_pvt->phonebook_querying = 0;
			sprintf(list_command, "AT+CPBR=%d,%d", tech_pvt->phonebook_first_entry, tech_pvt->phonebook_last_entry);
			tech_pvt->phonebook_listing_received_calls = 1;
			res = gsmopen_serial_write_AT_expect_longtime(tech_pvt, list_command, "OK");
			if (res) {
				WARNINGA("AT+CPBR=%d,%d failed, continue\n", GSMOPEN_P_LOG, tech_pvt->phonebook_first_entry, tech_pvt->phonebook_last_entry);
			}
			tech_pvt->phonebook_listing_received_calls = 0;
		}
	}

	if (tech_pvt->phone_callflow == CALLFLOW_INCOMING_RING) {
		struct timeval call_incoming_timeout;
		gettimeofday(&call_incoming_timeout, NULL);
		call_incoming_timeout.tv_sec -= 10;
		// DEBUGA_GSMOPEN ("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n", GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
		if (call_incoming_timeout.tv_sec > tech_pvt->ringtime.tv_sec) {
			ERRORA("Ringing stopped and I have not answered. Why?\n", GSMOPEN_P_LOG);
			DEBUGA_GSMOPEN
				("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
				 GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
			if (tech_pvt->owner) {
				gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
			}
		}
	}
	tech_pvt->line_array.elemcount = la_counter;
	//NOTICA (" OUTSIDE this gsmopen_serial_device %s \n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
	if (look_for_ack)
		return at_ack;
	else
		return 0;
}

int gsmopen_serial_write_AT(private_t *tech_pvt, const char *data)
{
	int howmany;
	int i;
	int res;
	int count;
	char *Data = (char *) data;

	if (!tech_pvt)
		return -1;

	howmany = strlen(Data);

	for (i = 0; i < howmany; i++) {
		res = tech_pvt->serialPort_serial_control->Write(&Data[i], 1);

		if (res != 1) {
			DEBUGA_GSMOPEN("Error sending (%.1s): %d (%s)\n", GSMOPEN_P_LOG, &Data[i], res, strerror(errno));
			gsmopen_sleep(100000);
			for (count = 0; count < 10; count++) {
				res = tech_pvt->serialPort_serial_control->Write(&Data[i], 1);
				if (res == 1) {
					DEBUGA_GSMOPEN("Successfully RE-sent (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &Data[i], count, res, strerror(errno));
					break;
				} else
					DEBUGA_GSMOPEN("Error RE-sending (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &Data[i], count, res, strerror(errno));
				gsmopen_sleep(100000);

			}
			if (res != 1) {
				ERRORA("Error RE-sending (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &Data[i], count, res, strerror(errno));



				ERRORA
					("wrote -1 bytes!!! Nenormalno! Marking this gsmopen_serial_device %s as dead, andif it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, power down or battery exhausted\n",
					 GSMOPEN_P_LOG, tech_pvt->controldevice_name);
				tech_pvt->controldev_dead = 1;
				ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
				tech_pvt->running = 0;
				alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
				tech_pvt->active = 0;
				tech_pvt->name[0] = '\0';

				UNLOCKA(tech_pvt->controldev_lock);
				if (tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
				switch_sleep(1000000);




				return -1;
			}
		}
		if (option_debug > 1)
			DEBUGA_GSMOPEN("sent data... (%.1s)\n", GSMOPEN_P_LOG, &Data[i]);
		gsmopen_sleep(1000);	/* release the cpu */
	}

	res = tech_pvt->serialPort_serial_control->Write((char *) "\r", 1);

	if (res != 1) {
		DEBUGA_GSMOPEN("Error sending (carriage return): %d (%s)\n", GSMOPEN_P_LOG, res, strerror(errno));
		gsmopen_sleep(100000);
		for (count = 0; count < 10; count++) {
			res = tech_pvt->serialPort_serial_control->Write((char *) "\r", 1);

			if (res == 1) {
				DEBUGA_GSMOPEN("Successfully RE-sent carriage return: %d %d (%s)\n", GSMOPEN_P_LOG, count, res, strerror(errno));
				break;
			} else
				DEBUGA_GSMOPEN("Error RE-sending (carriage return): %d %d (%s)\n", GSMOPEN_P_LOG, count, res, strerror(errno));
			gsmopen_sleep(100000);

		}
		if (res != 1) {
			ERRORA("Error RE-sending (carriage return): %d %d (%s)\n", GSMOPEN_P_LOG, count, res, strerror(errno));

			ERRORA
				("wrote -1 bytes!!! Nenormalno! Marking this gsmopen_serial_device %s as dead, andif it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, power down or battery exhausted\n",
				 GSMOPEN_P_LOG, tech_pvt->controldevice_name);
			tech_pvt->controldev_dead = 1;
			ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
			tech_pvt->running = 0;
			alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
			tech_pvt->active = 0;
			tech_pvt->name[0] = '\0';

			UNLOCKA(tech_pvt->controldev_lock);
			if (tech_pvt->owner) {
				tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
				gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
			}
			switch_sleep(1000000);


			return -1;
		}
	}
	if (option_debug > 1)
		DEBUGA_GSMOPEN("sent (carriage return)\n", GSMOPEN_P_LOG);
	gsmopen_sleep(1000);		/* release the cpu */

	return howmany;
}

int gsmopen_serial_write_AT_nocr(private_t *tech_pvt, const char *data)
{
	int howmany;
	int i;
	int res;
	int count;
	char *Data = (char *) data;

	if (!tech_pvt)
		return -1;

	howmany = strlen(Data);

	for (i = 0; i < howmany; i++) {
		res = tech_pvt->serialPort_serial_control->Write(&Data[i], 1);

		if (res != 1) {
			DEBUGA_GSMOPEN("Error sending (%.1s): %d (%s)\n", GSMOPEN_P_LOG, &Data[i], res, strerror(errno));
			gsmopen_sleep(100000);
			for (count = 0; count < 10; count++) {
				res = tech_pvt->serialPort_serial_control->Write(&Data[i], 1);
				if (res == 1)
					break;
				else
					DEBUGA_GSMOPEN("Error RE-sending (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &Data[i], count, res, strerror(errno));
				gsmopen_sleep(100000);

			}
			if (res != 1) {
				ERRORA("Error RE-sending (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &Data[i], count, res, strerror(errno));

				ERRORA
					("wrote -1 bytes!!! Nenormalno! Marking this gsmopen_serial_device %s as dead, andif it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, power down or battery exhausted\n",
					 GSMOPEN_P_LOG, tech_pvt->controldevice_name);
				tech_pvt->controldev_dead = 1;
				ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
				tech_pvt->running = 0;
				alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
				tech_pvt->active = 0;
				tech_pvt->name[0] = '\0';

				UNLOCKA(tech_pvt->controldev_lock);
				if (tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
				switch_sleep(1000000);


				return -1;
			}
		}
		if (option_debug > 1)
			DEBUGA_GSMOPEN("sent data... (%.1s)\n", GSMOPEN_P_LOG, &Data[i]);
		gsmopen_sleep(1000);	/* release the cpu */
	}

	gsmopen_sleep(1000);		/* release the cpu */

	return howmany;
}

int gsmopen_serial_write_AT_noack(private_t *tech_pvt, const char *data)
{

	if (option_debug > 1)
		DEBUGA_GSMOPEN("gsmopen_serial_write_AT_noack: %s\n", GSMOPEN_P_LOG, data);

	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (gsmopen_serial_write_AT(tech_pvt, data) != (int) strlen(data)) {

		ERRORA("Error sending data... (%s)\n", GSMOPEN_P_LOG, strerror(errno));
		UNLOCKA(tech_pvt->controldev_lock);

		ERRORA
			("wrote -1 bytes!!! Nenormalno! Marking this gsmopen_serial_device %s as dead, andif it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, power down or battery exhausted\n",
			 GSMOPEN_P_LOG, tech_pvt->controldevice_name);
		tech_pvt->controldev_dead = 1;
		ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
		tech_pvt->running = 0;
		alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
		tech_pvt->active = 0;
		tech_pvt->name[0] = '\0';

		UNLOCKA(tech_pvt->controldev_lock);
		if (tech_pvt->owner) {
			tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
			gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
		}
		switch_sleep(1000000);


		return -1;
	}
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return 0;
}

int gsmopen_serial_write_AT_ack(private_t *tech_pvt, const char *data)
{
	int at_result = AT_ERROR;

	if (!tech_pvt)
		return -1;

	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (option_debug > 1)
		DEBUGA_GSMOPEN("sending: %s\n", GSMOPEN_P_LOG, data);
	if (gsmopen_serial_write_AT(tech_pvt, data) != (int) strlen(data)) {
		ERRORA("Error sending data... (%s) \n", GSMOPEN_P_LOG, strerror(errno));
		UNLOCKA(tech_pvt->controldev_lock);

		ERRORA
			("wrote -1 bytes!!! Nenormalno! Marking this gsmopen_serial_device %s as dead, and if it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, powered down or battery exhausted\n",
			 GSMOPEN_P_LOG, tech_pvt->controldevice_name);
		tech_pvt->controldev_dead = 1;
		ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
		tech_pvt->running = 0;
		alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
		tech_pvt->active = 0;
		tech_pvt->name[0] = '\0';

		UNLOCKA(tech_pvt->controldev_lock);
		if (tech_pvt->owner) {
			tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
			gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
		}
		switch_sleep(1000000);


		return -1;
	}
	at_result = gsmopen_serial_read_AT(tech_pvt, 1, 100000, 0, NULL, 1);	// 1/10th sec timeout
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return at_result;

}

int gsmopen_serial_write_AT_ack_nocr_longtime(private_t *tech_pvt, const char *data)
{
	int at_result = AT_ERROR;

	if (!tech_pvt)
		return -1;

	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (option_debug > 1)
		DEBUGA_GSMOPEN("sending: %s\n", GSMOPEN_P_LOG, data);
	if (gsmopen_serial_write_AT_nocr(tech_pvt, data) != (int) strlen(data)) {
		ERRORA("Error sending data... (%s) \n", GSMOPEN_P_LOG, strerror(errno));
		UNLOCKA(tech_pvt->controldev_lock);

		ERRORA
			("wrote -1 bytes!!! Nenormalno! Marking this gsmopen_serial_device %s as dead, and if it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, powered down or battery exhausted\n",
			 GSMOPEN_P_LOG, tech_pvt->controldevice_name);
		tech_pvt->controldev_dead = 1;
		ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
		tech_pvt->running = 0;
		alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
		tech_pvt->active = 0;
		tech_pvt->name[0] = '\0';

		UNLOCKA(tech_pvt->controldev_lock);
		if (tech_pvt->owner) {
			tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
			gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
		}
		switch_sleep(1000000);


		return -1;
	}

	at_result = gsmopen_serial_read_AT(tech_pvt, 1, 500000, 20, NULL, 1);	// 20.5 sec timeout
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return at_result;

}

int gsmopen_serial_write_AT_expect1(private_t *tech_pvt, const char *data, const char *expected_string, int expect_crlf, int seconds)
{
	int at_result = AT_ERROR;

	if (!tech_pvt)
		return -1;

	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (option_debug > 1)
		DEBUGA_GSMOPEN("sending: %s, expecting: %s\n", GSMOPEN_P_LOG, data, expected_string);
	if (gsmopen_serial_write_AT(tech_pvt, data) != (int) strlen(data)) {
		ERRORA("Error sending data... (%s) \n", GSMOPEN_P_LOG, strerror(errno));
		UNLOCKA(tech_pvt->controldev_lock);

		ERRORA
			("wrote -1 bytes!!! Nenormalno! Marking this gsmopen_serial_device %s as dead, and if it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, powered down or battery exhausted\n",
			 GSMOPEN_P_LOG, tech_pvt->controldevice_name);
		tech_pvt->controldev_dead = 1;
		ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
		tech_pvt->running = 0;
		alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
		tech_pvt->active = 0;
		tech_pvt->name[0] = '\0';

		UNLOCKA(tech_pvt->controldev_lock);
		if (tech_pvt->owner) {
			tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
			gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
		}
		switch_sleep(1000000);


		return -1;
	}

	at_result = gsmopen_serial_read_AT(tech_pvt, 1, 500000, seconds, expected_string, expect_crlf);	// minimum half a sec timeout
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return at_result;

}

int gsmopen_serial_AT_expect(private_t *tech_pvt, const char *expected_string, int expect_crlf, int seconds)
{
	int at_result = AT_ERROR;

	if (!tech_pvt)
		return -1;


	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (option_debug > 1)
		DEBUGA_GSMOPEN("expecting: %s\n", GSMOPEN_P_LOG, expected_string);

	at_result = gsmopen_serial_read_AT(tech_pvt, 1, 100000, seconds, expected_string, expect_crlf);	//  minimum 1/10th sec timeout
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return at_result;

}

int gsmopen_serial_answer(private_t *tech_pvt)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_answer_AT(tech_pvt);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_answer_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_answer_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */
	return -1;
}

int gsmopen_serial_answer_AT(private_t *tech_pvt)
{
	int res;

	if (!tech_pvt)
		return -1;

	res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_answer, tech_pvt->at_answer_expect);
	if (res) {
		DEBUGA_GSMOPEN
			("at_answer command failed, command used: %s, expecting: %s, trying with AT+CKPD=\"S\"\n",
			 GSMOPEN_P_LOG, tech_pvt->at_answer, tech_pvt->at_answer_expect);

		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CKPD=\"S\"");
		if (res) {
			ERRORA("at_answer command failed, command used: 'AT+CKPD=\"S\"', giving up\n", GSMOPEN_P_LOG);
			return -1;
		}
	}
	//res = gsmopen_serial_write_AT_expect(tech_pvt, "AT^DDSETEX=2", tech_pvt->at_dial_expect);
	DEBUGA_GSMOPEN("AT: call answered\n", GSMOPEN_P_LOG);
	return 0;
}

int gsmopen_serial_hangup(private_t *tech_pvt)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_hangup_AT(tech_pvt);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_hangup_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_hangup_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */
	return -1;
}

int gsmopen_serial_hangup_AT(private_t *tech_pvt)
{
	int res;

	if (!tech_pvt)
		return -1;

	if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
		res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_hangup, tech_pvt->at_hangup_expect);
		if (res) {
			DEBUGA_GSMOPEN("at_hangup command failed, command used: %s, trying to use AT+CKPD=\"EEE\"\n", GSMOPEN_P_LOG, tech_pvt->at_hangup);
			res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CKPD=\"EEE\"");
			if (res) {
				ERRORA("at_hangup command failed, command used: 'AT+CKPD=\"EEE\"'\n", GSMOPEN_P_LOG);
				return -1;
			}

		}

		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CHUP");
		if (res) {
			DEBUGA_GSMOPEN("at_hangup command timeout, command used: 'AT+CHUP'\n", GSMOPEN_P_LOG);
		}

	}
	tech_pvt->interface_state = GSMOPEN_STATE_DOWN;
	tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
	return 0;
}

int gsmopen_serial_call(private_t *tech_pvt, char *dstr)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_call_AT(tech_pvt, dstr);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_call_FBUS2(tech_pvt, dstr);
#endif /* GSMOPEN_FBUS2 */
	if (tech_pvt->controldevprotocol == PROTOCOL_NO_SERIAL)
		return 0;
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_call_CVM_BUSMAIL(tech_pvt, dstr);
#endif /* GSMOPEN_CVM */
	return -1;
}

int gsmopen_serial_call_AT(private_t *tech_pvt, char *dstr)
{
	int res;
	char at_command[256];

	if (option_debug)
		DEBUGA_PBX("Dialing %s\n", GSMOPEN_P_LOG, dstr);
	memset(at_command, 0, sizeof(at_command));
	tech_pvt->phone_callflow = CALLFLOW_CALL_DIALING;
	tech_pvt->interface_state = GSMOPEN_STATE_DIALING;
	sprintf(at_command, "%s%s%s", tech_pvt->at_dial_pre_number, dstr, tech_pvt->at_dial_post_number);
	DEBUGA_PBX("Dialstring %s\n", GSMOPEN_P_LOG, at_command);
	res = gsmopen_serial_write_AT_expect(tech_pvt, at_command, tech_pvt->at_dial_expect);
	if (res) {
		ERRORA("dial command failed, dial string was: %s\n", GSMOPEN_P_LOG, at_command);
		return -1;
	}
	//res = gsmopen_serial_write_AT_expect(tech_pvt, "AT^DDSETEX=2", tech_pvt->at_dial_expect);

	return 0;
}

int ucs2_to_utf8(private_t *tech_pvt, char *ucs2_in, char *utf8_out, size_t outbytesleft)
{
	char converted[16000];
	iconv_t iconv_format;
	int iconv_res;
	char *outbuf;
	char *inbuf;
	size_t inbytesleft;
	int c;
	char stringa[5];
	double hexnum;
	int i = 0;

	memset(converted, '\0', sizeof(converted));

	DEBUGA_GSMOPEN("ucs2_in=|%s|, utf8_out=|%s|\n", GSMOPEN_P_LOG, ucs2_in, utf8_out);
	for (c = 0; c < (int) strlen(ucs2_in); c++) {
		sprintf(stringa, "0x%c%c", ucs2_in[c], ucs2_in[c + 1]);
		c++;
		hexnum = strtod(stringa, NULL);
		converted[i] = (char) hexnum;
		i++;
	}

	outbuf = utf8_out;
	inbuf = converted;

	iconv_format = iconv_open("UTF8", "UCS-2BE");
	if (iconv_format == (iconv_t) -1) {
		ERRORA("error: %s\n", GSMOPEN_P_LOG, strerror(errno));
		return -1;
	}

	inbytesleft = i;
	DEBUGA_GSMOPEN("1 ciao in=%s, inleft=%d, out=%s, outleft=%d, converted=%s, utf8_out=%s\n",
				   GSMOPEN_P_LOG, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, converted, utf8_out);

#ifdef WIN32
	iconv_res = iconv(iconv_format, (const char **) &inbuf, &inbytesleft, &outbuf, &outbytesleft);
#else // WIN32
	iconv_res = iconv(iconv_format, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
#endif // WIN32
	if (iconv_res == (size_t) -1) {
		DEBUGA_GSMOPEN("2 ciao in=%s, inleft=%d, out=%s, outleft=%d, converted=%s, utf8_out=%s\n",
					   GSMOPEN_P_LOG, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, converted, utf8_out);
		DEBUGA_GSMOPEN("3 error: %s %d\n", GSMOPEN_P_LOG, strerror(errno), errno);
		iconv_close(iconv_format);
		return -1;
	}
	DEBUGA_GSMOPEN
		("iconv_res=%d,  in=%s, inleft=%d, out=%s, outleft=%d, converted=%s, utf8_out=%s\n",
		 GSMOPEN_P_LOG, iconv_res, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, converted, utf8_out);
	iconv_close(iconv_format);

	return 0;
}

int utf8_to_iso_8859_1(private_t *tech_pvt, char *utf8_in, size_t inbytesleft, char *iso_8859_1_out, size_t outbytesleft)
{
	iconv_t iconv_format;
	int iconv_res;
	char *outbuf;
	char *inbuf;

	outbuf = iso_8859_1_out;
	inbuf = utf8_in;

	iconv_format = iconv_open("ISO_8859-1", "UTF8");
	if (iconv_format == (iconv_t) -1) {
		ERRORA("error: %s\n", GSMOPEN_P_LOG, strerror(errno));
		return -1;
	}
	outbytesleft = strlen(utf8_in) * 2;

	DEBUGA_GSMOPEN("in=%s, inleft=%d, out=%s, outleft=%d, utf8_in=%s, iso_8859_1_out=%s\n",
				   GSMOPEN_P_LOG, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf8_in, iso_8859_1_out);
#ifdef WIN32
	iconv_res = iconv(iconv_format, (const char **) &inbuf, &inbytesleft, &outbuf, &outbytesleft);
#else // WIN32
	iconv_res = iconv(iconv_format, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
#endif // WIN32
	if (iconv_res == (size_t) -1) {
		DEBUGA_GSMOPEN("cannot translate in iso_8859_1 error: %s (errno: %d)\n", GSMOPEN_P_LOG, strerror(errno), errno);
		return -1;
	}
	DEBUGA_GSMOPEN
		("iconv_res=%d,  in=%s, inleft=%d, out=%s, outleft=%d, utf8_in=%s, iso_8859_1_out=%s\n",
		 GSMOPEN_P_LOG, iconv_res, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf8_in, iso_8859_1_out);
	iconv_close(iconv_format);
	return 0;
}


int iso_8859_1_to_utf8(private_t *tech_pvt, char *iso_8859_1_in, char *utf8_out, size_t outbytesleft)
{
	iconv_t iconv_format;
	int iconv_res;
	char *outbuf;
	char *inbuf;
	size_t inbytesleft;

	DEBUGA_GSMOPEN("iso_8859_1_in=%s\n", GSMOPEN_P_LOG, iso_8859_1_in);

	outbuf = utf8_out;
	inbuf = iso_8859_1_in;

	iconv_format = iconv_open("UTF8", "ISO_8859-1");
	if (iconv_format == (iconv_t) -1) {
		ERRORA("error: %s\n", GSMOPEN_P_LOG, strerror(errno));
		return -1;
	}

	inbytesleft = strlen(iso_8859_1_in) * 2;
#ifdef WIN32
	iconv_res = iconv(iconv_format, (const char **) &inbuf, &inbytesleft, &outbuf, &outbytesleft);
#else // WIN32
	iconv_res = iconv(iconv_format, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
#endif // WIN32
	if (iconv_res == (size_t) -1) {
		DEBUGA_GSMOPEN("ciao in=%s, inleft=%d, out=%s, outleft=%d, utf8_out=%s\n",
					   GSMOPEN_P_LOG, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf8_out);
		DEBUGA_GSMOPEN("error: %s %d\n", GSMOPEN_P_LOG, strerror(errno), errno);
		return -1;
	}
	DEBUGA_GSMOPEN
		(" strlen(iso_8859_1_in)=%d, iconv_res=%d,  inbuf=%s, inleft=%d, out=%s, outleft=%d, utf8_out=%s\n",
		 GSMOPEN_P_LOG, (int) strlen(iso_8859_1_in), iconv_res, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf8_out);

	iconv_close(iconv_format);

	return 0;
}

int utf8_to_ucs2(private_t *tech_pvt, char *utf8_in, size_t inbytesleft, char *ucs2_out, size_t outbytesleft)
{
	iconv_t iconv_format;
	int iconv_res;
	char *outbuf;
	char *inbuf;
	char converted[16000];
	int i;
	char stringa[16];
	char stringa2[16];

	memset(converted, '\0', sizeof(converted));

	outbuf = converted;
	inbuf = utf8_in;

	iconv_format = iconv_open("UCS-2BE", "UTF8");
	if (iconv_format == (iconv_t) -1) {
		ERRORA("error: %s\n", GSMOPEN_P_LOG, strerror(errno));
		return -1;
	}
	outbytesleft = 16000;

	DEBUGA_GSMOPEN("in=%s, inleft=%d, out=%s, outleft=%d, utf8_in=%s, converted=%s\n",
				   GSMOPEN_P_LOG, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf8_in, converted);
#ifdef WIN32
	iconv_res = iconv(iconv_format, (const char **) &inbuf, &inbytesleft, &outbuf, &outbytesleft);
#else // WIN32
	iconv_res = iconv(iconv_format, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
#endif // WIN32
	if (iconv_res == (size_t) -1) {
		ERRORA("error: %s %d\n", GSMOPEN_P_LOG, strerror(errno), errno);
		return -1;
	}
	DEBUGA_GSMOPEN
		("iconv_res=%d,  in=%s, inleft=%d, out=%s, outleft=%d, utf8_in=%s, converted=%s\n",
		 GSMOPEN_P_LOG, iconv_res, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf8_in, converted);
	iconv_close(iconv_format);

	for (i = 0; i < 16000 - (int) outbytesleft; i++) {
		memset(stringa, '\0', sizeof(stringa));
		memset(stringa2, '\0', sizeof(stringa2));
		sprintf(stringa, "%02X", converted[i]);
		DEBUGA_GSMOPEN("character is |%02X|\n", GSMOPEN_P_LOG, converted[i]);
		stringa2[0] = stringa[strlen(stringa) - 2];
		stringa2[1] = stringa[strlen(stringa) - 1];
		strncat(ucs2_out, stringa2, ((outbytesleft - strlen(ucs2_out)) - 1));	//add the received line to the buffer
		DEBUGA_GSMOPEN("stringa=%s, stringa2=%s, ucs2_out=%s\n", GSMOPEN_P_LOG, stringa, stringa2, ucs2_out);
	}
	return 0;
}

/*! \brief  Answer incoming call,
 * Part of PBX interface */
int gsmopen_answer(private_t *tech_pvt)
{
	int res;

	if (option_debug) {
		DEBUGA_PBX("ENTERING FUNC\n", GSMOPEN_P_LOG);
	}
	/* do something to actually answer the call, if needed (eg. pick up the phone) */
	if (tech_pvt->controldevprotocol != PROTOCOL_NO_SERIAL) {
		if (gsmopen_serial_answer(tech_pvt)) {
			ERRORA("gsmopen_answer FAILED\n", GSMOPEN_P_LOG);
			if (option_debug) {
				DEBUGA_PBX("EXITING FUNC\n", GSMOPEN_P_LOG);
			}
			return -1;
		}
	}
	tech_pvt->interface_state = GSMOPEN_STATE_UP;
	tech_pvt->phone_callflow = CALLFLOW_CALL_ACTIVE;

	while (tech_pvt->interface_state == GSMOPEN_STATE_RING) {
		gsmopen_sleep(10000);	//10msec
	}
	if (tech_pvt->interface_state != GSMOPEN_STATE_UP) {
		ERRORA("call answering failed\n", GSMOPEN_P_LOG);
		res = -1;
	} else {
		if (option_debug)
			DEBUGA_PBX("call answered\n", GSMOPEN_P_LOG);
		res = 0;

		if (tech_pvt->owner) {
			DEBUGA_PBX("going to send GSMOPEN_STATE_UP\n", GSMOPEN_P_LOG);
			ast_setstate(tech_pvt->owner, GSMOPEN_STATE_UP);
			DEBUGA_PBX("just sent GSMOPEN_STATE_UP\n", GSMOPEN_P_LOG);
		}
	}
	if (option_debug) {
		DEBUGA_PBX("EXITING FUNC\n", GSMOPEN_P_LOG);
	}
	return res;
}

int gsmopen_ring(private_t *tech_pvt)
{
	int res = 0;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	session = switch_core_session_locate(tech_pvt->session_uuid_str);
	if (session) {
		switch_core_session_rwunlock(session);
		return 0;
	}

	new_inbound_channel(tech_pvt);

	gsmopen_sleep(10000);

	session = switch_core_session_locate(tech_pvt->session_uuid_str);
	if (session) {
		channel = switch_core_session_get_channel(session);

		switch_core_session_queue_indication(session, SWITCH_MESSAGE_INDICATE_RINGING);
		if (channel) {
			switch_channel_mark_ring_ready(channel);
			DEBUGA_GSMOPEN("switch_channel_mark_ring_ready(channel);\n", GSMOPEN_P_LOG);
		} else {
			ERRORA("no channel\n", GSMOPEN_P_LOG);
		}
		switch_core_session_rwunlock(session);
	} else {
		ERRORA("no session\n", GSMOPEN_P_LOG);

	}

	return res;
}

/*! \brief  Hangup gsmopen call
 * Part of PBX interface, called from ast_hangup */

int gsmopen_hangup(private_t *tech_pvt)
{

	/* if there is not gsmopen pvt why we are here ? */
	if (!tech_pvt) {
		ERRORA("Asked to hangup channel not connected\n", GSMOPEN_P_LOG);
		return 0;
	}

	DEBUGA_GSMOPEN("ENTERING FUNC\n", GSMOPEN_P_LOG);

	if (tech_pvt->controldevprotocol != PROTOCOL_NO_SERIAL) {
		if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
			/* actually hangup through the serial port */
			if (tech_pvt->controldevprotocol != PROTOCOL_NO_SERIAL) {
				int res;
				res = gsmopen_serial_hangup(tech_pvt);
				if (res) {
					ERRORA("gsmopen_serial_hangup error: %d\n", GSMOPEN_P_LOG, res);
					if (option_debug) {
						DEBUGA_PBX("EXITING FUNC\n", GSMOPEN_P_LOG);
					}
					return -1;
				}
			}

			while (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
				gsmopen_sleep(10000);	//10msec
			}
			if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
				ERRORA("call hangup failed\n", GSMOPEN_P_LOG);
				return -1;
			} else {
				DEBUGA_GSMOPEN("call hungup\n", GSMOPEN_P_LOG);
			}
		}
	} else {
		tech_pvt->interface_state = GSMOPEN_STATE_DOWN;
		tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
	}

	switch_set_flag(tech_pvt, TFLAG_HANGUP);
	if (option_debug) {
		DEBUGA_PBX("EXITING FUNC\n", GSMOPEN_P_LOG);
	}
	return 0;
}

int gsmopen_call(private_t *tech_pvt, char *rdest, int timeout)
{

	int result;

	DEBUGA_GSMOPEN("Calling GSM, rdest is: %s\n", GSMOPEN_P_LOG, rdest);

	result=gsmopen_serial_call(tech_pvt, rdest);
	return result;
}

int gsmopen_senddigit(private_t *tech_pvt, char digit)
{

	DEBUGA_GSMOPEN("DIGIT received: %c\n", GSMOPEN_P_LOG, digit);
	if (tech_pvt->controldevprotocol == PROTOCOL_AT && tech_pvt->at_send_dtmf[0]) {
		int res = 0;
		char at_command[256];

		memset(at_command, '\0', 256);
		sprintf(at_command, "%s=1,%c", tech_pvt->at_send_dtmf, digit);
		res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
		if (res) {
			DEBUGA_GSMOPEN("XXX answer (OK) takes long to come, goes into timeout. command used: '%s=1,%c'\n", GSMOPEN_P_LOG, tech_pvt->at_send_dtmf,
						   digit);
		}
	}

	return 0;
}

int gsmopen_sendsms(private_t *tech_pvt, char *dest, char *text)
{
	int failed = 0;
	int err = 0;
	char mesg_test[1024];

	DEBUGA_GSMOPEN("GSMopenSendsms: dest=%s text=%s\n", GSMOPEN_P_LOG, dest, text);
	DEBUGA_GSMOPEN("START\n", GSMOPEN_P_LOG);
	/* we can use gsmopen_request to get the channel, but gsmopen_request would look for onowned channels, and probably we can send SMSs while a call is ongoing
	 *
	 */

	if (tech_pvt->controldevprotocol != PROTOCOL_AT) {
		ERRORA(", GSMOPEN_P_LOGGSMopenSendsms supports only AT command cellphones at the moment :-( !\n", GSMOPEN_P_LOG);
		return RESULT_FAILURE;
	}

	if (tech_pvt->controldevprotocol == PROTOCOL_AT) {
		char smscommand[16000];
		memset(smscommand, '\0', sizeof(smscommand));
		char pdu2[16000];
		memset(pdu2, '\0', sizeof(pdu2));
		int pdulenght = 0;
		string pdu;

		PUSHA_UNLOCKA(&tech_pvt->controldev_lock);
		LOKKA(tech_pvt->controldev_lock);

		if (tech_pvt->no_ucs2 || tech_pvt->sms_pdu_not_supported == 0) {
			try {
				int bad_8859 = 0;

				memset(mesg_test, '\0', sizeof(mesg_test));
				sprintf(mesg_test, ":) ciao belè новости לק ראת ﺎﻠﺠﻤﻋﺓ 人大aèéàòçù");	//let's test the beauty of utf8
				//sprintf(mesg_test,":) ciao belè èéàòìù");
				//text=mesg_test;

				bad_8859 = utf8_to_iso_8859_1(tech_pvt, text, strlen(text), smscommand, sizeof(smscommand));
				if (!bad_8859) {
					err = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=0");
					if (err) {
						ERRORA("AT+CMGF=0 (set message sending to PDU (as opposed to TEXT)  didn't get OK from the phone\n", GSMOPEN_P_LOG);
					}
					SMSMessageRef smsMessage;
					smsMessage = new SMSSubmitMessage(smscommand, dest);
					pdu = smsMessage->encode();
					strncpy(pdu2, pdu.c_str(), sizeof(pdu2) - 1);
					memset(smscommand, '\0', sizeof(smscommand));
					pdulenght = pdu.length() / 2 - 1;
					sprintf(smscommand, "AT+CMGS=%d", pdulenght);
				} else {
					int ok;

					UNLOCKA(tech_pvt->controldev_lock);
					POPPA_UNLOCKA(&tech_pvt->controldev_lock);

					tech_pvt->no_ucs2 = 0;
					tech_pvt->sms_pdu_not_supported = 1;

					ok = gsmopen_sendsms(tech_pvt, dest, text);

					tech_pvt->no_ucs2 = 1;
					tech_pvt->sms_pdu_not_supported = 0;

					err = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=0");
					if (err) {
						ERRORA("AT+CMGF=0 (set message sending to PDU (as opposed to TEXT)  didn't get OK from the phone\n", GSMOPEN_P_LOG);
					}
					return ok;
				}

			}
			catch(GsmException & ge) {
				ERRORA("GsmException= |||%s|||\n", GSMOPEN_P_LOG, ge.what());
			}



		} else {
			char dest2[1048];

			err = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=1");
			if (err) {
				ERRORA("AT+CMGF=1 (set message sending to TEXT (as opposed to PDU)  didn't get OK from the phone\n", GSMOPEN_P_LOG);
			}

			memset(dest2, '\0', sizeof(dest2));
			utf8_to_ucs2(tech_pvt, dest, strlen(dest), dest2, sizeof(dest2));
			sprintf(smscommand, "AT+CMGS=\"%s\"", dest2);
		}
		err = gsmopen_serial_write_AT_noack(tech_pvt, smscommand);
		if (err) {
			ERRORA("Error sending SMS\n", GSMOPEN_P_LOG);
			failed = 1;
			goto uscita;
		}
		err = gsmopen_serial_AT_expect(tech_pvt, "> ", 0, 1);	// wait 1.1s for the prompt, no  crlf
		if (err) {
			DEBUGA_GSMOPEN
				("Error or timeout getting prompt '> ' for sending sms directly to the remote party. BTW, seems that we cannot do that with Motorola c350, so we'll write to cellphone memory, then send from memory\n",
				 GSMOPEN_P_LOG);

			err = gsmopen_serial_write_AT_ack(tech_pvt, "ATE1");	//motorola (at least c350) do not echo the '>' prompt when in ATE0... go figure!!!!
			if (err) {
				ERRORA("Error activating echo from modem\n", GSMOPEN_P_LOG);
			}
			tech_pvt->at_cmgw[0] = '\0';
			sprintf(smscommand, "AT+CMGW=\"%s\"", dest);	//TODO: support phones that only accept pdu mode
			err = gsmopen_serial_write_AT_noack(tech_pvt, smscommand);
			if (err) {
				ERRORA("Error writing SMS destination to the cellphone memory\n", GSMOPEN_P_LOG);
				failed = 1;
				goto uscita;
			}
			err = gsmopen_serial_AT_expect(tech_pvt, "> ", 0, 1);	// wait 1.5s for the prompt, no  crlf
			if (err) {
				ERRORA("Error or timeout getting prompt '> ' for writing sms text in cellphone memory\n", GSMOPEN_P_LOG);
				failed = 1;
				goto uscita;
			}
		}

		if (tech_pvt->no_ucs2 || tech_pvt->sms_pdu_not_supported == 0) {
			memset(smscommand, '\0', sizeof(smscommand));
			sprintf(smscommand, "%s", pdu2);
		} else {
			memset(mesg_test, '\0', sizeof(mesg_test));
			sprintf(mesg_test, ":) ciao belè новости לק ראת ﺎﻠﺠﻤﻋﺓ 人大aèéàòçù");	//let's test the beauty of utf8
			//text=mesg_test;

			memset(smscommand, '\0', sizeof(smscommand));
			if (tech_pvt->no_ucs2) {
				sprintf(smscommand, "%s", text);
			} else {
				utf8_to_ucs2(tech_pvt, text, strlen(text), smscommand, sizeof(smscommand));
			}

		}
		smscommand[strlen(smscommand)] = 0x1A;
		DEBUGA_GSMOPEN("smscommand len is: %d, text is:|||%s|||\n", GSMOPEN_P_LOG, (int) strlen(smscommand), smscommand);

		err = gsmopen_serial_write_AT_ack_nocr_longtime(tech_pvt, smscommand);
		//TODO would be better to unlock controldev here
		if (err) {
			ERRORA("Error writing SMS text to the cellphone memory\n", GSMOPEN_P_LOG);
			failed = 1;
			goto uscita;
		}
		if (tech_pvt->at_cmgw[0]) {
			sprintf(smscommand, "AT+CMSS=%s", tech_pvt->at_cmgw);
			err = gsmopen_serial_write_AT_expect_longtime(tech_pvt, smscommand, "OK");
			if (err) {
				ERRORA("Error sending SMS from the cellphone memory\n", GSMOPEN_P_LOG);
				failed = 1;
				goto uscita;
			}

			err = gsmopen_serial_write_AT_ack(tech_pvt, "ATE0");	//motorola (at least c350) do not echo the '>' prompt when in ATE0... go figure!!!!
			if (err) {
				ERRORA("Error de-activating echo from modem\n", GSMOPEN_P_LOG);
			}
		}
	  uscita:
		gsmopen_sleep(1000);

		if (tech_pvt->at_cmgw[0]) {

			/* let's see what we've sent, just for check TODO: Motorola isn't reliable! Motorola c350 tells that all was sent, but is not true! It just sends how much it fits into one SMS FIXME: need an algorithm to calculate how many ucs2 chars fits into an SMS. It make difference based, probably, on the GSM alphabet translation, or so */
			sprintf(smscommand, "AT+CMGR=%s", tech_pvt->at_cmgw);
			err = gsmopen_serial_write_AT_ack(tech_pvt, smscommand);
			if (err) {
				ERRORA("Error reading SMS back from the cellphone memory\n", GSMOPEN_P_LOG);
			}

			/* let's delete from cellphone memory what we've sent */
			sprintf(smscommand, "AT+CMGD=%s", tech_pvt->at_cmgw);
			err = gsmopen_serial_write_AT_ack(tech_pvt, smscommand);
			if (err) {
				ERRORA("Error deleting SMS from the cellphone memory\n", GSMOPEN_P_LOG);
			}

			tech_pvt->at_cmgw[0] = '\0';
		}
		UNLOCKA(tech_pvt->controldev_lock);
		POPPA_UNLOCKA(&tech_pvt->controldev_lock);
	}

	DEBUGA_GSMOPEN("FINISH\n", GSMOPEN_P_LOG);
	if (failed)
		return -1;
	else
		return RESULT_SUCCESS;
}

int gsmopen_ussd(private_t *tech_pvt, char *ussd, int waittime)
{
	int res = 0;
	DEBUGA_GSMOPEN("gsmopen_ussd: %s\n", GSMOPEN_P_LOG, ussd);
	if (tech_pvt->controldevprotocol == PROTOCOL_AT) {
		char at_command[1024];

		string ussd_enc = latin1ToGsm(ussd);
		SMSEncoder e;
		e.markSeptet();
		e.setString(ussd_enc);
		string ussd_hex = e.getHexString();

		memset(at_command, '\0', sizeof(at_command));
		tech_pvt->ussd_received = 0;
		if (tech_pvt->ussd_request_encoding == USSD_ENCODING_PLAIN  
					||tech_pvt->ussd_request_encoding == USSD_ENCODING_AUTO) {
			snprintf(at_command, sizeof(at_command), "AT+CUSD=1,\"%s\",15", ussd_enc.c_str());
			res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
			if (res && tech_pvt->ussd_request_encoding == USSD_ENCODING_AUTO) {
				DEBUGA_GSMOPEN("Plain request failed, trying HEX7 encoding...\n", GSMOPEN_P_LOG);
				snprintf(at_command, sizeof(at_command), "AT+CUSD=1,\"%s\",15", ussd_hex.c_str());
				res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
				if (res == 0) {
					DEBUGA_GSMOPEN("HEX 7-bit request encoding will be used from now on\n", GSMOPEN_P_LOG);
					tech_pvt->ussd_request_encoding = USSD_ENCODING_HEX_7BIT;
				}
			}
		} else if (tech_pvt->ussd_request_encoding == USSD_ENCODING_HEX_7BIT) {
			snprintf(at_command, sizeof(at_command), "AT+CUSD=1,\"%s\",15", ussd_hex.c_str());
			res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
		} else if (tech_pvt->ussd_request_encoding == USSD_ENCODING_HEX_8BIT) {
			string ussd_h8 = bufToHex((const unsigned char*)ussd_enc.c_str(), ussd_enc.length()); 
			snprintf(at_command, sizeof(at_command), "AT+CUSD=1,\"%s\",15", ussd_h8.c_str());
			res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
		} else if (tech_pvt->ussd_request_encoding == USSD_ENCODING_UCS2) {
			char ussd_ucs2[1000];
			memset(ussd_ucs2, '\0', sizeof(ussd_ucs2));
			utf8_to_ucs2(tech_pvt, ussd, strlen(ussd), ussd_ucs2, sizeof(ussd_ucs2));
			snprintf(at_command, sizeof(at_command), "AT+CUSD=1,\"%s\",15", ussd_ucs2);
			res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
		}
		if (res) {
			return res;
		}
		if (waittime > 0)
			res = gsmopen_serial_read_AT(tech_pvt, 1, 0, waittime, "+CUSD", 1);
	}
	return res;
}

/************************************************/

/* LUIGI RIZZO's magic */
/* boost support. BOOST_SCALE * 10 ^(BOOST_MAX/20) must
 * be representable in 16 bits to avoid overflows.
 */
#define BOOST_SCALE     (1<<9)
#define BOOST_MAX       40		/* slightly less than 7 bits */

/*
 * store the boost factor
 */
void gsmopen_store_boost(char *s, double *boost)
{
	private_t *tech_pvt = NULL;

	if (sscanf(s, "%lf", boost) != 1) {
		ERRORA("invalid boost <%s>\n", GSMOPEN_P_LOG, s);
		return;
	}
	if (*boost < -BOOST_MAX) {
		WARNINGA("boost %s too small, using %d\n", GSMOPEN_P_LOG, s, -BOOST_MAX);
		*boost = -BOOST_MAX;
	} else if (*boost > BOOST_MAX) {
		WARNINGA("boost %s too large, using %d\n", GSMOPEN_P_LOG, s, BOOST_MAX);
		*boost = BOOST_MAX;
	}
#ifdef WIN32
	*boost = exp(log((double) 10) * *boost / 20) * BOOST_SCALE;
#else
	*boost = exp(log(10) * *boost / 20) * BOOST_SCALE;
#endif //WIN32
	if (option_debug > 1)
		DEBUGA_GSMOPEN("setting boost %s to %f\n", GSMOPEN_P_LOG, s, *boost);
}

int gsmopen_sound_boost(void *data, int samples_num, double boost)
{
/* LUIGI RIZZO's magic */
	if (boost != 0 && (boost < 511 || boost > 513)) {	/* scale and clip values */
		int i, x;

		int16_t *ptr = (int16_t *) data;

		for (i = 0; i < samples_num; i++) {
			x = (int) (ptr[i] * boost) / BOOST_SCALE;
			if (x > 32767) {
				x = 32767;
			} else if (x < -32768) {
				x = -32768;
			}
			ptr[i] = (int16_t) x;
		}
	} else {
		//printf("BOOST=%f\n", boost);
	}

	return 0;
}

int gsmopen_serial_getstatus_AT(private_t *tech_pvt)
{
	int res;
	private_t *p = tech_pvt;

	if (!p)
		return -1;

	PUSHA_UNLOCKA(p->controldev_lock);
	LOKKA(p->controldev_lock);
	res = gsmopen_serial_write_AT_ack(p, "AT");
	if (res) {
		ERRORA("AT was not acknowledged, continuing but maybe there is a problem\n", GSMOPEN_P_LOG);
	}
	gsmopen_sleep(1000);

	if (strlen(p->at_query_battchg)) {
		res = gsmopen_serial_write_AT_expect(p, p->at_query_battchg, p->at_query_battchg_expect);
		if (res) {
			WARNINGA("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, p->at_query_battchg, p->at_query_battchg_expect);
		}
		gsmopen_sleep(1000);
	}

	if (strlen(p->at_query_signal)) {
		res = gsmopen_serial_write_AT_expect(p, p->at_query_signal, p->at_query_signal_expect);
		if (res) {
			WARNINGA("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, p->at_query_signal, p->at_query_signal_expect);
		}
		gsmopen_sleep(1000);
	}

	if (!p->network_creg_not_supported) {
		res = gsmopen_serial_write_AT_ack(p, "AT+CREG?");
		if (res) {
			WARNINGA("%s didn't get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, "AT+CREG?", "OK");
		}
		gsmopen_sleep(1000);
	}
	//FIXME all the following commands in config!

	if (p->sms_cnmi_not_supported) {
		res = gsmopen_serial_write_AT_ack(p, "AT+MMGL=\"HEADER ONLY\"");
		if (res) {
			WARNINGA
				("%s didn't get %s from the phone. If your phone is not Motorola, please contact the gsmopen developers. Else, if your phone IS a Motorola, probably a long msg was incoming and ther first part was read and then deleted. The second part is now orphan. If you got this warning  repeatedly, and you cannot correctly receive SMSs from this interface, please manually clean all messages (and the residual parts of them) from the cellphone/SIM. Continuing.\n",
				 GSMOPEN_P_LOG, "AT+MMGL=\"HEADER ONLY\"", "OK");
		} else {
			gsmopen_sleep(1000);
			if (p->unread_sms_msg_id) {
				char at_command[256];

				res = gsmopen_serial_write_AT_ack(p, "AT+CSCS=\"UCS2\"");
				if (res) {
					ERRORA("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  didn't get OK from the phone\n", GSMOPEN_P_LOG);
					memset(p->sms_message, 0, sizeof(p->sms_message));
				}

				memset(at_command, 0, sizeof(at_command));
				sprintf(at_command, "AT+CMGR=%d", p->unread_sms_msg_id);
				memset(p->sms_message, 0, sizeof(p->sms_message));

				p->reading_sms_msg = 1;
				res = gsmopen_serial_write_AT_ack(p, at_command);
				p->reading_sms_msg = 0;
				if (res) {
					ERRORA("AT+CMGR (read SMS) didn't get OK from the phone, message sent was:|||%s|||\n", GSMOPEN_P_LOG, at_command);
				}
				res = gsmopen_serial_write_AT_ack(p, "AT+CSCS=\"GSM\"");
				if (res) {
					ERRORA("AT+CSCS=\"GSM\" (set TE messages to GSM) didn't get OK from the phone\n", GSMOPEN_P_LOG);
				}
				memset(at_command, 0, sizeof(at_command));
				sprintf(at_command, "AT+CMGD=%d", p->unread_sms_msg_id);	/* delete the message */
				p->unread_sms_msg_id = 0;
				res = gsmopen_serial_write_AT_ack(p, at_command);
				if (res) {
					ERRORA("AT+CMGD (Delete SMS) didn't get OK from the phone, message sent was:|||%s|||\n", GSMOPEN_P_LOG, at_command);
				}

				if (strlen(p->sms_message)) {
					DEBUGA_GSMOPEN("got SMS incoming message. SMS received was:---%s---\n", GSMOPEN_P_LOG, p->sms_message);
				}
			}
		}
	}

	UNLOCKA(p->controldev_lock);
	POPPA_UNLOCKA(p->controldev_lock);
	return 0;
}

int gsmopen_serial_init_audio_port(private_t *tech_pvt, int controldevice_audio_speed)
{
	/*
	 * TODO
	 * hmm, it doesn't look very different from gsmopen_serial_init_port, does it?
	 */

	if (!tech_pvt)
		return -1;

	tech_pvt->serialPort_serial_audio = new ctb::SerialPort();

	/* windows: com ports above com9 need a special trick, which also works for com ports below com10 ... */
	char devname[512] = "";
	strcpy(devname, tech_pvt->controldevice_audio_name);
#ifdef WIN32
	strcpy(devname,"\\\\.\\");
	strcat(devname, tech_pvt->controldevice_audio_name);
#endif

	if (tech_pvt->serialPort_serial_audio->Open(devname, 115200, "8N1", ctb::SerialPort::NoFlowControl) >= 0) {
		DEBUGA_GSMOPEN("port %s, SUCCESS open\n", GSMOPEN_P_LOG, tech_pvt->controldevice_audio_name);
		tech_pvt->serialPort_serial_audio_opened =1;
		gsmopen_serial_write_AT_expect(tech_pvt, "AT^DDSETEX=2", tech_pvt->at_dial_expect);
	} else {
#ifdef WIN32
		LPVOID msg;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
							NULL,
							GetLastError(),
							MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
							(LPTSTR) &msg,
							0,
							NULL);
		ERRORA("port open failed for %s - %s", GSMOPEN_P_LOG, devname, (LPCTSTR) msg);
		LocalFree(msg);
#else
		ERRORA("port %s, NOT open\n", GSMOPEN_P_LOG, tech_pvt->controldevice_audio_name);
#endif
		return -1;
	}

	return 0;
}

int serial_audio_init(private_t *tech_pvt)
{
	int res;
	int err;

	res = gsmopen_serial_init_audio_port(tech_pvt, tech_pvt->controldevice_audio_speed);
	DEBUGA_GSMOPEN("serial_audio_init res=%d\n", GSMOPEN_P_LOG, res);

	if (res == 0)
		err = 0;
	else
		err = 1;

	return err;
}

int serial_audio_shutdown(private_t *tech_pvt)
{

	int res;

	if (!tech_pvt || !tech_pvt->serialPort_serial_audio)
		return -1;

	res = tech_pvt->serialPort_serial_audio->Close();
	DEBUGA_GSMOPEN("serial_audio_shutdown res=%d (controldev_audio_fd is %d)\n", GSMOPEN_P_LOG, res, tech_pvt->controldev_audio_fd);
	tech_pvt->serialPort_serial_audio_opened =0;

	return res;
}
