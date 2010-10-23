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

/******************************** Kommuter ************************************/
bool Kommuter::stop()
{
    if(_kwtd_timer_on)
    {
        Globals::global_timer->del(_kwtd_timer_index);
        _kwtd_timer_on = false;
    }

    /* stop all watches */
    if (Opt::_kommuter_activation == "auto")
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
   
    if(Opt::_kommuter_activation == "manual")
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
        int timeout = Opt::_kommuter_timeout;

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

