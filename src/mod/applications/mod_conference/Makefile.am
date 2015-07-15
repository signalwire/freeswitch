include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_conference

mod_LTLIBRARIES = mod_conference.la
mod_conference_la_SOURCES  = mod_conference.c mod_conference_api.c mod_conference_loop.c mod_conference_al.c mod_conference_cdr.c mod_conference_video.c
mod_conference_la_SOURCES += mod_conference_event.c mod_conference_member.c mod_conference_utils.c mod_conference_file.c mod_conference_record.c
mod_conference_la_CFLAGS   = $(AM_CFLAGS) -I.
mod_conference_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_conference_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_OPENAL
mod_conference_la_LDFLAGS += -lopenal -lm
mod_conference_la_CFLAGS += -DOPENAL_POSITIONING
endif

