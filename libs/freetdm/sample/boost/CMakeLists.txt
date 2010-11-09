#
# Arnaldo M Pereira <arnaldo@sangoma.com>
#
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(boost)

IF(NOT DEFINED WIN32)
	INCLUDE_DIRECTORIES(/usr/local/freeswitch/include)
	ADD_DEFINITIONS(-Wall)
	ADD_EXECUTABLE(ftdmstart ftdmstart.c)
	TARGET_LINK_LIBRARIES(ftdmstart freetdm)
ENDIF(NOT DEFINED WIN32)
