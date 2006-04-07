#!/bin/sh
#
# update.sh
#
# update copyright dates in files

a=`find . -name "*.[ch]"`
for x in $a; do 
    sed 's/(c) 2001-2004/(c) 2001-2005/' $x > $x.tmp; 
    mv $x.tmp $x; 
done



 
