
#include "fsep.h"
#include "fscon.h"
#include "fsmediastream.h"

extern switch_endpoint_interface_t *opalfs_endpoint_interface;





FSConnection::FSConnection(OpalCall & call, FSEndPoint & endpoint, const PString & token, unsigned int options)
	:OpalConnection(call, endpoint, token, options), FSEndpointInterface(NULL), FSRTPSession(NULL), OpalRTPSession(NULL)

{
	
	rtpmanager = new RTP_SessionManager();
	RTPLocalPort = endpoint.GetMediaPort();       // source is opal
	RTPRemotePort = endpoint.GetMediaPort() + 150; // destination is Freeswitch
	RTPLocalAddress = endpoint.GetRTPAddress();
	RTPRemoteAddress = endpoint.GetRTPAddress();
	
	mediaformats+=endpoint.GetMediaFormats(); // Set mediaformats from owner endpoint
	

}

FSConnection::~FSConnection()
{
	delete rtpmanager;
	
}


void FSConnection::InitiateCall(const PString & party)
{
	phase = SetUpPhase;
	PTRACE(1, "FSConnection: \t Initiating Call to " << party);
	if (!OnIncomingConnection(0, NULL)){
		PTRACE(1, "FSConnection: \t Releasing the call with CallerAbort");
		Release(EndedByCallerAbort);
	}else{
		PTRACE(1, "FSConnection: \t Setting up the ownerCall");
		ownerCall.OnSetUp(*this);
	}
}


switch_call_cause_t FSConnection::SwitchInitiateCall(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
						   					   		 switch_core_session_t **new_session, switch_memory_pool_t **pool)
{

	return SWITCH_CAUSE_SUCCESS;
}

// function name sucks, but lets keep it for now
// this function gets called when calls come from external opal endpoints
// and goes to fs
BOOL FSConnection::CreateIncomingFSConnection()
{

	OpalCall & call = GetCall();
	PSafePtr<OpalConnection> OtherConnection = call.GetOtherPartyConnection(*this);
	
	//ownerCall.GetOtherPartyConnection(*this)->PreviewPeerMediaFormats(mediaformats);
	
	PString dest(OtherConnection->GetCalledDestinationNumber());
	//dest = dest.Right(dest.GetLength() - 3); // get destination without 'fs:'
	
	switch_mutex_t *mut;
	switch_core_session_t *session = NULL;
	switch_caller_profile_t *caller_profile = NULL;
	fs_obj_t *fs_pvt = NULL; // private object

	if (!OtherConnection)
		return FALSE;
	
	session = switch_core_session_request(opalfs_endpoint_interface, NULL);

	if(!session){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate session object\n");
		call.Clear(EndedByNoEndPoint);
	}
	
	fssession = session; // store the fs session there
	fs_pvt = (fs_obj_t*) switch_core_session_alloc(session, sizeof(fs_obj_t));

	if (!fs_pvt){
		PTRACE(3, "!fs_pvt");
		return FALSE;
	}
	
	

	if (switch_mutex_init(&(fs_pvt->flag_mutex), SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session))){
		PTRACE(3, "CAnnot init mutex");
		return FALSE;
	}
	
	fs_pvt->ownercall = &call;
	fs_pvt->Connection = this;

	switch_core_session_set_private(session, fs_pvt);

	frame.data = databuf;
	switch_set_flag(fs_pvt, TFLAG_INBOUND);

	caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
            (const char*)GetRemotePartyName(),   /**  username */
            "XML",                               /** TODO -> this should be configurable by core */
            (const char*)GetRemotePartyName(),   /** caller_id_name */
            (const char*)GetRemotePartyNumber(), /** caller_id_number */
            (const char*)GetRemotePartyAddress(),/** network addr */
            NULL,                                /** ANI */
            NULL,                                /** ANI II */
            NULL,                                /** RDNIS */
            "FSOpal",                            /** source */
            "10.0.0.1",                          /** TODO -> set context  */
            (const char*) dest        /** destination_number */
    );
	
	PWaitAndSignal m(ChannelMutex);

	switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_channel_set_name(channel,(const char*) GetToken());
    switch_channel_set_caller_profile(channel, caller_profile);
	switch_core_session_thread_launch(session);
	switch_channel_set_state(channel, CS_INIT);
	switch_channel_set_flag(channel, CF_ORIGINATOR);
	SetConnected();

	return TRUE;
}

BOOL FSConnection::SetUpConnection()
{
	
	OpalCall & call = GetCall();
	PSafePtr<OpalConnection> OtherConnection = call.GetOtherPartyConnection(*this);
	
	if (!OtherConnection && !IsOriginating()){
		PTRACE(3, "FSConn:\t No other connection in the call");
		return FALSE;
	}
	
	if (IsOriginating()){
		PTRACE(3, "FSConn:\tOutgoing Connection");	// outgoing channel on fs
		return TRUE;
	}

	PTRACE(3, "FSConn:\tIncoming Connection for " << OtherConnection->GetCalledDestinationNumber());
	
	SetPhase(SetUpPhase);
	
	return CreateIncomingFSConnection();
}

BOOL FSConnection::OnSetUpConnection()
{
	switch_channel_t *channel = switch_core_session_get_channel(fssession);
    assert(channel);  
	
	
		
	return TRUE;
}


BOOL FSConnection::SetAlerting(const PString& caleeName, BOOL withMedia)
{  
	switch_channel_t *channel = NULL;
	
	channel = switch_core_session_get_channel(fssession);
	fs_obj_t *fs_pvt = (fs_obj_t*) switch_core_session_get_private(fssession);
	assert(channel);
	
	if (IsOriginating()) {
	    PTRACE(2, "FSConn\tSetAlerting ignored on call we originated.");
	    return TRUE;
  	}

	if (phase != SetUpPhase)
    	return FALSE;

	SetPhase(AlertingPhase);
	OnAlerting(*this);
	
	switch_channel_set_state(channel, CS_RING);
	switch_set_flag(fs_pvt, TFLAG_IO);
	return TRUE;
}

void FSConnection::OnAlerting(OpalConnection & connection)
{
	switch_channel_t *channel = NULL;
	
	channel = switch_core_session_get_channel(fssession);
	fs_obj_t *fs_pvt = (fs_obj_t*) switch_core_session_get_private(fssession);
	SetPhase(EstablishedPhase);
	switch_channel_set_variable(channel, SWITCH_ORIGINATOR_CODEC_VARIABLE, "PCMU@30i,PCMA@30i");
	
	switch_channel_set_flag(channel, CF_ANSWERED);
	//
	//if (!IsOriginating())
		
		//switch_channel_set_state(channel, CS_RING);

}

BOOL FSConnection::SetConnected()
{
	PTRACE(3, "FSConn:\tSetConnected");
	PINDEX Session = 1;
	if (!IsOriginating()){
		if (mediaStreams.IsEmpty()){
			
				//RTP_Session *rtpsession = rtpmanager->GetSession(sessionID);
				//Get an rtp session to see if we have some in the manager
			/*
			for (; Session <=3 ; Session++){ // We are adding rtpsession to the manager.
							  						  // we have only rtp over udp for now.
				PTRACE(3, "Adding a new rtp session to rtpmanager with id " << Session);
				RTP_UDP *newsession = new RTP_UDP(NULL, Session, FALSE);
				newsession->Open(RTPLocalAddress, RTPLocalPort, 0, Session, NULL, NULL);
				newsession->SetLocalAddress(RTPLocalAddress);
				newsession->SetRemoteSocketInfo(RTPRemoteAddress, RTPRemotePort, FALSE);
				rtpmanager->AddSession(newsession);
			}
			*/
			PTRACE(3, "Mediaformats " << mediaformats);
				//increments usecount on the rtp session object so, we have to release it after
			    //we are done.
				//rtpsession = rtpmanager->UseSession(sessionID); 
				//OpalRTPSession = rtpsession;
			OnConnected();
		}	
	}
	
	return TRUE;
}

void FSConnection::OnConnected()
{
	switch_channel_t *channel = NULL;
	
	channel = switch_core_session_get_channel(fssession);
	fs_obj_t *fs_pvt = (fs_obj_t*) switch_core_session_get_private(fssession);

	if (!IsOriginating()){
		switch_channel_set_state(channel, CS_INIT);
		SetPhase(SetUpPhase);
		SetAlerting("", FALSE);
		//switch_set_flag(fs_pvt, TFLAG_IO);
	}
	
}

void FSConnection::OnReleased()
{
	switch_channel_t *channel = NULL;
	
	SetPhase(ReleasingPhase);
	
	CloseMediaStreams();
	
	channel = switch_core_session_get_channel(fssession);
	assert(channel);
	switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	OpalConnection::OnReleased();
	
}

OpalMediaFormatList FSConnection::GetMediaFormats() const
{
	
	return mediaformats;
}

BOOL FSConnection::IsMediaBypassPossible(unsigned sessionID)
{
	return TRUE;
}

OpalMediaStream* FSConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat,
												 unsigned  	  sessionID,
												 BOOL  	  isSource)
{
	if (sessionID != OpalMediaFormat::DefaultAudioSessionID) // only audio for now
		return OpalConnection::CreateMediaStream(mediaFormat, sessionID, isSource);

	PTRACE(3, "CODEC NAME  from source is " << mediaFormat.GetEncodingName());
	
	RTP_Session *rtpsession = rtpmanager->GetSession(sessionID); // Get an rtp session
	
	if (!rtpsession){ // We have to add an rtpsession to the manager.
		// we have only rtp over udp for now.
		PTRACE(3, "Adding a new rtp session to rtpmanager");
		RTP_UDP *newsession = new RTP_UDP(NULL, sessionID, FALSE);
		newsession->Open(RTPLocalAddress, 44000, 0, sessionID, NULL, NULL);
		newsession->SetLocalAddress(RTPLocalAddress);
		newsession->SetRemoteSocketInfo(RTPRemoteAddress, 44501, FALSE);
		newsession->SetRemoteSocketInfo(RTPRemoteAddress, 44500, TRUE);
		rtpmanager->AddSession(newsession);
	}
	
	rtpsession = rtpmanager->UseSession(sessionID); /*increments usecount on the rtp session object*/
	
	return CreateMediaStream(*this, mediaFormat, isSource, *rtpsession /*RTPSession*/, 0 /*minjitter delay*/, 0/*maxjitter delay*/);
}



OpalMediaStream * FSConnection::CreateMediaStream(OpalConnection & conn,
												  const OpalMediaFormat & mediaFormat, 
												  BOOL isSource,
												  RTP_Session &rtpSession,
												  unsigned minAudioJitterDelay, 
												  unsigned maxAudioJitterDelay)
{
	switch_channel_t *channel;
	switch_rtp_flag_t flags ;
	const char *err;
	
	fs_obj_t *fs_pvt = (fs_obj_t*) switch_core_session_get_private(fssession);	
	channel = switch_core_session_get_channel(fssession);
	
	PTRACE(3, "Codec name: " << mediaFormat.GetEncodingName() << " " << mediaFormat.GetClockRate() << " " << mediaFormat.GetFrameTime());
	
	if (isSource) {
		if (switch_core_codec_init
        	(&write_codec, "L16", NULL, mediaFormat.GetClockRate(), 30, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
        	switch_core_session_get_pool(fssession)) != SWITCH_STATUS_SUCCESS) {
        	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Cannot set write codec\n", switch_channel_get_name(channel));
        	switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
        	return NULL;
    	}
		switch_core_session_set_write_codec(fssession, &write_codec);
		
	}else{
		if (switch_core_codec_init
        	(&read_codec, "L16", NULL, mediaFormat.GetClockRate(), 30, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
        	switch_core_session_get_pool(fssession)) != SWITCH_STATUS_SUCCESS) {
        	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Cannot set read codec\n", switch_channel_get_name(channel));
        	switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
        	return NULL;
    	}

		switch_core_session_set_read_codec(fssession, &read_codec);
		
		frame.rate = mediaFormat.GetClockRate();
		frame.codec = &read_codec;
		
		
	}
		
	FSRTPSession  = switch_rtp_new("10.0.0.1", 
								   44500, 				// Source and destination is switched from opal
								   "10.0.0.1",
								   44000,					//this goes to Opal rtp data port
								   write_codec.implementation->ianacode,
								   write_codec.implementation->samples_per_frame,
								   write_codec.implementation->microseconds_per_frame,
								   flags, NULL, "soft", &err, switch_core_session_get_pool(fssession));

	switch_rtp_set_flag(FSRTPSession, SWITCH_RTP_FLAG_DATAWAIT);
	//switch_rtp_set_flag(FSRTPSession, SWITCH_RTP_FLAG_USE_TIMER);
	//switch_rtp_set_flag(FSRTPSession, SWITCH_RTP_FLAG_AUTOADJ);
	
	
	
	PTRACE(1, "FSConnection: new media stream !!!" << mediaFormat.GetEncodingName());
	//switch_channel_set_state(channel, CS_UNHOLD);
	return new FSMediaStream(*this, mediaFormat, isSource, rtpSession /*RTPSession*/, minAudioJitterDelay /*minjitter delay*/, maxAudioJitterDelay/*maxjitter delay*/);
}


switch_status_t FSConnection::callback_on_init(switch_core_session_t *session)
{
	return SWITCH_STATUS_SUCCESS;
	PWaitAndSignal m(ChannelMutex);

	OpalCall & call = GetCall();
	PSafePtr<OpalConnection> OtherConnection = call.GetOtherPartyConnection(*this);

	switch_channel_t *channel = switch_core_session_get_channel(session);
    assert(channel);  

	if (!IsOriginating()) { // incoming call initialize
		// TODO Set all the Mediaformats from fs endpoint to the
		// fs codec strings for the channels. be carefull about early media etc.
		//PTRACE(2, "FSConn:\t Answering Incoming Call");
		SetConnected();
		return SWITCH_STATUS_SUCCESS;
	}
	
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSConnection::callback_on_ring(switch_core_session_t *io_session)
{
   	switch_channel_t *channel = NULL;
	
	channel = switch_core_session_get_channel(io_session);
	//switch_channel_set_state(channel, CS_HOLD);
	OpalConnection::StartMediaStreams();

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSConnection::callback_on_execute(switch_core_session_t *io_session)
{

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSConnection::callback_on_hangup(switch_core_session_t *io_session)
{
	phase = ReleasingPhase;
	//OpalConnection::CloseMediaStreams();
	OpalConnection::ownerCall.Clear();
	OnReleased();
          
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSConnection::callback_on_loopback(switch_core_session_t *session)
{
    OpalCall & call = GetCall();
	PSafePtr<OpalConnection> OtherConnection = call.GetOtherPartyConnection(*this);

	OtherConnection->AnsweringCall(AnswerCallNow);
	
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSConnection::io_read_frame(switch_core_session_t *session, switch_frame_t **o_frame, int i_timeout, switch_io_flag_t i_flag, int i_streamId)
{
	switch_channel_t *channel = NULL;
	switch_frame_t *pframe;
	switch_status_t result;
	
	fs_obj_t *fs_pvt = (fs_obj_t*) switch_core_session_get_private(session);
	
	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);
	
	
	if (!switch_test_flag(fs_pvt, TFLAG_IO)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "read");
        return SWITCH_STATUS_SUCCESS;
    }

	pframe = &frame;
	*o_frame = pframe;
	
	switch_set_flag_locked(fs_pvt, TFLAG_READING);
	result = switch_rtp_zerocopy_read_frame(FSRTPSession, pframe);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "result from read %i\n", result);
	switch_clear_flag_locked(fs_pvt, TFLAG_READING);
	
	
    return result;
}

switch_status_t FSConnection::io_write_frame(switch_core_session_t *session, switch_frame_t *frame, int i_timeout, switch_io_flag_t i_flag, int i_streamId)
{

	switch_channel_t *channel = NULL;
	
	channel = switch_core_session_get_channel(session);
	fs_obj_t *fs_pvt = (fs_obj_t*) switch_core_session_get_private(session);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "write \n");
	
	while(!switch_rtp_ready(FSRTPSession)){
		 if (switch_channel_ready(channel)) {
            switch_yield(10000);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Yielding\n");
        } else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Yielding\n");
            return SWITCH_STATUS_GENERR;
        }
	}

	
	if (!switch_test_flag(fs_pvt, TFLAG_IO)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "write");
        return SWITCH_STATUS_SUCCESS;
    }
	
	switch_set_flag_locked(fs_pvt, TFLAG_WRITING);
	switch_rtp_write_frame(FSRTPSession, frame, 0);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "write \n");
	switch_clear_flag_locked(fs_pvt, TFLAG_WRITING);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSConnection::io_receive_message(switch_core_session_t *i_session, switch_core_session_message_t *i_message)
{
    
    switch_channel_t *channel = NULL;
	
	channel = switch_core_session_get_channel(i_session);
    OpalCall & call = GetCall();
	PSafePtr<OpalConnection> OtherConnection = call.GetOtherPartyConnection(*this);

	
	
    switch(i_message->message_id)
    {
    case SWITCH_MESSAGE_REDIRECT_AUDIO:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"SWITCH_MESSAGE_REDIRECT_AUDIO\n");
    break;
    case SWITCH_MESSAGE_TRANSMIT_TEXT:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_TRANSMIT_TEXT\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_ANSWER:
		OtherConnection->AnsweringCall(AnswerCallNow);	
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_ANSWER\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_PROGRESS:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_PROGRESS\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_BRIDGE:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_BRIDGE\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_UNBRIDGE\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_TRANSFER:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_TRANSFER\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_RINGING:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_RINGING\n");                                
    break;        
    case SWITCH_MESSAGE_INDICATE_MEDIA:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_MEDIA\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_NOMEDIA:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_NOMEDIA\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_HOLD:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_HOLD\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_UNHOLD:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_UNHOLD\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_REDIRECT:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_REDIRECT\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_REJECT:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_REJECT\n");
    break;
    case SWITCH_MESSAGE_INDICATE_BROADCAST:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_BROADCAST\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT\n");
    break;  
    default:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_???\n");            
    }
    
    //switch_mutex_unlock(tech_prv->m_mutex);
    return SWITCH_STATUS_SUCCESS;
}

