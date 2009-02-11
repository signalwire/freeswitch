#FREESWITCH CMAKE file
#Shane Burrell 2009 (supjigator)


cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(include/ ../libs/libresample/include/ ../libs/srtp/crypto/include/ ../libs/srtp/include/) 

SET ( switch_ivr_SRCS switch_ivr.c include/switch_ivr.h)
ADD_LIBRARY(switch_ivr STATIC ${switch_ivr_SRCS})

SET ( switch_core_SRCS switch_core.c include/switch_core.h include/switch_core.h include/private/switch_core_pvt.h)
ADD_LIBRARY(switch_core STATIC ${switch_core_SRCS})

SET ( switch_core_io_SRCS switch_core_io.c )
ADD_LIBRARY(switch_core_io STATIC ${switch_core_io_SRCS})

SET ( switch_core_asr_SRCS switch_core_asr.c )
ADD_LIBRARY(switch_core_asr STATIC ${switch_core_asr_SRCS})

SET ( switch_apr_SRCS switch_apr.c )
ADD_LIBRARY(switch_apr STATIC ${switch_apr_SRCS})

if( HAVE_NETDB_H)
	add_definitions( -DHAVE_NETDB_H)
endif( HAVE_NETDB_H)

SET ( switch_utils_SRCS switch_utils.c include/switch_utils.h)
ADD_LIBRARY(switch_utils STATIC ${switch_utils_SRCS})

SET ( switch_xml_SRCS switch_xml.c include/switch_xml.h)
ADD_LIBRARY(switch_xml STATIC ${switch_xml_SRCS})

SET ( switch_buffer_SRCS switch_buffer.c include/switch_buffer.h)
ADD_LIBRARY(switch_buffer ${switch_buffer_SRCS})

SET ( switch_caller_SRCS switch_caller.c include/switch_caller.h)
ADD_LIBRARY(switch_caller ${switch_caller_SRCS})

SET ( switch_channel_SRCS switch_channel.c include/switch_channel.h)
ADD_LIBRARY(switch_channel ${switch_channel_SRCS})

SET ( switch_config_SRCS switch_config.c include/switch_config.h)
ADD_LIBRARY(switch_config ${switch_config_SRCS})

SET ( switch_console_SRCS switch_console.c include/switch_console.h)
ADD_LIBRARY(switch_console ${switch_console_SRCS})

SET ( switch_core_codec_SRCS switch_core_codec.c)
ADD_LIBRARY(switch_core_codec ${switch_core_codec_SRCS})

SET ( switch_core_db_SRCS switch_core_db.c include/switch_core_db.h)
ADD_LIBRARY(switch_core_db ${switch_core_db_SRCS})

SET ( switch_core_directory_SRCS switch_core_directory.c)
ADD_LIBRARY(switch_core_directory ${switch_core_directory_SRCS})

SET ( switch_core_event_hook_SRCS switch_core_event_hook.c include/switch_core_event_hook.h)
ADD_LIBRARY(switch_core_event_hook ${switch_core_event_hook_SRCS})



SET ( switch_core_file_SRCS switch_core_file.c)
ADD_LIBRARY(switch_core_file ${switch_core_file_SRCS})

SET ( switch_core_hash_SRCS switch_core_hash.c)
ADD_LIBRARY(switch_core_hash ${switch_core_hash_SRCS})


SET ( switch_core_media_bug_SRCS switch_core_media_bug.c)
ADD_LIBRARY(switch_core_media_bug ${switch_core_media_bug_SRCS})

SET ( switch_core_memory_SRCS switch_core_memory.c)
ADD_LIBRARY(switch_core_memory ${switch_core_memory_SRCS})

SET ( switch_core_port_allocator_SRCS switch_core_port_allocator.c)
ADD_LIBRARY(switch_core_port_allocator ${switch_core_port_allocator_SRCS})

SET ( switch_core_rwlock_SRCS switch_core_rwlock.c)
ADD_LIBRARY(switch_core_rwlock ${switch_core_rwlock_SRCS})

SET ( switch_core_session_SRCS switch_core_session.c)
ADD_LIBRARY(switch_core_session ${switch_core_session_SRCS})

SET ( switch_core_speech_SRCS switch_core_speech.c)
ADD_LIBRARY(switch_core_speech ${switch_core_speech_SRCS})






SET ( switch_core_sqldb_SRCS switch_core_sqldb.c)
ADD_LIBRARY(switch_core_sqldb ${switch_core_sqldb_SRCS})

SET ( switch_core_state_machine_SRCS switch_core_state_machine.c)
ADD_LIBRARY(switch_core_state_machine ${switch_core_state_machine_SRCS})

SET ( switch_core_timer_SRCS switch_core_timer.c)
ADD_LIBRARY(switch_core_timer ${switch_core_timer_SRCS})

SET ( switch_dso_SRCS switch_dso.c include/switch_dso.h)
ADD_LIBRARY(switch_dso ${switch_dso_SRCS})

SET ( switch_event_SRCS switch_event.c include/switch_event.h)
ADD_LIBRARY(switch_event ${switch_event_SRCS})

SET ( switch_ivr_async_SRCS switch_ivr_async.c)
ADD_LIBRARY(switch_ivr_async ${switch_ivr_async_SRCS})

SET ( switch_ivr_bridge_SRCS switch_ivr_bridge.c)
ADD_LIBRARY(switch_ivr_bridge ${switch_ivr_bridge_SRCS})

SET ( switch_ivr_menu_SRCS switch_ivr_menu.c)
ADD_LIBRARY(switch_ivr_menu ${switch_ivr_menu_SRCS})




SET ( switch_ivr_originate_SRCS switch_ivr_originate.c)
ADD_LIBRARY(switch_ivr_originate ${switch_ivr_originate_SRCS})

SET ( switch_ivr_play_say_SRCS switch_ivr_play_say.c)
ADD_LIBRARY(switch_ivr_play_say ${switch_ivr_play_say_SRCS})

SET ( switch_loadable_module_SRCS switch_loadable_module.c include/switch_loadable_module.h)
ADD_LIBRARY(switch_loadable_module ${switch_loadable_module_SRCS})

SET ( switch_log_SRCS switch_log.c include/switch_log.h)
ADD_LIBRARY(switch_log ${switch_log_SRCS})





SET ( switch_odbc_SRCS switch_odbc.c include/switch_odbc.h)
ADD_LIBRARY(switch_odbc ${switch_odbc_SRCS})

SET ( switch_pcm_SRCS switch_pcm.c)
ADD_LIBRARY(switch_pcm ${switch_pcm_SRCS})

SET ( switch_regex_SRCS switch_regex.c include/switch_regex.h)
ADD_LIBRARY(switch_regex ${switch_regex_SRCS})

SET ( switch_resample_SRCS switch_resample.c include/switch_resample.h)
ADD_LIBRARY(switch_resample ${switch_resample_SRCS})

SET ( switch_rtp_SRCS switch_rtp.c include/switch_rtp.h)
ADD_LIBRARY(switch_rtp ${switch_rtp_SRCS})

SET ( switch_scheduler_SRCS switch_scheduler.c include/switch_scheduler.h)
ADD_LIBRARY(switch_scheduler ${switch_scheduler_SRCS})

SET ( switch_stun_SRCS switch_stun.c include/switch_stun.h)
ADD_LIBRARY(switch_stun ${switch_stun_SRCS})

SET ( switch_swig_SRCS switch_swig.c)
ADD_LIBRARY(switch_swig ${switch_swig_SRCS})

SET ( switch_time_SRCS switch_time.c)
ADD_LIBRARY(switch_time ${switch_time_SRCS})






