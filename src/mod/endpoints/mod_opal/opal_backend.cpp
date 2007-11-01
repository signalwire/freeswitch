/*
 * opalh323_backend.cpp
 *
 * Backend for Opal module, implements 
 * H323 handling via OPAL library
 *
 * Copyright (c) 2007 Lukasz Zwierko (lzwierko@gmail.com)
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 *
 * The Initial Developer of the Original Code is Lukasz Zwierko (lzwierko@gmail.com)
 *
 * Contributor(s):
 *
 * $Log: opal_backend.cpp,v $
 *
 * Revision 1.00  2007/10/24 07:29:52  lzwierko
 * Initial revision
 */

#include <switch.h>
#include "opal_backend.h"

/** 
 * Private structre
 */

typedef struct OpalH323Private_s
{
    OpalConnection          *m_opalConnection;   /** pointer to OpalConnection object */
    switch_mutex_t          *m_mutex;            /** mutex for synchonizing access to session object */
    switch_caller_profile_t *m_callerProfile;          /** caller profile */
    
} OpalH323Private_t; 


static bool OpalH323Private_Create(OpalH323Private_t **o_private, switch_core_session_t *i_session)
{    
    *o_private = (OpalH323Private_t *)switch_core_session_alloc(i_session, sizeof(OpalH323Private_t));
    if(!o_private)
    {
        assert(0);
        return false;
    }
    if(SWITCH_STATUS_SUCCESS != switch_mutex_init(&(*o_private)->m_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(i_session)))
    {
        assert(0);
        return false;
    }
    
    return true;
    
}

static bool OpalH323Private_Delete(OpalH323Private_t *o_private)
{
    return (switch_mutex_destroy(o_private->m_mutex)==SWITCH_STATUS_SUCCESS); 
}

/** Default constructor
 *
 */
FSOpalManager::FSOpalManager() :
    m_isInitialized(false),
    m_pH323Endpoint(NULL),
    m_pMemoryPool(NULL),
    m_pH323EndpointInterface(NULL),
    m_pSessionsHashTable(NULL),
    m_pSessionsHashTableMutex(NULL)
{
    
}

/** Destructor
 *
 */
FSOpalManager::~FSOpalManager()
{    
    /**
     *  Destroy all allocated resources, if any
     *  !! all endpoints are automatically deleted in ~OpalManager, so leave them
     */
    if(m_isInitialized)
    {                
        switch_mutex_destroy(m_pSessionsHashTableMutex);
        switch_core_hash_destroy(&m_pSessionsHashTable);
        
    }
}

 /**
  *  Method does real initialization of the manager
  */     
bool FSOpalManager::initialize(
            const char* i_modName,
            switch_memory_pool_t* i_memoryPool,
            switch_endpoint_interface_t *i_endpointInterface
            )                
{                       
    /* check if not initialized */
    assert(!m_isInitialized);    
    /* check input parameters */
    assert(i_modName);
    assert(i_memoryPool);
    assert(i_endpointInterface);
    
    m_pModuleName = i_modName;
    m_pMemoryPool = i_memoryPool;
    m_pH323EndpointInterface = i_endpointInterface;

    /**
     * Create hash table for storing pointers to session objects,
     * Each OpalConnection object will retreive it's session object using
     * its callToken as a key
     */
    
    if(switch_core_hash_init(&m_pSessionsHashTable,m_pMemoryPool)!=SWITCH_STATUS_SUCCESS)
    {        
        assert(0);
        return false;
    }
    
    if(switch_mutex_init(&m_pSessionsHashTableMutex,SWITCH_MUTEX_UNNESTED,m_pMemoryPool)!=SWITCH_STATUS_SUCCESS)
    {
       assert(0);
       switch_core_hash_destroy(&m_pSessionsHashTable);     
       return false; 
    }
    
    /* create h323 endpoint */
    m_pH323Endpoint = new H323EndPoint( *(static_cast<OpalManager*>(this)) );   ///TODO, replace prefix and signaling port by values from configuration
    if(!m_pH323Endpoint)
    {
        assert(0);
        switch_core_hash_destroy(&m_pSessionsHashTable); 
        switch_mutex_destroy(m_pSessionsHashTableMutex);
        return false;
    }
    
    /**
     *  To do-> add codecs to capabilities (for call contol)
     *  m_pH323Endpoint->AddCapability();
    */
    
    m_pH323Endpoint->DisableFastStart(false);           ///TODO this should be configurable
    m_pH323Endpoint->DisableH245Tunneling(false);       ///TODO this should be configurable
    
    ///TODO gatekeeper use should be configurable, I think that sevral options should be implemented in config file: use, dont use, use one of specified with priorities, try to reconnect to the topmost...    
    ///TODO m_pH323Endpoint->SetInitialBandwidth(initialBandwidth);
    ///TODO m_pH323Endpoint->SetVendorIdentifierInfo()
    
    ///TODO address should be configurable, should allow creaeing listeners on multiple interfaces
    OpalTransportAddress opalTransportAddress("192.168.0.1",1720); //for time being create listener on all ip's and default port
    if(!m_pH323Endpoint->StartListeners(opalTransportAddress))
    {
        assert(0);
        switch_core_hash_destroy(&m_pSessionsHashTable);    
        switch_mutex_destroy(m_pSessionsHashTableMutex);
        delete m_pH323Endpoint;            
        return false;
    }                 
    
    /* at this point OPAL is ready to go */
    m_isInitialized = true;
    return true;
}

switch_core_session_t* FSOpalManager::getSessionToken(const PString &i_token)
{
    assert(m_pSessionsHashTable);
    assert(m_pSessionsHashTableMutex);
    return static_cast<switch_core_session_t*>(switch_core_hash_find_locked(m_pSessionsHashTable,(const char*)i_token,m_pSessionsHashTableMutex));
}

void FSOpalManager::saveSessionToken(const PString &i_token,switch_core_session_t* i_session)
{
    assert(m_pSessionsHashTable);
    assert(m_pSessionsHashTableMutex);    
    switch_core_hash_insert_locked(m_pSessionsHashTable,(const char*)i_token,i_session,m_pSessionsHashTableMutex);
}

void FSOpalManager::deleteSessionToken(const PString &i_token)
{
    assert(m_pSessionsHashTable);
    assert(m_pSessionsHashTableMutex);
    switch_core_hash_delete_locked(m_pSessionsHashTable,(const char*)i_token,m_pSessionsHashTableMutex);
}

switch_call_cause_t FSOpalManager::causeH323ToOpal(OpalConnection::CallEndReason i_cause)
{
    //TODO -> fill all causes
    return SWITCH_CAUSE_NORMAL_CLEARING;
}

OpalConnection::CallEndReason FSOpalManager::causeOpalToH323(switch_call_cause_t i_cause)
{
    //TODO -> fill all causes
    return OpalConnection::EndedByLocalUser;
}


BOOL FSOpalManager::OnIncomingConnection(
        OpalConnection & connection,   ///<  Connection that is calling
        unsigned options,              ///<  options for new connection (can't use default as overrides will fail)
        OpalConnection::StringOptions * stringOptions
        )
{
    //TODO check if options and stringOptions fields ever apply
    return OnIncomingConnection(connection);
}        
        
BOOL FSOpalManager::OnIncomingConnection(
        OpalConnection & connection,   ///<  Connection that is calling
        unsigned options               ///<  options for new connection (can't use default as overrides will fail)
        )
{
    //TODO, check if options field ever applies
    return OnIncomingConnection(connection);
}

BOOL FSOpalManager::OnIncomingConnection(
        OpalConnection & connection   ///<  Connection that is calling
        )
{
    /* allocate new session in switch core*/
    switch_core_session_t *session = switch_core_session_request(m_pH323EndpointInterface , NULL);
    assert(session);
    if(!session)
    {
        ///TODO add cause to the connection
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate session object?\n");
        return FALSE;
    }
    
    /* allocate private resources */
    OpalH323Private_t *tech_pvt = NULL;
    if(!OpalH323Private_Create(&tech_pvt,session))
    {
        ///TODO add cause to the connection
        switch_core_session_destroy(&session);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate private object?\n");
        return false;
    }     
    tech_pvt->m_opalConnection = &connection; 
   
    
    /** Save private data in session private data, and save session in hash tabel, under GetToken() key */
    switch_core_session_set_private(session,static_cast<void*>(tech_pvt));                                      ///save private data in session context
    
    /** Before adding this session to sessions hash table, 
     * lock he mutex so no concurrent 
     * callback can be runing for this connection 
     * probably other callbacks are called from this task
     * but carfulness wont bite
     */ 
    switch_mutex_lock(tech_pvt->m_mutex);        
    /** insert connection to hash table */
    saveSessionToken(connection.GetToken(),session); ///save pointer to session in hash table, for later retreival    
   
    /** Create calling side profile */
    tech_pvt->m_callerProfile = switch_caller_profile_new(
            switch_core_session_get_pool(session),
            (const char*)connection.GetRemotePartyName(),   /**  username */
            "default",                                      /** TODO -> this should be configurable by core */
            (const char*)connection.GetRemotePartyName(),   /** caller_id_name */
            (const char*)connection.GetRemotePartyNumber(), /** caller_id_number */
            (const char*)connection.GetRemotePartyAddress(),/** network addr */
            NULL,                                           /** ANI */
            NULL,                                           /** ANI II */
            NULL,                                           /** RDNIS */
            m_pModuleName,                                  /** source */
            NULL,                                           /** TODO -> set context  */
            (const char*)connection.GetCalledDestinationNumber()        /** destination_number */
    );
    
    if(!tech_pvt->m_callerProfile)  /* should never error */
    {       
        deleteSessionToken(connection.GetToken());  
        switch_mutex_unlock(tech_pvt->m_mutex);
        OpalH323Private_Delete(tech_pvt);
        switch_core_session_destroy(&session);              
        assert(0);
        return false;
    }
    
    /** Set up sessions channel */
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_channel_set_name(channel,(const char*)connection.GetToken());
    switch_channel_set_caller_profile(channel, tech_pvt->m_callerProfile);
    switch_channel_set_state(channel, CS_INIT);
    
    /** Set up codecs for the channel ??? */   
    
          
    /***Mark incoming call as AnsweredPending ??? */
   

    /** lunch thread */       
    if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) 
    {
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error spawning thread\n");
        deleteSessionToken(connection.GetToken());       
        switch_mutex_unlock(tech_pvt->m_mutex);
        OpalH323Private_Delete(tech_pvt);        
        switch_core_session_destroy(&session);        
        assert(0);
        return false;
    }
    switch_mutex_unlock(tech_pvt->m_mutex);
    
        
    /* the connection can be continued!!! */
    return TRUE;
    
}

OpalConnection::AnswerCallResponse FSOpalManager::OnAnswerCall(
            OpalConnection &connection,
            const PString &caller)
{
    switch_core_session_t *session = getSessionToken((const char*)connection.GetToken());
    if(session==NULL)   /* that can mean that session has been already destroyed by core and we should release it */
    {
        return OpalConnection::AnswerCallDenied;
    }
    return OpalConnection::AnswerCallDeferred;  //don't send alerting signal yet        
}





/** 
 * FS ON_INIT callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_init(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"callback_on_init\n");
    
    OpalH323Private_t* tech_prv = (OpalH323Private_t*)switch_core_session_get_private(io_session);
    
    if(tech_prv==NULL)
    {
        assert(0);
        return SWITCH_STATUS_NOTFOUND;        
    }
      
    SLock(tech_prv->m_mutex); /* lock channel */    
    switch_channel_t *channel = switch_core_session_get_channel(io_session);
    assert(channel);    
    /* Move Channel's State Machine to RING */
    switch_channel_set_state(channel, CS_RING);            
    
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_INIT callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_ring(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"callback_on_ring\n");
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_EXECUTE callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_execute(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"callback_on_execute\n");
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_HANGUP callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_hangup(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"callback_on_hangup\n");
    
    OpalH323Private_t* tech_prv = (OpalH323Private_t*)switch_core_session_get_private(io_session);                 
    switch_mutex_lock(tech_prv->m_mutex); /* lock channel */    
    deleteSessionToken(tech_prv->m_opalConnection->GetToken()); //delete this connection form connection pool
    switch_channel_t *channel = switch_core_session_get_channel(io_session);    
    if(tech_prv->m_opalConnection)
    {
        //switch_call_cause_t cause = switch_channel_get_cause(channel);    
        tech_prv->m_opalConnection->Release(); ///TODO add cause    
    }
    switch_mutex_unlock(tech_prv->m_mutex);
    OpalH323Private_Delete(tech_prv);
          
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_LOOPBACK callback handler
 *
 */

switch_status_t FSOpalManager::callback_on_loopback(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"callback_on_loopback\n");
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_TRANSMIT callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_transmit(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"callback_on_transmit\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_call_cause_t FSOpalManager::io_outgoing_channel(switch_core_session_t *i_session, switch_caller_profile_t *i_profile, switch_core_session_t **o_newSession, switch_memory_pool_t **o_memPool)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_outgoing_channel\n");
    return SWITCH_CAUSE_SUCCESS;
}

switch_status_t FSOpalManager::io_read_frame(switch_core_session_t *i_session, switch_frame_t **o_frame, int i_timout, switch_io_flag_t i_flag, int i_streamId)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_read_frame\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_write_frame(switch_core_session_t *i_session, switch_frame_t *i_frame, int i_timeout, switch_io_flag_t i_flag, int i_streamId)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_write_frame\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_kill_channel(switch_core_session_t *i_session, int sig)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_kill_channel\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_waitfor_read(switch_core_session_t *i_session, int i_ms, int i_streamId)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_waitfor_read\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_waitfor_write(switch_core_session_t *i_session, int i_ms, int i_streamId)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_waitfor_write\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_send_dtmf(switch_core_session_t *i_session, char *i_dtmf)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_send_dtmf\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_receive_message(switch_core_session_t *i_session, switch_core_session_message_t *i_message)
{
    assert(m_isInitialized);
    
    OpalH323Private_t* tech_prv = static_cast<OpalH323Private_t*>(switch_core_session_get_private(i_session));
    assert(tech_prv);
    
    switch_mutex_lock(tech_prv->m_mutex);
    
    switch(i_message->message_id)
    {
    case SWITCH_MESSAGE_REDIRECT_AUDIO:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"SWITCH_MESSAGE_REDIRECT_AUDIO\n");
    break;
    case SWITCH_MESSAGE_TRANSMIT_TEXT:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_TRANSMIT_TEXT\n");
    break;        
    case SWITCH_MESSAGE_INDICATE_ANSWER:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"SWITCH_MESSAGE_INDICATE_ANSWER\n");
        
        /* set call answer */
        //tech_prv->m_opalConnection->AnsweringCall(AnswerCallNow);        
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
    
    switch_mutex_unlock(tech_prv->m_mutex);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_receive_event(switch_core_session_t *i_session, switch_event_t *i_event)
{    
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_receive_event\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_state_change(switch_core_session_t *)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_state_change\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_read_video_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_read_video_frame\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_write_video_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int)
{
    assert(m_isInitialized);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG ,"io_write_video_frame\n");
    return SWITCH_STATUS_SUCCESS;
}
