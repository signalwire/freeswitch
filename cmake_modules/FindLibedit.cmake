 
find_path(PATH_INC_EDIT NAMES "histedit.h" PATHS ${CMAKE_SOURCE_DIR}/libs/libedit/src/)

if( NOT PATH_INC_EDIT )
        message(FATAL_ERROR"Unable to locate libedit include files" )
endif( NOT PATH_INC_EDIT )

find_library(PATH_LIB_EDIT NAMES "libedit.a" PATHS ${CMAKE_SOURCE_DIR}/libs/libedit/src/.libs/)

if( NOT PATH_LIB_EDIT )
        message(FATAL_ERROR "Unable to locate libedit library file" )
endif( NOT PATH_LIB_EDIT )

MESSAGE( STATUS "PATH_INC_EDIT = \"${PATH_INC_EDIT}\"" )
MESSAGE( STATUS "PATH_LIB_EDIT = \"${PATH_LIB_EDIT}\"" )
