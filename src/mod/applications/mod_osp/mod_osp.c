/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Contributor(s):
 *
 * Di-Shi Sun <di-shi@transnexus.com>
 *
 * mod_osp.c -- Open Settlement Protocol (OSP) Module
 *
 */

#include <switch.h>
#include <osp/osp.h>
#include <osp/ospb64.h>
#include <osp/osptrans.h>

/* OSP Buffer Size Constants */
#define OSP_SIZE_NORSTR		512		/* OSP normal string buffer size */
#define OSP_SIZE_KEYSTR		1024	/* OSP certificate string buffer size */
#define OSP_SIZE_ROUSTR		1024	/* OSP route buffer size */

/* OSP Module Configuration Constants */
#define OSP_CONFIG_FILE		"osp.conf"			/* OSP module configuration file name */
#define OSP_DEF_PROFILE		"default"			/* Default OSP profile name */
#define OSP_MAX_SPNUMBER	8					/* Max number of OSP service points */
#define OSP_DEF_LIFETIME	300					/* OSP default SSL lifetime */
#define OSP_MIN_MAXCONN		1					/* OSP min max connections */
#define OSP_MAX_MAXCONN		1000				/* OSP max max connections */
#define OSP_DEF_MAXCONN		20					/* OSP default max connections */
#define OSP_DEF_PERSIST		60					/* OSP default HTTP persistence in seconds */
#define OSP_MIN_RETRYDELAY	0					/* OSP min retry delay */
#define OSP_MAX_RETRYDELAY	10					/* OSP max retry delay */
#define OSP_DEF_RETRYDELAY	OSP_MIN_RETRYDELAY	/* OSP default retry delay in seconds */
#define OSP_MIN_RETRYLIMIT	0					/* OSP min retry times */
#define OSP_MAX_RETRYLIMIT	100					/* OSP max retry times */
#define OSP_DEF_RETRYLIMIT	2					/* OSP default retry times */
#define OSP_MIN_TIMEOUT		200					/* OSP min timeout in ms */
#define OSP_MAX_TIMEOUT		60000				/* OSP max timeout in ms */
#define OSP_DEF_TIMEOUT		10000				/* OSP default timeout in ms */
#define OSP_MIN_MAXDEST		1					/* OSP min max destinations */
#define OSP_MAX_MAXDEST		12					/* OSP max max destinations */
#define OSP_DEF_MAXDEST		OSP_MAX_MAXDEST		/* OSP default max destinations */

/* OSP Handle Constant */
#define OSP_INVALID_HANDLE	-1	/* Invalid OSP handle, provider, transaction etc. */

/* OSP Provider Contants */
#define OSP_AUDIT_URL		"localhost"	/* OSP default Audit URL */
#define OSP_LOCAL_VALID		1			/* OSP token validating method, locally */
#define OSP_CUSTOMER_ID		""			/* OSP customer ID */
#define OSP_DEVICE_ID		""			/* OSP device ID */

/* URI Contants */
#define OSP_URI_DELIM		'@'		/* URI delimit */
#define OSP_USER_DELIM		";:"	/* URI userinfo delimit */
#define OSP_HOST_DELIM		";>"	/* URI hostport delimit */

/* OSP Module Other Contants */
#define OSP_MAX_CINFO		8	/* Max number of custom info */
#define OSP_DEF_STRING		""	/* OSP default empty string */
#define OSP_DEF_STATS		-1	/* OSP default statistics */

/* OSP Supported Signaling Protocols for Default Protocol */
#define OSP_PROTOCOL_SIP	"sip"			/* SIP protocol name */
#define OSP_PROTOCOL_H323	"h323"			/* H.323 protocol name */
#define OSP_PROTOCOL_IAX	"iax"			/* IAX protocol name */
#define OSP_PROTOCOL_SKYPE	"skype"			/* Skype protocol name */
#define OSP_PROTOCOL_UNKNO	"unknown"		/* Unknown protocol */
#define OSP_PROTOCOL_UNDEF	"undefined"		/* Undefined protocol */
#define OSP_PROTOCOL_UNSUP	"unsupported"	/* Unsupported protocol */

/* OSP Supported Signaling Protocols for Signaling Protocol Usage */
#define OSP_MODULE_SIP		"mod_sofia"		/* FreeSWITCH SIP module name */
#define OSP_MODULE_H323		"mod_h323"		/* FreeSWITCH H.323 module name */
#define OSP_MODULE_IAX		"mod_iax"		/* FreeSWITCH IAX module name */
#define OSP_MODULE_SKYPE	"mod_skypopen"	/* FreeSWITCH Skype module name */

/* OSP Variable Names */
#define OSP_VAR_SRCDEV			"osp_source_device"			/* Source device IP, inbound (actual source device)*/
#define OSP_VAR_SRCNID			"osp_source_nid"			/* Source network ID, inbound */
#define OSP_VAR_CUSTOMINFO		"osp_custom_info_"			/* Custom info, inbound */
#define OSP_VAR_DNIDUSERPARAM	"osp_networkid_userparam"	/* Destination network ID user parameter name, outbound */
#define OSP_VAR_DNIDURIPARAM	"osp_networkid_uriparam"	/* Destination network ID URI parameter name, outbound */
#define OSP_VAR_USERPHONE		"osp_user_phone"			/* If to add "user=phone", outbound */
#define OSP_VAR_OUTPROXY		"osp_outbound_proxy"		/* Outbound proxy, outbound */
#define OSP_VAR_PROFILE			"osp_profile_name"			/* Profile name */
#define OSP_VAR_TRANSACTION		"osp_transaction_handle"	/* Transaction handle */
#define OSP_VAR_TRANSID			"osp_transaction_id"		/* Transaction ID */
#define OSP_VAR_ROUTETOTAL		"osp_route_total"			/* Total number of destinations */
#define OSP_VAR_ROUTECOUNT		"osp_route_count"			/* Destination count */
#define OSP_VAR_TCCODE			"osp_termination_cause"		/* Terimation cause */
#define OSP_VAR_AUTOROUTE		"osp_auto_route"			/* Bridge route string */
#define OSP_VAR_LOOKUPSTATUS	"osp_lookup_status"			/* OSP lookup function status */
#define OSP_VAR_NEXTSTATUS		"osp_next_status"			/* OSP next function status */

/* OSP Using FreeSWITCH Variable Names */
#define OSP_FS_CALLID			"sip_call_id"						/* Inbound SIP Call-ID */
#define OSP_FS_FROMUSER			"sip_from_user"						/* Inbound SIP From user */
#define OSP_FS_TOHOST			"sip_to_host"						/* Inbound SIP To host */
#define OSP_FS_TOPORT			"sip_to_port"						/* Inbound SIP To port */
#define OSP_FS_RPID				"sip_Remote-Party-ID"				/* Inbound SIP Remote-Party-ID header */
#define OSP_FS_PAI				"sip_P-Asserted-Identity"			/* Inbound SIP P-Asserted-Identity header */
#define OSP_FS_DIV				"sip_h_Diversion"					/* Inbound SIP Diversion header */
#define OSP_FS_PCI				"sip_h_P-Charge-Info"				/* Inbound SIP P-Charge-Info header */
#define OSP_FS_OUTCALLING		"origination_caller_id_number"		/* Outbound calling number */
#define OSP_FS_SIPRELEASE		"sip_hangup_disposition"			/* Usage SIP release source */
#define OSP_FS_SRCCODEC			"write_codec"						/* Usage source codec */
#define OSP_FS_DESTCODEC		"read_codec"						/* Usage destiantion codec */
#define OSP_FS_RTPSRCREPOCTS	"rtp_audio_out_media_bytes"			/* Usage source->reporter octets */
#define OSP_FS_RTPDESTREPOCTS	"rtp_audio_in_media_bytes"			/* Usage destination->reporter octets */
#define OSP_FS_RTPSRCREPPKTS	"rtp_audio_out_media_packet_count"	/* Usage source->reporter packets */
#define OSP_FS_RTPDESTREPPKTS	"rtp_audio_in_media_packet_count"	/* Usage destination->reporter packets */
#define OSP_FS_HANGUPCAUSE		"last_bridge_hangup_cause"			/* Termination cause */

/* FreeSWITCH Endpoint Parameters */
typedef struct osp_endpoint {
	const char *module;		/* Endpoint module name */
	const char *profile;	/* Endpoint profile name */
} osp_endpoint_t;

/* OSP Global Status */
typedef struct osp_global {
	switch_bool_t debug;							/* OSP module debug flag */
	switch_log_level_t loglevel;					/* Log level for debug messages */
	switch_bool_t hardware;							/* Crypto hardware flag */
	osp_endpoint_t endpoint[OSPC_PROTNAME_NUMBER];	/* Used endpoints */
	OSPE_PROTOCOL_NAME protocol;					/* Default signaling protocol */
	switch_bool_t shutdown;							/* OSP module status */
	switch_memory_pool_t *pool;						/* OSP module memory pool */
} osp_global_t;

/* OSP Work Modes */
typedef enum osp_workmode {
	OSP_MODE_DIRECT = 0,	/* Direct work mode */
	OSP_MODE_INDIRECT		/* Indirect work mode */
} osp_workmode_t;

/* OSP Service Types */
typedef enum osp_srvtype {
	OSP_SRV_VOICE = 0,	/* Normal voice service */
	OSP_SRV_NPQUERY		/* Number portability query service */
} osp_srvtype_t;

/* OSP Profile Parameters */
typedef struct osp_profile {
	const char *name;						/* OSP profile name */
	int spnumber;							/* Number of OSP service points */
	const char *spurl[OSP_MAX_SPNUMBER];	/* OSP service point URLs */
	const char *deviceip;					/* OSP client end IP */
	int lifetime;							/* SSL life time */
	int maxconnect;							/* Max number of HTTP connections */
	int persistence;						/* HTTP persistence in seconds */
	int retrydelay;							/* HTTP retry delay in seconds */
	int retrylimit;							/* HTTP retry times */
	int timeout;							/* HTTP timeout in ms */
	osp_workmode_t workmode;				/* OSP work mode */
	osp_srvtype_t srvtype;					/* OSP service type */
	int maxdest;							/* Max destinations */
	OSPTPROVHANDLE provider;				/* OSP provider handle */
	struct osp_profile *next;				/* Next OSP profile */
} osp_profile_t;

/* OSP Inbound Parameters */
typedef struct osp_inbound {
	const char *actsrc;					/* Actual source device IP address */
	const char *srcdev;					/* Source device IP address */
	const char *srcnid;					/* Source network ID */
	OSPE_PROTOCOL_NAME protocol;		/* Inbound signaling protocol */
	const char *callid;					/* Inbound Call-ID */
	char calling[OSP_SIZE_NORSTR];		/* Inbound calling number */
	char called[OSP_SIZE_NORSTR];		/* Inbound called number */
	char nprn[OSP_SIZE_NORSTR];			/* Inbound NP routing number */
	char npcic[OSP_SIZE_NORSTR];		/* Inbound NP carrier identification code */
	int npdi;							/* Inbound NP database dip indicator */
	const char *tohost;					/* Inbound host of To URI */
	const char *toport;					/* Inbound port of To URI */
	char rpiduser[OSP_SIZE_NORSTR];		/* Inbound user of SIP Remote-Party-ID header */
	char paiuser[OSP_SIZE_NORSTR];		/* Inbound user of SIP P-Asserted-Identity header */
	char divuser[OSP_SIZE_NORSTR];		/* Inbound user of SIP Diversion header */
	char divhost[OSP_SIZE_NORSTR];		/* Inbound hostport of SIP Diversion header */
	char pciuser[OSP_SIZE_NORSTR];		/* Inbound user of SIP P-Charge-Info header */
	const char *cinfo[OSP_MAX_CINFO];	/* Custom info */
} osp_inbound_t;

/* OSP Route Parameters */
typedef struct osp_results {
	const char *profile;								/* Profile name */
	OSPTTRANHANDLE transaction;							/* Transaction handle */
	uint64_t transid;									/* Transaction ID */
	unsigned int total;									/* Total number of destinations */
	unsigned int count;									/* Destination count starting from 1 */
	unsigned int timelimit;								/* Outbound duration limit */
	char calling[OSP_SIZE_NORSTR];						/* Outbound calling number, may be translated */
	char called[OSP_SIZE_NORSTR];						/* Outbound called number, may be translated */
	char dest[OSP_SIZE_NORSTR];							/* Destination IP address */
	char destnid[OSP_SIZE_NORSTR];						/* Destination network ID */
	char nprn[OSP_SIZE_NORSTR];							/* Outbound NP routing number */
	char npcic[OSP_SIZE_NORSTR];						/* Outbound NP carrier identification code */
	int npdi;											/* Outbound NP database dip indicator */
	char opname[OSPC_OPNAME_NUMBER][OSP_SIZE_NORSTR];	/* Outbound Operator names */
	OSPE_PROTOCOL_NAME protocol;						/* Signaling protocol */
	switch_bool_t supported;							/* Supported by FreeRADIUS OSP module */
	switch_call_cause_t cause;							/* Termination cause for current destination */
} osp_results_t;

/* OSP Outbound Parameters */
typedef struct osp_outbound {
	const char *dniduserparam;	/* Destination network ID user parameter name */
	const char *dniduriparam;	/* Destination network ID URI parameter name */
	switch_bool_t userphone;	/* If to add "user=phone" parameter */
	const char *outproxy;		/* Outbound proxy IP address */
} osp_outbound_t;

/* OSP Usage Parameters */
typedef struct osp_usage {
	OSPE_RELEASE release;		/* Release source */
	switch_call_cause_t cause;	/* Termination cause */
	switch_time_t start;		/* Call start time */
	switch_time_t alert;		/* Call alert time */
	switch_time_t connect;		/* Call answer time */
	switch_time_t end;			/* Call end time */
	switch_time_t duration;		/* Call duration */
	switch_time_t pdd;			/* Post dial delay, in us */
	const char *srccodec;		/* Source codec */
	const char *destcodec;		/* Destination codec */
	int rtpsrcrepoctets;		/* RTP source->reporter bytes */
	int rtpdestrepoctets;		/* RTP destination->reporter bytes */
	int rtpsrcreppackets;		/* RTP source->reporter packets */
	int rtpdestreppackets;		/* RTP destiantion->reporter packets */
} osp_usage_t;

/* Macro functions for debug */
#define OSP_DEBUG(_fmt, ...)				if (osp_global.debug) { switch_log_printf(SWITCH_CHANNEL_LOG, osp_global.loglevel, "%s: "_fmt"\n", __SWITCH_FUNC__, __VA_ARGS__); }
#define OSP_DEBUG_MSG(_msg)					OSP_DEBUG("%s", _msg)
#define OSP_DEBUG_START						OSP_DEBUG_MSG("Start")
#define OSP_DEBUG_END						OSP_DEBUG_MSG("End")
/* Macro to prevent NULL string */
#define OSP_FILTER_NULLSTR(_str)			(switch_strlen_zero(_str) ? OSP_DEF_STRING : (_str))
/* Macro to prevent NULL integer */
#define OSP_FILTER_NULLINT(_int)			((_int) ? *(_int) : 0)
/* Macro to adjust buffer length */
#define OSP_ADJUST_LEN(_head, _size, _len)	{ (_len) = strlen(_head); (_head) += (_len); (_size) -= (_len); }

/* OSP Module Global Status */
static osp_global_t osp_global;

/* OSP module profiles */
static osp_profile_t *osp_profiles = NULL;

/* OSP default certificates */
static const char *B64PKey = "MIIBOgIBAAJBAK8t5l+PUbTC4lvwlNxV5lpl+2dwSZGW46dowTe6y133XyVEwNiiRma2YNk3xKs/TJ3Wl9Wpns2SYEAJsFfSTukCAwEAAQJAPz13vCm2GmZ8Zyp74usTxLCqSJZNyMRLHQWBM0g44Iuy4wE3vpi7Wq+xYuSOH2mu4OddnxswCP4QhaXVQavTAQIhAOBVCKXtppEw9UaOBL4vW0Ed/6EA/1D8hDW6St0h7EXJAiEAx+iRmZKhJD6VT84dtX5ZYNVk3j3dAcIOovpzUj9a0CECIEduTCapmZQ5xqAEsLXuVlxRtQgLTUD4ZxDElPn8x0MhAiBE2HlcND0+qDbvtwJQQOUzDgqg5xk3w8capboVdzAlQQIhAMC+lDL7+gDYkNAft5Mu+NObJmQs4Cr+DkDFsKqoxqrm";
static const char *B64LCert = "MIIBeTCCASMCEHqkOHVRRWr+1COq3CR/xsowDQYJKoZIhvcNAQEEBQAwOzElMCMGA1UEAxMcb3NwdGVzdHNlcnZlci50cmFuc25leHVzLmNvbTESMBAGA1UEChMJT1NQU2VydmVyMB4XDTA1MDYyMzAwMjkxOFoXDTA2MDYyNDAwMjkxOFowRTELMAkGA1UEBhMCQVUxEzARBgNVBAgTClNvbWUtU3RhdGUxITAfBgNVBAoTGEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDBcMA0GCSqGSIb3DQEBAQUAA0sAMEgCQQCvLeZfj1G0wuJb8JTcVeZaZftncEmRluOnaME3ustd918lRMDYokZmtmDZN8SrP0yd1pfVqZ7NkmBACbBX0k7pAgMBAAEwDQYJKoZIhvcNAQEEBQADQQDnV8QNFVVJx/+7IselU0wsepqMurivXZzuxOmTEmTVDzCJx1xhA8jd3vGAj7XDIYiPub1PV23eY5a2ARJuw5w9";
static const char *B64CACert = "MIIBYDCCAQoCAQEwDQYJKoZIhvcNAQEEBQAwOzElMCMGA1UEAxMcb3NwdGVzdHNlcnZlci50cmFuc25leHVzLmNvbTESMBAGA1UEChMJT1NQU2VydmVyMB4XDTAyMDIwNDE4MjU1MloXDTEyMDIwMzE4MjU1MlowOzElMCMGA1UEAxMcb3NwdGVzdHNlcnZlci50cmFuc25leHVzLmNvbTESMBAGA1UEChMJT1NQU2VydmVyMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAPGeGwV41EIhX0jEDFLRXQhDEr50OUQPq+f55VwQd0TQNts06BP29+UiNdRW3c3IRHdZcJdC1Cg68ME9cgeq0h8CAwEAATANBgkqhkiG9w0BAQQFAANBAGkzBSj1EnnmUxbaiG1N4xjIuLAWydun7o3bFk2tV8dBIhnuh445obYyk1EnQ27kI7eACCILBZqi2MHDOIMnoN0=";

/*
 * Find OSP profile by name
 * param name OSP profile name
 * param profile OSP profile, NULL is allowed
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_find_profile(
	const char *name,
	osp_profile_t **profile)
{
	osp_profile_t *p;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	if (name) {
		if (profile) {
			*profile = NULL;
		}

		for (p = osp_profiles; p; p = p->next) {
			if (!strcasecmp(p->name, name)) {
				if (profile) {
					*profile = p;
				}
				status = SWITCH_STATUS_SUCCESS;
				break;
			}
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		OSP_DEBUG("Found profile '%s'", name);
	} else {
		OSP_DEBUG("Unable to find profile '%s'", name);
	}

	OSP_DEBUG_END;

	return status;
}

/*
 * Load OSP module configuration
 * param pool OSP module memory pool
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed, SWITCH_STATUS_MEMERR Memory Error.
 */
static switch_status_t osp_load_config(
	switch_memory_pool_t *pool)
{
	switch_xml_t xcfg, xml = NULL, xsettings, xparam, xprofile, xprofiles;
	const char *name;
	const char *value;
	const char *module;
	const char *context;
	osp_profile_t *profile;
	int number;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	OSP_DEBUG_START;

	/* Load OSP module configuration file */
	if (!(xml = switch_xml_open_cfg(OSP_CONFIG_FILE, &xcfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open OSP module configuration file '%s'\n", OSP_CONFIG_FILE);
		OSP_DEBUG_END;
		return SWITCH_STATUS_FALSE;
	}

	OSP_DEBUG_MSG("Parsing settings");

	/* Init OSP module global status */
	memset(&osp_global, 0, sizeof(osp_global));
	osp_global.loglevel = SWITCH_LOG_DEBUG;
	osp_global.protocol = OSPC_PROTNAME_SIP;
	osp_global.pool = pool;

	/* Get OSP module global settings */
	if ((xsettings = switch_xml_child(xcfg, "settings"))) {
		for (xparam = switch_xml_child(xsettings, "param"); xparam; xparam = xparam->next) {
			/* Settings parameter name */
			name = switch_xml_attr_soft(xparam, "name");
			/* Settings parameter value */
			value = switch_xml_attr_soft(xparam, "value");
			/* Endpoint module name */
			module = switch_xml_attr_soft(xparam, "module");
			/* Endpoint profile name */
			context = switch_xml_attr_soft(xparam, "profile");

			/* Ignore parameter without name */
			if (switch_strlen_zero(name)) {
				continue;
			}

			if (!strcasecmp(name, "debug-info")) {
				/* OSP module debug flag */
				if (!switch_strlen_zero(value)) {
					osp_global.debug = switch_true(value);
				}
				OSP_DEBUG("debug-info: '%d'", osp_global.debug);
			} else if (!strcasecmp(name, "log-level")) {
				/* OSP module debug message log level */
				if (switch_strlen_zero(value)) {
					continue;
				} else if (!strcasecmp(value, "console")) {
					osp_global.loglevel = SWITCH_LOG_CONSOLE;
				} else if (!strcasecmp(value, "alert")) {
					osp_global.loglevel = SWITCH_LOG_ALERT;
				} else if (!strcasecmp(value, "crit")) {
					osp_global.loglevel = SWITCH_LOG_CRIT;
				} else if (!strcasecmp(value, "error")) {
					osp_global.loglevel = SWITCH_LOG_ERROR;
				} else if (!strcasecmp(value, "warning")) {
					osp_global.loglevel = SWITCH_LOG_WARNING;
				} else if (!strcasecmp(value, "notice")) {
					osp_global.loglevel = SWITCH_LOG_NOTICE;
				} else if (!strcasecmp(value, "info")) {
					osp_global.loglevel = SWITCH_LOG_INFO;
				} else if (!strcasecmp(value, "debug")) {
					osp_global.loglevel = SWITCH_LOG_DEBUG;
				}
				OSP_DEBUG("log-level: '%d'", osp_global.loglevel);
			} else if (!strcasecmp(name, "crypto-hardware")) {
				/* OSP module crypto hardware flag */
				if (!switch_strlen_zero(value)) {
					osp_global.hardware = switch_true(value);
				}
				OSP_DEBUG("crypto-hardware: '%d'", osp_global.hardware);
			} else if (!strcasecmp(name, "default-protocol")) {
				/* OSP module default signaling protocol */
				if (switch_strlen_zero(value)) {
					continue;
				} else if (!strcasecmp(value, OSP_PROTOCOL_SIP)) {
					osp_global.protocol = OSPC_PROTNAME_SIP;
				} else if (!strcasecmp(value, OSP_PROTOCOL_H323)) {
					osp_global.protocol = OSPC_PROTNAME_Q931;
				} else if (!strcasecmp(value, OSP_PROTOCOL_IAX)) {
					osp_global.protocol = OSPC_PROTNAME_IAX;
				} else if (!strcasecmp(value, OSP_PROTOCOL_SKYPE)) {
					osp_global.protocol = OSPC_PROTNAME_SKYPE;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported protocol '%s'\n", value);
				}
				OSP_DEBUG("default-protocol: '%d'", osp_global.protocol);
			} else if (!strcasecmp(name, OSP_PROTOCOL_SIP)) {
				/* SIP endpoint module */
				if (!switch_strlen_zero(module)) {
					if (!(osp_global.endpoint[OSPC_PROTNAME_SIP].module = switch_core_strdup(osp_global.pool, module))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate SIP module name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				/* SIP endpoint profile */
				if (!switch_strlen_zero(context)) {
					if (!(osp_global.endpoint[OSPC_PROTNAME_SIP].profile = switch_core_strdup(osp_global.pool, context))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate SIP profile name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				OSP_DEBUG("SIP: '%s/%s'", osp_global.endpoint[OSPC_PROTNAME_SIP].module, osp_global.endpoint[OSPC_PROTNAME_SIP].profile);
			} else if (!strcasecmp(name, OSP_PROTOCOL_H323)) {
				/* H.323 endpoint module */
				if (!switch_strlen_zero(module)) {
					if (!(osp_global.endpoint[OSPC_PROTNAME_Q931].module = switch_core_strdup(osp_global.pool, module))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate H.323 module name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				/* H.323 endpoint profile */
				if (!switch_strlen_zero(context)) {
					if (!(osp_global.endpoint[OSPC_PROTNAME_Q931].profile = switch_core_strdup(osp_global.pool, context))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate H.323 profile name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				OSP_DEBUG("H.323: '%s/%s'", osp_global.endpoint[OSPC_PROTNAME_Q931].module, osp_global.endpoint[OSPC_PROTNAME_Q931].profile);
			} else if (!strcasecmp(name, OSP_PROTOCOL_IAX)) {
				/* IAX endpoint module */
				if (!switch_strlen_zero(module)) {
					if (!(osp_global.endpoint[OSPC_PROTNAME_IAX].module = switch_core_strdup(osp_global.pool, module))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate IAX module name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				/* IAX endpoint profile */
				if (!switch_strlen_zero(context)) {
					if (!(osp_global.endpoint[OSPC_PROTNAME_IAX].profile = switch_core_strdup(osp_global.pool, context))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate IAX profile name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				OSP_DEBUG("IAX: '%s/%s'", osp_global.endpoint[OSPC_PROTNAME_IAX].module, osp_global.endpoint[OSPC_PROTNAME_IAX].profile);
			} else if (!strcasecmp(name, OSP_PROTOCOL_SKYPE)) {
				/* Skype endpoint module */
				if (!switch_strlen_zero(module)) {
					if (!(osp_global.endpoint[OSPC_PROTNAME_SKYPE].module = switch_core_strdup(osp_global.pool, module))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate Skype module name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				/* Skype endpoint profile */
				if (!switch_strlen_zero(context)) {
					if (!(osp_global.endpoint[OSPC_PROTNAME_SKYPE].profile = switch_core_strdup(osp_global.pool, context))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate Skype profile name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				OSP_DEBUG("SKYPE: '%s/%s'", osp_global.endpoint[OSPC_PROTNAME_SKYPE].module, osp_global.endpoint[OSPC_PROTNAME_SKYPE].profile);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown parameter '%s'\n", name);
			}
		}
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		/* Fail for SWITCH_STATUS_MEMERR */
		switch_xml_free(xml);
		OSP_DEBUG_END;
		return status;
	}

	/* Get OSP module profiles */
	if ((xprofiles = switch_xml_child(xcfg, "profiles"))) {
		for (xprofile = switch_xml_child(xprofiles, "profile"); xprofile; xprofile = xprofile->next) {
			/* Profile name */
			name = switch_xml_attr_soft(xprofile, "name");
			if (switch_strlen_zero(name)) {
				name = OSP_DEF_PROFILE;
			}
			OSP_DEBUG("Parsing profile '%s'", name);

			/* Check duplate profile name */
			if (osp_find_profile(name, NULL) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignored duplicate profile '%s'\n", name);
				continue;
			}

			/* Allocate profile */
			if (!(profile = switch_core_alloc(osp_global.pool, sizeof(*profile)))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to alloc profile\n");
				status = SWITCH_STATUS_MEMERR;
				break;
			}

			/* Store profile name */
			if (!(profile->name = switch_core_strdup(osp_global.pool, name))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate profile name\n");
				status = SWITCH_STATUS_MEMERR;
				/* "profile" cannot free to pool in FreeSWITCH */
				break;
			}

			/* "profile" has been set to 0 by switch_core_alloc */
			profile->lifetime = OSP_DEF_LIFETIME;
			profile->maxconnect = OSP_DEF_MAXCONN;
			profile->persistence = OSP_DEF_PERSIST;
			profile->retrydelay = OSP_DEF_RETRYDELAY;
			profile->retrylimit = OSP_DEF_RETRYLIMIT;
			profile->timeout = OSP_DEF_TIMEOUT;
			profile->maxdest = OSP_DEF_MAXDEST;
			profile->provider = OSP_INVALID_HANDLE;

			for (xparam = switch_xml_child(xprofile, "param"); xparam; xparam = xparam->next) {
				/* Profile parameter name */
				name = switch_xml_attr_soft(xparam, "name");
				/* Profile parameter value */
				value = switch_xml_attr_soft(xparam, "value");

				/* Ignore profile parameter without name or value */
				if (switch_strlen_zero(name) || switch_strlen_zero(value)) {
					continue;
				}

				if (!strcasecmp(name, "service-point-url")) {
					/* OSP service point URL */
					if (profile->spnumber < OSP_MAX_SPNUMBER) {
						profile->spurl[profile->spnumber] = switch_core_strdup(osp_global.pool, value);
						OSP_DEBUG("service-point-url[%d]: '%s'", profile->spnumber, profile->spurl[profile->spnumber]);
						profile->spnumber++;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignored service point '%s'\n", value);
					}
				} else if (!strcasecmp(name, "device-ip")) {
					/* OSP client end IP */
					profile->deviceip = switch_core_strdup(osp_global.pool, value);
					OSP_DEBUG("device-ip: '%s'", profile->deviceip);
				} else if (!strcasecmp(name, "ssl-lifetime")) {
					/* SSL lifetime */
					if (sscanf(value, "%d", &number) == 1) {
						profile->lifetime = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "ssl-lifetime must be a number\n");
					}
					OSP_DEBUG("ssl-lifetime: '%d'", profile->lifetime);
				} else if (!strcasecmp(name, "http-max-connections")) {
					/* HTTP max connections */
					if ((sscanf(value, "%d", &number) == 1) && (number >= OSP_MIN_MAXCONN) && (number <= OSP_MAX_MAXCONN)) {
						profile->maxconnect = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "http-max-connections must be between %d and %d\n", OSP_MIN_MAXCONN, OSP_MAX_MAXCONN);
					}
					OSP_DEBUG("http-max-connections: '%d'", profile->maxconnect);
				} else if (!strcasecmp(name, "http-persistence")) {
					/* HTTP persistence */
					if (sscanf(value, "%d", &number) == 1) {
						profile->persistence = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "http-persistence must be a number\n");
					}
					OSP_DEBUG("http-persistence: '%d'", profile->persistence);
				} else if (!strcasecmp(name, "http-retry-delay")) {
					/* HTTP retry delay */
					if ((sscanf(value, "%d", &number) == 1) && (number >= OSP_MIN_RETRYDELAY) && (number <= OSP_MAX_RETRYDELAY)) {
						profile->retrydelay = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "http-retry-delay must be between %d and %d\n", OSP_MIN_RETRYDELAY, OSP_MAX_RETRYDELAY);
					}
					OSP_DEBUG("http-retry-delay: '%d'", profile->retrydelay);
				} else if (!strcasecmp(name, "http-retry-limit")) {
					/* HTTP retry limit */
					if ((sscanf(value, "%d", &number) == 1) && (number >= OSP_MIN_RETRYLIMIT) && (number <= OSP_MAX_RETRYLIMIT)) {
						profile->retrylimit = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "http-retry-limit must be between %d and %d\n", OSP_MIN_RETRYLIMIT, OSP_MAX_RETRYLIMIT);
					}
					OSP_DEBUG("http-retry-limit: '%d'", profile->retrylimit);
				} else if (!strcasecmp(name, "http-timeout")) {
					/* HTTP timeout value */
					if ((sscanf(value, "%d", &number) == 1) && (number >= OSP_MIN_TIMEOUT) && (number <= OSP_MAX_TIMEOUT)) {
						profile->timeout = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "http-timeout must be between %d and %d\n", OSP_MIN_TIMEOUT, OSP_MAX_TIMEOUT);
					}
					OSP_DEBUG("http-timeout: '%d'", profile->timeout);
				} else if (!strcasecmp(name, "work-mode")) {
					/* OSP work mode */
					if (!strcasecmp(value, "direct")) {
						profile->workmode = OSP_MODE_DIRECT;
					} else if (!strcasecmp(value, "indirect")) {
						profile->workmode = OSP_MODE_INDIRECT;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown work mode '%s'\n", value);
					}
					OSP_DEBUG("work-mode: '%d'", profile->workmode);
				} else if (!strcasecmp(name, "service-type")) {
					/* OSP service type */
					if (!strcasecmp(value, "voice")) {
						profile->srvtype = OSP_SRV_VOICE;
					} else if (!strcasecmp(value, "npquery")) {
						profile->srvtype = OSP_SRV_NPQUERY;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown service type '%s'\n", value);
					}
					OSP_DEBUG("service-type: '%d'", profile->srvtype);
				} else if (!strcasecmp(name, "max-destinations")) {
					/* Max destinations */
					if ((sscanf(value, "%d", &number) == 1) && (number >= OSP_MIN_MAXDEST) && (number <= OSP_MAX_MAXDEST)) {
						profile->maxdest = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "max-destinations must be between %d and %d\n", OSP_MIN_MAXDEST, OSP_MAX_MAXDEST);
					}
					OSP_DEBUG("max-destinations: '%d'", profile->maxdest);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown parameter '%s'\n", name);
				}
			}

			/* Check number of service porints */
			if (!profile->spnumber) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Without service point URI in profile '%s'\n", profile->name);
				/* "profile" cannot free to pool in FreeSWITCH */
				continue;
			}

			profile->next = osp_profiles;
			osp_profiles = profile;
		}
	}

	switch_xml_free(xml);

	OSP_DEBUG_END;

	return status;
}

/*
 * Init OSP client end
 * return
 */
static void osp_init_osptk(void)
{
	osp_profile_t *profile;
	OSPTPRIVATEKEY privatekey;
	unsigned char privatekeydata[OSP_SIZE_KEYSTR];
	OSPT_CERT localcert;
	unsigned char localcertdata[OSP_SIZE_KEYSTR];
	const OSPT_CERT *pcacert;
	OSPT_CERT cacert;
	unsigned char cacertdata[OSP_SIZE_KEYSTR];
	int error;

	OSP_DEBUG_START;

	/* Init OSP Toolkit */
	if (osp_global.hardware) {
		if ((error = OSPPInit(OSPC_TRUE)) != OSPC_ERR_NO_ERROR) {
			/* Unable to enable crypto hardware, disable it */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to enable crypto hardware, error '%d'\n", error);
			osp_global.hardware = SWITCH_FALSE;
			OSPPInit(OSPC_FALSE);
		}
	} else {
		OSPPInit(OSPC_FALSE);
	}

	/* Init OSP profile, using default certificates  */
	for (profile = osp_profiles; profile; profile = profile->next) {
		privatekey.PrivateKeyData = privatekeydata;
		privatekey.PrivateKeyLength = sizeof(privatekeydata);

		localcert.CertData = localcertdata;
		localcert.CertDataLength = sizeof(localcertdata);

		pcacert = &cacert;
		cacert.CertData = cacertdata;
		cacert.CertDataLength = sizeof(cacertdata);

		if ((error = OSPPBase64Decode(B64PKey, strlen(B64PKey), privatekey.PrivateKeyData, &privatekey.PrivateKeyLength)) != OSPC_ERR_NO_ERROR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to decode private key, error '%d'\n", error);
		} else if ((error = OSPPBase64Decode(B64LCert, strlen(B64LCert), localcert.CertData, &localcert.CertDataLength)) != OSPC_ERR_NO_ERROR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to decode local cert, error '%d'\n", error);
		} else if ((error = OSPPBase64Decode(B64CACert, strlen(B64CACert), cacert.CertData, &cacert.CertDataLength)) != OSPC_ERR_NO_ERROR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to decode cacert, error '%d'\n", error);
		}

		if (error == OSPC_ERR_NO_ERROR) {
			/* Create provider handle */
			error = OSPPProviderNew(
				profile->spnumber,		/* Number of service points */
				profile->spurl,			/* Service point URLs */
				NULL,					/* Weights */
				OSP_AUDIT_URL,			/* Audit URL */
				&privatekey,			/* Provate key */
				&localcert,				/* Local cert */
				1,						/* Number of cacerts */
				&pcacert,				/* cacerts */
				OSP_LOCAL_VALID,		/* Validating method */
				profile->lifetime,		/* SS lifetime */
				profile->maxconnect,	/* HTTP max connections */
				profile->persistence,	/* HTTP persistence */
				profile->retrydelay,	/* HTTP retry delay, in seconds */
				profile->retrylimit,	/* HTTP retry times */
				profile->timeout,		/* HTTP timeout */
				OSP_CUSTOMER_ID,		/* Customer ID */
				OSP_DEVICE_ID,			/* Device ID */
				&profile->provider);	/* Provider handle */
			if (error != OSPC_ERR_NO_ERROR) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to create provider for profile %s, error '%d'\n", profile->name, error);
				profile->provider = OSP_INVALID_HANDLE;
			} else {
				OSP_DEBUG("Created provider handle for profile '%s'", profile->name);
			}
		}
	}

	OSP_DEBUG_END;
}

/*
 * Cleanup OSP client end
 * return
 */
static void osp_cleanup_osptk(void)
{
	osp_profile_t *profile;

	OSP_DEBUG_START;

	for (profile = osp_profiles; profile; profile = profile->next) {
		if (profile->provider != OSP_INVALID_HANDLE) {
			/* Delete provider handle */
			OSPPProviderDelete(profile->provider, 0);
			profile->provider = OSP_INVALID_HANDLE;
			OSP_DEBUG("Deleted provider handle for profile '%s'", profile->name);
		}
	}

	/* Cleanup OSP Toolkit */
	OSPPCleanup();

	OSP_DEBUG_END;
}

/*
 * Get protocol name
 * param protocol Protocol
 * return protocol name
 */
static const char *osp_get_protocolname(
	OSPE_PROTOCOL_NAME protocol)
{
	const char *name;

	OSP_DEBUG_START;

	switch (protocol) {
	case OSPC_PROTNAME_UNKNOWN:
		name = OSP_PROTOCOL_UNKNO;
		break;
	case OSPC_PROTNAME_UNDEFINED:
		name = OSP_PROTOCOL_UNDEF;
		break;
	case OSPC_PROTNAME_SIP:
		name = OSP_PROTOCOL_SIP;
		break;
	case OSPC_PROTNAME_Q931:
		name = OSP_PROTOCOL_H323;
		break;
	case OSPC_PROTNAME_IAX:
		name = OSP_PROTOCOL_IAX;
		break;
	case OSPC_PROTNAME_SKYPE:
		name = OSP_PROTOCOL_SKYPE;
		break;
	case OSPC_PROTNAME_LRQ:
	case OSPC_PROTNAME_T37:
	case OSPC_PROTNAME_T38:
	case OSPC_PROTNAME_SMPP:
	case OSPC_PROTNAME_XMPP:
	case OSPC_PROTNAME_SMS:
	default:
		name = OSP_PROTOCOL_UNSUP;
		break;
	}
	OSP_DEBUG("Protocol %d: '%s'", protocol, name);

	OSP_DEBUG_END;

	return name;
}

/*
 * Get protocol from module name
 * param module Endpoint module name
 * return protocol Endpoint protocol name
 */
static OSPE_PROTOCOL_NAME osp_get_protocol(
	const char *module)
{
	OSPE_PROTOCOL_NAME protocol;

	OSP_DEBUG_START;

	if (!strcasecmp(module, OSP_MODULE_SIP)) {
		protocol = OSPC_PROTNAME_SIP;
	} else if (!strcasecmp(module, OSP_MODULE_H323)) {
		protocol = OSPC_PROTNAME_Q931;
	} else if (!strcasecmp(module, OSP_MODULE_IAX)) {
		protocol = OSPC_PROTNAME_IAX;
	} else if (!strcasecmp(module, OSP_MODULE_SKYPE)) {
		protocol = OSPC_PROTNAME_SKYPE;
	} else {
		protocol = OSPC_PROTNAME_UNKNOWN;
	}
	OSP_DEBUG("Module %s: '%d'", module, protocol);

	OSP_DEBUG_END;

	return protocol;
}

/*
 * Parse userinfo for user and LNP
 * param userinfo SIP URI userinfo
 * param user User part
 * param usersize Size of user buffer
 * param rn Routing number
 * param rnsize Size of rn buffer
 * param cic Carrier Identification Cod
 * param cicsize Size of cic buffer
 * param npdi NP Database Dip Indicator
 * return
 */
static void osp_parse_userinfo(
	const char *userinfo,
	char *user,
	switch_size_t usersize,
	char *rn,
	switch_size_t rnsize,
	char *cic,
	switch_size_t cicsize,
	int *npdi)
{
	char buffer[OSP_SIZE_NORSTR];
	char *item;
	char *tmp;

	OSP_DEBUG_START;

	/* Set default values */
	if (user && usersize) {
		user[0] = '\0';
	}
	if (rn && rnsize) {
		rn[0] = '\0';
	}
	if (cic && cicsize) {
		cic[0] = '\0';
	}
	if (npdi) {
		*npdi = 0;
	}

	/* Parse userinfo */
	if (!switch_strlen_zero(userinfo)) {
		/* Copy userinfo to temporary buffer */
		switch_copy_string(buffer, userinfo, sizeof(buffer));

		/* Parse user */
		item = strtok_r(buffer, OSP_USER_DELIM, &tmp);
		if (user && usersize) {
			switch_copy_string(user, item, usersize);
		}

		/* Parse LNP parameters */
		for (item = strtok_r(NULL, OSP_USER_DELIM, &tmp); item; item = strtok_r(NULL, OSP_USER_DELIM, &tmp)) {
			if (!strncasecmp(item, "rn=", 3)) {
				/* Parsed routing number */
				if (rn && rnsize) {
					switch_copy_string(rn, item + 3, rnsize);
				}
			} else if (!strncasecmp(item, "cic=", 4)) {
				/* Parsed cic */
				if (cic && cicsize) {
					switch_copy_string(cic, item + 4, cicsize);
				}
			} else if (!strcasecmp(item, "npdi")) {
				/* Parsed npdi */
				if (npdi) {
					*npdi = 1;
				}
			}
		}
	}

	OSP_DEBUG("user: '%s'", OSP_FILTER_NULLSTR(user));
	OSP_DEBUG("rn: '%s'", OSP_FILTER_NULLSTR(rn));
	OSP_DEBUG("cic: '%s'", OSP_FILTER_NULLSTR(cic));
	OSP_DEBUG("npdi: '%d'", OSP_FILTER_NULLINT(npdi));

	OSP_DEBUG_END;
}

/*
 * Parse SIP header user
 * param header SIP header
 * param user SIP header user
 * param usersize Size of user buffer
 * return
 */
static void osp_parse_header_user(
	const char *header,
	char *user,
	switch_size_t usersize)
{
	char buffer[OSP_SIZE_NORSTR];
	char *head;
	char *tmp;
	char *item;

	OSP_DEBUG_START;

	if (user && usersize) {
		user[0] = '\0';

		/* Parse user */
		if (!switch_strlen_zero(header) && user && usersize) {
			/* Copy header to temporary buffer */
			switch_copy_string(buffer, header, sizeof(buffer));

			if ((head = strstr(buffer, "sip:"))) {
				head += 4;
				if ((tmp = strchr(head, OSP_URI_DELIM))) {
					*tmp = '\0';
					item = strtok_r(head, OSP_USER_DELIM, &tmp);
					switch_copy_string(user, item, usersize);
				}
			}
		}

		OSP_DEBUG("user: '%s'", user);
	}

	OSP_DEBUG_END;
}

/*
 * Parse SIP header host
 * param header SIP header
 * param host SIP header host
 * param hostsize Size of host buffer
 * return
 */
static void osp_parse_header_host(
	const char *header,
	char *host,
	switch_size_t hostsize)
{
	char buffer[OSP_SIZE_NORSTR];
	char *head;
	char *tmp;

	OSP_DEBUG_START;

	if (hostsize) {
		host[0] = '\0';

		/* Parse host*/
		if (!switch_strlen_zero(header) && host && hostsize) {
			/* Copy header to temporary buffer */
			switch_copy_string(buffer, header, sizeof(buffer));

			if ((head = strstr(buffer, "sip:"))) {
				head += 4;
				if ((tmp = strchr(head, OSP_URI_DELIM))) {
					head = tmp + 1;
				}
				tmp = strtok(head, OSP_HOST_DELIM);
				switch_copy_string(host, tmp, hostsize);
			}
		}

		OSP_DEBUG("host: '%s'", host);
	}

	OSP_DEBUG_END;
}

/*
 * Convert "address:port" to "[x.x.x.x]:port" or "hostname:port" format
 * param src Source address string
 * param dest Destination address string
 * param destsize Size of dest buffer
 * return
 */
static void osp_convert_inout(
	const char *src,
	char *dest,
	int destsize)
{
	struct in_addr inp;
	char buffer[OSP_SIZE_NORSTR];
	char *port;

	OSP_DEBUG_START;

	if (dest && destsize) {
		dest[0] = '\0';

		if (!switch_strlen_zero(src)) {
			/* Copy IP address to temporary buffer */
			switch_copy_string(buffer, src, sizeof(buffer));

			if((port = strchr(buffer, ':'))) {
				*port = '\0';
				port++;
			}

			if (inet_pton(AF_INET, buffer, &inp) == 1) {
				if (port) {
					switch_snprintf(dest, destsize, "[%s]:%s", buffer, port);
				} else {
					switch_snprintf(dest, destsize, "[%s]", buffer);
				}
				dest[destsize - 1] = '\0';
			} else {
				switch_copy_string(dest, src, destsize);
			}
		}

		OSP_DEBUG("out: '%s'", dest);
	}

	OSP_DEBUG_END;
}

/*
 * Convert "[x.x.x.x]:port" or "hostname:prot" to "address:port" format
 * param src Source address string
 * param dest Destination address string
 * param destsize Size of dest buffer
 * return
 */
static void osp_convert_outin(
	const char *src,
	char *dest,
	int destsize)
{
	char buffer[OSP_SIZE_NORSTR];
	char *end;
	char *port;

	OSP_DEBUG_START;

	if (dest && destsize) {
		dest[0] = '\0';

		if (!switch_strlen_zero(src)) {
			switch_copy_string(buffer, src, sizeof(buffer));

			if (buffer[0] == '[') {
				if((port = strchr(buffer + 1, ':'))) {
					*port = '\0';
					port++;
				}

				if ((end = strchr(buffer + 1, ']'))) {
					*end = '\0';
				}

				if (port) {
					switch_snprintf(dest, destsize, "%s:%s", buffer + 1, port);
					dest[destsize - 1] = '\0';
				} else {
					switch_copy_string(dest, buffer + 1, destsize);
				}
			} else {
				switch_copy_string(dest, src, destsize);
			}
		}
		OSP_DEBUG("in: '%s'", dest);
	}

	OSP_DEBUG_END;
}

/*
 * Always log AuthReq parameters
 * param profile OSP profile
 * param inbound Inbound info
 * return
 */
static void osp_log_authreq(
	osp_profile_t *profile,
	osp_inbound_t *inbound)
{
	char *srvtype;
	const char *source;
	const char *srcdev;
	char term[OSP_SIZE_NORSTR];
	int total;

	OSP_DEBUG_START;

	/* Get source device and source */
	if (profile->workmode == OSP_MODE_INDIRECT) {
		source = inbound->srcdev;
		if (switch_strlen_zero(inbound->actsrc)) {
			srcdev = inbound->srcdev;
		} else {
			srcdev = inbound->actsrc;
		}
	} else {
		source = profile->deviceip;
		srcdev = inbound->srcdev;
	}

	/* Get preferred destination for NP query */
	if (profile->srvtype == OSP_SRV_NPQUERY) {
		srvtype = "npquery";
		if (switch_strlen_zero(inbound->tohost)) {
			switch_copy_string(term, source, sizeof(term));
		} else {
			if (switch_strlen_zero(inbound->toport)) {
				switch_copy_string(term, inbound->tohost, sizeof(term));
			} else {
				switch_snprintf(term, sizeof(term), "%s:%s", inbound->tohost, inbound->toport);
			}
		}
		total = 1;
	} else {
		srvtype = "voice";
		term[0] = '\0';
		total = profile->maxdest;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, osp_global.loglevel,
		"AuthReq: "
		"srvtype '%s' "
		"source '%s' "
		"srcdev '%s' "
		"srcnid '%s' "
		"protocol '%s' "
		"callid '%s' "
		"calling '%s' "
		"called '%s' "
		"lnp '%s/%s/%d' "
		"preferred '%s' "
		"rpid '%s' "
		"pai '%s' "
		"div '%s/%s' "
		"pci '%s' "
		"cinfo '%s/%s/%s/%s/%s/%s/%s/%s' "
		"maxcount '%d'\n",
		srvtype,
		source,
		srcdev,
		OSP_FILTER_NULLSTR(inbound->srcnid),
		osp_get_protocolname(inbound->protocol),
		OSP_FILTER_NULLSTR(inbound->callid),
		inbound->calling,
		inbound->called,
		inbound->nprn, inbound->npcic, inbound->npdi,
		term,
		inbound->rpiduser,
		inbound->paiuser,
		inbound->divuser, inbound->divhost,
		inbound->pciuser,
		OSP_FILTER_NULLSTR(inbound->cinfo[0]), OSP_FILTER_NULLSTR(inbound->cinfo[1]), OSP_FILTER_NULLSTR(inbound->cinfo[2]), OSP_FILTER_NULLSTR(inbound->cinfo[3]),
		OSP_FILTER_NULLSTR(inbound->cinfo[4]), OSP_FILTER_NULLSTR(inbound->cinfo[5]), OSP_FILTER_NULLSTR(inbound->cinfo[6]), OSP_FILTER_NULLSTR(inbound->cinfo[7]),
		total);

	OSP_DEBUG_END;
}

/*
 * Always log AuthRsp parameters
 * param results Routing info
 * return
 */
static void osp_log_authrsp(
	osp_results_t *results)
{
	OSP_DEBUG_START;

	switch_log_printf(SWITCH_CHANNEL_LOG, osp_global.loglevel,
		"AuthRsp: "
		"destcount '%d/%d' "
		"transid '%"PRIu64"' "
		"timelimit '%u' "
		"calling '%s' "
		"called '%s' "
		"destination '%s' "
		"destnid '%s' "
		"lnp '%s/%s/%d' "
		"protocol '%s' "
		"supported '%d'\n",
		results->count,
		results->total,
		results->transid,
		results->timelimit,
		results->calling,
		results->called,
		results->dest,
		results->destnid,
		results->nprn, results->npcic, results->npdi,
		osp_get_protocolname(results->protocol),
		results->supported);

	OSP_DEBUG_END;
}

/*
 * Alway log UsageInd parameters
 * param results Route info
 * param usage Usage info
 * return
 */
static void osp_log_usageind(
	osp_results_t *results,
	osp_usage_t *usage)
{
	OSP_DEBUG_START;

	switch_log_printf(SWITCH_CHANNEL_LOG, osp_global.loglevel,
		"UsageInd: "
		"destcount '%d/%d' "
		"transid '%"PRIu64"' "
		"cause '%d' "
		"release '%d' "
		"times '%"PRId64"/%"PRId64"/%"PRId64"/%"PRId64"' "
		"duration '%"PRId64"' "
		"pdd '%"PRId64"' "
		"codec '%s/%s' "
		"rtpctets '%d/%d' "
		"rtppackets '%d/%d'\n",
		results->count, results->total,
		results->transid,
		results->cause ? results->cause : usage->cause,
		usage->release,
		usage->start / 1000000, usage->alert / 1000000, usage->connect / 1000000, usage->end / 1000000,
		usage->duration / 1000000,
		usage->pdd / 1000,
		OSP_FILTER_NULLSTR(usage->srccodec), OSP_FILTER_NULLSTR(usage->destcodec),
		usage->rtpsrcrepoctets, usage->rtpdestrepoctets,
		usage->rtpsrcreppackets, usage->rtpdestreppackets);

	OSP_DEBUG_END;
}

/*
 * Get inbound info
 * param channel Inbound channel
 * param inbound Inbound info
 * return
 */
static void osp_get_inbound(
	switch_channel_t *channel,
	osp_inbound_t *inbound)
{
	switch_caller_profile_t *caller;
	const char *tmp;
	int i;
	char name[OSP_SIZE_NORSTR];

	OSP_DEBUG_START;

	/* Cleanup buffer */
	memset(inbound, 0, sizeof(*inbound));

	/* Get caller profile */
	caller = switch_channel_get_caller_profile(channel);

	/* osp_source_device */
	inbound->actsrc = switch_channel_get_variable(channel, OSP_VAR_SRCDEV);
	OSP_DEBUG("actsrc: '%s'", OSP_FILTER_NULLSTR(inbound->actsrc));

	/* Source device */
	inbound->srcdev = caller->network_addr;
	OSP_DEBUG("srcdev: '%s'", OSP_FILTER_NULLSTR(inbound->srcdev));

	/* osp_source_nid */
	inbound->srcnid = switch_channel_get_variable(channel, OSP_VAR_SRCNID);
	OSP_DEBUG("srcnid: '%s'", OSP_FILTER_NULLSTR(inbound->srcnid));

	/* Source signaling protocol */
	inbound->protocol = osp_get_protocol(caller->source);
	OSP_DEBUG("protocol: '%d'", inbound->protocol);

	/* Call-ID */
	inbound->callid = switch_channel_get_variable(channel, OSP_FS_CALLID);
	OSP_DEBUG("callid: '%s'", OSP_FILTER_NULLSTR(inbound->callid));

	/* Calling number */
	if ((tmp = switch_channel_get_variable(channel, OSP_FS_FROMUSER))) {
		osp_parse_userinfo(tmp, inbound->calling, sizeof(inbound->calling), NULL, 0, NULL, 0, NULL);
	} else {
		osp_parse_userinfo(caller->caller_id_number, inbound->calling, sizeof(inbound->calling), NULL, 0, NULL, 0, NULL);
	}
	OSP_DEBUG("calling: '%s'", inbound->calling);

	/* Called number and LNP parameters */
	osp_parse_userinfo(caller->destination_number, inbound->called, sizeof(inbound->called), inbound->nprn, sizeof(inbound->nprn), inbound->npcic, sizeof(inbound->npcic),
		&inbound->npdi);
	OSP_DEBUG("called: '%s'", inbound->called);
	OSP_DEBUG("nprn: '%s'", inbound->nprn);
	OSP_DEBUG("npcic: '%s'", inbound->npcic);
	OSP_DEBUG("npdi: '%d'", inbound->npdi);

	/* To header */
	inbound->tohost = switch_channel_get_variable(channel, OSP_FS_TOHOST);
	OSP_DEBUG("tohost: '%s'", OSP_FILTER_NULLSTR(inbound->tohost));
	inbound->toport = switch_channel_get_variable(channel, OSP_FS_TOPORT);
	OSP_DEBUG("toport: '%s'", OSP_FILTER_NULLSTR(inbound->toport));

	/* RPID calling number */
	if ((tmp = switch_channel_get_variable(channel, OSP_FS_RPID))) {
		osp_parse_header_user(tmp, inbound->rpiduser, sizeof(inbound->rpiduser));
	}
	OSP_DEBUG("RPID user: '%s'", inbound->rpiduser);

	/* PAI calling number */
	if ((tmp = switch_channel_get_variable(channel, OSP_FS_PAI))) {
		osp_parse_userinfo(tmp, inbound->paiuser, sizeof(inbound->paiuser), NULL, 0, NULL, 0, NULL);
	}
	OSP_DEBUG("PAI user: '%s'", inbound->paiuser);

	/* DIV calling number */
	if ((tmp = switch_channel_get_variable(channel, OSP_FS_DIV))) {
		osp_parse_header_user(tmp, inbound->divuser, sizeof(inbound->divuser));
		osp_parse_header_host(tmp, inbound->divhost, sizeof(inbound->divhost));
	}
	OSP_DEBUG("DIV user: '%s'", inbound->divuser);
	OSP_DEBUG("DIV host: '%s'", inbound->divhost);

	/* PCI calling number */
	if ((tmp = switch_channel_get_variable(channel, OSP_FS_PCI))) {
		osp_parse_header_user(tmp, inbound->pciuser, sizeof(inbound->pciuser));
	}
	OSP_DEBUG("PCI user: '%s'", inbound->pciuser);

	/* Custom info */
	for (i = 0; i < OSP_MAX_CINFO; i++) {
		switch_snprintf(name, sizeof(name), "%s%d", OSP_VAR_CUSTOMINFO, i + 1);
		inbound->cinfo[i] = switch_channel_get_variable(channel, name);
		OSP_DEBUG("cinfo[%d]: '%s'", i, OSP_FILTER_NULLSTR(inbound->cinfo[i]));
	}

	OSP_DEBUG_END;
}

/*
 * Get outbound settings
 * param channel Inbound channel
 * param outbound Outbound settings
 * return
 */
static void osp_get_outbound(
	switch_channel_t *channel,
	osp_outbound_t *outbound)
{
	const char *tmp;

	OSP_DEBUG_START;

	/* Cleanup buffer */
	memset(outbound, 0, sizeof(*outbound));

	/* Get destination network ID namd & location info */
	outbound->dniduserparam = switch_channel_get_variable(channel, OSP_VAR_DNIDUSERPARAM);
	OSP_DEBUG("dniduserparam: '%s'", OSP_FILTER_NULLSTR(outbound->dniduserparam));
	outbound->dniduriparam = switch_channel_get_variable(channel, OSP_VAR_DNIDURIPARAM);
	OSP_DEBUG("dniduriparam: '%s'", OSP_FILTER_NULLSTR(outbound->dniduriparam));

	/* Get "user=phone" insert flag */
	tmp = switch_channel_get_variable(channel, OSP_VAR_USERPHONE);
	if (!switch_strlen_zero(tmp)) {
		outbound->userphone = switch_true(tmp);
	}
	OSP_DEBUG("userphone: '%d'", outbound->userphone);

	/* Get outbound proxy info */
	outbound->outproxy = switch_channel_get_variable(channel, OSP_VAR_OUTPROXY);
	OSP_DEBUG("outporxy: '%s'", OSP_FILTER_NULLSTR(outbound->outproxy));

	OSP_DEBUG_END;
}

/*
 * Get transaction info
 * param channel Inbound channel
 * param aleg If A-leg
 * param results Route info
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_get_transaction(
	switch_channel_t *channel,
	switch_bool_t aleg,
	osp_results_t *results)
{
	const char *tmp;
	osp_profile_t *profile;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	/* Get profile name */
	if (!(results->profile = switch_channel_get_variable(channel, OSP_VAR_PROFILE))) {
		results->profile = OSP_DEF_PROFILE;
	}

	/* Get transaction handle */
	if (osp_find_profile(results->profile, &profile) == SWITCH_STATUS_FALSE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find profile '%s'\n", results->profile);
	} else if (profile->provider == OSP_INVALID_HANDLE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Disabled profile '%s'\n", results->profile);
	} else if (!(tmp = switch_channel_get_variable(channel, OSP_VAR_TRANSACTION)) || (sscanf(tmp, "%d", &results->transaction) != 1)){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get transaction handle'\n");
	} else if (results->transaction == OSP_INVALID_HANDLE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid transaction handle'\n");
	} else {
		if (!(tmp = switch_channel_get_variable(channel, OSP_VAR_TRANSID)) || (sscanf(tmp, "%"PRIu64"", &results->transid) != 1)) {
			results->transid = 0;
		}

		/* Get destination total */
		if (!(tmp = switch_channel_get_variable(channel, OSP_VAR_ROUTETOTAL)) || (sscanf(tmp, "%u", &results->total) != 1)) {
			results->total = 0;
		}

		/* Get destination count */
		if (!(tmp = switch_channel_get_variable(channel, OSP_VAR_ROUTECOUNT)) || (sscanf(tmp, "%u", &results->count) != 1)) {
			results->count = 0;
		}

		/*
		 * Get termiantion cause
		 * The logic is
		 * 1. osp_next_function should get TCCode from last_bridge_hangup_cause
		 * 2. osp_on_reporting should get TCCode from osp_terimation_cause
		 */
		if (aleg) {
			/* A-leg */
			if (!(tmp = switch_channel_get_variable(channel, OSP_VAR_TCCODE)) || (sscanf(tmp, "%u", &results->cause) != 1)) {
				results->cause = 0;
			}
		} else {
			/* B-leg */
			if ((tmp = switch_channel_get_variable(channel, OSP_FS_HANGUPCAUSE))) {
				results->cause = switch_channel_str2cause(tmp);
			}
		}

		status = SWITCH_STATUS_SUCCESS;
	}
	OSP_DEBUG("profile: '%s'", results->profile);
	OSP_DEBUG("transaction: '%d'", results->transaction);
	OSP_DEBUG("transid: '%"PRIu64"'", results->transid);
	OSP_DEBUG("total: '%d'", results->total);
	OSP_DEBUG("count: '%d'", results->count);
	OSP_DEBUG("cause: '%d'", results->cause);

	OSP_DEBUG_END;

	return status;
}

/*
 * Retrieve usage info
 * param channel channel
 * param originator Originator profile
 * param terminator Terminator profile, not used at this time
 * param results Route info
 * param usage Usage info
 * return
 */
static void osp_get_usage(
	switch_channel_t *channel,
	switch_caller_profile_t *originator,
	switch_caller_profile_t *terminator_unused,
	osp_results_t *results,
	osp_usage_t *usage)
{
	const char *tmp;
	switch_channel_timetable_t *times;

	OSP_DEBUG_START;

	/* Cleanup buffer */
	memset(usage, 0, sizeof(*usage));

	/* Release source */
	usage->release = OSPC_RELEASE_UNKNOWN;
	if (osp_get_protocol(originator->source) == OSPC_PROTNAME_SIP) {
		tmp = switch_channel_get_variable(channel, OSP_FS_SIPRELEASE);
		if (!tmp) {
			usage->release = OSPC_RELEASE_UNDEFINED;
		} else if (!strcasecmp(tmp, "send_bye")) {
			usage->release = OSPC_RELEASE_DESTINATION;
		} else if (!strcasecmp(tmp, "recv_bye")) {
			usage->release = OSPC_RELEASE_SOURCE;
		} else if (!strcasecmp(tmp, "send_refuse")) {
			usage->release = OSPC_RELEASE_INTERNAL;
		} else if (!strcasecmp(tmp, "send_cancel")) {
			usage->release = OSPC_RELEASE_SOURCE;
		}
	}
	OSP_DEBUG("release: '%d'", usage->release);

	/* Termiation cause */
	usage->cause = switch_channel_get_cause_q850(channel);
	OSP_DEBUG("cause: '%d'", usage->cause);

	/* Timestamps */
	times = switch_channel_get_timetable(channel);
	usage->start = times->created;
	OSP_DEBUG("start: '%"PRIu64"'", usage->start);
	usage->alert = times->progress;
	OSP_DEBUG("alert: '%"PRIu64"'", usage->alert);
	usage->connect = times->answered;
	OSP_DEBUG("connect: '%"PRIu64"'", usage->connect);
	usage->end = times->hungup;
	OSP_DEBUG("end: '%"PRIu64"'", usage->end);
	if (times->answered) {
		usage->duration = times->hungup - times->answered;
		OSP_DEBUG("duration: '%"PRIu64"'", usage->duration);
	}
	if (times->progress) {
		usage->pdd = times->progress - usage->start;
		OSP_DEBUG("pdd: '%"PRIu64"'", usage->pdd);
	}

	/* Codecs */
	usage->srccodec = switch_channel_get_variable(channel, OSP_FS_SRCCODEC);
	OSP_DEBUG("srccodec: '%s'", OSP_FILTER_NULLSTR(usage->srccodec));
	usage->destcodec = switch_channel_get_variable(channel, OSP_FS_DESTCODEC);
	OSP_DEBUG("destcodec: '%s'", OSP_FILTER_NULLSTR(usage->destcodec));

	/* QoS statistics */
	if (!(tmp = switch_channel_get_variable(channel, OSP_FS_RTPSRCREPOCTS)) || (sscanf(tmp, "%d", &usage->rtpsrcrepoctets) != 1)) {
		usage->rtpsrcrepoctets = OSP_DEF_STATS;
	}
	OSP_DEBUG("rtpsrcrepoctets: '%d'", usage->rtpsrcrepoctets);
	if (!(tmp = switch_channel_get_variable(channel, OSP_FS_RTPDESTREPOCTS)) || (sscanf(tmp, "%d", &usage->rtpdestrepoctets) != 1)) {
		usage->rtpdestrepoctets = OSP_DEF_STATS;
	}
	OSP_DEBUG("rtpdestrepoctets: '%d'", usage->rtpdestrepoctets);
	if (!(tmp = switch_channel_get_variable(channel, OSP_FS_RTPSRCREPPKTS)) || (sscanf(tmp, "%d", &usage->rtpsrcreppackets) != 1)) {
		usage->rtpsrcreppackets = OSP_DEF_STATS;
	}
	OSP_DEBUG("rtpsrcreppackets: '%d'", usage->rtpsrcreppackets);
	if (!(tmp = switch_channel_get_variable(channel, OSP_FS_RTPDESTREPPKTS)) || (sscanf(tmp, "%d", &usage->rtpdestreppackets) != 1)) {
		usage->rtpdestreppackets = OSP_DEF_STATS;
	}
	OSP_DEBUG("rtpdestreppackets: '%d'", usage->rtpdestreppackets);

	OSP_DEBUG_END;
}

/*
 * Check destination
 * param results Routing info
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_check_destination(
	osp_results_t *results)
{
	OSPE_DEST_OSPENABLED enabled;
	OSPE_PROTOCOL_NAME protocol;
	OSPE_OPERATOR_NAME type;
	int error;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	if ((error = OSPPTransactionIsDestOSPEnabled(results->transaction, &enabled)) != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get destination OSP version, error '%d'\n", error);
	} else if ((error = OSPPTransactionGetDestProtocol(results->transaction, &protocol)) != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get signaling protocol, error '%d'\n", error);
	} else {
		switch(protocol) {
		case OSPC_PROTNAME_UNDEFINED:
		case OSPC_PROTNAME_UNKNOWN:
			protocol = osp_global.protocol;
		case OSPC_PROTNAME_SIP:
		case OSPC_PROTNAME_Q931:
		case OSPC_PROTNAME_IAX:
		case OSPC_PROTNAME_SKYPE:
			results->protocol = protocol;
			if (!switch_strlen_zero(osp_global.endpoint[protocol].module) && !switch_strlen_zero(osp_global.endpoint[protocol].profile)) {
				results->supported = SWITCH_TRUE;
				results->cause = 0;
				status = SWITCH_STATUS_SUCCESS;
				break;
			}
		case OSPC_PROTNAME_LRQ:
		case OSPC_PROTNAME_T37:
		case OSPC_PROTNAME_T38:
		case OSPC_PROTNAME_SMPP:
		case OSPC_PROTNAME_XMPP:
		default:
			results->protocol = protocol;
			results->supported = SWITCH_FALSE;
			/* Q.850 protocol error, unspecified */
			results->cause = 111;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported protocol '%d'\n", protocol);
			break;
		}
		OSP_DEBUG("protocol: '%d'", results->protocol);
		OSP_DEBUG("supported: '%d'", results->supported);
		OSP_DEBUG("cause: '%d'", results->cause);

		if ((error = OSPPTransactionGetDestinationNetworkId(results->transaction, sizeof(results->destnid), results->destnid)) != OSPC_ERR_NO_ERROR) {
			results->destnid[0] = '\0';
		}
		OSP_DEBUG("destnid: '%s'", results->destnid);

		error = OSPPTransactionGetNumberPortabilityParameters(
			results->transaction,
			sizeof(results->nprn),
			results->nprn,
			sizeof(results->npcic),
			results->npcic,
			&results->npdi);
		if (error != OSPC_ERR_NO_ERROR) {
			results->nprn[0] = '\0';
			results->npcic[0] = '\0';
			results->npdi = 0;
		}
		OSP_DEBUG("nprn: '%s'", results->nprn);
		OSP_DEBUG("npcic: '%s'", results->npcic);
		OSP_DEBUG("npdi: '%d'", results->npdi);

		for (type = OSPC_OPNAME_START; type < OSPC_OPNAME_NUMBER; type++) {
			if ((error = OSPPTransactionGetOperatorName(results->transaction, type, sizeof(results->opname[type]), results->opname[type])) != OSPC_ERR_NO_ERROR) {
				results->opname[type][0] = '\0';
			}
			OSP_DEBUG("opname[%d]: '%s'", type, results->opname[type]);
		}

		osp_log_authrsp(results);
	}

	OSP_DEBUG_END;

	return status;
}

/*
 * Build parameter string for each channel
 * param results Route info
 * param buffer Buffer
 * param bufsize Buffer size
 * return
 */
static void osp_build_eachparam(
	osp_results_t *results,
	char *buffer,
	switch_size_t bufsize)
{
	OSP_DEBUG_START;

	if (results && buffer && bufsize) {
		switch_snprintf(buffer, bufsize, "[%s=%s]", OSP_FS_OUTCALLING, results->calling);
		OSP_DEBUG("eachparam: '%s'", buffer);
	}

	OSP_DEBUG_END;
}

/*
 * Build endpoint string
 * param results Route info
 * param outbound Outbound settings
 * param buffer Buffer
 * param bufsize Buffer size
 * return
 */
static void osp_build_endpoint(
	osp_results_t *results,
	osp_outbound_t *outbound,
	char *buffer,
	switch_size_t bufsize)
{
	char *head = buffer;
	switch_size_t len, size = bufsize;

	OSP_DEBUG_START;

	if (results && buffer && bufsize) {
		switch (results->protocol) {
		case OSPC_PROTNAME_SIP:
			/* module/profile/called */
			switch_snprintf(head, size, "%s/%s/%s", osp_global.endpoint[OSPC_PROTNAME_SIP].module, osp_global.endpoint[OSPC_PROTNAME_SIP].profile, results->called);
			OSP_ADJUST_LEN(head, size, len);

			/* RN */
			if (!switch_strlen_zero_buf(results->nprn)) {
				switch_snprintf(head, size, ";rn=%s", results->nprn);
				OSP_ADJUST_LEN(head, size, len);
			}

			/* CIC */
			if (!switch_strlen_zero_buf(results->npcic)) {
				switch_snprintf(head, size, ";cic=%s", results->npcic);
				OSP_ADJUST_LEN(head, size, len);
			}

			/* NPDI */
			if (results->npdi) {
				switch_snprintf(head, size, ";npdi");
				OSP_ADJUST_LEN(head, size, len);
			}

			/* User parameter destination network ID */
			if (!switch_strlen_zero(outbound->dniduserparam) && !switch_strlen_zero_buf(results->destnid)) {
				switch_snprintf(head, size, ";%s=%s", outbound->dniduserparam, results->destnid);
				OSP_ADJUST_LEN(head, size, len);
			}

			/* Destination */
			switch_snprintf(head, size, "@%s", results->dest);
			OSP_ADJUST_LEN(head, size, len);

			/* URI parameter destination network ID */
			if (!switch_strlen_zero(outbound->dniduriparam) && !switch_strlen_zero_buf(results->destnid)) {
				switch_snprintf(head, size, ";%s=%s", outbound->dniduriparam, results->destnid);
				OSP_ADJUST_LEN(head, size, len);
			}

			/* user=phone */
			if (outbound->userphone) {
				switch_snprintf(head, size, ";user=phone");
				OSP_ADJUST_LEN(head, size, len);
			}

			/* Outbound proxy */
			if (!switch_strlen_zero(outbound->outproxy)) {
				switch_snprintf(head, size, ";fs_path=sip:%s", outbound->outproxy);
				OSP_ADJUST_LEN(head, size, len);
			}
			break;
		case OSPC_PROTNAME_Q931:
			/* module/profile/called@destination */
			switch_snprintf(head, size, "%s/%s/%s@%s", osp_global.endpoint[OSPC_PROTNAME_Q931].module, osp_global.endpoint[OSPC_PROTNAME_Q931].profile,
				results->called, results->dest);
			OSP_ADJUST_LEN(head, size, len);
			break;
		case OSPC_PROTNAME_IAX:
			/* module/profile/destination/called */
			switch_snprintf(head, size, "%s/%s/%s/%s", osp_global.endpoint[OSPC_PROTNAME_Q931].module, osp_global.endpoint[OSPC_PROTNAME_Q931].profile,
				results->dest, results->called);
			OSP_ADJUST_LEN(head, size, len);
			break;
		case OSPC_PROTNAME_SKYPE:
			/* module/profile/called */
			switch_snprintf(head, size, "%s/%s/%s", osp_global.endpoint[OSPC_PROTNAME_Q931].module, osp_global.endpoint[OSPC_PROTNAME_Q931].profile, results->called);
			OSP_ADJUST_LEN(head, size, len);
			break;
		default:
			buffer[0] = '\0';
			break;
		}
		OSP_DEBUG("endpoint: '%s'", buffer);
	}

	OSP_DEBUG_END;
}

/*
 * Create route string
 * param outbound Outbound info
 * param results Routing info
 * param buffer Buffer
 * param bufsize Buffer size
 * return
 */
static void osp_create_route(
	osp_outbound_t *outbound,
	osp_results_t *results,
	char *buffer,
	switch_size_t bufsize)
{
	char eachparam[OSP_SIZE_NORSTR];
	char endpoint[OSP_SIZE_NORSTR];

	OSP_DEBUG_START;

	/* Build dial string for each channel part */
	osp_build_eachparam(results, eachparam, sizeof(eachparam));

	/* Build dial string for endpoint part */
	osp_build_endpoint(results, outbound, endpoint, sizeof(endpoint));

	/* Build dail string */
	switch_snprintf(buffer, bufsize, "%s%s", eachparam, endpoint);
	OSP_DEBUG("route: '%s'", buffer);

	OSP_DEBUG_END;
}

/*
 * Do AuthReq
 * param profile OSP profile
 * param inbound Call originator info
 * param results Routing info
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_request_auth(
	osp_profile_t *profile,
	osp_inbound_t *inbound,
	osp_results_t *results)
{
	const char *source;
	const char *srcdev;
	char tmp[OSP_SIZE_NORSTR];
	char src[OSP_SIZE_NORSTR];
	char dev[OSP_SIZE_NORSTR];
	char term[OSP_SIZE_NORSTR];
	const char *preferred[2] = { NULL };
	OSPTTRANS *context;
	int i, error;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	/* Set source network ID */
	OSPPTransactionSetNetworkIds(results->transaction, inbound->srcnid, NULL);

	/* Set source signaling protocol */
	OSPPTransactionSetProtocol(results->transaction, OSPC_PROTTYPE_SOURCE, inbound->protocol);

	/* Set source LNP parameters */
	OSPPTransactionSetNumberPortability(results->transaction, inbound->nprn, inbound->npcic, inbound->npdi);

	/* Set RPID */
	OSPPTransactionSetRemotePartyId(results->transaction, OSPC_NFORMAT_E164, inbound->rpiduser);

	/* Set PAI */
	OSPPTransactionSetAssertedId(results->transaction, OSPC_NFORMAT_E164, inbound->paiuser);

	/* Set diversion */
	osp_convert_inout(inbound->divhost, tmp, sizeof(tmp));
	OSPPTransactionSetDiversion(results->transaction, inbound->divuser, tmp);

	/* Set PCI */
	OSPPTransactionSetChargeInfo(results->transaction, OSPC_NFORMAT_E164, inbound->pciuser);

	/* Set custom info */
	for (i = 0; i < OSP_MAX_CINFO; i++) {
		if (!switch_strlen_zero(inbound->cinfo[i])) {
			OSPPTransactionSetCustomInfo(results->transaction, i, inbound->cinfo[i]);
		}
	}

	/* Device info and source */
	if (profile->workmode == OSP_MODE_INDIRECT) {
		source = inbound->srcdev;
		if (switch_strlen_zero(inbound->actsrc)) {
			srcdev = inbound->srcdev;
		} else {
			srcdev = inbound->actsrc;
		}
	} else {
		source = profile->deviceip;
		srcdev = inbound->srcdev;
	}
	osp_convert_inout(source, src, sizeof(src));
	osp_convert_inout(srcdev, dev, sizeof(dev));

	/* Preferred and max destinations */
	if (profile->srvtype == OSP_SRV_NPQUERY) {
		OSPPTransactionSetServiceType(results->transaction, OSPC_SERVICE_NPQUERY);

		if (switch_strlen_zero(inbound->tohost)) {
			switch_copy_string(term, src, sizeof(term));
		} else {
			if (switch_strlen_zero(inbound->toport)) {
				switch_copy_string(tmp, inbound->tohost, sizeof(tmp));
			} else {
				switch_snprintf(tmp, sizeof(tmp), "%s:%s", inbound->tohost, inbound->toport);
			}
			osp_convert_inout(tmp, term, sizeof(term));
		}
		preferred[0] = term;

		results->total = 1;
	} else {
		OSPPTransactionSetServiceType(results->transaction, OSPC_SERVICE_VOICE);

		results->total = profile->maxdest;
	}

	OSP_DEBUG_MSG("RequestAuthorisation");

	/* Request authorization */
	error = OSPPTransactionRequestAuthorisation(
		results->transaction,	/* Transaction handle */
		src,					/* Source */
		dev,					/* Source device */
		inbound->calling,		/* Calling */
		OSPC_NFORMAT_E164,		/* Calling format */
		inbound->called,		/* Called */
		OSPC_NFORMAT_E164,		/* Called format */
		NULL,					/* User */
		0,						/* Number of callids */
		NULL,					/* Callids */
		preferred,				/* Preferred destinations */
		&results->total,		/* Destination number */
		NULL,					/* Log buffer size */
		NULL);					/* Log buffer */
	if (error != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to request routing for '%s/%s', error '%d'\n", inbound->calling, inbound->called, error);
		results->total = 0;
	} else if (results->total == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Without destination\n");
	} else {
		context = OSPPTransactionGetContext(results->transaction, &error);
		if (error == OSPC_ERR_NO_ERROR) {
			results->transid = context->TransactionID;
		} else {
			results->transid = 0;
		}
		status = SWITCH_STATUS_SUCCESS;
	}
	OSP_DEBUG("transid: '%"PRIu64"'", results->transid);
	OSP_DEBUG("total: '%d'", results->total);

	OSP_DEBUG_END;

	return status;
}

/*
 * Get first destination
 * param results Routing info
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_get_first(
	osp_results_t *results)
{
	int error;
	char term[OSP_SIZE_NORSTR];
	unsigned int callidlen = 0, tokenlen = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	/* Set destination count */
	results->count = 1;

	OSP_DEBUG_MSG("GetFirstDestination");

	/* Get first destination */
	error = OSPPTransactionGetFirstDestination(
		results->transaction,		/* Transaction handle */
		0,							/* Timestamp buffer size */
		NULL,						/* Valid after */
		NULL,						/* Valid until */
		&results->timelimit,		/* Call duration limit */
		&callidlen,					/* Callid buffer size */
		NULL,						/* Callid buffer */
		sizeof(results->called),	/* Called buffer size */
		results->called,			/* Called buffer */
		sizeof(results->calling),	/* Calling buffer size */
		results->calling,			/* Calling buffer */
		sizeof(term),				/* Destination buffer size */
		term,						/* Destination buffer */
		0,							/* Destination device buffer size */
		NULL,						/* Destination device buffer */
		&tokenlen,					/* Token buffer length */
		NULL);						/* Token buffer */
	if (error != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get first destination, error '%d'\n", error);
	} else {
		osp_convert_outin(term, results->dest, sizeof(results->dest));

		/* Check destination */
		status = osp_check_destination(results);
	}
	OSP_DEBUG("status: '%d'", status);

	OSP_DEBUG_END;

	return status;
}

/*
 * Get next destination
 * param results Routing info
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_get_next(
	osp_results_t *results)
{
	int error;
	char term[OSP_SIZE_NORSTR];
	unsigned int callidlen = 0, tokenlen = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	while ((status == SWITCH_STATUS_FALSE) && (results->count < results->total)) {
		/* Set destination count */
		results->count++;

		OSP_DEBUG_MSG("GetNextDestination");

		/* Get next destination */
		error = OSPPTransactionGetNextDestination(
			results->transaction,		/* Transsaction handle */
			results->cause,				/* Failure reason */
			0,							/* Timestamp buffer size */
			NULL,						/* Valid after */
			NULL,						/* Valid until */
			&results->timelimit,		/* Call duration limit */
			&callidlen,					/* Callid buffer size */
			NULL,						/* Callid buffer */
			sizeof(results->called),	/* Called buffer size */
			results->called,			/* Called buffer */
			sizeof(results->calling),	/* Calling buffer size */
			results->calling,			/* Calling buffer */
			sizeof(term),				/* Destination buffer size */
			term,						/* Destination buffer */
			0,							/* Destination device buffer size */
			NULL,						/* Destination device buffer */
			&tokenlen,					/* Token buffer length */
			NULL);						/* Token buffer */
		if (error != OSPC_ERR_NO_ERROR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get destination, error '%d'\n", error);
			break;
		} else {
			osp_convert_outin(term, results->dest, sizeof(results->dest));

			/* Check destination */
			status = osp_check_destination(results);
		}
	}
	OSP_DEBUG("status: '%d'", status);

	OSP_DEBUG_END;

	return status;
}

/*
 * Report usage
 * param results Route info
 * param usage Usage info
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_report_usage(
	osp_results_t *results,
	osp_usage_t *usage)
{
	int error;
	unsigned int dummy = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	/* Set role info */
	OSPPTransactionSetRoleInfo(results->transaction, OSPC_RSTATE_STOP, OSPC_RFORMAT_OSP, OSPC_RVENDOR_FREESWITCH);

	/* Set termination cause */
	if (results->cause) {
		OSPPTransactionRecordFailure(results->transaction, results->cause);
	} else {
		OSPPTransactionRecordFailure(results->transaction, usage->cause);
	}

	/* Set codecs */
	if (!switch_strlen_zero(usage->srccodec)) {
		OSPPTransactionSetCodec(results->transaction, OSPC_CODEC_SOURCE, usage->srccodec);
	}
	if (!switch_strlen_zero(usage->destcodec)) {
		OSPPTransactionSetCodec(results->transaction, OSPC_CODEC_DESTINATION, usage->destcodec);
	}

	/* Set QoS statistics */
	if (usage->rtpsrcrepoctets != OSP_DEF_STATS) {
		OSPPTransactionSetOctets(results->transaction, OSPC_SMETRIC_RTP, OSPC_SDIR_SRCREP, usage->rtpsrcrepoctets);
	}
	if (usage->rtpdestrepoctets != OSP_DEF_STATS) {
		OSPPTransactionSetOctets(results->transaction, OSPC_SMETRIC_RTP, OSPC_SDIR_DESTREP, usage->rtpdestrepoctets);
	}
	if (usage->rtpsrcreppackets != OSP_DEF_STATS) {
		OSPPTransactionSetPackets(results->transaction, OSPC_SMETRIC_RTP, OSPC_SDIR_SRCREP, usage->rtpsrcreppackets);
	}
	if (usage->rtpdestreppackets != OSP_DEF_STATS) {
		OSPPTransactionSetPackets(results->transaction, OSPC_SMETRIC_RTP, OSPC_SDIR_DESTREP, usage->rtpdestreppackets);
	}

	OSP_DEBUG_MSG("ReportUsage");

	/* Report usage */
	error = OSPPTransactionReportUsage(
		results->transaction,				/* Transaction handle */
		usage->duration / 1000000,			/* Duration */
		usage->start / 1000000,				/* Start time */
		usage->end / 1000000,				/* End time */
		usage->alert / 1000000,				/* Alert time */
		usage->connect / 1000000,			/* Connect time */
		usage->alert,						/* If PDD exist (call ringed) */
		usage->pdd / 1000,					/* Post dial delay, in ms */
		usage->release,						/* Release source */
		NULL,								/* Conference ID */
		OSP_DEF_STATS,						/* Loss packet sent */
		OSP_DEF_STATS,						/* Loss fraction sent */
		OSP_DEF_STATS,						/* Loss packet received */
		OSP_DEF_STATS,						/* Loss fraction received */
		&dummy,								/* Detail log buffer size */
		NULL);								/* Detail log buffer */
	if (error != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to report usage, error '%d'\n", error);
	} else {
		status = SWITCH_STATUS_SUCCESS;
	}

	/* Delete transaction handle */
	OSPPTransactionDelete(results->transaction);

	OSP_DEBUG_END;

	return status;
}

/*
 * Export OSP lookup status to channel
 * param channel Originator channel
 * param status OSP lookup status
 * param outbound Outbound info
 * param results Routing info
 * return
 */
static void osp_export_lookup(
	switch_channel_t *channel,
	switch_status_t status,
	osp_outbound_t *outbound,
	osp_results_t *results)
{
	char value[OSP_SIZE_ROUSTR];

	OSP_DEBUG_START;

	/* Profile name */
	switch_channel_set_variable_var_check(channel, OSP_VAR_PROFILE, results->profile, SWITCH_FALSE);
	OSP_DEBUG("%s: '%s'", OSP_VAR_PROFILE, results->profile);

	/* Transaction handle */
	switch_snprintf(value, sizeof(value), "%d", results->transaction);
	switch_channel_set_variable_var_check(channel, OSP_VAR_TRANSACTION, value, SWITCH_FALSE);
	OSP_DEBUG("%s: '%s'", OSP_VAR_TRANSACTION, value);

	/* Transaction ID */
	switch_snprintf(value, sizeof(value), "%"PRIu64"", results->transid);
	switch_channel_set_variable_var_check(channel, OSP_VAR_TRANSID, value, SWITCH_FALSE);
	OSP_DEBUG("%s: '%s'", OSP_VAR_TRANSID, value);

	/* OSP lookup status */
	switch_snprintf(value, sizeof(value), "%d", status);
	switch_channel_set_variable_var_check(channel, OSP_VAR_LOOKUPSTATUS, value, SWITCH_FALSE);
	OSP_DEBUG("%s: '%s'", OSP_VAR_LOOKUPSTATUS, value);

	/* Destination total */
	switch_snprintf(value, sizeof(value), "%d", results->total);
	switch_channel_set_variable_var_check(channel, OSP_VAR_ROUTETOTAL, value, SWITCH_FALSE);
	OSP_DEBUG("%s: '%s'", OSP_VAR_ROUTETOTAL, value);

	/* Destination count */
	switch_snprintf(value, sizeof(value), "%d", results->count);
	switch_channel_set_variable_var_check(channel, OSP_VAR_ROUTECOUNT, value, SWITCH_FALSE);
	OSP_DEBUG("%s: '%s'", OSP_VAR_ROUTECOUNT, value);

	/* Dial string */
	if (status == SWITCH_STATUS_SUCCESS) {
		osp_create_route(outbound, results, value, sizeof(value));
		switch_channel_set_variable_var_check(channel, OSP_VAR_AUTOROUTE, value, SWITCH_FALSE);
	} else {
		value[0] = '\0';
		switch_channel_set_variable(channel, OSP_VAR_AUTOROUTE, NULL);
	}
	OSP_DEBUG("%s: '%s'", OSP_VAR_AUTOROUTE, value);

	/* Termiantion cause */
	if (results->cause) {
		switch_snprintf(value, sizeof(value), "%d", results->cause);
		switch_channel_set_variable_var_check(channel, OSP_VAR_TCCODE, value, SWITCH_FALSE);
	} else {
		value[0] = '\0';
		switch_channel_set_variable(channel, OSP_VAR_TCCODE, NULL);
	}
	OSP_DEBUG("%s: '%s'", OSP_VAR_TCCODE, value);

	OSP_DEBUG_END;
}

/*
 * Export OSP next status to channel
 * param channel Originator channel
 * param status OSP next status
 * param outbound Outbound info
 * param results Routing info
 * return
 */
static void osp_export_next(
	switch_channel_t *channel,
	switch_status_t status,
	osp_outbound_t *outbound,
	osp_results_t *results)
{
	char value[OSP_SIZE_NORSTR];

	OSP_DEBUG_START;

	/* OSP next status */
	switch_snprintf(value, sizeof(value), "%d", status);
	switch_channel_set_variable_var_check(channel, OSP_VAR_NEXTSTATUS, value, SWITCH_FALSE);
	OSP_DEBUG("%s: '%s'", OSP_VAR_NEXTSTATUS, value);

	/* Destination count */
	switch_snprintf(value, sizeof(value), "%d", results->count);
	switch_channel_set_variable_var_check(channel, OSP_VAR_ROUTECOUNT, value, SWITCH_FALSE);
	OSP_DEBUG("%s: '%s'", OSP_VAR_ROUTECOUNT, value);

	/* Dial string */
	if (status == SWITCH_STATUS_SUCCESS) {
		osp_create_route(outbound, results, value, sizeof(value));
		switch_channel_set_variable_var_check(channel, OSP_VAR_AUTOROUTE, value, SWITCH_FALSE);
	} else {
		value[0] = '\0';
		switch_channel_set_variable(channel, OSP_VAR_AUTOROUTE, NULL);
	}
	OSP_DEBUG("%s: '%s'", OSP_VAR_AUTOROUTE, value);

	/* Termiantion cause */
	if (results->cause) {
		switch_snprintf(value, sizeof(value), "%d", results->cause);
		switch_channel_set_variable_var_check(channel, OSP_VAR_TCCODE, value, SWITCH_FALSE);
	} else {
		value[0] = '\0';
		switch_channel_set_variable(channel, OSP_VAR_TCCODE, NULL);
	}
	OSP_DEBUG("%s: '%s'", OSP_VAR_TCCODE, value);

	OSP_DEBUG_END;
}

/*
 * Request auth and get first OSP route
 * param channel Originator channel
 * param results Routing info
 * return
 */
static void osp_do_lookup(
	switch_channel_t *channel,
	osp_results_t *results)
{
	int error;
	osp_profile_t *profile;
	osp_inbound_t inbound;
	osp_outbound_t outbound;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	if (osp_find_profile(results->profile, &profile) == SWITCH_STATUS_FALSE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find profile '%s'\n", results->profile);
	} else if (profile->provider == OSP_INVALID_HANDLE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Disabled profile '%s'\n", results->profile);
	} else if ((error = OSPPTransactionNew(profile->provider, &results->transaction)) != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create transaction handle, error '%d'\n", error);
	} else {
		/* Get inbound info */
		osp_get_inbound(channel, &inbound);

		/* Log AuthReq parameters */
		osp_log_authreq(profile, &inbound);

		/* Do AuthReq */
		if (osp_request_auth(profile, &inbound, results) == SWITCH_STATUS_SUCCESS) {
			/* Get route */
			if ((osp_get_first(results) == SWITCH_STATUS_SUCCESS) || (osp_get_next(results) == SWITCH_STATUS_SUCCESS)) {
				/* Get outbound info */
				osp_get_outbound(channel, &outbound);
				status = SWITCH_STATUS_SUCCESS;
			}
		}
	}

	/* Export OSP lookup info */
	osp_export_lookup(channel, status, &outbound, results);

	OSP_DEBUG_END;
}

/*
 * Get next OSP route
 * param channel Originator channel
 * param results Routing info
 * return
 */
static void osp_do_next(
	switch_channel_t *channel,
	osp_results_t *results)
{
	osp_outbound_t outbound;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	if (osp_get_transaction(channel, SWITCH_FALSE, results) == SWITCH_STATUS_SUCCESS) {
		/* Get next OSP route */
		if ((status = osp_get_next(results)) == SWITCH_STATUS_SUCCESS) {
			/* Get outbound info */
			osp_get_outbound(channel, &outbound);
		}
	}

	/* Export OSP next info */
	osp_export_next(channel, status, &outbound, results);

	OSP_DEBUG_END;
}

/*
 * Report OSP usage
 * param channel Originator channel
 * param originator Originate profile
 * param terminator Terminate profile
 * param results Routing info
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_do_report(
	switch_channel_t *channel,
	switch_caller_profile_t *originator,
	switch_caller_profile_t *terminator,
	osp_results_t *results)
{
	osp_usage_t usage;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	OSP_DEBUG_START;

	if (osp_get_transaction(channel, SWITCH_TRUE, results) == SWITCH_STATUS_SUCCESS) {
		/* Do not report usage for failed AuthReq */
		if (results->total) {
			/* Get usage info */
			osp_get_usage(channel, originator, terminator, results, &usage);

			/* Log usage info */
			osp_log_usageind(results, &usage);

			/* Report OSP usage */
			status = osp_report_usage(results, &usage);
		} else {
			OSP_DEBUG_MSG("Do not report usage");
		}
	}

	OSP_DEBUG_END;

	return status;
}

/*
 * OSP module CLI command
 * Macro expands to:
 * static switch_status_t osp_cli_function(_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream)
 */
SWITCH_STANDARD_API(osp_cli_function)
{
	int i, argc = 0;
	char *argv[2] = { 0 };
	char *params = NULL;
	char *param = NULL;
	osp_profile_t *profile;
	char *loglevel;

	OSP_DEBUG_START;

	if (session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This function cannot be called from the dialplan.\n");
		OSP_DEBUG_END;
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "Usage: osp status\n");
		OSP_DEBUG_END;
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(params = switch_safe_strdup(cmd))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate parameters\n");
		OSP_DEBUG_END;
		return SWITCH_STATUS_MEMERR;
	}

	if ((argc = switch_separate_string(params, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		param = argv[0];
		if (!strcasecmp(param, "status")) {
			stream->write_function(stream, "=============== OSP Module Settings & Status ===============\n");
			stream->write_function(stream, "                debug-info: %s\n", osp_global.debug ? "enabled" : "disabled");
			switch (osp_global.loglevel) {
			case SWITCH_LOG_CONSOLE:
				loglevel = "console";
				break;
			case SWITCH_LOG_ALERT:
				loglevel = "alert";
				break;
			case SWITCH_LOG_CRIT:
				loglevel = "crit";
				break;
			case SWITCH_LOG_ERROR:
				loglevel = "error";
				break;
			case SWITCH_LOG_WARNING:
				loglevel = "warning";
				break;
			case SWITCH_LOG_NOTICE:
				loglevel = "notice";
				break;
			case SWITCH_LOG_INFO:
				loglevel = "info";
				break;
			case SWITCH_LOG_DEBUG:
			default:
				loglevel = "debug";
				break;
			}
			stream->write_function(stream, "                 log-level: %s\n", loglevel);
			stream->write_function(stream, "           crypto-hardware: %s\n", osp_global.hardware ? "enabled" : "disabled");
			if (switch_strlen_zero(osp_global.endpoint[OSPC_PROTNAME_SIP].module) || switch_strlen_zero(osp_global.endpoint[OSPC_PROTNAME_SIP].profile)) {
				stream->write_function(stream, "                       sip: unsupported\n");
			} else {
				stream->write_function(stream, "                       sip: %s/%s\n",
					osp_global.endpoint[OSPC_PROTNAME_SIP].module, osp_global.endpoint[OSPC_PROTNAME_SIP].profile);
			}
			if (switch_strlen_zero(osp_global.endpoint[OSPC_PROTNAME_Q931].module) || switch_strlen_zero(osp_global.endpoint[OSPC_PROTNAME_Q931].profile)) {
				stream->write_function(stream, "                      h323: unsupported\n");
			} else {
				stream->write_function(stream, "                      h323: %s/%s\n",
					osp_global.endpoint[OSPC_PROTNAME_Q931].module, osp_global.endpoint[OSPC_PROTNAME_Q931].profile);
			}
			if (switch_strlen_zero(osp_global.endpoint[OSPC_PROTNAME_IAX].module) || switch_strlen_zero(osp_global.endpoint[OSPC_PROTNAME_IAX].profile)) {
				stream->write_function(stream, "                       iax: unsupported\n");
			} else {
				stream->write_function(stream, "                       iax: %s/%s\n",
					osp_global.endpoint[OSPC_PROTNAME_IAX].module, osp_global.endpoint[OSPC_PROTNAME_IAX].profile);
			}
			if (switch_strlen_zero(osp_global.endpoint[OSPC_PROTNAME_SKYPE].module) || switch_strlen_zero(osp_global.endpoint[OSPC_PROTNAME_SKYPE].profile)) {
				stream->write_function(stream, "                     skype: unsupported\n");
			} else {
				stream->write_function(stream, "                     skype: %s/%s\n",
					osp_global.endpoint[OSPC_PROTNAME_SKYPE].module, osp_global.endpoint[OSPC_PROTNAME_SKYPE].profile);
			}
			stream->write_function(stream, "          default-protocol: %s\n", osp_get_protocolname(osp_global.protocol));
			stream->write_function(stream, "============== OSP Profile Settings & Status ==============\n");
			for (profile = osp_profiles; profile; profile = profile->next) {
				stream->write_function(stream, "Profile: %s\n", profile->name);
				for (i = 0; i < profile->spnumber; i++) {
					stream->write_function(stream, "         service-point-url: %s\n", profile->spurl[i]);
				}
				stream->write_function(stream, "                 device-ip: %s\n", profile->deviceip);
				stream->write_function(stream, "              ssl-lifetime: %d\n", profile->lifetime);
				stream->write_function(stream, "      http-max-connections: %d\n", profile->maxconnect);
				stream->write_function(stream, "          http-persistence: %d\n", profile->persistence);
				stream->write_function(stream, "          http-retry-dalay: %d\n", profile->retrydelay);
				stream->write_function(stream, "          http-retry-limit: %d\n", profile->retrylimit);
				stream->write_function(stream, "              http-timeout: %d\n", profile->timeout);
				switch (profile->workmode) {
				case OSP_MODE_DIRECT:
					stream->write_function(stream, "                 work-mode: direct\n");
					break;
				case OSP_MODE_INDIRECT:
				default:
					stream->write_function(stream, "                 work-mode: indirect\n");
					break;
				}
				switch (profile->srvtype) {
				case OSP_SRV_NPQUERY:
					stream->write_function(stream, "              service-type: npquery\n");
					break;
				case OSP_SRV_VOICE:
				default:
					stream->write_function(stream, "              service-type: voice\n");
					break;
				}
				stream->write_function(stream, "          max-destinations: %d\n", profile->maxdest);
				stream->write_function(stream, "                    status: %s\n", profile->provider != OSP_INVALID_HANDLE ? "enabled" : "disabled");
			}
		} else {
			stream->write_function(stream, "Invalid Syntax!\n");
		}
	} else {
		stream->write_function(stream, "Invalid Input!\n");
	}

	switch_safe_free(params);

	OSP_DEBUG_END;

	return SWITCH_STATUS_SUCCESS;
}

/*
 * Macro expands to:
 * static void osp_lookup_function(switch_core_session_t *session, const char *data)
 */
SWITCH_STANDARD_APP(osp_lookup_function)
{
	switch_channel_t *channel;
	int argc = 0;
	char *argv[2] = { 0 };
	char *params = NULL;
	osp_results_t results;

	OSP_DEBUG_START;

	if (osp_global.shutdown) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "OSP application inavailable\n");
	} else if (!(channel = switch_core_session_get_channel(session))) {
		/* Make sure there is a valid channel when starting the OSP application */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to find origiantor channel\n");
	} else if (!(params = switch_core_session_strdup(session, data))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to alloc parameters\n");
	} else {
		memset(&results, 0, sizeof(osp_results_t));
		if ((argc = switch_separate_string(params, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			results.profile = argv[0];
		} else {
			results.profile = OSP_DEF_PROFILE;
		}
		results.transaction = OSP_INVALID_HANDLE;
		results.protocol = OSPC_PROTNAME_UNKNOWN;

		/* Do OSP lookup */
		osp_do_lookup(channel, &results);
	}

	OSP_DEBUG_END;
}

/*
 * Macro expands to:
 * static void osp_next_function(switch_core_session_t *session, const char *data)
 */
SWITCH_STANDARD_APP(osp_next_function)
{
	switch_channel_t *channel;
	osp_results_t results;

	OSP_DEBUG_START;

	if (osp_global.shutdown) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "OSP application inavailable\n");
	} else if (!(channel = switch_core_session_get_channel(session))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to find origiantor channel\n");
	} else {
		memset(&results, 0, sizeof(osp_results_t));
		results.transaction = OSP_INVALID_HANDLE;
		results.protocol = OSPC_PROTNAME_UNKNOWN;

		/* Do OSP next */
		osp_do_next(channel, &results);
	}

	OSP_DEBUG_END;
}

/*
 * OSP module CS_REPORTING state handler
 * param session Session
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_on_reporting(
	switch_core_session_t *session)
{
	switch_channel_t *channel;
	switch_caller_profile_t *originator;
	switch_caller_profile_t *terminator;
	osp_results_t results;
	switch_status_t status = SWITCH_STATUS_FALSE;

	OSP_DEBUG_START;

	if (osp_global.shutdown) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OSP application inavailable\n");
	} else if (!(channel = switch_core_session_get_channel(session))) {
		/* Make sure there is a valid channel when starting the OSP application */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to find origiantor channel\n");
	} else if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
		/* A-leg */
		OSP_DEBUG_MSG("A-leg");

		/* Get originator profile */
		if ((originator = switch_channel_get_caller_profile(channel))) {
			/* Get terminator profile, may be NULL */
			terminator = switch_channel_get_originatee_caller_profile(channel);

			memset(&results, 0, sizeof(osp_results_t));
			results.transaction = OSP_INVALID_HANDLE;
			results.protocol = OSPC_PROTNAME_UNKNOWN;

			/* Do OSP usage report */
			status = osp_do_report(channel, originator, terminator, &results);
		}
	} else {
		/* B-leg */
		OSP_DEBUG_MSG("B-leg");
	}

	OSP_DEBUG_END;

	return status;
}

/*
 * OSP module state handlers
 */
static switch_state_handler_table_t osp_handlers = {
	NULL,				/*.on_init */
	NULL,				/*.on_routing */
	NULL,				/*.on_execute */
	NULL,				/*.on_hangup */
	NULL,				/*.on_exchange_media */
	NULL,				/*.on_soft_execute */
	NULL,				/*.on_consume_media */
	NULL,				/*.on_hibernate */
	NULL,				/*.on_reset */
	NULL,				/*.on_park */
	osp_on_reporting	/*.on_reporting */
};

/* switch_status_t mod_osp_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)  */
SWITCH_MODULE_LOAD_FUNCTION(mod_osp_load);
/* switch_status_t mod_osp_shutdown(void) */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_osp_shutdown);
/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) */
SWITCH_MODULE_DEFINITION(mod_osp, mod_osp_load, mod_osp_shutdown, NULL);

/*
 * Called when OSP module is loaded
 * Macro expands to:
 * switch_status_t mod_osp_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_osp_load)
{
	switch_api_interface_t *cli_interface;
	switch_application_interface_t *app_interface;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	OSP_DEBUG_START;

	/* Load OSP module configuration */
	if ((status = osp_load_config(pool)) != SWITCH_STATUS_SUCCESS) {
		OSP_DEBUG_END;
		return status;
	}

	/* Init OSP client end */
	osp_init_osptk();

	/* Connect OSP module internal structure to the blank pointer passed to OSP module */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* Add CLI osp status command */
	SWITCH_ADD_API(cli_interface, "osp", "OSP", osp_cli_function, "status");
	switch_console_set_complete("add osp status");

	/* Add OSP module applications */
	SWITCH_ADD_APP(app_interface, "osplookup", "Perform an OSP lookup", "Perform an OSP lookup", osp_lookup_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "ospnext", "Retrive next OSP route", "Retrive next OSP route", osp_next_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	/* Add OSP module state handlers */
	switch_core_add_state_handler(&osp_handlers);

	OSP_DEBUG_END;

	/* Indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
 * Called when the system shuts down
 * Macro expands to:
 * switch_status_t mod_osp_shutdown(void)
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_osp_shutdown)
{
	OSP_DEBUG_START;

	/* Shutdown OSP module */
	osp_global.shutdown = SWITCH_TRUE;

	/* Cleanup OSP client end */
	osp_cleanup_osptk();

	/* Remoeve OSP module state handlers */
	switch_core_remove_state_handler(&osp_handlers);

	OSP_DEBUG_END;

	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */

