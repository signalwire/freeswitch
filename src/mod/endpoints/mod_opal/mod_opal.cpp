/* Opal endpoint interface for Freeswitch Modular Media Switching Software Library /
 * Soft-Switch Application
 *
 * Version: MPL 1.1
 *
 * Copyright (c) 2007 Tuyan Ozipek (tuyanozipek@gmail.com)
 * Copyright (c) 2008-2012 Vox Lucida Pty. Ltd. (robertj@voxlucida.com.au)
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
#if PTLIB_CHECK_VERSION(2,11,1)
#include <rtp/rtp_session.h>
#endif
#include <h323/h323pdu.h>
#include <h323/gkclient.h>


/* FreeSWITCH does not correctly handle an H.323 subtely, that is that a
   MAXIMUM audio frames per packet is negotiated, and there is no
   requirement for the remote to actually send that many. So, in say GSM, we
   negotiate up to 3 frames or 60ms of data and the remote actually sends one
   (20ms) frame per packet. Perfectly legal but blows up the media handling
   in FS.

   Eventually we will get around to bundling the packets, but not yet. This
   compile flag will just force one frame/packet for all audio codecs.
 */
#define IMPLEMENT_MULTI_FAME_AUDIO 0


static switch_call_cause_t create_outgoing_channel(switch_core_session_t   *session,
                                                   switch_event_t          *var_event,
                                                   switch_caller_profile_t *outbound_profile,
                                                   switch_core_session_t  **new_session,
                                                   switch_memory_pool_t   **pool,
                                                   switch_originate_flag_t  flags,
                                                   switch_call_cause_t     *cancel_cause);


static FSProcess *opal_process = NULL;


static PConstString const ModuleName("opal");
static char const ConfigFile[] = "opal.conf";
#define FS_PREFIX "fs"


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
    /*.on_hangup */ FSConnection::on_hangup,
    /*.on_exchange_media */ FSConnection::on_exchange_media,
    /*.on_soft_execute */ FSConnection::on_soft_execute,
    /*.on_consume_media*/ NULL,
    /*.on_hibernate*/ NULL,
    /*.on_reset*/ NULL,
    /*.on_park*/ NULL,
    /*.on_reporting*/ NULL,
    /*.on_destroy*/ FSConnection::on_destroy
};


SWITCH_BEGIN_EXTERN_C
/*******************************************************************************/

SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown);
SWITCH_MODULE_DEFINITION(mod_opal, mod_opal_load, mod_opal_shutdown, NULL);

SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Starting loading mod_opal\n");

    /* Prevent the loading of OPAL codecs via "plug ins", this is a directory
       full of DLLs that will be loaded automatically. */
    (void)putenv((char *)"PTLIBPLUGINDIR=/no/thanks");


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
        //return SWITCH_STATUS_UNLOAD;
        return SWITCH_STATUS_SUCCESS;
    }

    delete opal_process;
    opal_process = NULL;
    return SWITCH_STATUS_FALSE;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown)
{
    delete opal_process;
    opal_process = NULL;
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C
/*******************************************************************************/

///////////////////////////////////////////////////////////////////////

#if PTRACING

class FSTrace : public std::ostream
{
private:
  class Buffer : public std::stringbuf
  {
    virtual int sync()
    {
      std::string s = str();
      if (s.empty())
        return 0;

      //Due to explicit setting of flags we know exactly what we are getting
  #define THREAD_ID_INDEX 2
  #define FILE_NAME_INDEX 3
  #define FILE_LINE_INDEX 4
#if PTLIB_CHECK_VERSION(2,11,1)
  #define CONTEXT_ID_REGEX "([0-9]+|- - - - - - -)\t"
  #define LOG_PRINTF_FORMAT "{%s,%s} %s"
  #define FULL_TEXT_INDEX 6
#else
  #define CONTEXT_ID_REGEX
  #define LOG_PRINTF_FORMAT "{%s} %s"
  #define FULL_TEXT_INDEX 5
#endif
      PStringArray fields(7);
      static PRegularExpression logRE("^([0-9]+)\t *(.+)\t *([^(]+)\\(([0-9]+)\\)\t"CONTEXT_ID_REGEX"(.*)",
                                      PRegularExpression::Extended);
      if (!logRE.Execute(s.c_str(), fields)) {
        fields[1] = "4";
        fields[THREAD_ID_INDEX] = "unknown";
        fields[FILE_NAME_INDEX] = __FILE__;
        fields[FILE_LINE_INDEX] = __LINE__;
        fields[FULL_TEXT_INDEX] = s;
      }

      switch_log_level_t level;
      switch (fields[1].AsUnsigned()) {
        case 0 :
          level = SWITCH_LOG_ALERT;
          break;
        case 1 :
          level = SWITCH_LOG_ERROR;
          break;
        case 2 :
          level = SWITCH_LOG_WARNING;
          break;
        case 3 :
          level = SWITCH_LOG_INFO;
          break;
        default :
          level = SWITCH_LOG_DEBUG;
          break;
      }

      fields[4].Replace("\t", " ", true);
#if PTLIB_CHECK_VERSION(2,11,1)
      fields[5].Replace("- - - - - - -", "-"),
#endif
      switch_log_printf(SWITCH_CHANNEL_ID_LOG,
                        fields[FILE_NAME_INDEX],
                        "PTLib-OPAL",
                        fields[FILE_LINE_INDEX].AsUnsigned(),
                        NULL,
                        level,
                        LOG_PRINTF_FORMAT,
                        fields[THREAD_ID_INDEX].GetPointer(),
#if PTLIB_CHECK_VERSION(2,11,1)
                        fields[5].GetPointer(),
#endif
                        fields[FULL_TEXT_INDEX].GetPointer());

      // Reset string
      str(std::string());
      return 0;
    }
  } buffer;

public:
  FSTrace()
    : ostream(&buffer)
  {
  }
};

#endif // PTRACING


///////////////////////////////////////////////////////////////////////

FSProcess::FSProcess()
  : PLibraryProcess("Vox Lucida Pty. Ltd.", MODNAME, 1, 1, BetaCode, 1)
  , m_manager(NULL)
{
}


FSProcess::~FSProcess()
{
  delete m_manager;
#if PTRACING
    PTrace::SetStream(NULL); // This will delete the FSTrace object
#endif
}


bool FSProcess::Initialise(switch_loadable_module_interface_t *iface)
{
    m_manager = new FSManager();
    return m_manager != NULL && m_manager->Initialise(iface);
}


///////////////////////////////////////////////////////////////////////

FSManager::FSManager()
  : m_context("default")
  , m_dialplan("XML")
{
    // These are deleted by the OpalManager class, no need to have destructor
    m_h323ep = new H323EndPoint(*this);
    m_iaxep = new IAX2EndPoint(*this);
    m_fsep = new FSEndPoint(*this);
}


bool FSManager::Initialise(switch_loadable_module_interface_t *iface)
{
    ReadConfig(false);

    m_FreeSwitch = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(iface, SWITCH_ENDPOINT_INTERFACE);
    m_FreeSwitch->interface_name = ModuleName;
    m_FreeSwitch->io_routines = &opalfs_io_routines;
    m_FreeSwitch->state_handler = &opalfs_event_handlers;

    silenceDetectParams.m_mode = OpalSilenceDetector::NoSilenceDetection;

    if (m_listeners.empty()) {
        m_h323ep->StartListener("");
    } else {
        for (std::list < FSListener >::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it) {
            if (!m_h323ep->StartListener(OpalTransportAddress(it->m_address, it->m_port))) {
                PTRACE(2, "mod_opal\tCannot start listener for " << it->m_name);
            }
        }
    }

    AddRouteEntry("h323:.* = "FS_PREFIX":<da>");  // config option for direct routing
    AddRouteEntry("iax2:.* = "FS_PREFIX":<da>");  // config option for direct routing
    AddRouteEntry(FS_PREFIX":.* = h323:<da>");  // config option for direct routing

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

#if !IMPLEMENT_MULTI_FAME_AUDIO
    OpalMediaFormatList allCodecs = OpalMediaFormat::GetAllRegisteredMediaFormats();
    for (OpalMediaFormatList::iterator it = allCodecs.begin(); it != allCodecs.end(); ++it) {
      if (it->GetMediaType() == OpalMediaType::Audio()) {
        int ms_per_frame = it->GetFrameTime()/it->GetTimeUnits();
        int frames_in_20_ms = (ms_per_frame+19)/ms_per_frame;
        it->SetOptionInteger(OpalAudioFormat::RxFramesPerPacketOption(), frames_in_20_ms);
        it->SetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption(), frames_in_20_ms);
        OpalMediaFormat::SetRegisteredMediaFormat(*it);
        PTRACE(4, "mod_opal\tSet " << *it << " to " << frames_in_20_ms << "frames/packet");
      }
    }
#endif // IMPLEMENT_MULTI_FAME_AUDIO

    OpalMediaFormat t38 = OpalT38;
    t38.SetOptionBoolean("UDPTL-Raw-Mode", true);
    OpalMediaFormat::SetRegisteredMediaFormat(t38);

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
    switch_event_t *request_params = NULL;
    switch_event_create(&request_params, SWITCH_EVENT_REQUEST_PARAMS);
    switch_assert(request_params);
    switch_event_add_header_string(request_params, SWITCH_STACK_BOTTOM, "profile", switch_str_nil(""));

    switch_xml_t cfg;
    switch_xml_t xml = switch_xml_open_cfg(ConfigFile, &cfg, request_params);
    if (xml == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", ConfigFile);
        return SWITCH_STATUS_FALSE;
    }

    switch_xml_t xmlSettings = switch_xml_child(cfg, "settings");
    if (xmlSettings) {
        for (switch_xml_t xmlParam = switch_xml_child(xmlSettings, "param"); xmlParam != NULL; xmlParam = xmlParam->next) {
            PConstCaselessString const var(switch_xml_attr_soft(xmlParam, "name"));
            PConstString const val(switch_xml_attr_soft(xmlParam, "value"));

            if (var == "context") {
                m_context = val;
            } else if (var == "dialplan") {
                m_dialplan = val;
            } else if (var == "codec-prefs") {
                m_codecPrefs = val;
            } else if (var == "disable-transcoding") {
                m_disableTranscoding = switch_true(val);
            } else if (var == "dtmf-type") {
                if (val == "string")
                  m_h323ep->SetSendUserInputMode(OpalConnection::SendUserInputAsString);
                else if (val == "signal")
                  m_h323ep->SetSendUserInputMode(OpalConnection::SendUserInputAsTone);
                else if (val == "rfc2833")
                  m_h323ep->SetSendUserInputMode(OpalConnection::SendUserInputAsRFC2833);
                else if (val == "in-band")
                  m_h323ep->SetSendUserInputMode(OpalConnection::SendUserInputInBand);
            } else if (var == "jitter-size") {
                SetAudioJitterDelay(val.AsUnsigned(), val.Mid(val.Find(',')+1).AsUnsigned()); // In milliseconds
            } else if (var == "gk-address") {
                m_gkAddress = val;
            } else if (var == "gk-identifer") {
                m_gkIdentifer = val;
            } else if (var == "gk-interface") {
                m_gkInterface = val;
#if PTRACING
            } else if (var == "trace-level") {
                unsigned level = val.AsUnsigned();
                if (level > 0) {
                    PTrace::SetLevel(level);
                    PTrace::ClearOptions(0xffffffff); // Everything off
                    PTrace::SetOptions(               // Except these
                      PTrace::TraceLevel|PTrace::FileAndLine|PTrace::Thread
#if PTLIB_CHECK_VERSION(2,11,1)
                      |PTrace::ContextIdentifier
#endif
                    );
                    PTrace::SetStream(new FSTrace);
                }
#endif
            }
        }
    }

    switch_xml_t xmlListeners = switch_xml_child(cfg, "listeners");
    if (xmlListeners != NULL) {
        for (switch_xml_t xmlListener = switch_xml_child(xmlListeners, "listener"); xmlListener != NULL; xmlListener = xmlListener->next) {

            m_listeners.push_back(FSListener());
            FSListener & listener = m_listeners.back();

            listener.m_name = switch_xml_attr_soft(xmlListener, "name");
            if (listener.m_name.IsEmpty())
                listener.m_name = "unnamed";

            for (switch_xml_t xmlParam = switch_xml_child(xmlListener, "param"); xmlParam != NULL; xmlParam = xmlParam->next) {
                PConstCaselessString const var(switch_xml_attr_soft(xmlParam, "name"));
                PConstString const val(switch_xml_attr_soft(xmlParam, "value"));
                if (var == "h323-ip")
                    listener.m_address = val;
                else if (var == "h323-port")
                    listener.m_port = (uint16_t)val.AsUnsigned();
            }

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created Listener '%s'\n", (const char *) listener.m_name);
        }
    }

    switch_event_destroy(&request_params);

    if (xml)
        switch_xml_free(xml);

    return SWITCH_STATUS_SUCCESS;
}


static switch_call_cause_t create_outgoing_channel(switch_core_session_t   *session,
                                                   switch_event_t          *var_event,
                                                   switch_caller_profile_t *outbound_profile,
                                                   switch_core_session_t  **new_session,
                                                   switch_memory_pool_t   **pool,
                                                   switch_originate_flag_t  flags,
                                                   switch_call_cause_t     *cancel_cause)
{
    if (opal_process == NULL)
      return SWITCH_CAUSE_CRASH;

    FSConnection::outgoing_params params;
    params.var_event        = var_event;
    params.outbound_profile = outbound_profile;
    params.new_session      = new_session;
    params.pool             = pool;
    params.flags            = flags;
    params.cancel_cause     = cancel_cause;
    params.fail_cause       = SWITCH_CAUSE_INVALID_NUMBER_FORMAT;

    if (opal_process->GetManager().SetUpCall(FS_PREFIX":", outbound_profile->destination_number, &params) != NULL)
        return SWITCH_CAUSE_SUCCESS;

    if (*new_session != NULL)
        switch_core_session_destroy(new_session);
    return params.fail_cause;
}


///////////////////////////////////////////////////////////////////////

FSEndPoint::FSEndPoint(FSManager & manager)
  : OpalLocalEndPoint(manager, FS_PREFIX)
  , m_manager(manager)
{
    PTRACE(4, "mod_opal\tFSEndPoint created.");
}


OpalLocalConnection *FSEndPoint::CreateConnection(OpalCall & call, void *userData, unsigned options, OpalConnection::StringOptions* stringOptions)
{
    return new FSConnection(call, *this, options, stringOptions, (FSConnection::outgoing_params *)userData);
}


///////////////////////////////////////////////////////////////////////

FSConnection::FSConnection(OpalCall & call,
                           FSEndPoint & endpoint,
                           unsigned options,
                           OpalConnection::StringOptions* stringOptions,
                           outgoing_params * params)
  : OpalLocalConnection(call, endpoint, NULL, options, stringOptions)
  , m_endpoint(endpoint)
  , m_fsSession(NULL)
  , m_fsChannel(NULL)
  , m_flushAudio(false)
  , m_udptl(false)
{
    memset(&m_read_timer, 0, sizeof(m_read_timer));
    memset(&m_read_codec, 0, sizeof(m_read_codec));
    memset(&m_write_codec, 0, sizeof(m_write_codec));
    memset(&m_vid_read_timer, 0, sizeof(m_vid_read_timer));
    memset(&m_vid_read_codec, 0, sizeof(m_vid_read_codec));
    memset(&m_vid_write_codec, 0, sizeof(m_vid_write_codec));
    memset(&m_dummy_frame, 0, sizeof(m_dummy_frame));
    m_dummy_frame.flags = SFF_CNG;

    if (params != NULL) {
        // If we fail, this is the cause
        params->fail_cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

        if ((m_fsSession = switch_core_session_request(endpoint.GetManager().GetSwitchInterface(),
                                      SWITCH_CALL_DIRECTION_OUTBOUND, params->flags, params->pool)) == NULL) {
            PTRACE(1, "mod_opal\tCannot create session for outgoing call.");
            return;
        }
    }
    else {
      if ((m_fsSession = switch_core_session_request(endpoint.GetManager().GetSwitchInterface(),
                                                     SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL)) == NULL) {
            PTRACE(1, "mod_opal\tCannot create session for incoming call.");
            return;
      }
    }

    if ((m_fsChannel = switch_core_session_get_channel(m_fsSession)) == NULL) {
        switch_core_session_destroy(&m_fsSession);
        return;
    }

    switch_core_session_set_private(m_fsSession, this);
    SafeReference(); // Make sure cannot be deleted until on_destroy()

    if (params != NULL) {
        switch_caller_profile_t *caller_profile = switch_caller_profile_clone(m_fsSession, params->outbound_profile);
        switch_channel_set_caller_profile(m_fsChannel, caller_profile);
        SetLocalPartyName(caller_profile->caller_id_number);
        SetDisplayName(caller_profile->caller_id_name);

        *params->new_session = m_fsSession;
    }

    switch_channel_set_state(m_fsChannel, CS_INIT);
}


bool FSConnection::OnOutgoingSetUp()
{
    if (m_fsSession == NULL || m_fsChannel == NULL) {
        PTRACE(1, "mod_opal\tSession request failed.");
        return false;
    }

    // Transfer FS caller_id_number & caller_id_name from the FSConnection
    // to the protocol connection (e.g. H.323) so gets sent correctly
    // in outgoing packets
    PSafePtr<OpalConnection> proto = GetOtherPartyConnection();
    if (proto == NULL) {
        PTRACE(1, "mod_opal\tNo protocol connection in call.");
        return false;
    }

    proto->SetLocalPartyName(GetLocalPartyName());
    proto->SetDisplayName(GetDisplayName());

    switch_channel_set_name(m_fsChannel, ModuleName + '/' + GetRemotePartyURL());
    return true;
}


bool FSConnection::OnIncoming()
{
    if (m_fsSession == NULL || m_fsChannel == NULL) {
        PTRACE(1, "mod_opal\tSession request failed.");
        return false;
    }

    switch_core_session_add_stream(m_fsSession, NULL);

    PURL url = GetRemotePartyURL();
    switch_caller_profile_t *caller_profile = switch_caller_profile_new(
          switch_core_session_get_pool(m_fsSession),
          url.GetUserName(),      /** username */
          m_endpoint.GetManager().GetDialPlan(), /** dial plan */
          GetRemotePartyName(),   /** caller_id_name */
          GetRemotePartyNumber(), /** caller_id_number */
          url.GetHostName(),      /** network addr */
          NULL,                   /** ANI */
          NULL,                   /** ANI II */
          NULL,                   /** RDNIS */
          ModuleName,             /** source */
          m_endpoint.GetManager().GetContext(), /** set context  */
          GetCalledPartyNumber()  /** destination_number */
    );
    if (caller_profile == NULL) {
        PTRACE(1, "mod_opal\tCould not create caller profile");
        return false;
    }

    PTRACE(4, "mod_opal\tCreated switch caller profile:\n"
              "  username          = " << caller_profile->username << "\n"
              "  dialplan          = " << caller_profile->dialplan << "\n"
              "  caller_id_name    = " << caller_profile->caller_id_name << "\n"
              "  caller_id_number  = " << caller_profile->caller_id_number << "\n"
              "  network_addr      = " << caller_profile->network_addr << "\n"
              "  source            = " << caller_profile->source << "\n"
              "  context           = " << caller_profile->context << "\n"
              "  destination_number= " << caller_profile->destination_number);
    switch_channel_set_caller_profile(m_fsChannel, caller_profile);

    switch_channel_set_name(m_fsChannel, ModuleName + '/' + url.GetScheme() + ':' + caller_profile->destination_number);

    if (switch_core_session_thread_launch(m_fsSession) != SWITCH_STATUS_SUCCESS) {
        PTRACE(1, "mod_opal\tCould not launch session thread");
        switch_core_session_destroy(&m_fsSession);
        m_fsChannel = NULL;
        return false;
    }

    return true;
}


void FSConnection::OnEstablished()
{
  OpalLocalConnection::OnEstablished();

  if (switch_channel_direction(m_fsChannel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
    PTRACE(4, "mod_opal\tOnEstablished for outbound call, checking for media");
    if (GetMediaStream(OpalMediaType::Audio(), true) != NULL && GetMediaStream(OpalMediaType::Audio(), false) != NULL) {
      PTRACE(3, "mod_opal\tOnEstablished for outbound call, making call answered");
      switch_channel_mark_answered(m_fsChannel);
    }
  }
}


void FSConnection::OnReleased()
{
    m_rxAudioOpened.Signal();   // Just in case
    m_txAudioOpened.Signal();

    if (m_fsChannel != NULL) {
        PTRACE(3, "mod_opal\tHanging up FS side");
        switch_channel_hangup(m_fsChannel, (switch_call_cause_t)callEndReason.q931);
    }

    OpalLocalConnection::OnReleased();
}


PBoolean FSConnection::SetAlerting(const PString & calleeName, PBoolean withMedia)
{
    if (PAssertNULL(m_fsChannel) == NULL)
        return false;

    switch_channel_mark_ring_ready(m_fsChannel);
    return OpalLocalConnection::SetAlerting(calleeName, withMedia);
}


PBoolean FSConnection::SendUserInputTone(char tone, unsigned duration)
{
    if (PAssertNULL(m_fsChannel) == NULL)
        return false;

    switch_dtmf_t dtmf = { tone, duration };
    PTRACE(4, "mod_opal\tSending DTMF to FS: tone=" << tone << ", duration=" << duration);
    return switch_channel_queue_dtmf(m_fsChannel, &dtmf) == SWITCH_STATUS_SUCCESS;
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

    PString codec_string = switch_channel_get_variable(m_fsChannel, "absolute_codec_string");
    if (codec_string.IsEmpty()) {
        codec_string = switch_channel_get_variable(m_fsChannel, "codec_string");
        if (codec_string.IsEmpty()) {
            codec_string = m_endpoint.GetManager().GetCodecPrefs();
            if (codec_string.IsEmpty()) {
                numCodecs = switch_loadable_module_get_codecs(codecs, sizeof(codecs) / sizeof(codecs[0]));
                for (int i = 0; i < numCodecs; i++) {
                  if (i > 0)
                    codec_string += ',';
                  codec_string += codecs[i]->iananame;
                }
                PTRACE(4, "mod_opal\tDefault to all loaded codecs=" << codec_string);
            }
            else {
                PTRACE(4, "mod_opal\tSettings codec-prefs=" << codec_string);
            }
        }
        else {
            PTRACE(4, "mod_opal\tChannel codec_string=" << codec_string);
        }

        PString orig_codec = switch_channel_get_variable(m_fsChannel, SWITCH_ORIGINATOR_CODEC_VARIABLE);
        if (!orig_codec.IsEmpty()) {
          if (m_endpoint.GetManager().GetDisableTranscoding()) {
              codec_string = orig_codec;
              PTRACE(4, "mod_opal\tNo transcoding, forced to originator codec=" << orig_codec);
          }
          else {
              codec_string.Splice(orig_codec+',', 0);
              PTRACE(4, "mod_opal\tSetting preference to originator codec=" << orig_codec);
          }
        }
    }
    else {
        PTRACE(4, "mod_opal\tChannel absolute_codec_string=" << codec_string);
    }

    {
        char *codec_order[SWITCH_MAX_CODECS];
        int codec_order_last = switch_separate_string((char *)codec_string.GetPointer(), ',', codec_order, SWITCH_MAX_CODECS);
        numCodecs = switch_loadable_module_get_codecs_sorted(codecs, SWITCH_MAX_CODECS, codec_order, codec_order_last);
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
              PTRACE(2, "mod_opal\tCould not match FS codec "
                     << codec->iananame << '@' << codec->samples_per_second
                     << " (pt=" << (unsigned)codec->ianacode << ")"
                        " to an OPAL media format.");
              continue;
            }
        }

        PTRACE(4, "mod_opal\tMatched FS codec " << codec->iananame << " to OPAL media format " << switchFormat);

#if IMPLEMENT_MULTI_FAME_AUDIO
        // Did we match or create a new media format?
        if (switchFormat.IsValid() && codec->codec_type == SWITCH_CODEC_TYPE_AUDIO) {
            // Calculate frames per packet, do not use codec->codec_frames_per_packet as that field
            // has slightly different semantics when used in streamed codecs such as G.711
            int fpp = codec->samples_per_packet/switchFormat.GetFrameTime();

            /* Set the frames/packet to maximum of what is in the FS table. The OPAL negotiations will
               drop the value from there. This might fail if there are "holes" in the FS table, e.g.
               if for some reason G.723.1 has 30ms and 90ms but not 60ms, then the OPAL negotiations
               could end up with 60ms and the codec cannot be created. The "holes" are unlikely in
               all but streamed codecs such as G.711, where it is theoretically possible for OPAL to
               come up with 32ms and there is only 30ms and 40ms in the FS table. We deem these
               scenarios sufficiently rare that we can safely ignore them ... for now. */

            if (fpp > switchFormat.GetOptionInteger(OpalAudioFormat::RxFramesPerPacketOption())) {
                switchFormat.SetOptionInteger(OpalAudioFormat::RxFramesPerPacketOption(), fpp);
            }

            if (fpp > switchFormat.GetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption())) {
                switchFormat.SetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption(), fpp);
            }
        }
#endif // IMPLEMENT_MULTI_FAME_AUDIO

        m_switchMediaFormats += switchFormat;
    }

#if HAVE_T38
    OpalMediaFormat t38 = OpalT38;

    /* We need to have a T.38 options for TCS, but may be before the
       spandsp_mod has set it us. So, if not, we actually give to spandsp_mod. */
    switch_t38_options_t *t38_options = (switch_t38_options_t *)switch_channel_get_private(m_fsChannel, "t38_options");
    if (t38_options == NULL)
      SetT38OptionsFromMediaFormat(t38, "_preconfigured_t38_options");
    else {
      t38.SetOptionInteger("T38FaxVersion", t38_options->T38FaxVersion);
      t38.SetOptionInteger("T38MaxBitRate", t38_options->T38MaxBitRate);
      t38.SetOptionBoolean("T38FaxFillBitRemoval", t38_options->T38FaxFillBitRemoval);
      t38.SetOptionBoolean("T38FaxTranscodingMMR", t38_options->T38FaxTranscodingMMR);
      t38.SetOptionBoolean("T38FaxTranscodingJBIG", t38_options->T38FaxTranscodingJBIG);
      t38.SetOptionValue("T38FaxRateManagement", t38_options->T38FaxRateManagement);
      t38.SetOptionInteger("T38Version", t38_options->T38FaxMaxBuffer);
      t38.SetOptionInteger("T38Version", t38_options->T38FaxMaxDatagram);
      t38.SetOptionValue("T38FaxUdpEC", t38_options->T38FaxUdpEC);
    }

    m_switchMediaFormats += t38;
#endif // HAVE_T38
}


OpalMediaStream *FSConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat, unsigned sessionID, PBoolean isSource)
{
    return new FSMediaStream(*this, mediaFormat, sessionID, isSource);
}


void FSConnection::OnPatchMediaStream(PBoolean isSource, OpalMediaPatch & patch)
{
    OpalConnection::OnPatchMediaStream(isSource, patch);

    if (PAssertNULL(m_fsChannel) == NULL)
        return;

    if (patch.GetSource().GetMediaFormat().GetMediaType() != OpalMediaType::Audio())
        return;

    if (switch_channel_direction(m_fsChannel) == SWITCH_CALL_DIRECTION_INBOUND) {
        PTRACE(4, "mod_opal\tOnPatchMediaStream for inbound call, flagging media opened");
        if (isSource)
            m_rxAudioOpened.Signal();
        else
            m_txAudioOpened.Signal();
    }
    else {
      PTRACE(4, "mod_opal\tOnPatchMediaStream for outbound call, checking media");
      if (GetMediaStream(OpalMediaType::Audio(), !isSource) != NULL) {
          // Have open media in both directions.
          if (IsEstablished()) {
              PTRACE(3, "mod_opal\tOnPatchMediaStream for outbound call, making call answered");
              switch_channel_mark_answered(m_fsChannel);
          }
          else if (!IsReleased()) {
              PTRACE(3, "mod_opal\tOnPatchMediaStream for outbound call, making call pre-answered");
              switch_channel_mark_pre_answered(m_fsChannel);
          }
      }
    }
}


switch_status_t FSConnection::on_init()
{
    if (PAssertNULL(m_fsChannel) == NULL)
        return SWITCH_STATUS_FALSE;

    PTRACE(4, "mod_opal\tStarted routing for connection " << *this);

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_routing()
{
    if (PAssertNULL(m_fsChannel) == NULL)
        return SWITCH_STATUS_FALSE;

    PTRACE(4, "mod_opal\tRouting connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_execute()
{
    if (PAssertNULL(m_fsChannel) == NULL)
        return SWITCH_STATUS_FALSE;

    PTRACE(4, "mod_opal\tExecuting connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_destroy()
{
    PTRACE(3, "mod_opal\tFS on_destroy for connection " << *this);

    m_fsChannel = NULL; // Will be destroyed by FS, so don't use it any more.

    switch_core_codec_destroy(&m_read_codec);
    switch_core_codec_destroy(&m_write_codec);
    switch_core_codec_destroy(&m_vid_read_codec);
    switch_core_codec_destroy(&m_vid_write_codec);
    switch_core_timer_destroy(&m_read_timer);
    switch_core_timer_destroy(&m_vid_read_timer);

    switch_core_session_set_private(m_fsSession, NULL);
    SafeDereference();

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_hangup()
{
    if (PAssertNULL(m_fsChannel) == NULL)
        return SWITCH_STATUS_FALSE;

    /* if this is still here it was our idea to hangup not opal's */
    ClearCallSynchronous(NULL, H323TranslateToCallEndReason(
              (Q931::CauseValues)switch_channel_get_cause_q850(m_fsChannel), UINT_MAX));

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_exchange_media()
{
    PTRACE(4, "mod_opal\tExchanging media on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::on_soft_execute()
{
    PTRACE(4, "mod_opal\tSoft execute on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::kill_channel(int sig)
{
    switch (sig) {
    case SWITCH_SIG_KILL:
        PTRACE(4, "mod_opal\tSignal KILL received on connection " << *this);
        m_rxAudioOpened.Signal();
        m_txAudioOpened.Signal();
        CloseMediaStreams();
        break;

    case SWITCH_SIG_BREAK:
        PTRACE(4, "mod_opal\tSignal BREAK received on connection " << *this);
        break;

    default:
        PTRACE(4, "mod_opal\tSignal " << sig << " received on connection " << *this);
        break;
    }

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::send_dtmf(const switch_dtmf_t *dtmf)
{
    PTRACE(4, "mod_opal\tReceived DTMF from FS: tone=" << dtmf->digit << ", duration=" << dtmf->duration);
    OnUserInputTone(dtmf->digit, dtmf->duration);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::receive_message(switch_core_session_message_t *msg)
{
    if (PAssertNULL(m_fsChannel) == NULL)
        return SWITCH_STATUS_FALSE;

    switch (msg->message_id) {
    case SWITCH_MESSAGE_INDICATE_RINGING:
    case SWITCH_MESSAGE_INDICATE_PROGRESS:
    case SWITCH_MESSAGE_INDICATE_ANSWER:
    case SWITCH_MESSAGE_INDICATE_DEFLECT:
        if (switch_channel_direction(m_fsChannel) == SWITCH_CALL_DIRECTION_INBOUND) {
            switch_caller_profile_t * profile = switch_channel_get_caller_profile(m_fsChannel);
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
        else {
            return SWITCH_STATUS_FALSE;
        }
        break;

    default:
        break;
    }

    switch (msg->message_id) {
    case SWITCH_MESSAGE_INDICATE_BRIDGE:
    case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
    case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:
        m_flushAudio = true;
        break;

    case SWITCH_MESSAGE_INDICATE_RINGING:
        AlertingIncoming();
        break;

    case SWITCH_MESSAGE_INDICATE_PROGRESS:
        AutoStartMediaStreams();
        AlertingIncoming();

        if (!WaitForMedia())
            return SWITCH_STATUS_FALSE;

        if (!switch_channel_test_flag(m_fsChannel, CF_EARLY_MEDIA)) {
            switch_channel_mark_pre_answered(m_fsChannel);
        }
        break;

    case SWITCH_MESSAGE_INDICATE_ANSWER:
        AcceptIncoming();

        if (!WaitForMedia())
            return SWITCH_STATUS_FALSE;

        if (!switch_channel_test_flag(m_fsChannel, CF_ANSWERED)) {
            switch_channel_mark_answered(m_fsChannel);
        }
        break;

    case SWITCH_MESSAGE_INDICATE_DEFLECT:
        ownerCall.Transfer(msg->string_arg, GetOtherPartyConnection());
        break;

#if HAVE_T38
    case SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA:
      {
        PTRACE(2, "mod_opal\tRequesting switch to T.38");
        PSafePtr<OpalConnection> other = GetOtherPartyConnection();
        if (other != NULL && other->SwitchT38(true))
            switch_channel_set_flag(m_fsChannel, CF_REQ_MEDIA);
        else {
            PTRACE(1, "mod_opal\tMode change request to T.38 failed");
        }
        break;
      }

    case SWITCH_MESSAGE_INDICATE_T38_DESCRIPTION:
        PTRACE(2, "mod_opal\tSWITCH_MESSAGE_INDICATE_T38_DESCRIPTION");
        break;

    case SWITCH_MESSAGE_INDICATE_UDPTL_MODE:
        PTRACE(2, "mod_opal\tSWITCH_MESSAGE_INDICATE_UDPTL_MODE");
        m_udptl = true;
        break;
#endif // HAVE_T38

    default:
        PTRACE(3, "mod_opal\tReceived unhandled message " << msg->message_id << " on connection " << *this);
    }

    return SWITCH_STATUS_SUCCESS;
}


bool FSConnection::WaitForMedia()
{
    PTRACE(4, "mod_opal\tAwaiting media start on connection " << *this);
    m_rxAudioOpened.Wait();
    m_txAudioOpened.Wait();

    if (IsReleased()) {
        // Call got aborted
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_fsSession), SWITCH_LOG_ERROR, "Call abandoned!\n");
        return false;
    }

    PTRACE(3, "mod_opal\tMedia started on connection " << *this);
    return true;
}


#if HAVE_T38
void FSConnection::SetT38OptionsFromMediaFormat(const OpalMediaFormat & mediaFormat, const char * varname)
{
    switch_t38_options_t *t38_options = (switch_t38_options_t *)switch_channel_get_private(m_fsChannel, varname);
    if (t38_options == NULL)
      t38_options = (switch_t38_options_t *)switch_core_session_alloc(m_fsSession, sizeof(switch_t38_options_t));

    PString value;
    mediaFormat.GetOptionValue("T38FaxRateManagement", value);
    t38_options->T38FaxRateManagement = switch_core_session_strdup(m_fsSession, value);

    mediaFormat.GetOptionValue("T38FaxUdpEC", value);
    t38_options->T38FaxUdpEC = switch_core_session_strdup(m_fsSession, value);

    t38_options->T38MaxBitRate = mediaFormat.GetOptionInteger("T38MaxBitRate", 9600);
    t38_options->T38FaxMaxBuffer = mediaFormat.GetOptionInteger("T38FaxMaxBuffer", 2000);
    t38_options->T38FaxMaxDatagram = mediaFormat.GetOptionInteger("T38FaxMaxDatagram", 528);

    t38_options->T38FaxFillBitRemoval = mediaFormat.GetOptionBoolean("T38FaxFillBitRemoval") ? SWITCH_TRUE : SWITCH_FALSE;
    t38_options->T38FaxTranscodingMMR = mediaFormat.GetOptionBoolean("T38FaxTranscodingMMR") ? SWITCH_TRUE : SWITCH_FALSE;
    t38_options->T38FaxTranscodingJBIG = mediaFormat.GetOptionBoolean("T38FaxTranscodingJBIG") ? SWITCH_TRUE : SWITCH_FALSE;

    t38_options->T38VendorInfo = switch_core_session_strdup(m_fsSession, mediaFormat.GetOptionString("T38VendorInfo"));

    //t38_options->remote_ip = switch_core_session_strdup(session, mediaFormat.something);
    //t38_options->remote_port = mediaFormat.something;

    switch_channel_set_private(m_fsChannel, varname, t38_options);
    PTRACE(3, "mod_opal\tSet " << varname);
}


void FSConnection::OnSwitchedT38(bool toT38, bool success)
{
    if (toT38 && success && IndicateSwitchedT38()) {
        PTRACE(3, "mod_opal\tMode change request to T.38 succeeded");
    }
    else {
        AbortT38();
    }
}


void FSConnection::OnSwitchingT38(bool toT38)
{
    if (toT38 && IndicateSwitchedT38()) {
        PTRACE(3, "mod_opal\tMode change request to T.38 started");
    }
    else {
      AbortT38();
    }
}


void FSConnection::AbortT38()
{
    PTRACE(3, "mod_opal\tMode change request to T.38 failed");
    switch_channel_set_private(m_fsChannel, "t38_options", NULL);
    switch_channel_clear_app_flag_key("T38", m_fsChannel, CF_APP_T38);
    switch_channel_clear_app_flag_key("T38", m_fsChannel, CF_APP_T38_REQ);
    switch_channel_set_app_flag_key("T38", m_fsChannel, CF_APP_T38_FAIL);
}


bool FSConnection::IndicateSwitchedT38()
{
    PSafePtr<OpalConnection> other = GetOtherPartyConnection();
    if (other == NULL) {
        PTRACE(3, "mod_opal\tCan't change to T.38, no other connection");
        return false;
    }

    OpalMediaFormatList otherFormats = other->GetMediaFormats();
    OpalMediaFormatList::const_iterator t38 = otherFormats.FindFormat(OpalT38);
    if (t38 == otherFormats.end()) {
        PTRACE(3, "mod_opal\tCan't change to T.38, no remote capability");
        return false;
    }

    SetT38OptionsFromMediaFormat(*t38, "t38_options");

    switch_channel_set_variable(m_fsChannel, "has_t38", "true");
    switch_channel_set_app_flag_key("T38", m_fsChannel, CF_APP_T38);

    switch_channel_execute_on(m_fsChannel, "opal_execute_on_t38");
    switch_channel_api_on(m_fsChannel, "opal_api_on_t38");
    return true;
}
#endif // HAVE_T38


switch_status_t FSConnection::receive_event(switch_event_t *event)
{
    PTRACE(4, "mod_opal\tReceived event " << event->event_id << " on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::state_change()
{
    PTRACE(4, "mod_opal\tState changed on connection " << *this);
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::read_audio_frame(switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
    return read_frame(m_udptl ? OpalMediaType::Fax() : OpalMediaType::Audio(), frame, flags);
}


switch_status_t FSConnection::write_audio_frame(switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
    return write_frame(m_udptl ? OpalMediaType::Fax() : OpalMediaType::Audio(), frame, flags);
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
    if (!ownerCall.IsSwitchingT38()) {
        PSafePtr <FSMediaStream> stream = PSafePtrCast <OpalMediaStream, FSMediaStream>(GetMediaStream(mediaType, false));
        if (stream != NULL)
            return stream->read_frame(frame, flags);

        PTRACE(2, "mod_opal\tNo stream for read of " << mediaType);
    }

    // Avoid all the channel closing and re-opening, especially with faxa switching, upsetting FS
    *frame = &m_dummy_frame;
    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSConnection::write_frame(const OpalMediaType & mediaType, const switch_frame_t *frame, switch_io_flag_t flags)
{
    // Avoid all the channel closing and re-opening, especially with faxa switching, upsetting FS
    if (ownerCall.IsSwitchingT38())
      return SWITCH_STATUS_SUCCESS;

    PSafePtr <FSMediaStream> stream = PSafePtrCast<OpalMediaStream, FSMediaStream>(GetMediaStream(mediaType, true));
    if (stream != NULL)
      return stream->write_frame(frame, flags);

    PTRACE(2, "mod_opal\tNo stream for write of " << mediaType);
    return SWITCH_STATUS_SUCCESS;
}


///////////////////////////////////////////////////////////////////////

FSMediaStream::FSMediaStream(FSConnection & conn, const OpalMediaFormat & mediaFormat, unsigned sessionID, bool isSource)
    : OpalMediaStream(conn, mediaFormat, sessionID, isSource)
    , m_connection(conn)
    , m_switchTimer(NULL)
    , m_switchCodec(NULL)
    , m_readRTP(0, SWITCH_RECOMMENDED_BUFFER_SIZE)
{
    memset(&m_readFrame, 0, sizeof(m_readFrame));
}


PBoolean FSMediaStream::Open()
{
    if (IsOpen()) {
        return true;
    }

    switch_core_session_t *fsSession = m_connection.GetSession();
    switch_channel_t *fsChannel = m_connection.GetChannel();
    if (PAssertNULL(fsSession) == NULL || PAssertNULL(fsChannel) == NULL)
        return false;

    bool isAudio;
    OpalMediaType mediaType = mediaFormat.GetMediaType();
    if (mediaType == OpalMediaType::Audio())
        isAudio = true;
    else if (mediaType == OpalMediaType::Video())
        isAudio = false;
#if HAVE_T38
    else if (mediaType == OpalMediaType::Fax()) {
        m_readFrame.flags = SFF_UDPTL_PACKET|SFF_PROXY_PACKET;
        return OpalMediaStream::Open();
    }
#endif
    else {
        PTRACE(1, "mod_opal\tUnsupported media type: " << mediaType);
        return false;
    }

    int ptime = mediaFormat.GetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption()) * mediaFormat.GetFrameTime() / mediaFormat.GetTimeUnits();

    if (IsSink()) {
        m_switchCodec = isAudio ? &m_connection.m_read_codec : &m_connection.m_vid_read_codec;
        m_switchTimer = isAudio ? &m_connection.m_read_timer : &m_connection.m_vid_read_timer;
        m_readFrame.codec = m_switchCodec;
        m_readFrame.rate = mediaFormat.GetClockRate();
    } else {
        m_switchCodec = isAudio ? &m_connection.m_write_codec : &m_connection.m_vid_write_codec;
    }

    // The following is performed on two different instances of this object.
    if (switch_core_codec_init(m_switchCodec, mediaFormat.GetEncodingName(), NULL, // FMTP
                               mediaFormat.GetClockRate(), ptime, 1,  // Channels
                               SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,   // Settings
                               switch_core_session_get_pool(fsSession)) != SWITCH_STATUS_SUCCESS) {
        // Could not select a codecs using negotiated frames/packet, so try using default.
        if (switch_core_codec_init(m_switchCodec, mediaFormat.GetEncodingName(), NULL, // FMTP
                                   mediaFormat.GetClockRate(), 0, 1,  // Channels
                                   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,   // Settings
                                   switch_core_session_get_pool(fsSession)) != SWITCH_STATUS_SUCCESS) {
            PTRACE(1, "mod_opal\t" << switch_channel_get_name(fsChannel)
                   << " cannot initialise " << (IsSink()? "read" : "write") << ' '
                   << mediaType << " codec " << mediaFormat << " for connection " << *this);
            switch_channel_hangup(fsChannel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
            return false;
        }
        PTRACE(2, "mod_opal\t" << switch_channel_get_name(fsChannel)
               << " unsupported ptime of " << ptime << " on " << (IsSink()? "read" : "write") << ' '
               << mediaType << " codec " << mediaFormat << " for connection " << *this);
    }

    if (IsSink()) {
        if (isAudio) {
            switch_core_session_set_read_codec(fsSession, m_switchCodec);
            if (switch_core_timer_init(m_switchTimer,
                                       "soft",
                                       m_switchCodec->implementation->microseconds_per_packet / 1000,
                                       m_switchCodec->implementation->samples_per_packet,
                                       switch_core_session_get_pool(fsSession)) != SWITCH_STATUS_SUCCESS) {
                PTRACE(1, "mod_opal\t" << switch_channel_get_name(fsChannel)
                       << " timer init failed on " << (IsSink()? "read" : "write") << ' '
                       << mediaType << " codec " << mediaFormat << " for connection " << *this);
                switch_core_codec_destroy(m_switchCodec);
                m_switchCodec = NULL;
                return false;
            }
        } else {
            switch_core_session_set_video_read_codec(fsSession, m_switchCodec);
            switch_channel_set_flag(fsChannel, CF_VIDEO);
        }
    } else {
        if (isAudio) {
            switch_core_session_set_write_codec(fsSession, m_switchCodec);
        } else {
            switch_core_session_set_video_write_codec(fsSession, m_switchCodec);
            switch_channel_set_flag(fsChannel, CF_VIDEO);
        }
    }

    PTRACE(3, "mod_opal\t" << switch_channel_get_name(fsChannel)
           << " initialised " << (IsSink()? "read" : "write") << ' '
           << mediaType << " codec " << mediaFormat << " for connection " << *this);

    return OpalMediaStream::Open();
}


void FSMediaStream::InternalClose()
{
}


PBoolean FSMediaStream::IsSynchronous() const
{
    return true;
}


PBoolean FSMediaStream::RequiresPatchThread(OpalMediaStream *) const
{
    return false;
}


int FSMediaStream::StartReadWrite(PatchPtr & mediaPatch) const
{
    if (!IsOpen()) {
        PTRACE(1, "mod_opal\tNot open!");
        return -1;
    }

    if (!m_connection.IsChannelReady()) {
        PTRACE(1, "mod_opal\tChannel not ready!");
        return -1;
    }

    // We make referenced copy of pointer so can't be deleted out from under us
    mediaPatch = m_mediaPatch;
    if (mediaPatch == NULL) {
        /*There is a race here... sometimes we make it here and m_mediaPatch is NULL
          if we wait it shows up in 1ms, maybe there is a better way to wait. */
        PTRACE(2, "mod_opal\tPatch not ready!");
        return 1;
    }

    return 0;
}


switch_status_t FSMediaStream::read_frame(switch_frame_t **frame, switch_io_flag_t flags)
{
    *frame = &m_readFrame;
    m_readFrame.flags |= SFF_CNG;

    PatchPtr mediaPatch;
    switch (StartReadWrite(mediaPatch)) {
      case -1 :
        return SWITCH_STATUS_FALSE;
      case 1 :
        return SWITCH_STATUS_SUCCESS;
    }

    if (m_connection.NeedFlushAudio()) {
        mediaPatch->GetSource().EnableJitterBuffer(); // This flushes data and resets jitter buffer
        m_readRTP.SetPayloadSize(0);
    } else {
        if (m_switchCodec != NULL) {
            m_readRTP.SetTimestamp(m_readFrame.timestamp + m_switchCodec->implementation->samples_per_packet);
        }

        if (!mediaPatch->GetSource().ReadPacket(m_readRTP)) {
            PTRACE(1, "mod_opal\tread_frame: no source data!");
            return SWITCH_STATUS_FALSE;
        }
    }

    if (m_switchTimer != NULL) {
        switch_core_timer_next(m_switchTimer);
    }

    if (m_switchCodec != NULL) {
        if (!switch_core_codec_ready(m_switchCodec)) {
            PTRACE(1, "mod_opal\tread_frame: codec not ready!");
            return SWITCH_STATUS_FALSE;
        }
    }

    if (switch_test_flag(&m_readFrame, SFF_UDPTL_PACKET)) {
        m_readFrame.flags    &= ~SFF_CNG;
        m_readFrame.packet    = m_readRTP.GetPayloadPtr();
        m_readFrame.packetlen = m_readRTP.GetPayloadSize();
        return SWITCH_STATUS_SUCCESS;
    }

    if (switch_test_flag(&m_readFrame, SFF_RAW_RTP)) {
        m_readFrame.flags    &= ~SFF_CNG;
        m_readFrame.packet    = m_readRTP.GetPointer();
        m_readFrame.packetlen = m_readRTP.GetHeaderSize() + m_readRTP.GetPayloadSize();
        return SWITCH_STATUS_SUCCESS;
    }

#if IMPLEMENT_MULTI_FAME_AUDIO
    // Repackage frames in incoming packet to agree with what FS expects.
    // Not implmented yet!!!!!!!!!
    // Cheating and only supporting one frame per packet
#endif

    m_readFrame.buflen    = m_readRTP.GetSize();
    m_readFrame.data      = m_readRTP.GetPayloadPtr();
    m_readFrame.datalen   = m_readRTP.GetPayloadSize();
    m_readFrame.timestamp = m_readRTP.GetTimestamp();
    m_readFrame.seq       = m_readRTP.GetSequenceNumber();
    m_readFrame.ssrc      = m_readRTP.GetSyncSource();
    m_readFrame.m         = m_readRTP.GetMarker() ? SWITCH_TRUE : SWITCH_FALSE;
    m_readFrame.payload   = (switch_payload_t)m_readRTP.GetPayloadType();

    if (m_readFrame.datalen > 0 &&
        m_readFrame.payload != RTP_DataFrame::CN &&
        m_readFrame.payload != RTP_DataFrame::Cisco_CN) {
        m_readFrame.flags &= ~SFF_CNG;
    }

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t FSMediaStream::write_frame(const switch_frame_t *frame, switch_io_flag_t flags)
{
    PatchPtr mediaPatch;
    switch (StartReadWrite(mediaPatch)) {
      case -1 :
        return SWITCH_STATUS_FALSE;
      case 1 :
        return SWITCH_STATUS_SUCCESS;
    }

    RTP_DataFrame rtp;
    if (switch_test_flag(frame, SFF_RAW_RTP)) {
        rtp = RTP_DataFrame((const BYTE *)frame->packet, frame->packetlen, false);
    }
    else if (switch_test_flag(frame, SFF_UDPTL_PACKET)) {
        rtp.SetPayloadSize(frame->packetlen);
        memcpy(rtp.GetPayloadPtr(), frame->packet, frame->packetlen);
    }
    else {
        rtp.SetPayloadSize(frame->datalen);
        memcpy(rtp.GetPayloadPtr(), frame->data, frame->datalen);

        rtp.SetPayloadType(mediaFormat.GetPayloadType());

        /* Not sure what FS is going to give us!
           Suspect it depends on the mod on the other side sending it. */
        if (frame->timestamp != 0)
            timestamp = frame->timestamp;
        else if (frame->samples != 0)
            timestamp += frame->samples;
        else if (m_switchCodec != NULL)
            timestamp += m_switchCodec->implementation->samples_per_packet;
        rtp.SetTimestamp(timestamp);
    }

    if (mediaPatch->PushFrame(rtp))
      return SWITCH_STATUS_SUCCESS;

    PTRACE(1, "mod_opal\tread_frame: push failed!");
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:s:
 */
