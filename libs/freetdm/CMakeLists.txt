#
# cmake file that generate build files for freetdm.
# this automatically includes the tests and also
# mod_freetdm
#
# Arnaldo M Pereira <arnaldo@sangoma.com>
#
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(freetdm)

ADD_SUBDIRECTORY(sample)
ADD_SUBDIRECTORY(mod_freetdm)

# includes
SET(freetdm_INCLUDES
	${PROJECT_SOURCE_DIR}/src/include 
	${PROJECT_SOURCE_DIR}/src/isdn/include 
	${PROJECT_SOURCE_DIR}/src/include/private
	${PROJECT_SOURCE_DIR}/src/ftmod/ftmod_sangoma_boost
)
INCLUDE_DIRECTORIES(${freetdm_INCLUDES})
LINK_DIRECTORIES(${freetdm_BINARY_DIR})

# optional includes
IF(DEFINED SNGSS7)
	SET(freetdm_INCLUDES ${freetdm_INCLUDES} /usr/include/sng_ss7)
ENDIF(DEFINED SNGSS7)

IF(DEFINED SNGISDN)
	SET(freetdm_INCLUDES ${freetdm_INCLUDES} /usr/include/sng_isdn)
ENDIF(DEFINED SNGISDN)

# definitions / CFLAGS
ADD_DEFINITIONS(-DFTDM_CONFIG_DIR="/FIXME" -DFTDM_MOD_DIR="/FIXME")
IF(DEFINED WIN32)
	ADD_DEFINITIONS(-DFREETDM_EXPORTS -DTELETONE_EXPORTS -DMOD_EXPORTS -DDLL_EXPORTS)
ENDIF(DEFINED WIN32)
IF(DEFINED SNGISDN)
	SET(freetdm_INCLUDES ${freetdm_INCLUDES} /usr/include/sng_isdn)
ENDIF(DEFINED SNGISDN)

# lib sources
SET(freetdm_SOURCES
	${PROJECT_SOURCE_DIR}/src/hashtable.c
	${PROJECT_SOURCE_DIR}/src/hashtable_itr.c
	${PROJECT_SOURCE_DIR}/src/ftdm_io.c
	${PROJECT_SOURCE_DIR}/src/ftdm_queue.c
	${PROJECT_SOURCE_DIR}/src/ftdm_sched.c
	${PROJECT_SOURCE_DIR}/src/ftdm_call_utils.c
	${PROJECT_SOURCE_DIR}/src/ftdm_config.c
	${PROJECT_SOURCE_DIR}/src/ftdm_callerid.c
	${PROJECT_SOURCE_DIR}/src/fsk.c
	${PROJECT_SOURCE_DIR}/src/uart.c
	${PROJECT_SOURCE_DIR}/src/g711.c
	${PROJECT_SOURCE_DIR}/src/libteletone_detect.c
	${PROJECT_SOURCE_DIR}/src/libteletone_generate.c
	${PROJECT_SOURCE_DIR}/src/ftdm_buffer.c
	${PROJECT_SOURCE_DIR}/src/ftdm_threadmutex.c
	${PROJECT_SOURCE_DIR}/src/ftdm_dso.c
	${PROJECT_SOURCE_DIR}/src/ftdm_cpu_monitor.c
)

# libfreetdm.so
ADD_LIBRARY(${PROJECT_NAME} SHARED ${freetdm_SOURCES})

IF(NOT DEFINED WIN32)
	TARGET_LINK_LIBRARIES(${PROJECT_NAME} m pthread dl)
ENDIF(NOT DEFINED WIN32)

REMOVE_DEFINITIONS(-DLL_EXPORTS)

# tools & tests
IF(NOT DEFINED WIN32)
	FOREACH(TOOL testtones testpri testr2 testapp testcid)
		ADD_EXECUTABLE(${TOOL} ${PROJECT_SOURCE_DIR}/src/${TOOL}.c)
		TARGET_LINK_LIBRARIES(${TOOL} -l${PROJECT_NAME})
		ADD_DEPENDENCIES(${TOOL} ${PROJECT_NAME})
	ENDFOREACH(TOOL)

	ADD_EXECUTABLE(detect_dtmf
		${PROJECT_SOURCE_DIR}/src/detect_dtmf.c
		${PROJECT_SOURCE_DIR}/src/libteletone_detect.c
	)
	TARGET_LINK_LIBRARIES(detect_dtmf ${PROJECT_NAME})
	ADD_DEPENDENCIES(detect_dtmf ${PROJECT_NAME})

	ADD_EXECUTABLE(detect_tones
		${PROJECT_SOURCE_DIR}/src/detect_tones.c
		${PROJECT_SOURCE_DIR}/src/libteletone_detect.c
	)
	TARGET_LINK_LIBRARIES(detect_tones ${PROJECT_NAME})
	ADD_DEPENDENCIES(detect_tones ${PROJECT_NAME})

	ADD_EXECUTABLE(testanalog
		${PROJECT_SOURCE_DIR}/src/testanalog.c
	)
	TARGET_LINK_LIBRARIES(testanalog -l${PROJECT_NAME})
	ADD_DEPENDENCIES(testanalog ${PROJECT_NAME})

	# optional tests/tools
	IF(HAVE_SCTP)
		ADD_EXECUTABLE(testboost src/testboost.c)
		TARGET_LINK_LIBRARIES(testboost ${PROJECT_NAME})
	ENDIF(HAVE_SCTP)
ELSE(NOT DEFINED WIN32)
	MESSAGE(WARNING "Not building tools/tests on WIN32 yet.")
ENDIF(NOT DEFINED WIN32)

#
# ftmod modules
#
SET(ftmod_DIR ${PROJECT_SOURCE_DIR}/src/ftmod)

IF(DEFINED WIN32)
	SET(ftmod_ADDITIONAL_SOURCES
		${PROJECT_SOURCE_DIR}/src/ftdm_io.c
		${PROJECT_SOURCE_DIR}/src/ftdm_config.c
		${PROJECT_SOURCE_DIR}/src/ftdm_queue.c
		${PROJECT_SOURCE_DIR}/src/g711.c
	)
	SET(module_list skel analog analog_em)
ELSE(DEFINED WIN32)
	SET(module_list skel analog analog_em zt)
ENDIF(DEFINED WIN32)

# build default modules
FOREACH(module ${module_list})
	ADD_LIBRARY(ftmod_${module} MODULE ${ftmod_DIR}/ftmod_${module}/ftmod_${module}.c ${ftmod_ADDITIONAL_SOURCES})
	TARGET_LINK_LIBRARIES(ftmod_${module} ${PROJECT_NAME})
ENDFOREACH(module)

# build isdn ftmod
IF(DEFINED BUILD_FTMOD_ISDN)
	SET(ftmod_isdn_SOURCES
		${PROJECT_SOURCE_DIR}/src/isdn/EuroISDNStateNT.c
		${PROJECT_SOURCE_DIR}/src/isdn/EuroISDNStateTE.c
		${PROJECT_SOURCE_DIR}/src/isdn/mfifo.c
		${PROJECT_SOURCE_DIR}/src/isdn/Q921.c
		${PROJECT_SOURCE_DIR}/src/isdn/Q931api.c
		${PROJECT_SOURCE_DIR}/src/isdn/Q931.c
		${PROJECT_SOURCE_DIR}/src/isdn/Q931ie.c
		${PROJECT_SOURCE_DIR}/src/isdn/Q931mes.c
		${PROJECT_SOURCE_DIR}/src/isdn/Q931StateNT.c
		${PROJECT_SOURCE_DIR}/src/isdn/Q931StateTE.c
		${PROJECT_SOURCE_DIR}/src/isdn/nationalmes.c
		${PROJECT_SOURCE_DIR}/src/isdn/nationalStateNT.c
		${PROJECT_SOURCE_DIR}/src/isdn/nationalStateTE.c
		${PROJECT_SOURCE_DIR}/src/isdn/DMSmes.c
		${PROJECT_SOURCE_DIR}/src/isdn/DMSStateNT.c
		${PROJECT_SOURCE_DIR}/src/isdn/DMSStateTE.c
		${PROJECT_SOURCE_DIR}/src/isdn/5ESSmes.c
		${PROJECT_SOURCE_DIR}/src/isdn/5ESSStateNT.c
		${PROJECT_SOURCE_DIR}/src/isdn/5ESSStateTE.c
		${PROJECT_SOURCE_DIR}/src/isdn/Q932mes.c
		${ftmod_DIR}/ftmod_isdn/ftmod_isdn.c
	)
	IF(NOT DEFINED WIN32)
		ADD_DEFINITIONS(-D_GNU_SOURCE)
	ENDIF(NOT DEFINED WIN32)
	ADD_LIBRARY(ftmod_isdn MODULE ${ftmod_isdn_SOURCES})
	TARGET_LINK_LIBRARIES(ftmod_isdn ${PROJECT_NAME})
ENDIF(DEFINED BUILD_FTMOD_ISDN)

# from now on, optionals
IF(DEFINED LIBSANGOMA)
	ADD_LIBRARY(ftmod_wanpipe MODULE ${ftmod_DIR}/ftmod_wanpipe/ftmod_wanpipe.c)
	IF(DEFINED WIN32)
		MESSAGE(WARNING "FIXME: look for wanpipe headers on win32")
	ELSE(DEFINED WIN32)
		ADD_DEFINITIONS(-D__LINUX__)
		INCLUDE_DIRECTORIES(/usr/include/wanpipe)
	ENDIF(DEFINED WIN32)
	TARGET_LINK_LIBRARIES(ftmod_wanpipe sangoma ${PROJECT_NAME})
ENDIF(DEFINED LIBSANGOMA)

IF(DEFINED HAVE_SCTP)
	ADD_LIBRARY(ftmod_sangoma_boost MODULE
		${ftmod_DIR}/ftmod_sangoma_boost/sangoma_boost_client.c
		${ftmod_DIR}/ftmod_sangoma_boost/ftmod_sangoma_boost.c
	)
	TARGET_LINK_LIBRARIES(ftmod_sangoma_boost ${PROJECT_NAME})
ENDIF(DEFINED HAVE_SCTP)

IF(DEFINED LIBPRI)
	ADD_LIBRARY(ftmod_libpri MODULE
		${ftmod_DIR}/ftmod_libpri/libpri_client.c
		${ftmod_DIR}/ftmod_libpri/ftmod_libpri.c
	)
	TARGET_LINK_LIBRARIES(ftmod_libpri ${PROJECT_NAME})
ENDIF(DEFINED LIBPRI)

IF(DEFINED PRITAP)
	ADD_LIBRARY(ftmod_pritap MODULE
		${ftmod_DIR}/ftmod_pritap/pritap_client.c
		${ftmod_DIR}/ftmod_pritap/ftmod_pritap.c
	)
	TARGET_LINK_LIBRARIES(ftmod_pritap ${PROJECT_NAME} pri)
ENDIF(DEFINED PRITAP)

IF(DEFINED SNGSS7)
	ADD_LIBRARY(ftmod_sangoma_ss7 MODULE
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_support.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_main.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_handle.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_in.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_out.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_cntrl.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_xml.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_timers.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_cli.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_cfg.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_sta.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_sts.c
		${ftmod_DIR}/ftmod_sangoma_ss7/ftmod_sangoma_ss7_logger.c
	)
	IF(NOT DEFINED WIN32)
		ADD_DEFINITIONS(-D_GNU_SOURCE)
	ENDIF(NOT DEFINED WIN32)
	TARGET_LINK_LIBRARIES(ftmod_sangoma_ss7 ${PROJECT_NAME} sng_ss7)
ENDIF(DEFINED SNGSS7)

IF(DEFINED SNGISDN)
	ADD_LIBRARY(ftmod_sangoma_isdn MODULE
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn.c
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn_cfg.c
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn_cntrl.c
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn_trace.c
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn_support.c
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_cntrl.c
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_cfg.c
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_rcv.c
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_hndl.c
		${ftmod_DIR}/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_out.c
	)
	IF(NOT DEFINED WIN32)
		ADD_DEFINITIONS(-D_GNU_SOURCE)
	ENDIF(NOT DEFINED WIN32)
	TARGET_LINK_LIBRARIES(ftmod_sangoma_isdn ${PROJECT_NAME} sng_isdn)
ENDIF(DEFINED SNGISDN)

IF(DEFINED OPENR2)
	ADD_LIBRARY(ftmod_r2 MODULE ${ftmod_DIR}/ftmod_r2/ftmod_r2.c)
	TARGET_LINK_LIBRARIES(ftmod_r2 ${PROJECT_NAME} openr2)
ENDIF(DEFINED OPENR2)
