dnl Copyright (c) 1995  Colin Plumb.  All rights reserved.
dnl For licensing and other legal details, see the file legal.c.
dnl
dnl This file in input to autoconf.  It consists of a series of m4
dnl macros which expand to produce the shell script "configure".
dnl Anything which is not an m4 macro is copied directly to the output.
dnl
dnl Start things up.  If the specified file doesn't exist, configure
dnl will complain.
AC_INIT([libbn], [0.1])

dnl The following tests need to know that we intend to produce a config.h
dnl file, even though this won't expand to any shell script code until
dnl AC_OUTPUT time.  Name it bnconfig.hin to avoid MS-LOSS.
AC_CONFIG_HEADER(bnconfig.h:bnconfig.hin)

dnl Checks for programs.

# Find a compiler to use.
# Check 1) The $CC environment varaible, 2) egcc, 3) gcc, 4) acc, and 5) cc.
dnl This used to be just AC-CHECK-PROGS(CC, gcc acc, cc), but...
# This deals with brain-damaged Sun systems that place a bogus cc or
# acc executable in the $PATH, which just prints an error and exit.
# We deal with this by actually trying to compile a trivial test program.
if eval "test \"`echo '$''{'ac_cv_prog_CC'+set}'`\" = set"; then
  AC_MSG_CHECKING(For C compiler (cached))
  CC="$ac_cv_prog_CC"
  AC_MSG_RESULT($CC)
elif test -n "$CC"; then
  ac_cv_prog_CC="$CC" # Let the user override the test.
  AC_MSG_CHECKING(For C compiler)
  AC_MSG_RESULT($CC)
else
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:"
  echo 'main(){return 0;}' > conftest.$ac_ext
  for ac_prog in egcc gcc acc cc; do
# Extract the first word of "$ac_prog", so it can be a program name with args.
    set dummy $ac_prog; ac_word=$2
    AC_MSG_CHECKING(for $ac_word)
    for ac_dir in $PATH; do
      test -z "$ac_dir" && ac_dir=.
      if test -x "$ac_dir/$ac_word"; then
        CC="$ac_prog"
        if eval $ac_compile; then
          ac_cv_prog_CC="$ac_prog"
        fi
        break
      fi
    done
    CC="$ac_cv_prog_CC"
    if test -n "$CC"; then
      AC_MSG_RESULT($ac_dir/$CC)
      break;
    fi
    AC_MSG_RESULT(no)
  done
  if test ! -n "$CC"; then
    AC_MSG_ERROR(no C compiler found)
  fi
  IFS="$ac_save_ifs"
  rm -f conftest*
fi
AC_SUBST(CC)

AC_CACHE_CHECK(whether we are using GNU CC, ac_cv_prog_gcc,
[dnl The semicolon is to pacify NeXT's syntax-checking cpp.
cat > conftest.c <<EOF
#ifdef __GNUC__
  yes;
#endif
EOF
if ${CC-cc} -E conftest.c 2>&AC_FD_CC | egrep yes >/dev/null 2>&1; then
  ac_cv_prog_gcc=yes
else
  ac_cv_prog_gcc=no
fi])

if test $ac_cv_prog_gcc = yes; then
  if test "${CFLAGS+set}" != set; then
AC_CACHE_CHECK(whether ${CC-cc} accepts -g, ac_cv_prog_gcc_g,
[echo 'void f(){}' > conftest.c
if test -z "`${CC-cc} -g -c conftest.c 2>&1`"; then
  ac_cv_prog_gcc_g=yes
else
  ac_cv_prog_gcc_g=no
fi
rm -f conftest*
])
  fi

  # If we're using GCC, perform some Deep Magic to enable the result to
  # link cleanly with code compiled with a compiler that doesn't understand
  # GCC's support library (-lgcc).  Do a link, to relocatable object form,
  # with just -lgcc.  If it's not GCC, do the normal -c thing.
  # These substitutions are used in the Makefile to force that behaviour.
  GCCMAGIC1='-Wl,-r -nostdlib'
  GCCMAGIC2=-lgcc
else
  GCCMAGIC1=-c
  GCCMAGIC2=
fi
AC_SUBST(GCCMAGIC1)
AC_SUBST(GCCMAGIC2)

# Now, figure out the CFLAGS we want.  If the user didn't *ask*
# for CFLAGS, we're going to use some ideas of our own.
if test "${CFLAGS+set}" != set; then

# First, remember one useful thing that was just figured out,
# namely whether the compiler can take -g with -O.  (Most compilers
# seem to do the opposite of what I want here - if you give both, -g
# overrides and disables optimization.)  This is only done for
# gcc at the moment, and the no/yes combination is possible but
# misleading
if test $ac_cv_prog_gcc$ac_cv_prog_gcc_g = yesyes; then
  CFLAGS=-g
else
  CFLAGS=""
fi

# Now, the whole raison d'e^tre of this library is that it's *fast*.
# So we are *not* happy with autoconf's normal conservative compilation
# flags.  Try to figure out what kind the compiler we're using and soup
# things up a bit.  Also turn on warnings if possible.
# If it's GCC, crank up optimization to -O6, and try to add some
# -m options, too.
# Otherwise, it gets even more ad-hoc, but the test below works for the
# SunPro C compiler and cranks it up to maximum optimization.
dnl
dnl Note that the situation here is actually *worse* than the usual
dnl CPU-COMPANY-SYSTEM system type can detect, because it depends on
dnl the C compiler.  For example, all of the options below (as of the
dnl time this is written) are available on a single machine!
dnl Using compiler-specific checks rather than config.guess seems
dnl entirely appropriate here.
AC_MSG_CHECKING(for useful tuning options (\$TUNE))
if test $ac_cv_prog_gcc = yes; then
  : ${WARN="-Wall -W -Wshadow -Wpointer-arith -Wmissing-prototypes -Wwrite-strings"}
  if test "${TUNE+set}" != set; then
    TUNE=-O6
    case `$CC -v 2>&1` in
    *gcc-lib/sparc-*)
	# Try to use the architecture-detecting tool with SunPro CC.
	if bn_tune=`(fpversion -foption) 2>/dev/null`; then
	  if test "$bn_tune" = xcg92 || test "$bn_tune" = cg92; then
	    TUNE="$TUNE -mv8"
	  elif test "$bn_tune" != xcg89 && test "$bn_tune" != cg89; then
	    TUNE="$TUNE -mv8"
	    bn_tune_guess=yes
	  fi
	else
	  TUNE="$TUNE -mv8"
	  bn_tune_guess=yes
	fi
    esac
  fi
elif $CC -flags 2>&1 | grep SunSoft >/dev/null 2>&1; then
  if test "${WARN+set}" != set; then
    if $CC -flags 2>&1 | grep 'checking' | grep '^-vc' > /dev/null 2>&1; then
      WARN=-vc
    elif $CC -flags 2>&1 | grep 'checking' | grep '^-v ' > /dev/null 2>&1; then
      WARN=-v
    fi
    if $CC -flags 2>&1 | grep '^-xstrconst' > /dev/null 2>&1; then
      WARN="${WARN}${WARN+ }-xstrconst"
    fi
  fi
  # SunPro C compiler - now grok version and platform
  if test "${TUNE+set}" != set; then
    if $CC -flags 2>&1 | grep '^-xO.*5' >/dev/null 2>&1; then
      TUNE=-xO5
    else
      TUNE=-xO4
    fi
    # Architecture: -native iv avail., else as fpversion says, else guess -mv8
    if $CC -flags 2>&1 | grep '^-native' >/dev/null 2>&1; then
      TUNE="$TUNE -native"
    elif bn_tune=`(fpversion -foption) 2>/dev/null`; then
      TUNE="$TUNE -$bn_tune"
    elif $CC -flags 2>&1 | grep '^-xcg92' >/dev/null 2>&1; then
      TUNE="$TUNE -xcg92"
      bn_tune_guess=yes
    fi
  fi
fi
bn_tune_set=${TUNE+set}
# If nothing better is available, turn on -O
: ${TUNE=-O}
AC_MSG_RESULT(${TUNE-none})
if test "$bn_tune_set" != set; then
  AC_MSG_WARN(not optimizing heavily - try setting \$CFLAGS)
elif test "$bn_tune_guess" = yes; then
  AC_MSG_WARN([architecture guessed.  If incorrect, use explicit \$TUNE.])
fi
AC_MSG_CHECKING(for useful warning options (\$WARN))
AC_MSG_RESULT(${WARN-none})
fi 
# ^^ End of "$(CFLAGS+set)" != set condition
AC_SUBST(TUNE)
AC_SUBST(WARN)

# Find "ranlib".  Sone systems don't have or need ranlib.  If so,
# ":" (do nothing) is used instead.
AC_PROG_RANLIB

dnl Checks for libraries.
dnl (we don't have any)

dnl Checks for header files.
AC_HEADER_STDC

if test $ac_cv_header_stdc = yes; then
  AC_DEFINE(HAVE_ASSERT_H)
  AC_DEFINE(HAVE_LIMITS_H)
  AC_DEFINE(HAVE_STDLIB_H)
  AC_DEFINE(HAVE_STRING_H)

else	# If non-ANSI, check for other brokenness.

AC_CHECK_HEADERS(assert.h limits.h stdlib.h string.h)

fi
# ^^ End of non-ANSI header brokenness tests (first part)

# Check that we have <sys/time.h> explicitly.
AC_CHECK_HEADERS(sys/time.h)
AC_HEADER_TIME

dnl Checks for typedefs, structures, and compiler characteristics.
# Verify that the compiler supports const, and that it works.
# A number of compilers sort of support const, but they have bugs
# that will prevent valid programs from compiling.
AC_C_CONST

# See if we have size_t.  (If not, define it as unsigned.)
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_CHECK_FUNCS(clock_gettime gethrvtime getrusage)
dnl The following are for real-time clocks only as of yet.
dnl AC_CHECK_FUNCS(clock_getres gethrtime gettimeofday getitimer setitimer ftime)

# If we don't have ANSI C, see if a few functions are missing that
# we've noticed the lack of before.
if test $ac_cv_header_stdc = yes; then
  AC_DEFINE(HAVE_CLOCK)
  AC_DEFINE(HAVE_TIME)
  AC_DEFINE(HAVE_MEMMOVE)
  AC_DEFINE(HAVE_MEMCPY)
else
AC_CHECK_FUNCS(clock time memmove memcpy)
fi
# ^^ End of non-ANSI header brokenness tests (second part)

# libzrtp - fix it back
#AC_OUTPUT_SUBDIRS(test)

# The big bang!  Produce the output files.  This is config.cache, and
# config.status, which builds the config.h file and a long list of
# Makefiles.
dnl The value specified to AC_CONFIG_HEADER at the top if the file is
dnl used here to produce bnconfig.h.
AC_OUTPUT(Makefile)

echo
echo bnlib is now configured for a generic installation.
echo If you with assembly-language support, edit the Makefile according
echo to the instructions in README.bn.
