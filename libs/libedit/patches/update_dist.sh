#!/bin/bash -e


## copy ./export/* files to dist
""cp -rf export/* ..

## get clean ./export hierarchy
""rm -rf export
""mv export.unpatched export

## make new patches to be distributed
./patches_make.sh

## update EXTRA_DIST list in Makefile.am
./extra_dist_list.sh

## increment LT_VERSION in ../configure.ac
./update_version.sh
