#!/bin/bash

## export NetBSD source to ./export

rm -r export
rm -r export.unpatched

## cvs ... -dexport/src doesn't work
mkdir -p export
cd export

PSERV=:pserver:anoncvs@anoncvs.netbsd.org

## initial login (pw anoncvs)
##cvs -d :pserver:anoncvs@anoncvs.netbsd.org:/cvsroot login

for i in src/common/lib/libc/string/strlcat.c\
         src/common/lib/libc/string/strlcpy.c\
         src/lib/libc/gen/vis.c\
         src/lib/libc/gen/unvis.c\
         src/include/vis.h \
         src/tools/compat/fgetln.c
do
   echo $i
   cvs -d $PSERV:/cvsroot export -Dnow -dsrc $i
done

cvs -d $PSERV:/cvsroot export -Dnow -dsrc src/lib/libedit


## hierarchy canges

rm src/readline/Makefile
rm src/TEST/Makefile
rm src/Makefile
rm src/config.h

mv src/TEST examples

mkdir doc
mv src/editline.3 doc/editline.3.roff
mv src/editrc.5 doc/editrc.5.roff

mv src/readline src/editline
mv src/term.h src/el_term.h 

cd ..
date +"%Y%m%d" > timestamp.cvsexport

cp -rf export export.unpatched
