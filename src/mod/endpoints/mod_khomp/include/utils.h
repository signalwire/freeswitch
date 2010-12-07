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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <bitset>
#include <refcounter.hpp>
#include <ringbuffer.hpp>
#include <simple_lock.hpp>
#include <saved_condition.hpp>
#include <thread.hpp>
#include "globals.h"
#include "logger.h"
#include "defs.h"

/******************************************************************************/
/************************** Defining applications *****************************/

typedef enum
{
    FAX_ADJUST,
    FAX_SEND,
    FAX_RECEIVE,
    USER_TRANSFER,
    SMS_CHECK,
    SMS_SEND,
    SELECT_SIM_CARD,
}
ApplicationType;

/******************************************************************************/
/***************** Abstraction for defining channel flags *********************/

struct Kflags
{
    typedef enum
    {
        CONNECTED = 0,
        REALLY_CONNECTED,

        IS_OUTGOING,
        IS_INCOMING,

        STREAM_UP,
        LISTEN_UP,

        GEN_CO_RING,
        GEN_PBX_RING,

        HAS_PRE_AUDIO,
        HAS_CALL_FAIL,

        DROP_COLLECT,

        NEEDS_RINGBACK_CMD, //R2
        EARLY_RINGBACK, //FXO

        FAX_DETECTED, //Digital, FXO
        FAX_SENDING, //Digital, FXO
        FAX_RECEIVING, //Digital, FXO

        OUT_OF_BAND_DTMFS,

        KEEP_DTMF_SUPPRESSION,
        KEEP_ECHO_CANCELLATION,
        KEEP_AUTO_GAIN_CONTROL,

        WAIT_SEND_DTMF,

        CALL_WAIT_SEIZE, //FXO

        NUMBER_DIAL_FINISHD, //R2
        NUMBER_DIAL_ONGOING, //R2

        FXS_OFFHOOK, //FXS
        FXS_DIAL_FINISHD, //FXS
        FXS_DIAL_ONGOING, //FXS
        FXS_FLASH_TRANSFER, //FXS

        XFER_QSIG_DIALING, //ISDN
        XFER_DIALING, //ISDN, FXO

        SMS_DOING_UPLOAD, //GSM

        /*
        NOW LOADING ...

        BRIDGED,
        */

        /* Do not remove this last FLAG */
        INVALID_FLAG
    }
    FlagType;

    struct Flag
    {        
        const char *_name;
        bool _value;
    };

    Kflags() { init(); };

    void init();

    inline const char * name(FlagType bit) { return _flags[bit]._name; }

    inline bool check(FlagType bit) 
    {
#ifdef DEBUG_FLAGS
        DBG(FUNC, D("Flag %s=%s") % name(bit) % 
                (_flags[bit]._value ? "TRUE" : "FALSE"));
#endif
        return _flags[bit]._value;
    }

    inline void set(FlagType bit)
    {
#ifdef DEBUG_FLAGS
        DBG(FUNC, D("Flag %s") % name(bit));
#endif
        _flags[bit]._value = true;
    }

    inline void clear(FlagType bit)
    { 
#ifdef DEBUG_FLAGS
        DBG(FUNC, D("Flag %s") % name(bit));
#endif
        _flags[bit]._value = false;
    }

    inline void clearAll()
    {
        unsigned int index = 0;
        for(; (FlagType)index < INVALID_FLAG; index++)
        {
            if(_flags[index]._value)
            {
                DBG(FUNC, D("Flag %s was not clean!") % name((FlagType)index));
                clear((FlagType)index);
            }
        }
    }

 protected:
    Flag _flags[INVALID_FLAG+1];
};

/******************************************************************************/
/************************* Commands and Events Handler ************************/
struct CommandRequest
{
    typedef enum
    {
        NONE = 0,
        COMMAND,
        ACTION
    }
    ReqType;

    typedef enum
    {
        CNONE = 0,

        /* Commands */
        CMD_CALL,
        CMD_ANSWER,
        CMD_HANGUP,
        
        /* Actions */
        FLUSH_REC_STREAM,
        FLUSH_REC_BRIDGE,
        START_RECORD,
        STOP_RECORD

    }
    CodeType;

    typedef enum
    {
        RFA_CLOSE,
        RFA_KEEP_OPEN,
        RFA_REMOVE
    }
    RecFlagType;

    /* "empty" constructor */
    CommandRequest() : 
        _type(NONE),
        _code(CNONE),
        _obj(-1)
    {}

    CommandRequest(ReqType type, CodeType code, int obj) : 
            _type(type),
            _code(code),
            _obj(obj)
    {}

    CommandRequest(const CommandRequest & cmd) : 
            _type(cmd._type), 
            _code(cmd._code), 
            _obj(cmd._obj) 
    {}

    ~CommandRequest() {}

    void operator=(const CommandRequest & cmd)
    {
        _type = cmd._type;
        _code = cmd._code;
        _obj = cmd._obj;
    }

    void mirror(const CommandRequest & cmd_request)
    {
        _type = cmd_request._type;
        _code = cmd_request._code;
        _obj = cmd_request._obj;
    }

    short type() { return _type; }
    
    short code() { return _code; }

    int obj() { return _obj; }

private:
    short _type;
    short _code;
    int   _obj;
};

struct EventRequest
{
    /* "empty" constructor */
    EventRequest(bool can_delete = true) : 
        _delete(can_delete), 
        _obj(-1)
    {
        if(can_delete)
            _event = new K3L_EVENT();
    }

    /* Temporary constructor */
    EventRequest(int obj, K3L_EVENT * ev) :
            _delete(false),
            _obj(obj),
            _event(ev)
    {}
    
    //EventRequest(const EventRequest & ev) : _obj(ev._obj) {}

    ~EventRequest()
    {

        if(!_delete || !_event)
            return;

        if(_event->ParamSize)
            free(_event->Params);

        delete _event;
    }

    void operator=(const EventRequest & ev)
    {
        _delete = false;
        _obj = ev._obj;
        _event = ev._event;

    }

    void mirror(const EventRequest & ev_request)
    {
        //Checar o _event

        if(_event->ParamSize)
        {
            free(_event->Params);
        }

        _event->Params = NULL;

        _obj = ev_request._obj;

        K3L_EVENT * ev     = ev_request._event;

        if(!ev)
        {
            clearEvent();
            return;
        }

        _event->Code       = ev->Code;       // API code
        _event->AddInfo    = ev->AddInfo;    // Parameter 1
        _event->DeviceId   = ev->DeviceId;   // Hardware information
        _event->ObjectInfo = ev->ObjectInfo; // Additional object information
        _event->ParamSize  = ev->ParamSize;  // Size of parameter buffer
        _event->ObjectId   = ev->ObjectId;   // KEventObjectId: Event thrower object id

        if(ev->ParamSize)
        {
            // Pointer to the parameter buffer
            _event->Params = malloc(ev->ParamSize+1);
            memcpy(_event->Params, ev->Params, ev->ParamSize);
            ((char *)(_event->Params))[ev->ParamSize] = 0;
        }
    }

    bool clearEvent()
    {
        _event->Code       = -1;
        _event->AddInfo    = -1;
        _event->DeviceId   = -1;
        _event->ObjectInfo = -1;
        _event->ParamSize  = 0;
        _event->ObjectId   = -1;
    }

    int obj() { return _obj; }

    K3L_EVENT * event() { return _event; }


private:
    bool        _delete;
    int         _obj;
    K3L_EVENT * _event;
};

template < typename R, int S >
struct GenericFifo
{
    typedef R RequestType;
    typedef SimpleNonBlockLock<25,100>  LockType;

    GenericFifo(int device) : 
            _device(device), 
            _shutdown(false), 
            _buffer(S), 
            _mutex(Globals::module_pool), 
            _cond(Globals::module_pool)
    {};

    int                         _device;
    bool                        _shutdown;
    Ringbuffer < RequestType >  _buffer;
    LockType                    _mutex; /* to sync write acess to event list */
    SavedCondition              _cond;
    Thread                     *_thread;

};

typedef GenericFifo < CommandRequest, 250 > CommandFifo;
typedef GenericFifo < EventRequest, 500 >   EventFifo;

/* Used inside KhompPvt to represent an command handler */
struct ChanCommandHandler: NEW_REFCOUNTER(ChanCommandHandler)
{
    typedef int (HandlerType)(void *);

    ChanCommandHandler(int device, HandlerType * handler)
    {
        _fifo = new CommandFifo(device);
        /* device event handler */
        _fifo->_thread = new Thread(handler, (void *)this, Globals::module_pool);
        if(_fifo->_thread->start())
        {
            DBG(FUNC,"Device command handler started");
        }
        else
        {
            LOG(ERROR, "Device command handler error");
        }
    }

    ChanCommandHandler(const ChanCommandHandler & cmd)
    : INC_REFCOUNTER(cmd, ChanCommandHandler),
      _fifo(cmd._fifo)
    {};

    void unreference();

    CommandFifo * fifo()
    {
        return _fifo;
    }

    void signal()
    {
        _fifo->_cond.signal();
    };

    bool writeNoSignal(const CommandRequest &);
    bool write(const CommandRequest &);

protected:
    CommandFifo * _fifo;
};

/* Used inside KhompPvt to represent an event handler */
struct ChanEventHandler: NEW_REFCOUNTER(ChanEventHandler)
{
    typedef int (HandlerType)(void *);

    ChanEventHandler(int device, HandlerType * handler)
    {
        _fifo = new EventFifo(device);
        /* device event handler */
        _fifo->_thread = new Thread(handler, (void *)this, Globals::module_pool);
        if(_fifo->_thread->start())
        {
            DBG(FUNC,"Device event handler started");
        }
        else
        {
            LOG(ERROR, "Device event handler error");
        }
    }

    ChanEventHandler(const ChanEventHandler & evt)
    : INC_REFCOUNTER(evt, ChanEventHandler),
      _fifo(evt._fifo)
    {};

    void unreference();

    EventFifo * fifo()
    {
        return _fifo;
    }

    void signal()
    {
        _fifo->_cond.signal();
    };

    bool provide(const EventRequest &);
    bool writeNoSignal(const EventRequest &);
    bool write(const EventRequest &);

protected:
    EventFifo * _fifo;
};


/******************************************************************************/
/****************************** Internal **************************************/
struct RingbackDefs
{
    enum
    {
        RB_SEND_DEFAULT = -1,
        RB_SEND_NOTHING = -2,
    };

    typedef enum
    {
        RBST_SUCCESS,
        RBST_UNSUPPORTED,
        RBST_FAILURE,
    }
    RingbackStType;
};

/******************************************************************************/
/******************************* Others ***************************************/
static bool checkTrueString(const char * str)
{   
    if (str && *str)
    {   
        if (!SAFE_strcasecmp(str, "yes") || !SAFE_strcasecmp(str, "true") ||
                !SAFE_strcasecmp(str, "enabled") || !SAFE_strcasecmp(str, "sim"))
        {   
            return true;
        }   
    }   

    return false;
}   

static bool checkFalseString(const char * str)
{   
    if (str && *str)
    {   
        if (!SAFE_strcasecmp(str, "no") || !SAFE_strcasecmp(str, "false") ||
                !SAFE_strcasecmp(str, "disabled") || !SAFE_strcasecmp(str, "não"))
        {   
            return true;
        }   
    }   

    return false;
}   

static TriState getTriStateValue(const char * str)
{
    if (str)
    {   
        /**/ if (checkTrueString(str))  return T_TRUE;
        else if (checkFalseString(str)) return T_FALSE;
        else /***********************************/ return T_UNKNOWN;
    }   

    return T_UNKNOWN;
}

const char * answerInfoToString(int answer_info);

static bool replaceTemplate(std::string & haystack, const std::string & needed, int value)
{
    Regex::Expression e(std::string(needed).c_str());
    Regex::Match r(haystack,e);

    if(!r.matched()) return false;

    int len;
    std::string fmt;
    std::string tmp;

    // if needed isn't "SSSS" the length of needed will give string format
    // if needed is "SSSS" the length of value will give string format
    std::string ssss("SSSS");

    if (ssss != needed)
    {   
        len = needed.size();
        fmt = STG(FMT("%%0%dd") % len);
        tmp = STG(FMT(fmt) % value);
    }   
    else
    {   
        len = (STG(FMT("%d") % value)).size();
        fmt = STG(FMT("%%%dd") % len);
        tmp = STG(FMT(fmt) % value);
    }   

    haystack = r.replace(STG(FMT(fmt) % value));

    DBG(FUNC,FMT("haystack: %s") % haystack);
    return true;
}


static std::string timeToString (time_t time_value)
{
    int horas = (int)time_value / 3600;

    if (horas > 0) 
        time_value -= horas * 3600;

    int minutos = (int)time_value / 60;

    if (minutos > 0) 
        time_value -= minutos * 60;

    return STG(FMT("%02d:%02d:%02d") % horas % minutos % (int)time_value);
}
/******************************************************************************/
/******************************* Match functions ******************************/
struct MatchExtension
{
    typedef std::vector<std::string> ContextListType;
    typedef std::vector<std::string> ExtenListType;

    typedef enum
    {
        MATCH_NONE,
        MATCH_EXACT,
        MATCH_MORE
    }
    MatchType;

    static bool canMatch(std::string & context, std::string & exten, 
                std::string & caller_id, bool match_more = false);

    static MatchType matchExtension(std::string &, std::string &, 
                std::string &, bool match_only = false);

    static MatchType findExtension(std::string &, std::string &, 
                ContextListType &, std::string &, std::string &,
                bool default_ctx = true, bool default_ext = true);

};
/******************************************************************************/
/************************** Thread helper functions ***************************/
typedef int (HandlerType)(void *);
static Thread * threadCreate(HandlerType * handler,void * arg)
{
    Thread *t = new Thread(handler, (void *) arg, Globals::module_pool);
    return t;
}

/******************************************************************************/
/******************************** Kommuter ************************************/
struct Kommuter
{
    Kommuter() : 
        _kommuter_count(-1), 
        _kwtd_timer_on(false)
    {}
    
    bool start()
    {
        /* Checks for kommuter */
        try
        {
            Globals::k3lapi.command(-1,-1,CM_WATCHDOG_COUNT);
        }
        catch(K3LAPI::failed_command & e)
        {
            LOG(WARNING , "libkwd.so used by Kommuter devices is not available.");
            return false;
        }

        return true;
    }

    bool stop();
    
    bool initialize(K3L_EVENT *e);
  
    /* Index of the WatchDog timer */ 
    TimerTraits::Index  _kwtd_timer_index;
    int                 _kommuter_count;
    bool                _kwtd_timer_on;

    /* Timer to control our dog (totó) */
    static void wtdKickTimer(void *);
    
};

/******************************************************************************/
/******************************* Statistics ***********************************/
struct Statistics
{
    typedef enum
    {
        DETAILED = 0,
        ROW
    }
    Type;

    virtual std::string getDetailed() { return ""; }
    virtual void clear() {}
};
/******************************************************************************/
/****************************** ESL - Events **********************************/
    
struct ESL 
{

    typedef std::vector <std::string> VectorEvents;
    
    ESL(std::string type);

    ~ESL();
    
    switch_event_t * create(const std::string type)
    {
        switch_event_t *event;

        if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, type.c_str()) != SWITCH_STATUS_SUCCESS)
        {
            return NULL;
        }

        return event;
    }
    
    switch_event_t * create()
    {
        return create(_type);
    }

    bool add(switch_event_t *event, const std::string key, 
                                    const std::string value) const
    {
        if(!event) return false;

        switch_status_t res = switch_event_add_header_string(event, 
                SWITCH_STACK_BOTTOM,
                key.c_str(), 
                value.c_str());

        return (res == SWITCH_STATUS_SUCCESS ? true : false);
    }

    bool add(switch_event_t *event, K3LAPIBase::GenericTarget & target)
    {
        bool ok = add(event, "Device", STR(FMT("%d") % target.device));

        if(!ok)
            return ok;

        std::string type;

        switch(target.type)
        {
            case K3LAPIBase::GenericTarget::CHANNEL:
                type = "Channel";
                break;
            case K3LAPIBase::GenericTarget::MIXER:
                type = "Mixer";
                break;
            case K3LAPIBase::GenericTarget::LINK:
                type = "Link";
                break;
            default:
                type = "Other";
                break;
        }        

        return add(event, type, STR(FMT("%d") % target.object));
    }

    bool fire(switch_event_t **event) const
    {
        if(!event || !*event) return false;

        switch_status_t res = switch_event_fire(event);

        *event = NULL;

        return (res == SWITCH_STATUS_SUCCESS ? true : false);
    }

    static bool registerEvents();

    static bool unregisterEvents();

protected:
    const std::string _type;
    static VectorEvents * _events;
};

/******************************************************************************/
/******************************************************************************/

#endif  /* _UTILS_H_ */
