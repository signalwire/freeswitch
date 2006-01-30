### all of the standard pc files we need to generate
pcfiles = 							\
	jrtplib.pc

pcfiles_uninstalled =						\
	jrtplib-uninstalled.pc

all-local: $(pcfiles) $(pcfiles_uninstalled)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST =							\
	jrtplib.pc.in						\
	jrtplib-uninstalled.pc.in

CLEANFILES = $(pcfiles) $(pcfiles_uninstalled)
