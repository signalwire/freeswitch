dnl -------------------------------------------------------- -*- autoconf -*-
dnl Copyright 2003-2005 The Apache Software Foundation or its licensors, as
dnl applicable.
dnl
dnl Licensed under the Apache License, Version 2.0 (the "License");
dnl you may not use this file except in compliance with the License.
dnl You may obtain a copy of the License at
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is distributed on an "AS IS" BASIS,
dnl WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl See the License for the specific language governing permissions and
dnl limitations under the License.

dnl -----------------------------------------------------------------
dnl apu-hints.m4: apr-util's autoconf macros for platform-specific hints
dnl
dnl  We preload various configure settings depending
dnl  on previously obtained platform knowledge.
dnl  We allow all settings to be overridden from
dnl  the command-line.

dnl
dnl APU_PRELOAD
dnl
dnl  Preload various build parameters based on outside knowledge.
dnl
AC_DEFUN([APU_PRELOAD], [
if test "x$apu_preload_done" != "xyes" ; then
    apu_preload_done="yes"

    echo "Applying apr-util hints file rules for $host"

    case "$host" in
    *-dec-osf*)
        APR_SETIFNULL(apu_crypt_threadsafe, [1])
        ;;
    *-hp-hpux11.*)
        APR_SETIFNULL(apu_crypt_threadsafe, [1])
        ;;
    *-ibm-aix4*|*-ibm-aix5.1*)
        APR_SETIFNULL(apu_iconv_inbuf_const, [1])
        ;;
    *-ibm-os390)
        APR_SETIFNULL(apu_crypt_threadsafe, [1])
        ;;
    *-solaris2*)
        APR_SETIFNULL(apu_iconv_inbuf_const, [1])
        APR_SETIFNULL(apu_crypt_threadsafe, [1])
        ;;
    *-sco3.2v5*)
	APR_SETIFNULL(apu_db_xtra_libs, [-lsocket])
	;;
    esac

fi
])


