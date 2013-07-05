AC_DEFUN([CHECK_LIBUUID],
	[
	PKG_CHECK_MODULES([LIBUUID], [uuid >= 1.41.2],
			  [LIBUUID_FOUND=yes], [LIBUUID_FOUND=no])
	if test "$LIBUUID_FOUND" = "no" ; then
	    PKG_CHECK_MODULES([LIBUUID], [uuid],
			      [LIBUUID_FOUND=yes], [LIBUUID_FOUND=no])
	    if test "$LIBUUID_FOUND" = "no" ; then
		 AC_MSG_WARN([libuuid development package highly recommended!])
	    else
		LIBUUID_INCLUDEDIR=$(pkg-config --variable=includedir uuid)
		LIBUUID_CFLAGS+=" -I$LIBUUID_INCLUDEDIR/uuid "
	    fi
	fi
	AC_SUBST([LIBUUID_CFLAGS])
	AC_SUBST([LIBUUID_LIBS])
	])
