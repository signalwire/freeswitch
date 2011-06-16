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
    
    flite_config="config/config"
    for dir in $flite_path ; do
        cd $dir && flite_dir=`pwd` && cd - > /dev/null
        if test -f "$flite_dir/$flite_config"; then
	    target_os=`grep TARGET_OS "$flite_dir/$flite_config" | sed "s/^.*= //"` ;\
	    target_cpu=`grep TARGET_CPU "$flite_dir/$flite_config" | sed "s/^.*= //"` ;\
	    flite_libdir=$flite_dir/build/$target_cpu-$target_os/lib
    	    if test -d "$flite_libdir"; then
		UNIMRCP_FLITE_INCLUDES="-I$flite_dir/include"
		UNIMRCP_FLITE_LIBS="$flite_libdir/libflite_cmu_us_awb.a \
	                        $flite_libdir/libflite_cmu_us_kal.a \
                            	$flite_libdir/libflite_cmu_us_rms.a \
                         	$flite_libdir/libflite_cmu_us_slt.a \
                        	$flite_libdir/libflite_cmulex.a \
                        	$flite_libdir/libflite_usenglish.a \
                        	$flite_libdir/libflite.a"
		found_flite="yes"
		break
    	    else
		AC_MSG_WARN(Cannot find Flite lib dir: $flite_libdir)
	    fi
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
