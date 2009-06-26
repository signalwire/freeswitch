dnl UNIMRCP_CHECK_SOFIA

AC_DEFUN([UNIMRCP_CHECK_SOFIA],
[  
    AC_MSG_NOTICE([Sofia SIP library configuration])

    AC_MSG_CHECKING([for Sofia-SIP])
    AC_ARG_WITH(sofia-sip,
                [  --with-sofia-sip=PATH   prefix for installed Sofia-SIP or
                          path to Sofia-SIP build tree],
                [sofia_path=$withval],
                [sofia_path="/usr/local"]
                )
    
    found_sofia="no"
    sofiaconfig="lib/pkgconfig/sofia-sip-ua.pc"
    sofiasrcdir="libsofia-sip-ua"
    for dir in $sofia_path ; do
        cd $dir && sofiadir=`pwd` && cd - > /dev/null
        if test -f "$dir/$sofiaconfig"; then
            found_sofia="yes"
            UNIMRCP_SOFIA_INCLUDES="`pkg-config --cflags $dir/$sofiaconfig`"
            UNIMRCP_SOFIA_LIBS="`pkg-config --libs $dir/$sofiaconfig`"
	    sofia_version="`pkg-config --modversion $dir/$sofiaconfig`"
            break
        fi
        if test -d "$dir/$sofiasrcdir"; then
            found_sofia="yes"
            UNIMRCP_SOFIA_INCLUDES="-I$sofiadir/$sofiasrcdir -I$sofiadir/$sofiasrcdir/bnf -I$sofiadir/$sofiasrcdir/features -I$sofiadir/$sofiasrcdir/http -I$sofiadir/$sofiasrcdir/ipt -I$sofiadir/$sofiasrcdir/iptsec -I$sofiadir/$sofiasrcdir/msg -I$sofiadir/$sofiasrcdir/nea -I$sofiadir/$sofiasrcdir/nta -I$sofiadir/$sofiasrcdir/nth -I$sofiadir/$sofiasrcdir/nua -I$sofiadir/$sofiasrcdir/sdp -I$sofiadir/$sofiasrcdir/sip -I$sofiadir/$sofiasrcdir/soa -I$sofiadir/$sofiasrcdir/sresolv -I$sofiadir/$sofiasrcdir/stun -I$sofiadir/$sofiasrcdir/su -I$sofiadir/$sofiasrcdir/tport -I$sofiadir/$sofiasrcdir/url"
            UNIMRCP_SOFIA_LIBS="$sofiadir/$sofiasrcdir/libsofia-sip-ua.la"
	    sofia_version="`pkg-config --modversion $sofiadir/packages/sofia-sip-ua.pc`"
            break
        fi
    done

    if test x_$found_sofia != x_yes; then
        AC_MSG_ERROR(Cannot find Sofia-SIP - looked for sofia-config:$sofiaconfig and srcdir:$sofiasrcdir in $sofia_path)
    else
        AC_MSG_RESULT([$found_sofia])
        AC_MSG_RESULT([$sofia_version])

case "$host" in
    *darwin*)
	UNIMRCP_SOFIA_LIBS="$UNIMRCP_SOFIA_LIBS -framework CoreFoundation -framework SystemConfiguration"                                                                ;;
esac

        AC_SUBST(UNIMRCP_SOFIA_INCLUDES)
        AC_SUBST(UNIMRCP_SOFIA_LIBS)
    fi
])
