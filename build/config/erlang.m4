AC_DEFUN([CHECK_ERLANG], [
#
# Erlang checks for mod_erlang_event
#
AC_ARG_WITH(
	[erlang],
	[AS_HELP_STRING([--with-erlang], [Use system provided version of erlang (default: try)])],
	[with_erlang="$withval"],
	[with_erlang="try"]
)

AM_CONDITIONAL([HAVE_ERLANG],[false])
if test "$with_erlang" != "no"
then
	save_CFLAGS="$CFLAGS"
	save_LIBS="$LIBS"

	if test "$with_erlang" != "yes" -a "$with_erlang" != "try" ; then
		AC_MSG_CHECKING([for erlang])
		if test ! -x "$with_erlang" ; then
			AC_MSG_ERROR([Specified erlang does not exist or is not executable: $with_erlang])
		fi
		AC_MSG_RESULT([$with_erlang])
		AC_SUBST([ERLANG], ["$with_erlang"])
	else
		AC_PATH_PROG([ERLANG], ["erl"], ["no"], ["$PATH:/usr/bin:/usr/local/bin"])
	fi

	if test "$ERLANG" != "no" ; then
		AC_MSG_CHECKING([erlang version])
		ERLANG_VER="`$ERLANG -version 2>&1 | cut -d' ' -f6`"

		if test -z "$ERLANG_VER" ; then
			AC_MSG_ERROR([Unable to detect erlang version])
		fi
		AC_MSG_RESULT([$ERLANG_VER])

		ERLANG_LIBDIR=`$ERLANG -noshell -eval 'io:format("~n~s/lib~n", [[code:lib_dir("erl_interface")]]).' -s erlang halt | tail -n 1`
		AC_MSG_CHECKING([erlang libdir])
		if test -z "`echo $ERLANG_LIBDIR`" ; then
			AC_MSG_ERROR([failed])
		else
			ERLANG_LDFLAGS="-L$ERLANG_LIBDIR $ERLANG_LDFLAGS"
			LIBS="-L$ERLANG_LIBDIR $LIBS"
		fi
		AC_MSG_RESULT([$ERLANG_LIBDIR])

		ERLANG_INCDIR=`$ERLANG -noshell -eval 'io:format("~n~s/include~n", [[code:lib_dir("erl_interface")]]).' -s erlang halt | tail -n 1`
		AC_MSG_CHECKING([erlang incdir])
		if test -z "`echo $ERLANG_INCDIR`" ; then
			AC_MSG_ERROR([failed])
		else
			ERLANG_CFLAGS="-I$ERLANG_INCDIR $ERLANG_CFLAGS"
			CFLAGS="-I$ERLANG_INCDIR $CFLAGS"
		fi
		AC_MSG_RESULT([$ERLANG_INCDIR])

		AC_CHECK_HEADERS([ei.h], [has_ei_h="yes"], [has_ei_h="no"])

		ERLANG_LIB="ei"

		# check liei
		AC_CHECK_LIB([$ERLANG_LIB], [ei_encode_version], [has_libei="yes"], [has_libei="no"])
		# maybe someday ei will actually expose this?
		AC_CHECK_LIB([$ERLANG_LIB], [ei_link_unlink], [ERLANG_CFLAGS="$ERLANG_CFLAGS -DEI_LINK_UNLINK"])

		if test "$has_libei" = "no" ; then
			AS_IF([test "$with_erlang" = "try"],
			 	[AC_MSG_WARN([$ERLANG_LIB is unusable])],
				[AC_MSG_ERROR([$ERLANG_LIB is unusable])]
			)
		elif test "$has_ei_h" = "no"; then
			AS_IF([test "$with_erlang" = "try"],
				[AC_MSG_WARN([ei.h is unusable - are the erlang development headers installed?])],
				[AC_MSG_ERROR([ei.h is unusable - are the erlang development headers installed?])]
			)
		else
		    ERLANG_MAJOR="`echo "$ERLANG_VER" | sed 's/\([[^.]][[^.]]*\).*/\1/'`"
            ERLANG_MINOR="`echo "$ERLANG_VER" | sed 's/[[^.]][[^.]]*.\([[^.]][[^.]]*\).*/\1/'`"
			ERLANG_LDFLAGS="$ERLANG_LDFLAGS -lei"
			AC_MSG_NOTICE([Your erlang seems OK, do not forget to enable mod_erlang_event in modules.conf])
			AC_SUBST([ERLANG_CFLAGS],  [$ERLANG_CFLAGS])
			AC_SUBST([ERLANG_LDFLAGS], [$ERLANG_LDFLAGS])
            AC_SUBST([ERLANG_VERSION], [$ERLANG_VER])
            AC_SUBST([ERLANG_MAJOR], [$ERLANG_MAJOR])
            AC_SUBST([ERLANG_MINOR], [$ERLANG_MINOR])
			AM_CONDITIONAL([HAVE_ERLANG],[true])
		fi

		LIBS="$save_LIBS"
		CFLAGS="$save_CFLAGS"

	else
		AS_IF([test "$with_erlang" = "try"],
			[AC_MSG_WARN([Could not find erlang, mod_erlang_event will not build, use --with-erlang to specify the location])],
			[AC_MSG_ERROR([Could not find erlang, use --with-erlang to specify the location])]
		)
	fi
else
	AC_MSG_WARN([erlang support disabled, building mod_erlang_event will fail!])
fi

])
