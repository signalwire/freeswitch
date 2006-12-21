# This file was created automatically by SWIG 1.3.27.
# Don't modify this file, modify the SWIG interface instead.

import _freeswitch

# This file is compatible with both classic and new-style classes.
def _swig_setattr_nondynamic(self,class_type,name,value,static=1):
    if (name == "this"):
        if isinstance(value, class_type):
            self.__dict__[name] = value.this
            if hasattr(value,"thisown"): self.__dict__["thisown"] = value.thisown
            del value.thisown
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    if (not static) or hasattr(self,name) or (name == "thisown"):
        self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)

def _swig_setattr(self,class_type,name,value):
    return _swig_setattr_nondynamic(self,class_type,name,value,0)

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
class SessionContainer(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, SessionContainer, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, SessionContainer, name)
    def __repr__(self):
        return "<%s.%s; proxy of C++ SessionContainer instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def __init__(self, *args):
        _swig_setattr(self, SessionContainer, 'this', _freeswitch.new_SessionContainer(*args))
        _swig_setattr(self, SessionContainer, 'thisown', 1)
    def __del__(self, destroy=_freeswitch.delete_SessionContainer):
        try:
            if self.thisown: destroy(self)
        except: pass

    def console_log(*args): return _freeswitch.SessionContainer_console_log(*args)
    def console_clean_log(*args): return _freeswitch.SessionContainer_console_clean_log(*args)
    def answer(*args): return _freeswitch.SessionContainer_answer(*args)
    def pre_answer(*args): return _freeswitch.SessionContainer_pre_answer(*args)
    def hangup(*args): return _freeswitch.SessionContainer_hangup(*args)
    def set_variable(*args): return _freeswitch.SessionContainer_set_variable(*args)
    def get_variable(*args): return _freeswitch.SessionContainer_get_variable(*args)
    def set_state(*args): return _freeswitch.SessionContainer_set_state(*args)
    def play_file(*args): return _freeswitch.SessionContainer_play_file(*args)
    def set_dtmf_callback(*args): return _freeswitch.SessionContainer_set_dtmf_callback(*args)
    def speak_text(*args): return _freeswitch.SessionContainer_speak_text(*args)
    def set_tts_parms(*args): return _freeswitch.SessionContainer_set_tts_parms(*args)
    def get_digits(*args): return _freeswitch.SessionContainer_get_digits(*args)
    def transfer(*args): return _freeswitch.SessionContainer_transfer(*args)
    def play_and_get_digits(*args): return _freeswitch.SessionContainer_play_and_get_digits(*args)

class SessionContainerPtr(SessionContainer):
    def __init__(self, this):
        _swig_setattr(self, SessionContainer, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, SessionContainer, 'thisown', 0)
        self.__class__ = SessionContainer
_freeswitch.SessionContainer_swigregister(SessionContainerPtr)
cvar = _freeswitch.cvar



