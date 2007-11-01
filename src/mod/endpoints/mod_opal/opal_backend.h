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
 * $Log: opal_backend.cpp,v $
 *
 * Revision 1.00  2007/10/24 07:29:52  lzwierko
 * Initial revision
 */

#ifndef __FREESWITCH_OPAL_BACKEND__
#define __FREESWITCH_OPAL_BACKEND__

#include <switch.h>
#include <ptlib.h>
#include <opal/buildopts.h>
#include <opal/manager.h>
#include <h323/h323ep.h>




/** 
 * Helper class for mutex use
 *
 **/
class SLock
{
public:
    
    SLock(switch_mutex_t* i_mutex) :
        m_mutex(NULL)
    {
        assert(i_mutex);
        m_mutex = i_mutex;
        switch_mutex_lock(m_mutex);
    }
    
    ~SLock()
    {
        switch_mutex_unlock(m_mutex);
    }
    
private:
    switch_mutex_t* m_mutex;
    
};

/** This class is OpalManager implementation
 *  for FreeSWITCH OpalH323 module.
 *  All methods are inherited from base OpalManagerClass.
 *  Event callbacks will be filed with valid code
 *  Additional functions have been implemented to be called, or called by 
 */
class FSOpalManager : public OpalManager
{
    PCLASSINFO(FSOpalManager, PObject);
    
public:
    
    /** Default constructor
     *
     */
    FSOpalManager();
    
    /** Destructor
     *
     */
    ~FSOpalManager();
    
    /** 
     *  Method does real initialization of the manager
     */
    bool initialize(
            const char* i_modName,
            switch_memory_pool_t* i_memoryPool,
            switch_endpoint_interface_t *i_endpointInterface
            );
    
    /** FS callback handlers declarations
     *
     */
    switch_status_t callback_on_init(switch_core_session_t *io_session);
    switch_status_t callback_on_ring(switch_core_session_t *io_session);
    switch_status_t callback_on_execute(switch_core_session_t *io_session);
    switch_status_t callback_on_hangup(switch_core_session_t *io_session);
    switch_status_t callback_on_loopback(switch_core_session_t *io_session);
    switch_status_t callback_on_transmit(switch_core_session_t *io_session);
    
    /** FS io functions 
     *
     */
    
    switch_call_cause_t io_outgoing_channel(switch_core_session_t *, switch_caller_profile_t *, switch_core_session_t **, switch_memory_pool_t **);
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
    
    
    /**
     * Following OnIncomingConnection functions 
     * have been overriden for serving
     * connections comming from H323 network
     * They are called on receiving SETUP
     */
    virtual BOOL OnIncomingConnection(
        OpalConnection & connection,   ///<  Connection that is calling
        unsigned options,              ///<  options for new connection (can't use default as overrides will fail)
        OpalConnection::StringOptions * stringOptions
        );
    virtual BOOL OnIncomingConnection(
        OpalConnection & connection,   ///<  Connection that is calling
        unsigned options               ///<  options for new connection (can't use default as overrides will fail)
        );
    
    virtual BOOL OnIncomingConnection(
        OpalConnection & connection   ///<  Connection that is calling
        );
    
    /**
     * OnAnswerCall function is overriden for 
     * serving a situation where H323 driver has send 
     * CALL PROCEEDING message
     */
    virtual OpalConnection::AnswerCallResponse OnAnswerCall(
            OpalConnection &connection,
            const PString &caller);
    
        
        
    
private:          
    
    void                   saveSessionToken(const PString &i_token,switch_core_session_t* i_session);
    switch_core_session_t* getSessionToken(const PString &i_token);
    void                   deleteSessionToken(const PString &i_token);
    
    switch_call_cause_t             causeH323ToOpal(OpalConnection::CallEndReason i_cause);
    OpalConnection::CallEndReason   causeOpalToH323(switch_call_cause_t i_cause);
    
    
    const char                          *m_pModuleName;             /* name of this module */
    bool                                m_isInitialized;             /* true if module has been initialized properly */
    H323EndPoint                        *m_pH323Endpoint;           /* h323 endpoint control */
    switch_memory_pool_t                *m_pMemoryPool;             /* FS memory pool */
    switch_endpoint_interface_t         *m_pH323EndpointInterface;  /* FS endpoint inerface */
    switch_hash_t                       *m_pSessionsHashTable;      /* Stores pointrs to session object for each Opal connection */
    switch_mutex_t                      *m_pSessionsHashTableMutex; /* Protects hash table */
    
};



#endif /* __FREESWITCH_OPAL_BACKEND__ */
