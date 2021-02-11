# - Find APR-util (Apache Portable Runtime utilities) library
# This module finds if APR-util is installed or available in its source dir
# and determines where the include files and libraries are.
# This code sets the following variables:
#
#  APU_FOUND           - have the APR-util libs been found
#  APU_LIBRARIES       - path to the APR-util library
#  APU_INCLUDE_DIRS    - path to where apu.h is found
#  APU_DEFINES         - flags to define to compile with APR-util
#  APU_VERSION_STRING  - version of the APR-util lib found
#
# The APU_STATIC variable can be used to specify whether to prefer
# static version of APR-util library.
# You need to set this variable before calling find_package(APRUtil).
#
# If you'd like to specify the installation of APR-util to use, you should modify
# the following cache variables:
#  APU_LIBRARY             - path to the APR-util library
#  APU_INCLUDE_DIR         - path to where apu.h is found
#  APU_XML_LIBRARY         - path to eXpat library bundled with APR-util
#                            (only needed when linking statically)
# If APR-util not installed, it can be used from the source directory:
#  APU_SOURCE_DIR          - path to compiled APR-util source directory
#                            or APR-util installation prefix

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


option (APU_STATIC "Try to find and link static APR-util library" ${APU_STATIC})
mark_as_advanced (APU_STATIC)

include (SelectLibraryConfigurations)

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

macro (find_apu_static)
	set (_apu_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
	if (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
	else (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .a)
	endif (WIN32)
	set (_apu_hints)
	if (APU_SOURCE_DIR)
		set (_apu_hints ${_apu_hints} "${APU_SOURCE_DIR}/lib"
			"${APU_SOURCE_DIR}/xml/expat/lib" "${APU_SOURCE_DIR}/xml/expat/lib"
			"${APU_SOURCE_DIR}/x64/LibR" "${APU_SOURCE_DIR}/xml/expat/lib/x64/LibR"
			 "${APU_SOURCE_DIR}/.libs" "${APU_SOURCE_DIR}/xml/expat/.libs")
	endif (APU_SOURCE_DIR)
	set (_apu_hints ${_apu_hints} /usr/local/lib)
	find_libs (APU "aprutil-1" "${_apu_hints}")
	find_libs (APU_XML "expat;xml" "${_apu_hints}")
	set (CMAKE_FIND_LIBRARY_SUFFIXES ${_apu_CMAKE_FIND_LIBRARY_SUFFIXES})
endmacro (find_apu_static)

macro (find_apu_dynamic)
	set (_apu_hints)
	if (APU_SOURCE_DIR)
		set (_apu_hints ${_apu_hints} "${APU_SOURCE_DIR}/lib" "${APU_SOURCE_DIR}/x64/Release"
			"${APU_SOURCE_DIR}/.libs")
	endif (APU_SOURCE_DIR)
	set (_apu_hints ${_apu_hints} /usr/local/lib)
	find_libs (APU "libaprutil-1;aprutil-1" "${_apu_hints}")
endmacro (find_apu_dynamic)

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


# Do not override already found APR-util
if ((APU_LIBRARY OR APU_LIBRARY_DEBUG) AND APU_INCLUDE_DIR)
	set (APU_FOUND TRUE)
endif ((APU_LIBRARY OR APU_LIBRARY_DEBUG) AND APU_INCLUDE_DIR)


# First, try to use apu-1-config
if (NOT APU_FOUND)
	set (_apu_hints)
	if (APU_SOURCE_DIR)
		set (_apu_hints "${APU_SOURCE_DIR}/bin" "${APU_SOURCE_DIR}")
	endif (APU_SOURCE_DIR)
	find_program (APU_CONFIG apu-1-config
		HINTS ${_apu_hints} /usr/local/bin)
	mark_as_advanced (APU_CONFIG)
	if (APU_CONFIG)
		execute_process (COMMAND "${APU_CONFIG}" --link-ld --libs
			OUTPUT_VARIABLE APU_LIBRARIES OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process (COMMAND "${APU_CONFIG}" --includes
			OUTPUT_VARIABLE APU_INCLUDE_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process (COMMAND "${APU_CONFIG}" --version
			OUTPUT_VARIABLE APU_VERSION_STRING OUTPUT_STRIP_TRAILING_WHITESPACE)
		if (APU_LIBRARIES AND APU_INCLUDE_DIRS)
			separate_arguments (APU_LIBRARIES)
			sanitize_ldflags (APU_LIBRARIES "${APU_LIBRARIES}")
			set (APU_LIBRARY ${APU_LIBRARIES} CACHE STRING "APR-util link libraries")
			string (REGEX REPLACE " -I" " " APU_INCLUDE_DIRS " ${APU_INCLUDE_DIRS}")
			string (STRIP "${APU_INCLUDE_DIRS}" APU_INCLUDE_DIRS)
			separate_arguments (APU_INCLUDE_DIRS)
			set (APU_INCLUDE_DIR "${APU_INCLUDE_DIRS}" CACHE PATH "APR-util include directory")
			# apu-1-config does not have neither --cppflags nor --cflags
			set (APU_DEFINES CACHE STRING "APR-util compile flags")
			set (APU_VERSION_STRING ${APU_VERSION_STRING} CACHE INTERNAL "APR-util version")
			set (APU_FOUND TRUE)
			set (APU_FROM_CONFIG TRUE CACHE INTERNAL "APR-util found by a config program")
			find_package_message (APRUtil "APR found by apu-1-config" "${APU_CONFIG}")
		endif (APU_LIBRARIES AND APU_INCLUDE_DIRS)
	endif (APU_CONFIG)
endif (NOT APU_FOUND)


# Second, try pkg-config
if (NOT APU_FOUND)
	find_package (PkgConfig)
	if (PKGCONFIG_FOUND)
		# Our patched APR-util is usually not installed in standard system paths
		set (_apu_hints
			/usr/local/lib/pkgconfig
			/usr/local/pkgconfig
			/usr/local/lib)
		if (APU_SOURCE_DIR)
			set (_apu_hints
				"${APU_SOURCE_DIR}/lib/pkgconfig"
				"${APU_SOURCE_DIR}/pkgconfig"
				"${APU_SOURCE_DIR}/lib"
				"${APU_SOURCE_DIR}"
				"${APU_SOURCE_DIR}../apr"
				${_apu_hints})
		endif (APU_SOURCE_DIR)
		# First, try "installed" APR-util
		find_path (APU_PKGCONFIG apr-util-1.pc HINTS ${_apu_hints})
		# Then, "uninstalled", i.e. APR-util source tree
		find_path (APU_PKGCONFIG apr-util.pc HINTS ${_apu_hints})
		mark_as_advanced (APU_PKGCONFIG)
		set (_apu_PKG_CONFIG_PATH "$ENV{PKG_CONFIG_PATH}")
		if (APU_PKGCONFIG)
			set (ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${APU_PKGCONFIG}")
		endif (APU_PKGCONFIG)
		pkg_search_module (XAPU apr-util-1 apr-util)
		if (XAPU_FOUND)
			if (APU_STATIC)
				set (APU_LIBRARIES ${XAPU_STATIC_LDFLAGS})
				set (APU_INCLUDE_DIRS ${XAPU_STATIC_INCLUDE_DIRS})
				set (APU_DEFINES ${XAPU_STATIC_CFLAGS_OTHER} CACHE STRING "APR-Util compile flags")
				set (APU_VERSION_STRING ${XAPU_STATIC_VERSION} CACHE INTERNAL "APR-util version")
			else (APU_STATIC)
				set (APU_LIBRARIES ${XAPU_LDFLAGS})
				set (APU_INCLUDE_DIRS ${XAPU_INCLUDE_DIRS})
				set (APU_DEFINES ${XAPU_CFLAGS_OTHER} CACHE STRING "APR-Util compile flags")
				set (APU_VERSION_STRING ${XAPU_VERSION} CACHE INTERNAL "APR-util version")
			endif (APU_STATIC)
			if (APU_LIBRARIES AND APU_INCLUDE_DIRS)
				sanitize_ldflags (APU_LIBRARIES "${APU_LIBRARIES}")
				set (APU_LIBRARY ${APU_LIBRARIES} CACHE STRING "APR-util link libraries")
				set (APU_INCLUDE_DIR ${APU_INCLUDE_DIRS} CACHE PATH "APR-util include directory")
				set (APU_FOUND TRUE)
				set (APU_FROM_CONFIG TRUE CACHE INTERNAL "APR-util found by a config program")
				find_package_message (APRUtil "APR-util found by pkg-config" "${APU_PKGCONFIG}")
			endif (APU_LIBRARIES AND APU_INCLUDE_DIRS)
		endif (XAPU_FOUND)
		set (ENV{PKG_CONFIG_PATH} "${_apu_PKG_CONFIG_PATH}")
	endif (PKGCONFIG_FOUND)
endif (NOT APU_FOUND)


# Lastly, try heuristics
if (NOT APU_FOUND OR NOT APU_FROM_CONFIG)
	set (APU_FROM_CONFIG FALSE CACHE INTERNAL "APR-util found by a config program")
	if (APU_STATIC)
		find_apu_static ()
		if (NOT APU_LIBRARIES)
			find_package_message (APRUtil "Static APR-util library not found, trying dynamic"
				"[${APU_LIBRARY}][${APU_INCLUDE_DIR}][${APU_STATIC}]")
			find_apu_dynamic ()
		endif (NOT APU_LIBRARIES)
		set (APU_DEFINES -DAPU_DECLARE_STATIC CACHE STRING "APR-Util compile flags")
	else (APU_STATIC)
		find_apu_dynamic ()
		if (NOT APU_LIBRARIES)
			find_package_message (APRUtil "Dynamic APR-util library not found, trying static"
				"[${APU_LIBRARY}][${APU_INCLUDE_DIR}][${APU_STATIC}]")
			find_apu_static ()
		endif (NOT APU_LIBRARIES)
		set (APU_DEFINES CACHE STRING "APR-Util compile flags")
	endif (APU_STATIC)

	if (APU_STATIC AND APU_LIBRARIES)
		if (APU_XML_LIBRARIES)
			set (APU_LIBRARIES ${APU_LIBRARIES} ${APU_XML_LIBRARIES})
		else (APU_XML_LIBRARIES)
			message ("Statically linked APR-util requires eXpat, please set APU_XML_LIBRARY")
		endif (APU_XML_LIBRARIES)
	endif (APU_STATIC AND APU_LIBRARIES)

	set (_apu_hints)
	if (APU_SOURCE_DIR)
		set (_apu_hints ${_apu_hints} "${APU_SOURCE_DIR}/include" "${APU_SOURCE_DIR}/include/apr-1")
	endif (APU_SOURCE_DIR)
	set (_apu_hints ${_apu_hints} /usr/local/include/apr-1)
	find_path (APU_INCLUDE_DIR apu_version.h
		HINTS ${_apu_hints})
	mark_as_advanced (APU_INCLUDE_DIR)
	set (APU_INCLUDE_DIRS ${APU_INCLUDE_DIR})

	if (APU_INCLUDE_DIR)
		list (REMOVE_DUPLICATES APU_INCLUDE_DIRS)
		file (STRINGS "${APU_INCLUDE_DIR}/apu_version.h" _apu_ver
			REGEX "^#define[ \t]+APU_[ACHIJMNOPRT]+_VERSION[ \t]+[0-9]+")
		string (REGEX REPLACE ".*[ \t]APU_MAJOR_VERSION[ \t]+([0-9]+).*" "\\1" _apu_major "${_apu_ver}")
		string (REGEX REPLACE ".*[ \t]APU_MINOR_VERSION[ \t]+([0-9]+).*" "\\1" _apu_minor "${_apu_ver}")
		string (REGEX REPLACE ".*[ \t]APU_PATCH_VERSION[ \t]+([0-9]+).*" "\\1" _apu_patch "${_apu_ver}")
		set (APU_VERSION_STRING "${_apu_major}.${_apu_minor}.${_apu_patch}" CACHE INTERNAL "APR-util version")
	endif (APU_INCLUDE_DIR)
endif (NOT APU_FOUND OR NOT APU_FROM_CONFIG)


# Sanitize repeated calls from cache
if (APU_FROM_CONFIG)
	set (APU_LIBRARIES ${APU_LIBRARY})
	set (APU_INCLUDE_DIRS ${APU_INCLUDE_DIR})
endif (APU_FROM_CONFIG)

mark_as_advanced (APU_DEFINES)
unset (APU_FOUND)
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (APRUtil
	REQUIRED_VARS APU_LIBRARIES APU_INCLUDE_DIRS
	VERSION_VAR APU_VERSION_STRING)
