
#ifndef __FREESWITCH_OPAL_CONNECTION__
#define __FREESWITCH_OPAL_CONNECTION__

#include <switch.h>
#include <ptlib.h>
#include <ptlib/psync.h>
#include <opal/buildopts.h>
#include <opal/endpoint.h>
#include <opal/connection.h>
#include <opal/transports.h>

#include "fsep.h"
#include "fsmediastream.h"

typedef enum {
    TFLAG_IO = (1 << 0),
    TFLAG_INBOUND = (1 << 1),
    TFLAG_OUTBOUND = (1 << 2),
    TFLAG_READING = (1 << 3),
	TFLAG_WRITING = (1 << 4)
} TFLAGS;


class FSConnection;
class FSMediaStream;

typedef struct fs_obj
{
	unsigned int 				flags;
	switch_mutex_t 				*flag_mutex;
    OpalCall					*ownercall;
	FSConnection				*Connection;
    
} fs_obj_t; 



class FSConnection : public OpalConnection
{
	PCLASSINFO(FSConnection, OpalConnection);
	public:
		FSConnection(OpalCall & call, FSEndPoint & endpoint, const PString & token, unsigned int options);
		~FSConnection();
		virtual OpalMediaFormatList GetMediaFormats() const;
		virtual OpalMediaStream * CreateMediaStream(OpalConnection & conn,
													const OpalMediaFormat & mediaFormat,
													BOOL isSource,
													RTP_Session &rtpSession,
													unsigned minAudioJitterDelay, 
													unsigned maxAudioJitterDelay);
		virtual OpalMediaStream* CreateMediaStream  (const OpalMediaFormat & mediaFormat,
													 unsigned  	  sessionID,
													 BOOL  	  isSource);

		void InitiateCall(const PString & party);
	
		
		
		virtual BOOL SetUpConnection();
		virtual BOOL OnSetUpConnection();
		virtual BOOL SetAlerting(const PString& caleeName, BOOL withMedia);
		virtual BOOL SetConnected();
		virtual void OnAlerting(OpalConnection & connection);
		virtual void OnConnected();
		virtual void OnReleased();
		virtual BOOL IsMediaBypassPossible(unsigned sessionID);
		
		switch_status_t callback_on_init(switch_core_session_t *io_session);
	    switch_status_t callback_on_ring(switch_core_session_t *io_session);
	    switch_status_t callback_on_execute(switch_core_session_t *io_session);
	    switch_status_t callback_on_hangup(switch_core_session_t *io_session);
	    switch_status_t callback_on_loopback(switch_core_session_t *session);
	    switch_status_t callback_on_transmit(switch_core_session_t *io_session);
    
		switch_call_cause_t SwitchInitiateCall(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
						   					   switch_core_session_t **new_session, switch_memory_pool_t **pool);
	
	    switch_status_t io_read_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
	    switch_status_t io_write_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);
	    switch_status_t io_kill_channel(switch_core_session_t *, int);
	    switch_status_t io_waitfor_read(switch_core_session_t *, int, int);
	    switch_status_t io_waitfor_write(switch_core_session_t *, int, int);
	    switch_status_t io_send_dtmf(switch_core_session_t *, char *);
	    switch_status_t io_receive_message(switch_core_session_t *, switch_core_session_message_t *);
	    switch_status_t io_receive_event(switch_core_session_t *, switch_event_t *);
	    switch_status_t io_state_change(switch_core_session_t *);
	    switch_status_t io_read_video_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
	    switch_status_t io_write_video_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);
	
	private:
		OpalMediaFormatList mediaformats;
	
	
		switch_frame_t frame;
		char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	    switch_codec_t 			read_codec;   /* Read codec*/
    	switch_codec_t 			write_codec;  /* Write codec*/
		switch_timer_t 			timer;
		
		
	protected:
		
		BOOL CreateIncomingFSConnection();
		
		switch_endpoint_interface_t		*FSEndpointInterface;
		WORD				RTPLocalPort;
		WORD				RTPRemotePort;
		PIPSocket::Address	RTPLocalAddress;
		PIPSocket::Address	RTPRemoteAddress;
		
		PMutex				ChannelMutex;
	
		RTP_SessionManager *rtpmanager;
	
		RTP_Session			*OpalRTPSession;
	
		switch_core_session_t *fssession;
	
		
		switch_rtp_t  *FSRTPSession;
};


#endif //__FREESWITCH_OPAL_CONNECTION__

