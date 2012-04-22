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

#ifndef _KHOMP_PVT_H_
#define _KHOMP_PVT_H_

#include <timer.hpp>
#include "globals.h"
#include "frame.h"
#include "opt.h"
#include "logger.h"
#include "defs.h"

/*!
 \brief Callback generated from K3L API for every new event on the board.
 \param[in] obj Object ID (could be a channel or a board, depends on device type) which generated the event.
 \param[in] e The event itself.
 \return ksSuccess if the event was treated
 \see K3L_EVENT Event specification
 */
extern "C" int32 Kstdcall khompEventCallback (int32, K3L_EVENT *);

/*!
 \brief Callback generated from K3L API everytime audio is available on the board.
 @param[in] deviceid Board on which we get the event
 @param[in] objectid The channel we are getting the audio from
 @param[out] read_buffer The audio buffer itself (RAW)
 @param[in] read_size The buffer size, meaning the amount of data to be read
 \return ksSuccess if the event was treated
 */
extern "C" void Kstdcall khompAudioListener (int32, int32, byte *, int32);

/******************************************************************************/
/********************************* Board **************************************/
/******************************************************************************/
struct Board
{
    /* Timers */
    struct KhompPvt;
    typedef KhompPvt *     ChanTimerData;
    typedef                void (ChanTimerFunc)(ChanTimerData);
    typedef TimerTemplate < ChanTimerFunc, ChanTimerData > ChanTimer;

/******************************************************************************/
/******************************** Channel *************************************/
struct KhompPvt
{
    typedef SimpleNonBlockLock<25,100>      ChanLockType;

    typedef enum
    {
        CI_MESSAGE_BOX        = 0x01,
        CI_HUMAN_ANSWER       = 0x02,
        CI_ANSWERING_MACHINE  = 0x04,
        CI_CARRIER_MESSAGE    = 0x08,
        CI_UNKNOWN            = 0x10,
        CI_FAX                = 0x20,
    }
    CallInfoType;
    
    struct InitFailure {};

    struct InvalidSwitchChannel
    {
        typedef enum { NULL_SWITCH_CHANNEL, NULL_SWITCH_SESSION_PASSED, NULL_SWITCH_VALUE, FAILED } FailType;

        InvalidSwitchChannel(FailType fail, std::string msg)
            : _fail(fail),_msg(msg) {};

        std::string _msg;
        FailType _fail;
    };

    typedef enum
    {
        PLAY_NONE = 0,
        PLAY_VM_TONE,
        PLAY_PBX_TONE,
        PLAY_PUB_TONE,
        PLAY_RINGBACK,
        PLAY_FASTBUSY,
    }
    CadencesType;

    typedef enum
    {
        INDICA_NONE = 0,
        INDICA_RING,
        INDICA_BUSY,
        INDICA_FAST_BUSY,
    }
    IndicationType;
    
    typedef enum
    {
        CLN_HARD,
        CLN_SOFT,
        CLN_FAIL,
    }
    CleanupType;

/********************************** Call **************************************/
    struct Call
    {
        struct CallStatistics : public Statistics
        {
            CallStatistics(Call *call):
                _call(call),
                _total_time_incoming(0),
                _total_time_outgoing(0),
                _total_idle_time(0),
                _channel_fails(0)
            {
                time(&_base_idle_time);            
                time(&_base_time);            
            }

            void idle()
            {
                if (_call->_flags.check(Kflags::IS_INCOMING) || 
                    _call->_flags.check(Kflags::IS_OUTGOING) ||
                    _call->_indication == INDICA_RING ||
                    _call->_indication == INDICA_BUSY)
                {   
                    return;
                }   

                time_t tmp;
                time (&tmp);

                _total_idle_time += (tmp - _base_idle_time);
                time (&_base_idle_time);
            }

            void incrementNewCall()
            {
                time (&_base_time);
            }

            void incrementHangup()
            {
                time_t tmp;
                time (&tmp);

                if (_call->_flags.check(Kflags::IS_OUTGOING))
                {   
                    _total_time_outgoing += (tmp - _base_time);
                    time (&_base_time);
                }   
                else if (_call->_flags.check(Kflags::IS_INCOMING))
                {   
                    _total_time_incoming += (tmp - _base_time);
                    time (&_base_time);
                }   

                time(&_base_idle_time);
            }

            void incrementChannelFail()
            {
                _channel_fails++;
            }

            std::string getDetailed() 
            {
                /* buffer our data to return at the end */
                std::string strBuffer;

                /* very very important yet! */
                idle();

                std::string str_incoming_time = timeToString(_total_time_incoming);
                std::string str_outgoing_time = timeToString(_total_time_outgoing);
                std::string str_idle_time     = timeToString(_total_idle_time);

                strBuffer.append(STG(FMT("Total Incoming Time: \t%s\n") % str_incoming_time));
                strBuffer.append(STG(FMT("Total Outgoing Time: \t%s\n") % str_outgoing_time));
                strBuffer.append(STG(FMT("Total Idle Time: \t%s\n") % str_idle_time));
                strBuffer.append(STG(FMT("Number of channel fails: \t%d\n") % _channel_fails));                

                return strBuffer;
            }

            switch_xml_t getDetailedXML()
            {
                /* very very important yet! */
                idle();

                std::string str_incoming_time = timeToString(_total_time_incoming);
                std::string str_outgoing_time = timeToString(_total_time_outgoing);
                std::string str_idle_time     = timeToString(_total_idle_time);

                /* total */
                switch_xml_t xtotal = switch_xml_new("total");

                /* total/incoming_time */
                switch_xml_t xin_time = switch_xml_add_child_d(xtotal,"incoming_time",0);
                switch_xml_set_txt_d(xin_time, str_incoming_time.c_str());

                /* total/outgoing_time */
                switch_xml_t xout_time = switch_xml_add_child_d(xtotal,"outgoing_time",0);
                switch_xml_set_txt_d(xout_time, str_outgoing_time.c_str());

                /* total/idle_time */
                switch_xml_t xidle_time = switch_xml_add_child_d(xtotal,"idle_time",0);
                switch_xml_set_txt_d(xidle_time, str_idle_time.c_str());

                /* total/channel_fails */
                switch_xml_t xchannel_fails = switch_xml_add_child_d(xtotal,"channel_fails",0);
                switch_xml_set_txt_d(xchannel_fails, STR(FMT("%d") % _channel_fails));

                return xtotal;
            }

            void clear()
            {   
                time(&_base_time);
                time(&_base_idle_time);

                _total_idle_time = 0;
                _channel_fails = 0;

                _total_time_incoming = 0;
                _total_time_outgoing = 0;
            }

            time_t _base_time;
            time_t _total_time_incoming;
            time_t _total_time_outgoing;
            time_t _total_idle_time;
            time_t _base_idle_time;     /* base time for idle time refreshing */
            unsigned int _channel_fails;/* number of channel fails*/

        protected:        
            Call *_call;  /* associate to call, useful */
        };

        Call() 
        {
            clear(); 
            _call_statistics = new CallStatistics(this);
        }

        virtual ~Call() { delete _call_statistics; }

        CallStatistics * statistics() { return _call_statistics; };

        virtual bool process(std::string name, std::string value = "")
        {
            if (name == "pre_answer")
            {
                _pre_answer = true;
            }
            else if (name == "orig")
            {
                DBG(FUNC, FMT("orig addr adjusted (%s).") % value.c_str());
                _orig_addr = value;
            }
            else if (name == "dest")
            {
                _dest_addr = value;
            }

            else if (name == "input_volume" || name == "output_volume")
            {
                try
                {
                    int i = Strings::tolong(value);
    
                    if (i < -10 || i > 10)
                    {
                        LOG(ERROR, FMT("Could not set '%s': '%s' is not a valid number between -10 and 10.") % name % value);
                    }
                    else
                    {
                        DBG(FUNC, FMT("Changing '%s' volume to '%s'.") % name % value);
                        if(name == "input_volume")
                            _input_volume = i;
                        else
                            _output_volume = i;
                    }
                }
                catch (Strings::invalid_value & e)
                {
                    LOG(ERROR, D("invalid numeric value: %s") % e.value());
                }
            }

            else
            {
                return false;
            }

            return true;
        }

        virtual bool clear()
        {

            _orig_addr.clear();
            _dest_addr.clear();
            _incoming_context.clear();
            _queued_digits_buffer.clear();
            _pre_answer = false;
            _is_progress_sent = false;
            _collect_call = false;
            _hangup_cause = 0;
            _cleanup_upon_hangup = false;
            _input_volume = 999;
            _output_volume = 999;

            _var_dtmf_state = T_UNKNOWN;
            _var_echo_state = T_UNKNOWN;
            _var_gain_state = T_UNKNOWN;            
            
            _flags.clearAll();

            _cadence = PLAY_NONE;
            _indication = INDICA_NONE;

            return true;
        }

        /* used while answering calls */
        std::string _orig_addr;
        std::string _dest_addr;

        std::string _incoming_context;

        std::string _queued_digits_buffer;

        /* should freeswitch answer before connect event?  */
        bool _pre_answer;

        bool _is_progress_sent;

        /* is a collect call? */
        bool _collect_call;

        int _hangup_cause;

        bool _cleanup_upon_hangup;

        int _input_volume;
        int _output_volume;

        TriState _var_dtmf_state;
        TriState _var_echo_state;
        TriState _var_gain_state;        

        Kflags _flags;

        CadencesType _cadence;
        IndicationType _indication;
        
        ChanTimer::Index _idx_co_ring;
        ChanTimer::Index _idx_pbx_ring;

        CallStatistics *_call_statistics;
    };
/******************************************************************************/
public:

    /* KhompPvt constructor */
    KhompPvt(K3LAPIBase::GenericTarget & target);

    /* KhompPvt destructor */
    virtual ~KhompPvt()
    {
        delete _pvt_statistics;
        _session = NULL;
    }
   
    struct PvtStatistics : public Statistics
    {
        PvtStatistics(KhompPvt * pvt):
            _pvt(pvt) {}

        std::string getDetailedRates();
        switch_xml_t getDetailedRatesXML();

        std::string getDetailed();
        switch_xml_t getDetailedXML();

        std::string getRow();
        switch_xml_t getNode();


        void clear()
        {
            _pvt->call()->statistics()->clear();
        }
        
        KhompPvt * _pvt;
    };

    /* Virtual Methods */

    /*!
     \defgroup KhompEvents
                Callbacks that boards can implement to produce the expected
                particular behaviour. Refer to Khomp documentation for a
                detailed description of each method.
     */
    /*@{*/
    virtual bool onChannelRelease(K3L_EVENT *);
    virtual bool onNewCall(K3L_EVENT *);
    virtual bool onCallSuccess(K3L_EVENT *);
    virtual bool onCallFail(K3L_EVENT *);
    virtual bool onConnect(K3L_EVENT *);
    virtual bool onDisconnect(K3L_EVENT *);
    virtual bool onAudioStatus(K3L_EVENT *);
    virtual bool onCollectCall(K3L_EVENT *);
    virtual bool onSeizureStart(K3L_EVENT *);
    virtual bool onDtmfDetected(K3L_EVENT *);
    virtual bool onNoAnswer(K3L_EVENT *);
    virtual bool onDtmfSendFinish(K3L_EVENT *);
    virtual bool onEvUntreated(K3L_EVENT *);
    virtual bool eventHandler(K3L_EVENT *);
    /*@}*/
    
    virtual bool doChannelAnswer(CommandRequest &);
    virtual bool doChannelHangup(CommandRequest &);
    bool commandHandler(CommandRequest &);
    
    virtual int makeCall(std::string params = "");
    virtual bool setupConnection();

    virtual int causeFromCallFail(int fail)  { return SWITCH_CAUSE_USER_BUSY; };
    virtual int callFailFromCause(int cause) { return -1; };
    virtual void reportFailToReceive(int fail_code) 
    { 
        call()->_indication = INDICA_FAST_BUSY; 
    }
    virtual bool setCollectCall();

    virtual bool indicateBusyUnlocked(int cause, bool sent_signaling = false);

    virtual bool cleanup(CleanupType type = CLN_HARD);

    virtual RingbackDefs::RingbackStType sendRingBackStatus(int rb_value = RingbackDefs::RB_SEND_DEFAULT) 
    { 
        return RingbackDefs::RBST_UNSUPPORTED; 
    }

    virtual bool sendPreAudio(int rb_value = RingbackDefs::RB_SEND_NOTHING) 
    { 
        if (rb_value != RingbackDefs::RB_SEND_NOTHING)
        {
            if(sendRingBackStatus(rb_value) == RingbackDefs::RBST_FAILURE)
                return false;
        }

        return true;
    }
    
    virtual bool isOK(void) { return false; }
    virtual bool isPhysicalFree() { return false; }
    virtual bool isFree(bool just_phy = false);

    virtual bool hasNumberDial() { return true; }

    virtual void getSpecialVariables()
    {
        try
        {
            const char * str_sup = getFSChannelVar("KDTMFSuppression");
            const char * str_agc = getFSChannelVar("KAutoGainControl");
            const char * str_eco = getFSChannelVar("KEchoCanceller");

            call()->_var_dtmf_state = (str_sup ? ((!SAFE_strcasecmp(str_sup, "true") || !SAFE_strcasecmp(str_sup, "on"))? T_TRUE : T_FALSE) : T_UNKNOWN);
            call()->_var_echo_state = (str_eco ? (!SAFE_strcasecmp(str_eco, "true") ? T_TRUE : T_FALSE) : T_UNKNOWN);
            call()->_var_gain_state = (str_agc ? (!SAFE_strcasecmp(str_agc, "true") ? T_TRUE : T_FALSE) : T_UNKNOWN);
        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(_target, "%s") % err._msg.c_str());
        }

        try
        {
            char * volume = (char*) getFSChannelVar("KSetVolume");
            
            if(!volume)
            {
                return;
            }

            std::string datastr(volume);

            Strings::trim(datastr);

            Strings::vector_type params;
            Strings::tokenize(datastr, params, "|,", 2);

            int inpvol = INT_MAX;
            int outvol = INT_MAX;

            /**/ if (params.size() == 1)
            {    
                int vol = (params[0] != "none" ? Strings::tolong(params[0]) : INT_MAX);

                inpvol = vol; 
                outvol = vol; 
            }    
            else if (params.size() == 2)
            {    
                inpvol = (params[0] != "none" ? Strings::tolong(params[0]) : INT_MAX);
                outvol = (params[1] != "none" ? Strings::tolong(params[1]) : INT_MAX);
            }    
            else 
            {    
                LOG(ERROR, "invalid number of arguments for KSetVolume!");
                return;
            }    

            if (inpvol != INT_MAX)
            {
                if(_call->_input_volume < -10 || _call->_input_volume > 10)
                {
                    _call->_input_volume = inpvol;
                }
            }

            if (outvol != INT_MAX)
            {
                if(_call->_output_volume < -10 || _call->_output_volume > 10)
                {
                    _call->_output_volume = outvol;
                }
            }

        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(_target, "%s") % err._msg.c_str());            
        }
        catch (Strings::invalid_value e)
        {    
            LOG(ERROR, FMT("invalid numeric value: %s") % e.value());
        }    
    }
    
    virtual void setSpecialVariables() {}

    virtual bool application(ApplicationType type, switch_core_session_t * session, const char *data) 
    {
        switch(type)
        {
        case FAX_ADJUST:
        case FAX_SEND:
        case FAX_RECEIVE:
            LOG(ERROR, PVT_FMT(_target, "not a digital and not "
                    "a fxo Khomp channel, fax not supported"));
            break;
        default:
            LOG(ERROR, PVT_FMT(_target, 
                    "application not supported"));
            break;
        }

        return false;
    }

    /* statistics functions */
    virtual std::string getStatistics(Statistics::Type type);
    virtual switch_xml_t getStatisticsXML(Statistics::Type type);
    virtual void clearStatistics()
    {
        _pvt_statistics->clear();   
    }

    virtual bool indicateRinging();
    virtual bool sendDtmf(std::string digit);
    virtual void cleanupIndications(bool force);

    /* Methods */

    bool indicateProgress();

    bool signalDTMF(char d);

    bool loopWhileFlagTimed(Kflags::FlagType flag, int &timeout, bool clear = true)
    {
        bool pvt_locked = true;
        bool quit       = false;

        unsigned int sleeps = 0;

        while ((timeout != 0) && !quit)
        {
            for (; (sleeps < 10) && !quit; sleeps++)
            {
                /* unlock our pvt struct */
                if (pvt_locked)
                {
                    _mutex.unlock();
                    pvt_locked = false;
                }

                /* wait a little while (100ms is good?) */
                usleep (100000);
                ++sleeps;

                /* re-lock pvt struct */
                switch (_mutex.lock())
                {
                    case SimpleLock::ISINUSE:
                    case SimpleLock::FAILURE:
                        LOG(ERROR, PVT_FMT(_target, "unable to lock pvt_mutex, trying again."));

                        sched_yield();
                        continue;

                    default:
                        break;
                }

                pvt_locked = true;

                if(clear)
                {
                    if(!call()->_flags.check(flag))
                    {
                        quit = true;
                        break;
                    }
                }
                else
                {
                    if(call()->_flags.check(flag))
                    {
                        quit = true;
                        break;
                    }

                }
            }

            /* decrement timeout, zero "sleeps". */
            timeout = (timeout > 0) ? timeout - 1 : timeout;
            sleeps = 0;
        }

        /* pvt should always be locked when retuning here. */
        return pvt_locked;
    }

    K3LAPIBase::GenericTarget & target()
    {
        return _target;
    }

    void session(switch_core_session_t * newSession)
    {
        _session = newSession;
    }

    switch_core_session_t * session()
    {
        return _session;
    }

    void owner(switch_core_session_t * owner) { _owner = owner; }

    switch_core_session_t * owner() const { return _owner; }

    /* 
       Returns the FreeSWITCH channel (switch_channel_t) 
       Can throw InvalidSwitchChannel, so must be carefull
    */
    switch_channel_t * getFSChannel()
    {
        if(!session()) 
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_SESSION_PASSED,"null switch_core_session_t");
        switch_channel_t *c = switch_core_session_get_channel(session());
        if(!c) 
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_CHANNEL,"null switch_channel_t obtained");
        return c;       
    }

    /* 
       Returns the FreeSWITCH channel (switch_channel_t) from given session 
       Can throw InvalidSwitchChannel, so must be carefull
    */
    static switch_channel_t * getFSChannel(switch_core_session_t * s)
    {
        if(!s) 
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_SESSION_PASSED,"null switch_core_session_t passed");
        switch_channel_t *c = switch_core_session_get_channel(s);
        if(!c) 
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_CHANNEL,"null switch_channel_t obtained");
        return c;       
    }

    /* 
       Returns the FreeSWITCH partner session (switch_core_session_t) 
       Can throw InvalidSwitchChannel, so must be carefull
       Don't forget to unlock [unlockPartner(switch_core_session_t*)]
    */
    switch_core_session_t * getFSLockedPartnerSession()
    {
        if(!session()) 
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_SESSION_PASSED,"null switch_core_session_t");

        switch_core_session_t * p_s = NULL;
        switch_core_session_get_partner(session(), &p_s);

        if(!p_s)
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_SESSION_PASSED,"null switch_core_session_t (partner)");
        return p_s;
    }

    /* Unlock a partner session */
    void unlockPartner(switch_core_session_t * s)
    {
        if(!s) 
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_SESSION_PASSED,"null switch_core_session_t passed");
        switch_core_session_rwunlock(s);
    }

    /* Get the uuid of session */
    char * getUUID()
    {
        if(!session()) 
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_SESSION_PASSED,"null switch_core_session_t");
        return switch_core_session_get_uuid(session());        
    }

    /* Get the uuid of given session */
    char * getUUID(switch_core_session_t * s)
    {
        if(!s)
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_SESSION_PASSED,"null switch_core_session_t passed");
        return switch_core_session_get_uuid(s);        
    }

    /* 
       Set a variable into a FS channel 
       Can throw InvalidSwitchChannel, so must be carefull
    */
    void setFSChannelVar(const char * name, const char * value)
    {
        if(!name || !value)
            return;

        switch_channel_t *c = getFSChannel(_owner);
        if(!c) 
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_CHANNEL,"null switch_channel_t obtained");
        switch_channel_set_variable(c,name,value);
    }    

   /* 
      Set a variable into a FS channel 
      Can throw InvalidSwitchChannel, so must be carefull
   */
   void setFSChannelVar(switch_channel_t *c, const char * name, const char * value)
   {
       if(!c)
           throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_CHANNEL,"null switch_channel_t obtained");
      
       if(!name || !value)
           return;

       switch_channel_set_variable(c,name,value);
   }

    /* 
       Get a varibale from a FS channel
       Can throw InvalidSwitchChannel, so must be carefull
    */
    const char * getFSChannelVar(const char * value)
    {
        if(!value)
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_VALUE, "value is null");

        switch_channel_t *c = getFSChannel(_owner);
        if(!c) 
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_CHANNEL,"null switch_channel_t obtained");
        return switch_channel_get_variable(c,value);
    }

    /* 
       Get a varibale from a FS from a given channel
       Can throw InvalidSwitchChannel, so must be carefull
    */
    const char * getFSChannelVar(switch_channel_t *c, const char * value)
    {
        if(!c)
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_CHANNEL,"null switch_channel_t obtained");
        if(!value)
            throw InvalidSwitchChannel(InvalidSwitchChannel::NULL_SWITCH_VALUE, "value is null");
        return switch_channel_get_variable(c,value);
    }

    /* Set a global variable  (without any throw) */
    void setFSGlobalVar(const char * name, const char * value)
    {
        if(!name || !value)
            return;

        switch_core_set_variable(name,value);
    }

    /* Get a global variable (without any throw) */
    const char * getFSGlobalVar(const char * name)
    {
        if(!name)
            return NULL;

#if SWITCH_LESS_THAN(1,0,6)
        const char * tmp = switch_core_get_variable(name);

        if(!tmp) return NULL;

        const char * val = strdup(tmp);

        return val;
#else
        return switch_core_get_variable_dup(name);
#endif
    }

    void freeFSGlobalVar(const char ** val)
    {
        if(!val || !*val) return;

#if SWITCH_LESS_THAN(1,0,6)
        free((void *)*val);
        *val = NULL;
#else
        char * v = (char *)*val;
        switch_safe_free(v);
        *val=NULL;
#endif
    }

    bool mixer(const char *file, const char *func, int line, 
            byte track, KMixerSource src, int32 index)
    {
        KMixerCommand mix;

        mix.Track = track;
        mix.Source = src;
        mix.SourceIndex = index;

        return command(file, func, line, CM_MIXER, (const char *)&mix);
    }

    /* Error handling for send command */
    bool command(const char *file, const char *func, int line, int code,
            const char *params = NULL, bool log = true)
    {
        try
        {
            Globals::k3lapi.command(_target, code, params);
        }
        catch(K3LAPI::failed_command & e)
        {
            if(log)
            {
                DBG(FUNC, OBJ_FMT(e.dev,e.obj,"Command '%s' has failed with error '%s'")
                   % Verbose::commandName(e.code).c_str()
                   % Verbose::status((KLibraryStatus)e.rc).c_str());
            }

            return false;
        }

        return true;
    }
    
    //TODO: Unir os dois metodos
    int commandState(const char *file, const char *func, int line, int code,
            const char *params = NULL)
    {
        try
        {
            Globals::k3lapi.command(_target, code, params);
        }
        catch(K3LAPI::failed_command & e)
        {
            LOG(ERROR,OBJ_FMT(e.dev,e.obj,"Command '%s' has failed with error '%s'")
               % Verbose::commandName(e.code).c_str()
               % Verbose::status((KLibraryStatus)e.rc).c_str());

            return e.rc;
        }

        return ksSuccess;
    }

    /*!
     \brief Will init part of our private structure and setup all the read/write
     buffers along with the proper codecs. Right now, only PCMA.
    */
    switch_status_t justAlloc(bool is_answering = true, switch_memory_pool_t **pool = NULL);
    switch_status_t justStart(switch_caller_profile_t *profile = NULL);

    void destroy(switch_core_session_t * s = NULL);
    void destroyAll();

    void doHangup();

    void setHangupCause(int cause, bool set_now = false)
    {
        if(_call->_hangup_cause) 
        { 

            DBG(FUNC,PVT_FMT(_target,"cause already set to %s") % switch_channel_cause2str((switch_call_cause_t) _call->_hangup_cause));
            return;
        }

        if(!session())
        {
            DBG(FUNC,PVT_FMT(_target,"session is null"));
            return;
        }

        switch_channel_t * channel = NULL;
        
        try
        {
            channel = getFSChannel();
        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            DBG(FUNC, PVT_FMT(_target,"%s") % err._msg.c_str());
            return;
        }

        int cause_from_freeswitch = switch_channel_get_cause(channel);
        if(cause_from_freeswitch != SWITCH_CAUSE_NONE)
        {
            DBG(FUNC,PVT_FMT(_target,"cause already set to %s from freeswitch") % switch_channel_cause2str((switch_call_cause_t)cause_from_freeswitch));
            _call->_hangup_cause = cause_from_freeswitch;
            return;
        }
        
        if(!cause)
        {
            DBG(FUNC,PVT_FMT(_target,"cause not defined"));
        }
        else
        {
            DBG(FUNC,PVT_FMT(_target,"setting cause to '%s'") % switch_channel_cause2str((switch_call_cause_t) cause));
            _call->_hangup_cause = cause;

            // not set variable in channel owner
            if(set_now)
            {
                switch_channel_hangup(channel, (switch_call_cause_t)_call->_hangup_cause);
            }
        }
    }

    bool startCadence(CadencesType type);
    bool stopCadence()
    {
        if(call()->_cadence != PLAY_NONE)
        {
            call()->_cadence = PLAY_NONE;

            command(KHOMP_LOG, CM_STOP_CADENCE);
        }
    }

    bool startStream(bool enable_mixer = true);
    bool stopStream(bool enable_mixer = true);

    bool startListen(bool conn_rx = true);
    bool stopListen(void);

    bool obtainRX(bool with_delay = false);
    bool obtainTX();

    bool setVolume(const char * type, int volume);
    bool setVolume()
    {
        bool ret = false;

        if(_call->_input_volume >= -10 && _call->_input_volume <= 10)
        {
            setVolume("input", _call->_input_volume);
            ret = true;
        }

        if(_call->_output_volume >= -10 && _call->_output_volume <= 10)
        {
            setVolume("output", _call->_output_volume);
            ret = true;
        }
     
        return ret;
    }

    KSignaling getSignaling(void)
    {
        return Globals::k3lapi.channel_config(_target.device,_target.object).Signaling;
    }

    std::string getStateString(void);

    bool dtmfSuppression(bool enable);
    bool echoCancellation(bool enable);
    virtual bool autoGainControl(bool enable);    

    /* Timer callbacks */
    static void pbxRingGen(Board::KhompPvt * pvt);
    static void coRingGen(Board::KhompPvt * pvt);

    virtual int getActiveChannel(bool invalid_as_not_found);

    /* Let's validate the contexts */
    virtual bool validContexts(MatchExtension::ContextListType & contexts, 
                               std::string extra_string = "")
    {
        DBG(FUNC,PVT_FMT(_target,"c"));

        /*
        if(!_group_context.empty())
        {
            contexts.insert(contexts.begin(), _group_context);
            //contexts.push_back(_group_context);
        }
        */
    
        for (MatchExtension::ContextListType::iterator i = contexts.begin(); i != contexts.end(); i++) 
            replaceTemplate((*i),  "DD", _target.device);

        BEGIN_CONTEXT  
        {    
            const K3L_DEVICE_CONFIG & dev_cfg = Globals::k3lapi.device_config(_target);

            for (MatchExtension::ContextListType::iterator i = contexts.begin(); i != contexts.end(); i++) 
                replaceTemplate((*i), "SSSS", atoi(dev_cfg.SerialNumber));
        }    
        END_CONTEXT
      
        DBG(FUNC,PVT_FMT(_target,"r"));
        return true;
    }

    void cleanupBuffers()
    {
        DBG(FUNC,PVT_FMT(_target, "Cleanup buffers"));

        _reader_frames.clear();
        _writer_frames.clear();

        for(unsigned int i = 0; i < SILENCE_PACKS; i++)
        {
            /* add silence to the read buffer */
            if (!_reader_frames.give((const char *)Board::_cng_buffer, Globals::switch_packet_size))
            {
                LOG(ERROR, PVT_FMT(target(), "Problem in Reader Buffer"));
            }
        
            /* add silence to the writer buffer */
            if (!_writer_frames.give((const char *)Board::_cng_buffer, Globals::boards_packet_size))
            {
                LOG(ERROR, PVT_FMT(target(), "Problem in Writer Buffer"));
            }
        }
    }

    bool freeState()
    {
        return !(_call->_flags.check(Kflags::IS_INCOMING) || _call->_flags.check(Kflags::IS_OUTGOING));
    }

    Call * call() { return _call; }

    K3LAPIBase::GenericTarget           _target;    /*!< The device/channel pair to bind this pvt to */
    ChanLockType             _mutex;     /*!< Used for *our* internal locking */
    Call                    *_call;
    switch_core_session_t   *_session;   /*!< The session to which this pvt is associated with */
    switch_core_session_t   *_owner;
    bool                     _has_fail;

    switch_caller_profile_t *_caller_profile;

    switch_codec_t           _read_codec;
    switch_codec_t           _write_codec;

    FrameSwitchManager       _reader_frames;
    FrameBoardsManager       _writer_frames;

    std::string              _group_context;

    PvtStatistics           *_pvt_statistics;

    std::string              _mohclass;
    std::string              _language;
    std::string              _accountcode;
};

/******************************************************************************/
/******************************************************************************/
    typedef std::vector < Board * >    VectorBoard;
    typedef std::vector < KhompPvt * > VectorChannel;  /*!< Collection of pointers of KhompPvts */

     /*
        these (below) are going to rule the elements ordering in our multiset
        (used as a "ordering-save priority queue").
     */
    struct PvtCallCompare
    {
        bool operator() (KhompPvt * pvt1, KhompPvt * pvt2) const
        {
            /* true if pvt1 precedes pvt2 */
            return (Board::getStats(pvt1->target().device,pvt1->target().object,kcsiOutbound) <
                    Board::getStats(pvt2->target().device,pvt2->target().object,kcsiOutbound));
            return true;
        }
    };

    typedef std::multiset< KhompPvt *, PvtCallCompare > PriorityCallQueue;
   
public:
    
    /* Board constructor */
    Board(int id) : _device_id(id) {}

    /* Board destructor */    
    virtual ~Board() {}

    int id() { return _device_id; }

    KhompPvt * channel(int obj)
    {
        try
        {
            KhompPvt * pvt = _channels.at(obj);

            if(!pvt)
            {
                throw;
            }

            return pvt;
        }
        catch (...)
        {
            throw K3LAPITraits::invalid_channel(_device_id, obj);
        }
    }

    ChanEventHandler * chanEventHandler() { return _event_handler; }
    
    ChanCommandHandler * chanCommandHandler() { return _command_handler; }

    virtual void initializeChannels(void);
    void finalizeChannels(void);

    virtual bool eventHandler(const int obj, K3L_EVENT *e)
    {
        DBG(STRM, D("(Generic Board) c"));

        bool ret = true;

        switch(e->Code)
        {
        case EV_REQUEST_DEVICE_SECURITY_KEY:
            break;
        default:
            try
            {
                ret = channel(obj)->eventHandler(e);
            }
            catch (K3LAPITraits::invalid_channel & invalid)
            {
                LOG(ERROR, OBJ_FMT(_device_id,obj,"(Generic Board) r (invalid channel on event '%s')") 
                % Verbose::eventName(e->Code).c_str());

                return false;
            }

            break;
        }
        
        DBG(STRM, D("(Generic Board) r"));

        return ret;
    }

public:
    ChanTimer            _timers;
protected:
    const int            _device_id;
    ChanEventHandler   * _event_handler; /* The device event handler */
    ChanCommandHandler * _command_handler; /* The device command handler */
    VectorChannel        _channels;

public:
    /* static stuff */
    static bool initializeK3L(void);
    static bool finalizeK3L(void);
    static bool initializeHandlers(void);
    static bool finalizeHandlers(void);
    static void initializeBoards(void);
    static void finalizeBoards(void);
    static void initializeCngBuffer(void);
    static bool initialize(void);
    static bool finalize(void);

    /* Thread Device Event Handler */
    static int eventThread(void *);
    
    /* Thread Device Command Handler */
    static int commandThread(void *);

    /*!
      \brief Lookup channels and boards when dialed.
      \param allocation_string The dialstring as put on Dialplan. [Khomp/[a|A|0-board_high]/[a|A|0-channel_high]/dest].
      \param new_session Session allocated for this call.
      \param[out] cause Cause returned. Returns NULL if suceeded if not, the proper cause.
      \return KhompPvt to be used on the call.
      */
    static KhompPvt * find_channel(char* allocation_string, switch_core_session_t * new_session, switch_call_cause_t * cause);
    static void khomp_add_event_board_data(const K3LAPIBase::GenericTarget target, switch_event_t *event);

    static Board * board(int dev)
    {
        try
        {
            Board * b = _boards.at(dev);

            if(!b)
            {
                throw;
            }

            return b;
        }
        catch(...)
        {
            throw K3LAPITraits::invalid_device(dev);
        }
    }

    static KhompPvt * get(int32 device, int32 object)
    {
        //if (!Globals::k3lapi.valid_channel(device, object))
        //    throw K3LAPI::invalid_channel(device, object);

        try
        {
            return board(device)->channel(object);
        }
        catch(...)
        {
            throw K3LAPITraits::invalid_channel(device, object);
        }
    }

    static KhompPvt * get(K3LAPIBase::GenericTarget & target)
    {
        //if (!Globals::k3lapi.valid_channel(target.device, target.object))
        //    throw K3LAPI::invalid_channel(target.device, target.object);

        try
        {
            return board(target.device)->channel(target.object);
            //return KhompPvt::_pvts[target.device][target.object];
        }
        catch(...)
        {
            return NULL;
            //throw K3LAPI::invalid_channel(target.device, target.object);
        }
    }

    static unsigned int getStats(int32 device, int32 object, uint32 index)
    {
        unsigned int stats = (unsigned int)-1;

        try
        {
            stats = Globals::k3lapi.channel_stats(device, object, index);
        }
        catch(K3LAPITraits::invalid_channel & err)
        {
        //K::logger::logg(C_WARNING, B(dev,channel, "Command get_stats has failed with error '%s'.") %
        //                          Verbose::status((KLibraryStatus) stt_res));
        }

        return stats;
    }

    static KhompPvt * queueFindFree(PriorityCallQueue &pqueue);
    static void queueAddChannel(PriorityCallQueue &pqueue, unsigned int board, unsigned int object);
    static KhompPvt * findFree(unsigned int board, unsigned int object, bool fully_available = true);
    static void applyGlobalVolume(void);

public:

    static VectorBoard     _boards;
    static char            _cng_buffer[Globals::cng_buffer_size];

    static Kommuter        kommuter;

};

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

#endif /* _KHOMP_PVT_H_*/
