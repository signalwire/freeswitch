BASE=../../../..
MOD_CFLAGS= -march=core2 -g -O2 -malign-double -mtune=core2 -mmmx -msse -msse2 -msse3 -mssse3 -mfpmath=sse -ffast-math -funroll-loops -fprefetch-loop-arrays -funit-at-a-time -ftracer --save-temps
MOD_LDFLAGS= --save-temps
LOCAL_SOURCES=amplitude.c buffer.c desa2.c goertzel.c fast_acosf.c
LOCAL_OBJS=amplitude.o buffer.o desa2.o goertzel.o fast_acosf.o
include $(BASE)/build/modmake.rules

#compute_table: compute_table.o
#	gcc -o compute_table compute_table.o -lm
#
#compute_table.o: compute_table.c
#	gcc -c compute_table.c
#

