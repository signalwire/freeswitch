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



fs_core_set_globals = _freeswitch.fs_core_set_globals

fs_core_init = _freeswitch.fs_core_init

fs_core_destroy = _freeswitch.fs_core_destroy

fs_loadable_module_init = _freeswitch.fs_loadable_module_init

fs_loadable_module_shutdown = _freeswitch.fs_loadable_module_shutdown

fs_console_loop = _freeswitch.fs_console_loop

fs_consol_log = _freeswitch.fs_consol_log

fs_consol_clean = _freeswitch.fs_consol_clean

fs_core_session_locate = _freeswitch.fs_core_session_locate

fs_channel_answer = _freeswitch.fs_channel_answer

fs_channel_pre_answer = _freeswitch.fs_channel_pre_answer

fs_channel_hangup = _freeswitch.fs_channel_hangup

fs_channel_set_variable = _freeswitch.fs_channel_set_variable

fs_channel_get_variable = _freeswitch.fs_channel_get_variable

fs_channel_set_state = _freeswitch.fs_channel_set_state

fs_ivr_play_file = _freeswitch.fs_ivr_play_file

fs_switch_ivr_record_file = _freeswitch.fs_switch_ivr_record_file

fs_switch_ivr_sleep = _freeswitch.fs_switch_ivr_sleep

fs_ivr_play_file2 = _freeswitch.fs_ivr_play_file2

fs_switch_ivr_collect_digits_callback = _freeswitch.fs_switch_ivr_collect_digits_callback

fs_switch_ivr_collect_digits_count = _freeswitch.fs_switch_ivr_collect_digits_count

fs_switch_ivr_originate = _freeswitch.fs_switch_ivr_originate

fs_switch_ivr_session_transfer = _freeswitch.fs_switch_ivr_session_transfer

fs_switch_ivr_speak_text = _freeswitch.fs_switch_ivr_speak_text

fs_switch_channel_get_variable = _freeswitch.fs_switch_channel_get_variable

fs_switch_channel_set_variable = _freeswitch.fs_switch_channel_set_variable

