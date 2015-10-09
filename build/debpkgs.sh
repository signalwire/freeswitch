#!/bin/bash

a='amd64 i386'
c='wheezy jessie stretch sid'
n='1'
T='/tmp/fs.sources.list'
K='/tmp/fs.asc'

while getopts "a:c:n:" flag
do
    case $flag in
	a) a=$OPTARG;;
	c) c=$OPTARG;;
	n) n=$OPTARG;;
	T) T=$OPTARG;;
	K) K=$OPTARG;;
    esac
done

if [ "$EUID" -ne 0 ]
then
    echo "Build script must be run as root or under sudo"
    exit 1
fi

echo "./build/debpkgs.sh script is building FreeSWITCH Debian packages"

VERSION=`cat ./build/next-release.txt`
echo "This Version: $VERSION"

HASH=`git log -n 1 --oneline |cut -d ' ' -f 1`
echo "Commit hash $HASH"

# Use the FreeSWITCH release repo for dependency testing
# The release codename here does not matter, since the util.sh script
# will adapt to the release being built
if [ ! -r "$T" ]
then
    echo "deb http://files.freeswitch.org/repo/deb/debian/ jessie main" >> "$T"
fi

# Use the FreeSWITCH release repo key
if [ ! -r "$K" ]
then
    cat << EOF > "$K"
-----BEGIN PGP PUBLIC KEY BLOCK-----
Version: GnuPG v1.4.12 (GNU/Linux)

mQGiBE8jEfIRBAC+Cca0fPQxhyhn0NMsPaMQJgTvqhWb5/f4Mel++kosmUQQ4fJq
4U9NFvpfNyLp5MoHpnlDfAb+e57B2sr47NOJLTh83yQIAnvU+8O0Q4kvMaiiesX5
CisApLBs6Vx28y7VWmLsY3vWu8mC7M+PORKfpBV8DWy/7569wQPx2SCsIwCgzv2T
8YsnYsSVRrrmh46J1o4/ngsD/13ETX4ws/wNN+82RdqUxu7fjc0fNbUAb6XYddAb
1hrw5npQulgUNWkpnVmIDRHDXLNMeT8nZDkxsA8AsT+u7ACfPFa2o3R8w9zOPSO+
oSO0+Puhop2+z1gm6lmfMKq9HpeXG3yt/8zsEVUmOYT9m+vYEVghfpXtACVYheDq
LzUuA/9E9HBiNPVhJ/mEpOk9bZ1gpwr3mjlpUbvX5aGwTJJ+YoTfZOCL7go3uQHn
/sT35WoJ23wJCRlW0SYTFJqCoris9AhI+qw7xRTw9wb+txSI96uhafUUMCn6GLkN
+yAixqDwNHKkdax3GSGJtLB0t67QoBDIpcGog7ZfRMvWP3QLNLQ4RnJlZVNXSVRD
SCBQYWNrYWdlIFNpZ25pbmcgS2V5IDxwYWNrYWdlc0BmcmVlc3dpdGNoLm9yZz6I
YgQTEQIAIgUCTyMR8gIbAwYLCQgHAwIGFQgCCQoLBBYCAwECHgECF4AACgkQ127c
dyXgEM879ACffY0HFi+mACtfFYmX/Uk/qGELSP4An1B8D5L4dLFFr1zV9YawQUbz
O9/MuQENBE8jEfIQBAC7vnn855YDuz1gTsUMYDxfIRH5KPmDDEAf1WXoD3QG4qOQ
xVW5nhp/bolh2CacAxdOjZePdhGkkdNOBpcu9NlTNRru0myGN8etbnzP3O5dq0io
VMf23C5u9KPbxwRWS+WFtC4CRFn6DafDI1qa3Gv3CkiBWtKR0Wid2SQLzl3mVwAF
EQP9HlwGjhBfFA26LlSMPhSo0Ll+sdcOJupJ21zmGeg7c0GpBnzDzyyJg04gbahs
xWtW3Y/+B4LGM97o6lnu0OQI7MX5gY1G4Jgu6pgYv8tQd5XyU/CAJUA5VWTxUMIi
JP6qlzm1bz4AAPmGw4mkS1u4N+vai21Zl4iyFIQFeiuU/K2ISQQYEQIACQUCTyMR
8gIbDAAKCRDXbtx3JeAQzxReAJ4uvms1n7xV3CcJPQlM7ndX5MZU3QCgxp8zubcL
/SsMvw7XApSHFs5ooYc=
=Xc8P
-----END PGP PUBLIC KEY BLOCK-----
EOF
fi


./debian/util.sh build-all -a "$a" -c "$c" -T $T -K $K -f ./build/modules.conf.most -j -bn -z9 -v$VERSION-$n~$HASH

if [ $(ls -al ../freeswitch-mod* | wc -l) -lt 10 ]; then false; else true; fi

