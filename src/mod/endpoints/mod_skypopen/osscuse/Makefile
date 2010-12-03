# These can be overridden if needed
# DESTDIR is completely respected
CC := gcc
AR := ar
CFLAGS := -Wall $(CFLAGS)
XLDFLAGS := $(LDFLAGS)
LDFLAGS := -L. -lossp $(LDFLAGS)
prefix := /usr/local
DESTDIR :=
UDEVDIR := /etc/udev/rules.d

ifeq "$(origin OSSPD_CFLAGS)" "undefined"
OSSPD_CFLAGS := $(shell pkg-config --cflags fuse)
endif

ifeq "$(origin OSSPD_LDFLAGS)" "undefined"
OSSPD_LDFLAGS := $(shell pkg-config --libs fuse)
endif

ifeq "$(origin OSSP_PADSP_CFLAGS)" "undefined"
OSSP_PADSP_CFLAGS := $(shell pkg-config --cflags libpulse)
endif

ifeq "$(origin OSSP_PADSP_LDFLAGS)" "undefined"
OSSP_PADSP_LDFLAGS := $(shell pkg-config --libs libpulse)
endif

ifeq "$(origin OSSP_ALSAP_CFLAGS)" "undefined"
OSSP_ALSAP_CFLAGS := $(shell pkg-config --libs alsa)
endif

ifeq "$(origin OSSP_ALSAP_LDFLAGS)" "undefined"
OSSP_ALSAP_LDFLAGS := $(shell pkg-config --libs alsa)
endif

headers := ossp.h ossp-util.h ossp-slave.h

#all: osspd ossp-padsp ossp-alsap
all: osspd ossp-alsap

install:
	mkdir -p $(DESTDIR)$(prefix)/sbin
	install -m755 osspd ossp-padsp ossp-alsap $(DESTDIR)$(prefix)/sbin
	mkdir -p $(DESTDIR)$(UDEVDIR)
	install -m644 98-osscuse.rules $(DESTDIR)$(UDEVDIR)

libossp.a: ossp.c ossp.h ossp-util.c ossp-util.h ossp-slave.c ossp-slave.h
	$(CC) $(CFLAGS) -c -o ossp.o ossp.c
	$(CC) $(CFLAGS) -c -o ossp-util.o ossp-util.c
	$(CC) $(CFLAGS) -c -o ossp-slave.o ossp-slave.c
	$(AR) rc $@ ossp.o ossp-util.o ossp-slave.o

osspd: osspd.c libossp.a $(headers)
	$(CC) $(CFLAGS) $(OSSPD_CFLAGS) -o $@ $< $(OSSPD_LDFLAGS) $(LDFLAGS)

ossp-padsp: ossp-padsp.c libossp.a $(headers)
	$(CC) $(CFLAGS) $(OSSP_PADSP_CFLAGS) -o $@ $< $(OSSP_PADSP_LDFLAGS) $(LDFLAGS)

ossp-alsap: ossp-alsap.c libossp.a $(headers)
	$(CC) $(CFLAGS) $(OSSP_ALSAP_CFLAGS) -o $@ $< $(OSSP_ALSAP_LDFLAGS) $(LDFLAGS)

osstest: osstest.c
	$(CC) $(CFLAGS) -o $@ $< $(XLDFLAGS)

test: osstest
	@./osstest

clean:
	rm -f *.o *.a osspd ossp-padsp ossp-alsap osstest
