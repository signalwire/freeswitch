dnl
dnl UNIMRCP_CHECK_SOFIA
dnl
dnl This macro attempts to find the Sofia-SIP library and
dnl set corresponding variables on exit.
dnl
AC_DEFUN([UNIMRCP_CHECK_SOFIA],
[
    AC_MSG_NOTICE([Sofia SIP library configuration])

    AC_MSG_CHECKING([for Sofia-SIP])
    AC_ARG_WITH(sofia-sip,
                [  --with-sofia-sip=PATH   prefix for installed Sofia-SIP,
                          path to Sofia-SIP source/build tree,
                          or the full path to Sofia-SIP pkg-config],
                [sofia_path=$withval],
                [sofia_path="/usr/local"]
                )

    found_sofia="no"

    if test -n "$PKG_CONFIG"; then
        dnl Check for installed Sofia-SIP
        for dir in $sofia_path ; do
            sofia_config_path=$dir/lib/pkgconfig/sofia-sip-ua.pc
            if test -f "$sofia_config_path" && $PKG_CONFIG $sofia_config_path > /dev/null 2>&1; then
                found_sofia="yes"
                break
            fi
        done

        dnl Check for full path to Sofia-SIP pkg-config file
        if test "$found_sofia" != "yes" && test -f "$sofia_path" && $PKG_CONFIG $sofia_path > /dev/null 2>&1 ; then
            found_sofia="yes"
            sofia_config_path=$sofia_path
        fi

        if test "$found_sofia" = "yes" ; then
            UNIMRCP_SOFIA_INCLUDES="`$PKG_CONFIG --cflags $sofia_config_path`"
            UNIMRCP_SOFIA_LIBS="`$PKG_CONFIG --libs $sofia_config_path`"
            sofia_version="`$PKG_CONFIG --modversion $sofia_config_path`"
        fi
    fi

    if test "$found_sofia" != "yes" ; then
        dnl Check for path to Sofia-SIP source/build tree
        for dir in $sofia_path ; do
            sofia_uadir="$dir/libsofia-sip-ua"
            if test -d "$sofia_uadir"; then
                found_sofia="yes"
                sofia_abs_uadir="`cd $sofia_uadir && pwd`"
                UNIMRCP_SOFIA_INCLUDES="-I$sofia_abs_uadir -I$sofia_abs_uadir/bnf -I$sofia_abs_uadir/features -I$sofia_abs_uadir/http -I$sofia_abs_uadir/ipt -I$sofia_abs_uadir/iptsec -I$sofia_abs_uadir/msg -I$sofia_abs_uadir/nea -I$sofia_abs_uadir/nta -I$sofia_abs_uadir/nth -I$sofia_abs_uadir/nua -I$sofia_abs_uadir/sdp -I$sofia_abs_uadir/sip -I$sofia_abs_uadir/soa -I$sofia_abs_uadir/sresolv -I$sofia_abs_uadir/stun -I$sofia_abs_uadir/su -I$sofia_abs_uadir/tport -I$sofia_abs_uadir/url"
                UNIMRCP_SOFIA_LIBS="$sofia_abs_uadir/libsofia-sip-ua.la"
                sofia_version="`sed -n 's/#define SOFIA_SIP_VERSION.* "\(.*\)"/\1/p' $sofia_uadir/features/sofia-sip/sofia_features.h`"
                break
            fi
        done
    fi

    if test $found_sofia != "yes" ; then
        if test -n "$PKG_CONFIG"; then
            AC_MSG_ERROR(Cannot find Sofia-SIP - looked for sofia-config and libsofia-sip-ua in $sofia_path)
        else
            AC_MSG_ERROR(Cannot find Sofia-SIP - pkg-config not available, looked for libsofia-sip-ua in $sofia_path)
        fi
    else
        AC_MSG_RESULT([$found_sofia])
        AC_MSG_RESULT([$sofia_version])

case "$host" in
    *darwin*)
        UNIMRCP_SOFIA_LIBS="$UNIMRCP_SOFIA_LIBS -framework CoreFoundation -framework SystemConfiguration"
        ;;
esac

        AC_SUBST(UNIMRCP_SOFIA_INCLUDES)
        AC_SUBST(UNIMRCP_SOFIA_LIBS)
    fi
])
