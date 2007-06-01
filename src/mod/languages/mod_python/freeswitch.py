# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _freeswitch
import new
new_instancemethod = new.instancemethod
def _swig_setattr_nondynamic(self,class_type,name,value,static=1):
    if (name == "thisown"): return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'PySwigObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    if (not static) or hasattr(self,name):
        self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)

def _swig_setattr(self,class_type,name,value):
    return _swig_setattr_nondynamic(self,class_type,name,value,0)

def _swig_getattr(self,class_type,name):
    if (name == "thisown"): return self.this.own()
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError,name

def _swig_repr(self):
    try: strthis = "proxy of " + self.this.__repr__()
    except: strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

import types
try:
    _object = types.ObjectType
    _newclass = 1
except AttributeError:
    class _object : pass
    _newclass = 0
del types


console_log = _freeswitch.console_log
console_clean_log = _freeswitch.console_clean_log
api_execute = _freeswitch.api_execute
api_reply_delete = _freeswitch.api_reply_delete
process_callback_result = _freeswitch.process_callback_result
class CoreSession(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, CoreSession, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, CoreSession, name)
    __repr__ = _swig_repr
    def __init__(self, *args): 
        this = _freeswitch.new_CoreSession(*args)
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _freeswitch.delete_CoreSession
    __del__ = lambda self : None;
    __swig_setmethods__["session"] = _freeswitch.CoreSession_session_set
    __swig_getmethods__["session"] = _freeswitch.CoreSession_session_get
    if _newclass:session = property(_freeswitch.CoreSession_session_get, _freeswitch.CoreSession_session_set)
    __swig_setmethods__["channel"] = _freeswitch.CoreSession_channel_set
    __swig_getmethods__["channel"] = _freeswitch.CoreSession_channel_get
    if _newclass:channel = property(_freeswitch.CoreSession_channel_get, _freeswitch.CoreSession_channel_set)
    def answer(*args): return _freeswitch.CoreSession_answer(*args)
    def preAnswer(*args): return _freeswitch.CoreSession_preAnswer(*args)
    def hangup(*args): return _freeswitch.CoreSession_hangup(*args)
    def setVariable(*args): return _freeswitch.CoreSession_setVariable(*args)
    def getVariable(*args): return _freeswitch.CoreSession_getVariable(*args)
    def playFile(*args): return _freeswitch.CoreSession_playFile(*args)
    def setDTMFCallback(*args): return _freeswitch.CoreSession_setDTMFCallback(*args)
    def speakText(*args): return _freeswitch.CoreSession_speakText(*args)
    def set_tts_parms(*args): return _freeswitch.CoreSession_set_tts_parms(*args)
    def getDigits(*args): return _freeswitch.CoreSession_getDigits(*args)
    def transfer(*args): return _freeswitch.CoreSession_transfer(*args)
    def playAndGetDigits(*args): return _freeswitch.CoreSession_playAndGetDigits(*args)
    def streamfile(*args): return _freeswitch.CoreSession_streamfile(*args)
    def execute(*args): return _freeswitch.CoreSession_execute(*args)
    def begin_allow_threads(*args): return _freeswitch.CoreSession_begin_allow_threads(*args)
    def end_allow_threads(*args): return _freeswitch.CoreSession_end_allow_threads(*args)
CoreSession_swigregister = _freeswitch.CoreSession_swigregister
CoreSession_swigregister(CoreSession)

PythonDTMFCallback = _freeswitch.PythonDTMFCallback
class input_callback_state(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, input_callback_state, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, input_callback_state, name)
    __repr__ = _swig_repr
    __swig_setmethods__["function"] = _freeswitch.input_callback_state_function_set
    __swig_getmethods__["function"] = _freeswitch.input_callback_state_function_get
    if _newclass:function = property(_freeswitch.input_callback_state_function_get, _freeswitch.input_callback_state_function_set)
    __swig_setmethods__["threadState"] = _freeswitch.input_callback_state_threadState_set
    __swig_getmethods__["threadState"] = _freeswitch.input_callback_state_threadState_get
    if _newclass:threadState = property(_freeswitch.input_callback_state_threadState_get, _freeswitch.input_callback_state_threadState_set)
    __swig_setmethods__["extra"] = _freeswitch.input_callback_state_extra_set
    __swig_getmethods__["extra"] = _freeswitch.input_callback_state_extra_get
    if _newclass:extra = property(_freeswitch.input_callback_state_extra_get, _freeswitch.input_callback_state_extra_set)
    __swig_setmethods__["funcargs"] = _freeswitch.input_callback_state_funcargs_set
    __swig_getmethods__["funcargs"] = _freeswitch.input_callback_state_funcargs_get
    if _newclass:funcargs = property(_freeswitch.input_callback_state_funcargs_get, _freeswitch.input_callback_state_funcargs_set)
    def __init__(self, *args): 
        this = _freeswitch.new_input_callback_state(*args)
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _freeswitch.delete_input_callback_state
    __del__ = lambda self : None;
input_callback_state_swigregister = _freeswitch.input_callback_state_swigregister
input_callback_state_swigregister(input_callback_state)

class PySession(CoreSession):
    __swig_setmethods__ = {}
    for _s in [CoreSession]: __swig_setmethods__.update(_s.__swig_setmethods__)
    __setattr__ = lambda self, name, value: _swig_setattr(self, PySession, name, value)
    __swig_getmethods__ = {}
    for _s in [CoreSession]: __swig_getmethods__.update(_s.__swig_getmethods__)
    __getattr__ = lambda self, name: _swig_getattr(self, PySession, name)
    __repr__ = _swig_repr
    def __init__(self, *args): 
        this = _freeswitch.new_PySession(*args)
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _freeswitch.delete_PySession
    __del__ = lambda self : None;
    def streamfile(*args): return _freeswitch.PySession_streamfile(*args)
    def begin_allow_threads(*args): return _freeswitch.PySession_begin_allow_threads(*args)
    def end_allow_threads(*args): return _freeswitch.PySession_end_allow_threads(*args)
PySession_swigregister = _freeswitch.PySession_swigregister
PySession_swigregister(PySession)



