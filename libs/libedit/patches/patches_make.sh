#!/bin/bash

## make new patches after e.g. distributed ../src has been changed

rm *.patch

i=0
for file in $(find export -type f)
do
   diff -q $file ../${file#export/} > /dev/null
   res="$?"
   if test 1 -eq "$res"
   then
      diff -au $file ../${file#export/} > $(printf '%02d' $i)-${file##*/}.patch
      ((i++))
   fi
done
