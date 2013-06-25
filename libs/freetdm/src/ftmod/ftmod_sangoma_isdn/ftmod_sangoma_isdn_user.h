/*
 * Copyright (c) 2010, Sangoma Technologies 
 * David Yat Sin <davidy@sangoma.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __FTMOD_SANGOMA_ISDN_USER_H__
#define __FTMOD_SANGOMA_ISDN_USER_H__


#define SNGISDN_ENUM_NAMES(_NAME, _STRINGS) static const char * _NAME [] = { _STRINGS , NULL };
#define SNGISDN_STR2ENUM_P(_FUNC1, _FUNC2, _TYPE) _TYPE _FUNC1 (const char *name); const char * _FUNC2 (_TYPE type);
#define SNGISDN_STR2ENUM(_FUNC1, _FUNC2, _TYPE, _STRINGS, _MAX)			\
	_TYPE _FUNC1 (const char *name)										\
	{																	\
		int i;															\
		_TYPE t = _MAX ;												\
																		\
		for (i = 0; i < _MAX ; i++) {									\
			if (!strcasecmp(name, _STRINGS[i])) {						\
				t = (_TYPE) i;											\
				break;													\
			}															\
		}																\
		return t;														\
	}																	\
	const char * _FUNC2 (_TYPE type)									\
	{																	\
		if (type > _MAX) {												\
			type = _MAX;												\
		}																\
		return _STRINGS[(int)type];										\
	}																	\


typedef enum {
	/* Call is not end-to-end ISDN */
	SNGISDN_PROGIND_DESCR_NETE_ISDN,
	/* Destination address is non-ISDN */
	SNGISDN_PROGIND_DESCR_DEST_NISDN,
	/* Origination address is non-ISDN */
	SNGISDN_PROGIND_DESCR_ORIG_NISDN,
	/* Call has returned to the ISDN */
	SNGISDN_PROGIND_DESCR_RET_ISDN,
	/* Interworking as occured and has resulted in a telecommunication service change */
	SNGISDN_PROGIND_DESCR_SERV_CHANGE,
	/* In-band information or an appropriate pattern is now available */
	SNGISDN_PROGIND_DESCR_IB_AVAIL,
	/* Invalid */
	SNGISDN_PROGIND_DESCR_INVALID,
} ftdm_sngisdn_progind_descr_t;
#define SNGISDN_PROGIND_DESCR_STRINGS "not-end-to-end-isdn", "destination-is-non-isdn", "origination-is-non-isdn", "call-returned-to-isdn", "service-change", "inband-info-available", "invalid"
SNGISDN_STR2ENUM_P(ftdm_str2ftdm_sngisdn_progind_descr, ftdm_sngisdn_progind_descr2str, ftdm_sngisdn_progind_descr_t);


typedef enum {
	/* User */
	SNGISDN_PROGIND_LOC_USER,
	/* Private network serving the local user */
	SNGISDN_PROGIND_LOC_PRIV_NET_LOCAL_USR,
	/* Public network serving the local user */
	SNGISDN_PROGIND_LOC_PUB_NET_LOCAL_USR,
	/* Transit network */
	SNGISDN_PROGIND_LOC_TRANSIT_NET,
	/* Public network serving remote user */
	SNGISDN_PROGIND_LOC_PUB_NET_REMOTE_USR,
	/* Private network serving remote user */
	SNGISDN_PROGIND_LOC_PRIV_NET_REMOTE_USR,
	/* Network beyond the interworking point */
	SNGISDN_PROGIND_LOC_NET_BEYOND_INTRW,
	/* Invalid */
	SNGISDN_PROGIND_LOC_INVALID,
} ftdm_sngisdn_progind_loc_t;
#define SNGISDN_PROGIND_LOC_STRINGS "user", "private-net-local-user", "public-net-local-user", "transit-network", "public-net-remote-user", "private-net-remote-user", "beyond-interworking", "invalid"
SNGISDN_STR2ENUM_P(ftdm_str2ftdm_sngisdn_progind_loc, ftdm_sngisdn_progind_loc2str, ftdm_sngisdn_progind_loc_t);

typedef enum {
	/* User Specified */
	SNGISDN_NETSPECFAC_TYPE_USER_SPEC,
	/* National network identification */
	SNGISDN_NETSPECFAC_TYPE_NATIONAL_NETWORK_IDENT,
	/* International network identification */
	SNGISDN_NETSPECFAC_TYPE_INTERNATIONAL_NETWORK_IDENT,
	/* Invalid */
	SNGISDN_NETSPECFAC_TYPE_INVALID,
} ftdm_sngisdn_netspecfac_type_t;
#define SNGISDN_NETSPECFAC_TYPE_STRINGS "user-specified", "national-network-identification", "national-network-identification", "invalid"
SNGISDN_STR2ENUM_P(ftdm_str2ftdm_sngisdn_netspecfac_type, ftdm_sngisdn_netspecfac_type2str, ftdm_sngisdn_netspecfac_type_t);

typedef enum {
	/* Unknown */
	SNGISDN_NETSPECFAC_PLAN_UNKNOWN,
	/* Carrier Identification Code */
	SNGISDN_NETSPECFAC_PLAN_CARRIER_IDENT,
	/* Data network identification code */
	SNGISDN_NETSPECFAC_PLAN_DATA_NETWORK_IDENT,
	/* Invalid */
	SNGISDN_NETSPECFAC_PLAN_INVALID,
} ftdm_sngisdn_netspecfac_plan_t;
#define SNGISDN_NETSPECFAC_PLAN_STRINGS "unknown", "carrier-identification", "data-network-identification", "invalid"
SNGISDN_STR2ENUM_P(ftdm_str2ftdm_sngisdn_netspecfac_plan, ftdm_sngisdn_netspecfac_plan2str, ftdm_sngisdn_netspecfac_plan_t);

typedef enum {
	/* Unknown */
	SNGISDN_NETSPECFAC_SPEC_ACCUNET,
	SNGISDN_NETSPECFAC_SPEC_MEGACOM,
	SNGISDN_NETSPECFAC_SPEC_MEGACOM_800,
	SNGISDN_NETSPECFAC_SPEC_SDDN,
	SNGISDN_NETSPECFAC_SPEC_INVALID,
} ftdm_sngisdn_netspecfac_spec_t;
#define SNGISDN_NETSPECFAC_SPEC_STRINGS "accunet", "megacom", "megacom-800", "sddn", "invalid"
SNGISDN_STR2ENUM_P(ftdm_str2ftdm_sngisdn_netspecfac_spec, ftdm_sngisdn_netspecfac_spec2str, ftdm_sngisdn_netspecfac_spec_t);

#endif /* __FTMOD_SANGOMA_ISDN_USER_H__*/

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

/******************************************************************************/
