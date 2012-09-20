SRCDIR=../../src
CPPFLAGS = -I$(SRCDIR)

PROGS = other1
all: $(PROGS) run_tests

tpl.c: 
	cp $(SRCDIR)/tpl.c .

other1: other1.o tpl.o
	g++ -o other1 other1.o tpl.o


.PHONY: clean run_tests

clean:
	rm -f *.o $(PROGS) *.out
	rm -f tpl.c

run_tests:
	perl ./do_tests
