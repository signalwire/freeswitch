dnl UNIMRCP_CHECK_SWIFT

AC_DEFUN([UNIMRCP_CHECK_SWIFT],
[  
    AC_MSG_NOTICE([Cepstral Swift library configuration])

    AC_MSG_CHECKING([for Swift])
    AC_ARG_WITH(swift,
                [  --with-swift=PATH       prefix for installed Swift],
                [swift_path=$withval],
                [swift_path="/opt/swift"]
                )
    
    if test -d "$swift_path"; then
        found_swift="yes"
        UNIMRCP_SWIFT_INCLUDES="-I$swift_path/include"
        UNIMRCP_SWIFT_LIBS="-lswift -lceplex_us -lceplang_en -lm"
        UNIMRCP_SWIFT_LDFLAGS="-L$swift_path/lib/ -R$swift_path/lib/"

        AC_SUBST(UNIMRCP_SWIFT_INCLUDES)
        AC_SUBST(UNIMRCP_SWIFT_LIBS)
        AC_SUBST(UNIMRCP_SWIFT_LDFLAGS)
        
	AC_MSG_RESULT($swift_path)
    else
	AC_MSG_WARN([not found - looked for $swift_path])
    fi
])
