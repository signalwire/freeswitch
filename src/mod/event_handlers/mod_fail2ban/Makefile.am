include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_fail2ban

mod_LTLIBRARIES = mod_fail2ban.la
mod_fail2ban_la_SOURCES  = mod_fail2ban.c
mod_fail2ban_la_CFLAGS   = $(AM_CFLAGS)
mod_fail2ban_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_fail2ban_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
