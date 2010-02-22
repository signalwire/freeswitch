#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)

AC_INIT([unimrcp],[0.9.0])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/acmacros])
AC_PREFIX_DEFAULT(/usr/local/unimrcp)
CFLAGS="$CFLAGS $CONFIGURE_CFLAGS"
CXXFLAGS="$CXXFLAGS $CONFIGURE_CXXFLAGS"
LDFLAGS="$LDFLAGS $CONFIGURE_LDFLAGS"

plugindir='${prefix}/plugin'
confdir='${prefix}/conf'
logdir='${prefix}/log'
datadir='${prefix}/data'

AM_INIT_AUTOMAKE(foreign)

# Set default language
AC_LANG_C

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

# Skip detection of Fortran
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])
AC_PROG_LIBTOOL

# Get version information
get_version="build/get-version.sh"
version_hdr="build/uni_version.h"
plugin_version_hdr="libs/mrcp-engine/include/mrcp_engine_plugin.h"
UNI_DOTTED_VERSION="`$get_version all $version_hdr UNI`"
UNI_LT_VERSION="-version-info `$get_version libtool $version_hdr UNI`"
PLUGIN_LT_VERSION="-version-info `$get_version libtool $plugin_version_hdr PLUGIN`"

AC_SUBST(UNI_DOTTED_VERSION)
AC_SUBST(UNI_LT_VERSION)
AC_SUBST(PLUGIN_LT_VERSION)

echo "UniMRCP Version: ${UNI_DOTTED_VERSION}"

# Check APR
UNIMRCP_CHECK_APR
UNIMRCP_CHECK_APU

# Check SOFIA
UNIMRCP_CHECK_SOFIA



AC_SUBST(ac_aux_dir)
AC_SUBST(ac_macro_dir)
AC_SUBST(plugindir)
AC_SUBST(confdir)
AC_SUBST(logdir)

AC_DEFUN([AX_COMPILER_VENDOR],
[
AC_CACHE_CHECK([for _AC_LANG compiler vendor], ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor,
 [ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor=unknown
  # note: don't check for gcc first since some other compilers define __GNUC__
  for ventest in intel:__ICC,__ECC,__INTEL_COMPILER ibm:__xlc__,__xlC__,__IBMC__,__IBMCPP__ gnu:__GNUC__ sun:__SUNPRO_C,__SUNPRO_CC hp:__HP_cc,__HP_aCC dec:__DECC,__DECCXX,__DECC_VER,__DECCXX_VER borland:__BORLANDC__,__TURBOC__ comeau:__COMO__ cray:_CRAYC kai:__KCC lcc:__LCC__ metrowerks:__MWERKS__ sgi:__sgi,sgi microsoft:_MSC_VER watcom:__WATCOMC__ portland:__PGI; do
    vencpp="defined("`echo $ventest | cut -d: -f2 | sed 's/,/) || defined(/g'`")"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[
#if !($vencpp)
      thisisanerror;
#endif
])], [ax_cv_]_AC_LANG_ABBREV[_compiler_vendor=`echo $ventest | cut -d: -f1`; break])
  done
 ])
])

AX_COMPILER_VENDOR

#Enable maintainer mode
AC_ARG_ENABLE(maintainer-mode,
    [AC_HELP_STRING([--enable-maintainer-mode  ],[turn on debugging and compile time warnings])],
    [enable_maintainer_mode="$enableval"],
    [enable_maintainer_mode="no"])

if test "${enable_maintainer_mode}" != "no"; then
    CFLAGS="$CFLAGS -g"
    if test "x${ax_cv_c_compiler_vendor}"  =  "xgnu" ; then
      CFLAGS="$CFLAGS -Wall -Werror"
    fi
fi

#Enable test suites
AC_ARG_ENABLE(test-suites,
    [AC_HELP_STRING([--enable-test-suites  ],[build test suites])],
    [enable_test_suites="$enableval"],
    [enable_test_suites="no"])

AM_CONDITIONAL([TEST_SUITES],[test "${enable_test_suites}" != "no"])


### Plugins ###

#Enable demo synthesizer plugin
AC_ARG_ENABLE(demosynth-plugin,
    [AC_HELP_STRING([--disable-demosynth-plugin  ],[exclude demo synthesizer plugin from build])],
    [enable_demosynth_plugin="$enableval"],
    [enable_demosynth_plugin="yes"])

AM_CONDITIONAL([DEMOSYNTH_PLUGIN],[test "${enable_demosynth_plugin}" = "yes"])


#Enable demo recognizer plugin
AC_ARG_ENABLE(demorecog-plugin,
    [AC_HELP_STRING([--disable-demorecog-plugin  ],[exclude demo recognizer plugin from build])],
    [enable_demorecog_plugin="$enableval"],
    [enable_demorecog_plugin="yes"])

AM_CONDITIONAL([DEMORECOG_PLUGIN],[test "${enable_demorecog_plugin}" = "yes"])


#Enable recorder plugin
AC_ARG_ENABLE(recorder-plugin,
    [AC_HELP_STRING([--disable-recorder-plugin  ],[exclude recorder plugin from build])],
    [enable_recorder_plugin="$enableval"],
    [enable_recorder_plugin="yes"])

AM_CONDITIONAL([RECORDER_PLUGIN],[test "${enable_recorder_plugin}" = "yes"])


#Enable Cepstral Swift plugin
AC_ARG_ENABLE(cepstral-plugin,
    [AC_HELP_STRING([--disable-cepstral-plugin  ],[exclude cepstral plugin from build])],
    [enable_cepstral_plugin="$enableval"],
    [enable_cepstral_plugin="yes"])

if test "${enable_cepstral_plugin}" != "no"; then
    UNIMRCP_CHECK_SWIFT
fi

AM_CONDITIONAL([CEPSTRAL_PLUGIN],[test "${enable_cepstral_plugin}" = "yes" &&\
                                  test "${found_swift}" = "yes"])


#Enable PocketSphinx plugin
AC_ARG_ENABLE(pocketsphinx-plugin,
    [AC_HELP_STRING([--enable-pocketsphinx-plugin  ],[enable pocketsphinx plugin])],
    [enable_pocketsphinx_plugin="$enableval"],
    [enable_pocketsphinx_plugin="no"])

if test "${enable_pocketsphinx_plugin}" != "no"; then
    UNIMRCP_CHECK_SPHINXBASE
    UNIMRCP_CHECK_POCKETSPHINX
fi

AM_CONDITIONAL([POCKETSPHINX_PLUGIN],[test "${enable_pocketsphinx_plugin}" = "yes" &&\
                                      test "${found_pocketsphinx}" = "yes" &&\
                                      test "${found_sphinxbase}" = "yes"])


#Enable Flite plugin
AC_ARG_ENABLE(flite-plugin,
    [AC_HELP_STRING([--enable-flite-plugin  ],[enable flite plugin])],
    [enable_flite_plugin="$enableval"],
    [enable_flite_plugin="no"])

if test "${enable_flite_plugin}" != "no"; then
    UNIMRCP_CHECK_FLITE
fi

AM_CONDITIONAL([FLITE_PLUGIN],[test "${enable_flite_plugin}" = "yes" &&\
                               test "${found_flite}" = "yes"])


AM_CONDITIONAL(ISMAC, [test `uname -s` = Darwin])

AC_CONFIG_FILES([
  Makefile
  libs/Makefile
  libs/apr-toolkit/Makefile
  libs/mpf/Makefile
  libs/mrcp/Makefile
  libs/mrcp-signaling/Makefile
  libs/mrcpv2-transport/Makefile
  libs/mrcp-engine/Makefile
  libs/mrcp-server/Makefile
  libs/mrcp-client/Makefile
  libs/uni-rtsp/Makefile
  modules/Makefile
  modules/mrcp-sofiasip/Makefile
  modules/mrcp-unirtsp/Makefile
  plugins/Makefile
  plugins/mrcp-cepstral/Makefile
  plugins/mrcp-pocketsphinx/Makefile
  plugins/mrcp-flite/Makefile
  plugins/mrcp-recorder/Makefile
  plugins/demo-synth/Makefile
  plugins/demo-recog/Makefile
  platforms/Makefile
  platforms/libunimrcp-server/Makefile
  platforms/libunimrcp-client/Makefile
  platforms/unimrcp-server/Makefile
  platforms/unimrcp-client/Makefile
  platforms/libasr-client/Makefile
  platforms/asr-client/Makefile
  platforms/umc/Makefile
  tests/Makefile
  tests/apttest/Makefile
  tests/mpftest/Makefile
  tests/mrcptest/Makefile
  tests/rtsptest/Makefile
  tests/strtablegen/Makefile
  build/Makefile
  build/pkgconfig/Makefile
  build/pkgconfig/unimrcpclient.pc
  build/pkgconfig/unimrcpserver.pc
  build/pkgconfig/unimrcpplugin.pc
  conf/Makefile
  data/Makefile
])

AC_OUTPUT

AC_MSG_NOTICE
AC_MSG_NOTICE([Report:])
AC_MSG_NOTICE([  UniMRCP: $UNI_DOTTED_VERSION])
AC_MSG_NOTICE([  APR: $apr_version])
AC_MSG_NOTICE([  APR-Util: $apu_version])
AC_MSG_NOTICE([  Sofia-SIP: $sofia_version])
AC_MSG_NOTICE([Plugins:])
AC_MSG_NOTICE([  Demo Synthesizer: $enable_demosynth_plugin])
AC_MSG_NOTICE([  Demo Recognizer: $enable_demorecog_plugin])
AC_MSG_NOTICE([  Cepstral: $enable_cepstral_plugin])
AC_MSG_NOTICE([  PocketSphinx: $enable_pocketsphinx_plugin])
AC_MSG_NOTICE([  Flite: $enable_flite_plugin])
