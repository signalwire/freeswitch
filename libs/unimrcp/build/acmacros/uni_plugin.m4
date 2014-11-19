dnl
dnl UNI_PLUGIN_ENABLED(name)
dnl
dnl   where name is the name of the plugin.
dnl
dnl This macro can be used for a plugin which must be enabled by default.
dnl
dnl Adds the following argument to the configure script:
dnl
dnl   --disable-$1-plugin
dnl
dnl Sets the following variable on exit:
dnl
dnl   enable_$1_plugin : "yes" or "no"
dnl
AC_DEFUN([UNI_PLUGIN_ENABLED],[
    AC_ARG_ENABLE(
        [$1-plugin],
        [AC_HELP_STRING([--disable-$1-plugin],[exclude $1 plugin from build])],
        [enable_$1_plugin="$enableval"],
        [enable_$1_plugin="yes"])
])

dnl
dnl UNI_PLUGIN_DISABLED(name)
dnl
dnl   where name is the name of the plugin.
dnl
dnl This macro can be used for a plugin which must be disabled by default.
dnl
dnl Adds the following argument to the configure script:
dnl
dnl   --enable-$1-plugin
dnl
dnl Sets the following variable on exit:
dnl
dnl   enable_$1_plugin : "yes" or "no"
dnl
AC_DEFUN([UNI_PLUGIN_DISABLED],[
    AC_ARG_ENABLE(
        [$1-plugin],
        [AC_HELP_STRING([--enable-$1-plugin],[include $1 plugin in build])],
        [enable_$1_plugin="$enableval"],
        [enable_$1_plugin="no"])
])
