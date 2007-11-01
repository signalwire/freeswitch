/*
 * mod_opalh323.cpp
 *
 * Opal gluer for Freeswitch
 * This file implements fontend of Opal module functions
 * that is all functions that are used for communication
 * between FreeSWITCH core and this module
 *
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
 * $Log: mod_opal.cpp,v $
 *
 * Revision 1.00  2007/10/24 07:29:52  lzwierko
 * Initial revision
 */


#include "mod_opal.h"
#include "opal_backend.h"
#include <switch.h>
   
/*
 * IO routines handlers definitions for H323
 */
static switch_call_cause_t opalh323_outgoing_channel(switch_core_session_t *, switch_caller_profile_t *, switch_core_session_t **, switch_memory_pool_t **);
static switch_status_t opalh323_read_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
static switch_status_t opalh323_write_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);
static switch_status_t opalh323_kill_channel(switch_core_session_t *, int);
static switch_status_t opalh323_waitfor_read(switch_core_session_t *, int, int);
static switch_status_t opalh323_waitfor_write(switch_core_session_t *, int, int);
static switch_status_t opalh323_send_dtmf(switch_core_session_t *, char *);
static switch_status_t opalh323_receive_message(switch_core_session_t *, switch_core_session_message_t *);
static switch_status_t opalh323_receive_event(switch_core_session_t *, switch_event_t *);
static switch_status_t opalh323_state_change(switch_core_session_t *);
static switch_status_t opalh323_read_video_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
static switch_status_t opalh323_write_video_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);
/*
 * Event handlers declaration for H323
 */
static switch_status_t opalh323_on_init(switch_core_session_t *session);
static switch_status_t opalh323_on_ring(switch_core_session_t *session);
static switch_status_t opalh323_on_execute(switch_core_session_t *session);
static switch_status_t opalh323_on_hangup(switch_core_session_t *session);
static switch_status_t opalh323_on_loopback(switch_core_session_t *session);
static switch_status_t opalh323_on_transmit(switch_core_session_t *session);

/**
 * Declaration of private variables 
 */
static switch_memory_pool_t *opal_pool = NULL;
static switch_endpoint_interface_t *opalh323_endpoint_interface = NULL;

/** 
 * Declaration of PProces ancestor for OpalManager starting 
 *
 */
class OpalStartProcess : public PProcess
{
  PCLASSINFO(OpalStartProcess, PProcess)

  public:
    
      
    /** 
     * This dunction creates new instace of 
     * OpalStartProcess and starts it's task
     */
    static void createInstance(
            const char* i_moduleName,
            switch_memory_pool_t *i_memoryPool,
            switch_endpoint_interface_t *i_endpointInterface)
    {       
        assert(!s_startProcess);
        assert(i_moduleName);
        assert(i_memoryPool);
        assert(i_endpointInterface);        
               
        PProcess::PreInitialise(0, NULL, NULL);
        s_startProcess = new OpalStartProcess(i_moduleName,i_memoryPool,i_endpointInterface);
        assert(s_startProcess);
        if(!s_startProcess)
        {
            return;
        }
        s_startProcess->_main(); /* run process */    
        delete s_startProcess;   /* delete opal manager instance */ 
        s_startProcess = NULL;   /* clear pointer */
    }
    
    static OpalStartProcess* getInstance()
    {
        assert(s_startProcess);
        return s_startProcess;
    }
    
    static bool checkInstanceExists()
    {        
        return s_startProcess!=NULL;
    }
    
    OpalStartProcess(
        const char* i_moduleName,
        switch_memory_pool_t *i_memoryPool,
        switch_endpoint_interface_t *i_endpointInterface
    ):             
        PProcess("FreeSWITCH", "mod_opal"),
        m_pStopCondition(NULL),  
        m_pModuleName(i_moduleName),
        m_pMemoryPool(i_memoryPool),
        m_pEndpointInterface(i_endpointInterface)
    {                    
           
        
            switch_status_t status = switch_thread_cond_create(&m_pStopCondition, m_pMemoryPool);
            assert(status==SWITCH_STATUS_SUCCESS);
            if(status!=SWITCH_STATUS_SUCCESS) 
            {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can not init stop condition.");
                return;
            }                        
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "OpalStartProcess created\n");
    }
        
    ~OpalStartProcess()
    {
        switch_thread_cond_destroy(m_pStopCondition);
                
        m_pModuleName        = NULL;
        m_pMemoryPool        = NULL;
        m_pEndpointInterface = NULL; 
    }
    
    FSOpalManager *getManager()
    {
        assert(!m_pFSOpalManager);
        return m_pFSOpalManager;
    }
      
    /* main task of this PProcess */
    void Main()
    {                
        /* backend initialization */
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Begin opal backend init\n");
        m_pFSOpalManager = new FSOpalManager();
        assert(m_pFSOpalManager);
        if(!m_pFSOpalManager) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can not create opal manger\n");
            return;
        }
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Opal manager created\n");
        
        if(!m_pFSOpalManager->initialize(m_pModuleName, m_pMemoryPool, m_pEndpointInterface)) 
        {
            delete m_pFSOpalManager;
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can not initialize opal manger\n");
            return; /* if can't initialize return general error */
        }
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Opal manager initilaized and running\n");       
        WaitUntilStopped();        
        delete m_pFSOpalManager;        
        m_pFSOpalManager = NULL;                        
    }        
    
    /** 
     * Waits until this process is terminated
     * which means it exits Main() function
     */
    void StopProcess()
    {
        switch_thread_cond_signal(m_pStopCondition);
    }
    
  protected:              
      
      /*
       * After entering this functon, process 
       * locks o condition
       * and stays there until signalled 
       */
      
      void WaitUntilStopped()
      {
          switch_mutex_t* mutex = NULL;
          switch_status_t status = switch_mutex_init(&mutex, SWITCH_MUTEX_NESTED, m_pMemoryPool);
          if(status!=SWITCH_STATUS_SUCCESS) 
          {
              switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Error acquiring mutex!\n");
              assert(0);
              return;
          }
          switch_mutex_lock(mutex);
          switch_thread_cond_wait(m_pStopCondition, mutex);
          switch_mutex_unlock(mutex);
          switch_mutex_destroy(mutex);            
      }
      
      static OpalStartProcess     *s_startProcess;
      
      const char                  *m_pModuleName;
      switch_memory_pool_t        *m_pMemoryPool;
      switch_endpoint_interface_t *m_pEndpointInterface;              
      FSOpalManager               *m_pFSOpalManager;
      switch_thread_cond_t        *m_pStopCondition; /* the main thread waits on this condition until is to be stopped */      
        
};
OpalStartProcess* OpalStartProcess::s_startProcess = NULL;

/*
 * IO routines handlers set declaration 
 */
static switch_io_routines_t opalh323_io_routines = {
	/*.outgoing_channel */ opalh323_outgoing_channel,
	/*.read_frame */ opalh323_read_frame,
	/*.write_frame */ opalh323_write_frame,
	/*.kill_channel */ opalh323_kill_channel,
	/*.waitfor_read */ opalh323_waitfor_read,
	/*.waitfor_read */ opalh323_waitfor_write,
	/*.send_dtmf */ opalh323_send_dtmf,
	/*.receive_message */ opalh323_receive_message,
	/*.receive_event */ opalh323_receive_event,
	/*.state_change*/ opalh323_state_change,
	/*.read_video_frame*/ opalh323_read_video_frame,
	/*.write_video_frame*/ opalh323_write_video_frame
};

static switch_state_handler_table_t opalh323_event_handlers = {
	/*.on_init */ opalh323_on_init,
	/*.on_ring */ opalh323_on_ring,
	/*.on_execute */ opalh323_on_execute,
	/*.on_hangup */ opalh323_on_hangup,
	/*.on_loopback */ opalh323_on_loopback,
	/*.on_transmit */ opalh323_on_transmit
};

/* 
 * Loadable FreeSWITCH module functions declaration
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_opal_runtime);
SWITCH_MODULE_DEFINITION(mod_opal, mod_opal_load, mod_opal_shutdown, mod_opal_runtime);

/*
 * This function is called on module load
 * It sets up frontend interface to FS
 *
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load)
{
       
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Starting loading mod_opal\n");
    opal_pool = pool;
    /* frontend initialization*/
    *module_interface =NULL;
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    
    assert(*module_interface);
    if(!module_interface)
    {     
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Can not create loadable module interfacer\n");
        return SWITCH_STATUS_MEMERR;
    }
    opalh323_endpoint_interface = (switch_endpoint_interface_t*)switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
    opalh323_endpoint_interface->interface_name = "OpalH323";
    opalh323_endpoint_interface->io_routines = &opalh323_io_routines;
    opalh323_endpoint_interface->state_handler = &opalh323_event_handlers;
       
    return SWITCH_STATUS_SUCCESS;
}

/*
 * This function is called after module initilization
 * It sets up and run backend interface to opal
 *
 */

SWITCH_MODULE_RUNTIME_FUNCTION(mod_opal_runtime)
{
    OpalStartProcess::createInstance(modname,opal_pool,opalh323_endpoint_interface);    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Opal runtime fun exit\n");
    return SWITCH_STATUS_TERM;
}

/*
 * This functionis called on module teardown
 * It releases all internal resources, i.e. 
 * it dealocates OPAL core
 * 
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown)
{
    /* deallocate OPAL manager */
    
    OpalStartProcess::getInstance()->StopProcess(); /* terminate process */    
    while(OpalStartProcess::checkInstanceExists())
    {
        switch_yield(1000); /* wait 1s in each loop */
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Opal shutdown succesfully\n");
    return SWITCH_STATUS_SUCCESS;
}

/*
 * IO routines handlers definitions
 */
static switch_call_cause_t opalh323_outgoing_channel(
        switch_core_session_t *session,
	switch_caller_profile_t *outbound_profile,
        switch_core_session_t **new_session, 
        switch_memory_pool_t **pool)
{
     return  OpalStartProcess::getInstance()->getManager()->io_outgoing_channel(session,outbound_profile,new_session,pool);
}

static switch_status_t opalh323_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flags, int stream_id)
{
    return OpalStartProcess::getInstance()->getManager()->io_read_frame(session,frame,timeout,flags,stream_id);
}

static switch_status_t opalh323_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id)
{
    return OpalStartProcess::getInstance()->getManager()->io_write_frame(session,frame,timeout,flags,stream_id);
}

static switch_status_t opalh323_kill_channel(switch_core_session_t *session, int sig)
{
    return OpalStartProcess::getInstance()->getManager()->io_kill_channel(session,sig);
}

static switch_status_t opalh323_waitfor_read(switch_core_session_t *session, int ms, int stream_id)
{
    return OpalStartProcess::getInstance()->getManager()->io_waitfor_read(session,ms,stream_id);
}

static switch_status_t opalh323_waitfor_write(switch_core_session_t *session, int ms, int stream_id)
{
    return OpalStartProcess::getInstance()->getManager()->io_waitfor_write(session,ms,stream_id);
}

static switch_status_t opalh323_send_dtmf(switch_core_session_t *session, char *dtmf)
{
    return OpalStartProcess::getInstance()->getManager()->io_send_dtmf(session,dtmf);
}

static switch_status_t opalh323_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
    return OpalStartProcess::getInstance()->getManager()->io_receive_message(session,msg);
}

static switch_status_t opalh323_receive_event(switch_core_session_t *session, switch_event_t *event)
{
    return OpalStartProcess::getInstance()->getManager()->io_receive_event(session,event);
}

static switch_status_t opalh323_state_change(switch_core_session_t *session)
{
    return OpalStartProcess::getInstance()->getManager()->io_state_change(session);
}

static switch_status_t opalh323_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flag, int stream_id)
{
    return OpalStartProcess::getInstance()->getManager()->io_read_video_frame(session,frame,timeout,flag,stream_id);
}

static switch_status_t opalh323_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flag, int stream_id)
{
    return OpalStartProcess::getInstance()->getManager()->io_write_video_frame(session,frame,timeout,flag,stream_id);
}

/*
 * Event handlers
 */

static switch_status_t opalh323_on_init(switch_core_session_t *session)
{
    return OpalStartProcess::getInstance()->getManager()->callback_on_init(session);
}

static switch_status_t opalh323_on_ring(switch_core_session_t *session)
{
    return OpalStartProcess::getInstance()->getManager()->callback_on_ring(session);
}

static switch_status_t opalh323_on_execute(switch_core_session_t *session)
{
    return OpalStartProcess::getInstance()->getManager()->callback_on_execute(session);
}

static switch_status_t opalh323_on_hangup(switch_core_session_t *session)
{
    return OpalStartProcess::getInstance()->getManager()->callback_on_hangup(session);
}

static switch_status_t opalh323_on_loopback(switch_core_session_t *session)
{
    return OpalStartProcess::getInstance()->getManager()->callback_on_loopback(session);
}

static switch_status_t opalh323_on_transmit(switch_core_session_t *session)
{
    return OpalStartProcess::getInstance()->getManager()->callback_on_transmit(session);
}
