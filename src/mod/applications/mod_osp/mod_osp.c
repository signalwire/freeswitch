/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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

/*
 * TODO:
 * 1. NID -> outbound messages
 */

#include <switch.h>
#include <osp/osp.h>
#include <osp/ospb64.h>
#include <osp/osptrans.h>

/* OSP Buffer Size Constants */
#define OSP_SIZE_NORSTR		256		/* OSP normal string buffer size */
#define OSP_SIZE_KEYSTR		1024	/* OSP certificate string buffer size */
#define OSP_SIZE_ROUSTR		1024	/* OSP route buffer size */
#define OSP_SIZE_TOKSTR		4096	/* OSP token string buffer size */

/* OSP Settings Constants */
#define OSP_MAX_SP			8			/* Max number of OSP service points */
#define OSP_AUDIT_URL		"localhost"	/* OSP default Audit URL */
#define OSP_LOCAL_VALID		1			/* OSP token validating method, locally */
#define OSP_DEF_LIFETIME	300			/* OSP default SSL lifetime */
#define OSP_DEF_MAXCONN		20			/* OSP default max connections */
#define OSP_MIN_MAXCONN		1			/* OSP min max connections */
#define OSP_MAX_MAXCONN		1000		/* OSP max max connections */
#define OSP_DEF_PERSIST		60			/* OSP default HTTP persistence in seconds */
#define OSP_DEF_RETRYDELAY	0			/* OSP default retry delay in seconds */
#define OSP_MIN_RETRYDELAY	0			/* OSP min retry delay */
#define OSP_MAX_RETRYDELAY	10			/* OSP max retry delay */
#define OSP_DEF_RETRYLIMIT	2			/* OSP default retry times */
#define OSP_MIN_RETRYLIMIT	0			/* OSP min retry times */
#define OSP_MAX_RETRYLIMIT	100			/* OSP max retry times */
#define OSP_DEF_TIMEOUT		10000		/* OSP default timeout in ms */
#define OSP_MIN_TIMEOUT		200			/* OSP min timeout in ms */
#define OSP_MAX_TIMEOUT		60000		/* OSP max timeout in ms */
#define OSP_CUSTOMER_ID		""			/* OSP customer ID */
#define OSP_DEVICE_ID		""			/* OSP device ID */
#define OSP_DEF_MAXDEST		5			/* OSP default max destinations */
#define OSP_MIN_MAXDEST		1			/* OSP min max destinations */
#define OSP_MAX_MAXDEST		10			/* OSP max max destinations */
#define OSP_DEF_PROFILE		"default"	/* OSP default profile name */
#define OSP_DEF_STRING		""			/* OSP default empty string */
#define OSP_DEF_CALLID		"UNDEFINED"	/* OSP default Call-ID */
#define OSP_DEF_STATS		-1			/* OSP default statistics */
#define OSP_URI_DELIM		'@'			/* URI delimit */
#define OSP_USER_DELIM		";:"		/* URI userinfo delimit */
#define OSP_HOST_DELIM		";>"		/* URI hostport delimit */
#define OSP_MAX_CINFO		8			/* Max number of custom info */

/* OSP Handle Constant */
#define OSP_INVALID_HANDLE	-1	/* Invalid OSP handle, provider, transaction etc. */

/* OSP Supported Destination Protocols for Default Protocol */
#define OSP_PROTOCOL_SIP	"sip"			/* SIP protocol name */
#define OSP_PROTOCOL_H323	"h323"			/* H.323 protocol name */
#define OSP_PROTOCOL_IAX	"iax"			/* IAX protocol name */
#define OSP_PROTOCOL_SKYPE	"skype"			/* Skype protocol name */
#define OSP_PROTOCOL_UNKNO	"unknown"		/* Unknown protocol */
#define OSP_PROTOCOL_UNDEF	"undefined"		/* Undefined protocol */
#define OSP_PROTOCOL_UNSUP	"unsupported"	/* Unsupported protocol */

/* OSP Supported Destination Protocols for Destination Protocol Usage */
#define OSP_MODULE_SIP		"mod_sofia"		/* FreeSWITCH SIP module name */
#define OSP_MODULE_H323		"mod_h323"		/* FreeSWITCH H.323 module name */
#define OSP_MODULE_IAX		"mod_iax"		/* FreeSWITCH IAX module name */
#define OSP_MODULE_SKYPE	"mod_skypopen"	/* FreeSWITCH Skype module name */

/* OSP Variables Name */
#define OSP_VAR_PROFILE		"osp_profile"			/* Provider name, in OSP cookie */
#define OSP_VAR_TRANSID		"osp_transaction_id"	/* Transaction ID, in OSP cookie */
#define OSP_VAR_START		"osp_start_time"		/* Inbound Call start time, in OSP cookie */
#define OSP_VAR_DESTCOUNT	"osp_destination_count"	/* Destination count, in OSP cookie */
#define OSP_VAR_SRCNID		"osp_source_nid"		/* Source network ID, inbound and in OSP cookie */
#define OSP_VAR_DESTIP		"osp_destination_ip"	/* Destination IP, in OSP cookie */
#define OSP_VAR_DESTNID		"osp_destination_nid"	/* Destination network ID, in OSP cookie */
#define OSP_VAR_CUSTOMINFO	"osp_custom_info_"		/* Custom info */
#define OSP_VAR_ROUTECOUNT	"osp_route_count"		/* Number of destinations */
#define OSP_VAR_ROUTEPRE	"osp_route_"			/* Destination prefix */
#define OSP_VAR_AUTOROUTE	"osp_auto_route"		/* Bridge route string */

/* OSP Use Variable Name */
#define OSP_FS_TOHOST		"sip_to_host"						/* Inbound SIP To host */
#define OSP_FS_TOPORT		"sip_to_port"						/* Inbound SIP To port */
#define OSP_FS_DIVERSION	"sip_h_Diversion"					/* Inbound SIP Diversion header */
#define OSP_FS_OUTCALLID	"sip_call_id"						/* Outbound SIP Call-ID */
#define OSP_FS_OUTCALLING	"origination_caller_id_number"		/* Outbound calling number */
#define OSP_FS_SIPRELEASE	"sip_hangup_disposition"			/* SIP release source */
#define OSP_FS_DOWNCODEC	"write_codec"						/* Downstream codec */
#define OSP_FS_UPCODEC		"read_codec"						/* Upstream codec */
#define OSP_FS_RTPDOWNOCTS	"rtp_audio_out_media_bytes"			/* Downstream octets */
#define OSP_FS_RTPUPOCTS	"rtp_audio_in_media_bytes"			/* Upstream octets */
#define OSP_FS_RTPDOWNPKTS	"rtp_audio_out_media_packet_count"	/* Downstream packets */
#define OSP_FS_RTPUPPKTS	"rtp_audio_in_media_packet_count"	/* Upstream packets */

typedef struct osp_settings {
	switch_bool_t debug;					/* OSP module debug info flag */
	switch_log_level_t loglevel;			/* Log level for debug info */
	switch_bool_t hardware;					/* Crypto hardware flag */
	const char *module[OSPC_DPROT_NUMBER];	/* Endpoint names */
	const char *profile[OSPC_DPROT_NUMBER];	/* Profile names */
	OSPE_DEST_PROTOCOL protocol;			/* Default destination protocol */
	switch_bool_t shutdown;					/* OSP module status */
	switch_memory_pool_t *pool;				/* OSP module memory pool */
} osp_settings_t;

/* OSP Service Types */
typedef enum osp_srvtype {
	OSP_SRV_VOICE = 0,	/* Normal voice service */
	OSP_SRV_NPQUERY		/* Number portability query service */
} osp_srvtype_t;

typedef struct osp_provider {
	const char *name;				/* OSP provider profile name */
	int spnum;						/* Number of OSP service points */
	const char *spurls[OSP_MAX_SP];	/* OSP provider service point URLs */
	const char *device;				/* OSP source IP */
	int lifetime;					/* SSL life time */
	int maxconnect;					/* Max number of HTTP connections */
	int persistence;				/* HTTP persistence in seconds */
	int retrydelay;					/* HTTP retry delay in seconds */
	int retrylimit;					/* HTTP retry times */
	int timeout;					/* HTTP timeout in ms */
	osp_srvtype_t srvtype;			/* OSP service type */
	int maxdest;					/* Max destinations */
	switch_bool_t userphone;		/* Add "user=phone" URI parameter */
	OSPTPROVHANDLE handle;			/* OSP provider handle */
	struct osp_provider *next;		/* Next OSP provider */
} osp_provider_t;

typedef struct osp_inbound {
	const char *srcdev;					/* Source device IP address */
	const char *calling;				/* Inbound calling number */
	char called[OSP_SIZE_NORSTR];		/* Inbound called number */
	char nprn[OSP_SIZE_NORSTR];			/* Inbound NP routing number */
	char npcic[OSP_SIZE_NORSTR];		/* Inbound NP carrier identification code */
	int npdi;							/* Inbound NP database dip indicator */
	const char *tohost;					/* Inbound host of To URI */
	const char *toport;					/* Inbound port of To URI */
	char divuser[OSP_SIZE_NORSTR];		/* Inbound user of SIP Diversion header */
	char divhost[OSP_SIZE_NORSTR];		/* Inbound hostport of SIP Diversion header */
	const char *srcnid;					/* Inbound source network ID */
	switch_time_t start;				/* Call start time */
	const char *cinfo[OSP_MAX_CINFO];	/* Custom info */
} osp_inbound_t;

typedef struct osp_destination {
	unsigned int timelimit;								/* Outbound duration limit */
	char dest[OSP_SIZE_NORSTR];							/* Destination IP address */
	char calling[OSP_SIZE_NORSTR];						/* Outbound calling number, may be translated */
	char called[OSP_SIZE_NORSTR];						/* Outbound called number, may be translated */
	char destnid[OSP_SIZE_NORSTR];						/* Destination network ID */
	char nprn[OSP_SIZE_NORSTR];							/* Outbound NP routing number */
	char npcic[OSP_SIZE_NORSTR];						/* Outbound NP carrier identification code */
	int npdi;											/* Outbound NP database dip indicator */
	char opname[OSPC_OPNAME_NUMBER][OSP_SIZE_NORSTR];	/* Outbound Operator names */
	OSPE_DEST_PROTOCOL protocol;						/* Destination protocol */
	switch_bool_t supported;							/* Supported by FreeRADIUS OSP module */
	switch_bool_t userphone;							/* Add "user=phone" parameter */
} osp_destination_t;

typedef struct osp_results {
	const char *profile;					/* Provider name */
	unsigned long long transid;				/* Transaction ID */
	switch_time_t start;					/* Call start time */
	char called[OSP_SIZE_NORSTR];			/* Original called number */
	const char *srcnid;						/* Source network ID */
	int numdest;							/* Number of destinations */
	osp_destination_t dests[OSP_MAX_SP];	/* Destinations */
} osp_results_t;

typedef struct osp_cookie {
	const char *profile;		/* Provider name */
	unsigned long long transid;	/* Transaction ID */
	switch_time_t start;		/* Call start time */
	int destcount;				/* Destination count */
	const char *dest;			/* Destination IP */
	const char *srcnid;			/* Source network ID */
	const char *destnid;		/* Destination network ID */
} osp_cookie_t;

typedef struct osp_usage {
	const char *callid;				/* Call-ID */
	const char *calling;			/* Calling number */
	char called[OSP_SIZE_NORSTR];	/* Called number */
	const char *srcdev;				/* Source device IP */
	OSPE_DEST_PROTOCOL protocol;	/* Destination protocol */
	int release;					/* Release source */
	switch_call_cause_t cause;		/* Termination cause */
	switch_time_t alert;			/* Call alert time */
	switch_time_t connect;			/* Call answer time */
	switch_time_t end;				/* Call end time */
	switch_time_t duration;			/* Call duration */
	switch_time_t pdd;				/* Post dial delay */
	const char *fcodec;				/* Forward codec */
	const char *rcodec;				/* Reverse codec */
	int rtpdownoctets;				/* RTP downstream bytes */
	int rtpupoctets;				/* RTP upstream bytes */
	int rtpdownpackets;				/* RTP downstream packets */
	int rtpuppackets;				/* RTP upstream packets */
} osp_usage_t;

typedef struct osp_threadarg {
	OSPTTRANHANDLE handle;		/* Transaction handle */
	unsigned long long transid;	/* Transaction ID */
	switch_call_cause_t cause;	/* Release code */
	time_t start;				/* Call start time */
	time_t alert;				/* Call alert time */
	time_t connect;				/* Call connect time */
	time_t end;					/* Call end time */
	int duration;				/* Call duration */
	int pdd;					/* Post dial delay */
	int release;				/* EP that released the call */
} osp_threadarg_t;

/* OSP module global settings */
static osp_settings_t osp_globals;

/* OSP module providers */
static osp_provider_t *osp_providers = NULL;

/* switch_status_t mod_osp_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)  */
SWITCH_MODULE_LOAD_FUNCTION(mod_osp_load);
/* switch_status_t mod_osp_shutdown(void) */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_osp_shutdown);
/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) */
SWITCH_MODULE_DEFINITION(mod_osp, mod_osp_load, mod_osp_shutdown, NULL);

/* Macro to prevent NULL string */
#define osp_filter_null(_str)				switch_strlen_zero(_str) ? OSP_DEF_STRING : _str
#define osp_adjust_len(_head, _size, _len)	{ _len = strlen(_head); _head += _len; _size -= _len; }

/*
 * Find OSP provider by name
 * param name OSP provider name
 * param provider OSP provider, NULL is allowed
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_find_provider(
	const char *name,
	osp_provider_t **provider)
{
	osp_provider_t *p;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (name) {
		if (provider) {
			*provider = NULL;
		}

		for (p = osp_providers; p; p = p->next) {
			if (!strcasecmp(p->name, name)) {
				if (provider) {
					*provider = p;
				}
				status = SWITCH_STATUS_SUCCESS;
				break;
			}
		}
	}

	return status;
}

/*
 * Load OSP module configuration
 * param pool OSP module memory pool
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed, SWITCH_STATUS_MEMERR Memory Error.
 */
static switch_status_t osp_load_settings(
	switch_memory_pool_t *pool)
{
	char *cf = "osp.conf";
	switch_xml_t cfg, xml = NULL, param, settings, profile, profiles;
	const char *name;
	const char *value;
	const char *module;
	const char *context;
	osp_provider_t *provider;
	int number;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open '%s'\n", cf);
		status = SWITCH_STATUS_FALSE;
		return status;
	}

	memset(&osp_globals, 0, sizeof(osp_globals));
	osp_globals.loglevel = SWITCH_LOG_DEBUG;
	osp_globals.pool = pool;
	osp_globals.protocol = OSPC_DPROT_SIP;

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			name = switch_xml_attr_soft(param, "name");
			value = switch_xml_attr_soft(param, "value");
			module = switch_xml_attr_soft(param, "module");
			context = switch_xml_attr_soft(param, "profile");
			if (switch_strlen_zero(name)) {
				continue;
			}
			if (!strcasecmp(name, "debug-info")) {
				if (!switch_strlen_zero(value)) {
					osp_globals.debug = switch_true(value);
				}
			} else if (!strcasecmp(name, "log-level")) {
				if (switch_strlen_zero(value)) {
					continue;
				} else if (!strcasecmp(value, "console")) {
					osp_globals.loglevel = SWITCH_LOG_CONSOLE;
				} else if (!strcasecmp(value, "alert")) {
					osp_globals.loglevel = SWITCH_LOG_ALERT;
				} else if (!strcasecmp(value, "crit")) {
					osp_globals.loglevel = SWITCH_LOG_CRIT;
				} else if (!strcasecmp(value, "error")) {
					osp_globals.loglevel = SWITCH_LOG_ERROR;
				} else if (!strcasecmp(value, "warning")) {
					osp_globals.loglevel = SWITCH_LOG_WARNING;
				} else if (!strcasecmp(value, "notice")) {
					osp_globals.loglevel = SWITCH_LOG_NOTICE;
				} else if (!strcasecmp(value, "info")) {
					osp_globals.loglevel = SWITCH_LOG_INFO;
				} else if (!strcasecmp(value, "debug")) {
					osp_globals.loglevel = SWITCH_LOG_DEBUG;
				}
			} else if (!strcasecmp(name, "crypto-hardware")) {
				if (!switch_strlen_zero(value)) {
					osp_globals.hardware = switch_true(value);
				}
			} else if (!strcasecmp(name, "default-protocol")) {
				if (switch_strlen_zero(value)) {
					continue;
				} else if (!strcasecmp(value, OSP_PROTOCOL_SIP)) {
					osp_globals.protocol = OSPC_DPROT_SIP;
				} else if (!strcasecmp(value, OSP_PROTOCOL_H323)) {
					osp_globals.protocol = OSPC_DPROT_Q931;
				} else if (!strcasecmp(value, OSP_PROTOCOL_IAX)) {
					osp_globals.protocol = OSPC_DPROT_IAX;
				} else if (!strcasecmp(value, OSP_PROTOCOL_SKYPE)) {
					osp_globals.protocol = OSPC_DPROT_SKYPE;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported default protocol '%s'\n", value);
				}
			} else if (!strcasecmp(name, "sip")) {
				if (!switch_strlen_zero(module)) {
					if (!(osp_globals.module[OSPC_DPROT_SIP] = switch_core_strdup(osp_globals.pool, module))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate SIP module name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				if (!switch_strlen_zero(context)) {
					if (!(osp_globals.profile[OSPC_DPROT_SIP] = switch_core_strdup(osp_globals.pool, context))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate SIP profile name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
			} else if (!strcasecmp(name, "h323")) {
				if (!switch_strlen_zero(module)) {
					if (!(osp_globals.module[OSPC_DPROT_Q931] = switch_core_strdup(osp_globals.pool, module))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate H.323 module name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				if (!switch_strlen_zero(context)) {
					if (!(osp_globals.profile[OSPC_DPROT_Q931] = switch_core_strdup(osp_globals.pool, context))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate H.323 profile name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
			} else if (!strcasecmp(name, "iax")) {
				if (!switch_strlen_zero(module)) {
					if (!(osp_globals.module[OSPC_DPROT_IAX] = switch_core_strdup(osp_globals.pool, module))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate IAX module name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
				if (!switch_strlen_zero(context)) {
					if (!(osp_globals.profile[OSPC_DPROT_IAX] = switch_core_strdup(osp_globals.pool, context))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate IAX profile name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
			} else if (!strcasecmp(name, "skype")) {
				if (!switch_strlen_zero(module)) {
					if (!(osp_globals.module[OSPC_DPROT_SKYPE] = switch_core_strdup(osp_globals.pool, module))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate Skype module name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
					}
				if (!switch_strlen_zero(context)) {
					if (!(osp_globals.profile[OSPC_DPROT_SKYPE] = switch_core_strdup(osp_globals.pool, context))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate Skype profile name\n");
						status = SWITCH_STATUS_MEMERR;
						break;
					}
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown parameter '%s'\n", name);
			}
		}
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_xml_free(xml);
		return status;
	}

	if ((profiles = switch_xml_child(cfg, "profiles"))) {
		for (profile = switch_xml_child(profiles, "profile"); profile; profile = profile->next) {
			name = switch_xml_attr_soft(profile, "name");
			if (switch_strlen_zero(name)) {
				name = OSP_DEF_PROFILE;
			}
			if (osp_find_provider(name, NULL) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignored duplicate profile '%s'\n", name);
				continue;
			}
			if (!(provider = switch_core_alloc(osp_globals.pool, sizeof(*provider)))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to alloc provider\n");
				status = SWITCH_STATUS_MEMERR;
				break;
			}
			if (!(provider->name = switch_core_strdup(osp_globals.pool, name))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate provider name\n");
				status = SWITCH_STATUS_MEMERR;
				/* "provider" cannot free to pool in FreeSWITCH */
				break;
			}

			/* Provider has been set to 0 by switch_core_alloc */
			provider->lifetime = OSP_DEF_LIFETIME;
			provider->maxconnect = OSP_DEF_MAXCONN;
			provider->persistence = OSP_DEF_PERSIST;
			provider->retrydelay = OSP_DEF_RETRYDELAY;
			provider->retrylimit = OSP_DEF_RETRYLIMIT;
			provider->timeout = OSP_DEF_TIMEOUT;
			provider->maxdest = OSP_DEF_MAXDEST;
			provider->handle = OSP_INVALID_HANDLE;

			for (param = switch_xml_child(profile, "param"); param; param = param->next) {
				name = switch_xml_attr_soft(param, "name");
				value = switch_xml_attr_soft(param, "value");
				if (switch_strlen_zero(name) || switch_strlen_zero(value)) {
					continue;
				}
				if (!strcasecmp(name, "service-point-url")) {
					if (provider->spnum < OSP_MAX_SP) {
						provider->spurls[provider->spnum] = switch_core_strdup(osp_globals.pool, value);
						provider->spnum++;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignored excess service point '%s'\n", value);
					}
				} else if (!strcasecmp(name, "device-ip")) {
					provider->device = switch_core_strdup(osp_globals.pool, value);
				} else if (!strcasecmp(name, "ssl-lifetime")) {
					provider->lifetime = atoi(value);
				} else if (!strcasecmp(name, "http-max-connections")) {
					number = atoi(value);
					if ((number >= OSP_MIN_MAXCONN) && (number <= OSP_MAX_MAXCONN)) {
						provider->maxconnect = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							"http-max-connections must be between %d and %d\n", OSP_MIN_MAXCONN, OSP_MAX_MAXCONN);
					}
				} else if (!strcasecmp(name, "http-persistence")) {
					provider->persistence = atoi(value);
				} else if (!strcasecmp(name, "http-retry-delay")) {
					number = atoi(value);
					if ((number >= OSP_MIN_RETRYDELAY) && (number <= OSP_MAX_RETRYDELAY)) {
						provider->retrydelay = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							"http-retry-delay must be between %d and %d\n", OSP_MIN_RETRYDELAY, OSP_MAX_RETRYDELAY);
					}
				} else if (!strcasecmp(name, "http-retry-limit")) {
					number = atoi(value);
					if ((number >= OSP_MIN_RETRYLIMIT) && (number <= OSP_MAX_RETRYLIMIT)) {
						provider->retrylimit = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							"http-retry-limit must be between %d and %d\n", OSP_MIN_RETRYLIMIT, OSP_MAX_RETRYLIMIT);
					}
				} else if (!strcasecmp(name, "http-timeout")) {
					number = atoi(value);
					if ((number >= OSP_MIN_TIMEOUT) && (number <= OSP_MAX_TIMEOUT)) {
						provider->timeout = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							"http-timeout must be between %d and %d\n", OSP_MIN_TIMEOUT, OSP_MAX_TIMEOUT);
					}
				} else if (!strcasecmp(name, "service-type")) {
					if (!strcasecmp(value, "voice")) {
						provider->srvtype = OSP_SRV_VOICE;
					} else if (!strcasecmp(value, "npquery")) {
						provider->srvtype = OSP_SRV_NPQUERY;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown service type '%s'\n", value);
					}
				} else if (!strcasecmp(name, "max-destinations")) {
					number = atoi(value);
					if ((number >= OSP_MIN_MAXDEST) && (number <= OSP_MAX_MAXDEST)) {
						provider->maxdest = number;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							"max-destinations must be between %d and %d\n", OSP_MIN_MAXDEST, OSP_MAX_MAXDEST);
					}
				} else if (!strcasecmp(name, "user-phone")) {
					provider->userphone = switch_true(value);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown parameter '%s'\n", name);
				}
			}

			if (!provider->spnum) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Without service point URI in profile '%s'\n", provider->name);
				/* "provider" cannot free to pool in FreeSWITCH */
				continue;
			}

			provider->next = osp_providers;
			osp_providers = provider;
		}
	}

	switch_xml_free(xml);

	return status;
}

/* OSP default certificates */
const char *B64PKey = "MIIBOgIBAAJBAK8t5l+PUbTC4lvwlNxV5lpl+2dwSZGW46dowTe6y133XyVEwNiiRma2YNk3xKs/TJ3Wl9Wpns2SYEAJsFfSTukCAwEAAQJAPz13vCm2GmZ8Zyp74usTxLCqSJZNyMRLHQWBM0g44Iuy4wE3vpi7Wq+xYuSOH2mu4OddnxswCP4QhaXVQavTAQIhAOBVCKXtppEw9UaOBL4vW0Ed/6EA/1D8hDW6St0h7EXJAiEAx+iRmZKhJD6VT84dtX5ZYNVk3j3dAcIOovpzUj9a0CECIEduTCapmZQ5xqAEsLXuVlxRtQgLTUD4ZxDElPn8x0MhAiBE2HlcND0+qDbvtwJQQOUzDgqg5xk3w8capboVdzAlQQIhAMC+lDL7+gDYkNAft5Mu+NObJmQs4Cr+DkDFsKqoxqrm";
const char *B64LCert = "MIIBeTCCASMCEHqkOHVRRWr+1COq3CR/xsowDQYJKoZIhvcNAQEEBQAwOzElMCMGA1UEAxMcb3NwdGVzdHNlcnZlci50cmFuc25leHVzLmNvbTESMBAGA1UEChMJT1NQU2VydmVyMB4XDTA1MDYyMzAwMjkxOFoXDTA2MDYyNDAwMjkxOFowRTELMAkGA1UEBhMCQVUxEzARBgNVBAgTClNvbWUtU3RhdGUxITAfBgNVBAoTGEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDBcMA0GCSqGSIb3DQEBAQUAA0sAMEgCQQCvLeZfj1G0wuJb8JTcVeZaZftncEmRluOnaME3ustd918lRMDYokZmtmDZN8SrP0yd1pfVqZ7NkmBACbBX0k7pAgMBAAEwDQYJKoZIhvcNAQEEBQADQQDnV8QNFVVJx/+7IselU0wsepqMurivXZzuxOmTEmTVDzCJx1xhA8jd3vGAj7XDIYiPub1PV23eY5a2ARJuw5w9";
const char *B64CACert = "MIIBYDCCAQoCAQEwDQYJKoZIhvcNAQEEBQAwOzElMCMGA1UEAxMcb3NwdGVzdHNlcnZlci50cmFuc25leHVzLmNvbTESMBAGA1UEChMJT1NQU2VydmVyMB4XDTAyMDIwNDE4MjU1MloXDTEyMDIwMzE4MjU1MlowOzElMCMGA1UEAxMcb3NwdGVzdHNlcnZlci50cmFuc25leHVzLmNvbTESMBAGA1UEChMJT1NQU2VydmVyMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAPGeGwV41EIhX0jEDFLRXQhDEr50OUQPq+f55VwQd0TQNts06BP29+UiNdRW3c3IRHdZcJdC1Cg68ME9cgeq0h8CAwEAATANBgkqhkiG9w0BAQQFAANBAGkzBSj1EnnmUxbaiG1N4xjIuLAWydun7o3bFk2tV8dBIhnuh445obYyk1EnQ27kI7eACCILBZqi2MHDOIMnoN0=";

/*
 * Init OSP client end
 * return
 */
static void osp_init_osptk(void)
{
	osp_provider_t *provider;
	OSPTPRIVATEKEY privatekey;
	unsigned char privatekeydata[OSP_SIZE_KEYSTR];
	OSPT_CERT localcert;
	unsigned char localcertdata[OSP_SIZE_KEYSTR];
	const OSPT_CERT *pcacert;
	OSPT_CERT cacert;
	unsigned char cacertdata[OSP_SIZE_KEYSTR];
	int error;

	if (osp_globals.hardware) {
		if ((error = OSPPInit(OSPC_TRUE)) != OSPC_ERR_NO_ERROR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to enable crypto hardware, error '%d'\n", error);
			osp_globals.hardware = SWITCH_FALSE;
			OSPPInit(OSPC_FALSE);
		}
	} else {
		OSPPInit(OSPC_FALSE);
	}

	for (provider = osp_providers; provider; provider = provider->next) {
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
			error = OSPPProviderNew(
				provider->spnum,		/* Number of service points */
				provider->spurls,		/* Service point URLs */
				NULL,					/* Weights */
				OSP_AUDIT_URL,			/* Audit URL */
				&privatekey,			/* Provate key */
				&localcert,				/* Local cert */
				1,						/* Number of cacerts */
				&pcacert,				/* cacerts */
				OSP_LOCAL_VALID,		/* Validating method */
				provider->lifetime,		/* SS lifetime */
				provider->maxconnect,	/* HTTP max connections */
				provider->persistence,	/* HTTP persistence */
				provider->retrydelay,	/* HTTP retry delay, in seconds */
				provider->retrylimit,	/* HTTP retry times */
				provider->timeout,		/* HTTP timeout */
				OSP_CUSTOMER_ID,		/* Customer ID */
				OSP_DEVICE_ID,			/* Device ID */
				&provider->handle);		/* Provider handle */
			if (error != OSPC_ERR_NO_ERROR) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to create provider %s, error '%d'\n", provider->name, error);
				provider->handle = OSP_INVALID_HANDLE;
			}
		}
	}
}

/*
 * Get protocol name
 * param protocol Supported protocol
 * return protocol name
 */
static const char *osp_get_protocol(
	OSPE_DEST_PROTOCOL protocol)
{
	const char *name;

	switch (protocol) {
	case OSPC_DPROT_UNKNOWN:
		name = OSP_PROTOCOL_UNKNO;
		break;
	case OSPC_DPROT_UNDEFINED:
		name = OSP_PROTOCOL_UNDEF;
		break;
	case OSPC_DPROT_SIP:
		name = OSP_PROTOCOL_SIP;
		break;
	case OSPC_DPROT_Q931:
		name = OSP_PROTOCOL_H323;
		break;
	case OSPC_DPROT_IAX:
		name = OSP_PROTOCOL_IAX;
		break;
	case OSPC_DPROT_SKYPE:
		name = OSP_PROTOCOL_SKYPE;
		break;
	case OSPC_DPROT_LRQ:
	case OSPC_DPROT_T37:
	case OSPC_DPROT_T38:
	case OSPC_DPROT_SMPP:
	case OSPC_DPROT_XMPP:
	case OSPC_DPROT_SMS:
	default:
		name = OSP_PROTOCOL_UNSUP;
		break;
	}

	return name;
}

/*
 * Macro expands to:
 * static switch_status_t osp_api_function(_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream)
 */
SWITCH_STANDARD_API(osp_api_function)
{
	int i, argc = 0;
	char *argv[2] = { 0 };
	char *params = NULL;
	char *param = NULL;
	osp_provider_t *provider;
	char *loglevel;

	if (session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This function cannot be called from the dialplan.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "Usage: osp status\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(params = switch_safe_strdup(cmd))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to duplicate parameters\n");
		return SWITCH_STATUS_MEMERR;
	}

	if ((argc = switch_separate_string(params, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		param = argv[0];
		if (!strcasecmp(param, "status")) {
			stream->write_function(stream, "=============== OSP Module Settings & Status ===============\n");
			stream->write_function(stream, "                debug-info: %s\n", osp_globals.debug ? "enabled" : "disabled");
			switch (osp_globals.loglevel) {
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
			stream->write_function(stream, "           crypto-hardware: %s\n", osp_globals.hardware ? "enabled" : "disabled");
			if (switch_strlen_zero(osp_globals.module[OSPC_DPROT_SIP]) || switch_strlen_zero(osp_globals.profile[OSPC_DPROT_SIP])) {
				stream->write_function(stream, "                       sip: unsupported\n");
			} else {
				stream->write_function(stream, "                       sip: %s/%s\n",
					osp_globals.module[OSPC_DPROT_SIP], osp_globals.profile[OSPC_DPROT_SIP]);
			}
			if (switch_strlen_zero(osp_globals.module[OSPC_DPROT_Q931]) || switch_strlen_zero(osp_globals.profile[OSPC_DPROT_Q931])) {
				stream->write_function(stream, "                      h323: unsupported\n");
			} else {
				stream->write_function(stream, "                      h323: %s/%s\n",
					osp_globals.module[OSPC_DPROT_Q931], osp_globals.profile[OSPC_DPROT_Q931]);
			}
			if (switch_strlen_zero(osp_globals.module[OSPC_DPROT_IAX]) || switch_strlen_zero(osp_globals.profile[OSPC_DPROT_IAX])) {
				stream->write_function(stream, "                       iax: unsupported\n");
			} else {
				stream->write_function(stream, "                       iax: %s/%s\n",
					osp_globals.module[OSPC_DPROT_IAX], osp_globals.profile[OSPC_DPROT_IAX]);
			}
			if (switch_strlen_zero(osp_globals.module[OSPC_DPROT_SKYPE]) || switch_strlen_zero(osp_globals.profile[OSPC_DPROT_SKYPE])) {
				stream->write_function(stream, "                     skype: unsupported\n");
			} else {
				stream->write_function(stream, "                     skype: %s/%s\n",
					osp_globals.module[OSPC_DPROT_SKYPE], osp_globals.profile[OSPC_DPROT_SKYPE]);
			}
			stream->write_function(stream, "          default-protocol: %s\n", osp_get_protocol(osp_globals.protocol));
			stream->write_function(stream, "============== OSP Profiles Settings & Status ==============\n");
			for (provider = osp_providers; provider; provider = provider->next) {
				stream->write_function(stream, "Profile: %s\n", provider->name);
				for (i = 0; i < provider->spnum; i++) {
					stream->write_function(stream, "         service-point-url: %s\n", provider->spurls[i]);
				}
				stream->write_function(stream, "                 device-ip: %s\n", provider->device);
				stream->write_function(stream, "              ssl-lifetime: %d\n", provider->lifetime);
				stream->write_function(stream, "      http-max-connections: %d\n", provider->maxconnect);
				stream->write_function(stream, "          http-persistence: %d\n", provider->persistence);
				stream->write_function(stream, "          http-retry-dalay: %d\n", provider->retrydelay);
				stream->write_function(stream, "          http-retry-limit: %d\n", provider->retrylimit);
				stream->write_function(stream, "              http-timeout: %d\n", provider->timeout);
				switch (provider->srvtype) {
				case OSP_SRV_NPQUERY:
					stream->write_function(stream, "              service-type: npquery\n");
					break;
				case OSP_SRV_VOICE:
				default:
					stream->write_function(stream, "              service-type: voice\n");
					break;
				}
				stream->write_function(stream, "          max-destinations: %d\n", provider->maxdest);
				stream->write_function(stream, "                user-phone: %s\n", provider->userphone ? "enabled" : "disabled");
				stream->write_function(stream, "                    status: %s\n", provider->handle != OSP_INVALID_HANDLE ? "enabled" : "disabled");
			}
		} else {
			stream->write_function(stream, "Invalid Syntax!\n");
		}
	} else {
		stream->write_function(stream, "Invalid Input!\n");
	}

	switch_safe_free(params);

	return SWITCH_STATUS_SUCCESS;
}

/*
 * Parse URI userinfo
 * param user URI userinfo
 * param user URI user
 * param usersize Size of user buffer
 * param inbound Inbound info
 * return
 */
static void osp_parse_userinfo(
	const char *userinfo,
	char *user,
	switch_size_t usersize,
	osp_inbound_t *inbound)
{
	char buffer[OSP_SIZE_NORSTR];
	char *item;
	char *tmp;

	if (switch_strlen_zero(userinfo)) {
		if (user && usersize) {
			user[0] = '\0';
		}
		/* It is not necessary to clean inbound */
		return;
	}

	switch_copy_string(buffer, userinfo, sizeof(buffer));

	item = strtok_r(buffer, OSP_USER_DELIM, &tmp);

	if (user && usersize) {
		switch_copy_string(user, item, usersize);
	}

	if (inbound) {
		switch_copy_string(inbound->called, item, sizeof(inbound->called));

		for (item = strtok_r(NULL, OSP_USER_DELIM, &tmp); item; item = strtok_r(NULL, OSP_USER_DELIM, &tmp)) {
			if (!strncasecmp(item, "rn=", 3)) {
				switch_copy_string(inbound->nprn, item + 3, sizeof(inbound->nprn));
			} else if (!strncasecmp(item, "cic=", 4)) {
				switch_copy_string(inbound->npcic, item + 4, sizeof(inbound->npcic));
			} else if (!strcasecmp(item, "npdi")) {
				inbound->npdi = 1;
			}
		}
	}
}

/*
 * Parse SIP Diversion
 * param diversion SIP Diversion header
 * param inbound Inbound info
 * return
 */
static void osp_parse_diversion(
	const char *diversion,
	osp_inbound_t *inbound)
{
	char buffer[OSP_SIZE_NORSTR];
	char userinfo[OSP_SIZE_NORSTR];
	char *head;
	char *tmp;

	if (switch_strlen_zero(diversion)) {
		/* It is not necessary to clean inbound */
		return;
	}

	switch_copy_string(buffer, diversion, sizeof(buffer));

	if ((head = strstr(buffer, "sip:"))) {
		head += 4;
		if ((tmp = strchr(head, OSP_URI_DELIM))) {
			*tmp = '\0';
			switch_copy_string(userinfo, head, sizeof(userinfo));
			osp_parse_userinfo(userinfo, inbound->divuser, sizeof(inbound->divuser), NULL);

			head = tmp + 1;
			tmp = strtok(head, OSP_HOST_DELIM);
			switch_copy_string(inbound->divhost, tmp, sizeof(inbound->divhost));
		}
	}
}

/*
 * Log AuthReq parameters
 * param provider OSP provider
 * param inbound Inbound info
 * return
 */
static void osp_log_authreq(
	osp_provider_t *provider,
	osp_inbound_t *inbound)
{
	char *srvtype;
	char term[OSP_SIZE_NORSTR];
	int total;

	if (osp_globals.debug) {
		if (provider->srvtype == OSP_SRV_NPQUERY) {
			srvtype = "npquery";
			if (switch_strlen_zero(inbound->tohost)) {
				switch_copy_string(term, provider->device, sizeof(term));
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
			total = provider->maxdest;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, osp_globals.loglevel, 
			"AuthReq: "
			"srvtype = '%s' "
			"source = '%s' "
			"srcdev = '%s' "
			"calling = '%s' "
			"called = '%s' "
			"lnp = '%s/%s/%d' "
			"prefer = '%s' "
			"div = '%s/%s' "
			"srcnid = '%s' "
			"cinfo = '%s/%s/%s/%s/%s/%s/%s/%s' "
			"maxcount = '%d'\n",
			srvtype,
			provider->device,
			inbound->srcdev,
			inbound->calling,
			inbound->called,
			inbound->nprn, inbound->npcic, inbound->npdi,
			term,
			inbound->divuser, inbound->divhost,
			osp_filter_null(inbound->srcnid),
			osp_filter_null(inbound->cinfo[0]), osp_filter_null(inbound->cinfo[1]),
			osp_filter_null(inbound->cinfo[2]), osp_filter_null(inbound->cinfo[3]),
			osp_filter_null(inbound->cinfo[4]), osp_filter_null(inbound->cinfo[5]),
			osp_filter_null(inbound->cinfo[6]), osp_filter_null(inbound->cinfo[7]),
			total);
	}
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
	switch_channel_timetable_t *times;
	const char *tmp;
	int i;
	char name[OSP_SIZE_NORSTR];

	memset(inbound, 0, sizeof(*inbound));

	caller = switch_channel_get_caller_profile(channel);
	inbound->srcdev = caller->network_addr;
	inbound->calling = caller->caller_id_number;
	osp_parse_userinfo(caller->destination_number, NULL, 0, inbound);

	inbound->tohost = switch_channel_get_variable(channel, OSP_FS_TOHOST);
	inbound->toport = switch_channel_get_variable(channel, OSP_FS_TOPORT);

	if ((tmp = switch_channel_get_variable(channel, OSP_FS_DIVERSION))) {
		osp_parse_diversion(tmp, inbound);
	}

	inbound->srcnid = switch_channel_get_variable(channel, OSP_VAR_SRCNID);

	times = switch_channel_get_timetable(channel);
	inbound->start = times->created;

	for (i = 0; i < OSP_MAX_CINFO; i++) {
		switch_snprintf(name, sizeof(name), "%s%d", OSP_VAR_CUSTOMINFO, i + 1);
		inbound->cinfo[i] = switch_channel_get_variable(channel, name);
	}
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

	if (dest && (destsize > 0)) {
		if (!switch_strlen_zero(src)) {
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
		} else {
			*dest = '\0';
		}
	}
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

	if (dest && (destsize > 0)) {
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
		} else {
			*dest = '\0';
		}
	}
}

/*
 * Check destination
 * param handle Transaction handle
 * param dest Destination
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_check_destination(
	OSPTTRANHANDLE handle,
	osp_destination_t *dest)
{
	OSPE_DEST_OSPENABLED enabled;
	OSPE_DEST_PROTOCOL protocol;
	OSPE_OPERATOR_NAME type;
	int error;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if ((handle == OSP_INVALID_HANDLE) || !dest) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid parameters\n");
		return status;
	}

	dest->supported = SWITCH_FALSE;

	if ((error = OSPPTransactionIsDestOSPEnabled(handle, &enabled)) != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get destination OSP version, error '%d'\n", error);
		return status;
	}

	if ((error = OSPPTransactionGetDestProtocol(handle, &protocol)) != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get destination protocol, error '%d'\n", error);
		return status;
	}

	switch(protocol) {
	case OSPC_DPROT_UNDEFINED:
	case OSPC_DPROT_UNKNOWN:
		protocol = osp_globals.protocol;
	case OSPC_DPROT_SIP:
	case OSPC_DPROT_Q931:
	case OSPC_DPROT_IAX:
	case OSPC_DPROT_SKYPE:
		dest->protocol = protocol;
		if (!switch_strlen_zero(osp_globals.module[protocol]) && !switch_strlen_zero(osp_globals.profile[protocol])) {
			dest->supported = SWITCH_TRUE;
			status = SWITCH_STATUS_SUCCESS;
		}
		break;
	case OSPC_DPROT_LRQ:
	case OSPC_DPROT_T37:
	case OSPC_DPROT_T38:
	case OSPC_DPROT_SMPP:
	case OSPC_DPROT_XMPP:
	default:
		dest->protocol = protocol;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported protocol '%d'\n", protocol);
		break;
	}

	if ((error = OSPPTransactionGetDestinationNetworkId(handle, sizeof(dest->destnid), dest->destnid)) != OSPC_ERR_NO_ERROR) {
		dest->destnid[0] = '\0';
	}

	error = OSPPTransactionGetNumberPortabilityParameters(handle,
		sizeof(dest->nprn),
		dest->nprn,
		sizeof(dest->npcic),
		dest->npcic,
		&dest->npdi);
	if (error != OSPC_ERR_NO_ERROR) {
		dest->nprn[0] = '\0';
		dest->npcic[0] = '\0';
		dest->npdi = 0;
	}

	for (type = OSPC_OPNAME_START; type < OSPC_OPNAME_NUMBER; type++) {
		if ((error = OSPPTransactionGetOperatorName(handle, type, sizeof(dest->opname[type]), dest->opname[type])) != OSPC_ERR_NO_ERROR) {
			dest->opname[type][0] = '\0';
		}
	}

	return status;
}

/*
 * Log AuthRsp parameters
 * param results Routing info
 * return
 */
static void osp_log_authrsp(
	osp_results_t *results)
{
	int i;

	if (osp_globals.debug) {
		for (i = 0; i < results->numdest; i++) {
			switch_log_printf(SWITCH_CHANNEL_LOG, osp_globals.loglevel, 
				"AuthRsp: "
				"transid = '%llu' "
				"destcount = '%d' "
				"timelimit = '%u' "
				"destination = '%s' "
				"calling = '%s' "
				"called = '%s' "
				"destnid = '%s' "
				"lnp = '%s/%s/%d' "
				"protocol = '%s' "
				"supported = '%d'\n",
				results->transid,
				i + 1,
				results->dests[i].timelimit,
				results->dests[i].dest,
				results->dests[i].calling,
				results->dests[i].called,
				results->dests[i].destnid,
				results->dests[i].nprn, results->dests[i].npcic, results->dests[i].npdi,
				osp_get_protocol(results->dests[i].protocol),
				results->dests[i].supported);
		}
	}
}

/*
 * Do auth/routing request
 * param provider OSP provider
 * param handle Transaction handle
 * param source Call originator info
 * param results Routing info
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_do_request(
	osp_provider_t *provider,
	OSPTTRANHANDLE handle,
	osp_inbound_t *inbound,
	osp_results_t *results)
{
	OSPTTRANS *context;
	osp_destination_t *dest;
	char tmp[OSP_SIZE_NORSTR];
	char src[OSP_SIZE_NORSTR];
	char dev[OSP_SIZE_NORSTR];
	char term[OSP_SIZE_NORSTR];
	const char *preferred[2] = { NULL };
	unsigned int callidlen = 0, tokenlen = 0, total;
	int count, error;
	switch_status_t status = SWITCH_STATUS_FALSE;

	osp_log_authreq(provider, inbound);

	OSPPTransactionSetNumberPortability(handle, inbound->nprn, inbound->npcic, inbound->npdi);

	osp_convert_inout(inbound->divhost, tmp, sizeof(tmp));
	OSPPTransactionSetDiversion(handle, inbound->divuser, tmp);

	OSPPTransactionSetNetworkIds(handle, inbound->srcnid, NULL);

	for (count = 0; count < OSP_MAX_CINFO; count++) {
		if (!switch_strlen_zero(inbound->cinfo[count])) {
			OSPPTransactionSetCustomInfo(handle, count, inbound->cinfo[count]);
		}
	}

	osp_convert_inout(provider->device, src, sizeof(src));
	osp_convert_inout(inbound->srcdev, dev, sizeof(dev));
	if (provider->srvtype == OSP_SRV_NPQUERY) {
		OSPPTransactionSetServiceType(handle, OSPC_SERVICE_NPQUERY);

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

		total = 1;
	} else {
		OSPPTransactionSetServiceType(handle, OSPC_SERVICE_VOICE);

		total = provider->maxdest;
	}

	error = OSPPTransactionRequestAuthorisation(
		handle,				/* Transaction handle */
		src,				/* Source */
		dev,				/* Source device */
		inbound->calling,	/* Calling */
		OSPC_NFORMAT_E164,	/* Calling format */
		inbound->called,	/* Called */
		OSPC_NFORMAT_E164,	/* Called format */
		NULL,				/* User */
		0,					/* Number of callids */
		NULL,				/* Callids */
		preferred,			/* Preferred destinations */
		&total,				/* Destination number */
		NULL,				/* Log buffer size */
		NULL);				/* Log buffer */
	if (error != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to request routing, error '%d'\n", error);
		results->numdest = 0;
		return status;
	} else if (!total) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Without destination\n");
		results->numdest = 0;
		return status;
	}

	context = OSPPTransactionGetContext(handle, &error);
	if (error == OSPC_ERR_NO_ERROR) {
		results->transid = context->TransactionID;
	} else {
		results->transid = 0;
	}

	switch_copy_string(results->called, inbound->called, sizeof(results->called));
	results->srcnid = inbound->srcnid;
	results->start = inbound->start;

	count = 0;
	dest = &results->dests[count];
	error = OSPPTransactionGetFirstDestination(
		handle,					/* Transaction handle */
		0,						/* Timestamp buffer size */
		NULL,					/* Valid after */
		NULL,					/* Valid until */
		&dest->timelimit,		/* Call duration limit */
		&callidlen,				/* Callid buffer size */
		NULL,					/* Callid buffer */
		sizeof(dest->called),	/* Called buffer size */
		dest->called,			/* Called buffer */
		sizeof(dest->calling),	/* Calling buffer size */
		dest->calling,			/* Calling buffer */
		sizeof(term),			/* Destination buffer size */
		term,					/* Destination buffer */
		0,						/* Destination device buffer size */
		NULL,					/* Destination device buffer */
		&tokenlen,				/* Token buffer length */
		NULL);					/* Token buffer */
	if (error != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get first destination, error '%d'\n", error);
		return status;
	}

	osp_convert_outin(term, dest->dest, sizeof(dest->dest));
	osp_check_destination(handle, dest);
	dest->userphone = provider->userphone;

	for (count = 1; count < total; count++) {
		dest = &results->dests[count];
		error = OSPPTransactionGetNextDestination(
			handle,					/* Transsaction handle */
			OSPC_FAIL_NONE,			/* Failure reason */
			0,						/* Timestamp buffer size */
			NULL,					/* Valid after */
			NULL,					/* Valid until */
			&dest->timelimit,		/* Call duration limit */
			&callidlen,				/* Callid buffer size */
			NULL,					/* Callid buffer */
			sizeof(dest->called),	/* Called buffer size */
			dest->called,			/* Called buffer */
			sizeof(dest->calling),	/* Calling buffer size */
			dest->calling,			/* Calling buffer */
			sizeof(term),			/* Destination buffer size */
			term,					/* Destination buffer */
			0,						/* Destination device buffer size */
			NULL,					/* Destination device buffer */
			&tokenlen,				/* Token buffer length */
			NULL);					/* Token buffer */
		if (error == OSPC_ERR_NO_ERROR) {
			osp_convert_outin(term, dest->dest, sizeof(dest->dest));
			osp_check_destination(handle, dest);
			dest->userphone = provider->userphone;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get destination, error '%d'\n", error);
			break;
		}
	}
	if (count == total) {
		results->numdest = total;
		osp_log_authrsp(results);
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

/*
 * Request auth/routing info
 * param channel Originator channel
 * param profile Provider name
 * param results Routing info
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_request_routing(
	switch_channel_t *channel,
	const char *profile,
	osp_results_t *results)
{
	osp_provider_t *provider;
	OSPTTRANHANDLE handle;
	osp_inbound_t inbound;
	int error;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (osp_find_provider(profile, &provider) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find provider '%s'\n", profile);
		return status;
	}

	if (provider->handle == OSP_INVALID_HANDLE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Disabled provider '%s'\n", profile);
		return status;
	}

	results->profile = profile;

	if ((error = OSPPTransactionNew(provider->handle, &handle)) != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create transaction handle, error '%d'\n", error);
		return status;
	}

	osp_get_inbound(channel, &inbound);

	status = osp_do_request(provider, handle, &inbound, results);

	OSPPTransactionDelete(handle);

	return status;
}

/*
 * Build parameter string for all channels
 * param results Routing results
 * param buffer Buffer
 * param bufsize Buffer size
 * return
 */
static void osp_build_allparam(
	osp_results_t *results,
	char *buffer,
	switch_size_t bufsize)
{
	char *head = buffer;
	switch_size_t len, size = bufsize;

	if (results && head && size) {
		switch_snprintf(head, size,
			"{%s=%s,%s=%llu,%s=%llu",
			OSP_VAR_PROFILE, results->profile,
			OSP_VAR_TRANSID, results->transid,
			OSP_VAR_START, results->start);
		osp_adjust_len(head, size, len);

		if (!switch_strlen_zero(results->srcnid)) {
			switch_snprintf(head, size, ",%s=%s", OSP_VAR_SRCNID, results->srcnid);
		}
		osp_adjust_len(head, size, len);

		switch_snprintf(head, size, "}");
		osp_adjust_len(head, size, len);
	}
}

/*
 * Build parameter string for each channel
 * param count Destination count
 * param dest Destination
 * param buffer Buffer
 * param bufsize Buffer size
 * return
 */
static void osp_build_eachparam(
	int count,
	osp_destination_t *dest,
	char *buffer,
	switch_size_t bufsize)
{
	char *head = buffer;
	switch_size_t len, size = bufsize;

	if ((count > 0) && dest && head && size) {
		switch_snprintf(buffer, bufsize,
			"[%s=%d,%s=%s,%s=%s",
			OSP_VAR_DESTCOUNT, count,
			OSP_VAR_DESTIP, dest->dest,
			OSP_FS_OUTCALLING, dest->calling);
		osp_adjust_len(head, size, len);

		if (!switch_strlen_zero_buf(dest->destnid)) {
			switch_snprintf(head, size, ",%s=%s", OSP_VAR_DESTNID, dest->destnid);
		}
		osp_adjust_len(head, size, len);

		switch_snprintf(head, size, "]");
		osp_adjust_len(head, size, len);
	}
}

/*
 * Build endpoint string
 * param dest Destination
 * param buffer Buffer
 * param bufsize Buffer size
 * return
 */
static void osp_build_endpoint(
	osp_destination_t *dest,
	char *buffer,
	switch_size_t bufsize)
{
	char *head = buffer;
	switch_size_t len, size = bufsize;

	if (head && size) {
		switch (dest->protocol) {
		case OSPC_DPROT_SIP:
			switch_snprintf(head, size, "%s/%s/%s", osp_globals.module[OSPC_DPROT_SIP], osp_globals.profile[OSPC_DPROT_SIP], dest->called);
			osp_adjust_len(head, size, len);
			
			if (!switch_strlen_zero_buf(dest->nprn)) {
				switch_snprintf(head, size, ";rn=%s", dest->nprn);
				osp_adjust_len(head, size, len);
			}
			if (!switch_strlen_zero_buf(dest->npcic)) {
				switch_snprintf(head, size, ";cic=%s", dest->npcic);
				osp_adjust_len(head, size, len);
			}
			if (dest->npdi) {
				switch_snprintf(head, size, ";npdi");
				osp_adjust_len(head, size, len);
			}

			switch_snprintf(head, size, "@%s", dest->dest);
			osp_adjust_len(head, size, len);

			if (dest->userphone) {
				switch_snprintf(head, size, ";user=phone");
				osp_adjust_len(head, size, len);
			}
			break;
		case OSPC_DPROT_Q931:
			switch_snprintf(head, size, "%s/%s/%s@%s", osp_globals.module[OSPC_DPROT_Q931], osp_globals.profile[OSPC_DPROT_Q931], dest->called, dest->dest);
			osp_adjust_len(head, size, len);
			break;
		case OSPC_DPROT_IAX:
			switch_snprintf(head, size, "%s/%s/%s/%s", osp_globals.module[OSPC_DPROT_Q931], osp_globals.profile[OSPC_DPROT_Q931], dest->dest, dest->called);
			osp_adjust_len(head, size, len);
			break;
		case OSPC_DPROT_SKYPE:
			switch_snprintf(head, size, "%s/%s/%s", osp_globals.module[OSPC_DPROT_Q931], osp_globals.profile[OSPC_DPROT_Q931], dest->called);
			osp_adjust_len(head, size, len);
			break;
		default:
			buffer[0] = '\0';
			break;
		}
	}
}

/*
 * Create route string
 * param channel Originator channel
 * param numdest Numer of destinations
 * param destlist Destinations
 * return
 */
static void osp_create_route(
	switch_channel_t *channel,
	osp_results_t *results)
{
	osp_destination_t *dest;
	char name[OSP_SIZE_NORSTR];
	char value[OSP_SIZE_NORSTR];
	char eachparam[OSP_SIZE_NORSTR];
	char endpoint[OSP_SIZE_NORSTR];
	char buffer[OSP_SIZE_ROUSTR];
	int i, len, count, size = sizeof(buffer);
	char *head = buffer;
	switch_event_header_t *hi;
	char *var;

	/* Cleanup OSP varibales in originator */
	if ((hi = switch_channel_variable_first(channel))) {
		for (; hi; hi = hi->next) {
			var = hi->name;
			if (var && !strncmp(var, "osp_", 4)) {
				switch_channel_set_variable(channel, var, NULL);
			}
		}
		switch_channel_variable_last(channel);
	}

	osp_build_allparam(results, head, size);
	osp_adjust_len(head, size, len);

	for (count = 0, i = 0; i < results->numdest; i++) {
		dest = &results->dests[i];
		if (dest->supported) {
			count++;
			osp_build_eachparam(i + 1, dest, eachparam, sizeof(eachparam));
			osp_build_endpoint(dest, endpoint, sizeof(endpoint));
			switch_snprintf(name, sizeof(name), "%s%d", OSP_VAR_ROUTEPRE, count);
			switch_snprintf(value, sizeof(value), "%s%s", eachparam, endpoint);
			switch_channel_set_variable_var_check(channel, name, value, SWITCH_FALSE);
			switch_snprintf(head, size, "%s|", value);
			osp_adjust_len(head, size, len);
		}
	}

	switch_snprintf(value, sizeof(value), "%d", count);
	switch_channel_set_variable_var_check(channel, OSP_VAR_ROUTECOUNT, value, SWITCH_FALSE);

	if (count) {
		*(buffer + strlen(buffer) - 1) = '\0';
	} else {
		buffer[0] = '\0';
	}
	switch_channel_set_variable_var_check(channel, OSP_VAR_AUTOROUTE, buffer, SWITCH_FALSE);
}

/*
 * Macro expands to:
 * static void osp_app_function(switch_core_session_t *session, const char *data)
 */
SWITCH_STANDARD_APP(osp_app_function)
{
	int argc = 0;
	char *argv[2] = { 0 };
	char *params = NULL;
	char *profile = NULL;
	switch_channel_t *channel;
	osp_results_t results;
	switch_status_t retval;

	if (osp_globals.shutdown) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "OSP application inavailable\n");
		return;
	}

	/* Make sure there is a valid channel when starting the OSP application */
	if (!(channel = switch_core_session_get_channel(session))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to find origiantor channel\n");
		return;
	}

	if (!(params = switch_core_session_strdup(session, data))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to alloc parameters\n");
		return;
	}

	if ((argc = switch_separate_string(params, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		profile = argv[0];
	} else {
		profile = OSP_DEF_PROFILE;
	}

	retval = osp_request_routing(channel, profile, &results);
	if (retval == SWITCH_STATUS_SUCCESS) {
		osp_create_route(channel, &results);
	}
}

/*
 * Add application
 * param session
 * param channel Originator channel
 * param extenstion
 * param results OSP routing info
 * return
 */
static void osp_add_application(
	switch_core_session_t *session,
	switch_channel_t *channel,
	switch_caller_extension_t **extension,
	osp_results_t *results)
{
	osp_destination_t *dest;
	char allparam[OSP_SIZE_NORSTR];
	char eachparam[OSP_SIZE_NORSTR];
	char endpoint[OSP_SIZE_NORSTR];
	char buffer[OSP_SIZE_ROUSTR];
	int i;

	if ((*extension = switch_caller_extension_new(session, results->called, results->called)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to create extension\n");
		return;
	}

	switch_channel_set_variable(channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE, "true");

	osp_build_allparam(results, allparam, sizeof(allparam));

	for (i = 0; i < results->numdest; i++) {
		dest = &results->dests[i];
		if (dest->supported) {
			osp_build_eachparam(i + 1, dest, eachparam, sizeof(eachparam));
			osp_build_endpoint(dest, endpoint, sizeof(endpoint));
			switch_snprintf(buffer, sizeof(buffer), "%s%s%s", allparam, eachparam, endpoint);
			switch_caller_extension_add_application(session, *extension, "bridge", buffer);
		}
	}
}

/*
 * Macro expands to:
 * switch_caller_extension_t * osp_dialplan_function(switch_core_session_t *session, void *arg, switch_caller_profile_t *caller_profile)
 */
SWITCH_STANDARD_DIALPLAN(osp_dialplan_function)
{
	int argc = 0;
	char *argv[2] = { 0 };
	char *profile = NULL;
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	osp_results_t results;
	switch_status_t retval;

	if (osp_globals.shutdown) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "OSP dialplan inavailable\n");
		return extension;
	}

	if ((argc = switch_separate_string((char *)arg, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		profile = argv[0];
	} else {
		profile = OSP_DEF_PROFILE;
	}

	retval = osp_request_routing(channel, profile, &results);
	if (retval == SWITCH_STATUS_SUCCESS) {
		osp_add_application(session, channel, &extension, &results);
	}

	return extension;
}

/*
 * Retrieve OSP cookie
 * param channel Destination channel
 * param cookie OSP cookie
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_get_ospcookie(
	switch_channel_t *channel,
	osp_cookie_t *cookie)
{
	const char *strvar;

	if (!(cookie->profile = switch_channel_get_variable(channel, OSP_VAR_PROFILE))) {
		return SWITCH_STATUS_FALSE;
	}

	if ((strvar = switch_channel_get_variable(channel, OSP_VAR_TRANSID))) {
		cookie->transid = atoll(strvar);
	} else {
		cookie->transid = 0;
	}

	if ((strvar = switch_channel_get_variable(channel, OSP_VAR_START))) {
		cookie->start = atoll(strvar);
	} else {
		cookie->start = 0;
	}

	if ((strvar = switch_channel_get_variable(channel, OSP_VAR_DESTCOUNT))) {
		cookie->destcount = atoi(strvar);
	} else {
		cookie->destcount = 0;
	}
	
	cookie->dest = switch_channel_get_variable(channel, OSP_VAR_DESTIP);

	cookie->srcnid = switch_channel_get_variable(channel, OSP_VAR_SRCNID);
	cookie->destnid = switch_channel_get_variable(channel, OSP_VAR_DESTNID);

	return SWITCH_STATUS_SUCCESS;
}

/*
 * Retrieve usage info
 * param channel Destination channel
 * param originator Originator channel
 * param cookie OSP cookie
 * param usage Usage info
 * return
 */
static void osp_get_usage(
	switch_channel_t *channel,
	switch_caller_profile_t *originator,
	osp_cookie_t *cookie,
	osp_usage_t *usage)
{
	const char *strvar;
	switch_caller_profile_t *terminator;
	switch_channel_timetable_t *times;

	memset(usage, 0, sizeof(*usage));

	usage->callid = switch_channel_get_variable(channel, OSP_FS_OUTCALLID);
	if (switch_strlen_zero(usage->callid)) {
		usage->callid = OSP_DEF_CALLID;
	}

	/* Originator had been checked by osp_on_reporting */
	if (originator) {
		usage->srcdev = originator->network_addr;
		usage->calling = originator->caller_id_number;
		osp_parse_userinfo(originator->destination_number, usage->called, sizeof(usage->called), NULL);
	}

	terminator = switch_channel_get_caller_profile(channel);
	if (!strcasecmp(terminator->source, OSP_MODULE_SIP)) {
		usage->protocol = OSPC_DPROT_SIP;
		strvar = switch_channel_get_variable(channel, OSP_FS_SIPRELEASE);
		if (!strvar || !strcasecmp(strvar, "recv_bye")) {
			usage->release = 1;
		}
	} else if (!strcasecmp(terminator->source, OSP_MODULE_H323)) {
		usage->protocol = OSPC_DPROT_Q931;
	} else if (!strcasecmp(terminator->source, OSP_MODULE_IAX)) {
		usage->protocol = OSPC_DPROT_IAX;
	} else if (!strcasecmp(terminator->source, OSP_MODULE_SKYPE)) {
		usage->protocol = OSPC_DPROT_SKYPE;
	} else {
		usage->protocol = OSPC_DPROT_UNKNOWN;
	}

	usage->cause = switch_channel_get_cause_q850(channel);

	times = switch_channel_get_timetable(channel);
	usage->alert = times->progress;
	usage->connect = times->answered;
	usage->end = times->hungup;
	if (times->answered) {
		usage->duration = times->hungup - times->answered;
		usage->pdd = times->answered - cookie->start;
	}

	usage->fcodec = switch_channel_get_variable(channel, OSP_FS_DOWNCODEC);
	usage->rcodec = switch_channel_get_variable(channel, OSP_FS_UPCODEC);
	if ((strvar = switch_channel_get_variable(channel, OSP_FS_RTPDOWNOCTS))) {
		usage->rtpdownoctets = atoi(strvar);
	} else {
		usage->rtpdownoctets = OSP_DEF_STATS;
	}
	if ((strvar = switch_channel_get_variable(channel, OSP_FS_RTPUPOCTS))) {
		usage->rtpupoctets = atoi(strvar);
	} else {
		usage->rtpupoctets = OSP_DEF_STATS;
	}
	if ((strvar = switch_channel_get_variable(channel, OSP_FS_RTPDOWNPKTS))) {
		usage->rtpdownpackets = atoi(strvar);
	} else {
		usage->rtpdownpackets = OSP_DEF_STATS;
	}
	if ((strvar = switch_channel_get_variable(channel, OSP_FS_RTPUPPKTS))) {
		usage->rtpuppackets = atoi(strvar);
	} else {
		usage->rtpuppackets = OSP_DEF_STATS;
	}
}

/*
 * Report OSP usage thread function
 * param threadarg Thread argments
 * return
 */
static OSPTTHREADRETURN osp_report_thread(
	void *threadarg)
{
	int i, error;
	osp_threadarg_t *info;

	info = (osp_threadarg_t *)threadarg;

	OSPPTransactionRecordFailure(info->handle, info->cause);

	for (i = 0; i < 3; i++) {
		error = OSPPTransactionReportUsage(
			info->handle,	/* Transaction handle */
			info->duration,	/* Call duration */
			info->start,	/* Call start time */
			info->end,		/* Call end time */
			info->alert,	/* Call alert time */
			info->connect,	/* Call connect time */
			info->pdd != 0,	/* Post dial delay present */
			info->pdd,		/* Post dial delay */
			info->release,	/* Release source */
			NULL,			/* Conference ID */
			-1,				/* Packets not received by peer */
			-1,				/* Fraction of packets not received by peer */
			-1,				/* Packets not received that were expected */
			-1,				/* Fraction of packets expected but not received */
			NULL,			/* Log buffer size */
			NULL);			/* Log buffer */
		if (error != OSPC_ERR_NO_ERROR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				"Failed to report usage for '%llu' attempt '%d'\n",
				info->transid,
				i + 1);
	} else {
			break;
		}
	}

	OSPPTransactionDelete(info->handle);

	switch_safe_free(info);

	OSPTTHREADRETURN_NULL();
}

/*
 * Report usage
 * param cookie OSP cookie
 * param usage Usage
 * return SWITCH_STATUS_SUCCESS Successful, SWITCH_STATUS_FALSE Failed
 */
static switch_status_t osp_report_usage(
	osp_cookie_t *cookie,
	osp_usage_t *usage)
{
	osp_provider_t *provider;
	OSPTTRANHANDLE handle;
	osp_threadarg_t *info;
	OSPTTHREADID threadid;
	OSPTTHRATTR threadattr;
	int error;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (osp_find_provider(cookie->profile, &provider) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find provider '%s'\n", cookie->profile);
		return status;
	}

	if ((error = OSPPTransactionNew(provider->handle, &handle)) != OSPC_ERR_NO_ERROR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create transaction handle, error '%d'\n", error);
		return status;
	}

	error = OSPPTransactionBuildUsageFromScratch(
		handle,					/* Transaction handle */
		cookie->transid,		/* Transaction ID */
		OSPC_ROLE_SOURCE,		/* CDR type, source */
		provider->device,		/* Source */
		cookie->dest,			/* Destination */
		usage->srcdev,			/* Source device */
		OSP_DEF_STRING,			/* Destination device */
		usage->calling,			/* Calling */
		OSPC_NFORMAT_E164,		/* Calling format */
		usage->called,			/* Called */
		OSPC_NFORMAT_E164,		/* Called format */
		strlen(usage->callid),	/* Size of Call-ID */
		usage->callid,			/* Call-ID */
		0,						/* Failure reason */
		NULL,					/* Log buffer size */
		NULL);					/* Log buffer */
	if (error != OSPC_ERR_NO_ERROR) {
		OSPPTransactionDelete(handle);
		return status;
	}

	status = SWITCH_STATUS_SUCCESS;

	OSPPTransactionSetDestinationCount(handle, cookie->destcount);

	if (!switch_strlen_zero(cookie->srcnid)) {
		OSPPTransactionSetSrcNetworkId(handle, cookie->srcnid);
	}

	if (!switch_strlen_zero(cookie->destnid)) {
		OSPPTransactionSetDestNetworkId(handle, cookie->destnid);
	}

	if (!switch_strlen_zero(usage->fcodec)) {
		OSPPTransactionSetForwardCodec(handle, usage->fcodec);
	}
	if (!switch_strlen_zero(usage->rcodec)) {
		OSPPTransactionSetReverseCodec(handle, usage->rcodec);
	}

	if (usage->rtpdownoctets != OSP_DEF_STATS) {
		OSPPTransactionSetOctets(handle, OSPC_SMETRIC_RTP, OSPC_SFLOW_DOWNSTREAM, usage->rtpdownoctets);
	}
	if (usage->rtpupoctets != OSP_DEF_STATS) {
		OSPPTransactionSetOctets(handle, OSPC_SMETRIC_RTP, OSPC_SFLOW_UPSTREAM, usage->rtpupoctets);
	}
	if (usage->rtpdownpackets != OSP_DEF_STATS) {
		OSPPTransactionSetPackets(handle, OSPC_SMETRIC_RTP, OSPC_SFLOW_DOWNSTREAM, usage->rtpdownpackets);
	}
	if (usage->rtpuppackets != OSP_DEF_STATS) {
		OSPPTransactionSetPackets(handle, OSPC_SMETRIC_RTP, OSPC_SFLOW_UPSTREAM, usage->rtpuppackets);
	}

	info = (osp_threadarg_t *)malloc(sizeof(osp_threadarg_t));
	info->handle = handle;
	info->transid = cookie->transid;
	info->cause = usage->cause;
	info->start = cookie->start / 1000000;
	info->alert = usage->alert / 1000000;
	info->connect = usage->connect / 1000000;
	info->end = usage->end / 1000000;
	info->duration = usage->duration / 1000000;
	info->pdd = usage->pdd / 1000000;
	info->release = usage->release;

	OSPM_THRATTR_INIT(threadattr, error);
	OSPM_SETDETACHED_STATE(threadattr, error);
	OSPM_CREATE_THREAD(threadid, &threadattr, osp_report_thread, info, error);
	OSPM_THRATTR_DESTROY(threadattr);

	/* handle and info will be released by osp_report_thread */

	return status;
}

/*
 * Log UsageInd parameters
 * param cookie OSP cookie
 * param usage Usage info
 * return
 */
static void osp_log_usageind(
	osp_cookie_t *cookie,
	osp_usage_t *usage)
{
	if (osp_globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, osp_globals.loglevel,
			"UsageInd: "
			"transid = '%llu' "
			"destcount = '%d' "
			"callid = '%s' "
			"calling = '%s' "
			"called = '%s' "
			"srcdev = '%s' "
			"dest = '%s' "
			"nid = '%s/%s' "
			"protocol = '%s' "
			"cause = '%d' "
			"release = '%s' "
			"times = '%llu/%llu/%llu/%llu' "
			"duration = '%llu' "
			"pdd = '%llu' "
			"outsessionid = '%s' "
			"codec = '%s/%s' "
			"rtpctets = '%d/%d' "
			"rtppackets = '%d/%d'\n",
			cookie->transid,
			cookie->destcount,
			usage->callid,
			usage->calling,
			usage->called,
			usage->srcdev,
			cookie->dest,
			osp_filter_null(cookie->srcnid), osp_filter_null(cookie->destnid),
			osp_get_protocol(usage->protocol),
			usage->cause,
			usage->release ? "term" : "orig",
			cookie->start / 1000000, usage->alert / 1000000, usage->connect / 1000000, usage->end / 1000000,
			usage->duration / 1000000,
			usage->pdd / 1000000,
			usage->callid,
			osp_filter_null(usage->fcodec), osp_filter_null(usage->rcodec),
			usage->rtpdownoctets, usage->rtpupoctets,
			usage->rtpdownpackets, usage->rtpuppackets);
	}
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
	osp_cookie_t cookie;
	osp_usage_t usage;
	switch_caller_profile_t *originator;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (osp_globals.shutdown) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "OSP application inavailable\n");
		return status;
	}

	/* Only report for B-leg */
	if (!(channel = switch_core_session_get_channel(session)) || !(originator = switch_channel_get_originator_caller_profile(channel))) {
		return status;
	}

	if (osp_get_ospcookie(channel, &cookie) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	osp_get_usage(channel, originator, &cookie, &usage);

	osp_log_usageind(&cookie, &usage);

	osp_report_usage(&cookie, &usage);

	return status;
}

/*
 * OSP module state handlers
 */
static switch_state_handler_table_t state_handlers = {
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

/*
 * Macro expands to:
 * switch_status_t mod_osp_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_osp_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_dialplan_interface_t *dp_interface;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	/* Load OSP configuration */
	if ((status = osp_load_settings(pool)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/* Init OSP Toolkit */
	osp_init_osptk();

	/* Connect OSP internal structure to the blank pointer passed to OSP module */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* Add CLI OSP command */
	SWITCH_ADD_API(api_interface, "osp", "OSP", osp_api_function, "status");
	switch_console_set_complete("add osp status");

	/* Add OSP application */
	SWITCH_ADD_APP(app_interface, "osp", "Perform an OSP lookup", "Perform an OSP lookup", osp_app_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	/* Add OSP dialplan */
	SWITCH_ADD_DIALPLAN(dp_interface, "osp", osp_dialplan_function);

	/* Add OSP state handlers */
	switch_core_add_state_handler(&state_handlers);

	/* Indicate that the module should continue to be loaded */
	return status;
}

/*
 * Cleanup OSP client end
 * return
 */
static void osp_cleanup_osptk(void)
{
	osp_provider_t *provider;

	for (provider = osp_providers; provider; provider = provider->next) {
		OSPPProviderDelete(provider->handle, 0);
		provider->handle = OSP_INVALID_HANDLE;
	}

	OSPPCleanup();
}

/*
 * Called when the system shuts down
 * Macro expands to:
 * switch_status_t mod_osp_shutdown(void)
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_osp_shutdown)
{
	/* Shutdown OSP module */
	osp_globals.shutdown = SWITCH_TRUE;

	/* Cleanup OSP Toolkit */
	osp_cleanup_osptk();

	/* Remoeve OSP state handlers */
	switch_core_remove_state_handler(&state_handlers);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */

