/*
	Version 0.0.20
*/

#include "mod_h323.h"

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, mod_h323_globals.codec_string);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_context, mod_h323_globals.context);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, mod_h323_globals.dialplan);


SWITCH_MODULE_LOAD_FUNCTION(mod_h323_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_h323_shutdown);
SWITCH_MODULE_DEFINITION(mod_h323, mod_h323_load, mod_h323_shutdown, NULL);

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

static FSProcess *opal_process = NULL;
SWITCH_MODULE_LOAD_FUNCTION(mod_h323_load){
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Starting loading mod_h323\n");
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
			
    if (!*module_interface) {
        return SWITCH_STATUS_MEMERR;
    }

    h323_process = new FSProcess();
    if (h323_process == NULL) {
        return SWITCH_STATUS_MEMERR;
    }

    if (h323_process->Initialise(*module_interface)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Opal manager initialized and running\n");
        //unloading causes a seg in linux
        return SWITCH_STATUS_NOUNLOAD;
        //return SWITCH_STATUS_SUCCESS;
    }

    delete h323_process;
    h323_process = NULL;
    return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_h323_shutdown){
    
    switch_safe_free(mod_h323_globals.context);
    switch_safe_free(mod_h323_globals.dialplan);
    switch_safe_free(mod_h323_globals.codec_string);
    delete h323_process;
    h323_process = NULL;
    return SWITCH_STATUS_SUCCESS;
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

            int bufSize = pptr() - pbase();

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

PString GetH245CodecName(const H323Capability* cap){
	
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
	return "Unknown";
}

FSProcess::FSProcess()
  : PLibraryProcess("Test", "mod_h323", 1, 0, AlphaCode, 1)
  , m_h323endpoint(NULL){
}

FSProcess::~FSProcess(){
  delete m_h323endpoint;
}

bool FSProcess::Initialise(switch_loadable_module_interface_t *iface){
	PTRACE(4, "mod_h323\t======>FSProcess::Initialise " << *this);
	
  m_h323endpoint = new FSH323EndPoint();
  return m_h323endpoint != NULL && m_h323endpoint->Initialise(iface);
}

bool FSH323EndPoint::Initialise(switch_loadable_module_interface_t *iface){
	PTRACE(4, "mod_h323\t======>FSManager::Initialise " << *this);
    ReadConfig(false);

    PTrace::SetLevel(mod_h323_globals.trace_level);        //just for fun and eyecandy ;)
    PTrace::SetOptions(PTrace::TraceLevel);
    PTrace::SetStream(new FSTrace);

    m_freeswitch = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(iface, SWITCH_ENDPOINT_INTERFACE);
    m_freeswitch->interface_name = modulename;
    m_freeswitch->io_routines = &h323fs_io_routines;
    m_freeswitch->state_handler = &h323fs_event_handlers;
			
	PString codec = ((const char *)mod_h323_globals.codec_string);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Config capabilliti %s \n",(const char *)codec);

	if (!codec.IsEmpty()) {		
		const char** f = h323_formats;
		for (; *f; f += 2) {			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Find capabilliti %s to %s\n",f[1],(const char *)codec);
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

	
    AddAllUserInputCapabilities(0,1);
	PTRACE(1, "OpenPhone\tCapability Table:\n" << setprecision(4) << capabilities);
	
	DisableFastStart(!m_faststart);
    DisableH245Tunneling(!m_h245tunneling);
    DisableH245inSetup(!m_h245insetup);	
	 
    if (m_listeners.empty()) {
        StartListener("");
    } else {
        for (std::list < FSListener >::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it) {
            if (!StartListener(it->listenAddress)) {
                PTRACE(3, "mod_h323\tCannot start listener for " << it->name);
            }
        }
    }

    if (!m_gkAddress.IsEmpty() && !m_gkIdentifer.IsEmpty() && !m_gkInterface.IsEmpty()) {
		m_thread = new FSGkRegThread(this,&m_gkAddress,&m_gkIdentifer,&m_gkInterface,m_gkretry);
		m_thread->SetAutoDelete();
		m_thread->Resume();
    }
	
    return TRUE;
}

switch_status_t FSH323EndPoint::ReadConfig(int reload){
	PTRACE(4, "mod_h323\t======>FSH323EndPoint::ReadConfig " << *this);
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
            } else if (!strcasecmp(var, "jitter-size")) {
                char * next;
                unsigned minJitter = strtoul(val, &next, 10);
                if (minJitter >= 10) {
                    unsigned maxJitter = minJitter;
                    if (*next == ',')
                      maxJitter = atoi(next+1);
                    SetAudioJitterDelay(minJitter, maxJitter); // In milliseconds
                } else{
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set zero Jitter buffer\n");
					SetAudioJitterDelay(0, 0);
				}
			} else if (!strcasecmp(var, "faststart")) {
				m_faststart = switch_true(val);
			} else if (!strcasecmp(var, "h245tunneling")) {
				m_h245tunneling = switch_true(val);
			} else if (!strcasecmp(var, "h245insetup")) {
				m_h245insetup = switch_true(val);
            } else if (!strcasecmp(var, "gk-address")) {
                m_gkAddress = val;
            } else if (!strcasecmp(var, "gk-identifer")) {
                m_gkIdentifer = val;
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

            listener.listenAddress = new H323ListenerTCP(*this,ip,port);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created Listener '%s'\n", (const char *) listener.name);
        }
    }

    switch_event_destroy(&params);

    if (xml)
        switch_xml_free(xml);

    return status;
}

FSH323EndPoint::FSH323EndPoint()
	:m_faststart(true)
	,m_h245tunneling(true)
	,m_h245insetup(true)
{
	PTRACE(4, "mod_h323\t======>FSH323EndPoint::FSH323EndPoint [" << *this<<"]");
	terminalType = e_GatewayOnly;
}	

FSH323EndPoint::~FSH323EndPoint(){
	PTRACE(4, "mod_h323\t======>FSH323EndPoint::~FSH323EndPoint [" << *this<<"]");
	StopGkClient();
}

H323Connection  *FSH323EndPoint::CreateConnection(
	unsigned callReference,
	void* userData,
	H323Transport* transport,
	H323SignalPDU* setupPDU){
	PTRACE(4, "mod_h323\t======>FSH323EndPoint::CreateConnection callReference = "<< callReference <<" userDate = "<<userData<<" [" << *this<<"]");

	if ((switch_caller_profile_t *)userData){
		PTRACE(4, "mod_h323\t------> SWITCH_CALL_DIRECTION_OUTBOUND");
	} else{
		PTRACE(4, "mod_h323\t------> SWITCH_CALL_DIRECTION_INBOUND");
	}
	
    switch_core_session_t *fsSession = switch_core_session_request(GetSwitchInterface(), 
                                       (switch_caller_profile_t *)userData ? SWITCH_CALL_DIRECTION_OUTBOUND : SWITCH_CALL_DIRECTION_INBOUND, NULL);
    if (fsSession == NULL)
        return NULL;
	
	PTRACE(4, "mod_h323\t------> fsSession = "<<fsSession);
    switch_channel_t *fsChannel = switch_core_session_get_channel(fsSession);

	if (fsChannel == NULL) {
        switch_core_session_destroy(&fsSession);
        return NULL;
	}
	
  return new FSH323Connection(*this,transport,callReference,(switch_caller_profile_t *)userData, fsSession, fsChannel);
}

bool FSH323EndPoint::OnSetGatewayPrefixes(PStringList & prefixes) const{
	PTRACE(4, "mod_h323\t======>FSH323EndPoint::OnSetGatewayPrefixes " << *this);
	if(m_gkPrefixes.GetSize() > 0) {
		PTRACE(4, "mod_h323\tOnSetGatewayPrefixes " << m_gkPrefixes);
		prefixes = m_gkPrefixes;
		return true;
	}
	return false;
}

void FSH323EndPoint::StartGkClient(int retry, PString* gkAddress,PString* gkIdentifer,PString* gkInterface){
	PTRACE(4, "mod_h323\t======>FSH323EndPoint::StartGkClient [" << *this<<"]");
	while(!UseGatekeeper(m_gkAddress, m_gkIdentifer, m_gkInterface) && retry > 0 ){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                          "Could not start gatekeeper: addr=\"%s\", id=\"%s\", if=\"%s\"\n",
                          (const char *)m_gkAddress,
                          (const char *)m_gkIdentifer,
                          (const char *)m_gkInterface);
			switch_yield(retry*1000);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Started gatekeeper: %s\n",
							(const char *)GetGatekeeper()->GetName());	
	m_thread = 0;	
}

void FSH323EndPoint::StopGkClient(){
	PTRACE(4, "mod_h323\t======> FSH323EndPoint::StopGkClient [" << *this<<"]");
	if (m_thread) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Started gatekeeper thread\n");
		RemoveGatekeeper();
		m_thread->Terminate();
		m_thread = 0;
    }
}

FSH323Connection::FSH323Connection(FSH323EndPoint& endpoint, H323Transport* transport, unsigned callReference,  switch_caller_profile_t *outbound_profile, switch_core_session_t *fsSession, switch_channel_t *fsChannel)
	: H323Connection(endpoint,callReference)
	, m_endpoint(&endpoint)
	, m_fsSession(fsSession)
    , m_fsChannel(fsChannel)
	, m_callOnPreAnswer(false)
	, m_startRTP(false)
	, m_rxChennel(false)
	, m_txChennel(false)
	, m_ChennelAnswer(false)
	, m_ChennelProgress(false){
	PTRACE(4, "mod_h323\t======>FSH323Connection::FSH323Connection [" << *this<<"]");

    h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_alloc(m_fsSession, sizeof(*tech_pvt));
    tech_pvt->me = this;
    switch_core_session_set_private(m_fsSession, tech_pvt);
	
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(m_fsSession));
	switch_mutex_init(&tech_pvt->h323_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(m_fsSession));
	
    if (outbound_profile != NULL) {
        SetLocalPartyName(outbound_profile->caller_id_number);
        SetDisplayName(outbound_profile->caller_id_name);

        switch_caller_profile_t *caller_profile = switch_caller_profile_clone(m_fsSession, outbound_profile);
        switch_channel_set_caller_profile(m_fsChannel, caller_profile);

        PString name = "h323/";
        name += outbound_profile->destination_number;
        switch_channel_set_name(m_fsChannel, name);

        switch_channel_set_flag(m_fsChannel, CF_OUTBOUND);
        switch_channel_set_state(m_fsChannel, CS_INIT);
    }
	
	m_RTPlocalPort = switch_rtp_request_port((const char *)m_RTPlocalIP.AsString());
}	

FSH323Connection::~FSH323Connection(){
    PTRACE(4, "mod_h323\t======>FSH323Connection::~FSH323Connection  ["<<*this<<"]");
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
    tech_pvt->me = NULL;
}	

void FSH323Connection::OnSetLocalCapabilities(){
	PTRACE(4, "mod_h323\t======>FSH323Connection::OnSetLocalCapabilities() [" << *this<<"]");
	H323Connection::OnSetLocalCapabilities();
	SetLocalCapabilities();
}

bool FSH323Connection::SetLocalCapabilities(){
	PTRACE(4, "mod_h323\t======>FSH323Connection::SetLocalCapabilities() Size local capability = "<<localCapabilities.GetSize()<<" [" << *this<<"]");
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
				PTRACE(1, "mod_h323\tRemoving capability '"<<fname<<"' ("<<format<<") not in remote '"<<m_globalcodec<<"'");
				changed = true;
				for (PINDEX idx = 0; idx < fastStartChannels.GetSize(); idx++) {
					if (fastStartChannels[idx].GetCapability() == localCapabilities[i]) {
						PTRACE(1, "mod_h323\tRemoving fast start channel "<<fastStartChannels[idx].GetDirection()<<" '"<<fname<<"' ("<<format<<")");
						fastStartChannels.RemoveAt(idx--);
					}		
				}
				localCapabilities.Remove(fname);
				i--;
			} else	nocodecs = false;
		}
    }
    if (nocodecs) {
		PTRACE(3, "mod_h323\tNo codecs remaining for H323 connection ["<<*this<<"]");
		changed = false;
		ClearCall(EndedByCapabilityExchange);		
    }
    return changed;
}

bool FSH323Connection::decodeCapability(const H323Capability& capability, const char** dataFormat, int* payload, PString* capabName){
	PTRACE(4, "mod_h323\t======>FSH323Connection::decodeCapability");
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
    
	PTRACE(1, "mod_h323\tcapability '"<< fname << "' format '"<<format<<"' payload "<<pload);
    if (format) {
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
    const H323SignalPDU &setupPDU, H323SignalPDU &connectPDU){
	PTRACE(4, "mod_h323\t======>FSH323Connection::OnAnswerCall caller = "<< caller<<" [" << *this<<"]");
	
	if (m_fsSession == NULL) {
        PTRACE(1, "mod_h323\tSession request failed.");
        return H323Connection::AnswerCallDenied;
    }

    switch_core_session_add_stream(m_fsSession, NULL);

    switch_channel_t *channel = switch_core_session_get_channel(m_fsSession);
    if (channel == NULL) {
        PTRACE(1, "mod_h323\tSession does not have a channel");
        return H323Connection::AnswerCallDenied;
    }

	const Q931& q931 = setupPDU.GetQ931();
	const H225_Setup_UUIE& setup = setupPDU.m_h323_uu_pdu.m_h323_message_body;
    const H225_ArrayOf_AliasAddress& address = setup.m_destinationAddress;
    for (int i = 0; i<address.GetSize(); i++)
		PTRACE(2, "mod_h323\t address index = "<<i<<" value = "<<(const char *)H323GetAliasAddressString(address[i]));
	PString called;
    if (address.GetSize() > 0)
		called = (const char *)H323GetAliasAddressString(address[0]);
    if (!called.IsEmpty())
		PTRACE(2, "mod_h323\t Called number or alias = "<<called);
    else {
		PString callnam;
		if (q931.GetCalledPartyNumber(callnam)) {
			called=(const char *)callnam;			
			PTRACE(2, "mod_h323\t Called-Party-Number = "<<called);
		}
    }
       
    switch_caller_profile_t *caller_profile = switch_caller_profile_new(switch_core_session_get_pool(m_fsSession),
                                                                        NULL,
                                                                        /** username */
                                                                        mod_h323_globals.dialplan,
                                                                        /** dial plan */
                                                                        GetRemotePartyName(),
                                                                        /** caller_id_name */
                                                                        GetRemotePartyNumber(),
                                                                        /** caller_id_number */
                                                                        NULL,
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
        PTRACE(1, "mod_h323\tCould not create caller profile");
        return H323Connection::AnswerCallDenied;
    }

    PTRACE(4, "mod_h323\tCreated switch caller profile:\n"
           "  username       = " << caller_profile->username << "\n"
           "  dialplan       = " << caller_profile->dialplan << "\n"
           "  caller_id_name     = " << caller_profile->caller_id_name << "\n"
           "  caller_id_number   = " << caller_profile->caller_id_number << "\n"
           "  network_addr   = " << caller_profile->network_addr << "\n"
           "  source         = " << caller_profile->source << "\n"
           "  context        = " << caller_profile->context << "\n" 
		   "  destination_number= " << caller_profile->destination_number);
    switch_channel_set_caller_profile(channel, caller_profile);

    char name[256] = "h323/";
    switch_copy_string(name + 5, caller_profile->destination_number, sizeof(name)-5);
    switch_channel_set_name(channel, name);
    switch_channel_set_state(channel, CS_INIT);

    if (switch_core_session_thread_launch(m_fsSession) != SWITCH_STATUS_SUCCESS) {
        PTRACE(1, "mod_h323\tCould not launch session thread");
        return H323Connection::AnswerCallDenied;
    }

	return H323Connection::AnswerCallDeferred;
}

H323Channel* FSH323Connection::CreateRealTimeLogicalChannel(const H323Capability& capability,H323Channel::Directions dir,unsigned sessionID,const H245_H2250LogicalChannelParameters* param, RTP_QOS * rtpqos){
	PTRACE(4, "mod_h323\t======>FSH323Connection::CreateRealTimeLogicalChannel [" << *this<<"]");
	
	H323TransportAddress m_h323transportadd = GetSignallingChannel()->GetLocalAddress();
	m_h323transportadd.GetIpAddress(m_RTPlocalIP);

	return new FSH323_ExternalRTPChannel(*this, capability, dir, sessionID,m_RTPlocalIP,m_RTPlocalPort);
}

PBoolean FSH323Connection::OnStartLogicalChannel(H323Channel & channel){
    PTRACE(4, "mod_h323\t======>FSH323Connection::OnStartLogicalChannel chennel = "<<&channel<<" ["<<*this<<"]");
    
	return true;
}

PBoolean FSH323Connection::OnCreateLogicalChannel(const H323Capability& capability, H323Channel::Directions dir, unsigned& errorCode){
    PTRACE(4, "mod_h323\t======>FSH323Connection::OnCreateLogicalChannel ('"<< (const char *)capability.GetFormatName()<<"',"<<dir<<") ["<<*this<<"]");
    
	return H323Connection::OnCreateLogicalChannel(capability,dir,errorCode);
}

void FSH323Connection::OnReceivedReleaseComplete(const H323SignalPDU & pdu){
	PTRACE(4, "mod_h323\t======>FSH323Connection::OnReceivedReleaseComplete cause = "<<pdu.GetQ931().GetCause()<<" value = "<<(switch_call_cause_t)pdu.GetQ931().GetCause());
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	tech_pvt->me = NULL;
	switch_channel_hangup(switch_core_session_get_channel(m_fsSession),(switch_call_cause_t)pdu.GetQ931().GetCause()); 
	return H323Connection::OnReceivedReleaseComplete(pdu);
}

bool FSH323Connection::OnReceivedProgress(const H323SignalPDU &pdu)
{
	PTRACE(4, "mod_h323\t======>FSH323Connection::OnReceivedProgress ["<<*this<<"]");
	H323Connection::OnReceivedProgress(pdu);
	if ((m_rxChennel && m_txChennel) || (m_ChennelProgress && m_rxChennel))
		switch_channel_mark_pre_answered(m_fsChannel);
	else{
		m_ChennelProgress = true;
	}
	return true;
}

bool FSH323Connection::OnReceivedSignalSetup(const H323SignalPDU & setupPDU){

	PTRACE(4, "mod_h323\t======>FSH323Connection::OnReceivedSignalSetup ["<<*this<<"]");
	
	if (!H323Connection::OnReceivedSignalSetup(setupPDU)) return false;
	
	H323SignalPDU callProceedingPDU;
	H225_CallProceeding_UUIE & callProceeding = callProceedingPDU.BuildCallProceeding(*this);
	
	if (SendFastStartAcknowledge(callProceeding.m_fastStart)){
					callProceeding.IncludeOptionalField(H225_CallProceeding_UUIE::e_fastStart);
	} else {
		PTRACE(2, "H323\tSendFastStartAcknowledge = FALSE ");
		if (connectionState == ShuttingDownConnection){
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
					
	if (!WriteSignalPDU(callProceedingPDU))
        return false;
		
	return true;		
}

bool FSH323Connection::OnReceivedCallProceeding(const H323SignalPDU & pdu){
	PTRACE(4, "mod_h323\t======>PFSH323Connection::OnReceivedCallProceeding ["<<*this<<"]");
	unsigned pi;

	
	if (!pdu.GetQ931().GetProgressIndicator(pi))
		pi = 0;
	PTRACE(4, "mod_h323\t----------->OnAlerting PI = "<<pi);
	if (pi > 0){
		if ((m_rxChennel && m_txChennel) || (m_ChennelProgress && m_rxChennel))
			switch_channel_mark_pre_answered(m_fsChannel);
		else{
			m_ChennelProgress = true;
		}
	}
	return H323Connection::OnReceivedCallProceeding(pdu);
}

bool FSH323Connection::OnSendCallProceeding(H323SignalPDU & callProceedingPDU){
	PTRACE(4, "mod_h323\t======>FSH323Connection::OnSendCallProceeding fastStartState = "<<FastStartStateNames[fastStartState]<<" ["<<*this<<"]");
	
	return false;
//	return true;
}

bool FSH323Connection::OnSendReleaseComplete(H323SignalPDU & pdu)
{
	PTRACE(4, "mod_h323\t======>FSH323Connection::OnSendReleaseComplete cause = "<<pdu.GetQ931().GetCause()<<" value = "<<(switch_call_cause_t)pdu.GetQ931().GetCause());
	
	switch_channel_hangup(m_fsChannel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);			
	return H323Connection::OnSendReleaseComplete(pdu);
}

PBoolean FSH323Connection::OpenLogicalChannel(const H323Capability& capability, unsigned sessionID, H323Channel::Directions dir){
	PTRACE(4, "mod_h323\t======>FSH323Connection::OpenLogicalChannel ('"<< (const char *)capability.GetFormatName()<<"', "<< sessionID<<", "<<dir <<") "<<*this);
    
    return H323Connection::OpenLogicalChannel(capability,sessionID,dir);
}


bool FSH323Connection::OnReceivedCapabilitySet(const H323Capabilities & remoteCaps,
							const H245_MultiplexCapability * muxCap,
							H245_TerminalCapabilitySetReject & reject){
	
	PTRACE(4, "mod_h323\t======>FSH323Connection::OnReceivedCapabilitySet ["<<*this<<"]");
	if (!H323Connection::OnReceivedCapabilitySet(remoteCaps, muxCap, reject)) {
		return false;
	}
	PTRACE(4, "mod_h323\t======>END H323Connection::OnReceivedCapabilitySet ["<<*this<<"]");
	
	for (int i = 0; i < remoteCapabilities.GetSize(); ++i) {
		PTRACE(4, "mod_h323\t----> Capabilities = "<<remoteCapabilities[i]);
	}
	
	H323Capability * cap = remoteCapabilities.FindCapability(H323Capability::e_Audio);
	if (cap == NULL) {
		PTRACE(4, "mod_h323\t----> Capabilities is NULL ");
		return false;
	}
	PTRACE(4, "mod_h323\t----> Capabilities not NULL ");
	
	return true;						
}


bool FSH323Connection::OnAlerting(const H323SignalPDU &alertingPDU, const PString &user){
	
	PTRACE(4, "mod_h323\t======>PFSH323Connection::OnAlerting user = "<<(const char *)user<<" ["<<*this<<"]");
	unsigned pi;
	switch_status_t status = switch_channel_mark_ring_ready(m_fsChannel);
	PTRACE(4, "mod_h323\t----------->OnAlerting return = "<<status);
	
	if (!alertingPDU.GetQ931().GetProgressIndicator(pi))
		pi = 0;
	PTRACE(4, "mod_h323\t----------->OnAlerting PI = "<<pi);
	if (pi > 0){
		if ((m_rxChennel && m_txChennel) || (m_ChennelProgress && m_rxChennel))
			switch_channel_mark_pre_answered(m_fsChannel);
		else{
			m_ChennelProgress = true;
		}
	}
	return ( status == SWITCH_STATUS_SUCCESS);
}

void FSH323Connection::AnsweringCall(AnswerCallResponse response){

	PTRACE(4, "mod_h323\t======>FSH323Connection::AnsweringCall ["<<*this<<"]");
	
	switch (response) {
		case AnswerCallDeferredWithMedia:{
			PTRACE(2, "H323\tAnswering call: " << response);
			if (!Lock())
				return;
			if (!mediaWaitForConnect) {
				// create a new facility PDU if doing AnswerDeferredWithMedia
				H323SignalPDU want245PDU;
				H225_Progress_UUIE & prog = want245PDU.BuildProgress(*this);
				PBoolean sendPDU = TRUE;
				PTRACE(2, "H323\tmediaWaitForConnect = FALSE ");
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
			return;
		} 
		case AnswerCallPending :{
			if (alertingPDU != NULL) {
				if (!Lock())
					return;
				// send Q931 Alerting PDU
				PTRACE(3, "H225\tSending Alerting PDU");
				
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
				return;
			}
		}
		default :H323Connection::AnsweringCall(response);
	} 
}

void FSH323Connection::OnEstablished(){

	PTRACE(4, "mod_h323\t======>PFSH323Connection::OnEstablished ["<<*this<<"]");
	if(m_startRTP)		
		switch_channel_mark_answered(m_fsChannel);
	else m_ChennelAnswer = true;
}



void FSH323Connection::setRemoteAddress(const char* remoteIP, WORD remotePort){
	PTRACE(4, "mod_h323\t======>PFSH323Connection::setRemoteAddress remoteIP ="<<remoteIP<<", remotePort = "<<remotePort<<" "<<*this);
	
    if (!m_remotePort) {
	PTRACE(4, "mod_h323\tGot remote RTP address "<<remoteIP<<":"<<remotePort<<" ["<<*this<<"]");
	m_remotePort = remotePort;
	m_remoteAddr = remoteIP;
    }
}

switch_status_t FSH323Connection::on_execute(){
	PTRACE(4, "mod_h323\t======>FSH323Connection::on_execute [" << *this<<"]");
    
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::on_routing(){
	PTRACE(4, "mod_h323\t======>FSH323Connection::on_routing ["<< *this<<"]");
   
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::kill_channel(int sig){
	PTRACE(4, "mod_h323\t======>FSH323Connection::kill_channel ["<< *this<<"]");
    PTRACE(3, "mod_h323\tKill " << sig << " on connection " << *this);
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
			switch_rtp_kill_socket(tech_pvt->rtp_session);
		}
        break;
    }

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::send_dtmf(const switch_dtmf_t *dtmf){
	PTRACE(4, "mod_h323\t======>FSH323Connection::send_dtmf " << *this);
    SendUserInputTone(dtmf->digit, dtmf->duration);
    return SWITCH_STATUS_SUCCESS;
}

void FSH323Connection::SendUserInputTone(char tone, unsigned duration, unsigned logicalChannel, unsigned rtpTimestamp)
{
	PTRACE(4, "mod_h323\t======>FSH323Connection::SendUserInputTone [" << *this<<"]");
	H323Connection::SendUserInputTone(tone, duration);
}

void FSH323Connection::OnUserInputTone(char tone, unsigned duration, unsigned logicalChannel, unsigned rtpTimestamp)
{
	PTRACE(4, "mod_h323\t======>FSH323Connection::OnUserInputTone [" << *this<<"]");
	
	switch_dtmf_t dtmf = { tone, duration };
    switch_channel_queue_dtmf(m_fsChannel, &dtmf);
	H323Connection::OnUserInputTone( tone,  duration, logicalChannel, rtpTimestamp);
}

void FSH323Connection::OnUserInputString(const PString &value)
{
	PTRACE(4, "mod_h323\t======>FSH323Connection::OnUserInputString [" << *this<<"]");
	switch_dtmf_t dtmf = { value[0], 500 };
    switch_channel_queue_dtmf(m_fsChannel, &dtmf);
	H323Connection::OnUserInputString(value);
}


switch_status_t FSH323Connection::receive_message(switch_core_session_message_t *msg){
	PTRACE(4, "mod_h323\t======>FSH323Connection::receive_message MSG=" << msg->message_id);
    switch_channel_t *channel = switch_core_session_get_channel(m_fsSession);

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
			break;
		}
		case SWITCH_MESSAGE_INDICATE_PROGRESS:	{		
			AnsweringCall(AnswerCallPending);
			AnsweringCall(AnswerCallDeferredWithMedia);
			
			if (m_txChennel && m_rxChennel){
				if (!switch_channel_test_flag(m_fsChannel, CF_EARLY_MEDIA)) {
					switch_channel_mark_pre_answered(m_fsChannel);
				}
			} else { 
				m_callOnPreAnswer = true;
				if (fastStartState == FastStartDisabled){
					m_txAudioOpened.Wait();
				}
			}
			break;
		}
		case SWITCH_MESSAGE_INDICATE_ANSWER:	{
			if (switch_channel_test_flag(channel, CF_OUTBOUND)) {
				return SWITCH_STATUS_FALSE;
			}
			AnsweringCall(H323Connection::AnswerCallNow);
			PTRACE(4, "mod_h323\tMedia started on connection " << *this);
		
			if (m_txChennel && m_rxChennel){
				if (!switch_channel_test_flag(m_fsChannel, CF_EARLY_MEDIA)) {
					PTRACE(4, "mod_h323\t-------------------->switch_channel_mark_answered(m_fsChannel) " << *this);
					switch_channel_mark_answered(m_fsChannel);
				}
			} else{
				m_ChennelAnswer =  true;
				if (fastStartState == FastStartDisabled){
					m_txAudioOpened.Wait();
					m_rxAudioOpened.Wait();
				}
			}
			break;
		}
		default:{
			PTRACE(3, "mod_h323\tReceived message " << msg->message_id << " on connection " << *this);
		}	
    }
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::receive_event(switch_event_t *event){
	PTRACE(4, "mod_h323\t======>FSH323Connection::receive_event " << *this);
    PTRACE(3, "mod_h323\tReceived event " << event->event_id << " on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::state_change(){
	PTRACE(4, "mod_h323\t======>FSH323Connection::state_change " << *this);
    PTRACE(3, "mod_h323\tState changed on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::on_init(){
	PTRACE(4, "mod_h323\t======>FSH323Connection::on_init " << *this);
    switch_channel_t *channel = switch_core_session_get_channel(m_fsSession);
    if (channel == NULL) {
        return SWITCH_STATUS_FALSE;
    }

    PTRACE(3, "mod_h323\tStarted routing for connection " << *this);
    switch_channel_set_state(channel, CS_ROUTING);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::on_exchange_media(){
	PTRACE(4, "mod_h323\t======>FSH323Connection::on_exchange_media " << *this);
    
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::on_soft_execute(){
    PTRACE(4, "mod_h323\t======>FSH323Connection::on_soft_execute " << *this);
	
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::read_audio_frame(switch_frame_t **frame, switch_io_flag_t flags, int stream_id){
	PTRACE(4, "mod_h323\t======>FSH323Connection::read_audio_frame " << *this);
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	tech_pvt->read_frame.flags = 0;

	switch_set_flag_locked(tech_pvt, TFLAG_READING);
    if (!switch_channel_ready(m_fsChannel)) {
		PTRACE(4, "mod_h323\t---------> RETURN");
		switch_clear_flag_locked(tech_pvt, TFLAG_READING);		
	    return SWITCH_STATUS_FALSE;
    }

    if (!switch_core_codec_ready(&tech_pvt->read_codec )) {
		PTRACE(4, "mod_h323\t---------> RETURN");
		switch_clear_flag_locked(tech_pvt, TFLAG_READING);
        return SWITCH_STATUS_FALSE;
    }

	switch_status_t status = switch_rtp_zerocopy_read_frame(tech_pvt->rtp_session, &tech_pvt->read_frame, flags);
	if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			PTRACE(4, "mod_h323\t---------> RETURN");
			switch_clear_flag_locked(tech_pvt, TFLAG_READING);
			return SWITCH_STATUS_FALSE;
	}
	PTRACE(4, "mod_h323\t--------->\n source = "<<tech_pvt->read_frame.source<< "\n packetlen = "<<tech_pvt->read_frame.packetlen<<"\n datalen = "<<tech_pvt->read_frame.datalen<<"\n samples = "<<tech_pvt->read_frame.samples<<"\n rate = "<<tech_pvt->read_frame.rate<<"\n payload = "<<(int)tech_pvt->read_frame.payload<<"\n timestamp = "<<tech_pvt->read_frame.timestamp<<"\n seq = "<<tech_pvt->read_frame.seq<<"\n ssrc = "<<tech_pvt->read_frame.ssrc);
    if (tech_pvt->read_frame.flags & SFF_CNG) {
        tech_pvt->read_frame.buflen = sizeof(m_buf);
        tech_pvt->read_frame.data = m_buf;
        tech_pvt->read_frame.packet = NULL;
        tech_pvt->read_frame.packetlen = 0;
        tech_pvt->read_frame.timestamp = 0;
        tech_pvt->read_frame.m = SWITCH_FALSE;
        tech_pvt->read_frame.seq = 0;
        tech_pvt->read_frame.ssrc = 0;
        tech_pvt->read_frame.codec = &tech_pvt->read_codec ;
    } else {
        tech_pvt->read_frame.codec = &tech_pvt->read_codec ;
    }
	switch_clear_flag_locked(tech_pvt, TFLAG_READING);
	
    *frame = &tech_pvt->read_frame;

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSH323Connection::write_audio_frame(switch_frame_t *frame, switch_io_flag_t flags, int stream_id){
	PTRACE(4, "mod_h323\t======>FSH323Connection::write_audio_frame " << *this);
	
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	switch_assert(tech_pvt != NULL);
	
	if (!switch_channel_ready(m_fsChannel)) {
		PTRACE(4, "mod_h323\t---------> RETURN");
        return SWITCH_STATUS_FALSE;
    }	
	
	while (!(tech_pvt->read_codec.implementation && switch_rtp_ready(tech_pvt->rtp_session))) {
		if (switch_channel_ready(m_fsChannel)) {
			switch_yield(10000);
		} else {
			PTRACE(4, "mod_h323\t---------> RETURN");
			return SWITCH_STATUS_GENERR;
		}
	}
	
	if (!switch_core_codec_ready(&tech_pvt->read_codec) || !tech_pvt->read_codec.implementation) {
		PTRACE(4, "mod_h323\t---------> RETURN");
		return SWITCH_STATUS_GENERR;
	}

	if ((frame->flags & SFF_CNG)) {
		PTRACE(4, "mod_h323\t---------> RETURN");
        return SWITCH_STATUS_SUCCESS;
    }
	switch_set_flag_locked(tech_pvt, TFLAG_WRITING);
	
	if (switch_rtp_write_frame(tech_pvt->rtp_session, frame)< 0) {
		status = SWITCH_STATUS_GENERR;
	}
	
	switch_clear_flag_locked(tech_pvt, TFLAG_WRITING);
	PTRACE(4, "mod_h323\t---------> RETURN");	
	return status;
}

switch_status_t FSH323Connection::read_video_frame(switch_frame_t **frame, switch_io_flag_t flag, int stream_id){
	PTRACE(4, "mod_h323\t======>FSH323Connection::read_video_frame " << *this);
 
}

switch_status_t FSH323Connection::write_video_frame(switch_frame_t *frame, switch_io_flag_t flag, int stream_id){
	PTRACE(4, "mod_h323\t======>FSH323Connection::write_video_frame " << *this);
}

///////////////////////////////////////////////////////////////////////

FSH323_ExternalRTPChannel::FSH323_ExternalRTPChannel( 
    FSH323Connection& connection,
    const H323Capability& capability,
    Directions direction, 
    unsigned sessionID,
	const PIPSocket::Address& ip, 
	WORD dataPort)
    : H323_ExternalRTPChannel(connection, capability, direction, sessionID,ip,dataPort)
	, m_conn(&connection)
	, m_fsSession(connection.GetSession())
	, m_capability(&capability)
	, m_RTPlocalPort(dataPort){ 
	
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	m_RTPlocalIP = (const char *)ip.AsString();
	SetExternalAddress(H323TransportAddress(ip, dataPort), H323TransportAddress(ip, dataPort+1));
    PTRACE(4, "mod_h323\t======>FSH323_ExternalRTPChannel::FSH323_ExternalRTPChannel "<< GetDirection()<< " addr="<< m_RTPlocalIP <<":"<< m_RTPlocalPort<<" ["<<*this<<"]");	
	
	memset(&m_readFrame, 0, sizeof(m_readFrame));
    m_readFrame.codec = m_switchCodec;
    m_readFrame.flags = SFF_RAW_RTP;
	
	m_fsChannel = switch_core_session_get_channel(m_fsSession);
    //SetExternalAddress(H323TransportAddress(localIpAddress, m_RTPlocalPort), H323TransportAddress(localIpAddress, m_RTPlocalPort+1));
	PTRACE(4, "mod_h323\t------->capability.GetPayloadType() return = "<<capability.GetPayloadType());
	PTRACE(4, "mod_h323\t------->capability.GetFormatName() return = "<<capability.GetFormatName());
	
	PString fname((const char *)capability.GetFormatName());
  
    if (fname.Find("{sw}") == (fname.GetLength() - 4))
		fname = fname.Mid(0,fname.GetLength()-4);
    if (fname.Find("{hw}") == (fname.GetLength() - 4))
		fname = fname.Mid(0,fname.GetLength()-4);
	
	OpalMediaFormat format(fname, FALSE);
	m_format = &format;
	payloadCode = format.GetPayloadType();
	PTRACE(4, "mod_h323\t------->payloadCode = "<<(int)payloadCode);
}


FSH323_ExternalRTPChannel::~FSH323_ExternalRTPChannel(){
    PTRACE(4, "mod_h323\t======>FSH323_ExternalRTPChannel::~FSH323_ExternalRTPChannel  "<< GetDirection()<<" "<<*this);
	
	h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	if (IsRunning()){
		PTRACE(4, "mod_h323\t------------->Running");
		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			switch_rtp_kill_socket(tech_pvt->rtp_session);
		}
	}
}

PBoolean FSH323_ExternalRTPChannel::Start(){
    PTRACE(4, "mod_h323\t======>FSH323_ExternalRTPChannel::Start() "<<*this);
	
	const char *err = NULL;
	switch_rtp_flag_t flags;
	char * timer_name = NULL;
	const char *var;
    h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(m_fsSession);
	if (!(m_conn && H323_ExternalRTPChannel::Start()))
		return FALSE;
	
	bool isAudio;
    if (m_capability->GetMainType() == H323Capability::e_Audio) {
        isAudio = true;
		PTRACE(4, "mod_h323\t------------------------->H323Capability::e_Audio");
    } else if (m_capability->GetMainType() == H323Capability::e_Video) {
        isAudio = false;
		PTRACE(4, "mod_h323\t------------------------->H323Capability::e_Video");
    }
	
	H323Codec *codec = GetCodec();
		
	PTRACE(4, "mod_h323\t------------------->GetFrameSize() return = "<<m_format->GetFrameSize());
	PTRACE(4, "mod_h323\t------------------->GetFrameTime() return = "<<m_format->GetFrameTime());
	PTRACE(4, "mod_h323\t------------------->payloadCode = "<<(int)payloadCode);
	PTRACE(4, "mod_h323\t------------------->m_capability->GetTxFramesInPacket() return =  "<<m_capability->GetTxFramesInPacket());
	PTRACE(4, "mod_h323\t------------------->m_capability->GetFormatName() return =  "<<m_capability->GetFormatName());
	PTRACE(4, "mod_h323\t------------------->GetH245CodecName() return =  "<<GetH245CodecName(m_capability));
	
	if (GetDirection() == IsReceiver){
		m_switchCodec = isAudio ? &tech_pvt->read_codec : &tech_pvt->vid_read_codec;
        m_switchTimer = isAudio ? &tech_pvt->read_timer : &tech_pvt->vid_read_timer;
		m_conn->m_rxChennel = true;
	}else{
		m_switchCodec = isAudio ? &tech_pvt->write_codec : &tech_pvt->vid_write_codec;
		m_conn->m_txChennel = true;
	}
	
	if (m_conn->m_callOnPreAnswer && !(GetDirection() == IsReceiver)){
		m_switchCodec = isAudio ? &tech_pvt->read_codec : &tech_pvt->vid_read_codec;
        m_switchTimer = isAudio ? &tech_pvt->read_timer : &tech_pvt->vid_read_timer;
	}
	
	if (switch_core_codec_init(m_switchCodec, GetH245CodecName(m_capability), NULL, // FMTP
                               8000, m_capability->GetTxFramesInPacket(), 1,  // Channels
                               SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,   // Settings
                               switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {
        
        if (switch_core_codec_init(m_switchCodec, GetH245CodecName(m_capability), NULL, // FMTP
                                   8000, 0, 1,  // Channels
                                   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,   // Settings
                                   switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {
            PTRACE(1, "mod_h323\t" << switch_channel_get_name(m_fsChannel)<< " Cannot initialise " << ((GetDirection() == IsReceiver)? " read" : " write") << ' '
                   << m_capability->GetMainType() << " codec " << m_capability << " for connection " << *this);
            switch_channel_hangup(m_fsChannel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
            return false;
        }
        PTRACE(2, "mod_h323\t" << switch_channel_get_name(m_fsChannel)<< " Unsupported ptime of " << m_capability->GetTxFramesInPacket() << " on " << ((GetDirection() == IsReceiver)? " read" : " write") << ' '
               << m_capability->GetMainType() << " codec " << m_capability << " for connection " << *this);
    }

    PTRACE(1, "mod_h323\t" << switch_channel_get_name(m_fsChannel)<< " initialise " << 
           switch_channel_get_name(m_fsChannel) << ((GetDirection() == IsReceiver)? " read" : " write") << ' '
           << m_capability->GetMainType() << " codec " << m_capability << " for connection " << *this);
	
	 if (GetDirection() == IsReceiver) {
        m_readFrame.rate = tech_pvt->read_codec.implementation->actual_samples_per_second;

        if (isAudio) {
            switch_core_session_set_read_codec(m_fsSession, m_switchCodec);
            if (switch_core_timer_init(m_switchTimer,
                                       "soft",
                                       m_switchCodec->implementation->microseconds_per_packet / 1000,
                                       m_switchCodec->implementation->samples_per_packet,
                                       switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {
                switch_core_codec_destroy(m_switchCodec);
                m_switchCodec = NULL;
                return false;
            }
			switch_channel_set_variable(m_fsChannel,"timer_name","soft");
        } else {
            switch_core_session_set_video_read_codec(m_fsSession, m_switchCodec);
            switch_channel_set_flag(m_fsChannel, CF_VIDEO);
        }
    } else {
        if (isAudio) {
            switch_core_session_set_write_codec(m_fsSession, m_switchCodec);
        } else {
            switch_core_session_set_video_write_codec(m_fsSession, m_switchCodec);
            switch_channel_set_flag(m_fsChannel, CF_VIDEO);
        }
    }

	if (m_conn->m_callOnPreAnswer && !(GetDirection() == IsReceiver)){
		m_readFrame.rate = tech_pvt->read_codec.implementation->actual_samples_per_second;

        if (isAudio) {
            switch_core_session_set_read_codec(m_fsSession, m_switchCodec);
            if (switch_core_timer_init(m_switchTimer,
                                       "soft",
                                       m_switchCodec->implementation->microseconds_per_packet / 1000,
                                       m_switchCodec->implementation->samples_per_packet,
                                       switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {
                switch_core_codec_destroy(m_switchCodec);
                m_switchCodec = NULL;
                return false;
            }
			switch_channel_set_variable(m_fsChannel,"timer_name","soft");
		}	
	}
	
	if (m_conn->m_ChennelProgress && (GetDirection() == IsReceiver)){
		if (isAudio) {
            switch_core_session_set_write_codec(m_fsSession, m_switchCodec);
        }
	}
	
    PTRACE(3, "mod_h323\tSet " << ((GetDirection() == IsReceiver)? " read" : " write") << ' '
           << m_capability->GetMainType() << " codec to << " << m_capability << " for connection " << *this);

	switch_mutex_lock(tech_pvt->h323_mutex);
	
	PIPSocket::Address remoteIpAddress;
    GetRemoteAddress(remoteIpAddress,m_RTPremotePort);	
	m_RTPremoteIP = (const char *)remoteIpAddress.AsString();
	PTRACE(4, "mod_h323\t------------------->tech_pvt->rtp_session = "<<tech_pvt->rtp_session);
	PTRACE(4, "mod_h323\t------------------->samples_per_packet = "<<m_switchCodec->implementation->samples_per_packet);
	PTRACE(4, "mod_h323\t------------------->actual_samples_per_second = "<<m_switchCodec->implementation->actual_samples_per_second);
	
	if ((!m_conn->m_startRTP)) {			
		flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_DATAWAIT|SWITCH_RTP_FLAG_AUTO_CNG|SWITCH_RTP_FLAG_RAW_WRITE);
		PTRACE(4, "mod_h323\t------------------->timer_name = "<<switch_channel_get_variable(m_fsChannel, "timer_name"));
		if ((var = switch_channel_get_variable(m_fsChannel, "timer_name"))) {
			timer_name = (char *) var;
		}
		PTRACE(4, "mod_h323\t------------------->timer_name = "<<timer_name);
		tech_pvt->rtp_session = switch_rtp_new((const char *)m_RTPlocalIP,
											   m_RTPlocalPort,
											   (const char *)m_RTPremoteIP,
											   m_RTPremotePort,
											   (switch_payload_t)payloadCode,
											   m_switchCodec->implementation->samples_per_packet,	
											   m_capability->GetTxFramesInPacket() * 1000,
											   (switch_rtp_flag_t) flags, timer_name, &err,
											   switch_core_session_get_pool(m_fsSession));
		PTRACE(4, "mod_h323\t------------------------->tech_pvt->rtp_session = "<<tech_pvt->rtp_session);
		m_conn->m_startRTP = true;
		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			PTRACE(4, "mod_h323\t+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");		
			switch_channel_set_flag(m_fsChannel, CF_FS_RTP);
			
		}else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
			switch_channel_hangup(m_fsChannel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);			
			switch_mutex_unlock(tech_pvt->h323_mutex);
			return SWITCH_STATUS_FALSE;
		}
	}
	
    PTRACE(4, "mod_h323\t------------->External RTP address "<<m_RTPremoteIP<<":"<<m_RTPremotePort);
	switch_mutex_unlock(tech_pvt->h323_mutex);
	
	if (GetDirection() == IsReceiver) m_conn->m_rxAudioOpened.Signal();
	else m_conn->m_txAudioOpened.Signal();                             
	
	if ( m_conn->m_ChennelAnswer && m_conn->m_rxChennel &&  m_conn->m_txChennel)
		switch_channel_mark_answered(m_fsChannel);
		
	if ((m_conn->m_ChennelProgress && m_conn->m_rxChennel)||(m_conn->m_callOnPreAnswer && m_conn->m_txChennel))
		switch_channel_mark_pre_answered(m_fsChannel);
		
	return true;
}


PBoolean FSH323_ExternalRTPChannel::OnReceivedPDU(
				const H245_H2250LogicalChannelParameters& param,
				unsigned& errorCode){
    PTRACE(4, "mod_h323\t======>FSH323_ExternalRTPChannel::OnReceivedPDU ["<<*this<<"]");
	
    if (!H323_ExternalRTPChannel::OnReceivedPDU(param,errorCode))
		return true;
    PIPSocket::Address remoteIpAddress;
    WORD remotePort;
    GetRemoteAddress(remoteIpAddress,remotePort);
    PTRACE(4, "mod_h323\tRemote RTP address "<<(const char *)remoteIpAddress.AsString()<<":"<<remotePort);
    m_conn->setRemoteAddress((const char *)remoteIpAddress.AsString(), remotePort);
    return true;
}

PBoolean FSH323_ExternalRTPChannel::OnSendingPDU(H245_H2250LogicalChannelParameters& param){
   PTRACE(4, "mod_h323\t======>FSH323_ExternalRTPChannel::OnSendingPDU ["<<*this<<"]");
    return H323_ExternalRTPChannel::OnSendingPDU(param);
}

PBoolean FSH323_ExternalRTPChannel::OnReceivedAckPDU(const H245_H2250LogicalChannelAckParameters& param){
    PTRACE(4, "mod_h323\t======>FSH323_ExternalRTPChannel::OnReceivedAckPDU ["<<*this<<"]");
    return H323_ExternalRTPChannel::OnReceivedAckPDU(param);
}

void FSH323_ExternalRTPChannel::OnSendOpenAck(H245_H2250LogicalChannelAckParameters& param){
    PTRACE(4, "mod_h323\t======>FSH323_ExternalRTPChannel::OnSendOpenAck ["<<*this<<"]");
    H323_ExternalRTPChannel::OnSendOpenAck(param);
}


FSH323Connection * FSH323EndPoint::FSMakeCall(const PString & dest, void *userData){
	PTRACE(4, "mod_h323\t======>FSH323EndPoint::FSMakeCall DST NUMBER = "<<dest<<" ["<<*this<<"]");
	
	FSH323Connection * connection;
	PString token;
	H323Transport *transport = NULL;
	
    if (listeners.GetSize() > 0) {
			H323TransportAddress taddr = listeners[0].GetTransportAddress();
			PIPSocket::Address addr;
			WORD port;
			if (taddr.GetIpAndPort(addr, port)) {				
				if (addr) {
					PTRACE(4, "mod_h323\t----> Using "<<addr<<" for outbound call");
					transport = new H323TransportTCP(*this, addr,false);
					if (!transport)						
						PTRACE(4, "mod_h323\t----> Unable to create transport for outgoing call");
				}
			} else
				PTRACE(4, "mod_h323\t----> Unable to get address and port");
	}
	
    if (!(connection = (FSH323Connection *)H323EndPoint::MakeCall(dest, token, userData))) {
        return NULL;
    }
	return connection;
}


static switch_call_cause_t create_outgoing_channel(switch_core_session_t *session,
                                                   switch_event_t *var_event,
                                                   switch_caller_profile_t *outbound_profile,
                                                   switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause){
	PTRACE(4, "mod_h323\t======>create_outgoing_channel DST NUMBER = "<<outbound_profile->destination_number);
	
	FSH323Connection * connection;
    if (h323_process == NULL) {
        return SWITCH_CAUSE_CRASH;
    }
	FSH323EndPoint & ep = h323_process->GetH323EndPoint();
	if (!(connection = ep.FSMakeCall(outbound_profile->destination_number,outbound_profile))){
		return SWITCH_CAUSE_PROTOCOL_ERROR;
	}

    *new_session = connection->GetSession();
	PTRACE(4, "mod_h323\t--------->GetSession() return = "<<connection->GetSession());
    return SWITCH_CAUSE_SUCCESS;
}


static switch_status_t on_destroy(switch_core_session_t *session){
	PTRACE(4, "mod_h323\t======>on_destroy ");

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


static switch_status_t on_hangup(switch_core_session_t *session){
	PTRACE(4, "mod_h323\t======>switch_status_t on_hangup ");

    switch_channel_t *channel = switch_core_session_get_channel(session);
    h323_private_t *tech_pvt = (h323_private_t *) switch_core_session_get_private(session);
    if (tech_pvt->me) {
		PTRACE(4, "mod_h323\t----->");
        Q931::CauseValues cause = (Q931::CauseValues)switch_channel_get_cause_q850(channel);
        tech_pvt->me->SetQ931Cause(cause);
        tech_pvt->me->ClearCallSynchronous(NULL, H323TranslateToCallEndReason(cause, UINT_MAX));
        tech_pvt->me = NULL;
    }

    return SWITCH_STATUS_SUCCESS;
}








