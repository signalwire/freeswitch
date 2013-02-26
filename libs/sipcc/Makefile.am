AM_CFLAGS = -Iinclude  -Icore/includes -Icpr/include -Icore/common -Icore/sipstack/h -I core/ccapp -Icore/sdp -Icore/gsm/h -Iplat/common
AUTOMAKE_OPTIONS = gnu

AM_CPPFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES	= libsipcc.la

CORE_CCAPP_SRC = \
core/ccapp/call_logger.c \
core/ccapp/capability_set.c \
core/ccapp/cc_blf.c \
core/ccapp/cc_call_feature.c \
core/ccapp/cc_config.c \
core/ccapp/cc_device_feature.c \
core/ccapp/cc_device_manager.c \
core/ccapp/cc_info.c \
core/ccapp/cc_service.c \
core/ccapp/ccapi_call.c \
core/ccapp/ccapi_call_info.c \
core/ccapp/ccapi_config.c \
core/ccapp/ccapi_device.c \
core/ccapp/ccapi_device_info.c \
core/ccapp/ccapi_feature_info.c \
core/ccapp/ccapi_line.c \
core/ccapp/ccapi_line_info.c \
core/ccapp/ccapi_service.c \
core/ccapp/ccapi_snapshot.c \
core/ccapp/ccapp_task.c \
core/ccapp/ccprovider.c \
core/ccapp/conf_roster.c \
core/ccapp/sessionHash.c

CORE_COMMON_SRC = \
core/common/cfgfile_utils.c \
core/common/config_api.c \
core/common/config_parser.c \
core/common/init.c \
core/common/logger.c \
core/common/misc.c \
core/common/plat.c \
core/common/platform_api.c \
core/common/prot_configmgr.c \
core/common/resource_manager.c \
core/common/sip_socket_api.c \
core/common/subscription_handler.c \
core/common/text_strings.c \
core/common/ui.c

GSM_SRC = \
core/gsm/ccapi.c \
core/gsm/ccapi_strings.c \
core/gsm/dcsm.c \
core/gsm/fim.c \
core/gsm/fsm.c \
core/gsm/fsmb2bcnf.c \
core/gsm/fsmcac.c \
core/gsm/fsmcnf.c \
core/gsm/fsmdef.c \
core/gsm/fsmxfr.c \
core/gsm/gsm.c \
core/gsm/gsm_sdp.c \
core/gsm/gsm_sdp_crypto.c \
core/gsm/lsm.c \
core/gsm/media_cap_tbl.c \
core/gsm/sm.c \
core/gsm/subapi.c

CORE_SDP_SRC = \
core/sdp/ccsdp.c \
core/sdp/sdp_access.c \
core/sdp/sdp_attr.c \
core/sdp/sdp_attr_access.c \
core/sdp/sdp_base64.c \
core/sdp/sdp_config.c \
core/sdp/sdp_main.c \
core/sdp/sdp_services_unix.c \
core/sdp/sdp_token.c \
core/sdp/sdp_utils.c

CORE_SIPSTACK_SRC = \
core/sipstack/ccsip_callinfo.c \
core/sipstack/ccsip_cc.c \
core/sipstack/ccsip_common_util.c \
core/sipstack/ccsip_core.c \
core/sipstack/ccsip_debug.c \
core/sipstack/ccsip_info.c \
core/sipstack/ccsip_messaging.c \
core/sipstack/ccsip_platform.c \
core/sipstack/ccsip_platform_tcp.c \
core/sipstack/ccsip_platform_timers.c \
core/sipstack/ccsip_platform_tls.c \
core/sipstack/ccsip_platform_udp.c \
core/sipstack/ccsip_pmh.c \
core/sipstack/ccsip_publish.c \
core/sipstack/ccsip_register.c \
core/sipstack/ccsip_reldev.c \
core/sipstack/ccsip_sdp.c \
core/sipstack/ccsip_spi_utils.c \
core/sipstack/ccsip_subsmanager.c \
core/sipstack/ccsip_task.c \
core/sipstack/httpish.c \
core/sipstack/pmhutils.c \
core/sipstack/sip_common_regmgr.c \
core/sipstack/sip_common_transport.c \
core/sipstack/sip_csps_transport.c \
core/sipstack/sip_interface_regmgr.c

CORE_SIPSTACK_UNIX_SRC = core/sipstack/sip_platform_task.c

CORE_SRCCOMMON_SRC = \
core/src-common/configapp.c \
core/src-common/dialplan.c \
core/src-common/dialplanint.c \
core/src-common/digcalc.c \
core/src-common/kpml_common_util.c \
core/src-common/kpmlmap.c \
core/src-common/md5.c \
core/src-common/misc_apps_task.c \
core/src-common/pres_sub_not_handler.c \
core/src-common/publish_int.c \
core/src-common/singly_link_list.c \
core/src-common/sll_lite.c \
core/src-common/string_lib.c \
core/src-common/util_ios_queue.c \
core/src-common/util_parse.c \
core/src-common/util_string.c

CPR_COMMON_SRC = cpr/common/cpr_string.c cpr/common/strtok.c

CPR_DARWIN_SRC = \
cpr/darwin/cpr_darwin_errno.c \
cpr/darwin/cpr_darwin_init.c \
cpr/darwin/cpr_darwin_ipc.c \
cpr/darwin/cpr_darwin_locks.c \
cpr/darwin/cpr_darwin_socket.c \
cpr/darwin/cpr_darwin_stdio.c \
cpr/darwin/cpr_darwin_string.c \
cpr/darwin/cpr_darwin_threads.c \
cpr/darwin/cpr_darwin_timers_using_select.c

CPR_LINUX_SRC = \
cpr/linux/cpr_linux_errno.c \
cpr/linux/cpr_linux_init.c \
cpr/linux/cpr_linux_ipc.c \
cpr/linux/cpr_linux_locks.c \
cpr/linux/cpr_linux_socket.c \
cpr/linux/cpr_linux_stdio.c \
cpr/linux/cpr_linux_string.c \
cpr/linux/cpr_linux_threads.c \
cpr/linux/cpr_linux_timers_using_select.c \
cpr/linux/cpr_linux_tst.c

CPR_WIN32_SRC = \
cpr/win32/cpr_win_debug.c \
cpr/win32/cpr_win_errno.c \
cpr/win32/cpr_win_init.c \
cpr/win32/cpr_win_ipc.c \
cpr/win32/cpr_win_locks.c \
cpr/win32/cpr_win_rand.c \
cpr/win32/cpr_win_socket.c \
cpr/win32/cpr_win_stdio.c \
cpr/win32/cpr_win_string.c \
cpr/win32/cpr_win_threads.c \
cpr/win32/cpr_win_timers.c

PLAT_COMMON_SRC = plat/common/dns_utils.c

PLAT_CSF2G_SRC = \
plat/csf2g/model.c \
plat/csf2g/reset_api.c

PLAT_DARWIN_SRC = plat/darwin/plat_api_stub.c
#plat/darwin/netif.c

libsipcc_la_CFLAGS = $(AM_CFLAGS)  $(SWITCH_AM_CFLAGS) 
libsipcc_la_LDFLAGS = $(LIBS) $(PLATFORM_CORE_LDFLAGS)

libsipcc_la_SOURCES = $(CORE_CCAPP_SRC) $(CORE_COMMON_SRC) $(GSM_SRC) $(CORE_SDP_SRC) $(CORE_SIPSTACK_SRC) $(CORE_SRCCOMMON_SRC) $(CPR_COMMON_SRC) $(PLAT_CSF2G_SRC)

if ISMAC
libsipcc_la_SOURCES += $(CPR_DARWIN_SRC) $(PLAT_DARWIN_SRC) 
libsipcc_la_CFLAGS += -DSIP_OS_OSX -D_POSIX_SOURCE -DCPR_MEMORY_LITTLE_ENDIAN -DNO_SOCKET_POLLING -DUSE_TIMER_SELECT_BASED -DFULL_BUILD -DSTUBBED_OUT -DUSE_PRINTF -D_DARWIN_C_SOURCE -DNO_NSPR_10_SUPPORT
endif

#if not win32
libsipcc_la_SOURCES += $(PLAT_COMMON_SRC) $(CORE_SIPSTACK_UNIX_SRC)
if !ISMAC
libsipcc_la_CFLAGS += -DSIP_OS_LINUX -D_GNU_SOURCE -DCPR_MEMORY_LITTLE_ENDIAN -DNO_SOCKET_POLLING -DUSE_TIMER_SELECT_BASED -DFULL_BUILD -DSTUBBED_OUT -DUSE_PRINTF -DLINUX
libsipcc_la_SOURCES += $(CPR_LINUX_SRC) 
endif
#endif

library_includedir = $(prefix)/include

library_include_HEADERS	= \
core/ccapp/call_logger.h \
core/ccapp/capability_set.h \
core/ccapp/cc_device_manager.h \
core/ccapp/ccapi_snapshot.h \
core/ccapp/ccapp_task.h \
core/ccapp/CCProvider.h \
core/ccapp/conf_roster.h \
core/ccapp/sessionHash.h \
core/common/cfgfile_utils.h \
core/common/config_parser.h \
core/common/logger.h \
core/common/logmsg.h \
core/common/prot_cfgmgr_private.h \
core/common/prot_configmgr.h \
core/common/resource_manager.h \
core/common/subscription_handler.h \
core/common/text_strings.h \
core/gsm/h/fim.h \
core/gsm/h/fsm.h \
core/gsm/h/gsm.h \
core/gsm/h/gsm_sdp.h \
core/gsm/h/lsm.h \
core/gsm/h/lsm_private.h \
core/gsm/h/sm.h \
core/includes/ccapi.h \
core/includes/ccSession.h \
core/includes/check_sync.h \
core/includes/ci.h \
core/includes/config.h \
core/includes/configapp.h \
core/includes/configmgr.h \
core/includes/debug.h \
core/includes/dialplan.h \
core/includes/dialplanint.h \
core/includes/digcalc.h \
core/includes/dns_utils.h \
core/includes/dtmf.h \
core/includes/embedded.h \
core/includes/intelpentiumtypes.h \
core/includes/kpml_common_util.h \
core/includes/kpmlmap.h \
core/includes/md5.h \
core/includes/memory.h \
core/includes/misc_apps_task.h \
core/includes/misc_util.h \
core/includes/phntask.h \
core/includes/phone.h \
core/includes/phone_debug.h \
core/includes/phone_platform_constants.h \
core/includes/phone_types.h \
core/includes/platform_api.h \
core/includes/pres_sub_not_handler.h \
core/includes/publish_int.h \
core/includes/rcc_int_types.h \
core/includes/regexp.h \
core/includes/ringlist.h \
core/includes/rtp_defs.h \
core/includes/scSession.h \
core/includes/session.h \
core/includes/sessionConstants.h \
core/includes/sessionTypes.h \
core/includes/sessuri.h \
core/includes/singly_link_list.h \
core/includes/sip_socket_api.h \
core/includes/sntp.h \
core/includes/string_lib.h \
core/includes/subapi.h \
core/includes/task.h \
core/includes/time2.h \
core/includes/timer.h \
core/includes/tnpphone.h \
core/includes/uart.h \
core/includes/uiapi.h \
core/includes/upgrade.h \
core/includes/util_ios_queue.h \
core/includes/util_parse.h \
core/includes/util_string.h \
core/includes/www.h \
core/includes/xml_defs.h \
core/sdp/sdp.h \
core/sdp/sdp_base64.h \
core/sdp/sdp_os_defs.h \
core/sdp/sdp_private.h \
core/sipstack/h/ccsip_callinfo.h \
core/sipstack/h/ccsip_cc.h \
core/sipstack/h/ccsip_common_cb.h \
core/sipstack/h/ccsip_core.h \
core/sipstack/h/ccsip_credentials.h \
core/sipstack/h/ccsip_macros.h \
core/sipstack/h/ccsip_messaging.h \
core/sipstack/h/ccsip_platform.h \
core/sipstack/h/ccsip_platform_tcp.h \
core/sipstack/h/ccsip_platform_timers.h \
core/sipstack/h/ccsip_platform_tls.h \
core/sipstack/h/ccsip_platform_udp.h \
core/sipstack/h/ccsip_pmh.h \
core/sipstack/h/ccsip_protocol.h \
core/sipstack/h/ccsip_publish.h \
core/sipstack/h/ccsip_register.h \
core/sipstack/h/ccsip_reldev.h \
core/sipstack/h/ccsip_sdp.h \
core/sipstack/h/ccsip_sim.h \
core/sipstack/h/ccsip_spi_utils.h \
core/sipstack/h/ccsip_subsmanager.h \
core/sipstack/h/ccsip_task.h \
core/sipstack/h/httpish.h \
core/sipstack/h/httpish_protocol.h \
core/sipstack/h/pmhdefs.h \
core/sipstack/h/pmhutils.h \
core/sipstack/h/regmgrapi.h \
core/sipstack/h/sip_ccm_transport.h \
core/sipstack/h/sip_common_regmgr.h \
core/sipstack/h/sip_common_transport.h \
core/sipstack/h/sip_csps_transport.h \
core/sipstack/h/sip_interface_regmgr.h \
core/sipstack/h/sip_platform_task.h \
cpr/android/cpr_android_align.h \
cpr/android/cpr_android_assert.h \
cpr/android/cpr_android_errno.h \
cpr/android/cpr_android_in.h \
cpr/android/cpr_android_ipc.h \
cpr/android/cpr_android_locks.h \
cpr/android/cpr_android_private.h \
cpr/android/cpr_android_rand.h \
cpr/android/cpr_android_socket.h \
cpr/android/cpr_android_stdio.h \
cpr/android/cpr_android_string.h \
cpr/android/cpr_android_strings.h \
cpr/android/cpr_android_time.h \
cpr/android/cpr_android_timers.h \
cpr/android/cpr_android_tst.h \
cpr/android/cpr_android_types.h \
cpr/darwin/cpr_darwin_align.h \
cpr/darwin/cpr_darwin_assert.h \
cpr/darwin/cpr_darwin_errno.h \
cpr/darwin/cpr_darwin_in.h \
cpr/darwin/cpr_darwin_ipc.h \
cpr/darwin/cpr_darwin_locks.h \
cpr/darwin/cpr_darwin_private.h \
cpr/darwin/cpr_darwin_rand.h \
cpr/darwin/cpr_darwin_socket.h \
cpr/darwin/cpr_darwin_stdio.h \
cpr/darwin/cpr_darwin_string.h \
cpr/darwin/cpr_darwin_strings.h \
cpr/darwin/cpr_darwin_time.h \
cpr/darwin/cpr_darwin_timers.h \
cpr/darwin/cpr_darwin_tst.h \
cpr/darwin/cpr_darwin_types.h \
cpr/include/cpr.h \
cpr/include/plstr.h \
cpr/include/cpr_assert.h \
cpr/include/cpr_debug.h \
cpr/include/cpr_errno.h \
cpr/include/cpr_in.h \
cpr/include/cpr_ipc.h \
cpr/include/cpr_locks.h \
cpr/include/cpr_memory.h \
cpr/include/cpr_rand.h \
cpr/include/cpr_socket.h \
cpr/include/cpr_stddef.h \
cpr/include/cpr_stdio.h \
cpr/include/cpr_stdlib.h \
cpr/include/cpr_string.h \
cpr/include/cpr_strings.h \
cpr/include/cpr_threads.h \
cpr/include/cpr_time.h \
cpr/include/cpr_timers.h \
cpr/include/cpr_types.h \
cpr/linux/cpr_linux_align.h \
cpr/linux/cpr_linux_assert.h \
cpr/linux/cpr_linux_errno.h \
cpr/linux/cpr_linux_in.h \
cpr/linux/cpr_linux_ipc.h \
cpr/linux/cpr_linux_locks.h \
cpr/linux/cpr_linux_private.h \
cpr/linux/cpr_linux_rand.h \
cpr/linux/cpr_linux_socket.h \
cpr/linux/cpr_linux_stdio.h \
cpr/linux/cpr_linux_string.h \
cpr/linux/cpr_linux_strings.h \
cpr/linux/cpr_linux_time.h \
cpr/linux/cpr_linux_timers.h \
cpr/linux/cpr_linux_tst.h \
cpr/linux/cpr_linux_types.h \
include/cc_blf.h \
include/cc_blf_listener.h \
include/cc_call_feature.h \
include/cc_call_listener.h \
include/cc_config.h \
include/cc_constants.h \
include/cc_debug.h \
include/cc_device_feature.h \
include/cc_device_listener.h \
include/cc_info.h \
include/cc_info_listener.h \
include/cc_service.h \
include/cc_service_listener.h \
include/cc_types.h \
include/ccapi_call.h \
include/ccapi_call_info.h \
include/ccapi_call_listener.h \
include/ccapi_calllog.h \
include/ccapi_conf_roster.h \
include/ccapi_device.h \
include/ccapi_device_info.h \
include/ccapi_device_listener.h \
include/ccapi_feature_info.h \
include/ccapi_line.h \
include/ccapi_line_info.h \
include/ccapi_line_listener.h \
include/ccapi_service.h \
include/ccapi_types.h \
include/ccsdp.h \
include/config_api.h \
include/dns_util.h \
include/peer_connection_types.h \
include/plat_api.h \
include/reset_api.h \
include/sll_lite.h \
include/vcm.h \
include/xml_parser_defines.h \
plat/common/plat_debug.h \
plat/common/tnp_blf.h

