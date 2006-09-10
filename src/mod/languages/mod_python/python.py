# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _python
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


fs_core_set_globals = _python.fs_core_set_globals
fs_core_init = _python.fs_core_init
fs_core_destroy = _python.fs_core_destroy
fs_loadable_module_init = _python.fs_loadable_module_init
fs_loadable_module_shutdown = _python.fs_loadable_module_shutdown
fs_console_loop = _python.fs_console_loop
fs_console_clean = _python.fs_console_clean
fs_core_session_locate = _python.fs_core_session_locate
fs_channel_answer = _python.fs_channel_answer
fs_channel_pre_answer = _python.fs_channel_pre_answer
fs_channel_hangup = _python.fs_channel_hangup
fs_channel_set_variable = _python.fs_channel_set_variable
fs_channel_get_variable = _python.fs_channel_get_variable
fs_channel_set_state = _python.fs_channel_set_state
fs_ivr_play_file = _python.fs_ivr_play_file
fs_switch_ivr_record_file = _python.fs_switch_ivr_record_file
fs_switch_ivr_sleep = _python.fs_switch_ivr_sleep
fs_ivr_play_file2 = _python.fs_ivr_play_file2
fs_switch_ivr_collect_digits_callback = _python.fs_switch_ivr_collect_digits_callback
fs_switch_ivr_collect_digits_count = _python.fs_switch_ivr_collect_digits_count
fs_switch_ivr_originate = _python.fs_switch_ivr_originate
fs_switch_ivr_session_transfer = _python.fs_switch_ivr_session_transfer
fs_switch_ivr_speak_text = _python.fs_switch_ivr_speak_text
fs_switch_channel_get_variable = _python.fs_switch_channel_get_variable
fs_switch_channel_set_variable = _python.fs_switch_channel_set_variable

fs_console_log = _python.fs_console_log

