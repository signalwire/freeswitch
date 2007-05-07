# This file was created automatically by SWIG.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _freeswitch

def _swig_setattr(self,class_type,name,value):
    if (name == "this"):
        if isinstance(value, class_type):
            self.__dict__[name] = value.this
            if hasattr(value,"thisown"): self.__dict__["thisown"] = value.thisown
            del value.thisown
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    self.__dict__[name] = value

def _swig_getattr(self,class_type,name):
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError,name

import types
try:
    _object = types.ObjectType
    _newclass = 1
except AttributeError:
    class _object : pass
    _newclass = 0
del types



PythonDTMFCallback = _freeswitch.PythonDTMFCallback

console_log = _freeswitch.console_log

console_clean_log = _freeswitch.console_clean_log

api_execute = _freeswitch.api_execute

api_reply_delete = _freeswitch.api_reply_delete
class SessionContainer(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, SessionContainer, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, SessionContainer, name)
    def __repr__(self):
        return "<C SessionContainer instance at %s>" % (self.this,)
    def __init__(self, *args):
        _swig_setattr(self, SessionContainer, 'this', _freeswitch.new_SessionContainer(*args))
        _swig_setattr(self, SessionContainer, 'thisown', 1)
    def __del__(self, destroy=_freeswitch.delete_SessionContainer):
        try:
            if self.thisown: destroy(self)
        except: pass
    def answer(*args): return _freeswitch.SessionContainer_answer(*args)
    def pre_answer(*args): return _freeswitch.SessionContainer_pre_answer(*args)
    def hangup(*args): return _freeswitch.SessionContainer_hangup(*args)
    def set_variable(*args): return _freeswitch.SessionContainer_set_variable(*args)
    def get_variable(*args): return _freeswitch.SessionContainer_get_variable(*args)
    def play_file(*args): return _freeswitch.SessionContainer_play_file(*args)
    def set_dtmf_callback(*args): return _freeswitch.SessionContainer_set_dtmf_callback(*args)
    def speak_text(*args): return _freeswitch.SessionContainer_speak_text(*args)
    def set_tts_parms(*args): return _freeswitch.SessionContainer_set_tts_parms(*args)
    def get_digits(*args): return _freeswitch.SessionContainer_get_digits(*args)
    def transfer(*args): return _freeswitch.SessionContainer_transfer(*args)
    def play_and_get_digits(*args): return _freeswitch.SessionContainer_play_and_get_digits(*args)
    def execute(*args): return _freeswitch.SessionContainer_execute(*args)

class SessionContainerPtr(SessionContainer):
    def __init__(self, this):
        _swig_setattr(self, SessionContainer, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, SessionContainer, 'thisown', 0)
        _swig_setattr(self, SessionContainer,self.__class__,SessionContainer)
_freeswitch.SessionContainer_swigregister(SessionContainerPtr)


