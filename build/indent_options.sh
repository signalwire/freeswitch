#!/bin/bash
echo -n "-brs -npsl -di0 -br -ce -d0 -cli0 -npcs -nfc1 -ut -i4 -ts4 -l120 -cs -T size_t " > ../.indent.pro
grep typedef ../src/include/*.h | grep switch_ | grep -v "\*\|{"  | sed -e s/struct// | perl -ne '@l = split; $l[2] =~ s/;//g ; print "-T $l[2] "' >> ../.indent.pro
grep "} switch_" ../src/include/*.h | perl -ne '@l = split; $l[1] =~ s/;//g ; print " -T $l[1] "' >> ../.indent.pro
