include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_amqp

if HAVE_AMQP

mod_LTLIBRARIES = mod_amqp.la
mod_amqp_la_SOURCES  = mod_amqp_utils.c mod_amqp_connection.c mod_amqp_producer.c mod_amqp_command.c mod_amqp.c
mod_amqp_la_CFLAGS   = $(AM_CFLAGS) $(AMQP_CFLAGS)
mod_amqp_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_amqp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(AMQP_LIBS) $(SWITCH_AM_LDFLAGS)

else
install: error
all: error
error:
	$(error You must install librabbitmq1 and librabbitmq-dev to build this module)
endif
