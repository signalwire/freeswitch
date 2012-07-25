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

#ifndef _APPLICATIONS_H_
#define _APPLICATIONS_H_

#include "lock.h"
#include "khomp_pvt.h"

struct Application 
{
    Application(Board::KhompPvt * pvt) : _pvt(pvt) {}
    ~Application() {}

    Statistics * statistics() { return _app_statistics; }

    template <typename T>
    T* statistics() { return static_cast<T*>(_app_statistics); }

    Board::KhompPvt *_pvt;
    Statistics      *_app_statistics;
};

/*************************** FAX **********************************************/
struct Fax
{
    Fax(Board::KhompPvt * pvt) : _pvt(pvt) {}

    /*
    bool clear(Board::KhompPvt * pvt)
    {
        _pvt = pvt;
        return true;
    }
    */

    bool adjustForFax();    

    bool sendFax(switch_core_session_t * session, const char *data);
    bool receiveFax(switch_core_session_t * session, const char *data);

    bool onFaxChannelRelease(K3L_EVENT *e);

    bool startFaxTX(const char * orig_addr = NULL);
    bool stopFaxTX();
    bool startFaxRX(const char * filename, const char * orig_addr = NULL);
    bool stopFaxRX();
    bool addFaxFile(const char * filename, bool last = true);


    Board::KhompPvt *_pvt;

    /* used by app FAX */
    SavedCondition _fax_cond;
    KFaxResult     _fax_result;

};

/*************************** TRANSFER *****************************************/
template <typename T, bool flash = true>
struct Transfer
{
    Transfer(Board::KhompPvt * pvt) : _pvt(pvt), _is_ok(false) {}
    
    bool clear()
    {
        if(!_is_ok)
        {
            _call = dynamic_cast<T *>(_pvt->call());

            _is_ok = true;

            if(!_call)
            {
                DBG(FUNC, D("Error in cast"));
                _is_ok = false;
            }
        }

        _call->_flags.clear(Kflags::XFER_DIALING);

        return true;
    }

    bool userTransfer(switch_core_session_t * session, const char *data)
    {
        DBG(FUNC, PVT_FMT(_pvt->target(), "c"));

        std::string dest("");
        std::string opts("");

        try
        {
            Strings::vector_type params;

            Strings::tokenize((const char *)data, params, "|,", 2);

            dest = params[0];

            if (params.size() > 1)
            {
                // other options go here...
            }
            
            ScopedPvtLock lock(_pvt);

            int timeout = 5;

            if(!_pvt->call()->_flags.check(Kflags::REALLY_CONNECTED) && !_pvt->loopWhileFlagTimed(Kflags::REALLY_CONNECTED, timeout, false))
                return false;

            DBG(FUNC, PVT_FMT(_pvt->target(), "flashing channel!"));

            _pvt->command(KHOMP_LOG, CM_FLASH);

            lock.unlock();

            timeout = 15; // 15 * 200000 = 3s

            do
            {
                usleep(200000);
                timeout--;

                ScopedPvtLock lock2(_pvt);

                if(!_pvt->call()->_flags.check(Kflags::IS_INCOMING) && !_pvt->call()->_flags.check(Kflags::IS_OUTGOING))
                {
                    DBG(FUNC, PVT_FMT(_pvt->target(), "unable to do a user transfer, channel disconnected"));
                    return false;
                }

            }
            while(timeout);

            ScopedPvtLock lock3(_pvt);

            _pvt->command(KHOMP_LOG, CM_DIAL_DTMF, dest.c_str());

            _pvt->call()->_flags.set(Kflags::WAIT_SEND_DTMF);

            lock3.unlock();

            timeout = 300; // 300 * 200000 = 60s

            do
            {
                usleep(200000);
                timeout--;

                ScopedPvtLock lock4(_pvt);

                if(!_pvt->call()->_flags.check(Kflags::WAIT_SEND_DTMF))
                    break;
            }
            while(timeout);

        }
        catch (ScopedLockFailed & err)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(),"r (unable to lock %s!)") % err._msg.c_str() );
            return false;
        }


        DBG(FUNC, PVT_FMT(_pvt->target(), "r"));
        return true;
    }

    /* User transfer functions */
    bool doUserXferUnlocked()
    {
        DBG(FUNC, PVT_FMT(_pvt->target(), "c (flashing channel!)"));

        bool ret = false;

        ret = _pvt->command(KHOMP_LOG, CM_FLASH);

        DBG(FUNC, PVT_FMT(_pvt->target(), "r (%s)") % (ret ? "true" : "false"));
        return ret;
    }

    bool checkUserXferUnlocked(std::string digit)
    { 
        DBG(FUNC, PVT_FMT(_pvt->target(), "c (CM_FLASH)"));


        if (_call->_user_xfer_digits.empty())
        {
            _call->_digits_buffer += digit;
            DBG(FUNC, PVT_FMT(_pvt->target(), "r (disabled)"));
            return false;
        }

        _call->_user_xfer_buffer += digit;

        /* temporary buffer */
        std::string tmp = _call->_user_xfer_buffer;

        unsigned int amount = tmp.size();
        
        try
        {

        if (amount == _call->_user_xfer_digits.size())
        {
            if (tmp == _call->_user_xfer_digits)
            {
                bool ret = doUserXferUnlocked();

                _call->_user_xfer_buffer.clear();
                _call->_digits_buffer.clear();

                Board::board(_pvt->target().device)->_timers.del(_idx_xfer_dial);

                DBG(FUNC, PVT_FMT(_pvt->target(), "r (ret=%s, done xfer)") % (ret ? "true" : "false"));
                return ret;
            }

            _call->_digits_buffer += tmp[0];
            _call->_user_xfer_buffer.erase(0, 1);
            DBG(FUNC, PVT_FMT(_pvt->target(), "r (false, no xfer)"));
            return false;
        }

        if (tmp == _call->_user_xfer_digits.substr(0,amount))
        {
            if (!(_call->_flags.check(Kflags::XFER_DIALING)))
            {
                _call->_flags.set(Kflags::XFER_DIALING);
                _idx_xfer_dial = Board::board(_pvt->target().device)->_timers.add(Opt::_options._transferdigittimeout(), &userXferTimer, _pvt, TM_VAL_CALL);
            }
            else
            {
                Board::board(_pvt->target().device)->_timers.restart(_idx_xfer_dial);
            }

            DBG(FUNC, PVT_FMT(_pvt->target(), "r (true, buffering)"));
            return true;
        }

        }
        catch (K3LAPITraits::invalid_device & err)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "Unable to get device: %d!") % err.device);
        }

        _call->_digits_buffer += tmp[0];
        _call->_user_xfer_buffer.erase(0, 1);
        DBG(FUNC, PVT_FMT(_pvt->target(), "r (false, buffering)"));

        return false;

    }

    static void userXferTimer(Board::KhompPvt * pvt)
    {
        DBG(FUNC, PVT_FMT(pvt->target(), "c"));
        
        T * call = static_cast<T *>(pvt->call());

        try
        {
            ScopedPvtLock lock(pvt);

            if (!call->_user_xfer_buffer.empty())
            {
                pvt->command(KHOMP_LOG, CM_DIAL_DTMF, call->_user_xfer_buffer.c_str());

                /* clear the buffer that has been send */
                call->_user_xfer_buffer.clear();
            }

            call->_flags.clear(Kflags::XFER_DIALING);
        }
        catch (ScopedLockFailed & err)
        {
            LOG(ERROR, PVT_FMT(pvt->target(),"r (unable to lock %s!)") % err._msg.c_str() );
            return;
        }

        DBG(FUNC, PVT_FMT(pvt->target(), "r"));
    }

    bool                    _is_ok;
    T *                     _call;
    Board::KhompPvt *       _pvt;
    Board::ChanTimer::Index _idx_xfer_dial;
};

template<typename T> 
struct Transfer<T, false>
{
    Transfer(Board::KhompPvt * pvt) : _pvt(pvt), _is_ok(false) {}

    bool clear()
    {
        if(!_is_ok)
        {
            _call = dynamic_cast<T *>(_pvt->call());

            _is_ok = true;

            if(!_call)
            {
                DBG(FUNC, D("Error in cast"));
                _is_ok = false;
            }
        }

        _call->_flags.clear(Kflags::XFER_DIALING);
        _call->_flags.clear(Kflags::XFER_QSIG_DIALING);

        return true;
    }
    
    bool userTransfer(switch_core_session_t * session, const char *data)
    {
        DBG(FUNC, PVT_FMT(_pvt->target(), "c"));

        std::string dest("");

        bool opt_nowait = false;

        try
        {
            Strings::vector_type params;

            Strings::tokenize((const char *)data, params, "|,", 2);

            dest = params[0];

            if (params.size() > 1)
            {
                opt_nowait = (params[1].find('n') != std::string::npos);

                // other options go here...
            }
            
            ScopedPvtLock lock(_pvt);

            int timeout = 5;
            
            if(!_pvt->call()->_flags.check(Kflags::REALLY_CONNECTED) && !_pvt->loopWhileFlagTimed(Kflags::REALLY_CONNECTED, timeout, false))
                return false;

            DBG(FUNC, PVT_FMT(_pvt->target(), "ss_transfer on channel!"));

            _pvt->command(KHOMP_LOG, CM_SS_TRANSFER,
                    STG(FMT("transferred_to=\"%s\" await_connect=\"%d\"")
                    % dest % (opt_nowait ? 0 : 1)).c_str());

        }
        catch (ScopedLockFailed & err)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(),"r (unable to lock %s!)") % err._msg.c_str() );
            return false;
        }


        DBG(FUNC, PVT_FMT(_pvt->target(), "r"));
        return true;
    }

    /* User transfer functions */
    bool doUserXferUnlocked(void)
    {
        DBG(FUNC, PVT_FMT(_pvt->target(), "c"));

        bool ret = false;

        if (_call->_flags.check(Kflags::XFER_QSIG_DIALING))
        {
            DBG(FUNC, PVT_FMT(_pvt->target(), "ss_transfer on channel!"));

            _call->_flags.clear(Kflags::XFER_DIALING);
            _call->_flags.clear(Kflags::XFER_QSIG_DIALING);

            ret = _pvt->command(KHOMP_LOG, CM_SS_TRANSFER,
                    STG(FMT("transferred_to=\"%s\" await_connect=\"1\"") % _call->_qsig_number).c_str());
        }
        else
        {
            DBG(FUNC, PVT_FMT(_pvt->target(), "starting to store digits for ss_transfer..."));
            _call->_flags.set(Kflags::XFER_QSIG_DIALING);

            _xfer_thread = threadCreate(Transfer<T, false>::userXferPlayback,(void*) _pvt);
            _xfer_thread->start();

            ret = true;
        }

        DBG(FUNC, PVT_FMT(_pvt->target(), "r (%s)") % (ret ? "true" : "false"));
        return ret;

    }

    bool checkUserXferUnlocked(std::string digit) 
    { 
        DBG(FUNC, PVT_FMT(_pvt->target(), "c (CM_SS_TRANSFER)"));


        if (_call->_user_xfer_digits.empty())
        {
            _call->_digits_buffer += digit;
            DBG(FUNC, PVT_FMT(_pvt->target(), "r (disabled)"));
            return false;
        }

        _call->_user_xfer_buffer += digit;

        DBG(FUNC, PVT_FMT(_pvt->target(), "c digits=[%s] buffer=[%s]") % _call->_user_xfer_digits % _call->_user_xfer_buffer );

        /* temporary buffer */
        std::string tmp = _call->_user_xfer_buffer;

        unsigned int amount = tmp.size();
        
        try
        {

        if (amount == _call->_user_xfer_digits.size())
        {
            if (tmp == _call->_user_xfer_digits)
            {
                bool ret = doUserXferUnlocked();

                _call->_user_xfer_buffer.clear();
                _call->_qsig_number.clear();
                _call->_digits_buffer.clear();

                if(!_call->_flags.check(Kflags::XFER_QSIG_DIALING))
                {
                    Board::board(_pvt->target().device)->_timers.del(_idx_xfer_dial);

                    DBG(FUNC, PVT_FMT(_pvt->target(), "r (ret=%s, done xfer)") % (ret ? "true" : "false"));
                }
                else
                {
                    Board::board(_pvt->target().device)->_timers.restart(_idx_xfer_dial);
                    DBG(FUNC, PVT_FMT(_pvt->target(), "r (waiting digits for transfer)"));
                }
                return ret;
            }

            if (_call->_flags.check(Kflags::XFER_QSIG_DIALING))
            {
                DBG(FUNC, PVT_FMT(_pvt->target(), "putting digits ('%s') on transfer-to number!") % tmp);

                _call->_qsig_number += tmp[0];
                _call->_user_xfer_buffer.erase(0, 1);
                Board::board(_pvt->target().device)->_timers.restart(_idx_xfer_dial);

                DBG(FUNC, PVT_FMT(_pvt->target(), "r (true, qsig transfer)"));
                return true;
            }

            _call->_digits_buffer += tmp[0];
            _call->_user_xfer_buffer.erase(0, 1);
            DBG(FUNC, PVT_FMT(_pvt->target(), "r (false, no qsig)"));
            return false;
        }

        if (tmp == _call->_user_xfer_digits.substr(0,amount))
        {
            if (!(_call->_flags.check(Kflags::XFER_DIALING) || _call->_flags.check(Kflags::XFER_QSIG_DIALING)))
            {
                _call->_flags.set(Kflags::XFER_DIALING);
                _idx_xfer_dial = Board::board(_pvt->target().device)->_timers.add(Opt::_options._transferdigittimeout(), &userXferTimer, _pvt, TM_VAL_CALL);
            }
            else
            {
                Board::board(_pvt->target().device)->_timers.restart(_idx_xfer_dial);
            }

            DBG(FUNC, PVT_FMT(_pvt->target(), "r (true, buffering)"));
            return true;
        }

        if (_call->_flags.check(Kflags::XFER_QSIG_DIALING))
        {
            DBG(FUNC, PVT_FMT(_pvt->target(), "putting digits ('%s') on transfer-to number!") % tmp);

            _call->_qsig_number += tmp[0];
            _call->_user_xfer_buffer.erase(0, 1);

            Board::board(_pvt->target().device)->_timers.restart(_idx_xfer_dial);
            DBG(FUNC, PVT_FMT(_pvt->target(), "r (true, qsig buffering)"));
            return true;
        }
        
        }
        catch (K3LAPITraits::invalid_device & err)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "Unable to get device: %d!") % err.device);
        }

        _call->_digits_buffer += tmp[0];
        _call->_user_xfer_buffer.erase(0, 1);
        DBG(FUNC, PVT_FMT(_pvt->target(), "r (false, buffering)"));

        return false;
    }

    static void userXferTimer(Board::KhompPvt * pvt)
    {
        DBG(FUNC, PVT_FMT(pvt->target(), "c"));

        T * call = static_cast<T *>(pvt->call());

        try
        {
            ScopedPvtLock lock(pvt);

            if (!call->_user_xfer_buffer.empty())
            {
                pvt->command(KHOMP_LOG, CM_DIAL_DTMF, call->_user_xfer_buffer.c_str());

                /* clear the buffer that has been send */
                call->_user_xfer_buffer.clear();
            }

            if (!call->_qsig_number.empty())
            {
                pvt->command(KHOMP_LOG, CM_SS_TRANSFER,
                        STG(FMT("transferred_to=\"%s\" await_connect=\"1\"") % call->_qsig_number).c_str());

                /* clear the buffer that has been send */
                call->_qsig_number.clear();
            }

            call->_flags.clear(Kflags::XFER_DIALING);
            call->_flags.clear(Kflags::XFER_QSIG_DIALING);
        }
        catch (ScopedLockFailed & err)
        {
            LOG(ERROR, PVT_FMT(pvt->target(),"r (unable to lock %s!)") % err._msg.c_str() );
            return;
        }

        DBG(FUNC, PVT_FMT(pvt->target(), "r"));
    }

    static switch_status_t dtmfCallback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
    {
        char sbuf[3];

        if(!session)
        {
            DBG(FUNC,D("session is NULL"))
                return SWITCH_STATUS_FALSE;
        }

        switch_channel_t * chan = switch_core_session_get_channel(session);

        if(!chan)
        {
            DBG(FUNC,D("channel is NULL"))
                return SWITCH_STATUS_FALSE;
        }

        switch_core_session_t *peer_session = switch_core_session_locate(switch_channel_get_partner_uuid(chan));

        if(!peer_session)
        {
            DBG(FUNC,D("session is NULL"))
                return SWITCH_STATUS_FALSE;
        }

        switch (itype) 
        {
            case SWITCH_INPUT_TYPE_DTMF:
            {
                switch_dtmf_t *dtmf = (switch_dtmf_t *) input;

                Board::KhompPvt * tech_pvt = static_cast< Board::KhompPvt* >(switch_core_session_get_private(peer_session));
                if(!tech_pvt)
                {
                    DBG(FUNC,D("Init: pvt is NULL"))
                    switch_core_session_rwunlock(peer_session);
                    return SWITCH_STATUS_FALSE;
                }

                char s[] = { dtmf->digit, '\0' };
                tech_pvt->sendDtmf(s); 

                break;
            }        
            default: 
                break;
        }        

        switch_core_session_rwunlock(peer_session);
        return SWITCH_STATUS_SUCCESS;
    }


    static int userXferPlayback(void * pvt_ptr)
    {
        /* get pointer... */
        Board::KhompPvt * pvt = static_cast < Board::KhompPvt * > (pvt_ptr);

        DBG(FUNC, PVT_FMT(pvt->target(), "c"));

        try
        {
            ScopedPvtLock lock(pvt);

            /* get the owner */
            switch_channel_t * chan = pvt->getFSChannel();

            /* get other side of the bridge */
            switch_core_session_t * peer_session = NULL;
            switch_core_session_get_partner(pvt->session(),&peer_session);

            if(!peer_session)
            {
                DBG(FUNC, PVT_FMT(pvt->target(), "r (session is null)"));
                return NULL;
            }

            switch_channel_t * peer = switch_core_session_get_channel(peer_session);

            /* put the channel in hold */
            //switch_core_session_t *session = switch_core_session_locate(switch_channel_get_partner_uuid(chan));
            //switch_channel_t *chan_core = switch_core_session_get_channel(session);

            const char *stream;

            if (!(stream = switch_channel_get_variable(chan, SWITCH_HOLD_MUSIC_VARIABLE)))
            {
                stream = "silence";
            }

            DBG(FUNC, PVT_FMT(pvt->target(), "stream=%s") % stream);

            if (stream && strcasecmp(stream, "silence"))
            {
                /* Freeswitch not get/put frames */
                //switch_channel_set_flag(channel, CF_HOLD);
                switch_ivr_broadcast(switch_core_session_get_uuid(pvt->session()),stream, SMF_ECHO_ALEG | SMF_LOOP | SMF_PRIORITY);
            }

            switch_core_session_rwunlock(peer_session);
            lock.unlock();

            /* kickstart my heart */
            switch_input_args_t args = {0};
            args.input_callback = dtmfCallback;

            /* wait while xfering... */
            while (true)
            {
                switch_ivr_collect_digits_callback(peer_session,&args,1000,0);
                ScopedPvtLock lock2(pvt);

                if (!pvt->call()->_flags.check(Kflags::XFER_QSIG_DIALING))
                {
                    break;
                }

                lock2.unlock();
            }

            //switch_channel_clear_flag(channel, CF_HOLD);

            switch_channel_stop_broadcast(chan);
            switch_channel_wait_for_flag(chan, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
            switch_core_session_rwunlock(pvt->session());
            switch_core_session_rwunlock(peer_session);

            //switch_ivr_unhold_uuid(switch_channel_get_partner_uuid(chan));
        }
        catch (ScopedLockFailed & err)
        {
            LOG(ERROR, PVT_FMT(pvt->target(),"r (unable to lock %s!)") % err._msg.c_str() );
            return NULL;
        }
        catch (Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(pvt->target(), "r (%s)") % err._msg.c_str() );
            return NULL;
        }

        DBG(FUNC, PVT_FMT(pvt->target(), "r"));

        return NULL;
    }

    bool                    _is_ok;
    T *                     _call;
    Board::KhompPvt *       _pvt;
    Thread *                _xfer_thread;
    Board::ChanTimer::Index _idx_xfer_dial;
};

/*************************** SMS **********************************************/
#define ESL_SMS_RECEIVED "khomp::sms_received"
#define ESL_SMS_SENT     "khomp::sms_sent"

struct SMS : public Application
{
    typedef std::list< switch_core_session_t *> OwnersList;

    struct SMSStatistics : public Statistics
    {
        SMSStatistics():
            _sms_number_incoming(0),
            _sms_number_outgoing(0),
            _sms_number_confirm(0),
            _sms_number_broadcast(0) {};

        void incrementIncoming()
        {
            _sms_number_incoming++;
        }

        void incrementOutgoing()
        {
            _sms_number_outgoing++;
        }

        void incrementConfirm()
        {
            _sms_number_confirm++;
        }

        void incrementBroadcast()
        {
            _sms_number_broadcast++;
        }
        
        std::string getDetailed()
        {
            std::string tmpBuffer;

            tmpBuffer.append(STG(FMT("Number of incoming SMS: \t%d\n")  % _sms_number_incoming));
            tmpBuffer.append(STG(FMT("Number of outgoing SMS: \t%d\n")  % _sms_number_outgoing));
            tmpBuffer.append(STG(FMT("Number of broadcast SMS: \t%d\n") % _sms_number_broadcast));
            tmpBuffer.append(STG(FMT("Number of confirm SMS:  \t%d\n")  % _sms_number_confirm));

            return tmpBuffer;
        }

        void clear()
        {
            _sms_number_incoming  = 0;
            _sms_number_outgoing  = 0;
            _sms_number_confirm   = 0;
            _sms_number_broadcast = 0;
        }

        unsigned int _sms_number_incoming;
        unsigned int _sms_number_outgoing;
        unsigned int _sms_number_confirm;
        unsigned int _sms_number_broadcast;
    };

    SMS(Board::KhompPvt * pvt) : 
    Application(pvt),
    _thread(NULL),
    _shutdown(false),
    _can_receive(false),
    _can_send(false),
    _result(0),
    _mutex(Globals::module_pool),
    _cond(Globals::module_pool),
    _buffer(8)
    {
        _cond.reset();
        _app_statistics = new SMSStatistics();
    }

    ~SMS()
    {
        stop();
        delete _app_statistics;
    }

    struct ReceiveData
    {
        ReceiveData() {};

        ReceiveData(const ReceiveData & o)
        {
            _type    = o._type;
            _from    = o._from;
            _date    = o._date;
            _size    = o._size;
            _coding  = o._coding;
            _serial  = o._serial;
            _id      = o._id;
            _page    = o._page;
            _pages   = o._pages;
            _sc_date = o._sc_date;
            _status  = o._status;
            _body    = o._body;
        };

        void clear(void)
        {
            /* reset data stuff */
            _type.clear();
            _from.clear();
            _date.clear();
            _size.clear();
            _coding.clear();
            _serial.clear();
            _id.clear();
            _page.clear();
            _pages.clear();
            _sc_date.clear();
            _status.clear();
            _body.clear();
        };

        std::string _type;
        std::string _from;
        std::string _date;
        std::string _size;
        std::string _coding;
        std::string _serial;
        std::string _id;
        std::string _page;
        std::string _pages;
        std::string _sc_date;
        std::string _status;
        std::string _body;
    };

    struct SendData
    {
        SendData(): _conf(false) {};

        SendData(const SendData & o)
        {
            _dest    = o._dest;
            _body    = o._body;
            _conf    = o._conf;
        };

        void clear(void)
        {
            /* reset data stuff */
            _dest.clear();
            _body.clear();
            _conf = false;
        };

        std::string _dest;
        std::string _body;
        bool        _conf;
    };

    static struct _SMSEvent : public ESL
    {

        _SMSEvent() : ESL("khomp::sms") 
        {
            if(_events)
            {
                _events->push_back(ESL_SMS_RECEIVED);
                _events->push_back(ESL_SMS_SENT);
            }
        }

        ~_SMSEvent()
        {
            if(_events)
            {
                //Remove two from vector
                _events->pop_back();
                _events->pop_back();
            }
        }
        
        bool operator()(Board::KhompPvt * pvt, ReceiveData & data)
        {
            switch_event_t *event = create(ESL_SMS_RECEIVED);

            if(!event)
            {
                LOG(ERROR, "Cannot create SMS ESL");
                return false;
            }

            add(event, pvt->target());
            add(event, "Type", data._type);
            add(event, "From", data._from);
            add(event, "Date", data._date);
            add(event, "Size", data._size);
            add(event, "Coding", data._coding);
            add(event, "Serial", data._serial);
            add(event, "Id", data._id);
            add(event, "Page", data._page);
            add(event, "Pages", data._pages);
            add(event, "Sc_date", data._sc_date);
            add(event, "Status", data._status);
            add(event, "Body", data._body);

            return fire(&event);
        }
        
        bool operator()(Board::KhompPvt * pvt, SendData & data)
        {
            switch_event_t *event = create(ESL_SMS_SENT);

            if(!event)
            {
                LOG(ERROR, "Cannot create SMS ESL");
                return false;
            }

            add(event, pvt->target());
            add(event, "Dest", data._dest);
            add(event, "Body", data._body);
            add(event, "Confirmation?", (data._conf ? "Yes" : "No"));


            return fire(&event);
        }



    } SMSEvent;

    struct Request
    {
        /* "empty" constructor */
        Request(): _finished(NULL), _cause(NULL) {};

        /* "real" constructor */
        Request(SendData & send_sms, volatile bool * finished, volatile KGsmCallCause * cause)
            : _send_sms(send_sms), _finished(finished), _cause(cause)
        {};

        SendData _send_sms;

        volatile bool          * _finished;
        volatile KGsmCallCause * _cause;
    };

    bool start()
    {
        _pvt->call()->_flags.clear(Kflags::SMS_DOING_UPLOAD);

        _thread = threadCreate(&smsThread, (void*) this);
        _thread->start();
    }

    bool stop()
    {
        if(!_thread)
        {
            return false;
        }

        _shutdown = true;
        _cond.signal();
        _thread->join();
        delete _thread;
        _thread = NULL;

        return true;
    }

    bool justAlloc(unsigned int count = 0);
    bool justStart();

    bool sendSMS(switch_core_session_t * session, const char *data);
    

    bool onNewSMS(K3L_EVENT *e);
    bool onSMSInfo(K3L_EVENT *e);
    bool onSMSData(K3L_EVENT *e);
    bool onSMSSendResult(K3L_EVENT *e);
   
    Thread                                             *_thread;
    bool                                                _shutdown;
    bool                                                _can_receive;
    bool                                                _can_send;
    ReceiveData                                         _got_sms;
    SendData                                            _send_sms;
    int                                                 _result;
    SavedCondition                                      _cond;
    Globals::Mutex                                      _mutex;
    Ringbuffer < SMS::Request >                         _buffer;
    OwnersList                                          _owners;

    static int smsThread(void * sms_ptr);
};

/******************************************************************************/


#endif /* _APPLICATIONS_H_ */

