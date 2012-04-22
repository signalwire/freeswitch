#
# Arnaldo M Pereira <arnaldo@sangoma.com>
#
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(sched)

IF(NOT DEFINED WIN32)
	INCLUDE_DIRECTORIES(/usr/local/freeswitch/include)
	ADD_DEFINITIONS(-Wall)
	ADD_EXECUTABLE(ftdmsched ftdmsched.c)
	TARGET_LINK_LIBRARIES(ftdmsched freetdm)
ENDIF(NOT DEFINED WIN32)
