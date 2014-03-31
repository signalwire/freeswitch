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


#ifndef __FREESWITCH_MOD_OPAL__
#define __FREESWITCH_MOD_OPAL__

#if defined(__GNUC__) && defined(HAVE_VISIBILITY)
#pragma GCC visibility push(default)
#endif

#include <ptlib.h>
#include <opal/manager.h>

#ifndef OPAL_CHECK_VERSION
  #define OPAL_CHECK_VERSION(a,b,c) 0
#endif

#if !OPAL_CHECK_VERSION(3,12,8)
  #error OPAL is too old to use, must be >= 2.12.8
#endif

#include <ep/localep.h>
#include <h323/h323ep.h>
#include <iax2/iax2ep.h>

#if defined(__GNUC__) && defined(HAVE_VISIBILITY)
#pragma GCC visibility pop
#endif

#undef strcasecmp
#undef strncasecmp

#include <switch.h>

#define MODNAME "mod_opal"

#define HAVE_T38 OPAL_T38_CAPABILITY



class FSEndPoint;
class FSManager;


class FSProcess : public PLibraryProcess
{
    PCLASSINFO(FSProcess, PLibraryProcess);
  public:
    FSProcess();
    ~FSProcess();

    bool Initialise(switch_loadable_module_interface_t *iface);

    FSManager & GetManager() const
    {
        return *m_manager;
    }

  protected:
    FSManager * m_manager;
};


struct FSListener
{
    FSListener() : m_port(H323EndPoint::DefaultTcpSignalPort) { }

    PString            m_name;
    PIPSocket::Address m_address;
    uint16_t           m_port;
};


class FSManager : public OpalManager
{
    PCLASSINFO(FSManager, OpalManager);

  public:
    FSManager();

    bool Initialise(switch_loadable_module_interface_t *iface);

    switch_status_t ReadConfig(int reload);

    switch_endpoint_interface_t *GetSwitchInterface() const { return m_FreeSwitch; }
    const PString & GetContext() const { return m_context; }
    const PString & GetDialPlan() const { return m_dialplan; }
    const PString & GetCodecPrefs() const { return m_codecPrefs; }
    bool GetDisableTranscoding() const { return m_disableTranscoding; }

  private:
    switch_endpoint_interface_t *m_FreeSwitch;

    H323EndPoint *m_h323ep;
    IAX2EndPoint *m_iaxep;
    FSEndPoint   *m_fsep;

    PString m_context;
    PString m_dialplan;
    PString m_codecPrefs;
    bool    m_disableTranscoding;
    PString m_gkAddress;
    PString m_gkIdentifer;
    PString m_gkInterface;

    list <FSListener> m_listeners;
};


class FSEndPoint : public OpalLocalEndPoint
{
    PCLASSINFO(FSEndPoint, OpalLocalEndPoint);
  public:
    FSEndPoint(FSManager & manager);

    virtual OpalLocalConnection *CreateConnection(OpalCall & call, void *userData, unsigned options, OpalConnection::StringOptions * stringOptions);

    FSManager & GetManager() const { return m_manager; }

  protected:
    FSManager & m_manager;
};


class FSConnection;


class FSMediaStream : public OpalMediaStream
{
    PCLASSINFO(FSMediaStream, OpalMediaStream);
  public:
    FSMediaStream(
      FSConnection & conn,
      const OpalMediaFormat & mediaFormat,    ///<  Media format for stream
      unsigned sessionID,    ///<  Session number for stream
      bool isSource    ///<  Is a source stream
    );

    virtual PBoolean Open();
    virtual PBoolean IsSynchronous() const;
    virtual PBoolean RequiresPatchThread(OpalMediaStream *) const;

    switch_status_t read_frame(switch_frame_t **frame, switch_io_flag_t flags);
    switch_status_t write_frame(const switch_frame_t *frame, switch_io_flag_t flags);

  protected:
    virtual void InternalClose();
    int StartReadWrite(PatchPtr & mediaPatch) const;

  private:
    bool CheckPatchAndLock();

    FSConnection          &m_connection;
    switch_timer_t        *m_switchTimer;
    switch_codec_t        *m_switchCodec;
    switch_frame_t         m_readFrame;
    RTP_DataFrame          m_readRTP;
};


#define DECLARE_CALLBACK0(name)                           \
    static switch_status_t name(switch_core_session_t *session) {       \
        FSConnection *tech_pvt = (FSConnection *) switch_core_session_get_private(session); \
        return tech_pvt != NULL ? tech_pvt->name() : SWITCH_STATUS_FALSE; } \
    switch_status_t name()

#define DECLARE_CALLBACK1(name, type1, name1)                           \
    static switch_status_t name(switch_core_session_t *session, type1 name1) { \
        FSConnection *tech_pvt = (FSConnection *) switch_core_session_get_private(session); \
        return tech_pvt != NULL ? tech_pvt->name(name1) : SWITCH_STATUS_FALSE; } \
    switch_status_t name(type1 name1)

#define DECLARE_CALLBACK3(name, type1, name1, type2, name2, type3, name3) \
    static switch_status_t name(switch_core_session_t *session, type1 name1, type2 name2, type3 name3) { \
        FSConnection *tech_pvt = (FSConnection *) switch_core_session_get_private(session); \
        return tech_pvt != NULL ? tech_pvt->name(name1, name2, name3) : SWITCH_STATUS_FALSE; } \
    switch_status_t name(type1 name1, type2 name2, type3 name3)




class FSConnection : public OpalLocalConnection
{
    PCLASSINFO(FSConnection, OpalLocalConnection)

  public:
    struct outgoing_params {
      switch_event_t          *var_event;
      switch_caller_profile_t *outbound_profile;
      switch_core_session_t  **new_session;
      switch_memory_pool_t   **pool;
      switch_originate_flag_t  flags;
      switch_call_cause_t     *cancel_cause;
      switch_call_cause_t      fail_cause;
    };

    FSConnection(OpalCall & call,
                 FSEndPoint & endpoint,
                 unsigned options,
                 OpalConnection::StringOptions * stringOptions,
                 outgoing_params * params);

    virtual bool OnOutgoingSetUp();
    virtual bool OnIncoming();
    virtual void OnEstablished();
    virtual void OnReleased();
    virtual PBoolean SetAlerting(const PString & calleeName, PBoolean withMedia);
    virtual OpalMediaStream *CreateMediaStream(const OpalMediaFormat &, unsigned, PBoolean);
    virtual void OnPatchMediaStream(PBoolean isSource, OpalMediaPatch & patch);
    virtual OpalMediaFormatList GetMediaFormats() const;
    virtual PBoolean SendUserInputTone(char tone, unsigned duration);
#if HAVE_T38
    virtual void OnSwitchedT38(bool toT38, bool success);
    virtual void OnSwitchingT38(bool toT38);
#endif

    DECLARE_CALLBACK0(on_init);
    DECLARE_CALLBACK0(on_destroy);
    DECLARE_CALLBACK0(on_routing);
    DECLARE_CALLBACK0(on_execute);
    DECLARE_CALLBACK0(on_hangup);

    DECLARE_CALLBACK0(on_exchange_media);
    DECLARE_CALLBACK0(on_soft_execute);

    DECLARE_CALLBACK1(kill_channel, int, sig);
    DECLARE_CALLBACK1(send_dtmf, const switch_dtmf_t *, dtmf);
    DECLARE_CALLBACK1(receive_message, switch_core_session_message_t *, msg);
    DECLARE_CALLBACK1(receive_event, switch_event_t *, event);
    DECLARE_CALLBACK0(state_change);
    DECLARE_CALLBACK3(read_audio_frame, switch_frame_t **, frame, switch_io_flag_t, flags, int, stream_id);
    DECLARE_CALLBACK3(write_audio_frame, switch_frame_t *, frame, switch_io_flag_t, flags, int, stream_id);
    DECLARE_CALLBACK3(read_video_frame, switch_frame_t **, frame, switch_io_flag_t, flag, int, stream_id);
    DECLARE_CALLBACK3(write_video_frame, switch_frame_t *, frame, switch_io_flag_t, flag, int, stream_id);

    __inline switch_core_session_t *GetSession() const
    {
        return m_fsSession;
    }

    __inline switch_channel_t *GetChannel() const
    {
        return m_fsChannel;
    }

    bool IsChannelReady() const
    {
        return m_fsChannel != NULL && switch_channel_ready(m_fsChannel);
    }

    bool NeedFlushAudio()
    {
        if (!m_flushAudio)
          return false;
        m_flushAudio = false;
        return true;
    }

  protected:
    void SetCodecs();
    bool WaitForMedia();
#if HAVE_T38
    void SetT38OptionsFromMediaFormat(const OpalMediaFormat & mediaFormat, const char * varname);
    bool IndicateSwitchedT38();
    void AbortT38();
#endif

    switch_status_t read_frame(const OpalMediaType & mediaType, switch_frame_t **frame, switch_io_flag_t flags);
    switch_status_t write_frame(const OpalMediaType & mediaType, const switch_frame_t *frame, switch_io_flag_t flags);

  private:
    FSEndPoint            &m_endpoint;
    switch_core_session_t *m_fsSession;
    switch_channel_t      *m_fsChannel;
    PSyncPoint             m_rxAudioOpened;
    PSyncPoint             m_txAudioOpened;
    OpalMediaFormatList    m_switchMediaFormats;

    // If FS ever supports more than one audio and one video, this needs to change
    switch_timer_t m_read_timer;
    switch_codec_t m_read_codec;
    switch_codec_t m_write_codec;

    switch_timer_t m_vid_read_timer;
    switch_codec_t m_vid_read_codec;
    switch_codec_t m_vid_write_codec;

    switch_frame_t m_dummy_frame;

    bool m_flushAudio;
    bool m_udptl;

    friend PBoolean FSMediaStream::Open();
};


#endif /* __FREESWITCH_MOD_OPAL__ */

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
