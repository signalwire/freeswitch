
# comma separated list (e.g. "iconv.dll,libiconv.dll")
DEFAULT_LIBICONV_DLL ?= \"\"

CFLAGS += -pedantic -Wall
CFLAGS += -DUSE_LIBICONV_DLL
CFLAGS += -DDEFAULT_LIBICONV_DLL=$(DEFAULT_LIBICONV_DLL)

all: iconv.dll libiconv.a win_iconv.exe

dist: test win_iconv.zip

iconv.dll: win_iconv.c
	gcc $(CFLAGS) -c win_iconv.c -DMAKE_DLL
	dllwrap --dllname iconv.dll --def iconv.def win_iconv.o $(SPECS_FLAGS)
	strip iconv.dll

libiconv.a: win_iconv.c
	gcc $(CFLAGS) -c win_iconv.c
	ar rcs libiconv.a win_iconv.o
	ranlib libiconv.a

win_iconv.exe: win_iconv.c
	gcc $(CFLAGS) -s -o win_iconv.exe win_iconv.c -DMAKE_EXE

libmlang.a: mlang.def
	dlltool --kill-at --input-def mlang.def --output-lib libmlang.a

test:
	gcc $(CFLAGS) -s -o win_iconv_test.exe win_iconv_test.c
	./win_iconv_test.exe

win_iconv.zip: msvcrt msvcr70 msvcr71
	rm -rf win_iconv
	svn export . win_iconv
	cp msvcrt/iconv.dll msvcrt/win_iconv.exe win_iconv/
	mkdir win_iconv/msvcr70
	cp msvcr70/iconv.dll win_iconv/msvcr70/
	mkdir win_iconv/msvcr71
	cp msvcr71/iconv.dll win_iconv/msvcr71/
	zip -r win_iconv.zip win_iconv

msvcrt:
	svn export . msvcrt; \
	cd msvcrt; \
	$(MAKE);

msvcr70:
	svn export . msvcr70; \
	cd msvcr70; \
	gcc -dumpspecs | sed s/-lmsvcrt/-lmsvcr70/ > specs; \
	$(MAKE) "SPECS_FLAGS=-specs=$$PWD/specs";

msvcr71:
	svn export . msvcr71; \
	cd msvcr71; \
	gcc -dumpspecs | sed s/-lmsvcrt/-lmsvcr71/ > specs; \
	$(MAKE) "SPECS_FLAGS=-specs=$$PWD/specs";

clean:
	rm -f win_iconv.exe
	rm -f win_iconv.o
	rm -f iconv.dll
	rm -f libiconv.a
	rm -f win_iconv_test.exe
	rm -f libmlang.a
	rm -rf win_iconv
	rm -rf win_iconv.zip
	rm -rf msvcrt
	rm -rf msvcr70
	rm -rf msvcr71

