# Copyright (c) 2007, Anthony Minessale II
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# 
# * Neither the name of the original author; nor the names of any contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.
# 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


OBJS=hashtable.o \
hashtable_itr.o \
zap_io.o \
zap_isdn.o \
zap_analog.o \
zap_config.o \
g711.o \
libteletone_detect.o \
libteletone_generate.o \
zap_buffer.o \
zap_threadmutex.o \
isdn/EuroISDNStateNT.o \
isdn/EuroISDNStateTE.o \
isdn/mfifo.o \
isdn/Q921.o \
isdn/Q931api.o \
isdn/Q931.o \
isdn/Q931ie.o \
isdn/Q931mes.o \
isdn/Q931StateNT.o \
isdn/Q931StateTE.o \
isdn/nationalie.o \
isdn/nationalmes.o \
isdn/nationalStateNT.o \
isdn/nationalStateTE.o \
isdn/Q932mes.o 

HEADERS=isdn/include/Q931.h \
	include/openzap.h 

PWD=$(shell pwd)
INCS=-I$(PWD)/include -I$(PWD)/isdn/include
CFLAGS=$(ZAP_CFLAGS) $(INCS)
MYLIB=libopenzap.a
TMP=-I../../libpri-1.2.4 -Iinclude -I. -w

include general.makefile $(ZAP_MODS)

all: $(MYLIB)

$(MYLIB): $(OBJS) $(HEADERS)
	ar rcs $(MYLIB) $(OBJS)
	ranlib $(MYLIB)

testapp: testapp.c $(MYLIB)
	$(CC) $(INCS) -L. testapp.c -o testapp -lopenzap -lm -lpthread

testisdn: testisdn.c $(MYLIB)
	$(CC) $(INCS) -L. testisdn.c -o testisdn -lopenzap -lm -lpthread

testanalog: testanalog.c $(MYLIB)
	$(CC) $(INCS) -L. testanalog.c -o testanalog -lopenzap -lm -lpthread

priserver.o: priserver.c
	$(CC) $(INCS) $(TMP) -c priserver.c -o priserver.o 

sangoma_pri.o: sangoma_pri.c
	$(CC) $(INCS) $(TMP) -c sangoma_pri.c -o sangoma_pri.o

priserver: $(MYLIB) priserver.o sangoma_pri.o
	$(CC) sangoma_pri.o priserver.o -L. -o priserver -lopenzap -lm -lpthread ../../libpri-1.2.4/libpri.a

zap_io.o: zap_io.c
	$(CC) $(MOD_CFLAGS) $(CC_CFLAGS) $(CFLAGS) -c $< -o $@

zap_wanpipe.o: zap_wanpipe.c
	$(CC) $(CFLAGS) $(ZAP_CFLAGS) $(WP_CFLAGS) -c $< -o $@

zap_zt.o: zap_zt.c
	$(CC) $(CFLAGS) $(ZAP_CFLAGS) $(ZT_CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) -c $< -o $@

dox:
	cd docs && doxygen $(PWD)/docs/Doxygen.conf
clean:
	rm -f *.o isdn/*.o $(MYLIB) *~ \#* testapp priserver testisdn testanalog

