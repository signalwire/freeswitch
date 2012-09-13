TPLSRC = ../../src
PROGS = test1

# Thread support requires compiler-specific options
# ----------------------------------------------------------------------------
# GNU 
CFLAGS = -I$(TPLSRC) -g -pthread
# Solaris (Studio 11 on Sparc Ultra3) 
#CFLAGS = -I$(TPLSRC) -g -mt
# ----------------------------------------------------------------------------

all: $(PROGS) run_tests

tpl.o :	$(TPLSRC)/tpl.c $(TPLSRC)/tpl.h
	$(CC) -c $(CFLAGS) $(TPLSRC)/tpl.c

$(PROGS) : tpl.o 
	$(CC) $(CFLAGS) -o $@ $(@).c tpl.o

run_tests: $(PROGS)
	perl ../do_tests

.PHONY: clean

clean:	
	rm -f $(PROGS) tpl.o test*.out
