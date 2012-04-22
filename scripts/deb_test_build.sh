#!/bin/bash

# These are the steps to use to test building the debian packages
# Run from the root FS directory

./scripts/ci/src_tarball.sh 1.1.beta2
./scripts/ci/debsrcbuilder.sh 1.1.beta2 70 testing
sudo pbuilder --build --buildresult ./debbuild/results/ ./debbuild/*.dsc 

lintian -i --fail-on-warnings --pedantic --suppress-tags source-contains-prebuilt-windows-binary -s ./debbuild/*.dsc
lintian -i --fail-on-warnings --pedantic --suppress-tags source-contains-prebuilt-windows-binary ./debbuild/results/*.deb

