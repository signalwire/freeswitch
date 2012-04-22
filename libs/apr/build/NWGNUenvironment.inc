#
# Setup needed Tools and Libraries
#

ifeq "$(wildcard $(APR_WORK)\build\NWGNUcustom.inc)" "$(APR_WORK)\build\NWGNUcustom.inc"
include $(APR_WORK)\build\NWGNUcustom.inc
CUSTOM_INI = $(AP_WORK)\NWGNUcustom.ini
endif

ifndef VERBOSE
.SILENT:
endif

#
# Treat like an include
#
ifndef EnvironmentDefined

#
# simple macros for parsing makefiles
#
EOLIST:=
EMPTY :=
COMMA := ,
SPACE := $(EMPTY) $(EMPTY)

#
# Base environment
#

# Try and handle case issues
ifndef NOVELLLIBC
ifdef NovellLibC
NOVELLLIBC = $(NovellLibC)
endif
endif

ifndef NOVELLLIBC
NOVELLLIBC = C:/novell/ndk/libc
endif

# This is a placeholder
# ifndef LDAPSDK
# LDAPSDK = C:/novell/ndk/cldapsdk
# endif

ifndef METROWERKS
METROWERKS = C:\Program Files\Metrowerks\CodeWarrior
endif

# If LM_LICENSE_FILE isn't defined, define a variable that can be used to
# restart make with it defined
ifndef LM_LICENSE_FILE
NO_LICENSE_FILE = NO_LICENSE_FILE
endif

#
# Set the Release type that you want to build, possible values are:
#
#  debug		- full debug switches are set
#  noopt		- normal switches are set (default)
#  optimized	- optimization switches are set

ifdef reltype
RELEASE=$(reltype)
endif

ifdef RELTYPE
RELEASE=$(RELTYPE)
endif

ifdef debug
RELEASE=debug
endif

ifdef DEBUG
RELEASE=debug
endif

ifdef optimized
RELEASE=optimized
endif

ifdef OPTIMIZED
RELEASE=optimized
endif

ifndef RELEASE
RELEASE = optimized
endif

ifeq "$(RELEASE)" "debug"
OBJDIR = Debug
endif

ifeq "$(RELEASE)" "noopt"
OBJDIR = Noopt
endif

ifeq "$(RELEASE)" "optimized"
OBJDIR = Release
endif

#
# Setup compiler information
#

# MetroWerks NLM tools
CC		= mwccnlm
CPP		= mwccnlm
LINK	= mwldnlm
LIB		= mwldnlm -type library -w nocmdline

ifdef IPV6
ifndef USE_STDSOCKETS
USE_STDSOCKETS=1
endif
endif

NOVI	= $(NOVELLLIBC)\imports

INCDIRS 	= $(NOVELLLIBC)\include;$(NOVELLLIBC)\include\nks;$(NOVELLLIBC)\include\winsock;

DEFINES		= -DNETWARE
ifndef USE_STDSOCKETS
DEFINES += -DUSE_WINSOCK
endif
ifndef DEBUG
DEFINES += -DNDEBUG
endif

#
# MetroWerks static Libraries

CLIB3S	= $(METROWERKS)\Novell Support\Metrowerks Support\Libraries\Runtime\mwcrtl.lib
MATH3S	=
PLIB3S	= $(METROWERKS)\Novell Support\Metrowerks Support\Libraries\MSL C++\MWCPP.lib

# Base compile flags
# and prefix or precompiled header added here.

# The default flags are as follows:
#
# -c                    compile only, no link
# -nosyspath            treat #include <...> like #include "..."
# -Cpp_exceptions off   disable C++ exceptions
# -RTTI off             disable C++ run-time typing information
# -align 4              align on 4 byte bounderies
# -w nocmdline          disable command-line driver/parser warnings
# -proc PII             generate code base on Pentium II instruction set
# -inst mmx             use MMX extensions (not used)

CFLAGS = -c -nosyspath -Cpp_exceptions off -RTTI off -align 4 -w nocmdline -proc PII

# -g                    generate debugging information
# -O0                   level 0 optimizations

ifeq "$(RELEASE)" "debug"
CFLAGS += -g -O0
endif

# -O4,p                 level 4 optimizations, optimize for speed
ifeq "$(RELEASE)" "optimized"
CFLAGS += -O4,p
endif

# -prefix apr_arch_pre_nw.h      #include pre_nw.h for all files

CFLAGS += -prefix apr_arch_pre_nw.h


PATH:=$(PATH);$(METROWERKS)\bin;$(METROWERKS)\Other Metrowerks Tools\Command Line Tools

#
# Declare major project deliverables output directories here
#

ifdef DEST
INSTALL = $(DEST)
ifeq (\, $(findstring \,$(INSTALL)))
INSTDIRS = $(DEST)
endif
endif

ifdef dest
INSTALL = $(dest)
ifeq (\, $(findstring \,$(INSTALL)))
INSTDIRS = $(dest)
endif
endif

ifndef INSTALL
INSTALL = $(APR_WORK)\Dist
INSTDIRS = $(APR_WORK)\Dist
endif

# Add support for building IPV6 alongside
ifneq "$(IPV6)" ""
DEFINES += -DNW_BUILD_IPV6
INCDIRS := $(NOVELLLIBC)\include\winsock\IPV6;$(INCDIRS)

ifneq "$(findstring IPV6,$(OBJDIR))" "IPV6"
OBJDIR := $(OBJDIR)_IPV6
endif
        
ifneq "$(findstring IPV6,$(INSTALL))" "IPV6"
INSTALL := $(INSTALL)_IPV6
endif        

ifneq "$(findstring IPV6,$(INSTDIRS))" "IPV6"
INSTDIRS := $(INSTDIRS)_IPV6
endif

endif

ifdef DEST
INSTALLBASE := $(INSTALL)\Apache2

INSTDEVDIRS := \
    $(INSTDIRS) \
	$(INSTALLBASE) \
	$(INSTALLBASE)\include \
	$(INSTALLBASE)\lib \

INSTDIRS += \
	$(INSTALLBASE) \
	
else
INSTALLBASE := $(INSTALL)\apr

INSTDEVDIRS := \
    $(INSTDIRS) \
	$(INSTALLBASE) \
	$(INSTALLBASE)\include \
	$(INSTALLBASE)\lib \

INSTDIRS += \
	$(INSTALLBASE) \
	
endif	

#
# Declare Command and tool macros here
#

# Os2LibPath is an extra check to see if we are on NT
ifdef Os2LibPath
OS = Windows_NT
endif

ifeq "$(OS)" "Windows_NT"
CMD=cmd /C
CHK=cmd /C if exist
CHKNOT=cmd /C if not exist
DEL = del /F
DELTREE = cmd /C rd /s/q
WINNT=1
XCOPYSW = /E
else
CMD=command /C
CHK=command /C if exist
CHKNOT=command /C if not exist
DEL = del
DELTREE = deltree /y
XCOPYSW = /E /Y
endif


#
# Setup base C compiler flags
#

#
# Common directories
#

APR		= $(APR_WORK)
APRTEST		= $(APR_WORK)/test
APRUTIL		= $(APU_WORK)
APULDAP		= $(APU_WORK)/ldap
XML		= $(APRUTIL)/xml

#
# Internal Libraries
#

APRLIB		= $(APR)/$(OBJDIR)/aprlib.lib
APRUTLIB	= $(APRUTIL)/$(OBJDIR)/aprutil.lib
APULDAPLIB	= $(APULDAP)/$(OBJDIR)/apuldap.lib
XMLLIB		= $(XML)/$(OBJDIR)/xmllib.lib

#
# Additional general defines
#

EnvironmentDefined = 1
endif # ifndef EnvironmentDefined

# This is always set so that it will show up in lower directories

ifdef Path
Path = $(PATH)
endif

