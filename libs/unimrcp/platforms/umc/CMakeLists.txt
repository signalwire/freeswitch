cmake_minimum_required (VERSION 2.8)
project (umc)

# Set header files
set (UMC_HEADERS
	include/dtmfscenario.h
	include/dtmfsession.h
	include/recogscenario.h
	include/recogsession.h
	include/recorderscenario.h
	include/recordersession.h
	include/setparamscenario.h
	include/setparamsession.h
	include/synthscenario.h
	include/synthsession.h
	include/umcconsole.h
	include/umcframework.h
	include/umcscenario.h
	include/umcsession.h
	include/verifierscenario.h
	include/verifiersession.h
)
source_group ("include" FILES ${UMC_HEADERS})

# Set source files
set (UMC_SOURCES
	src/main.cpp
	src/umcconsole.cpp
	src/umcframework.cpp
	src/umcscenario.cpp
	src/umcsession.cpp
	src/synthscenario.cpp
	src/synthsession.cpp
	src/recogscenario.cpp
	src/recogsession.cpp
	src/recorderscenario.cpp
	src/recordersession.cpp
	src/dtmfscenario.cpp
	src/dtmfsession.cpp
	src/setparamscenario.cpp
	src/setparamsession.cpp
	src/verifierscenario.cpp
	src/verifiersession.cpp
)
source_group ("src" FILES ${UMC_SOURCES})

# Application declaration
add_executable (${PROJECT_NAME} ${UMC_SOURCES} ${UMC_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "platforms")

# Input libraries
target_link_libraries(${PROJECT_NAME} unimrcpclient)

# Preprocessor definitions
add_definitions (
	${MRCP_DEFINES}
	${MPF_DEFINES}
	${APR_TOOLKIT_DEFINES} 
	${APR_DEFINES} 
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
	${VERSION_INCLUDE_DIRS}
	${UNIMRCP_CLIENT_INCLUDE_DIRS}
	${MRCP_CLIENT_INCLUDE_DIRS}
	${MRCP_SIGNALING_INCLUDE_DIRS}
	${MRCPv2_TRANSPORT_INCLUDE_DIRS}
	${MRCP_INCLUDE_DIRS}
	${MPF_INCLUDE_DIRS}
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)

# Installation directives
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
if (MSVC)
	install (FILES ${PROJECT_BINARY_DIR}/Debug/${PROJECT_NAME}.pdb DESTINATION bin CONFIGURATIONS Debug)
	install (FILES ${PROJECT_BINARY_DIR}/RelWithDebInfo/${PROJECT_NAME}.pdb DESTINATION bin CONFIGURATIONS RelWithDebInfo)
endif (MSVC)
