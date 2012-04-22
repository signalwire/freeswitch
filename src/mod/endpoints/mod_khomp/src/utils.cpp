/*******************************************************************************

    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2010 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License 
  Version 1.1 (the "License"); you may not use this file except in compliance 
  with the License. You may obtain a copy of the License at 
  http://www.mozilla.org/MPL/ 

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file 
  under the MPL, indicate your decision by deleting the provisions above and 
  replace them with the notice and other provisions required by the LGPL 
  License. If you do not delete the provisions above, a recipient may use your 
  version of this file under either the MPL or the LGPL License.

  The LGPL header follows below:

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation, 
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*******************************************************************************/

#include <utils.h>
#include "khomp_pvt.h"

void Kflags::init()
{
    _flags[CONNECTED]._name = "CONNECTED";
    _flags[CONNECTED]._value = false;
    _flags[REALLY_CONNECTED]._name = "REALLY_CONNECTED";
    _flags[REALLY_CONNECTED]._value = false;
    _flags[IS_OUTGOING]._name = "IS_OUTGOING";
    _flags[IS_OUTGOING]._value = false;
    _flags[IS_INCOMING]._name = "IS_INCOMING";
    _flags[IS_INCOMING]._value = false;
    _flags[STREAM_UP]._name = "STREAM_UP";
    _flags[STREAM_UP]._value = false;
    _flags[LISTEN_UP]._name = "LISTEN_UP";
    _flags[LISTEN_UP]._value = false;
    _flags[GEN_CO_RING]._name = "GEN_CO_RING";
    _flags[GEN_CO_RING]._value = false;
    _flags[GEN_PBX_RING]._name = "GEN_PBX_RING";
    _flags[GEN_PBX_RING]._value = false;
    _flags[HAS_PRE_AUDIO]._name = "HAS_PRE_AUDIO";
    _flags[HAS_PRE_AUDIO]._value = false;
    _flags[HAS_CALL_FAIL]._name = "HAS_CALL_FAIL";
    _flags[HAS_CALL_FAIL]._value = false;
    _flags[DROP_COLLECT]._name = "DROP_COLLECT";
    _flags[DROP_COLLECT]._value = false;
    _flags[NEEDS_RINGBACK_CMD]._name = "NEEDS_RINGBACK_CMD";
    _flags[NEEDS_RINGBACK_CMD]._value = false;
    _flags[EARLY_RINGBACK]._name = "EARLY_RINGBACK";
    _flags[EARLY_RINGBACK]._value = false;
    _flags[FAX_DETECTED]._name = "FAX_DETECTED";
    _flags[FAX_DETECTED]._value = false;
    _flags[FAX_SENDING]._name = "FAX_SENDING";
    _flags[FAX_SENDING]._value = false;
    _flags[FAX_RECEIVING]._name = "FAX_RECEIVING";
    _flags[FAX_RECEIVING]._value = false;
    _flags[OUT_OF_BAND_DTMFS]._name = "OUT_OF_BAND_DTMFS";
    _flags[OUT_OF_BAND_DTMFS]._value = false;
    _flags[KEEP_DTMF_SUPPRESSION]._name = "KEEP_DTMF_SUPPRESSION";
    _flags[KEEP_DTMF_SUPPRESSION]._value = false;
    _flags[KEEP_ECHO_CANCELLATION]._name = "KEEP_ECHO_CANCELLATION";
    _flags[KEEP_ECHO_CANCELLATION]._value = false;
    _flags[KEEP_AUTO_GAIN_CONTROL]._name = "KEEP_AUTO_GAIN_CONTROL";
    _flags[KEEP_AUTO_GAIN_CONTROL]._value = false;
    _flags[WAIT_SEND_DTMF]._name = "WAIT_SEND_DTMF";
    _flags[WAIT_SEND_DTMF]._value = false;
    _flags[CALL_WAIT_SEIZE]._name = "CALL_WAIT_SEIZE";
    _flags[CALL_WAIT_SEIZE]._value = false;
    _flags[NUMBER_DIAL_FINISHD]._name = "NUMBER_DIAL_FINISHD";
    _flags[NUMBER_DIAL_FINISHD]._value = false;
    _flags[NUMBER_DIAL_ONGOING]._name = "NUMBER_DIAL_ONGOING";
    _flags[NUMBER_DIAL_ONGOING]._value = false;

    _flags[FXS_OFFHOOK]._name = "FXS_OFFHOOK";
    _flags[FXS_OFFHOOK]._value = false;
    _flags[FXS_DIAL_FINISHD]._name = "FXS_DIAL_FINISHD";
    _flags[FXS_DIAL_FINISHD]._value = false;
    _flags[FXS_DIAL_ONGOING]._name = "FXS_DIAL_ONGOING";
    _flags[FXS_DIAL_ONGOING]._value = false;
    _flags[FXS_FLASH_TRANSFER]._name = "FXS_FLASH_TRANSFER";
    _flags[FXS_FLASH_TRANSFER]._value = false;
    _flags[XFER_QSIG_DIALING]._name = "XFER_QSIG_DIALING";
    _flags[XFER_QSIG_DIALING]._value = false;
    _flags[XFER_DIALING]._name = "XFER_DIALING";
    _flags[XFER_DIALING]._value = false;

    _flags[SMS_DOING_UPLOAD]._name = "SMS_DOING_UPLOAD";
    _flags[SMS_DOING_UPLOAD]._value = false;

    /*
    NOW LOADING ... 

    _flags[BRIDGED]._name = "BRIDGED";
    _flags[BRIDGED]._value = false;
    */

    _flags[INVALID_FLAG]._name = "INVALID_FLAG";
    _flags[INVALID_FLAG]._value = false;
}


/* Command */

bool ChanCommandHandler::writeNoSignal(const CommandRequest & cmd)
{
    _fifo->_mutex.lock();
    bool status = _fifo->_buffer.provide(cmd);
    _fifo->_mutex.unlock();
    return status;
};

bool ChanCommandHandler::write(const CommandRequest & cmd)
{
    bool status = writeNoSignal(cmd);

    if (status) signal();

    return status;
};

void ChanCommandHandler::unreference()
{
    
    if (!_fifo)
        return;
    
    if(_fifo->_thread)
    {
        _fifo->_thread->join();
        delete _fifo->_thread;
        _fifo->_thread = NULL;
    }

    delete _fifo;
    _fifo = NULL;
};


/* Event */

bool ChanEventHandler::provide(const EventRequest & evt)
{
    _fifo->_mutex.lock();
    bool status = _fifo->_buffer.provide(evt);
    _fifo->_mutex.unlock();
    return status;
};

bool ChanEventHandler::writeNoSignal(const EventRequest & evt)
{
    bool status = true;
    _fifo->_mutex.lock();

    try
    {
        _fifo->_buffer.provider_start().mirror(evt);
        _fifo->_buffer.provider_commit();
    }
    catch(...) //BufferFull & e
    {
        status = false;
    }

    _fifo->_mutex.unlock();
    return status;
};

bool ChanEventHandler::write(const EventRequest & evt)
{
    bool status = writeNoSignal(evt);

    if (status) signal();

    return status;
};

void ChanEventHandler::unreference()
{
    
    if (!_fifo)
        return;
    
    if(_fifo->_thread)
    {
        _fifo->_thread->join();
        delete _fifo->_thread;
        _fifo->_thread = NULL;
    }

    delete _fifo;
    _fifo = NULL;
};

const char * answerInfoToString(int answer_info)
{
    switch (answer_info)
    {    
        case Board::KhompPvt::CI_MESSAGE_BOX:
            return "MessageBox";
        case Board::KhompPvt::CI_HUMAN_ANSWER:
            return "HumanAnswer";
        case Board::KhompPvt::CI_ANSWERING_MACHINE:
            return "AnsweringMachine";
        case Board::KhompPvt::CI_CARRIER_MESSAGE:
            return "CarrierMessage";
        case Board::KhompPvt::CI_UNKNOWN:
            return "Unknown";
        case Board::KhompPvt::CI_FAX:
            return "Fax";
    }    

    return NULL;
}
/******************************* Match functions ******************************/
bool MatchExtension::canMatch(std::string & context, std::string & exten, 
                              std::string & caller_id, bool match_more)
{
    if(!match_more)
    {
        return true;
    }

    if (Opt::_options._fxs_sharp_dial())
    {
        char key_digit  = '#';
        size_t finished = exten.find_last_of(key_digit);
        char last_char  = exten.at(exten.size() - 1);
    
        if(finished != std::string::npos && last_char == key_digit)
        {
            if(exten.size() <= 1)
            {
                DBG(FUNC, FMT("exten=%s size=%d") % exten % exten.size());
                return true;
            }

            exten.erase(finished);
            DBG(FUNC, FMT("match exact!!! exten=%s") % exten);
            return false;
        }
    }
    
    return true;

/*
    switch_xml_t xml = NULL;
    switch_xml_t xcontext = NULL;
    switch_regex_t *re;
    int ovector[30];

    if (switch_xml_locate("dialplan","context","name",context.c_str(),&xml,&xcontext, NULL,SWITCH_FALSE) == SWITCH_STATUS_SUCCESS)
    {
        switch_xml_t xexten = NULL;

        if(!(xexten = switch_xml_child(xcontext,"extension")))
        {
            DBG(FUNC,"extension cannot match, returning");

            if(xml)
                switch_xml_free(xml); 

            return false;
        }

        while(xexten)
        {
            switch_xml_t xcond = NULL;

            for (xcond = switch_xml_child(xexten, "condition"); xcond; xcond = xcond->next)
            {
                std::string expression;

                if (switch_xml_child(xcond, "condition")) 
                { 
                    LOG(ERROR,"Nested conditions are not allowed");
                }  

                switch_xml_t xexpression = switch_xml_child(xcond, "expression");

                if ((xexpression = switch_xml_child(xcond, "expression"))) 
                {
                    expression = switch_str_nil(xexpression->txt);
                }
                else 
                {
                    expression =  switch_xml_attr_soft(xcond, "expression");
                }  

                if(expression.empty() || expression == "^(.*)$")
                {                    
                    //We're not gonna take it
                    //No, we ain't gonna take it
                    // We're not gonna take it anymore
                    //
                    continue;
                }

                int pm = -1; 
                switch_status_t is_match = SWITCH_STATUS_FALSE;
                is_match =  switch_regex_match_partial(exten.c_str(),expression.c_str(),&pm);

                if(is_match == SWITCH_STATUS_SUCCESS)
                {
                    if(match_more)
                    {
                        if(pm == 1)
                        {
                            switch_xml_free(xml);
                            return true;
                        }
                    }
                    else
                    {
                        switch_xml_free(xml);
                        return true;
                    }
                }
                else
                {
                    // not match
                }
            }            

            xexten = xexten->next;
        }
    }
    else
    {
        DBG(FUNC,"context cannot match, returning");
    }

    if(xml)
        switch_xml_free(xml); 

    return false;
*/
}


MatchExtension::MatchType MatchExtension::matchExtension(
                                        std::string & context, 
                                        std::string & exten,
                                        std::string & callerid, 
                                        bool match_only)
{
    if(!canMatch(context,exten,callerid))
    {
        DBG(FUNC, "context/extension cannot match");
        return MATCH_NONE;
    }

    if(match_only)
    {
        DBG(FUNC, "for now we want know if it matches...");
        return MATCH_MORE;
    }

    if(!canMatch(context,exten,callerid,true))
    {
        DBG(FUNC, "it match exact!");
        return MATCH_EXACT;
    }

    return MATCH_MORE;
}

MatchExtension::MatchType MatchExtension::findExtension(
                                        std::string & ref_extension,
                                        std::string & ref_context,
                                        ContextListType & contexts,
                                        std::string & extension,
                                        std::string & caller_id,
                                        bool default_ctx,
                                        bool default_ext)
{
    ExtenListType extens;

    if(!extension.empty())
    {
        extens.push_back(extension);
    }

    if(default_ext)
    {
        if (extension != "s")
        {
            extens.push_back("s");
        }

        extens.push_back("i");
    }

    if(default_ctx)
    {
        contexts.push_back("default");
    }

    for(ContextListType::iterator itc = contexts.begin(); itc != contexts.end(); itc++)
    {
        for(ExtenListType::iterator ite = extens.begin(); ite != extens.end(); ite++)
        {
            DBG(FUNC, FMT("trying context '%s' with exten '%s'...") % *itc % *ite);
            ref_context   = *itc;
            ref_extension = *ite;

            MatchType m = matchExtension(ref_context, ref_extension, caller_id, false);

            switch (m)
            {
                case MATCH_NONE:
                    continue;

                case MATCH_MORE:
                case MATCH_EXACT:
                {
                    //ref_context = *itc;
                    //ref_extension = *ite;

                    DBG(FUNC, ".... can match context/extension (some way)!");

                    return m;
                }
            }
        }
    }
            
    ref_context.clear();
    ref_extension.clear();

    DBG(FUNC, D("... no context/extension found!"));
    return MATCH_NONE;
}

/******************************** Kommuter ************************************/
bool Kommuter::stop()
{
    if(_kwtd_timer_on)
    {
        Globals::global_timer->del(_kwtd_timer_index);
        _kwtd_timer_on = false;
    }

    /* stop all watches */
    if (Opt::_options._kommuter_activation() == "auto")
    {
        for (int kommuter = 0 ; kommuter < _kommuter_count ; kommuter++)
        {
            try
            {
                Globals::k3lapi.command(-1, kommuter, CM_STOP_WATCHDOG);
                LOG(MESSAGE, FMT("kommuter device (%d) was stoped at finalize_module().") % kommuter);
            }
            catch(K3LAPI::failed_command & e)
            {
                LOG(ERROR, FMT("could not stop the Kommuter device (%d) at finalize_module().") % kommuter);
            }

        }
    }

    return true;
}

bool Kommuter::initialize(K3L_EVENT *e)
{
    /* get total of kommuter devices */
    _kommuter_count = e->AddInfo;
   
    if(Opt::_options._kommuter_activation() == "manual")
    {
        if (_kommuter_count > 0)
        {
            LOG(WARNING, "Kommuter devices were found on your system, but activation is set to manual. To activate this devices use the command 'khomp kommuter on'.");
        }

        return true;
    }

    if (_kommuter_count > 0) 
    {    
        bool start_timer = false;
        int timeout = Opt::_options._kommuter_timeout();

        std::string param = STG(FMT("timeout=%d") % timeout);

        for (int kommuter = 0; kommuter < _kommuter_count; kommuter++)
        {   
            try
            {
                Globals::k3lapi.command(-1, kommuter, CM_START_WATCHDOG, param.c_str());
                start_timer = true;
            }
            catch(K3LAPI::failed_command & e)
            {
                switch(e.rc)
                {
                case ksInvalidParams:
                    LOG(ERROR, FMT("invalid timeout '%d' for Kommuter device '%d' timeout. Mininum is 0, maximum is 255.")
                                % timeout % kommuter);
                    break;
                default:
                    LOG(ERROR, FMT("could not start the kommuter device number '%d'.") % kommuter);
                    break;

                }
            }
            catch(...)
            {
                LOG(ERROR, FMT("could not start the kommuter device number '%d'.") % kommuter);
            }
        }

        if(timeout == 0)
        {    
            DBG(FUNC, D("kommuter watchdog timer not created because timeout is 0."));
            return true;
        }    

        if (start_timer)
        {    
            if (!Globals::global_timer)
            {    
                LOG(ERROR, D("error creating the timer for kommuter."));
            }
            else
            {
                _kwtd_timer_index = Globals::global_timer->add((timeout < 5 ? (timeout*500) : 2000), &wtdKickTimer);
                _kwtd_timer_on = true;

                DBG(FUNC, D("kommuter watchdog timer created and started."));
            }
        }
    }
    else
    {
        DBG(FUNC, D("no kommuter devices were found on system."));
    }
 
    return true;
}

void Kommuter::wtdKickTimer(void *)
{
    try
    {
        bool restart_timer = false;

        for (int kommuter = 0; kommuter < Board::kommuter._kommuter_count; kommuter++)
        {
            try
            {
                Globals::k3lapi.command(-1, kommuter, CM_NOTIFY_WATCHDOG);
                restart_timer = true;
                DBG(FUNC, D("Kommuter device (%d) notified.") % kommuter);
            }
            catch(K3LAPI::failed_command & e)
            {
                switch(e.rc)
                {
                    case ksInvalidState:
                        LOG(ERROR, FMT("Kommuter device '%d' was not initialized.") % kommuter);
                        break;
                    case ksNotAvailable:
                        LOG(ERROR, FMT("Kommuter device '%d' not found.") % kommuter);
                        break;
                    case ksFail:
                        LOG(ERROR, FMT("Kommuter notify command has failed for device '%d'.") % kommuter);
                        break;
                    default:
                        LOG(ERROR, FMT("Kommuter device '%d' could not be notified for some unknow reason.") % kommuter);
                        break;
                }
            }
        }

        if(restart_timer)
        {
            Globals::global_timer->restart( Board::kommuter._kwtd_timer_index, true );
            DBG(FUNC, D("Kommuter timer restarted."));
        }
    }
    catch (...)
    {
        return;
    }
}

/************************************ ESLs **********************************/
ESL::VectorEvents * ESL::_events = NULL;

ESL::ESL(std::string type) : _type(type)
{
    if(!_events)
        _events = new VectorEvents();

//    _events->push_back(_type);
}

ESL::~ESL()
{
    if(!_events)
        return;

    //Remove one from vector
    //_events->pop_back();
    
    if(_events->size() == 0)
    {
        delete _events;
        _events = NULL;
    }
}

bool ESL::registerEvents()
{
    bool ok = true;

    DBG(FUNC, "Register ESLs");

    if(!_events)
        return true;

    for(VectorEvents::const_iterator event = _events->begin(); event != _events->end(); event++)
    {
        if (switch_event_reserve_subclass((*(event)).c_str()) != SWITCH_STATUS_SUCCESS)
        {
            LOG(ERROR, FMT("Couldn't register subclass=\"%s\"") % *(event));
            ok = false;
        }
        else
        {
            DBG(FUNC, FMT("Register subclass=\"%s\"") % *(event));
        }
    }

    return ok;
}

bool ESL::unregisterEvents()
{
    DBG(FUNC, "Unregister ESLs");
    
    if(!_events)
        return true;

    for(VectorEvents::const_iterator event = _events->begin(); event != _events->end(); event++)
    {
        DBG(FUNC, FMT("Unregister subclass=\"%s\"") % *(event));
        switch_event_free_subclass((*(event)).c_str());
    }

    return true;
}



/******************************************************************************/

