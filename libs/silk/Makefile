#					
# Makefile for Silk SDK			
#
# Copyright (c) 2010, Skype Limited
# All rights reserved.
#

#Platform detection and settings

BUILD_OS := $(shell uname | sed -e 's/^.*Darwin.*/MacOS-X/ ; s/^.*CYGWIN.*/Windows/')

BUILD_ARCHITECTURE := $(shell uname -m | sed -e 's/i686/i386/')

EXESUFFIX = 
LIBPREFIX = lib
LIBSUFFIX = .a
OBJSUFFIX = .o

CC     = $(TOOLCHAIN_PREFIX)gcc$(TOOLCHAIN_SUFFIX)
AR     = $(TOOLCHAIN_PREFIX)ar
RANLIB = $(TOOLCHAIN_PREFIX)ranlib
CP     = $(TOOLCHAIN_PREFIX)cp

cflags-from-defines    = $(addprefix -D,$(1))
cflags-from-includes   = $(addprefix -I,$(1))
ldflags-from-ldlibdirs = $(addprefix -L,$(1))
ldlibs-from-libs       = $(addprefix -l,$(1))

CFLAGS	+= -Wall -enable-threads -O3 -fPIC

CFLAGS  += $(call cflags-from-defines,$(CDEFINES))
CFLAGS  += $(call cflags-from-includes,$(CINCLUDES))
LDFLAGS += $(call ldflags-from-ldlibdirs,$(LDLIBDIRS))
LDLIBS  += $(call ldlibs-from-libs,$(LIBS))

COMPILE.c.cmdline   = $(CC) -c $(CFLAGS) -o $@ $<
LINK.o.cmdline      = $(LINK.o) -lm $^ $(LDLIBS) -o $@$(EXESUFFIX) 
ARCHIVE.cmdline     = $(AR) $(ARFLAGS) $@ $^ && $(RANLIB) $@

%$(OBJSUFFIX):%.c
	$(COMPILE.c.cmdline)

# Directives

CINCLUDES += interface src test

# VPATH e.g. VPATH = src:../headers
VPATH = ./ \
        interface \
        src \
        test 

# Variable definitions
LIB_NAME = SKP_SILK_SDK
TARGET = $(LIBPREFIX)$(LIB_NAME)$(LIBSUFFIX)

SRCS_C = $(wildcard src/*.c)

OBJS := $(patsubst %.c,%$(OBJSUFFIX),$(SRCS_C))

ENCODER_SRCS_C = test/Encoder.c
ENCODER_OBJS := $(patsubst %.c,%$(OBJSUFFIX),$(ENCODER_SRCS_C))

DECODER_SRCS_C = test/Decoder.c
DECODER_OBJS := $(patsubst %.c,%$(OBJSUFFIX),$(DECODER_SRCS_C))

SIGNALCMP_SRCS_C = test/signalCompare.c
SIGNALCMP_OBJS := $(patsubst %.c,%$(OBJSUFFIX),$(SIGNALCMP_SRCS_C))

LIBS = \
	$(LIB_NAME)

LDLIBDIRS = ./

# Rules
default: all

all: $(TARGET) encoder decoder signalcompare

lib: $(TARGET)

$(TARGET): $(OBJS)
	$(ARCHIVE.cmdline)

encoder$(EXESUFFIX): $(ENCODER_OBJS)	
	$(LINK.o.cmdline)

decoder$(EXESUFFIX): $(DECODER_OBJS)	
	$(LINK.o.cmdline)

signalcompare$(EXESUFFIX): $(SIGNALCMP_OBJS)	
	$(LINK.o.cmdline)

clean:
	$(RM) $(TARGET)* $(OBJS) $(ENCODER_OBJS) $(DECODER_OBJS) \
		  $(SIGNALCMP_OBJS) $(TEST_OBJS) \
		  encoder$(EXESUFFIX) decoder$(EXESUFFIX) signalcompare$(EXESUFFIX)

