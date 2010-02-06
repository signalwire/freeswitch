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


#ifndef __FREESWITCH_MOD_OPAL__
#define __FREESWITCH_MOD_OPAL__

#if defined(__GNUC__) && defined(HAVE_VISIBILITY)
#pragma GCC visibility push(default)
#endif

#include <ptlib.h>
#include <opal/manager.h>
#include <opal/localep.h>
#include <h323/h323ep.h>
#include <iax2/iax2ep.h>

#if defined(__GNUC__) && defined(HAVE_VISIBILITY)
#pragma GCC visibility pop
#endif

#undef strcasecmp
#undef strncasecmp

#define HAVE_APR
#include <switch.h>
#include <switch_version.h>
#define MODNAME "mod_opal"


class FSEndPoint;
class FSManager;


struct mod_opal_globals {
	int trace_level;
	char *codec_string;
	char *context;
	char *dialplan;
};

extern struct mod_opal_globals mod_opal_globals;


class FSProcess:public PLibraryProcess {
	PCLASSINFO(FSProcess, PLibraryProcess);

  public:
	FSProcess();
	~FSProcess();

	bool Initialise(switch_loadable_module_interface_t *iface);

	     FSManager & GetManager() const {
		return *m_manager;
  } protected:
	      FSManager * m_manager;
};


struct FSListener {
	FSListener() {
	} PString name;
	OpalTransportAddress listenAddress;
	PString localUserName;
	PString gatekeeper;
};


class FSCall:public OpalCall {
	PCLASSINFO(FSCall, OpalCall);
  public:
	FSCall(OpalManager & manager);
	virtual PBoolean OnSetUp(OpalConnection & connection);
};


class FSManager:public OpalManager {
	PCLASSINFO(FSManager, OpalManager);

  public:
	FSManager();

	bool Initialise(switch_loadable_module_interface_t *iface);

	switch_status_t ReadConfig(int reload);

	switch_endpoint_interface_t *GetSwitchInterface() const {
		return m_FreeSwitch;
	} virtual OpalCall *CreateCall(void *userData);

  private:
	switch_endpoint_interface_t *m_FreeSwitch;

	H323EndPoint *m_h323ep;
	IAX2EndPoint *m_iaxep;
	FSEndPoint *m_fsep;

	PString m_gkAddress;
	PString m_gkIdentifer;
	PString m_gkInterface;

	        list < FSListener > m_listeners;
};


class FSConnection;
typedef struct {
	switch_timer_t read_timer;
	switch_codec_t read_codec;
	switch_codec_t write_codec;

	switch_timer_t vid_read_timer;
	switch_codec_t vid_read_codec;
	switch_codec_t vid_write_codec;
	FSConnection *me;
} opal_private_t;


class FSEndPoint:public OpalLocalEndPoint {
	PCLASSINFO(FSEndPoint, OpalLocalEndPoint);
  public:
	FSEndPoint(FSManager & manager);

	virtual bool OnIncomingCall(OpalLocalConnection &);
	virtual OpalLocalConnection *CreateConnection(OpalCall & call, void *userData, unsigned options, OpalConnection::StringOptions * stringOptions);
};


#define DECLARE_CALLBACK0(name)                           \
    static switch_status_t name(switch_core_session_t *session) {       \
        opal_private_t *tech_pvt = (opal_private_t *) switch_core_session_get_private(session); \
        return tech_pvt && tech_pvt->me != NULL ? tech_pvt->me->name() : SWITCH_STATUS_FALSE; } \
switch_status_t name()

#define DECLARE_CALLBACK1(name, type1, name1)                           \
    static switch_status_t name(switch_core_session_t *session, type1 name1) { \
        opal_private_t *tech_pvt = (opal_private_t *) switch_core_session_get_private(session); \
        return tech_pvt && tech_pvt->me != NULL ? tech_pvt->me->name(name1) : SWITCH_STATUS_FALSE; } \
switch_status_t name(type1 name1)

#define DECLARE_CALLBACK3(name, type1, name1, type2, name2, type3, name3) \
    static switch_status_t name(switch_core_session_t *session, type1 name1, type2 name2, type3 name3) { \
        opal_private_t *tech_pvt = (opal_private_t *) switch_core_session_get_private(session); \
        return tech_pvt && tech_pvt->me != NULL ? tech_pvt->me->name(name1, name2, name3) : SWITCH_STATUS_FALSE; } \
switch_status_t name(type1 name1, type2 name2, type3 name3)




class FSConnection:public OpalLocalConnection {
	PCLASSINFO(FSConnection, OpalLocalConnection)

  public:
	FSConnection(OpalCall & call,
				 FSEndPoint & endpoint,
				 void *userData,
				 unsigned options,
				 OpalConnection::StringOptions * stringOptions,
				 switch_caller_profile_t *outbound_profile, switch_core_session_t *fsSession, switch_channel_t *fsChannel);

	virtual bool OnIncoming();
	virtual void OnReleased();
	virtual PBoolean SetAlerting(const PString & calleeName, PBoolean withMedia);
	virtual void OnAlerting();
	virtual void OnEstablished();
	virtual OpalMediaStream *CreateMediaStream(const OpalMediaFormat &, unsigned, PBoolean);
	virtual PBoolean OnOpenMediaStream(OpalMediaStream & stream);
	virtual OpalMediaFormatList GetMediaFormats() const;
	virtual PBoolean SendUserInputTone(char tone, unsigned duration);
	virtual PBoolean SendUserInputString(const PString & value);

	void SetCodecs();

	     DECLARE_CALLBACK0(on_init);
	     DECLARE_CALLBACK0(on_routing);
	     DECLARE_CALLBACK0(on_execute);

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

	switch_status_t read_frame(const OpalMediaType & mediaType, switch_frame_t **frame, switch_io_flag_t flags);
	switch_status_t write_frame(const OpalMediaType & mediaType, const switch_frame_t *frame, switch_io_flag_t flags);

	switch_core_session_t *GetSession() const {
		return m_fsSession;
  } private:
	      FSEndPoint & m_endpoint;
	switch_core_session_t *m_fsSession;
	switch_channel_t *m_fsChannel;
	PSyncPoint m_rxAudioOpened;
	PSyncPoint m_txAudioOpened;
	OpalMediaFormatList m_switchMediaFormats;
};


class FSMediaStream:public OpalMediaStream {
	PCLASSINFO(FSMediaStream, OpalMediaStream);
  public:
	FSMediaStream(FSConnection & conn, const OpalMediaFormat & mediaFormat,	///<  Media format for stream
				  unsigned sessionID,	///<  Session number for stream
				  bool isSource	///<  Is a source stream
		);

	virtual PBoolean Open();
	virtual PBoolean Close();
	virtual PBoolean IsSynchronous() const;
	virtual PBoolean RequiresPatchThread(OpalMediaStream *) const;

	switch_status_t read_frame(switch_frame_t **frame, switch_io_flag_t flags);
	switch_status_t write_frame(const switch_frame_t *frame, switch_io_flag_t flags);

  private:
	switch_core_session_t *m_fsSession;
	switch_channel_t *m_fsChannel;
	switch_timer_t *m_switchTimer;
	switch_codec_t *m_switchCodec;
	switch_frame_t m_readFrame;
	unsigned char m_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	RTP_DataFrame m_readRTP;
	bool m_callOnStart;
	uint32_t m_timeStamp;

	bool CheckPatchAndLock();
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:s:
 */
