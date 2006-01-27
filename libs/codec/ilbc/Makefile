ARCH=$(PROC)
CFLAGS+=-Wall -O3 -funroll-loops
ifneq (${OSARCH},CYGWIN)
CFLAGS += -fPIC
endif
LIB=libilbc.a

OBJS= src/anaFilter.o src/iCBSearch.o src/packing.o \
        src/constants.o src/gainquant.o src/iLBC_decode.o src/StateConstructW.o \
        src/createCB.o src/getCBvec.o src/iLBC_encode.o src/StateSearchW.o src/doCPLC.o \
        src/helpfun.o src/syntFilter.o src/enhancer.o src/hpInput.o src/LPCdecode.o \
        src/filter.o src/hpOutput.o src/LPCencode.o src/FrameClassify.o  src/iCBConstruct.o src/lsf.o

all: $(LIB)


$(LIB): $(OBJS)
	ar cr $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	rm -f $(LIB) *.o

install:

