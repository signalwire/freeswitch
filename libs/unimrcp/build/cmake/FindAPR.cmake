# - Find APR (Apache Portable Runtime) library
# This module finds if APR is installed or available in its source dir
# and determines where the include files and libraries are.
# This code sets the following variables:
#
#  APR_FOUND           - have the APR libs been found
#  APR_LIBRARIES       - path to the APR library
#  APR_INCLUDE_DIRS    - path to where apr.h is found
#  APR_DEFINES         - flags to define to compile with APR
#  APR_VERSION_STRING  - version of the APR lib found
#
# The APR_STATIC variable can be used to specify whether to prefer
# static version of APR library.
# You need to set this variable before calling find_package(APR).
#
# If you'd like to specify the installation of APR to use, you should modify
# the following cache variables:
#  APR_LIBRARY             - path to the APR library
#  APR_INCLUDE_DIR         - path to where apr.h is found
# If APR not installed, it can be used from the source directory:
#  APR_SOURCE_DIR          - path to compiled APR source directory
#                            or APR installation prefix

#=============================================================================
# Copyright 2014-2015 SpeechTech, s.r.o. http://www.speechtech.cz/en
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#=============================================================================


option (APR_STATIC "Try to find and link static APR library" ${APR_STATIC})
mark_as_advanced (APR_STATIC)

# Try to find library specified by ${libnames}
# in ${hints} and put its path to ${var}_LIBRARY and ${var}_LIBRARY_DEBUG,
# and set ${var}_LIBRARIES similarly to CMake's select_library_configurations macro.
# For 32bit configurations, "/x64/" is replaced with "/".
function (find_libs var libnames hints)
	if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
		string (REGEX REPLACE "[\\\\/][xX]64[\\\\/]" "/" hints "${hints}")
	endif (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	string (REPLACE "/LibR" "/LibD" hints_debug "${hints}")
	string (REPLACE "/Release" "/Debug" hints_debug "${hints_debug}")
	find_library (${var}_LIBRARY
		NAMES ${libnames}
		HINTS ${hints})
	find_library (${var}_LIBRARY_DEBUG
		NAMES ${libnames}
		HINTS ${hints_debug})
	mark_as_advanced (${var}_LIBRARY ${var}_LIBRARY_DEBUG)
	if (${var}_LIBRARY AND ${var}_LIBRARY_DEBUG AND
			NOT (${var}_LIBRARY STREQUAL ${var}_LIBRARY_DEBUG) AND
			(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE))
		set (${var}_LIBRARIES optimized ${${var}_LIBRARY} debug ${${var}_LIBRARY_DEBUG} PARENT_SCOPE)
	elseif (${var}_LIBRARY)
		set (${var}_LIBRARIES ${${var}_LIBRARY} PARENT_SCOPE)
	elseif (${var}_LIBRARY_DEBUG)
		set (${var}_LIBRARIES ${${var}_LIBRARY_DEBUG} PARENT_SCOPE)
	else ()
		set (${var}_LIBRARIES ${var}_LIBRARY-NOTFOUND PARENT_SCOPE)
	endif ()
endfunction (find_libs)

macro (find_apr_static)
	set (_apr_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
	if (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
	else (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .a)
	endif (WIN32)
	set (_apr_hints)
	if (APR_SOURCE_DIR)
		set (_apr_hints ${_apr_hints} "${APR_SOURCE_DIR}/lib"
			"${APR_SOURCE_DIR}/x64/LibR" "${APR_SOURCE_DIR}/.libs")
	endif (APR_SOURCE_DIR)
	set (_apr_hints ${_apr_hints} /usr/local/lib)
	find_libs (APR "apr-1" "${_apr_hints}")
	set (CMAKE_FIND_LIBRARY_SUFFIXES ${_apr_CMAKE_FIND_LIBRARY_SUFFIXES})
endmacro (find_apr_static)

macro (find_apr_dynamic)
	set (_apr_hints)
	if (APR_SOURCE_DIR)
		set (_apr_hints ${_apr_hints} "${APR_SOURCE_DIR}/lib"
			"${APR_SOURCE_DIR}/x64/Release" "${APR_SOURCE_DIR}/.libs")
	endif (APR_SOURCE_DIR)
	set (_apr_hints ${_apr_hints} /usr/local/lib)
	find_libs (APR "libapr-1;apr-1" "${_apr_hints}")
endmacro (find_apr_dynamic)

# Try to transform "-L/path/to -llib -flag" into "/path/to/lib -flag"
# to turn libraries to CMake fashion for e.g. rpath to work
macro (sanitize_ldflags var ldflags)
	set (_apr_hints)
	set (${var})
	foreach (flag ${ldflags})
		if ("${flag}" MATCHES "^-L(.+)$")
			set (_apr_hints ${_apr_hints} "${CMAKE_MATCH_1}")
		endif()
	endforeach (flag)
	foreach (flag ${ldflags})
		if ("${flag}" MATCHES "^-l(.+)$")
			find_library (_lib ${CMAKE_MATCH_1} HINTS ${_apr_hints})
			if (_lib)
				set (${var} ${${var}} ${_lib})
			else (_lib)
				set (${var} ${${var}} ${flag})
			endif (_lib)
			unset (_lib CACHE)
		elseif (NOT "${flag}" MATCHES "^-[lL]")
			set (${var} ${${var}} ${flag})
		endif ()
	endforeach (flag)
	unset(flag)
endmacro (sanitize_ldflags)

include (FindPackageMessage)


# Do not override already found APR
if ((APR_LIBRARY OR APR_LIBRARY_DEBUG) AND APR_INCLUDE_DIR)
	set (APR_FOUND TRUE)
endif ((APR_LIBRARY OR APR_LIBRARY_DEBUG) AND APR_INCLUDE_DIR)


# First, try to use apr-1-config
if (NOT APR_FOUND)
	set (_apr_hints)
	if (APR_SOURCE_DIR)
		set (_apr_hints "${APR_SOURCE_DIR}/bin" "${APR_SOURCE_DIR}")
	endif (APR_SOURCE_DIR)
	find_program (APR_CONFIG apr-1-config
		HINTS ${_apr_hints} /usr/local/bin)
	mark_as_advanced (APR_CONFIG)
	if (APR_CONFIG)
		execute_process (COMMAND "${APR_CONFIG}" --link-ld --libs
			OUTPUT_VARIABLE APR_LIBRARIES OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process (COMMAND "${APR_CONFIG}" --includes
			OUTPUT_VARIABLE APR_INCLUDE_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process (COMMAND "${APR_CONFIG}" --cppflags --cflags
			OUTPUT_VARIABLE APR_DEFINES OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process (COMMAND "${APR_CONFIG}" --version
			OUTPUT_VARIABLE APR_VERSION_STRING OUTPUT_STRIP_TRAILING_WHITESPACE)
		if (APR_LIBRARIES AND APR_INCLUDE_DIRS)
			separate_arguments (APR_LIBRARIES)
			sanitize_ldflags (APR_LIBRARIES "${APR_LIBRARIES}")
			set (APR_LIBRARY ${APR_LIBRARIES} CACHE STRING "APR link libraries")
			string (REGEX REPLACE " -I" " " APR_INCLUDE_DIRS " ${APR_INCLUDE_DIRS}")
			string (STRIP "${APR_INCLUDE_DIRS}" APR_INCLUDE_DIRS)
			separate_arguments (APR_INCLUDE_DIRS)
			set (APR_INCLUDE_DIR "${APR_INCLUDE_DIRS}" CACHE PATH "APR include directory")
			separate_arguments (APR_DEFINES)
			set (APR_DEFINES ${APR_DEFINES} CACHE STRING "APR compile flags")
			set (APR_VERSION_STRING ${APR_VERSION_STRING} CACHE INTERNAL "APR version")
			set (APR_FOUND TRUE)
			set (APR_FROM_CONFIG TRUE CACHE INTERNAL "APR found by a config program")
			find_package_message (APR "APR found by apr-1-config" "${APR_CONFIG}")
		endif (APR_LIBRARIES AND APR_INCLUDE_DIRS)
	endif (APR_CONFIG)
endif (NOT APR_FOUND)


# Second, try pkg-config
if (NOT APR_FOUND)
	find_package (PkgConfig)
	if (PKGCONFIG_FOUND)
		# Our patched APR is usually not installed in standard system paths
		set (_apr_hints
			/usr/local/lib/pkgconfig
			/usr/local/pkgconfig
			/usr/local/lib)
		if (APR_SOURCE_DIR)
			set (_apr_hints
				"${APR_SOURCE_DIR}/lib/pkgconfig"
				"${APR_SOURCE_DIR}/pkgconfig"
				"${APR_SOURCE_DIR}/lib"
				"${APR_SOURCE_DIR}"
				${_apr_hints})
		endif (APR_SOURCE_DIR)
		# First, try "installed" APR
		find_path (APR_PKGCONFIG apr-1.pc HINTS ${_apr_hints})
		# Then, "uninstalled", i.e. APR source tree
		find_path (APR_PKGCONFIG apr.pc HINTS ${_apr_hints})
		mark_as_advanced (APR_PKGCONFIG)
		set (_apr_PKG_CONFIG_PATH "$ENV{PKG_CONFIG_PATH}")
		if (APR_PKGCONFIG)
			set (ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${APR_PKGCONFIG}")
		endif (APR_PKGCONFIG)
		pkg_search_module (XAPR apr-1 apr)
		if (XAPR_FOUND)
			if (APR_STATIC)
				set (APR_LIBRARIES ${XAPR_STATIC_LDFLAGS})
				set (APR_INCLUDE_DIRS ${XAPR_STATIC_INCLUDE_DIRS})
				set (APR_DEFINES ${XAPR_STATIC_CFLAGS_OTHER} CACHE STRING "APR compile flags")
				set (APR_VERSION_STRING ${XAPR_STATIC_VERSION})
			else (APR_STATIC)
				set (APR_LIBRARIES ${XAPR_LDFLAGS})
				set (APR_INCLUDE_DIRS ${XAPR_INCLUDE_DIRS})
				set (APR_DEFINES ${XAPR_CFLAGS_OTHER} CACHE STRING "APR compile flags")
				set (APR_VERSION_STRING ${XAPR_VERSION})
			endif (APR_STATIC)
			if (APR_LIBRARIES AND APR_INCLUDE_DIRS)
				sanitize_ldflags (APR_LIBRARIES "${APR_LIBRARIES}")
				set (APR_LIBRARY ${APR_LIBRARIES} CACHE STRING "APR link libraries")
				set (APR_INCLUDE_DIR ${APR_INCLUDE_DIRS} CACHE PATH "APR include directory")
				set (APR_VERSION_STRING ${APR_VERSION_STRING} CACHE INTERNAL "APR version")
				set (APR_FOUND TRUE)
				set (APR_FROM_CONFIG TRUE CACHE INTERNAL "APR found by a config program")
				find_package_message (APR "APR found by pkg-config" "${APR_PKGCONFIG}")
			endif (APR_LIBRARIES AND APR_INCLUDE_DIRS)
		endif (XAPR_FOUND)
		set (ENV{PKG_CONFIG_PATH} "${_apr_PKG_CONFIG_PATH}")
	endif (PKGCONFIG_FOUND)
endif (NOT APR_FOUND)


# Lastly, try heuristics
if (NOT APR_FOUND OR NOT APR_FROM_CONFIG)
	set (APR_FROM_CONFIG FALSE CACHE INTERNAL "APR found by a config program")
	if (APR_STATIC)
		find_apr_static ()
		if (NOT APR_LIBRARIES)
			find_package_message (APR "Static APR library not found, trying dynamic"
				"[${APR_LIBRARY}][${APR_INCLUDE_DIR}][${APR_STATIC}]")
			find_apr_dynamic ()
		endif (NOT APR_LIBRARIES)
		set (APR_DEFINES -DAPR_DECLARE_STATIC CACHE STRING "APR compile flags")
	else (APR_STATIC)
		find_apr_dynamic ()
		if (NOT APR_LIBRARIES)
			find_package_message (APR "Dynamic APR library not found, trying static"
				"[${APR_LIBRARY}][${APR_INCLUDE_DIR}][${APR_STATIC}]")
			find_apr_static ()
		endif (NOT APR_LIBRARIES)
		set (APR_DEFINES CACHE STRING "APR compile flags")
	endif (APR_STATIC)

	set (_apr_hints)
	if (APR_SOURCE_DIR)
		set (_apr_hints ${_apr_hints} "${APR_SOURCE_DIR}/include" "${APR_SOURCE_DIR}/include/apr-1")
	endif (APR_SOURCE_DIR)
	set (_apr_hints ${_apr_hints} /usr/local/include/apr-1)
	find_path (APR_INCLUDE_DIR apr_version.h
		HINTS ${_apr_hints})
	mark_as_advanced (APR_INCLUDE_DIR)
	set (APR_INCLUDE_DIRS ${APR_INCLUDE_DIR})

	if (APR_INCLUDE_DIR)
		file (STRINGS "${APR_INCLUDE_DIR}/apr_version.h" _apr_ver
			REGEX "^#define[ \t]+APR_[ACHIJMNOPRT]+_VERSION[ \t]+[0-9]+")
		string (REGEX REPLACE ".*[ \t]APR_MAJOR_VERSION[ \t]+([0-9]+).*" "\\1" _apr_major "${_apr_ver}")
		string (REGEX REPLACE ".*[ \t]APR_MINOR_VERSION[ \t]+([0-9]+).*" "\\1" _apr_minor "${_apr_ver}")
		string (REGEX REPLACE ".*[ \t]APR_PATCH_VERSION[ \t]+([0-9]+).*" "\\1" _apr_patch "${_apr_ver}")
		set (APR_VERSION_STRING "${_apr_major}.${_apr_minor}.${_apr_patch}" CACHE INTERNAL "APR version")
	endif (APR_INCLUDE_DIR)
endif (NOT APR_FOUND OR NOT APR_FROM_CONFIG)


# Sanitize repeated calls from cache
if (APR_FROM_CONFIG)
	set (APR_LIBRARIES ${APR_LIBRARY})
	set (APR_INCLUDE_DIRS ${APR_INCLUDE_DIR})
endif (APR_FROM_CONFIG)

mark_as_advanced (APR_DEFINES)
unset (APR_FOUND)
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (APR
	REQUIRED_VARS APR_LIBRARIES APR_INCLUDE_DIRS
	VERSION_VAR APR_VERSION_STRING)
