#!/bin/sh
echo -n "-brs -npsl -di0 -br -ce -d0 -cli0 -npcs -nfc1 -ut -i4 -ts4 -l155 -cs -T size_t " > ./.indent.pro

for i in `find src/ -name \*.c` ; do cat $i | perl -ne 'print "-T $1 " if (/([0-9A-Za-z_-]+_t)/)' ; done | sort | uniq >> ./.indent.pro
for i in `find src/ -name \*.h` ; do cat $i | perl -ne 'print "-T $1 " if (/([0-9A-Za-z_-]+_t)/)' ; done | sort | uniq >> ./.indent.pro


