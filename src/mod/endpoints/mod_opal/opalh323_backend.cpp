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
    OpalConnection *m_opalConnection;   /** pointer to OpalConnection object */
    switch_mutex_t *m_mutex;            /** mutex for synchonizing access to session object */
    
} OpalH323Private_t;
        


/** Default constructor
 *
 */
FSOpalManager::FSOpalManager() :
    m_isInitialized(false),
    m_pH323Endpoint(NULL);
    m_pMemoryPool(NULL)
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
        m_pH323Endpoint = NULL;
        m_isInitialized = false;
    }
}

 /**
  *  Method does real initialization of the manager
  */
bool FSOpalManager::initialize(
        switch_memory_pool_t        *i_memoryPool,
        switch_endpoint_interface_t *i_endpointInterface
        )
{
    bool result = true;
    
    /* check if everything is not initialized */
    assert(m_isInitialized);
    assert(!m_pH323Endpoint);
    assert(!m_pMemoryPool)
    
    /* check input parameters */
    assert(i_memoryPool);
    assert(i_endpointInterface);
    
    
    
    m_pMemoryPool = i_memoryPool;
    m_pEndpointInterface = i_endpointInterface;
    
    /* create h323 endpoint */
    m_pH323Endpoint = new H323EndPoint(this);   ///TODO, replace prefix and signaling port by values from configuration
    if(!m_pH323Endpoint)
    {
        assert(0);
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
    result = m_pH323Endpoint->StartListeners(opalTransportAddress);
    assert(result);
    if(!result)
    {
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
    OpalH323Private_t *tech_pvt = (OpalH323Private_t *)switch_core_session_alloc(session, sizeof(OpalH323Private_t));
    assert(tech_pvt);
    if(!tech_pvt)
    {
        ///TODO add cause to the connection
        switch_core_session_destroy(&session);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate private object?\n");
        return false;
    }
    tech_pvt->m_opalConnection = &connection;
    switch_mutex_init(&tech_pvt->m_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
    /** save private data under hash and in session */
    
    /***Mark incoming call as AnsweredPending ??? */

    
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


