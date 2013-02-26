#!/bin/bash

# verbose, skittish
set -ex

# run valgrind?
VG="valgrind --error-exitcode=149 --leak-check=full --show-reachable=yes"

# make sure we're up-to-date
make


# runs, not certaion wht it does yet
${VG} ./tcodec2
${VG} ./tinterp
${VG} ./tquant

# these fail, missing arguments
${VG} ./extract
${VG} ./genlsp
${VG} ./genres
${VG} ./scalarlsptest
${VG} ./tnlp
${VG} ./vq_train_jvm
${VG} ./vqtrain
${VG} ./vqtrainjnd

