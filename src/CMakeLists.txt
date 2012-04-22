#FREESWITCH CMAKE file
#Shane Burrell 2009 (supjigator)


cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(include/ ../libs/libresample/include/ ../libs/srtp/crypto/include/ ../libs/srtp/include/ ../libs/speex/include/) 



SET ( freeswitch_la_SRCS 

switch_apr.c 
switch_buffer.c 
switch_caller.c 
switch_channel.c 
switch_console.c 
switch_core_media_bug.c 
switch_core_timer.c 
switch_core_asr.c 
switch_core_event_hook.c 
switch_core_speech.c 
switch_core_memory.c 
switch_core_codec.c 
switch_core_file.c 
switch_core_hash.c 
switch_core_sqldb.c 
switch_core_session.c 
switch_core_directory.c 
switch_core_state_machine.c 
switch_core_io.c 
switch_core_rwlock.c 
switch_core_port_allocator.c 
switch_core.c 
switch_scheduler.c 
switch_core_db.c
switch_dso.c
switch_loadable_module.c 
switch_utils.c 
switch_event.c 
switch_resample.c 
switch_regex.c
switch_rtp.c
switch_ivr_bridge.c 
switch_ivr_originate.c 
switch_ivr_async.c 
switch_ivr_play_say.c 
switch_ivr_menu.c 
switch_ivr.c 
switch_stun.c
switch_log.c
switch_xml.c
switch_config.c
switch_time.c
../libs/stfu/stfu.c
switch_cpp.cpp
g711.c
switch_pcm.c
../libs/libteletone/src/libteletone_detect.c
../libs/libteletone/src/libteletone_generate.c

include/switch_am_config.h
include/switch.h
include/switch_apr.h
include/switch_buffer.h
include/switch_caller.h
include/switch_channel.h
include/switch_console.h
include/switch_core_event_hook.h
include/switch_scheduler.h
include/switch_core.h
include/switch_core_db.h
include/switch_config.h
include/switch_event.h
include/switch_frame.h
include/switch_ivr.h
include/switch_dso.h
include/switch_loadable_module.h
include/switch_module_interfaces.h
include/switch_platform.h
include/switch_resample.h
include/switch_regex.h
include/switch_types.h
include/switch_utils.h
include/switch_rtp.h
include/switch_stun.h
include/switch_log.h
include/switch_xml.h
include/switch_cpp.h
../libs/libteletone/src/libteletone_detect.h
../libs/libteletone/src/libteletone_generate.h
../libs/libteletone/src/libteletone.h
include/switch_odbc.h



)


ADD_LIBRARY(freeswitch_la ${freeswitch_la_SRCS})






