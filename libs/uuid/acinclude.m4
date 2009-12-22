dnl ##
dnl ##  SA - OSSP Socket Abstraction Library
dnl ##  Copyright (c) 2001-2003 Ralf S. Engelschall <rse@engelschall.com>
dnl ##  Copyright (c) 2001-2003 The OSSP Project <http://www.ossp.org/>
dnl ##  Copyright (c) 2001-2003 Cable & Wireless Deutschland <http://www.cw.com/de/>
dnl ##
dnl ##  This file is part of OSSP SA, a socket abstraction library which
dnl ##  can be found at http://www.ossp.org/pkg/sa/.
dnl ##
dnl ##  Permission to use, copy, modify, and distribute this software for
dnl ##  any purpose with or without fee is hereby granted, provided that
dnl ##  the above copyright notice and this permission notice appear in all
dnl ##  copies.
dnl ##
dnl ##  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
dnl ##  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
dnl ##  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
dnl ##  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
dnl ##  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
dnl ##  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
dnl ##  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
dnl ##  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
dnl ##  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
dnl ##  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
dnl ##  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
dnl ##  SUCH DAMAGE.
dnl ##
dnl ##  aclocal.m4: GNU Autoconf local macro definitions
dnl ##

dnl ##
dnl ##  Check whether compiler option works
dnl ##
dnl ##  configure.in:
dnl ##    AC_COMPILER_OPTION(<name>, <display>, <option>,
dnl ##                       <action-success>, <action-failure>)
dnl ##

AC_DEFUN([AC_COMPILER_OPTION],[dnl
AC_MSG_CHECKING(whether compiler option(s) $2 work)
AC_CACHE_VAL(ac_cv_compiler_option_$1,[
SAVE_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $3"
AC_TRY_COMPILE([],[], ac_cv_compiler_option_$1=yes, ac_cv_compiler_option_$1=no)
CFLAGS="$SAVE_CFLAGS"
])dnl
if test ".$ac_cv_compiler_option_$1" = .yes; then
    ifelse([$4], , :, [$4])
else
    ifelse([$5], , :, [$5])
fi
AC_MSG_RESULT([$ac_cv_compiler_option_$1])
])dnl

dnl ##
dnl ##  Debugging Support
dnl ##
dnl ##  configure.in:
dnl ##    AC_CHECK_DEBUGGING
dnl ##

AC_DEFUN([AC_CHECK_DEBUGGING],[dnl
AC_ARG_ENABLE(debug,dnl
[  --enable-debug          build for debugging (default=no)],
[dnl
if test ".$ac_cv_prog_gcc" = ".yes"; then
    case "$CFLAGS" in
        *-O* ) ;;
           * ) CFLAGS="$CFLAGS -O2" ;;
    esac
    case "$CFLAGS" in
        *-g* ) ;;
           * ) CFLAGS="$CFLAGS -g" ;;
    esac
    case "$CFLAGS" in
        *-pipe* ) ;;
              * ) AC_COMPILER_OPTION(pipe, -pipe, -pipe, CFLAGS="$CFLAGS -pipe") ;;
    esac
    AC_COMPILER_OPTION(defdbg, -DDEBUG, -DDEBUG, CFLAGS="$CFLAGS -DDEBUG")
    CFLAGS="$CFLAGS -pedantic"
    CFLAGS="$CFLAGS -Wall"
    WMORE="-Wshadow -Wpointer-arith -Wcast-align -Winline"
    WMORE="$WMORE -Wmissing-prototypes -Wmissing-declarations -Wnested-externs"
    AC_COMPILER_OPTION(wmore, -W<xxx>, $WMORE, CFLAGS="$CFLAGS $WMORE")
    AC_COMPILER_OPTION(wnolonglong, -Wno-long-long, -Wno-long-long, CFLAGS="$CFLAGS -Wno-long-long")
else
    case "$CFLAGS" in
        *-g* ) ;;
           * ) CFLAGS="$CFLAGS -g" ;;
    esac
fi
msg="enabled"
],[
if test ".$ac_cv_prog_gcc" = ".yes"; then
case "$CFLAGS" in
    *-pipe* ) ;;
          * ) AC_COMPILER_OPTION(pipe, -pipe, -pipe, CFLAGS="$CFLAGS -pipe") ;;
esac
fi
case "$CFLAGS" in
    *-g* ) CFLAGS=`echo "$CFLAGS" |\
                   sed -e 's/ -g / /g' -e 's/ -g$//' -e 's/^-g //g' -e 's/^-g$//'` ;;
esac
case "$CXXFLAGS" in
    *-g* ) CXXFLAGS=`echo "$CXXFLAGS" |\
                     sed -e 's/ -g / /g' -e 's/ -g$//' -e 's/^-g //g' -e 's/^-g$//'` ;;
esac
msg=disabled
])dnl
AC_MSG_CHECKING(for compilation debug mode)
AC_MSG_RESULT([$msg])
if test ".$msg" = .enabled; then
    enable_shared=no
fi
])

dnl ##
dnl ##  Check for C99 va_copy() implementation
dnl ##  (and provide fallback implementation if neccessary)
dnl ##
dnl ##  configure.in:
dnl ##    AC_CHECK_VA_COPY
dnl ##  foo.c:
dnl ##    #include "config.h"
dnl ##    [...]
dnl ##    va_copy(d,s)
dnl ##
dnl ##  This check is rather complex: first because we really have to
dnl ##  try various possible implementations in sequence and second, we
dnl ##  cannot define a macro in config.h with parameters directly.
dnl ##

dnl #   test program for va_copy() implementation
changequote(<<,>>)
m4_define(__va_copy_test, <<[
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#define DO_VA_COPY(d, s) $1
void test(char *str, ...)
{
    va_list ap, ap2;
    int i;
    va_start(ap, str);
    DO_VA_COPY(ap2, ap);
    for (i = 1; i <= 9; i++) {
        int k = (int)va_arg(ap, int);
        if (k != i)
            abort();
    }
    DO_VA_COPY(ap, ap2);
    for (i = 1; i <= 9; i++) {
        int k = (int)va_arg(ap, int);
        if (k != i)
            abort();
    }
    va_end(ap);
}
int main(int argc, char *argv[])
{
    test("test", 1, 2, 3, 4, 5, 6, 7, 8, 9);
    exit(0);
}
]>>)
changequote([,])

dnl #   test driver for va_copy() implementation
m4_define(__va_copy_check, [
    AH_VERBATIM($1,
[/* Predefined possible va_copy() implementation (id: $1) */
#define __VA_COPY_USE_$1(d, s) $2])
    if test ".$ac_cv_va_copy" = .; then
        AC_TRY_RUN(__va_copy_test($2), [ac_cv_va_copy="$1"])
    fi
])

dnl #   Autoconf check for va_copy() implementation checking
AC_DEFUN([AC_CHECK_VA_COPY],[
  dnl #   provide Autoconf display check message
  AC_MSG_CHECKING(for va_copy() function)
  dnl #   check for various implementations in priorized sequence   
  AC_CACHE_VAL(ac_cv_va_copy, [
    ac_cv_va_copy=""
    dnl #   1. check for standardized C99 macro
    __va_copy_check(C99, [va_copy((d), (s))])
    dnl #   2. check for alternative/deprecated GCC macro
    __va_copy_check(GCM, [VA_COPY((d), (s))])
    dnl #   3. check for internal GCC macro (high-level define)
    __va_copy_check(GCH, [__va_copy((d), (s))])
    dnl #   4. check for internal GCC macro (built-in function)
    __va_copy_check(GCB, [__builtin_va_copy((d), (s))])
    dnl #   5. check for assignment approach (assuming va_list is a struct)
    __va_copy_check(ASS, [do { (d) = (s); } while (0)])
    dnl #   6. check for assignment approach (assuming va_list is a pointer)
    __va_copy_check(ASP, [do { *(d) = *(s); } while (0)])
    dnl #   7. check for memory copying approach (assuming va_list is a struct)
    __va_copy_check(CPS, [memcpy((void *)&(d), (void *)&(s), sizeof((s)))])
    dnl #   8. check for memory copying approach (assuming va_list is a pointer)
    __va_copy_check(CPP, [memcpy((void *)(d), (void *)(s), sizeof(*(s)))])
    if test ".$ac_cv_va_copy" = .; then
        AC_ERROR([no working implementation found])
    fi
  ])
  dnl #   optionally activate the fallback implementation
  if test ".$ac_cv_va_copy" = ".C99"; then
      AC_DEFINE(HAVE_VA_COPY, 1, [Define if va_copy() macro exists (and no fallback implementation is required)])
  fi
  dnl #   declare which fallback implementation to actually use
  AC_DEFINE_UNQUOTED([__VA_COPY_USE], [__VA_COPY_USE_$ac_cv_va_copy],
      [Define to id of used va_copy() implementation])
  dnl #   provide activation hook for fallback implementation
  AH_VERBATIM([__VA_COPY_ACTIVATION],
[/* Optional va_copy() implementation activation */
#ifndef HAVE_VA_COPY
#define va_copy(d, s) __VA_COPY_USE(d, s)
#endif
])
  dnl #   provide Autoconf display result message
  if test ".$ac_cv_va_copy" = ".C99"; then
      AC_MSG_RESULT([yes])
  else
      AC_MSG_RESULT([no (using fallback implementation)])
  fi
])

dnl ##
dnl ##  Check for an external/extension library.
dnl ##  - is aware of <libname>-config style scripts
dnl ##  - searches under standard paths include, lib, etc.
dnl ##  - searches under subareas like .libs, etc.
dnl ##
dnl ##  configure.in:
dnl ##      AC_CHECK_EXTLIB(<realname>, <libname>, <func>, <header>,
dnl ##                      [<success-action> [, <fail-action>]])
dnl ##  Makefile.in:
dnl ##      CFLAGS  = @CFLAGS@
dnl ##      LDFLAGS = @LDFLAGS@
dnl ##      LIBS    = @LIBS@
dnl ##  shell:
dnl ##      $ ./configure --with-<libname>[=DIR]
dnl ##

AC_DEFUN([AC_CHECK_EXTLIB],[dnl
AC_ARG_WITH($2, [dnl
[  --with-]m4_substr([$2[[=DIR]]                     ], 0, 19)[build with external $1 library (default=no)]], [dnl
    if test ".$with_$2" = .yes; then
        #   via config script in PATH
        $2_version=`($2-config --version) 2>/dev/null`
        if test ".$$2_version" != .; then
            CPPFLAGS="$CPPFLAGS `$2-config --cflags`"
            CFLAGS="$CFLAGS `$2-config --cflags`"
            LDFLAGS="$LDFLAGS `$2-config --ldflags`"
        fi
    else
        if test -d "$with_$2"; then
            found=0
            #   via config script
            for dir in $with_$2/bin $with_$2; do
                if test -f "$dir/$2-config" && test ! -f "$dir/$2-config.in"; then
                    $2_version=`($dir/$2-config --version) 2>/dev/null`
                    if test ".$$2_version" != .; then
                        CPPFLAGS="$CPPFLAGS `$dir/$2-config --cflags`"
                        CFLAGS="$CFLAGS `$dir/$2-config --cflags`"
                        LDFLAGS="$LDFLAGS `$dir/$2-config --ldflags`"
                        found=1
                        break
                    fi
                fi
            done
            #   in standard sub-areas
            if test ".$found" = .0; then
                for dir in $with_$2/include/$2 $with_$2/include $with_$2; do
                    if test -f "$dir/$4"; then
                        CPPFLAGS="$CPPFLAGS -I$dir"
                        CFLAGS="$CFLAGS -I$dir"
                        found=1
                        break
                    fi
                done
                for dir in $with_$2/lib/$2 $with_$2/lib $with_$2; do
                    if test -f "$dir/lib$2.la" && test -d "$dir/.libs"; then
                        LDFLAGS="$LDFLAGS -L$dir -L$dir/.libs"
                        found=1
                        break
                    elif test -f "$dir/lib$2.a" || test -f "$dir/lib$2.so"; then
                        LDFLAGS="$LDFLAGS -L$dir"
                        found=1
                        break
                    fi
                done
            fi
            #   in any sub-area
            if test ".$found" = .0; then
changequote(, )dnl
                for file in x `find $with_$2 -name "$4" -type f -print`; do
                    test .$file = .x && continue
                    dir=`echo $file | sed -e 's;[^/]*$;;' -e 's;\(.\)/$;\1;'`
                    CPPFLAGS="$CPPFLAGS -I$dir"
                    CFLAGS="$CFLAGS -I$dir"
                done
                for file in x `find $with_$2 -name "lib$2.[aso]" -type f -print`; do
                    test .$file = .x && continue
                    dir=`echo $file | sed -e 's;[^/]*$;;' -e 's;\(.\)/$;\1;'`
                    LDFLAGS="$LDFLAGS -L$dir"
                done
changequote([, ])dnl
            fi
        fi
    fi
    AC_HAVE_HEADERS($4)
    AC_CHECK_LIB($2, $3)
    with_$2=yes
    ac_var="ac_cv_header_`echo $4 | sed 'y%./+-%__p_%'`"
    eval "ac_val=\$$ac_var"
    if test ".$ac_val" != .yes; then
        with_$2=no
    fi
    if test ".$ac_cv_lib_$2_$3" != .yes; then
        with_$2=no
    fi
    if test ".$with_$2" = .no; then
        AC_ERROR([Unable to find $1 library])
    fi
    ], [dnl
if test ".$with_$2" = .; then
    with_$2=no
fi
    ])dnl
AC_MSG_CHECKING(whether to build against external $1 library)
if test ".$with_$2" = .yes; then
    ifelse([$5], , :, [$5])
else
    ifelse([$6], , :, [$6])
fi
AC_MSG_RESULT([$with_$2])
])dnl

dnl ##
dnl ##  Check whether to activate Dmalloc
dnl ##
dnl ##  configure.in:
dnl ##    AC_CHECK_DMALLOC
dnl ##

AC_DEFUN([AC_CHECK_DMALLOC],[dnl
AC_CHECK_EXTLIB(Dmalloc, dmalloc, dmalloc_debug, dmalloc.h,
                AC_DEFINE(WITH_DMALLOC, 1, [define if building with Dmalloc]))
if test ".$with_dmalloc" = .yes; then
    CFLAGS=`echo "X$CFLAGS" | sed -e 's;^X;;' -e 's; -Wredundant-decls;;'`
fi
])dnl

