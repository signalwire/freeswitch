/* Opal endpoint interface for Freeswitch Modular Media Switching Software Library /
 * Soft-Switch Application
 *
 * Version: MPL 1.1
 *
 * Copyright (c) 2007 Tuyan Ozipek (tuyanozipek@gmail.com)
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
 * Tuyan Ozipek   (tuyanozipek@gmail.com)
 * Lukasz Zwierko (lzwierko@gmail.com)
 * Robert Jongbloed (robertj@voxlucida.com.au)
 *
 */

#include "mod_opal.h"
#include <opal/patch.h>
#include <rtp/rtp.h>
#include <h323/h323pdu.h>
#include <h323/gkclient.h>

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, mod_opal_globals.codec_string);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_context, mod_opal_globals.context);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, mod_opal_globals.dialplan);


#define CF_NEED_FLUSH (1 << 1)

struct mod_opal_globals mod_opal_globals = { 0 };


static switch_call_cause_t create_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
                                                   switch_caller_profile_t *outbound_profile, switch_core_session_t **new_session,
                                                   switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);


static FSProcess *opal_process = NULL;


static const char ModuleName[] = "opal";


static switch_status_t on_hangup(switch_core_session_t *session);
static switch_status_t on_destroy(switch_core_session_t *session);


static switch_io_routines_t opalfs_io_routines = {
    /*.outgoing_channel */ create_outgoing_channel,
    /*.read_frame */ FSConnection::read_audio_frame,
    /*.write_frame */ FSConnection::write_audio_frame,
    /*.kill_channel */ FSConnection::kill_channel,
    /*.send_dtmf */ FSConnection::send_dtmf,
    /*.receive_message */ FSConnection::receive_message,
    /*.receive_event */ FSConnection::receive_event,
    /*.state_change */ FSConnection::state_change,
    /*.read_video_frame */ FSConnection::read_video_frame,
    /*.write_video_frame */ FSConnection::write_video_frame
};

static switch_state_handler_table_t opalfs_event_handlers = {
    /*.on_init */ FSConnection::on_init,
    /*.on_routing */ FSConnection::on_routing,
    /*.on_execute */ FSConnection::on_execute,
    /*.on_hangup */ on_hangup,
    /*.on_exchange_media */ FSConnection::on_exchange_media,
    /*.on_soft_execute */ FSConnection::on_soft_execute,
    /*.on_consume_media*/ NULL,
    /*.on_hibernate*/ NULL,
    /*.on_reset*/ NULL,
    /*.on_park*/ NULL,
    /*.on_reporting*/ NULL,
    /*.on_destroy*/ on_destroy
};


SWITCH_BEGIN_EXTERN_C
/*******************************************************************************/

SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown);
SWITCH_MODULE_DEFINITION(mod_opal, mod_opal_load, mod_opal_shutdown, NULL);

SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Starting loading mod_opal\n");

    /* Prevent the loading of OPAL codecs via "plug ins", this is a directory
       full of DLLs that will be loaded automatically. */
    putenv((char *)"PTLIBPLUGINDIR=/no/thanks");


    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    if (!*module_interface) {
        return SWITCH_STATUS_MEMERR;
    }

    opal_process = new FSProcess();
    if (opal_process == NULL) {
        return SWITCH_STATUS_MEMERR;
    }

    if (opal_process->Initialise(*module_interface)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Opal manager initialized and running\n");
        //unloading causes a seg in linux
        return SWITCH_STATUS_NOUNLOAD;
        //return SWITCH_STATUS_SUCCESS;
    }

    delete opal_process;
    opal_process = NULL;
    return SWITCH_STATUS_FALSE;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown) {
    
    switch_safe_free(mod_opal_globals.context);
    switch_safe_free(mod_opal_globals.dialplan);
    switch_safe_free(mod_opal_globals.codec_string);
    delete opal_process;
    opal_process = NULL;
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C
/*******************************************************************************/



static switch_call_cause_t create_outgoing_channel(switch_core_session_t *session,
                                                   switch_event_t *var_event,
                                                   switch_caller_profile_t *outbound_profile,
                                                   switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
    if (opal_process == NULL) {
        return SWITCH_CAUSE_CRASH;
    }

    PString token;

    FSManager & manager = opal_process->GetManager();
    if (!manager.SetUpCall("local:", outbound_profile->destination_number, token, outbound_profile)) {
        return SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
    }

    PSafePtr < OpalCall > call = manager.FindCallWithLock(token);

    if (call == NULL) {
        return SWITCH_CAUSE_PROTOCOL_ERROR;
    }

    PSafePtr < FSConnection > connection = call->GetConnectionAs < FSConnection > (0);

    if (connection == NULL) {
        return SWITCH_CAUSE_PROTOCOL_ERROR;
    }

    *new_session = connection->GetSession();

    return SWITCH_CAUSE_SUCCESS;
}


///////////////////////////////////////////////////////////////////////

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
                        if (bufPtr && ((e = strchr(bufPtr, ' ')) || (e = strchr(bufPtr, '\t')))) {
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


///////////////////////////////////////////////////////////////////////

FSProcess::FSProcess()
  : PLibraryProcess("Vox Lucida Pty. Ltd.", "mod_opal", 1, 0, AlphaCode, 1)
  , m_manager(NULL)
{
}


FSProcess::~FSProcess()
{
  delete m_manager;
}


bool FSProcess::Initialise(switch_loadable_module_interface_t *iface)
{
  m_manager = new FSManager();
  return m_manager != NULL && m_manager->Initialise(iface);
}


///////////////////////////////////////////////////////////////////////

FSManager::FSManager()
{
    // These are deleted by the OpalManager class, no need to have destructor
    m_h323ep = new H323EndPoint(*this);
    m_iaxep = new IAX2EndPoint(*this);
    m_fsep = new FSEndPoint(*this);
}


bool FSManager::Initialise(switch_loadable_module_interface_t *iface)
{
    ReadConfig(false);

#if PTRACING
    PTrace::SetLevel(mod_opal_globals.trace_level);        //just for fun and eyecandy ;)
    PTrace::SetOptions(PTrace::TraceLevel);
    PTrace::SetStream(new FSTrace);
#endif

    m_FreeSwitch = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(iface, SWITCH_ENDPOINT_INTERFACE);
    m_FreeSwitch->interface_name = ModuleName;
    m_FreeSwitch->io_routines = &opalfs_io_routines;
    m_FreeSwitch->state_handler = &opalfs_event_handlers;

    silenceDetectParams.m_mode = OpalSilenceDetector::NoSilenceDetection;

    if (m_listeners.empty()) {
        m_h323ep->StartListener("");
    } else {
        for (std::list < FSListener >::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it) {
            if (!m_h323ep->StartListener(it->listenAddress)) {
                PTRACE(3, "mod_opal\tCannot start listener for " << it->name);
            }
        }
    }

    AddRouteEntry("h323:.* = local:<da>");  // config option for direct routing
    AddRouteEntry("iax2:.* = local:<da>");  // config option for direct routing
    AddRouteEntry("local:.* = h323:<da>");  // config option for direct routing

    // Make sure all known codecs are instantiated,
    // these are ones we know how to translate into H.323 capabilities
    GetOpalG728();
    GetOpalG729();
    GetOpalG729A();
    GetOpalG729B();
    GetOpalG729AB();
    GetOpalG7231_6k3();
    GetOpalG7231_5k3();
    GetOpalG7231A_6k3();
    GetOpalG7231A_5k3();
    GetOpalGSM0610();
    GetOpalGSMAMR();
    GetOpaliLBC();

    /* For compatibility with the algorithm in FSConnection::SetCodecs() we need
       to set all audio media formats to be 1 frame per packet */
    OpalMediaFormatList allCodecs = OpalMediaFormat::GetAllRegisteredMediaFormats();
    for (OpalMediaFormatList::iterator it = allCodecs.begin(); it != allCodecs.end(); ++it) {
      if (it->GetMediaType() == OpalMediaType::Audio()) {
        it->SetOptionInteger(OpalAudioFormat::RxFramesPerPacketOption(), 1);
        it->SetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption(), 1);
      }
    }

    if (!m_gkAddress.IsEmpty()) {
      if (m_h323ep->UseGatekeeper(m_gkAddress, m_gkIdentifer, m_gkInterface))
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Started gatekeeper: %s\n",
                          (const char *)m_h323ep->GetGatekeeper()->GetName());
      else
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                          "Could not start gatekeeper: addr=\"%s\", id=\"%s\", if=\"%s\"\n",
                          (const char *)m_gkAddress,
                          (const char *)m_gkIdentifer,
                          (const char *)m_gkInterface);
    }

    return TRUE;
}


switch_status_t FSManager::ReadConfig(int reload)
{
    const char *cf = "opal.conf";
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
    if (xmlSettings) {
        for (switch_xml_t xmlParam = switch_xml_child(xmlSettings, "param"); xmlParam != NULL; xmlParam = xmlParam->next) {
            const char *var = switch_xml_attr_soft(xmlParam, "name");
            const char *val = switch_xml_attr_soft(xmlParam, "value");

            if (!strcasecmp(var, "trace-level")) {
                int level = atoi(val);
                if (level > 0) {
                    mod_opal_globals.trace_level = level;
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
                }
            } else if (!strcasecmp(var, "gk-address")) {
                m_gkAddress = val;
            } else if (!strcasecmp(var, "gk-identifer")) {
                m_gkIdentifer = val;
            } else if (!strcasecmp(var, "gk-interface")) {
                m_gkInterface = val;
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
                //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Var - '%s' and Val - '%s' \n", var, val);
                if (!strcasecmp(var, "h323-ip"))
                    ip = val;
                else if (!strcasecmp(var, "h323-port"))
                    port = (WORD) atoi(val);
            }

            listener.listenAddress = OpalTransportAddress(ip, port);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created Listener '%s'\n", (const char *) listener.name);
        }
    }

    switch_event_destroy(&params);

    if (xml)
        switch_xml_free(xml);

    return status;
}


OpalCall * FSManager::CreateCall(void * /*userData*/)
{
  return new FSCall(*this);
}


///////////////////////////////////////////////////////////////////////

FSEndPoint::FSEndPoint(FSManager & manager)
:   OpalLocalEndPoint(manager)
{
    PTRACE(3, "mod_opal\t FSEndPoint Created!");
}


bool FSEndPoint::OnIncomingCall(OpalLocalConnection & connection)
{
    return ((FSConnection &) connection).OnIncoming();
}


OpalLocalConnection *FSEndPoint::CreateConnection(OpalCall & call, void *userData, unsigned options, OpalConnection::StringOptions* stringOptions)
{
    FSManager & mgr = (FSManager &) GetManager();
    switch_core_session_t *fsSession = switch_core_session_request(mgr.GetSwitchInterface(), 
                                       (switch_caller_profile_t *)userData ? SWITCH_CALL_DIRECTION_OUTBOUND : SWITCH_CALL_DIRECTION_INBOUND, NULL);
    if (fsSession == NULL)
        return NULL;

    switch_channel_t *fsChannel = switch_core_session_get_channel(fsSession);

    if (fsChannel == NULL) {
        switch_core_session_destroy(&fsSession);
        return NULL;
    }

    return new FSConnection(call, *this, userData, options, stringOptions, (switch_caller_profile_t *)userData, fsSession, fsChannel);
}


///////////////////////////////////////////////////////////////////////

FSCall::FSCall(OpalManager & manager)
  : OpalCall(manager)
{
}


PBoolean FSCall::OnSetUp(OpalConnection & connection)
{
  // Transfer FS caller_id_number & caller_id_name from the FSConnection
  // to the protocol connectionm (e.g. H.323) so gets sent correctly
  // in outgoing packets
  PSafePtr<FSConnection> local = GetConnectionAs<FSConnection>();
  if (local != NULL) {
    PSafePtr<OpalConnection> proto = local->GetOtherPartyConnection();
    if (proto != NULL) {
      proto->SetLocalPartyName(local->GetLocalPartyName());
      proto->SetDisplayName(local->GetDisplayName());
    }
  }

  return OpalCall::OnSetUp(connection);
}


///////////////////////////////////////////////////////////////////////


FSConnection::FSConnection(OpalCall & call, FSEndPoint & endpoint, void* userData, unsigned options, OpalConnection::StringOptions* stringOptions, switch_caller_profile_t *outbound_profile, switch_core_session_t *fsSession, switch_channel_t *fsChannel)
  : OpalLocalConnection(call, endpoint, userData, options, stringOptions)
  , m_endpoint(endpoint)
  , m_fsSession(fsSession)
  , m_fsChannel(fsChannel)
{
    opal_private_t *tech_pvt;

    tech_pvt = (opal_private_t *) switch_core_session_alloc(m_fsSession, sizeof(*tech_pvt));
    tech_pvt->me = this;
    switch_core_session_set_private(m_fsSession, tech_pvt);

    if (outbound_profile != NULL) {
        SetLocalPartyName(outbound_profile->caller_id_number);
        SetDisplayName(outbound_profile->caller_id_name);

        switch_caller_profile_t *caller_profile = switch_caller_profile_clone(m_fsSession, outbound_profile);
        switch_channel_set_caller_profile(m_fsChannel, caller_profile);

        PString name = "opal/";
        name += outbound_profile->destination_number;
        switch_channel_set_name(m_fsChannel, name);

        switch_channel_set_flag(m_fsChannel, CF_OUTBOUND);
        switch_channel_set_state(m_fsChannel, CS_INIT);
    }
}


bool FSConnection::OnIncoming()
{
    if (m_fsSession == NULL) {
        PTRACE(1, "mod_opal\tSession request failed.");
        return false;
    }

    switch_core_session_add_stream(m_fsSession, NULL);

    switch_channel_t *channel = switch_core_session_get_channel(m_fsSession);
    if (channel == NULL) {
        PTRACE(1, "mod_opal\tSession does not have a channel");
        return false;
    }

    PURL url = GetRemotePartyURL();
    switch_caller_profile_t *caller_profile = switch_caller_profile_new(switch_core_session_get_pool(m_fsSession),
                                                                        url.GetUserName(),
                                                                        /** username */
                                                                        mod_opal_globals.dialplan,
                                                                        /** dial plan */
                                                                        GetRemotePartyName(),
                                                                        /** caller_id_name */
                                                                        GetRemotePartyNumber(),
                                                                        /** caller_id_number */
                                                                        url.GetHostName(),
                                                                        /** network addr */
                                                                        NULL,
                                                                        /** ANI */
                                                                        NULL,
                                                                        /** ANI II */
                                                                        NULL,
                                                                        /** RDNIS */
                                                                        ModuleName,
                                                                        /** source */
                                                                        mod_opal_globals.context,
                                                                        /** set context  */
                                                                        GetCalledPartyNumber()
                                                                        /** destination_number */
                                                                        );
    if (caller_profile == NULL) {
        PTRACE(1, "mod_opal\tCould not create caller profile");
        return false;
    }

    PTRACE(4, "mod_opal\tCreated switch caller profile:\n"
           "  username       = " << caller_profile->username << "\n"
           "  dialplan       = " << caller_profile->dialplan << "\n"
           "  caller_id_name     = " << caller_profile->caller_id_name << "\n"
           "  caller_id_number   = " << caller_profile->caller_id_number << "\n"
           "  network_addr   = " << caller_profile->network_addr << "\n"
           "  source         = " << caller_profile->source << "\n"
           "  context        = " << caller_profile->context << "\n" "  destination_number= " << caller_profile->destination_number);
    switch_channel_set_caller_profile(channel, caller_profile);

    char name[256] = "opal/in:";
    switch_copy_string(name + 8, caller_profile->destination_number, sizeof(name)-8);
    switch_channel_set_name(channel, name);
    switch_channel_set_state(channel, CS_INIT);

    if (switch_core_session_thread_launch(m_fsSession) != SWITCH_STATUS_SUCCESS) {
        PTRACE(1, "mod_opal\tCould not launch session thread");
        return false;
    }

    return true;
}


void FSConnection::OnReleased()
{
    opal_private_t *tech_pvt = (opal_private_t *) switch_core_session_get_private(m_fsSession);
    
    /* so FS on_hangup will not try to deref a landmine */
    tech_pvt->me = NULL;
    
    m_rxAudioOpened.Signal();   // Just in case
    m_txAudioOpened.Signal();
    H225_ReleaseCompleteReason dummy;
    switch_channel_hangup(switch_core_session_get_channel(m_fsSession),
                          (switch_call_cause_t)H323TranslateFromCallEndReason(GetCallEndReason(), dummy));    
    OpalLocalConnection::OnReleased();
}


void FSConnection::OnAlerting()
{
    switch_channel_mark_ring_ready(m_fsChannel);
    return OpalLocalConnection::OnAlerting();
}

PBoolean FSConnection::SetAlerting(const PString & calleeName, PBoolean withMedia)
{
    return OpalLocalConnection::SetAlerting(calleeName, withMedia);
}


void FSConnection::OnEstablished()
{
    OpalLocalConnection::OnEstablished();
}


PBoolean FSConnection::SendUserInputTone(char tone, unsigned duration)
{
    switch_dtmf_t dtmf = { tone, duration };
    return switch_channel_queue_dtmf(m_fsChannel, &dtmf) == SWITCH_STATUS_SUCCESS;
}


PBoolean FSConnection::SendUserInputString(const PString & value)
{
  return OpalConnection::SendUserInputString(value);
}


OpalMediaFormatList FSConnection::GetMediaFormats() const
{
    if (m_switchMediaFormats.IsEmpty()) {
        const_cast<FSConnection *>(this)->SetCodecs();
    }
    
    return m_switchMediaFormats;
}


void FSConnection::SetCodecs()
{
    int numCodecs = 0;
    const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];    
    const char *codec_string = NULL, *abs, *ocodec;
    char *tmp_codec_string = NULL;
    char *codec_order[SWITCH_MAX_CODECS];
    int codec_order_last;


    if ((abs = switch_channel_get_variable(m_fsChannel, "absolute_codec_string"))) {
        codec_string = abs;
    } else {
        if ((abs = switch_channel_get_variable(m_fsChannel, "codec_string"))) {
            codec_string = abs;
        }

        if ((ocodec = switch_channel_get_variable(m_fsChannel, SWITCH_ORIGINATOR_CODEC_VARIABLE))) {
            codec_string = switch_core_session_sprintf(m_fsSession, "%s,%s", ocodec, codec_string);
        }
    }
    
    if (!codec_string) {
        codec_string = mod_opal_globals.codec_string;
    }

    if (codec_string) {
        if ((tmp_codec_string = strdup(codec_string))) {
            codec_order_last = switch_separate_string(tmp_codec_string, ',', codec_order, SWITCH_MAX_CODECS);
            numCodecs = switch_loadable_module_get_codecs_sorted(codecs, SWITCH_MAX_CODECS, codec_order, codec_order_last);
            
        }
    } else {
        numCodecs = switch_loadable_module_get_codecs(codecs, sizeof(codecs) / sizeof(codecs[0]));
    }
    
    for (int i = 0; i < numCodecs; i++) {
        const switch_codec_implementation_t *codec = codecs[i];

        // See if we have a match by PayloadType/rate/name
        OpalMediaFormat switchFormat((RTP_DataFrame::PayloadTypes)codec->ianacode,
                                     codec->samples_per_second,
                                     codec->iananame);
        if (!switchFormat.IsValid()) {
            // See if we have a match by name alone
            switchFormat = codec->iananame;
            if (!switchFormat.IsValid()) {
              PTRACE(2, "mod_opal\tCould not match FS codec " << codec->iananame << " to OPAL media format.");
              continue;
            }
        }
        

        // Did we match or create a new media format?
        if (switchFormat.IsValid() && codec->codec_type == SWITCH_CODEC_TYPE_AUDIO) {
            PTRACE(2, "mod_opal\tMatched FS codec " << codec->iananame << " to OPAL media format " << switchFormat);

            // Calculate frames per packet, do not use codec->codec_frames_per_packet as that field
            // has slightly different semantics when used in streamed codecs such as G.711
            int fpp = codec->samples_per_packet/switchFormat.GetFrameTime();

            /* Set the frames/packet to maximum of what is in the FS table. The OPAL negotiations will
               drop the value from there. This might fail if there are "holes" in the FS table, e.g.
               if for some reason G.723.1 has 30ms and 90ms but not 60ms, then the OPAL negotiations
               could end up with 60ms and the codec cannot be created. The "holes" are unlikely in
               all but streamed codecs such as G.711, where it is theoretically possible for OPAL to
               come up with 32ms and there is only 30ms and 40ms in the FS table. We deem these
               scenarios succifiently rare that we can safely ignore them ... for now. */

            if (fpp > switchFormat.GetOptionInteger(OpalAudioFormat::RxFramesPerPacketOption())) {
                switchFormat.SetOptionInteger(OpalAudioFormat::RxFramesPerPacketOption(), fpp);
            }

            if (fpp > switchFormat.GetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption())) {
                switchFormat.SetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption(), fpp);
            }
        }

        m_switchMediaFormats += switchFormat;
    }
    
    switch_safe_free(tmp_codec_string);
}


OpalMediaStream *FSConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat, unsigned sessionID, PBoolean isSource)
{
    return new FSMediaStream(*this, mediaFormat, sessionID, isSource);
}


PBoolean FSConnection::OnOpenMediaStream(OpalMediaStream & stream)
{
    if (!OpalConnection::OnOpenMediaStream(stream)) {
        return false;
    }

    if (stream.GetMediaFormat().GetMediaType() != OpalMediaType::Audio()) {
        return true;
    }

    if (stream.IsSource()) {
        m_rxAudioOpened.Signal();
    } else {
        m_txAudioOpened.Signal();
    }

    if (GetMediaStream(stream.GetSessionID(), stream.IsSink()) != NULL) {
        // Have open media in both directions.
        if (GetPhase() == AlertingPhase) {
            switch_channel_mark_pre_answered(m_fsChannel);
        } else if (GetPhase() < ReleasingPhase) {
            switch_channel_mark_answered(m_fsChannel);
        }
    }

    return true;
}


switch_status_t FSConnection::on_init()
{
    switch_channel_t *channel = switch_core_session_get_channel(m_fsSession);
    if (channel == NULL) {
        return SWITCH_STATUS_FALSE;
    }

    PTRACE(3, "mod_opal\tStarted routing for connection " << *this);
    switch_channel_set_state(channel, CS_ROUTING);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_routing()
{
    PTRACE(3, "mod_opal\tRouting connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_execute()
{
    PTRACE(3, "mod_opal\tExecuting connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t on_destroy(switch_core_session_t *session)
{
    //switch_channel_t *channel = switch_core_session_get_channel(session);
    opal_private_t *tech_pvt = (opal_private_t *) switch_core_session_get_private(session);
    
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

/* this function has to be called with the original session beause the FSConnection might already be destroyed and we 
   will can't have it be a method of a dead object
 */
static switch_status_t on_hangup(switch_core_session_t *session)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    opal_private_t *tech_pvt = (opal_private_t *) switch_core_session_get_private(session);
    
    /* if this is still here it was our idea to hangup not opal's */
    if (tech_pvt->me) {
        Q931::CauseValues cause = (Q931::CauseValues)switch_channel_get_cause_q850(channel);
        tech_pvt->me->SetQ931Cause(cause);
        tech_pvt->me->ClearCallSynchronous(NULL, H323TranslateToCallEndReason(cause, UINT_MAX));
        tech_pvt->me = NULL;
    }

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_exchange_media()
{
    PTRACE(3, "mod_opal\tLoopback on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_soft_execute()
{
    PTRACE(3, "mod_opal\tTransmit on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::kill_channel(int sig)
{
    PTRACE(3, "mod_opal\tKill " << sig << " on connection " << *this);

    switch (sig) {
    case SWITCH_SIG_BREAK:
        break;
    case SWITCH_SIG_KILL:
        m_rxAudioOpened.Signal();
        m_txAudioOpened.Signal();
        break;
    default:
        break;
    }

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::send_dtmf(const switch_dtmf_t *dtmf)
{
    OnUserInputTone(dtmf->digit, dtmf->duration);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::receive_message(switch_core_session_message_t *msg)
{
    switch_channel_t *channel = switch_core_session_get_channel(m_fsSession);


    /*
      SWITCH_MESSAGE_INDICATE_PROGRESS:  establish early media now and return SWITCH_STATUS_FALSE if you can't
      SWITCH_MESSAGE_INDICATE_ANSWER:  answer and set up media now if it's not already and return SWITCH_STATUS_FALSE if you can't

      Neither message means anything on an outbound call....

      It would only happen if someone called switch_channel_answer() instead of switch_channel_mark_answered() on an outbound call.
      it should not do anything if someone does it by accident somewhere hense this in both cases:

      if (switch_channel_test_flag(channel, CF_OUTBOUND)) {
      return SWITCH_STATUS_FALSE;
      }


      When we get these messages the core will trust that you have triggered FSMediaStream::Open and are ready for media if we do not
      have media we MUST return SWITCH_STATUS_FALSE or it will cause a CRASH.



    */
    switch (msg->message_id) {
    case SWITCH_MESSAGE_INDICATE_BRIDGE:
    case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
    case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:
        switch_channel_set_private_flag(channel, CF_NEED_FLUSH);
        break;

    case SWITCH_MESSAGE_INDICATE_RINGING:
    case SWITCH_MESSAGE_INDICATE_PROGRESS:
    case SWITCH_MESSAGE_INDICATE_ANSWER:
        {
            switch_caller_profile_t * profile = switch_channel_get_caller_profile(channel);
            if (profile != NULL && profile->caller_extension != NULL)
            {
                PSafePtr<OpalConnection> other = GetOtherPartyConnection();
                if (other != NULL) {
                    other->SetLocalPartyName(profile->caller_extension->extension_number);
                    other->SetDisplayName(profile->caller_extension->extension_name);
                }
                SetLocalPartyName(profile->caller_extension->extension_number);
                SetDisplayName(profile->caller_extension->extension_name);
            }
        }
        break;

    default:
        break;
    }

    switch (msg->message_id) {
    case SWITCH_MESSAGE_INDICATE_RINGING:
        SetPhase(OpalConnection::AlertingPhase);
        OnAlerting();
        break;

    case SWITCH_MESSAGE_INDICATE_DEFLECT:
    {
        PSafePtr<OpalConnection> other = GetOtherPartyConnection();
        if (other != NULL)
          other->TransferConnection(msg->string_arg);
        break;
    }

    case SWITCH_MESSAGE_INDICATE_PROGRESS:
    case SWITCH_MESSAGE_INDICATE_ANSWER:
        {
            int fixed = 0;
            
            if (switch_channel_test_flag(channel, CF_OUTBOUND)) {
                return SWITCH_STATUS_FALSE;
            }

            if (msg->message_id == SWITCH_MESSAGE_INDICATE_PROGRESS) {
                if (fixed) {
                    /* this should send alerting + media and wait for it to be established and return SUCCESS or FAIL
                       depending on if media was able to be established.  Need code to tell the other side we want early media here.
                    */
                    GetCall().OpenSourceMediaStreams(*this, OpalMediaType::Audio());
                    SetPhase(OpalConnection::AlertingPhase);
                    /* how do i say please establish early media ? */
                    OnAlerting();
                } else {
                    /* hack to avoid getting stuck, pre_answer will imply answer */
                    OnConnectedInternal();
                }
            } else {
                OnConnectedInternal();
            }

            // Wait for media
            PTRACE(2, "mod_opal\tAwaiting media start on connection " << *this);
            m_rxAudioOpened.Wait();
            m_txAudioOpened.Wait();
            
            if (GetPhase() >= ReleasingPhase) {
                // Call got aborted
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_fsSession), SWITCH_LOG_ERROR, "Call abandoned!\n");
                return SWITCH_STATUS_FALSE;
            }

            PTRACE(4, "mod_opal\tMedia started on connection " << *this);

            if (msg->message_id == SWITCH_MESSAGE_INDICATE_PROGRESS) {
                if (!switch_channel_test_flag(m_fsChannel, CF_EARLY_MEDIA)) {
                    switch_channel_mark_pre_answered(m_fsChannel);
                }
            } else {
                if (!switch_channel_test_flag(m_fsChannel, CF_EARLY_MEDIA)) {
                    switch_channel_mark_answered(m_fsChannel);
                }
            }

        }
        break;

    default:
        PTRACE(3, "mod_opal\tReceived message " << msg->message_id << " on connection " << *this);
    }

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::receive_event(switch_event_t *event)
{
    PTRACE(3, "mod_opal\tReceived event " << event->event_id << " on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::state_change()
{
    PTRACE(3, "mod_opal\tState changed on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::read_audio_frame(switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
    return read_frame(OpalMediaType::Audio(), frame, flags);
}


switch_status_t FSConnection::write_audio_frame(switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
    return write_frame(OpalMediaType::Audio(), frame, flags);
}


switch_status_t FSConnection::read_video_frame(switch_frame_t **frame, switch_io_flag_t flag, int stream_id)
{
    return read_frame(OpalMediaType::Video(), frame, flag);
}


switch_status_t FSConnection::write_video_frame(switch_frame_t *frame, switch_io_flag_t flag, int stream_id)
{
    return write_frame(OpalMediaType::Video(), frame, flag);
}


switch_status_t FSConnection::read_frame(const OpalMediaType & mediaType, switch_frame_t **frame, switch_io_flag_t flags)
{
    PSafePtr < FSMediaStream > stream = PSafePtrCast < OpalMediaStream, FSMediaStream > (GetMediaStream(mediaType, false));
    return stream != NULL ? stream->read_frame(frame, flags) : SWITCH_STATUS_FALSE;
}


switch_status_t FSConnection::write_frame(const OpalMediaType & mediaType, const switch_frame_t *frame, switch_io_flag_t flags)
{
    PSafePtr < FSMediaStream > stream = PSafePtrCast < OpalMediaStream, FSMediaStream > (GetMediaStream(mediaType, true));
    return stream != NULL ? stream->write_frame(frame, flags) : SWITCH_STATUS_FALSE;
}


///////////////////////////////////////////////////////////////////////

FSMediaStream::FSMediaStream(FSConnection & conn, const OpalMediaFormat & mediaFormat, unsigned sessionID, bool isSource)
    : OpalMediaStream(conn, mediaFormat, sessionID, isSource)
    , m_fsSession(conn.GetSession())
    , m_readRTP(0, 512)
    , m_callOnStart(true)
{
    memset(&m_readFrame, 0, sizeof(m_readFrame));
    m_readFrame.codec = m_switchCodec;
    m_readFrame.flags = SFF_RAW_RTP;
}


PBoolean FSMediaStream::Open()
{
    opal_private_t *tech_pvt = (opal_private_t *) switch_core_session_get_private(m_fsSession);

    if (IsOpen()) {
        return true;
    }

    bool isAudio;
    if (mediaFormat.GetMediaType() == OpalMediaType::Audio()) {
        isAudio = true;
    } else if (mediaFormat.GetMediaType() == OpalMediaType::Video()) {
        isAudio = false;
    } else {
        return OpalMediaStream::Open();
    }

    m_fsChannel = switch_core_session_get_channel(m_fsSession);
    
    int ptime = mediaFormat.GetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption()) * mediaFormat.GetFrameTime() / mediaFormat.GetTimeUnits();


    if (IsSink()) {
        m_switchCodec = isAudio ? &tech_pvt->read_codec : &tech_pvt->vid_read_codec;
        m_switchTimer = isAudio ? &tech_pvt->read_timer : &tech_pvt->vid_read_timer;
    } else {
        m_switchCodec = isAudio ? &tech_pvt->write_codec : &tech_pvt->vid_write_codec;
    }

    // The following is performed on two different instances of this object.
    if (switch_core_codec_init(m_switchCodec, mediaFormat.GetEncodingName(), NULL, // FMTP
                               mediaFormat.GetClockRate(), ptime, 1,  // Channels
                               SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,   // Settings
                               switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {
        // Could not select a codecs using negotiated frames/packet, so try using default.
        if (switch_core_codec_init(m_switchCodec, mediaFormat.GetEncodingName(), NULL, // FMTP
                                   mediaFormat.GetClockRate(), 0, 1,  // Channels
                                   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,   // Settings
                                   switch_core_session_get_pool(m_fsSession)) != SWITCH_STATUS_SUCCESS) {
            PTRACE(1, "mod_opal  " << switch_channel_get_name(m_fsChannel)<< " Cannot initialise " << (IsSink()? "read" : "write") << ' '
                   << mediaFormat.GetMediaType() << " codec " << mediaFormat << " for connection " << *this);
            switch_channel_hangup(m_fsChannel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
            return false;
        }
        PTRACE(2, "mod_opal " << switch_channel_get_name(m_fsChannel)<< " Unsupported ptime of " << ptime << " on " << (IsSink()? "read" : "write") << ' '
               << mediaFormat.GetMediaType() << " codec " << mediaFormat << " for connection " << *this);
    }

    PTRACE(1, "mod_opal " << switch_channel_get_name(m_fsChannel)<< " initialise " << 
           switch_channel_get_name(m_fsChannel) << (IsSink()? "read" : "write") << ' '
           << mediaFormat.GetMediaType() << " codec " << mediaFormat << " for connection " << *this);

    if (IsSink()) {
        m_readFrame.rate = mediaFormat.GetClockRate();

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

    PTRACE(3, "mod_opal\tSet " << (IsSink()? "read" : "write") << ' '
           << mediaFormat.GetMediaType() << " codec to << " << mediaFormat << " for connection " << *this);

    return OpalMediaStream::Open();
}


PBoolean FSMediaStream::Close()
{
    if (!IsOpen())
        return false;

    /* forget these FS will properly destroy them for us */

    m_switchTimer = NULL;
    m_switchCodec = NULL;

    return OpalMediaStream::Close();
}


PBoolean FSMediaStream::IsSynchronous() const
{
    return true;
}


PBoolean FSMediaStream::RequiresPatchThread(OpalMediaStream *) const
{
    return false;
}

bool FSMediaStream::CheckPatchAndLock()
{
    if (GetConnection().GetPhase() >= GetConnection().ReleasingPhase || !IsOpen())
        return false;

    if (LockReadWrite()) {
        if (!GetPatch() || !IsOpen()) {
            UnlockReadWrite();
            return false;
        }
        return true;
    } else {
        return false;
    }
}

switch_status_t FSMediaStream::read_frame(switch_frame_t **frame, switch_io_flag_t flags)
{

    if (!m_switchCodec) {
        return SWITCH_STATUS_FALSE;
    }

    if (m_callOnStart) {
        /*
          There is a race here... sometimes we make it here and GetPatch() is NULL
          if we wait it shows up in 1ms, maybe there is a better way to wait.
          
        */
        while(!GetPatch()) {
            if (!m_fsChannel || !switch_channel_up(m_fsChannel)) {
                return SWITCH_STATUS_FALSE;
            }
            switch_cond_next();
        }
        if (CheckPatchAndLock()) {
            GetPatch()->OnStartMediaPatch();
            m_callOnStart = false;
            UnlockReadWrite();
        } else {
            return SWITCH_STATUS_FALSE; 
        }
    }

    m_readFrame.flags = 0;

    /*
    while (switch_channel_ready(m_fsChannel)) {
        if (CheckPatchAndLock()) {
            if (!GetPatch()->GetSource().ReadPacket(m_readRTP)) {
                UnlockReadWrite();
                return SWITCH_STATUS_FALSE;
            }            
            UnlockReadWrite();
        } else {
            return SWITCH_STATUS_FALSE; 
        }
        
        if ((m_readFrame.datalen = m_readRTP.GetPayloadSize()) || switch_core_timer_check(&m_switchTimer, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
            if (m_readFrame.datalen) {
            } else {
                m_readFrame.flags = SFF_CNG;
            }
            break;
        }

        switch_yield(1000);
    }
    */

    if (switch_channel_test_private_flag(m_fsChannel, CF_NEED_FLUSH)) {
        switch_channel_clear_private_flag(m_fsChannel, CF_NEED_FLUSH);
        for(;;) {
            if (CheckPatchAndLock()) {
                if (!GetPatch()->GetSource().ReadPacket(m_readRTP)) {
                    UnlockReadWrite();
                    return SWITCH_STATUS_FALSE;
                }            
                UnlockReadWrite();
            } else {
                return SWITCH_STATUS_FALSE; 
            }

            if (!m_readRTP.GetPayloadSize()) {
                m_readFrame.flags = SFF_CNG;
                break;
            }
        }
    } else {

        if (CheckPatchAndLock()) {
            if (!m_switchTimer || !GetPatch()->GetSource().ReadPacket(m_readRTP)) {
                UnlockReadWrite();
                return SWITCH_STATUS_FALSE;
            }            
            UnlockReadWrite();
        } else {
            return SWITCH_STATUS_FALSE; 
        }
    
        switch_core_timer_next(m_switchTimer);
    
        if (!(m_readFrame.datalen = m_readRTP.GetPayloadSize())) {
            m_readFrame.flags = SFF_CNG;
        }
    }

    if (!switch_channel_ready(m_fsChannel)) {
        return SWITCH_STATUS_FALSE;
    }

    if (!switch_core_codec_ready(m_switchCodec)) {
        return SWITCH_STATUS_FALSE;
    }

    //switch_core_timer_step(&m_switchTimer);

    if (m_readFrame.payload == RTP_DataFrame::CN || m_readFrame.payload == RTP_DataFrame::Cisco_CN) {
        m_readFrame.flags = SFF_CNG;
    }

    if (m_readFrame.flags & SFF_CNG) {
        m_readFrame.buflen = sizeof(m_buf);
        m_readFrame.data = m_buf;
        m_readFrame.packet = NULL;
        m_readFrame.packetlen = 0;
        m_readFrame.timestamp = 0;
        m_readFrame.m = SWITCH_FALSE;
        m_readFrame.seq = 0;
        m_readFrame.ssrc = 0;
        m_readFrame.codec = m_switchCodec;
    } else {
        m_readFrame.buflen = m_readRTP.GetSize();
        m_readFrame.data = m_readRTP.GetPayloadPtr();
        m_readFrame.packet = m_readRTP.GetPointer();
        m_readFrame.packetlen = m_readRTP.GetHeaderSize() + m_readFrame.datalen;
        m_readFrame.payload = (switch_payload_t) m_readRTP.GetPayloadType();
        m_readFrame.timestamp = m_readRTP.GetTimestamp();
        m_readFrame.m = (switch_bool_t) m_readRTP.GetMarker();
        m_readFrame.seq = m_readRTP.GetSequenceNumber();
        m_readFrame.ssrc = m_readRTP.GetSyncSource();
        m_readFrame.codec = m_switchCodec;
    }

    *frame = &m_readFrame;

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSMediaStream::write_frame(const switch_frame_t *frame, switch_io_flag_t flags)
{
    if (!switch_channel_ready(m_fsChannel)) {
        return SWITCH_STATUS_FALSE;
    }

    if (m_callOnStart) {
        if (CheckPatchAndLock()) {
            GetPatch()->OnStartMediaPatch();
            m_callOnStart = false;
            UnlockReadWrite();
        } else {
            return SWITCH_STATUS_FALSE; 
        }
    }

    if ((frame->flags & SFF_CNG)) {
        return SWITCH_STATUS_SUCCESS;
    }

    if ((frame->flags & SFF_RAW_RTP) != 0) {
        RTP_DataFrame rtp((const BYTE *) frame->packet, frame->packetlen, false);

        if (CheckPatchAndLock()) {
            if (GetPatch()->PushFrame(rtp)) {
                UnlockReadWrite();
                return SWITCH_STATUS_SUCCESS;
            }
            UnlockReadWrite();
        } else {
            return SWITCH_STATUS_FALSE; 
        }
    } 
    
    /* If we reach this code it means a call to an ivr or something else that does not generate timestamps
       Its possible that frame->timestamp is set but not guarenteed and is best ignored for the time being.
       We are probably relying on the rtp stack to generate the timestamp and ssrc for us at this point.
       As a quick hack I am going to keep a sample counter and increment it by frame->samples but it would be 
       better if we could engage whatever it is in opal that makes it generate the timestamp.
     */

    RTP_DataFrame rtp(frame->datalen);
    rtp.SetPayloadType(mediaFormat.GetPayloadType());

    m_timeStamp += frame->samples;
    rtp.SetTimestamp(m_timeStamp);
    
    //rtp.SetTimestamp(frame->timestamp);
    //rtp.SetSyncSource(frame->ssrc);
    //rtp.SetMarker(frame->m);

    memcpy(rtp.GetPayloadPtr(), frame->data, frame->datalen);

    if (CheckPatchAndLock()) {
        if (GetPatch()->PushFrame(rtp)) {
            UnlockReadWrite();
            return SWITCH_STATUS_SUCCESS;
        }
        UnlockReadWrite();
    } else {
        return SWITCH_STATUS_FALSE; 
    }


    return SWITCH_STATUS_FALSE;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:s:
 */
