ARCH=$(PROC)
CFLAGS+=-Wall -O3 -funroll-loops
ifneq (${OSARCH},CYGWIN)
CFLAGS += -fPIC
endif
LIB=libilbc.a

OBJS= anaFilter.o iCBSearch.o packing.o \
        constants.o gainquant.o iLBC_decode.o StateConstructW.o \
        createCB.o getCBvec.o iLBC_encode.o StateSearchW.o doCPLC.o \
        helpfun.o syntFilter.o enhancer.o hpInput.o LPCdecode.o \
        filter.o hpOutput.o LPCencode.o FrameClassify.o  iCBConstruct.o lsf.o

all: $(LIB)


$(LIB): $(OBJS)
	ar cr $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	rm -f $(LIB) *.o

install:

