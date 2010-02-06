#!/bin/sh
echo -n "-brs -npsl -di0 -br -ce -d0 -cli0 -npcs -nfc1 -ut -i4 -ts4 -l155 -cs -T size_t " > ./.indent.pro
grep "typedef struct.*_t;" `find ./src/include/ -name \*.h` | grep apr_ | grep -v "\*\|{"  | sed -e s/struct// | perl -ne '@l = split; $l[2] =~ s/;//g ; print "-T $l[2] "' >> ./.indent.pro
grep "typedef struct.*_t;" `find ./src/include/ -name \*.h` | grep switch_ | grep -v "\*\|{"  | sed -e s/struct// | perl -ne '@l = split; $l[2] =~ s/;//g ; print "-T $l[2] "' >> ./.indent.pro
grep "} switch_" ./src/include/*.h | perl -ne '@l = split; $l[1] =~ s/;//g ; print " -T $l[1] "' >> ./.indent.pro
