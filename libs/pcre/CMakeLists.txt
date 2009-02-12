cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(include/) 

SET ( pcre_SRCS pcre_compile.c pcre_tables.c pcre_config.c pcre_try_flipped.c pcre_dfa_exec.c pcre_ucp_searchfuncs.c pcre_exec.c pcre_valid_utf8.c pcre_fullinfo.c pcre_version.c dftables.c pcre_get.c pcre_xclass.c pcre_globals.c pcre_info.c pcrecpp.h pcre_internal.h pcre_maketables.c pcrecpparg.h pcre_ord2utf8.c pcredemo.c pcre_refcount.c pcregrep.c pcreposix.c pcre_scanner.h pcreposix.h pcre_scanner_unittest.cc pcretest.c pcre_stringpiece.h pcre_stringpiece.h.in ucp.h pcre.h ucpinternal.h pcre_chartables.c )




ADD_LIBRARY(pcre STATIC ${pcre_SRCS})







