/*
 * opalh323_backend.cpp
 *
 * Backend for OpalH323 module, implements 
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
 * $Log: opalh323_backend.cpp,v $
 *
 * Revision 1.00  2007/10/24 07:29:52  lzwierko
 * Initial revision
 */

#include <switch.h>
#include "opalh323_backend.h"
#include <include/h323/h323ep.h>

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
    if(SWITCH_STATUS_SUCCESS != switch_mutex_init(&tech_pvt->m_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(i_session))
    {
        assert(0);
        return false;
    }
    
    return true;
    
}

static bool OpalH323Private_Delete(OpalH323Private_t *o_private)
{
    switch_mutex_destroy(tech_pvt->m_mutex);
}

/** Default constructor
 *
 */
FSOpalManager::FSOpalManager() :
    m_isInitialized(false),
    m_pH323Endpoint(NULL),
    m_pMemoryPool(NULL),
    m_pEndpointInterface(NULL),
    m_pSessionsHashTable(NULL),
    m_pSessionsHashTableMutex(NULL)
{
    
}

/** Destructor
 *
 */
~FSOpalManager::FSOpalManager()
{    
    /**
     *  Destroy all allocated resources, if any
     */
    if(m_isInitialized)
    {
        delete m_pH323Endpoint;
        switch_mutex_destroy(m_pSessionsHashTableMutex);
        switch_core_hash_destroy(m_pSessionsHashTable);
        
    }
}

 /**
  *  Method does real initialization of the manager
  */
bool FSOpalManager::initialize(
        char                        *i_modName,
        switch_memory_pool_t        *i_memoryPool,
        switch_endpoint_interface_t *i_endpointInterface
        )
{
    bool result = true;
    
    /* check if everything is not initialized */
    assert(m_isInitialized);
    assert(!m_pH323Endpoint);
    assert(!m_pMemoryPool)
    assert(!m_pEndpointInterface);
    assert(!m_pSessionsHashTable);
    
    /* check input parameters */
    assert(i_modName);
    assert(i_memoryPool);
    assert(i_endpointInterface);
    
    
    m_pModuleName = i_modName;
    m_pMemoryPool = i_memoryPool;
    m_pEndpointInterface = i_endpointInterface;

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
    
    if(switch_mutex_init(&m_pSessionsHashTableMutex,SWITCH_THREAD_MUTEX_UNNESTED,m_pMemoryPool)!=SWITCH_STATUS_SUCCESS)
    {
       assert(0);
       switch_core_hash_destroy(m_pSessionsHashTable);     
       return false; 
    }
    
    /* create h323 endpoint */
    m_pH323Endpoint = new H323EndPoint(this);   ///TODO, replace prefix and signaling port by values from configuration
    if(!m_pH323Endpoint)
    {
        assert(0);
        switch_core_hash_destroy(m_pSessionsHashTable); 
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
    OpalTransportAddress opalTransportAddress("0.0.0.0",1720); //for time being create listener on all ip's and default port
    if(!m_pH323Endpoint->StartListeners(opalTransportAddress))
    {
        assert(0);
        swith_core_hash_destroy(m_pSessionsHashTable);    
        switch_mutex_destroy(m_pSessionsHashTableMutex);
        delete m_pH323Endpoint:            
        return false;
    }                 
    
    /* at this point OPAL is ready to go */
    m_isInitialized = true;
    return true;
}


BOOL FSOpalManager::OnIncomingConnection(
        OpalConnection & connection,   ///<  Connection that is calling
        unsigned options,              ///<  options for new connection (can't use default as overrides will fail)
        OpalConnection::StringOptions * stringOptions
        )
{
    //TODO check if options and stringOptions fields ever apply
    retrun OnIncomingConnection(connection);
}        
        
BOOL FSOpalManager::OnIncomingConnection(
        OpalConnection & connection,   ///<  Connection that is calling
        unsigned options               ///<  options for new connection (can't use default as overrides will fail)
        )
{
    //TODO, check if options field ever applies
    retrun OnIncomingConnection(connection);
}

BOOL FSOpalManager::OnIncomingConnection(
        OpalConnection & connection   ///<  Connection that is calling
        )
{
    /* allocate new session in switch core*/
    switch_core_session_t *session = switch_core_session_request(m_pEndpointInterface , NULL);
    assert(session);
    if(!sesion)
    {
        ///TODO add cause to the connection
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate session object?\n");
        return FALSE;
    }
    
    /* allocate private resources */
    OpalH323Private_t *tech_pvt = NULL;
    if(!OpalH323Private_Create(&tech_pvt))
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
    switch_core_hash_insert_locked(m_pSessionsHashTable,*(connection.GetToken()),static_cast<void*>(session));  ///save pointer to session in hash table, for later retreival    
   
    /** Create calling side profile */
    tech_pvt->m_callerProfile = switch_caller_profile_new(
            switch_core_session_get_pool(session),
            *connection.GetRemotePartyName(),               /**  username */
            "default",                                      /** TODO -> this should be configurable by core */
            *connection.GetRemotePartyName(),               /** caller_id_name */
            *connection.GetRemotePartyNumber(),             /** caller_id_number */
            *connection.GetRemotePartyAddress(),            /** network addr */
            NULL,                                           /** ANI */
            NULL,                                           /** ANI II */
            NULL,                                           /** RDNIS */
            m_pModuleName,                                  /** source */
            NULL,                                           /** TODO -> set context  */
            *connection.GetCalledDestinationNumber()        /** destination_number */
    );
    
    if(!tech_pvt->m_callerProfile)  /* should never error */
    {       
        switch_core_hash_delete_locked(m_pSessionsHashTable,*(connection.GetToken()));  
        switch_mutex_unlock(tech_pvt->m_mutex);
        OpalH323Private_Delete(tech_pvt);
        switch_core_session_destroy(&session);              
        assert(0);
        return false;
    }
    
    /** Set up sessions channel */
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_channel_set_name(channel,*connection.GetToken());
    switch_channel_set_caller_profile(channel, tech_pvt->m_callerProfile);
    switch_channel_set_state(channel, CS_INIT);
    
    /** Set up codecs for the channel ??? */   
    
          
    /***Mark incoming call as AnsweredPending ??? */
    

    /** lunch thread */       
    if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) 
    {
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error spawning thread\n");
        switch_core_hash_delete_locked(m_pSessionsHashTable,*(connection.GetToken()));        
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
 






/** 
 * FS ON_INIT callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_init(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_INIT callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_ring(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_EXECUTE callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_execute(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_HANGUP callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_hangup(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_LOOPBACK callback handler
 *
 */

switch_status_t FSOpalManager::callback_on_loopback(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

/** 
 * FS ON_TRANSMIT callback handler
 *
 */
switch_status_t FSOpalManager::callback_on_transmit(switch_core_session_t *io_session)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_call_cause_t FSOpalManager::io_outgoing_channel(switch_core_session_t *i_session, switch_caller_profile_t *i_profile, switch_core_session_t **o_newSession, switch_memory_pool_t **o_memPool)
{
    assert(m_isInitialized);
    return 0;
}

switch_status_t FSOpalManager::io_read_frame(switch_core_session_t *i_session, switch_frame_t **o_frame, int i_timout, switch_io_flag_t i_flag, int i_streamId)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_write_frame(switch_core_session_t *i_session, switch_frame_t *i_frame, int i_timeout, switch_io_flag_t i_flag, int i_streamId)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_kill_channel(switch_core_session_t *i_session, int sig)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_waitfor_read(switch_core_session_t *i_session, int i_ms, int i_streamId)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_waitfor_write(switch_core_session_t *i_session, int i_ms, int i_streamId)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_send_dtmf(switch_core_session_t *i_session, char *i_dtmf)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_receive_message(switch_core_session_t *i_session, switch_core_session_message_t *i_message)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_receive_event(switch_core_session_t *i_session, switch_event_t *i_event)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_state_change(switch_core_session_t *)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_read_video_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSOpalManager::io_write_video_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int)
{
    assert(m_isInitialized);
    return SWITCH_STATUS_SUCCESS;
}