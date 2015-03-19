/* 
 * H323 endpoint interface for Freeswitch Modular Media Switching Software Library /
 * Soft-Switch Application
 *
 * Version: MPL 1.1
 *
 * Copyright (c) 2010 Ilnitskiy Mixim (max.h323@gmail.com)
 * Copyright (c) 2010 Georgiewskiy Yuriy (bottleman@icf.org.ru)
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
 * Contributor(s):
 * Jan Willamowius.
 * 
 * 
 * 
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 *
 * mod_h323.cpp -- H323 endpoint
 *
 *	Version 0.0.58
*/

//#define DEBUG_RTP_PACKETS
#include "mod_h323.h"

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, mod_h323_globals.codec_string);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_context, mod_h323_globals.context);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, mod_h323_globals.dialplan);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_rtp_timer_name, mod_h323_globals.rtp_timer_name);



#define CF_NEED_FLUSH (1 << 1)
struct mod_h323_globals mod_h323_globals = { 0 };

static switch_call_cause_t create_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
                                                   switch_caller_profile_t *outbound_profile, switch_core_session_t **new_session,
                                                   switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);

static const char modulename[] = "h323";
static const char* h323_formats[] = {
	"G.711-ALaw-64k", "PCMA",
	"G.711-uLaw-64k", "PCMU",
	"GSM-06.10", "GSM",
	"G.723", "G723",
	"G.729B", "G729b",
	"G.729", "G729",
	"G.729A", "G729a",
	"G.729A/B", "G729ab",
	"G.723.1", "G723.1",
	"G.723.1(5.3k)", "G723.1-5k3",
	"G.723.1A(5.3k)", "G723.1a-5k3",
	"G.723.1A(6.3k)", "G723.1a-6k3",
	0
};

static char encodingName_COR[7] = "t38";
static char encodingName_PRE[7] = "t38pre";

void SetT38_IFP_PRE()
{
  strcpy(encodingName_COR, "t38cor");
  strcpy(encodingName_PRE, "t38");
}

const OpalMediaFormat & GetOpalT38_IFP_COR()
{
  static const OpalMediaFormat opalT38_IFP(
    "T.38-IFP-COR",
    OpalMediaFormat::DefaultDataSessionID,
    RTP_DataFrame::IllegalPayloadType,
    encodingName_COR,
    FALSE, // No jitter for data
    1440, // 100's bits/sec
    0,
    0,
    0);

  return opalT38_IFP;
}

const OpalMediaFormat & GetOpalT38_IFP_PRE()
{
  static const OpalMediaFormat opalT38_IFP(
    "T.38-IFP-PRE",
    OpalMediaFormat::DefaultDataSessionID,
    RTP_DataFrame::IllegalPayloadType,
    encodingName_PRE,
    FALSE, // No jitter for data
    1440, // 100's bits/sec
    0,
    0,
    0);

  return opalT38_IFP;
}

static switch_status_t on_hangup(switch_core_session_t *session);
static switch_status_t on_destroy(switch_core_session_t *session);

static switch_io_routines_t h323fs_io_routines = {
	/*.outgoing_channel */ create_outgoing_channel,
	/*.read_frame */ FSH323Connection::read_audio_frame,
	/*.write_frame */ FSH323Connection::write_audio_frame,
	/*.kill_channel */ FSH323Connection::kill_channel,
	/*.send_dtmf */ FSH323Connection::send_dtmf,
	/*.receive_message */ FSH323Connection::receive_message,
	/*.receive_event */ FSH323Connection::receive_event,
	/*.state_change */ FSH323Connection::state_change,
	/*.read_video_frame */ FSH323Connection::read_video_frame,
	/*.write_video_frame */ FSH323Connection::write_video_frame
};

static switch_state_handler_table_t h323fs_event_handlers = {
	/*.on_init */ FSH323Connection::on_init,
	/*.on_routing */ FSH323Connection::on_routing,
	/*.on_execute */ FSH323Connection::on_execute,
	/*.on_hangup */ on_hangup,
	/*.on_exchange_media */ FSH323Connection::on_exchange_media,
	/*.on_soft_execute */ FSH323Connection::on_soft_execute,
	/*.on_consume_media*/ NULL,
	/*.on_hibernate*/ NULL,
	/*.on_reset*/ NULL,
	/*.on_park*/ NULL,
	/*.on_reporting*/ NULL,
	/*.on_destroy*/ on_destroy
};

SWITCH_BEGIN_EXTERN_C
/*******************************************************************************/

static switch_memory_pool_t *module_pool = NULL;

SWITCH_MODULE_LOAD_FUNCTION(mod_h323_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_h323_shutdown);
SWITCH_MODULE_DEFINITION(mod_h323, mod_h323_load, mod_h323_shutdown, NULL);

SWITCH_MODULE_LOAD_FUNCTION(mod_h323_load)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Starting loading mod_h323\n");

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (!*module_interface) {
		return SWITCH_STATUS_MEMERR;
	}

	module_pool = pool;

	h323_process = new FSProcess();

	if (h323_process == NULL) {
		return SWITCH_STATUS_MEMERR;
	}

	if (h323_process->Initialise(*module_interface)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "H323 mod initialized and running\n");
#ifdef WIN32
		return SWITCH_STATUS_NOUNLOAD; /* Unload doesn't work right now, at least not in Windows */
#else
		return SWITCH_STATUS_SUCCESS;
#endif
	}

	delete h323_process;
	h323_process = NULL;

	return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_h323_shutdown)
{
	switch_safe_free(mod_h323_globals.context);
	switch_safe_free(mod_h323_globals.dialplan);
	switch_safe_free(mod_h323_globals.codec_string);
	switch_safe_free(mod_h323_globals.rtp_timer_name);

	delete h323_process;
	h323_process = NULL;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C
/*******************************************************************************/

void h_timer(unsigned sec)
{
#ifdef WIN32
	switch_sleep(sec * 1000000);
#else
	timeval timeout;
	timeout.tv_sec = sec;
	timeout.tv_usec = 0; 
	select(0, NULL, NULL, NULL, &timeout);
#endif
}


#if PTRACING

class FSTrace : public ostream {
 public:
 FSTrace()
     : ostream(&buffer)
        {
        }

 private:
 class Buffer : public streambuf {
        char buffer[250];

    public:
        Buffer()
            {
				// leave 2 chars room at end: 1 for overflow char and1 for \0
                setg(buffer, buffer, &buffer[sizeof(buffer)-2]);
                setp(buffer, &buffer[sizeof(buffer)-2]);
            }

        virtual int sync()
        {
            return overflow(EOF);
        }

        virtual int underflow()
        {
            return EOF;
        }

        virtual int overflow(int c)
        {
            const char *fmt = "%s";
            char *func = NULL;

            int64_t bufSize = pptr() - pbase();

            if (c != EOF) {
                *pptr() = (char)c;
                bufSize++;
            }
            
            if (bufSize != 0) {
                char *bufPtr = pbase();
                char *bufEndPtr = NULL;
                setp(bufPtr, epptr());
                bufPtr[bufSize] = '\0';
                int line = 0;
                char *p;
                
                char *file = NULL;
                switch_log_level_t level;

                
                switch (strtoul(bufPtr, &file, 10)) {
                case 1 :
                    level = SWITCH_LOG_INFO;
                    break;
                default :
                    level = SWITCH_LOG_DEBUG;
                    break;
                }

                if (file) {
                    while (isspace(*file)) file++;
                    
                    if (file && (bufPtr = strchr(file, '(')) && (bufEndPtr = strchr(bufPtr, ')'))) {
                        char *e;

                        for(p = bufPtr; p && *p; p++) {
                            if (*p == '\t') {
                                *p = ' ';
                            }
                        }

                        *bufPtr++ = '\0';
                        line = atoi(bufPtr);
                        while (bufEndPtr && isspace(*(++bufEndPtr)));
                        bufPtr = bufEndPtr;
                        if (bufPtr && (e = strchr(bufPtr, ' ')) || (e = strchr(bufPtr, '\t'))) {
                            func = bufPtr;
                            bufPtr = e;
                            *bufPtr++ = '\0';
                        }
                    }
                }
                
                switch_text_channel_t tchannel = SWITCH_CHANNEL_ID_LOG;

                if (!bufPtr) {
                    bufPtr = pbase();
                    level = SWITCH_LOG_DEBUG;
                }

                if (bufPtr) {
                    if (end_of(bufPtr) != '\n') {
                        fmt = "%s\n";
                    }
                    if (!(file && func && line)) tchannel = SWITCH_CHANNEL_ID_LOG_CLEAN;

                    switch_log_printf(tchannel, file, func, line, NULL, level, fmt, bufPtr);
                }
                
            }

            return 0;
        }
    } buffer;
};

#endif

PString GetH245CodecName(const H323Capability* cap)
{
	switch (cap->GetSubType()) {
		case H245_AudioCapability::e_g711Alaw64k:
		case H245_AudioCapability::e_g711Alaw56k:
			return "PCMA";
		case H245_AudioCapability::e_g711Ulaw64k:
		case H245_AudioCapability::e_g711Ulaw56k:
			return "PCMU";
		case H245_AudioCapability::e_g722_64k:
		case H245_AudioCapability::e_g722_56k:
		case H245_AudioCapability::e_g722_48k:
			return "G722";
		case H245_AudioCapability::e_g728:
			return "G728";
		case H245_AudioCapability::e_g729:
		case H245_AudioCapability::e_g729AnnexA:
		case H245_AudioCapability::e_g729wAnnexB:
		case H245_AudioCapability::e_g729AnnexAwAnnexB:
			return "G729";
		case H245_AudioCapability::e_g7231:
		case H245_AudioCapability::e_g7231AnnexCCapability:
			return "G723";
		case H245_AudioCapability::e_gsmFullRate:
		case H245_AudioCapability::e_gsmHalfRate:
		case H245_AudioCapability::e_gsmEnhancedFullRate:
			return "GSM";	
	}
	return "L16";
}

FSProcess::FSProcess()
	: PLibraryProcess("FreeSWITCH", "mod_h323", 1, 0, AlphaCode, 1)
	, m_h323endpoint(NULL){
}

FSProcess::~FSProcess(){
	delete m_h323endpoint;
}

bool FSProcess::Initialise(switch_loadable_module_interface_t *iface)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "======>FSProcess::Initialise [%p]\n", this);
	
	m_h323endpoint = new FSH323EndPoint();
	return m_h323endpoint != NULL && m_h323endpoint->Initialise(iface);
}

bool FSH323EndPoint::Initialise(switch_loadable_module_interface_t *iface)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSManager::Initialise [%p]\n", this);
	ReadConfig(false);

	/* Update tracing level for h323 */
	PTrace::SetLevel(mod_h323_globals.trace_level);
	PTrace::SetOptions(PTrace::TraceLevel);
	PTrace::SetStream(new FSTrace);

	m_freeswitch = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(iface, SWITCH_ENDPOINT_INTERFACE);
	m_freeswitch->interface_name = modulename;
	m_freeswitch->io_routines = &h323fs_io_routines;
	m_freeswitch->state_handler = &h323fs_event_handlers;
			
	PString codec = ((const char *)mod_h323_globals.codec_string);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Config capability %s \n", (const char *)codec);

	if (!codec.IsEmpty()) {		
		const char** f = h323_formats;
		for (; *f; f += 2) {			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Find capability %s to %s\n",f[1], (const char *)codec);
			if (codec.Find(f[1]) != P_MAX_INDEX) {
				PString tmp = f[0];
				tmp += "*{sw}";
				PINDEX init = GetCapabilities().GetSize();
				AddAllCapabilities(0, 0, tmp);
				PINDEX num = GetCapabilities().GetSize() - init;
				if (!num) {
					// failed to add so pretend we support it in hardware
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "H323 failed to add capability '%s' \n",(const char *)tmp);
					tmp = f[0];
					tmp += "*{hw}";
					AddAllCapabilities(0, 0, tmp);
					num = GetCapabilities().GetSize() - init;
				}
				if (num)					
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "H.323 added %d capabilities '%s' \n",num,(const char *)tmp);
				else
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "H323 failed to add capability '%s' \n",(const char *)tmp);
					
			}		
		}
	}
	
	if (m_fax_old_asn) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "--->fax_old_asn\n");
		SetT38_IFP_PRE();
		SetCapability(0, 0, new FSH323_T38Capability(OpalT38_IFP_PRE));
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "--->fax_asn\n");
		SetCapability(0, 0, new FSH323_T38Capability(OpalT38_IFP_COR));
	}
		
	AddAllUserInputCapabilities(0, 1);
	
	DisableFastStart(!m_faststart);
	DisableH245Tunneling(!m_h245tunneling);
	DisableH245inSetup(!m_h245insetup);
	DisableDetectInBandDTMF(!m_dtmfinband);
	if (!m_endpointname.IsEmpty())
		SetLocalUserName(m_endpointname);
	 
	if (m_listeners.empty()) {
		StartListener("");
	} else {
		for (std::list < FSListener >::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it) {
			if (!StartListener(it->listenAddress)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Cannot start listener for %s\n", (const char*)(it->name));
			}
		}
	}

	if (!m_gkAddress.IsEmpty()) {
		m_thread = new FSGkRegThread(this, &m_gkAddress, &m_gkIdentifer, &m_gkInterface, m_gkretry);
		m_thread->SetAutoDelete();
		m_thread->Resume();
	}
	
	return TRUE;
}

switch_status_t FSH323EndPoint::ReadConfig(int reload)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323EndPoint::ReadConfig [%p]\n",this);

	const char *cf = "h323.conf";
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_memory_pool_t *pool = NULL;
	
	if ((status = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		return status;
	}

	set_global_context("default");
	set_global_dialplan("XML");

	switch_event_t *params = NULL;
	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "profile", switch_str_nil(""));
	switch_xml_t cfg;
	switch_xml_t xml = switch_xml_open_cfg(cf, &cfg, params);

	if (xml == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_FALSE;
	}

	switch_xml_t xmlSettings = switch_xml_child(cfg, "settings");
	m_pi = 8;
	m_ai = 0;
	m_endpointname = "FreeSwitch";
	mod_h323_globals.ptime_override_value = -1;

	if (xmlSettings) {
		for (switch_xml_t xmlParam = switch_xml_child(xmlSettings, "param"); xmlParam != NULL; xmlParam = xmlParam->next) {
			const char *var = switch_xml_attr_soft(xmlParam, "name");
			const char *val = switch_xml_attr_soft(xmlParam, "value");

			if (!strcasecmp(var, "trace-level")) {
				int level = atoi(val);
				if (level > 0) {
					mod_h323_globals.trace_level = level;
				}
			} else if (!strcasecmp(var, "context")) {
				set_global_context(val);
			} else if (!strcasecmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcasecmp(var, "codec-prefs")) {
				set_global_codec_string(val);
			} else if (!strcasecmp(var, "use-rtp-timer")) {
				mod_h323_globals.use_rtp_timer = switch_true(val);
			} else if (!strcasecmp(var, "rtp-timer-name")) {
				set_global_rtp_timer_name(val);
			} else if (!strcasecmp(var, "ptime-override-value")) {
				mod_h323_globals.ptime_override_value = atoi(val);
			} else if (!strcasecmp(var, "jitter-size")) {
				char * next;
				unsigned minJitter = strtoul(val, &next, 10);
				if (minJitter >= 10) {
					unsigned maxJitter = minJitter;
					if (*next == ',')
						maxJitter = atoi(next+1);
					SetAudioJitterDelay(minJitter, maxJitter); // In milliseconds
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set zero Jitter buffer\n");
					SetAudioJitterDelay(0, 0);
				}
			} else if (!strcasecmp(var, "faststart")) {
				m_faststart = switch_true(val);
			} else if (!strcasecmp(var, "h245tunneling")) {
				m_h245tunneling = switch_true(val);
			} else if (!strcasecmp(var, "h245insetup")) {
				m_h245insetup = switch_true(val);
			} else if (!strcasecmp(var, "dtmfinband")) {
				m_dtmfinband = switch_true(val);
			} else if (!strcasecmp(var, "gk-address")) {
				m_gkAddress = val;
			} else if (!strcasecmp(var, "gk-identifer")) {
				m_gkIdentifer = val;
			} else if (!strcasecmp(var, "endpoint-name")) {
				m_endpointname = val;
			} else if (!strcasecmp(var, "gk-interface")) {
				m_gkInterface = val;
			} else if (!strcasecmp(var, "gk-prefix")) {
				m_gkPrefixes.AppendString(val);
			} else if (!strcasecmp(var, "gk-retry")) {
				m_gkretry = atoi(val);
			} else if (!strcasecmp(var, "progress-indication")) {
				m_pi = atoi(val);
			} else if (!strcasecmp(var, "alerting-indication")) {
				m_ai = atoi(val);
			} else if (!strcasecmp(var, "fax-old-asn")) {
				m_fax_old_asn = switch_true(val);
			}			
		}
	}

	switch_xml_t xmlListeners = switch_xml_child(cfg, "listeners");
	if (xmlListeners != NULL) {
		for (switch_xml_t xmlListener = switch_xml_child(xmlListeners, "listener"); xmlListener != NULL; xmlListener = xmlListener->next) {

			m_listeners.push_back(FSListener());
			FSListener & listener = m_listeners.back();

			listener.name = switch_xml_attr_soft(xmlListener, "name");
			if (listener.name.IsEmpty())
				listener.name = "unnamed";

			PIPSocket::Address ip;
			WORD port = 1720;

			for (switch_xml_t xmlParam = switch_xml_child(xmlListener, "param"); xmlParam != NULL; xmlParam = xmlParam->next) {
				const char *var = switch_xml_attr_soft(xmlParam, "name");
				const char *val = switch_xml_attr_soft(xmlParam, "value");

				if (!strcasecmp(var, "h323-ip"))
					ip = val;
				else if (!strcasecmp(var, "h323-port"))
					port = (WORD) atoi(val);
			}

			listener.listenAddress = new H323ListenerTCP(*this, ip, port);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created Listener '%s'\n", (const char *) listener.name);
		}
	}

	switch_event_destroy(&params);

	if (xml)
		switch_xml_free(xml);

	if (mod_h323_globals.use_rtp_timer && !mod_h323_globals.rtp_timer_name)
		set_global_rtp_timer_name("soft");

	return status;
}

FSH323EndPoint::FSH323EndPoint()
	:m_faststart(true)
	,m_h245tunneling(true)
	,m_h245insetup(true)
	,m_dtmfinband(false)
	,m_thread(NULL)
	,m_stop_gk(false)
	,m_fax_old_asn(false)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323EndPoint::FSH323EndPoint [%p]\n",this);
	terminalType = e_GatewayAndMC;
}	

FSH323EndPoint::~FSH323EndPoint()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323EndPoint::~FSH323EndPoint [%p]\n",this);
	StopGkClient();
	ClearAllCalls(H323Connection::EndedByLocalUser, false);
}

H323Connection  *FSH323EndPoint::CreateConnection(
	unsigned callReference,
	void* userData,
	H323Transport* transport,
	H323SignalPDU* setupPDU)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323EndPoint::CreateConnection callReference = %u  userDate = %p [%p]\n",callReference,userData,this);

	if ((switch_caller_profile_t *)userData) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------> SWITCH_CALL_DIRECTION_OUTBOUND\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------> SWITCH_CALL_DIRECTION_INBOUND\n");
	}
	
	switch_core_session_t *fsSession = switch_core_session_request(GetSwitchInterface(), 
				(switch_caller_profile_t *)userData ? SWITCH_CALL_DIRECTION_OUTBOUND : SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL);
	if (fsSession == NULL)
		return NULL;
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------> fsSession = %p\n",fsSession);
	switch_channel_t *fsChannel = switch_core_session_get_channel(fsSession);

	if (fsChannel == NULL) {
		switch_core_session_destroy(&fsSession);
		return NULL;
	}
	
	return new FSH323Connection(*this, transport, callReference, (switch_caller_profile_t *)userData, fsSession, fsChannel);
}

bool FSH323EndPoint::OnSetGatewayPrefixes(PStringList & prefixes) const
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323EndPoint::OnSetGatewayPrefixes [%p]\n",this);
	if(m_gkPrefixes.GetSize() > 0) {
//		PTRACE(4, "mod_h323\tOnSetGatewayPrefixes " << m_gkPrefixes);
		prefixes = m_gkPrefixes;
		return true;
	}
	return false;
}

void FSH323EndPoint::StartGkClient(int retry, PString* gkAddress,PString* gkIdentifer,PString* gkInterface)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323EndPoint::StartGkClient [%p]\n",this);

	while (!UseGatekeeper(m_gkAddress, m_gkIdentifer, m_gkInterface) && retry > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			"Could not start gatekeeper: gw name=\"%s\", addr=\"%s\", id=\"%s\", if=\"%s\"\n",
			(const char *)m_endpointname,
			(const char *)m_gkAddress,
			(const char *)m_gkIdentifer,
			(const char *)m_gkInterface);
		if (m_stop_gk) {
			m_stop_gk = false;
			return;
		}		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Wait next go connect gatekeeper %d\n",retry);
		h_timer(retry);
		if (m_stop_gk) {
			m_stop_gk = false;
			return;
		}
		RemoveGatekeeper();
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Started gatekeeper: %s\n",
							(const char *)GetGatekeeper()->GetName());	
	m_thread = NULL;
}

void FSH323EndPoint::StopGkClient()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323EndPoint::StopGkClient [%p]\n", this);

	if (m_thread) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stop gatekeeper thread\n");
		m_stop_gk = true;
		
		while (m_stop_gk){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Wait stop gatekeeper thread\n");
			h_timer(2);
		}
		RemoveGatekeeper();
		m_thread = NULL;
	}
}

FSH323Connection::FSH323Connection(FSH323EndPoint& endpoint, H323Transport* transport, unsigned callReference,  switch_caller_profile_t *outbound_profile, switch_core_session_t *fsSession, switch_channel_t *fsChannel)
	: H323Connection(endpoint,callReference)
	, m_endpoint(&endpoint)
	, m_fsSession(fsSession)
	, m_fsChannel(fsChannel)
	, m_callOnPreAnswer(false)
	, m_startRTP(false)
	, m_rxChannel(false)
	, m_txChannel(false)
	, m_ChannelAnswer(false)
	, m_ChannelProgress(false)
	, m_select_dtmf(0)
	, m_active_sessionID(0)
	, m_active_channel_fax(false)
	, m_rtp_resetting(0)
	, m_isRequst_fax(false)
	, m_channel_hangup(false)
	, m_RTPlocalPort(0)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::FSH323Connection [%p]\n",this);

	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_alloc(m_fsSession, sizeof(*tech_pvt));
	tech_pvt->me = this;
	tech_pvt->active_connection = true;
	switch_core_session_set_private(m_fsSession, tech_pvt);
	
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(m_fsSession));
	switch_mutex_init(&tech_pvt->h323_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(m_fsSession));
	switch_mutex_init(&tech_pvt->h323_io_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(m_fsSession));
	
	if (outbound_profile != NULL) {
		SetLocalPartyName(outbound_profile->caller_id_number);
		SetDisplayName(outbound_profile->caller_id_name);

		switch_caller_profile_t *caller_profile = switch_caller_profile_clone(m_fsSession, outbound_profile);
		switch_channel_set_caller_profile(m_fsChannel, caller_profile);

		PString name = "h323/";
		name += outbound_profile->destination_number;
		switch_channel_set_name(m_fsChannel, name);

		switch_channel_set_state(m_fsChannel, CS_INIT);
	}
}	

FSH323Connection::~FSH323Connection()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::~FSH323Connection  [%p]\n",this);
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	if ((m_rtp_resetting == 1)) {
		switch_core_session_unlock_codec_read(m_fsSession);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_read [%p]\n",m_fsSession);
		switch_core_session_unlock_codec_write(m_fsSession);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_write [%p]\n",m_fsSession);
	}

	if (tech_pvt->rtp_session) {
		switch_rtp_destroy(&tech_pvt->rtp_session);
		tech_pvt->rtp_session = NULL;
	} else if (m_RTPlocalPort) {
		switch_rtp_release_port((const char *)m_RTPlocalIP.AsString(), m_RTPlocalPort);
	}
	
	tech_pvt->me = NULL;
	tech_pvt->active_connection = false;
//	switch_mutex_unlock(tech_pvt->h323_mutex);
//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
	switch_safe_free(tech_pvt->token);
}	

void FSH323Connection::AttachSignalChannel(const PString & token,
                                         H323Transport * channel,
                                         PBoolean answeringCall)
{
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	tech_pvt->token = strdup((const char *)token);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"---------->token = %s [%p]\n",(const char *)token,this);
	H323Connection::AttachSignalChannel(token,channel,answeringCall);
}


void FSH323Connection::OnSetLocalCapabilities()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnSetLocalCapabilities() [%p]\n",this);
	H323Connection::OnSetLocalCapabilities();
	SetLocalCapabilities();
}

bool FSH323Connection::SetLocalCapabilities()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::SetLocalCapabilities() Size local capability = %d [%p]\n",localCapabilities.GetSize(),this);

	if (!mod_h323_globals.codec_string)
		return false;
    
	bool nocodecs = true;
	bool changed = false;

	for (int i = 0; i < localCapabilities.GetSize(); i++) {
		const char* format = 0;
		PString fname;
		decodeCapability(localCapabilities[i],&format,0,&fname);
		if (format) {
			PString m_globalcodec = ((const char *)mod_h323_globals.codec_string);
			if (m_globalcodec.Find(format) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"Removing capability '%s' (%s) not in remote %s",(const char*)fname,format,(const char*)m_globalcodec);
				changed = true;
				for (PINDEX idx = 0; idx < fastStartChannels.GetSize(); idx++) {
					if (fastStartChannels[idx].GetCapability() == localCapabilities[i]) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"Removing fast start channel %s '%s' (%s)\n",GetDirections[fastStartChannels[idx].GetDirection()],(const char*)fname,(const char*)format);
						fastStartChannels.RemoveAt(idx--);
					}		
				}
				localCapabilities.Remove(fname);
				i--;
			} else
				nocodecs = false;
		}
	}

	if (nocodecs) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No codecs remaining for H323 connection [%p]\n",this);
		changed = false;
		ClearCall(EndedByCapabilityExchange);		
	}

	return changed;
}

bool FSH323Connection::decodeCapability(const H323Capability& capability, const char** dataFormat, int* payload, PString* capabName)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::decodeCapability [%p]\n",this);

	PString fname((const char *)capability.GetFormatName());
  
	if (fname.Find("{sw}") == (fname.GetLength() - 4))
		fname = fname.Mid(0,fname.GetLength()-4);
	if (fname.Find("{hw}") == (fname.GetLength() - 4))
		fname = fname.Mid(0,fname.GetLength()-4);
		
	OpalMediaFormat oformat(fname, false);
	int pload = oformat.GetPayloadType();
	const char *format = 0;
	const char** f = h323_formats;

	for (; *f; f += 2) {
		if (fname.Find(*f) == 0) {
			format = f[1];
			break;
		}
	}
    
	if (format) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"capability '%s' format '%s' %d\n",(const char*)fname,format,pload);
		if (capabName)
			*capabName = fname;
		if (dataFormat)
			*dataFormat = format;
		if (payload)
			*payload = pload;
		return true;
	}

	return false;
}

H323Connection::AnswerCallResponse FSH323Connection::OnAnswerCall(const PString &caller,
    const H323SignalPDU &setupPDU, H323SignalPDU &connectPDU)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnAnswerCall caller = %s [%p]\n",(const char*)caller,this);
	
	if (m_fsSession == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Session request failed.\n");
		return H323Connection::AnswerCallDenied;
	}

	switch_core_session_add_stream(m_fsSession, NULL);

	switch_channel_t *channel = switch_core_session_get_channel(m_fsSession);
	if (channel == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Session does not have a channel\n");
		return H323Connection::AnswerCallDenied;
	}

	const Q931& q931 = setupPDU.GetQ931();
	const H225_Setup_UUIE& setup = setupPDU.m_h323_uu_pdu.m_h323_message_body;
	const H225_ArrayOf_AliasAddress& address = setup.m_destinationAddress;

	for (int i = 0; i<address.GetSize(); i++)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"address index = %d value = %s",i,(const char *)H323GetAliasAddressString(address[i]));

	PString called;

	if (address.GetSize() > 0)
		called = (const char *)H323GetAliasAddressString(address[0]);
	if (!called.IsEmpty())
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"Called number or alias = %s\n",(const char*)called);
	else {
		PString callnam;
		if (q931.GetCalledPartyNumber(callnam)) {
			called=(const char *)callnam;			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"Called-Party-Number = %s\n",(const char*)called);
		}
	}
	
	PIPSocket::Address remote_network_addr;
	GetSignallingChannel()->GetRemoteAddress().GetIpAddress(remote_network_addr);
       
	switch_caller_profile_t *caller_profile = switch_caller_profile_new(switch_core_session_get_pool(m_fsSession),
                                                                        NULL,
                                                                        /** username */
                                                                        mod_h323_globals.dialplan,
                                                                        /** dial plan */
                                                                        GetRemotePartyName(),
                                                                        /** caller_id_name */
                                                                        GetRemotePartyNumber(),
                                                                        /** caller_id_number */
                                                                	remote_network_addr.AsString(),
                                                                        /** network addr */
                                                                        NULL,
                                                                        /** ANI */
                                                                        NULL,
                                                                        /** ANI II */
                                                                        NULL,
                                                                        /** RDNIS */
                                                                        modulename,
                                                                        /** source */
                                                                        mod_h323_globals.context,
                                                                        /** set context  */
                                                                        called
                                                                        /** destination_number */
                                                                        );
	if (caller_profile == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not create caller profile\n");
		return H323Connection::AnswerCallDenied;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Created switch caller profile:\n"
		"  username       	  = %s\n"
		"  dialplan           = %s\n"
		"  caller_id_name     = %s\n"
		"  caller_id_number   = %s\n"
		"  network_addr       = %s\n"
		"  source             = %s\n"
		"  context            = %s\n"
		"  destination_number = %s\n"
		,caller_profile->username
		,caller_profile->dialplan
		,caller_profile->caller_id_name
		,caller_profile->caller_id_number
		,caller_profile->network_addr
		,caller_profile->source
		,caller_profile->context
		,caller_profile->destination_number);


	switch_channel_set_caller_profile(channel, caller_profile);

	char name[256] = "h323/";
	switch_copy_string(name + 5, caller_profile->destination_number, sizeof(name)-5);
	switch_channel_set_name(channel, name);
	switch_channel_set_state(channel, CS_INIT);

	if (switch_core_session_thread_launch(m_fsSession) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not launch session thread\n");
		return H323Connection::AnswerCallDenied;
	}

	return H323Connection::AnswerCallDeferred;
}

H323Channel* FSH323Connection::CreateRealTimeLogicalChannel(const H323Capability& capability,H323Channel::Directions dir,unsigned sessionID,const H245_H2250LogicalChannelParameters* param, RTP_QOS * rtpqos)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::CreateRealTimeLogicalChannel [%p]\n",this);
	
	H323TransportAddress m_h323transportadd = GetSignallingChannel()->GetLocalAddress();
	m_h323transportadd.GetIpAddress(m_RTPlocalIP);
	if (!m_RTPlocalPort) {
		m_RTPlocalPort = switch_rtp_request_port((const char *)m_RTPlocalIP.AsString());
	}

	return new FSH323_ExternalRTPChannel(*this, capability, dir, sessionID,m_RTPlocalIP,m_RTPlocalPort);
}

PBoolean FSH323Connection::OnStartLogicalChannel(H323Channel & channel)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnStartLogicalChannel chennel = %p [%p]\n",&channel,this);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnStartLogicalChannel connectionState = %s [%p]\n",ConnectionStatesNames[connectionState],this);
	return connectionState != ShuttingDownConnection;
}

PBoolean FSH323Connection::OnCreateLogicalChannel(const H323Capability& capability, H323Channel::Directions dir, unsigned& errorCode)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnCreateLogicalChannel ('%s',%s) [%p]\n",(const char *)capability.GetFormatName(),GetDirections[dir],this);
    
	return H323Connection::OnCreateLogicalChannel(capability,dir,errorCode);
}

void FSH323Connection::OnReceivedReleaseComplete(const H323SignalPDU & pdu)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnReceivedReleaseComplete  value = %d\n",(switch_call_cause_t)pdu.GetQ931().GetCause());
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	if ((tech_pvt->me != NULL) && (m_rtp_resetting == 1)) {
			switch_core_session_unlock_codec_read(m_fsSession);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_read [%p]\n",m_fsSession);
			switch_core_session_unlock_codec_write(m_fsSession);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_write [%p]\n",m_fsSession);
	}
	tech_pvt->me = NULL;
	switch_channel_hangup(switch_core_session_get_channel(m_fsSession),(switch_call_cause_t)pdu.GetQ931().GetCause()); 

	return H323Connection::OnReceivedReleaseComplete(pdu);
}

bool FSH323Connection::OnReceivedProgress(const H323SignalPDU &pdu)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnReceivedProgress [%p]\n", this);
	H323Connection::OnReceivedProgress(pdu);
	if ((m_rxChannel && m_txChannel) || (m_ChannelProgress && m_rxChannel))
		switch_channel_mark_pre_answered(m_fsChannel);
	else{
		m_ChannelProgress = true;
	}
	return true;
}

bool FSH323Connection::OnReceivedSignalSetup(const H323SignalPDU & setupPDU)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnReceivedSignalSetup [%p]\n",this);

	if (!H323Connection::OnReceivedSignalSetup(setupPDU))
		return false;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"---------> after FSH323Connection::OnReceivedSignalSetup connectionState = %s [%p]\n",ConnectionStatesNames[connectionState],this);
	H323SignalPDU callProceedingPDU;
	H225_CallProceeding_UUIE & callProceeding = callProceedingPDU.BuildCallProceeding(*this);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"---------> after callProceedingPDU.BuildCallProceeding connectionState = %s [%p]\n",ConnectionStatesNames[connectionState],this);
	if (connectionState == ShuttingDownConnection){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"---------> connectionState = ShuttingDownConnection [%p]\n",this);
		return false; 
	}

	if (SendFastStartAcknowledge(callProceeding.m_fastStart)) {
		callProceeding.IncludeOptionalField(H225_CallProceeding_UUIE::e_fastStart);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"SendFastStartAcknowledge = FALSE\n");
		if (connectionState == ShuttingDownConnection) {
			return true;
		}
		earlyStart = TRUE;
		if (!h245Tunneling && (controlChannel == NULL)) {
			if (!StartControlChannel()){
				return true;
			}
			callProceeding.IncludeOptionalField(H225_CallProceeding_UUIE::e_h245Address);
			controlChannel->SetUpTransportPDU(callProceeding.m_h245Address, TRUE);
		}
	}
	if (connectionState == ShuttingDownConnection) {
		return true;
	}

	if (!WriteSignalPDU(callProceedingPDU)) {
		return false;
	}

	return true;		
}

bool FSH323Connection::OnReceivedCallProceeding(const H323SignalPDU & pdu)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>PFSH323Connection::OnReceivedCallProceeding [%p]\n",this);
	unsigned pi;

	if (!pdu.GetQ931().GetProgressIndicator(pi))
		pi = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"----------->OnAlerting PI = %u",pi);
	if (pi > 0) {
		if ((m_rxChannel && m_txChannel) || (m_ChannelProgress && m_rxChannel))
			switch_channel_mark_pre_answered(m_fsChannel);
		else {
			m_ChannelProgress = true;
		}
	}

	return H323Connection::OnReceivedCallProceeding(pdu);
}

bool FSH323Connection::OnSendCallProceeding(H323SignalPDU & callProceedingPDU)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnSendCallProceeding fastStartState = %s [%p]\n",FastStartStateNames[fastStartState],this);	

	return false;
//	return true;
}

bool FSH323Connection::OnSendReleaseComplete(H323SignalPDU & pdu)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnSendReleaseComplete cause = %u\n",(switch_call_cause_t)pdu.GetQ931().GetCause());	

	switch_channel_hangup(m_fsChannel, (switch_call_cause_t) pdu.GetQ931().GetCause());
	return H323Connection::OnSendReleaseComplete(pdu);
}

PBoolean FSH323Connection::OpenLogicalChannel(const H323Capability& capability, unsigned sessionID, H323Channel::Directions dir)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OpenLogicalChannel ('%s', %d, %s) [%p]\n",(const char *)capability.GetFormatName(),sessionID,GetDirections[dir],this);

	return H323Connection::OpenLogicalChannel(capability,sessionID,dir);
}


bool FSH323Connection::OnReceivedCapabilitySet(const H323Capabilities & remoteCaps,
							const H245_MultiplexCapability * muxCap,
							H245_TerminalCapabilitySetReject & reject)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnReceivedCapabilitySet [%p]\n",this);
	if (connectionState == ShuttingDownConnection)
		return false;
	if (!H323Connection::OnReceivedCapabilitySet(remoteCaps, muxCap, reject)) {
		return false;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>END H323Connection::OnReceivedCapabilitySet [%p]\n",this);
	
	for (int i = 0; i < remoteCapabilities.GetSize(); ++i) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"----> Capabilities = %s\n",(const char*)(remoteCapabilities[i].GetFormatName()));
	}
	
	H323Capability * cap = remoteCapabilities.FindCapability(H323Capability::e_Audio);
	if (cap == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"----> Capabilities is NULL \n");
		return false;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"----> Capabilities not NULL \n");
	
	return connectionState != ShuttingDownConnection;						
}

bool FSH323Connection::OnAlerting(const H323SignalPDU &alertingPDU, const PString &user)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>PFSH323Connection::OnAlerting user = %s [%p]\n",(const char *)user,this);
	unsigned pi;
	switch_channel_mark_ring_ready(m_fsChannel);
	
	if (!alertingPDU.GetQ931().GetProgressIndicator(pi))
		pi = 0;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"----------->OnAlerting PI = %u\n",pi);
	if (pi > 0) {
		if ((m_rxChannel && m_txChannel) || (m_ChannelProgress && m_rxChannel))
			switch_channel_mark_pre_answered(m_fsChannel);
		else {
			m_ChannelProgress = true;
		}
	}
	return H323Connection::OnAlerting(alertingPDU,user);
}

void FSH323Connection::AnsweringCall(AnswerCallResponse response)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::AnsweringCall [%p]\n",this);
	
	switch (response) {
		case AnswerCallDeferredWithMedia:{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Answering call: %s\n",GetAnswerCallResponse[response]);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->Lock\n");
			if (!Lock())
				return;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->Lock ok\n");
			if (!mediaWaitForConnect) {
				// create a new facility PDU if doing AnswerDeferredWithMedia
				H323SignalPDU want245PDU;
				want245PDU.BuildProgress(*this);
				PBoolean sendPDU = TRUE;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"mediaWaitForConnect = FALSE\n");
/*				if (SendFastStartAcknowledge(prog.m_fastStart)){
					PTRACE(2, "H323\tSendFastStartAcknowledge = TRUE ");
					prog.IncludeOptionalField(H225_Progress_UUIE::e_fastStart);
				} else {
					PTRACE(2, "H323\tSendFastStartAcknowledge = FALSE ");
					// See if aborted call
					if (connectionState == ShuttingDownConnection){
						Unlock();
						return;
					}
					// Do early H.245 start
					H225_Facility_UUIE & fac = *want245PDU.BuildFacility(*this, FALSE, H225_FacilityReason::e_startH245);
					earlyStart = TRUE;
					if (!h245Tunneling && (controlChannel == NULL)) {
						if (!StartControlChannel()){
							Unlock();
							return;
						}
						fac.IncludeOptionalField(H225_Facility_UUIE::e_h245Address);
						controlChannel->SetUpTransportPDU(fac.m_h245Address, TRUE);
					} 
					else
						sendPDU = FALSE;
				}
*/				
				const char *vpi = switch_channel_get_variable(m_fsChannel, "progress-indication"); 
				unsigned pi = 8;
				if (vpi){
					pi = atoi(vpi); 
				}
				else pi = m_endpoint->m_pi;
				if ((pi< 1) || (pi > 8)||(pi == 7)) pi = 8;
				want245PDU.GetQ931().SetProgressIndicator(pi);
				if (sendPDU) {
					HandleTunnelPDU(&want245PDU);
					WriteSignalPDU(want245PDU);
				}
			}
			InternalEstablishedConnectionCheck();
			Unlock();
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->UnLock\n");
			return;
		} 
		case AnswerCallPending :{
			if (alertingPDU != NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->Lock\n");
				if (!Lock())
					return;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->Lock ok\n");
				// send Q931 Alerting PDU
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"Sending Alerting PDU\n");
				
				const char *vai = switch_channel_get_variable(m_fsChannel, "alerting-indication"); 
				unsigned ai = 0;
				if (vai){
					ai = atoi(vai); 
				}
				else ai = m_endpoint->m_ai;
				if ((ai< 0) || (ai > 8)||(ai == 7)) ai = 8;
				if (ai > 0)
					(*alertingPDU).GetQ931().SetProgressIndicator(ai);
				
				HandleTunnelPDU(alertingPDU);
				WriteSignalPDU(*alertingPDU);
				alertingTime = PTime();
				InternalEstablishedConnectionCheck();
				Unlock();
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->UnLock\n");
				return;
			}
		}
		default :H323Connection::AnsweringCall(response);
	} 
}

void FSH323Connection::OnEstablished()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>PFSH323Connection::OnEstablished [%p]\n",this);
	if(m_startRTP)		
		switch_channel_mark_answered(m_fsChannel);
	else
		m_ChannelAnswer = true;

	if (m_active_channel_fax)
		RequestModeChangeT38("T.38\nT.38");
	else 
		m_active_channel_fax = true;
}
PBoolean FSH323Connection::OnRequestModeChange(const H245_RequestMode & pdu,
                                         H245_RequestModeAck & /*ack*/,
                                         H245_RequestModeReject & /*reject*/,
                                         PINDEX & selectedMode)
{
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_lock\n");
	switch_mutex_lock(tech_pvt->h323_mutex);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>PFSH323Connection::OnRequestModeChange [%p]\n",this);
	if (!m_isRequst_fax){
		m_isRequst_fax = true;
		switch_mutex_unlock(tech_pvt->h323_mutex);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
		return true;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
	switch_mutex_unlock(tech_pvt->h323_mutex);
	return false;
}

void FSH323Connection::OnModeChanged(const H245_ModeDescription & newMode)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>PFSH323Connection::OnModeChanged [%p]\n",this);
	for (PINDEX i = 0; i < newMode.GetSize(); i++) {
		H323Capability * capability = localCapabilities.FindCapability(newMode[i]);
		if (PAssertNULL(capability) != NULL)  {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Open channel after mode change: %s\n",(const char*)(capability->GetFormatName()));			
			if (capability->GetMainType() == H323Capability::e_Data){
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"capability->GetMainType() = H323Capability::e_Data\n");
				H245_DataMode & type = newMode[i].m_type;
				if (type.m_application.GetTag() == H245_DataMode_application::e_t38fax){
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"ttype.m_application.GetTag() = H245_DataMode_application::e_t38fax\n");
					H245_DataMode_application_t38fax & fax = type.m_application;
					//H245_DataProtocolCapability & proto = fax.m_t38FaxProtocol;
					const H245_T38FaxProfile & profile = fax.m_t38FaxProfile;
					switch_t38_options_t* t38_options = (switch_t38_options_t*)switch_channel_get_private(m_fsChannel, "t38_options");

					if (!t38_options) {
						t38_options = (switch_t38_options_t*)switch_core_session_alloc(m_fsSession, sizeof(* t38_options));
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"switch_core_session_alloc t38_options\n");
					}
					t38_options->T38VendorInfo = "0 0 0";
					
					t38_options->T38FaxVersion = (uint16_t)profile.m_version;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"T38FaxVersion:%lu\n",(unsigned long)profile.m_version);
					t38_options->T38MaxBitRate = type.m_bitRate*100;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"T38MaxBitRate:%d\n",t38_options->T38MaxBitRate);
					if (profile.m_fillBitRemoval) 
						t38_options->T38FaxFillBitRemoval = SWITCH_TRUE;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"T38FaxFillBitRemoval:%d\n",(int)profile.m_fillBitRemoval);
					if (profile.m_transcodingMMR)
						t38_options->T38FaxTranscodingMMR = SWITCH_TRUE;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"T38FaxTranscodingMMR:%d\n",(int)profile.m_transcodingMMR);
					if (profile.m_transcodingJBIG) 
						t38_options->T38FaxTranscodingJBIG = SWITCH_TRUE;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"T38FaxTranscodingJBIG:%d\n",(int)profile.m_transcodingJBIG);
					if (profile.m_t38FaxRateManagement.GetTag() == H245_T38FaxRateManagement::e_transferredTCF)
						t38_options->T38FaxRateManagement = "transferredTCF";
					else
						t38_options->T38FaxRateManagement = "localTCF";
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"T38FaxRateManagement:%s\n",t38_options->T38FaxRateManagement);
					t38_options->T38FaxMaxBuffer = profile.m_t38FaxUdpOptions.m_t38FaxMaxBuffer;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"T38FaxMaxBuffer:%lu\n",(unsigned long)profile.m_t38FaxUdpOptions.m_t38FaxMaxBuffer);
					t38_options->T38FaxMaxDatagram = profile.m_t38FaxUdpOptions.m_t38FaxMaxDatagram;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"T38FaxMaxDatagram:%lu\n",(unsigned long)profile.m_t38FaxUdpOptions.m_t38FaxMaxDatagram);
					if (profile.m_t38FaxUdpOptions.m_t38FaxUdpEC.GetTag() == H245_T38FaxUdpOptions_t38FaxUdpEC::e_t38UDPFEC)
						t38_options->T38FaxUdpEC = "t38UDPFEC";
					else
						t38_options->T38FaxUdpEC = "t38UDPRedundancy";
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"T38FaxUdpEC:%s\n",t38_options->T38FaxUdpEC);
					const char *uuid = switch_channel_get_partner_uuid(m_fsChannel); 
					if (uuid != NULL) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"uuid:%s\n",uuid);
						
						switch_core_session_t *session = switch_core_session_locate(switch_channel_get_partner_uuid(m_fsChannel));
						if (session) {
							switch_channel_t * channel = switch_core_session_get_channel(session);
							if (channel) {
								switch_channel_set_private(channel, "t38_options", t38_options);
							}else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no channel?\n");
							}
							switch_core_session_rwunlock(session);
						}else{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no session\n");
						}						
					
					
						switch_core_session_message_t msg = { 0 };
						int insist = 0;
						const char *v;
						if ((v = switch_channel_get_variable(m_fsChannel, "fax_enable_t38_insist"))) {
							insist = switch_true(v);
						} 
						msg.from = switch_channel_get_name(m_fsChannel);
						msg.message_id = SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA;
						msg.numeric_arg = insist;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"uuid:%s\n",switch_channel_get_uuid(switch_core_session_get_channel(switch_channel_get_session(m_fsChannel))));					
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"uuid:%s\n",uuid);
						switch_core_session_message_send(uuid,&msg);
					} else {
						switch_channel_set_private(m_fsChannel, "t38_options", t38_options);
					}
				}
			}
		} 
	}
	H323Connection::OnModeChanged(newMode);
}

bool FSH323Connection::OnSendSignalSetup(H323SignalPDU & setupPDU)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>PFSH323Connection::OnSendSignalSetup [%p]\n",this);
	setupPDU.GetQ931().SetBearerCapabilities(Q931::TransferSpeech, 1); 
	return true;
}

void FSH323Connection::setRemoteAddress(const char* remoteIP, WORD remotePort)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>PFSH323Connection::setRemoteAddress remoteIP = %s , remotePort = %d [%p]\n",remoteIP,remotePort,this);

	if (!m_remotePort) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Got remote RTP address %s:%d [%p]\n",remoteIP,remotePort,this);
		m_remotePort = remotePort;
		m_remoteAddr = remoteIP;
	}
}

switch_status_t FSH323Connection::on_execute()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::on_execute [%p]\n",this);
    
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::on_routing()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::on_routing [%p]\n",this);
   
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::kill_channel(int sig)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::kill_channel sig = %d [%p]\n",sig,this);

	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);

	if (!tech_pvt) {
		return SWITCH_STATUS_FALSE;
	}
	
	switch (sig) {
		case SWITCH_SIG_BREAK:
			if (switch_rtp_ready(tech_pvt->rtp_session)) {
				switch_rtp_break(tech_pvt->rtp_session);
			}
			break;
		case SWITCH_SIG_KILL:
		default:
			m_rxAudioOpened.Signal();
			m_txAudioOpened.Signal();        
			if (switch_rtp_ready(tech_pvt->rtp_session)) {
				if ((m_rtp_resetting == 1)) {
					switch_core_session_unlock_codec_read(m_fsSession);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_read [%p]\n",m_fsSession);
					switch_core_session_unlock_codec_write(m_fsSession);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_write [%p]\n",m_fsSession);
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"--->Kill soket [%p]\n",this);
				switch_rtp_kill_socket(tech_pvt->rtp_session);
			}
			break;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::send_dtmf(const switch_dtmf_t *dtmf)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::send_dtmf [%p]\n",this);

	SendUserInputTone(dtmf->digit, dtmf->duration);
	return SWITCH_STATUS_SUCCESS;
}

void FSH323Connection::SendUserInputTone(char tone, unsigned duration, unsigned logicalChannel, unsigned rtpTimestamp)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::SendUserInputTone [%p]\n",this);
	H323Connection::SendUserInputTone(tone, duration);
}

void FSH323Connection::OnUserInputTone(char tone, unsigned duration, unsigned logicalChannel, unsigned rtpTimestamp)
{
	if (m_select_dtmf == 0 || m_select_dtmf == 1){
		m_select_dtmf = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnUserInputTone [%p]\n",this);
		switch_dtmf_t dtmf = { tone, duration };
		switch_channel_queue_dtmf(m_fsChannel, &dtmf);
		H323Connection::OnUserInputTone( tone,  duration, logicalChannel, rtpTimestamp);
	}
}

void FSH323Connection::OnUserInputString(const PString &value)
{
	if (m_select_dtmf == 0 || m_select_dtmf == 2){
		m_select_dtmf = 2;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::OnUserInputString [%p]\n",this);
		switch_dtmf_t dtmf = { value[0], 500 };
		switch_channel_queue_dtmf(m_fsChannel, &dtmf);
		H323Connection::OnUserInputString(value);
	}
}

void FSH323Connection::CleanUpOnCall()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::CleanUpOnCall [%p]\n",this);
	connectionState = ShuttingDownConnection;
}

switch_status_t FSH323Connection::receive_message(switch_core_session_message_t *msg)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::receive_message MSG = %d\n",msg->message_id);

	switch_channel_t *channel = switch_core_session_get_channel(m_fsSession);
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);

	switch (msg->message_id) {
		case SWITCH_MESSAGE_INDICATE_BRIDGE:
		case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:
			switch_channel_set_private_flag(channel, CF_NEED_FLUSH);
			break;
		default:
			break;
	}

	switch (msg->message_id) {
		case SWITCH_MESSAGE_INDICATE_RINGING:	{
			AnsweringCall(AnswerCallPending);
			break;
		}
		case SWITCH_MESSAGE_INDICATE_DEFLECT:	{
			if (msg->string_arg != NULL)
				TransferCall(msg->string_arg);
			break;
		}
		case SWITCH_MESSAGE_INDICATE_PROGRESS:	{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_lock\n");
			switch_mutex_lock(tech_pvt->h323_mutex);
			if (m_txChannel && m_rxChannel){
				m_callOnPreAnswer = true;
			}
			switch_mutex_unlock(tech_pvt->h323_mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
			AnsweringCall(AnswerCallPending);
			AnsweringCall(AnswerCallDeferredWithMedia);
			
			if (m_txChannel && m_rxChannel){
				if (!switch_channel_test_flag(m_fsChannel, CF_EARLY_MEDIA)) {
					switch_channel_mark_pre_answered(m_fsChannel);
				}
			} else { 
				m_callOnPreAnswer = true;
				if (fastStartState == FastStartDisabled){
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-------------------->m_txAudioOpened.Wait START [%p]\n",this);
					m_txAudioOpened.Wait();
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-------------------->m_rxAudioOpened.Wait STOP [%p]\n",this);
				}
			}
			break;
		}
		case SWITCH_MESSAGE_INDICATE_ANSWER:	{
			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				return SWITCH_STATUS_FALSE;
			}
			AnsweringCall(H323Connection::AnswerCallNow);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Media started on connection [%p]\n",this);
		
			if (m_txChannel && m_rxChannel){
				if (!switch_channel_test_flag(m_fsChannel, CF_EARLY_MEDIA)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-------------------->switch_channel_mark_answered(m_fsChannel) [%p]\n",this);					
					switch_channel_mark_answered(m_fsChannel);
				}
			} else{
				m_ChannelAnswer =  true;
				if (fastStartState == FastStartDisabled){
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-------------------->m_txAudioOpened.Wait START [%p]\n",this);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-------------------->m_rxAudioOpened.Wait START [%p]\n",this);
					m_txAudioOpened.Wait();
					m_rxAudioOpened.Wait();
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-------------------->m_txAudioOpened.Wait STOP [%p]\n",this);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-------------------->m_rxAudioOpened.Wait STOP [%p]\n",this);
				}
			}
			break;
		}
		case SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA:	{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Received message SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA on connection [%p]\n",this);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_lock\n");
			switch_mutex_lock(tech_pvt->h323_mutex);
			if (!m_isRequst_fax)
				m_isRequst_fax = true;
			switch_mutex_unlock(tech_pvt->h323_mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
			if (m_active_channel_fax)
				RequestModeChangeT38("T.38\nT.38");
			else 
				m_active_channel_fax = true;
			break;
		}
		case SWITCH_MESSAGE_INDICATE_UDPTL_MODE:{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Received message SWITCH_MESSAGE_INDICATE_UDPTL_MODE on connection [%p]\n",this);
			if (switch_rtp_ready(tech_pvt->rtp_session)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"---->switch_rtp_udptl_mode [%p]\n",this);
				switch_rtp_udptl_mode(tech_pvt->rtp_session);
			}
			break;
		}
		case SWITCH_MESSAGE_INDICATE_T38_DESCRIPTION:{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Received message SWITCH_MESSAGE_INDICATE_T38_DESCRIPTION on connection [%p]\n",this);
			if (switch_rtp_ready(tech_pvt->rtp_session)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"---->switch_rtp_udptl_mode [%p]\n",this);
				switch_rtp_udptl_mode(tech_pvt->rtp_session);
			}
			break;
		}
		case SWITCH_MESSAGE_INDICATE_DEBUG_MEDIA:{
			if (switch_rtp_ready(tech_pvt->rtp_session) && !zstr(msg->string_array_arg[0]) && !zstr(msg->string_array_arg[1])) {
				switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {(switch_rtp_flag_t)0};
				int x = 0;

				if (!strcasecmp(msg->string_array_arg[0], "read")) {
					x++; flags[SWITCH_RTP_FLAG_DEBUG_RTP_READ] = (switch_rtp_flag_t)1;
				} else if (!strcasecmp(msg->string_array_arg[0], "write")) {
					x++; flags[SWITCH_RTP_FLAG_DEBUG_RTP_WRITE] = (switch_rtp_flag_t)1;
				} else if (!strcasecmp(msg->string_array_arg[0], "both")) {
					x++;
					flags[SWITCH_RTP_FLAG_DEBUG_RTP_READ] = (switch_rtp_flag_t)1;
					flags[SWITCH_RTP_FLAG_DEBUG_RTP_WRITE] = (switch_rtp_flag_t)1;
				} else if (*msg->string_array_arg[0] == 'v') {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_fsSession), SWITCH_LOG_ERROR, "Video is not supported yet\n");
					break;
				}

				if (x) {
					if (switch_true(msg->string_array_arg[1])) {
						switch_rtp_set_flags(tech_pvt->rtp_session, flags);
					} else {
						switch_rtp_clear_flags(tech_pvt->rtp_session, flags);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_fsSession), SWITCH_LOG_ERROR, "Invalid Options\n");
				}
			}
			break;
		}
		default:{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Received message id = %d [%p]\n", msg->message_id,this);
		}	
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::receive_event(switch_event_t *event)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::receive_event event id = %d [%p]\n",event->event_id,this);
//	PTRACE(4, "mod_h323\tReceived event " << event->event_id << " on connection " << *this);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::state_change()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::state_change [%p]\n",this);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"State changed on connection [%p]\n",this);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::on_init()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::on_init [%p]\n",this);

	switch_channel_t *channel = switch_core_session_get_channel(m_fsSession);
	if (channel == NULL) {
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Started routing for connection [%p]\n",this);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::on_exchange_media()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::on_exchange_media [%p]\n",this);
    
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::on_soft_execute()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::on_soft_execute [%p]\n",this);
	
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::read_audio_frame(switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
#ifdef DEBUG_RTP_PACKETS
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::read_audio_frame [%p]\n",this);
#endif

	/*switch_channel_t *channel = NULL;
	h323_private_t *tech_pvt = NULL;
	int payload = 0;

	channel = m_fsChannel;
	assert(channel != NULL);

	tech_pvt = (h323_private_t *)switch_core_session_get_private(m_fsSession);
	assert(tech_pvt != NULL);

	while (!(tech_pvt->read_codec.implementation && switch_rtp_ready(tech_pvt->rtp_session))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			PTRACE(4, "mod_h323\t<======FSH323Connection::read_audio_frame " << this);
			return SWITCH_STATUS_GENERR;
		}
	}

	tech_pvt->read_frame.datalen = 0;
	switch_set_flag_locked(tech_pvt, TFLAG_READING);

	switch_status_t status;

	switch_assert(tech_pvt->rtp_session != NULL);
	tech_pvt->read_frame.datalen = 0;


	while (tech_pvt->read_frame.datalen == 0) {
		tech_pvt->read_frame.flags = SFF_NONE;

		status = switch_rtp_zerocopy_read_frame(tech_pvt->rtp_session, &tech_pvt->read_frame, flags);
		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			PTRACE(4, "mod_h323\t<======FSH323Connection::read_audio_frame " << this);
			return SWITCH_STATUS_FALSE;
		}

		payload = tech_pvt->read_frame.payload;

		if (switch_rtp_has_dtmf(tech_pvt->rtp_session)) {
			switch_dtmf_t dtmf = { 0 };
			switch_rtp_dequeue_dtmf(tech_pvt->rtp_session, &dtmf);
			switch_channel_queue_dtmf(channel, &dtmf);
		}

		if (tech_pvt->read_frame.datalen > 0) {
			size_t bytes = 0;
			int frames = 1;

			if (!switch_test_flag((&tech_pvt->read_frame), SFF_CNG)) {
				if ((bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_packet)) {
					frames = (tech_pvt->read_frame.datalen / bytes);
				}
				tech_pvt->read_frame.samples = (int) (frames * tech_pvt->read_codec.implementation->samples_per_packet);
			}
			break;
		}
	}

	switch_clear_flag_locked(tech_pvt, TFLAG_READING);

	if (tech_pvt->read_frame.datalen == 0) {
		*frame = NULL;
		PTRACE(4, "mod_h323\t<======FSH323Connection::read_audio_frame " << this);
		return SWITCH_STATUS_GENERR;
	}

	*frame = &tech_pvt->read_frame;

	PTRACE(4, "mod_h323\t<======FSH323Connection::read_audio_frame " << this);
	return SWITCH_STATUS_SUCCESS;*/

	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	tech_pvt->read_frame.flags = 0;

	switch_set_flag_locked(tech_pvt, TFLAG_READING);

	if (!switch_channel_ready(m_fsChannel)) {
		switch_clear_flag_locked(tech_pvt, TFLAG_READING);		
#ifdef DEBUG_RTP_PACKETS
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::read_audio_frame END\n");
#endif
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(tech_pvt->h323_io_mutex);
	if (switch_test_flag(tech_pvt, TFLAG_IO)) {
		if (!switch_core_codec_ready(&tech_pvt->read_codec )) {
			switch_clear_flag_locked(tech_pvt, TFLAG_READING);
			switch_mutex_unlock(tech_pvt->h323_io_mutex);
#ifdef DEBUG_RTP_PACKETS
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::read_audio_frame END\n\n");
#endif
			return SWITCH_STATUS_FALSE;
		}
#ifdef DEBUG_RTP_PACKETS
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::read_audio_frame ---> switch_rtp_zerocopy_read_frame start\n");
#endif
		switch_status_t status = switch_rtp_zerocopy_read_frame(tech_pvt->rtp_session, &tech_pvt->read_frame, flags);
#ifdef DEBUG_RTP_PACKETS
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::read_audio_frame ---> switch_rtp_zerocopy_read_frame stop\n");
#endif
		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			switch_clear_flag_locked(tech_pvt, TFLAG_READING);
			switch_mutex_unlock(tech_pvt->h323_io_mutex);
#ifdef DEBUG_RTP_PACKETS
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::read_audio_frame END\n\n");
#endif
			return status;			
		}

//		PTRACE(4, "mod_h323\t--------->\n source = "<<tech_pvt->read_frame.source<< "\n packetlen = "<<tech_pvt->read_frame.packetlen<<"\n datalen = "<<tech_pvt->read_frame.datalen<<"\n samples = "<<tech_pvt->read_frame.samples<<"\n rate = "<<tech_pvt->read_frame.rate<<"\n payload = "<<(int)tech_pvt->read_frame.payload<<"\n timestamp = "<<tech_pvt->read_frame.timestamp<<"\n seq = "<<tech_pvt->read_frame.seq<<"\n ssrc = "<<tech_pvt->read_frame.ssrc);

		if (tech_pvt->read_frame.flags & SFF_CNG) {
			tech_pvt->read_frame.buflen = sizeof(m_buf);
			tech_pvt->read_frame.data = m_buf;
			tech_pvt->read_frame.packet = NULL;
			tech_pvt->read_frame.packetlen = 0;
			tech_pvt->read_frame.timestamp = 0;
			tech_pvt->read_frame.m = SWITCH_FALSE;
			tech_pvt->read_frame.seq = 0;
			tech_pvt->read_frame.ssrc = 0;
			/* The codec has alrady been set here */ //tech_pvt->read_frame.codec = &tech_pvt->read_codec ;
		} else {
			/* The codec has alrady been set here */ //tech_pvt->read_frame.codec = &tech_pvt->read_codec ;
		}
	} else {
		switch_mutex_unlock(tech_pvt->h323_io_mutex);
#ifdef DEBUG_RTP_PACKETS
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"--------->TFLAG_IO OFF\n");
#endif
		switch_yield(10000);
	}

	switch_mutex_unlock(tech_pvt->h323_io_mutex);
	switch_clear_flag_locked(tech_pvt, TFLAG_READING);
	
	*frame = &tech_pvt->read_frame;
#ifdef DEBUG_RTP_PACKETS
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::read_audio_frame END\n\n");
#endif

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::write_audio_frame(switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
#ifdef DEBUG_RTP_PACKETS
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::write_audio_frame [%p]\n",this);
#endif
	
	/*switch_channel_t *channel = NULL;
	h323_private_t *tech_pvt = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	
	channel = switch_core_session_get_channel(m_fsSession);
	assert(channel != NULL);

	tech_pvt = (h323_private_t *)switch_core_session_get_private(m_fsSession);
	assert(tech_pvt != NULL);

#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
	if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
		switch_swap_linear(frame->data, (int) frame->datalen / 2);
	}
#endif

	switch_set_flag_locked(tech_pvt, TFLAG_WRITING);

	switch_rtp_write_frame(tech_pvt->rtp_session, frame);

	switch_clear_flag_locked(tech_pvt, TFLAG_WRITING);

	PTRACE(4, "mod_h323\t<======FSH323Connection::write_audio_frame " << this);
	return status;*/

	switch_status_t status = SWITCH_STATUS_SUCCESS;
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	switch_assert(tech_pvt != NULL);
	
	if (!switch_channel_ready(m_fsChannel)) {
#ifdef DEBUG_RTP_PACKETS
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::write_audio_frame END\n\n");
#endif
		return SWITCH_STATUS_FALSE;
	}
	
	while (!(tech_pvt->read_codec.implementation && switch_rtp_ready(tech_pvt->rtp_session))) {
		if (switch_channel_ready(m_fsChannel)) {
			switch_yield(10000);
		} else {
#ifdef DEBUG_RTP_PACKETS
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::write_audio_frame END\n\n");
#endif
			return SWITCH_STATUS_GENERR;
		}
	}
	
	if (!switch_core_codec_ready(&tech_pvt->read_codec) || !tech_pvt->read_codec.implementation) {
#ifdef DEBUG_RTP_PACKETS
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::write_audio_frame END\n\n");
#endif
		return SWITCH_STATUS_GENERR;
	}

	if ((frame->flags & SFF_CNG)) {
#ifdef DEBUG_RTP_PACKETS
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::write_audio_frame END\n\n");
#endif
		return SWITCH_STATUS_SUCCESS;
	}

	switch_set_flag_locked(tech_pvt, TFLAG_WRITING);
	
	if (switch_rtp_write_frame(tech_pvt->rtp_session, frame) < 0) {
		status = SWITCH_STATUS_GENERR;
	}
	
	switch_clear_flag_locked(tech_pvt, TFLAG_WRITING);
#ifdef DEBUG_RTP_PACKETS
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::write_audio_frame END\n\n");
#endif

	return status;
}

switch_status_t FSH323Connection::read_video_frame(switch_frame_t **frame, switch_io_flag_t flag, int stream_id)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::read_video_frame [%p]\n",this);
	return SWITCH_STATUS_FALSE; /* Not yet implemented */
}

switch_status_t FSH323Connection::write_video_frame(switch_frame_t *frame, switch_io_flag_t flag, int stream_id)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323Connection::write_video_frame [%p]\n",this);
	return SWITCH_STATUS_FALSE; /* Not yet implemented */
}

///////////////////////////////////////////////////////////////////////

FSH323_ExternalRTPChannel::FSH323_ExternalRTPChannel(FSH323Connection& connection,
							const H323Capability& capability,
							Directions direction, 
							unsigned sessionID,
							const PIPSocket::Address& ip, 
							WORD dataPort)
	: H323_ExternalRTPChannel(connection, capability, direction, sessionID,ip,dataPort)
	, m_conn(&connection)
	, m_fsSession(connection.GetSession())
	, m_capability(&capability)
	, m_RTPlocalPort(dataPort)
	, m_sessionID(sessionID)
	, m_rtp_resetting(0)
{ 
	m_RTPlocalIP = (const char *)ip.AsString();
	SetExternalAddress(H323TransportAddress(ip, dataPort), H323TransportAddress(ip, dataPort+1));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323_ExternalRTPChannel::FSH323_ExternalRTPChannel sessionID = %d :%s addr = %s:%d [%p]\n",sessionID,GetDirections[GetDirection()],(const char*)m_RTPlocalIP,m_RTPlocalPort,this);	

	m_fsChannel = switch_core_session_get_channel(m_fsSession);
	//SetExternalAddress(H323TransportAddress(localIpAddress, m_RTPlocalPort), H323TransportAddress(localIpAddress, m_RTPlocalPort+1));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------->capability.GetPayloadType() return = %s\n",((capability.GetPayloadType() <= RTP_DataFrame::LastKnownPayloadType)?PayloadTypesNames[capability.GetPayloadType()]:"[pt=128]"));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------->capability.GetFormatName() return = %s\n",(const char*)(capability.GetFormatName()));
	
	PString fname((const char *)capability.GetFormatName());
  
	if (fname.Find("{sw}") == (fname.GetLength() - 4))
		fname = fname.Mid(0,fname.GetLength()-4);
	if (fname.Find("{hw}") == (fname.GetLength() - 4))
		fname = fname.Mid(0,fname.GetLength()-4);
	
	OpalMediaFormat format(fname, FALSE);
	m_format = &format;
	payloadCode = (BYTE)format.GetPayloadType();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------->payloadCode = %d\n",(int)payloadCode);
}


FSH323_ExternalRTPChannel::~FSH323_ExternalRTPChannel()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323_ExternalRTPChannel::~FSH323_ExternalRTPChannel  %s [%p]\n",GetDirections[GetDirection()],this);		
	if (m_rtp_resetting) {
		switch_core_session_unlock_codec_read(m_fsSession);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_read [%p]\n",m_fsSession);
		switch_core_session_unlock_codec_write(m_fsSession);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_write [%p]\n",m_fsSession);	
	}
}

PBoolean FSH323_ExternalRTPChannel::Start()
{
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_lock\n");
	switch_mutex_lock(tech_pvt->h323_mutex);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323_ExternalRTPChannel::Start() [%p]\n",this);

	const char *err = NULL;
	switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {(switch_rtp_flag_t)0};
	char * timer_name = NULL;
	const char *var;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->m_sessionID = %d  m_active_sessionID = %d\n",sessionID,m_conn->m_active_sessionID);    
	if (!(m_conn && H323_ExternalRTPChannel::Start())) {
		switch_mutex_unlock(tech_pvt->h323_mutex);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
		return false;
	}
	
	if ((tech_pvt->me == NULL) || ((tech_pvt->me != NULL) && tech_pvt->me->m_channel_hangup)) {
		switch_mutex_unlock(tech_pvt->h323_mutex);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
		return false;
	}
	
	if ((m_conn->m_active_sessionID != 0) && (m_conn->m_active_sessionID != sessionID)) {
		if (switch_core_codec_ready(&tech_pvt->read_codec) || switch_core_codec_ready(&tech_pvt->write_codec)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->Changing Codec to %s\n",(const char*)GetH245CodecName(m_capability));
			m_conn->m_rxChannel = false;
			m_conn->m_txChannel = false;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_lock_codec_read [%p]\n",m_fsSession);
			switch_core_session_lock_codec_read(m_fsSession);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_lock_codec_write [%p]\n",m_fsSession);
			switch_core_session_lock_codec_write(m_fsSession);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_io_mutex_lock\n");
			switch_mutex_lock(tech_pvt->h323_io_mutex);
			switch_clear_flag_locked(tech_pvt, TFLAG_IO);
			switch_mutex_unlock(tech_pvt->h323_io_mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_io_mutex_unlock\n");
			m_conn->m_rtp_resetting = 1;
			m_rtp_resetting = 1;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_codec_read_destroy\n");
			switch_core_codec_destroy(&tech_pvt->read_codec);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_codec_write_destroy\n");
			switch_core_codec_destroy(&tech_pvt->write_codec);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_rtp_destroy\n");
			switch_rtp_destroy(&tech_pvt->rtp_session);
			m_conn->m_startRTP = false;
			tech_pvt->rtp_session = NULL;
		}
	}
	m_conn->m_active_sessionID = sessionID;
	bool isAudio = false;
	switch_codec_t *codec = NULL;
	unsigned m_codec_ms = m_capability->GetTxFramesInPacket();

	if (mod_h323_globals.ptime_override_value > -1)
		m_codec_ms = mod_h323_globals.ptime_override_value;

	switch (m_capability->GetMainType()){
		case H323Capability::e_Audio:{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------------->H323Capability::e_Audio\n");
			isAudio = true;
			break;
		}
		case H323Capability::e_Video:{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------------->H323Capability::e_Video\n");
			isAudio = false;
			break;
		}
		case H323Capability::e_Data:{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------------->H323Capability::e_Data\n");
			isAudio = true;
			m_codec_ms = 20;
			switch_channel_set_app_flag_key("T38", m_fsChannel, CF_APP_T38);
			break;
		}
		default:break;
	}
		
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->GetFrameSize() return = %u\n",(unsigned)m_format->GetFrameSize());
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->GetFrameTime() return = %u\n",m_format->GetFrameTime());
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->payloadCode = %d\n",(int)payloadCode);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->m_codec_ms return =  %u\n",m_codec_ms);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->m_capability->GetFormatName() return =  %s\n",(const char*)(m_capability->GetFormatName()));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->GetH245CodecName() return =  %s\n",(const char*)GetH245CodecName(m_capability));
	
	if (GetDirection() == IsReceiver) {
		codec = isAudio ? &tech_pvt->read_codec : &tech_pvt->vid_read_codec;
		//m_switchTimer = isAudio ? &tech_pvt->read_timer : &tech_pvt->vid_read_timer;
		m_conn->m_rxChannel = true;
	} else {
		codec = isAudio ? &tech_pvt->write_codec : &tech_pvt->vid_write_codec;
		m_conn->m_txChannel = true;
		if (m_conn->m_callOnPreAnswer) {
			codec = isAudio ? &tech_pvt->read_codec : &tech_pvt->vid_read_codec;
			//m_switchTimer = isAudio ? &tech_pvt->read_timer : &tech_pvt->vid_read_timer;
		}
	}
	
	tech_pvt->read_frame.codec = &tech_pvt->read_codec; /* Set codec here - no need to set it every time a frame is read */

	if (switch_core_codec_init(codec, GetH245CodecName(m_capability), NULL, NULL, // FMTP
		8000, m_codec_ms, 1,  // Channels
		SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,   // Settings
		switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {
        
		if (switch_core_codec_init(codec, GetH245CodecName(m_capability), NULL, NULL, // FMTP
			8000, 0, 1,  // Channels
			SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,   // Settings
			switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"%s 	Cannot initialise %s  %s codec for connection [%p]\n",switch_channel_get_name(m_fsChannel), ((GetDirection() == IsReceiver)? " read" : " write")
				, GetMainTypes[m_capability->GetMainType()],(const char*)(m_capability->GetFormatName()),this);
			switch_channel_hangup(m_fsChannel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
			switch_mutex_unlock(tech_pvt->h323_mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
			return false;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"%s  Unsupported ptime of %u on %s %s codec %s  for connection [%p]\n",switch_channel_get_name(m_fsChannel),m_codec_ms,((GetDirection() == IsReceiver)? " read" : " write")
			, GetMainTypes[m_capability->GetMainType()],(const char*)(m_capability->GetFormatName()),this);
	}

	if (GetDirection() == IsReceiver) {
		//m_readFrame.rate = tech_pvt->read_codec.implementation->actual_samples_per_second;
		
		if (isAudio) {
			switch_core_session_set_read_codec(m_fsSession, codec);
			/*if (switch_core_timer_init(m_switchTimer,
				"soft",
				codec->implementation->microseconds_per_packet / 1000,
				codec->implementation->samples_per_packet,
				switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {
				
				switch_core_codec_destroy(codec);
				codec = NULL;
				switch_mutex_unlock(tech_pvt->h323_mutex);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
				return false;
			}
			switch_channel_set_variable(m_fsChannel,"timer_name", "soft");*/
			if (m_conn->m_ChannelProgress)
				switch_core_session_set_write_codec(m_fsSession, codec);
		} else {
			switch_core_session_set_video_read_codec(m_fsSession, codec);
			switch_channel_set_flag(m_fsChannel, CF_VIDEO);
		}
	} else {
		if (isAudio) {
			switch_core_session_set_write_codec(m_fsSession, codec);
			if (m_conn->m_callOnPreAnswer){
				//m_readFrame.rate = tech_pvt->read_codec.implementation->actual_samples_per_second;
				switch_core_session_set_read_codec(m_fsSession, codec);
				/*if (switch_core_timer_init(m_switchTimer,
								   "soft",
								   m_switchCodec->implementation->microseconds_per_packet / 1000,
								   m_switchCodec->implementation->samples_per_packet,
								   switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {
						switch_core_codec_destroy(m_switchCodec);
						m_switchCodec = NULL;
						switch_mutex_unlock(tech_pvt->h323_mutex);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
						return false;
				}
				switch_channel_set_variable(m_fsChannel,"timer_name","soft");*/
			}	
		} else {
			switch_core_session_set_video_write_codec(m_fsSession, codec);
			switch_channel_set_flag(m_fsChannel, CF_VIDEO);
		}
	}
	
//	PTRACE(4, "mod_h323\tSet " << ((GetDirection() == IsReceiver)? " read" : " write") << ' '
//		<< m_capability->GetMainType() << " codec to << " << m_capability << " for connection " << *this);
		
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Set %s %s codec to %s  for connection [%p]\n",((GetDirection() == IsReceiver)? " read" : " write")
		,GetMainTypes[m_capability->GetMainType()],(const char*)m_capability->GetFormatName(),this);
		
	PIPSocket::Address remoteIpAddress;
	GetRemoteAddress(remoteIpAddress,m_RTPremotePort);	
	m_RTPremoteIP = (const char *)remoteIpAddress.AsString();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->tech_pvt->rtp_session = [%p]\n",tech_pvt->rtp_session);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->samples_per_packet = %lu\n", codec->implementation->samples_per_packet);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->actual_samples_per_second = %lu\n", codec->implementation->actual_samples_per_second);
	
	bool ch_port = false;
	if (tech_pvt->rtp_session != NULL){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->old remote port = %d new remote port = %d\n",switch_rtp_get_remote_port(tech_pvt->rtp_session),m_RTPremotePort);
		if ((switch_rtp_get_remote_port(tech_pvt->rtp_session) != m_RTPremotePort) && (GetDirection() != IsReceiver) && (m_conn->m_rtp_resetting == 1)){
			ch_port = true;
			m_conn->m_startRTP = false;
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_lock_codec_read [%p]\n",m_fsSession);
			switch_core_session_lock_codec_read(m_fsSession);			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_io_mutex_lock\n");
			switch_mutex_lock(tech_pvt->h323_io_mutex);
			switch_clear_flag_locked(tech_pvt, TFLAG_IO);
			switch_mutex_unlock(tech_pvt->h323_io_mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_io_mutex_unlock\n");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_rtp_destroy\n");
			switch_rtp_destroy(&tech_pvt->rtp_session);
		}
	}
	
	if ((!m_conn->m_startRTP)) {			
		flags[SWITCH_RTP_FLAG_DATAWAIT] = (switch_rtp_flag_t)1;
		flags[SWITCH_RTP_FLAG_RAW_WRITE] = (switch_rtp_flag_t)1;

		if (mod_h323_globals.use_rtp_timer) {
			flags[SWITCH_RTP_FLAG_USE_TIMER] = (switch_rtp_flag_t)1;
			timer_name = mod_h323_globals.rtp_timer_name;
		} else {
			if ((var = switch_channel_get_variable(m_fsChannel, "timer_name"))) {
				timer_name = (char *) var;
				flags[SWITCH_RTP_FLAG_USE_TIMER] = (switch_rtp_flag_t)1;
			}
		}

		if (timer_name)
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------->timer_name = %s\n", timer_name);

		tech_pvt->rtp_session = switch_rtp_new((const char *)m_RTPlocalIP,
							m_RTPlocalPort,
							(const char *)m_RTPremoteIP,
							m_RTPremotePort,
							(switch_payload_t)payloadCode,
							codec->implementation->samples_per_packet,	
							codec->implementation->microseconds_per_packet,
							flags, timer_name, &err,
							switch_core_session_get_pool(m_fsSession));

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------------->tech_pvt->rtp_session = %p\n",tech_pvt->rtp_session);
		m_conn->m_startRTP = true;
		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			switch_channel_set_flag(m_fsChannel, CF_FS_RTP);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
			switch_channel_hangup(m_fsChannel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);			
			switch_mutex_unlock(tech_pvt->h323_mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	if (ch_port){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_io_mutex_lock\n");
		switch_mutex_lock(tech_pvt->h323_io_mutex);
		switch_set_flag_locked(tech_pvt, TFLAG_IO);
		switch_mutex_unlock(tech_pvt->h323_io_mutex);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_io_mutex_unlock\n");
		switch_core_session_unlock_codec_read(m_fsSession);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_read [%p]\n",m_fsSession);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->External RTP address %s:%u\n",(const char*)m_RTPremoteIP,m_RTPremotePort);
	
	if (GetDirection() == IsReceiver) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_io_mutex_lock\n");
		switch_mutex_lock(tech_pvt->h323_io_mutex);
		switch_set_flag_locked(tech_pvt, TFLAG_IO);
		switch_mutex_unlock(tech_pvt->h323_io_mutex);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_io_mutex_unlock\n");
	}
	
	if (m_conn->m_rtp_resetting) {
		if (GetDirection() == IsReceiver) {
			switch_core_session_unlock_codec_read(m_fsSession);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_read [%p]\n",m_fsSession);
		} else {
			switch_core_session_unlock_codec_write(m_fsSession);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_write [%p]\n",m_fsSession);
			if (m_conn->m_callOnPreAnswer) {
				switch_core_session_unlock_codec_read(m_fsSession);	
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_read [%p]\n",m_fsSession);
			}
		}
	}
	
	if (GetDirection() == IsReceiver)
		m_conn->m_rxAudioOpened.Signal();
	else
		m_conn->m_txAudioOpened.Signal();                             
	
	if (m_conn->m_ChannelAnswer && m_conn->m_rxChannel &&  m_conn->m_txChannel)
		switch_channel_mark_answered(m_fsChannel);
		
	if ((m_conn->m_ChannelProgress && m_conn->m_rxChannel)||(m_conn->m_callOnPreAnswer && m_conn->m_txChannel))
		switch_channel_mark_pre_answered(m_fsChannel);

	if (m_capability->GetMainType() == H323Capability::e_Data && m_conn->m_rxChannel &&  m_conn->m_txChannel) {
		const char *uuid = switch_channel_get_partner_uuid(m_fsChannel); 
		if (uuid != NULL){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------------------->switch_rtp_udptl_mode\n");
			switch_rtp_udptl_mode(tech_pvt->rtp_session);
			switch_core_session_message_t msg = { 0 };
			msg.from = switch_channel_get_name(m_fsChannel);
			msg.message_id = SWITCH_MESSAGE_INDICATE_UDPTL_MODE;
			switch_core_session_message_send(uuid,&msg);
		}
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
	switch_mutex_unlock(tech_pvt->h323_mutex);

	return true;
}

PBoolean FSH323_ExternalRTPChannel::OnReceivedPDU(const H245_H2250LogicalChannelParameters& param, unsigned& errorCode)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323_ExternalRTPChannel::OnReceivedPDU [%p]\n",this);
	
	if (!H323_ExternalRTPChannel::OnReceivedPDU(param,errorCode))
		return true;
	PIPSocket::Address remoteIpAddress;
	WORD remotePort;
	GetRemoteAddress(remoteIpAddress,remotePort);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Remote RTP address %s:%u\n",(const char *)remoteIpAddress.AsString(),remotePort);
	m_conn->setRemoteAddress((const char *)remoteIpAddress.AsString(), remotePort);
	return true;
}

PBoolean FSH323_ExternalRTPChannel::OnSendingPDU(H245_H2250LogicalChannelParameters& param)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"FSH323_ExternalRTPChannel::OnSendingPDU [%p]\n",this);
	return H323_ExternalRTPChannel::OnSendingPDU(param);
}

PBoolean FSH323_ExternalRTPChannel::OnReceivedAckPDU(const H245_H2250LogicalChannelAckParameters& param)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323_ExternalRTPChannel::OnReceivedAckPDU [%p]\n",this);
	return H323_ExternalRTPChannel::OnReceivedAckPDU(param);
}

void FSH323_ExternalRTPChannel::OnSendOpenAck(H245_H2250LogicalChannelAckParameters& param)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323_ExternalRTPChannel::OnSendOpenAck [%p]\n",this);
	H323_ExternalRTPChannel::OnSendOpenAck(param);
}


FSH323Connection * FSH323EndPoint::FSMakeCall(const PString & dest, void *userData)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>FSH323EndPoint::FSMakeCall DST NUMBER = %s [%p]\n",(const char*)dest,this);
	
	FSH323Connection * connection;
	PString token;
	H323Transport *transport = NULL;
	
	if (listeners.GetSize() > 0) {
		H323TransportAddress taddr = listeners[0].GetTransportAddress();
		PIPSocket::Address addr;
		WORD port;
		if (taddr.GetIpAndPort(addr, port)) {				
			if (addr) {
				transport = new H323TransportTCP(*this, addr,false);
				if (!transport)						
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"----> Unable to create transport for outgoing call\n");
			}
		} else
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"----> Unable to get address and port\n");
	}
	
	if (!(connection = (FSH323Connection *)H323EndPoint::MakeCall(dest, token, userData))) {
		return NULL;
	}

	return connection;
}

static switch_call_cause_t create_outgoing_channel(switch_core_session_t *session,
                                                   switch_event_t *var_event,
                                                   switch_caller_profile_t *outbound_profile,
                                                   switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>create_outgoing_channel DST NUMBER = %s\n",outbound_profile->destination_number);
	
	FSH323Connection * connection;
	if (h323_process == NULL) {
		return SWITCH_CAUSE_CRASH;
	}
	FSH323EndPoint & ep = h323_process->GetH323EndPoint();
	if (!(connection = ep.FSMakeCall(outbound_profile->destination_number,outbound_profile))){
		return SWITCH_CAUSE_PROTOCOL_ERROR;
	}

	*new_session = connection->GetSession();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"--------->GetSession() return = [%p]\n",connection->GetSession());
	return SWITCH_CAUSE_SUCCESS;
}


static switch_status_t on_destroy(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>on_destroy\n");

	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(session);
    
	if (tech_pvt) {
		if (tech_pvt->read_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}
		if (tech_pvt->write_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
		if (tech_pvt->vid_read_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->vid_read_codec);
		}
		if (tech_pvt->vid_write_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->vid_write_codec);
		}
		if (tech_pvt->read_timer.timer_interface) {
			switch_core_timer_destroy(&tech_pvt->read_timer);
		}
		if (tech_pvt->vid_read_timer.timer_interface) {
			switch_core_timer_destroy(&tech_pvt->vid_read_timer);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t on_hangup(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"======>switch_status_t on_hangup [%p]\n",session);

	switch_channel_t *channel = switch_core_session_get_channel(session);
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(session);
	FSH323Connection *me = tech_pvt->me;
	FSH323EndPoint & ep = h323_process->GetH323EndPoint();
	tech_pvt->me = NULL;
    
	if (me) {
		if (me->m_rtp_resetting == 1) {
			switch_core_session_unlock_codec_read(session);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_read [%p]\n",session);
			switch_core_session_unlock_codec_write(session);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->switch_core_session_unlock_codec_write [%p]\n",session);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_lock\n");
		switch_mutex_lock(tech_pvt->h323_mutex);
		me->m_channel_hangup = true;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
		switch_mutex_unlock(tech_pvt->h323_mutex);
		if (me->TryLock() == 1) {
			me->CloseAllLogicalChannels(true);
			me->CloseAllLogicalChannels(false);
			me->Unlock();
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"----->%s\n",(const char *)(tech_pvt->token));
		Q931::CauseValues cause = (Q931::CauseValues)switch_channel_get_cause_q850(channel);
		int trylock = me->TryLock();
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-----> () = %d\n",trylock);
		if (trylock == 1) {
			const PString currentToken(tech_pvt->token);
			FSH323Connection *connection = (FSH323Connection *)me->GetEndPoint()->FindConnectionWithLock(currentToken); 
			if (connection) {
				connection->Unlock();
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-----> connection->UnLock()\n");
			}
			me->Unlock();
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"-----> UnLock()\n");
		} else if (trylock == -1) {
			/* Failed to lock - just go on */
		}
		const PString currentToken(tech_pvt->token);
		me->SetQ931Cause(cause);
//		me->ClearCallSynchronous(NULL, H323TranslateToCallEndReason(cause, UINT_MAX));
		ep.ClearCall(currentToken, H323TranslateToCallEndReason(cause, UINT_MAX));
//		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_lock\n");
//		switch_mutex_lock(tech_pvt->h323_mutex);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_lock\n");
	switch_mutex_lock(tech_pvt->h323_mutex);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"------------->h323_mutex_unlock\n");
	switch_mutex_unlock(tech_pvt->h323_mutex);

	while (tech_pvt->active_connection){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Wait clear h323 connection\n");
		h_timer(1);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "H323 connection was cleared successfully\n");
	
	return SWITCH_STATUS_SUCCESS;
}
