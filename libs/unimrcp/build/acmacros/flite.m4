dnl UNIMRCP_CHECK_FLITE

AC_DEFUN([UNIMRCP_CHECK_FLITE],
[  
    AC_MSG_NOTICE([Flite library configuration])

    AC_MSG_CHECKING([for Flite])
    AC_ARG_WITH(flite,
                [  --with-flite=PATH      path to Flite build tree],
                [flite_path=$withval],
                [flite_path="/usr/src/flite"]
                )
    
    found_flite="no"
    
    flite_libdir="build/libs"
    for dir in $flite_path ; do
        cd $dir && flite_dir=`pwd` && cd - > /dev/null
        if test -d "$dir/$flite_libdir"; then
            found_flite="yes"
            UNIMRCP_FLITE_INCLUDES="-I$flite_dir/include"
            UNIMRCP_FLITE_LIBS="$dir/$flite_libdir/libflite_cmu_us_awb.a \
	                        $dir/$flite_libdir/libflite_cmu_us_kal.a \
                            	$dir/$flite_libdir/libflite_cmu_us_rms.a \
                         	$dir/$flite_libdir/libflite_cmu_us_slt.a \
                        	$dir/$flite_libdir/libflite_cmulex.a \
                        	$dir/$flite_libdir/libflite_usenglish.a \
                        	$dir/$flite_libdir/libflite.a"
            break
        fi
    done

    if test x_$found_flite != x_yes; then
        AC_MSG_ERROR(Cannot find Flite - looked for srcdir:$flite_srcdir in $flite_path)
    else
        AC_MSG_RESULT([$found_flite])

case "$host" in
    *darwin*)
	UNIMRCP_FLITE_LIBS="$UNIMRCP_FLITE_LIBS -framework CoreFoundation -framework SystemConfiguration"                                                                ;;
esac

        AC_SUBST(UNIMRCP_FLITE_INCLUDES)
        AC_SUBST(UNIMRCP_FLITE_LIBS)
    fi
])
