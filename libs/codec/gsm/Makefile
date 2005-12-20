# Copyright 1992-1996 by Jutta Degener and Carsten Bormann, Technische
# Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
# details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.

# Machine- or installation dependent flags you should configure to port

SASR	= -DSASR
######### Define SASR if >> is a signed arithmetic shift (-1 >> 1 == -1)

#MULHACK = -DUSE_FLOAT_MUL
######### Define this if your host multiplies floats faster than integers,
######### e.g. on a SPARCstation.

#FAST	= -DFAST
######### Define together with USE_FLOAT_MUL to enable the GSM library's
######### approximation option for incorrect, but good-enough results.

# LTP_CUT	= -DLTP_CUT
LTP_CUT	=
######### Define to enable the GSM library's long-term correlation 
######### approximation option---faster, but worse; works for
######### both integer and floating point multiplications.
######### This flag is still in the experimental stage.

WAV49	= -DWAV49
#WAV49	=
######### Define to enable the GSM library's option to pack GSM frames 
######### in the style used by the WAV #49 format.  If you want to write
######### a tool that produces .WAV files which contain GSM-encoded data,
######### define this, and read about the GSM_OPT_WAV49 option in the
######### manual page on gsm_option(3).

#The problem with sparc is the best stuff is in newer versions of gcc (post 3.0) only.
#This works for even old (2.96) versions of gcc and provides a small boost either way.
#A ultrasparc cpu is really v9 but the stock debian stable 3.0 gcc doesn't support it.
#So we go lowest common available by gcc and go a step down, still a step up from
#the default as we now have a better instruction set to work with. - Belgarath
ifeq (${PROC},ultrasparc)
OPTIMIZE+=-mcpu=v8 -mtune=$(PROC) -O3 
endif

PG =
#PG = -g -pg
######### Profiling flags.  If you don't know what that means, leave it blank.
 
# Choose a compiler.  The code works both with ANSI and K&R-C.
# Use -DNeedFunctionPrototypes to compile with, -UNeedFunctionPrototypes to
# compile without, function prototypes in the header files.
#
# You can use the -DSTUPID_COMPILER to circumvent some compilers'
# static limits regarding the number of subexpressions in a statement.

# CC		= cc
# CCFLAGS 	= -c -DSTUPID_COMPILER

# CC		= /usr/lang/acc
# CCFLAGS 	= -c -O

CC		?= gcc
CCFLAGS 	+= -c -DNeedFunctionPrototypes=1 -funroll-loops $(OPTIMIZE)
ifneq ($(findstring CYGWIN,${OSARCH}),CYGWIN)
CCFLAGS   += -fPIC
endif


LD 		= $(CC)

# LD		= gcc
# LDFLAGS 	=


# If your compiler needs additional flags/libraries, regardless of
# the source compiled, configure them here.

# CCINC	= -I/usr/gnu/lib/gcc-2.1/gcc-lib/sparc-sun-sunos4.1.2/2.1/include
######### Includes needed by $(CC)

# LDINC	= -L/usr/gnu/lib/gcc-2.1/gcc-lib/sparc-sun-sunos4.1.2/2.1
######### Library paths needed by $(LD)

# LDLIB	= -lgcc
######### Additional libraries needed by $(LD)


# Where do you want to install libraries, binaries, a header file
# and the manual pages?
#
# Leave INSTALL_ROOT empty (or just don't execute "make install") to
# not install gsm and toast outside of this directory.

INSTALL_ROOT	=

# Where do you want to install the gsm library, header file, and manpages?
#
# Leave GSM_INSTALL_ROOT empty to not install the GSM library outside of
# this directory.

GSM_INSTALL_ROOT = $(INSTALL_ROOT)
GSM_INSTALL_LIB = $(GSM_INSTALL_ROOT)/lib
GSM_INSTALL_INC = $(GSM_INSTALL_ROOT)/inc
GSM_INSTALL_MAN = $(GSM_INSTALL_ROOT)/man/man3

#  Other tools

SHELL		= /bin/sh
LN		= ln
BASENAME 	= basename
AR		= ar
ARFLAGS		= cr
RMFLAGS		= -f
FIND		= find
COMPRESS 	= compress
COMPRESSFLAGS 	= 
# RANLIB 	= true
RANLIB	 	= ranlib

#
#    You shouldn't have to configure below this line if you're porting.
# 


# Local Directories

ROOT	= .
MAN	= $(ROOT)/man
BIN	= $(ROOT)/bin
SRC	= $(ROOT)/src
LIB	= $(ROOT)/lib
TLS	= $(ROOT)/tls
INC	= $(ROOT)/inc

# Flags

DEBUG	= -DNDEBUG
######### Remove -DNDEBUG to enable assertions.

CFLAGS += $(PG) $(CCFLAGS) $(SASR) $(DEBUG) $(MULHACK) $(FAST) \
	$(LTP_CUT) $(WAV49) $(CCINC) -I$(INC)
######### It's $(CC) $(CFLAGS)

LFLAGS	= $(PG) $(LDFLAGS) $(LDINC)
######### It's $(LD) $(LFLAGS)


# Targets

LIBGSM	= $(LIB)/libgsm.a
LIBGSMSO= $(LIB)/libgsm.so

# Headers

GSM_HEADERS =	$(INC)/gsm.h

HEADERS	=	$(INC)/proto.h		\
		$(INC)/unproto.h		\
		$(INC)/config.h		\
		$(INC)/private.h		\
		$(INC)/gsm.h			\
		$(INC)/toast.h			\
		$(TLS)/taste.h

# Sources

SOURCES =	$(SRC)/add.c			\
		$(SRC)/gsm_create.c		\
		$(SRC)/gsm_destroy.c	\
		$(SRC)/gsm_decode.c		\
		$(SRC)/gsm_encode.c		\
		$(SRC)/gsm_option.c		\
		$(SRC)/code.c			\
		$(SRC)/decode.c		\
		$(SRC)/gsm_lpc.c		\
		$(SRC)/rpe.c			\
		$(SRC)/preprocess.c		\
		$(SRC)/long_term.c		\
		$(SRC)/short_term.c		\
		$(SRC)/table.c

# Object files

OBJECTS =	$(SRC)/add.o			\
		$(SRC)/gsm_create.o		\
		$(SRC)/gsm_destroy.o	\
		$(SRC)/gsm_decode.o		\
		$(SRC)/gsm_encode.o		\
		$(SRC)/gsm_option.o		\
		$(SRC)/code.o			\
		$(SRC)/decode.o		\
		$(SRC)/gsm_lpc.o		\
		$(SRC)/rpe.o			\
		$(SRC)/preprocess.o		\
		$(SRC)/long_term.o		\
		$(SRC)/short_term.o		\
		$(SRC)/table.o

# Manuals

MANUALS =	$(MAN)/gsm.3			\
		$(MAN)/gsm_explode.3	\
		$(MAN)/gsm_option.3		\
		$(MAN)/gsm_print.3

# Other stuff in the distribution

STUFF = 	ChangeLog			\
		INSTALL			\
		MACHINES			\
		MANIFEST			\
		Makefile			\
		README


# Install targets

GSM_INSTALL_TARGETS =					\
		$(GSM_INSTALL_LIB)/libgsm.a		\
		$(GSM_INSTALL_INC)/gsm.h			\
		$(GSM_INSTALL_MAN)/gsm.3			\
		$(GSM_INSTALL_MAN)/gsm_explode.3	\
		$(GSM_INSTALL_MAN)/gsm_option.3	\
		$(GSM_INSTALL_MAN)/gsm_print.3


# Default rules

.c.o:
		$(CC) $(CFLAGS) $?
		@-mv `$(BASENAME) $@` $@ > /dev/null 2>&1

# Target rules

all:		$(LIBGSM) $(LIBGSMSO)
		@-echo $(ROOT): Done.


install:	gsminstall
		@-echo install: Done.


# The basic API: libgsm

$(LIBGSMSO):	$(LIB) $(OBJECTS)
		$(LD) -o $@.1.0.10 -shared -Xlinker -soname -Xlinker libgsm.so.1 $(OBJECTS) -lc
		ln -fs libgsm.so.1.0.10 lib/libgsm.so.1
		ln -fs libgsm.so.1.0.10 lib/libgsm.so

$(LIBGSM):	$(LIB) $(OBJECTS)
		-rm $(RMFLAGS) $(LIBGSM)
		$(AR) $(ARFLAGS) $(LIBGSM) $(OBJECTS)
		$(RANLIB) $(LIBGSM)


# The local bin and lib directories

$(BIN):
		if [ ! -d $(BIN) ] ; then mkdir $(BIN) ; fi

$(LIB):
		if [ ! -d $(LIB) ] ; then mkdir $(LIB) ; fi


# Installation

gsminstall:
		-if [ x"$(GSM_INSTALL_ROOT)" != x ] ; then	\
			$(MAKE) $(GSM_INSTALL_TARGETS) ;	\
		fi

gsmuninstall:
		-if [ x"$(GSM_INSTALL_ROOT)" != x ] ; then	\
			rm $(RMFLAGS) $(GSM_INSTALL_TARGETS) ;	\
		fi

$(GSM_INSTALL_MAN)/gsm.3:	$(MAN)/gsm.3
		-rm $@
		cp $? $@
		chmod 444 $@

$(GSM_INSTALL_MAN)/gsm_option.3:	$(MAN)/gsm_option.3
		-rm $@
		cp $? $@
		chmod 444 $@

$(GSM_INSTALL_MAN)/gsm_explode.3:	$(MAN)/gsm_explode.3
		-rm $@
		cp $? $@
		chmod 444 $@

$(GSM_INSTALL_MAN)/gsm_print.3:	$(MAN)/gsm_print.3
		-rm $@
		cp $? $@
		chmod 444 $@

$(GSM_INSTALL_INC)/gsm.h:	$(INC)/gsm.h
		-rm $@
		cp $? $@
		chmod 444 $@

$(GSM_INSTALL_LIB)/libgsm.a:	$(LIBGSM)
		-rm $@
		cp $? $@
		chmod 444 $@


# Distribution

dist:		gsm-1.0.tar.Z
		@echo dist: Done.

gsm-1.0.tar.Z:	$(STUFF) $(SOURCES) $(HEADERS) $(MANUALS)
		(	cd $(ROOT)/..;				\
			tar cvf - `cat $(ROOT)/gsm-1.0/MANIFEST	\
				| sed '/^#/d'`			\
		) | $(COMPRESS) $(COMPRESSFLAGS) > $(ROOT)/gsm-1.0.tar.Z

# Clean

uninstall:	gsmuninstall
		@-echo uninstall: Done.

semi-clean:
		-rm $(RMFLAGS)  */*.o			\
			$(TST)/lin2cod $(TST)/lin2txt	\
			$(TST)/cod2lin $(TST)/cod2txt	\
			$(TST)/gsm2cod 			\
			$(TST)/*.*.*
		-$(FIND) . \( -name core -o -name foo \) \
			-print | xargs rm $(RMFLAGS)

clean:	semi-clean
		-rm $(RMFLAGS) $(LIBGSM) \
			$(ROOT)/gsm-1.0.tar.Z
		rm -rf lib




