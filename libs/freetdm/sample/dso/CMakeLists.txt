#
# Arnaldo M Pereira <arnaldo@sangoma.com>
#
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(dso)

IF(NOT DEFINED WIN32)
	INCLUDE_DIRECTORIES(/usr/local/freeswitch/include)
	ADD_DEFINITIONS(-Wall)
	ADD_EXECUTABLE(ftdmload ftdmload.c)
	TARGET_LINK_LIBRARIES(ftdmload freetdm)
ENDIF(NOT DEFINED WIN32)
