# 
# Copyright (c) 2006-2011 Philip R. Zimmermann. All rights reserved.
# Contact: http://philzimmermann.com
# For licensing and other legal details, see the file zrtp_legal.c.
# 
# Viktor Krikun <v.krikun at zfoneproject.com> 
#

AC_INIT([libzrtp], [1.2.0])

AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADER(config/config.h)

# Checks for target OS
AC_CANONICAL_TARGET

case $target_os in
  aix*)       ;;
  *mingw* | *cygw* | *win32* | *w32* )
  echo "------- START libzrtp configuration for Windows platform ------------"
  ;;
  *darwin*) 
  echo "------- START libzrtp configuration for Darwin platform ------------"
  ;;
  *freebsd2* | *freebsd* | *netbsd* | *openbsd* | *osf[12]*)
  echo "------- START libzrtp configuration for BSD platform ------------"
  ;;
  hpux* | irix* | linuxaout* | linux* | osf* | solaris2* | sunos4*)
  echo "------- START libzrtp configuration for Linux platform ------------"
  ;;
esac


AM_INIT_AUTOMAKE
AX_PREFIX_CONFIG_H(include/zrtp_config_unix.h,ZRTP,config/config.h)

CFLAGS="$CFLAGS -std=c99 -O2 -g3 -Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing -fPIC -DZRTP_AUTOMAKE=1"

# Configuring external libraries
echo "========================= configuring bnlib =============================="
cd third_party/bnlib
./configure CFLAGS="$CFLAGS"
cd ../..
echo "================================ done  ==================================="

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AM_PROG_CC_C_O

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([linux/version.h endian.h])
AC_CHECK_HEADERS([errno.h])
AC_CHECK_HEADERS([asm/types.h])
AC_CHECK_HEADERS([stdlib.h stdint.h stdarg.h])
AC_CHECK_HEADERS([string.h strings.h])
AC_CHECK_HEADERS([stdio.h unistd.h])
AC_CHECK_HEADERS([inttypes.h sys/inttypes.h sys/types.h machine/types.h])
AC_CHECK_HEADERS([pthread.h semaphore.h sys/time.h fcntl.h])

AC_CHECK_TYPES([int8_t,uint8_t,int16_t,uint16_t,int32_t,uint32_t,uint64_t,int64_t])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS([memset memcpy malloc free]) 
AC_CHECK_FUNCS([usleep nanosleep])
AC_CHECK_FUNCS([fopen fread])
AC_CHECK_FUNCS([pthread_mutex_lock pthread_mutex_unlock pthread_mutex_init pthread_mutex_destroy])
AC_CHECK_FUNCS([pthread_attr_init pthread_attr_setdetachstate pthread_create])
AC_CHECK_FUNCS([sem_wait sem_trywait sem_post sem_unlink sem_destroy sem_open sem_init])

AC_CHECK_LIB([pthread], [main], [LIBS="-lpthread $LIBS"], [echo "  Couldn't find library pthread";])

# Other
AC_DEFINE(PRAGMA_PACK_PUSH,[#pragma pack(push, 1)],[Define pragma pack(push) for your platform])
AC_DEFINE(PRAGMA_PACK_POP,[#pragma pack(pop)],[Define pragma pack(pop) for your platform])
AC_DEFINE(INLINE,[static inline],[Define inline construction for your platform])

#
# Documentation
#
AM_CONDITIONAL([HAVE_DOXYGEN], [false])
AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN"; then
  AC_MSG_WARN([Doxygen not found - continuing without Doxygen support])
else
  AM_CONDITIONAL([HAVE_DOXYGEN], [true])
  AC_CONFIG_FILES([doc/Doxyfile])
fi

#
# Generate Makefiles
AC_OUTPUT([Makefile])
