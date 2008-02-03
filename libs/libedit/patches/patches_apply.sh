#!/bin/bash

## apply patches to the ./export hierarchy

for patch in *.patch
do
   patch -d export -p1 < $patch
done
