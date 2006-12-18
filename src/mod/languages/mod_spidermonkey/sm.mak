# This is all used to make sure we use the right options during build and link.  

OS_ARCH         := $(subst /,_,$(shell uname -s | sed /\ /s//_/))
#VER=DBG
#BO=0
VER=OPT
BO=1

# Attempt to differentiate between SunOS 5.4 and x86 5.4
OS_CPUARCH      := $(shell uname -m)
ifeq ($(OS_CPUARCH),i86pc)
OS_RELEASE      := $(shell uname -r)_$(OS_CPUARCH)
else
ifeq ($(OS_ARCH),AIX)
OS_RELEASE      := $(shell uname -v).$(shell uname -r)
else
OS_RELEASE      := $(shell uname -r)
endif
endif
ifeq ($(OS_ARCH),IRIX64)
OS_ARCH         := IRIX
endif

# Handle output from win32 unames other than Netscape's version
ifeq (,$(filter-out Windows_95 Windows_98 CYGWIN_95-4.0 CYGWIN_98-4.10, $(OS_ARCH)))
	OS_ARCH   := WIN95
endif
ifeq ($(OS_ARCH),WIN95)
	OS_ARCH	   := WINNT
	OS_RELEASE := 4.0
endif
ifeq ($(OS_ARCH), Windows_NT)
	OS_ARCH    := WINNT
	OS_MINOR_RELEASE := $(shell uname -v)
	ifeq ($(OS_MINOR_RELEASE),00)
		OS_MINOR_RELEASE = 0
	endif
	OS_RELEASE := $(OS_RELEASE).$(OS_MINOR_RELEASE)
endif
ifeq (CYGWIN_NT,$(findstring CYGWIN_NT,$(OS_ARCH)))
	OS_RELEASE := $(patsubst CYGWIN_NT-%,%,$(OS_ARCH))
	OS_ARCH    := WINNT
endif
ifeq ($(OS_ARCH), CYGWIN32_NT)
	OS_ARCH    := WINNT
endif
ifeq (MINGW32_NT,$(findstring MINGW32_NT,$(OS_ARCH)))
	OS_RELEASE := $(patsubst MINGW32_NT-%,%,$(OS_ARCH))
	OS_ARCH    := WINNT
endif

ifeq ($(OS_ARCH),Linux)
OS_CONFIG      := Linux_All
else
ifeq ($(OS_ARCH),dgux)
OS_CONFIG      := dgux
else
ifeq ($(OS_ARCH),Darwin)
OS_CONFIG      := Darwin
else
OS_CONFIG       := $(OS_ARCH)$(OS_OBJTYPE)$(OS_RELEASE)
endif
endif
endif

CFLAGS += -I../mod_spidermonkey -I$(BASE)/libs/mozilla/js/src -Wall -Wno-format -g -DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DHAVE_LOCALTIME_R -DX86_LINUX -DDEBUG_root -DJS_THREADSAFE -I$(BASE)/libs/mozilla/js/src -I$(BASE)/libs/mozilla/js/src/$(OS_CONFIG)_$(VER).OBJ -Wall -Wno-format -g -DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DHAVE_LOCALTIME_R -DX86_LINUX -DDEBUG_root -DJS_THREADSAFE -I$(BASE)/libs/mozilla/nsprpub/dist/include/nspr -I$(OS_CONFIG)_$(VER).OBJ
LDFLAGS +=-DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DHAVE_LOCALTIME_R -DX86_LINUX  -DDEBUG_root -DJS_THREADSAFE -I$(BASE)/libs/mozilla/nsprpub/dist/include/nspr -Wall -Wno-format -g -DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DHAVE_LOCALTIME_R -DX86_LINUX -DDEBUG_root -DJS_THREADSAFE -I$(BASE)/libs/mozilla/nsprpub/dist/include/nspr
