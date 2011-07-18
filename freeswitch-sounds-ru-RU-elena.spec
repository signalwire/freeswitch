##############################################################################
# Copyright and license
##############################################################################
#
# Spec file for package freeswitch-sounds-ru-RU-elena (version 1.0.13-1)
#
# Based on parts by Copyright (c) 2009 Patrick Laimbock 
# Copyright (c) 2011 Michal Bielicki
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

##############################################################################
# Determine distribution
##############################################################################

%define is_rhel5 %(test -f /etc/redhat-release && egrep -q 'release 5' /etc/redhat-release && echo 1 || echo 0)

##############################################################################
# Set variables
##############################################################################

%define version	1.0.13
%define release	1

%define fsname  freeswitch
# you could add a version number to be more strict

%define prefix	/opt/freeswitch
%define _prefix	%{prefix}

##############################################################################
# General
##############################################################################

Summary: FreeSWITCH ru-RU Elena prompts
Name: freeswitch-sounds-ru-RU-elena
Version: %{version}
Release: %{release}%{?dist}
License: MPL
Group: Applications/Communications
Packager: Michal Bielicki <michal.bielicki@seventhsignal.de>
URL: http://www.freeswitch.org
Source0:http://files.freeswitch.org/%{name}-48000-%{version}.tar.gz
BuildArch: noarch
BuildRequires: sox
Requires: freeswitch
Requires: freeswitch-sounds-ru-RU-elena-48000
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
FreeSWITCH 48kHz ru-RU Elena prompts plus, during the installation,
it will also install locally generated 8KHz, 16KHz and 32KHz prompts

%package -n freeswitch-sounds-ru-RU-elena-8000
Summary: FreeSWITCH 8kHz ru-RU Elena prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-ru-RU-elena-8000
FreeSWITCH 8kHz ru-RU Elena prompts

%package -n freeswitch-sounds-ru-RU-elena-16000
Summary: FreeSWITCH 16kHz ru-RU Elena prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-ru-RU-elena-16000
FreeSWITCH 16kHz ru-RU Elena prompts

%package -n freeswitch-sounds-ru-RU-elena-32000
Summary: FreeSWITCH 32kHz ru-RU Elena prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-ru-RU-elena-32000
FreeSWITCH 32kHz ru-RU Elena prompts

%package -n freeswitch-sounds-ru-RU-elena-48000
Summary: FreeSWITCH 48kHz ru-RU Elena prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-ru-RU-elena-48000
FreeSWITCH 48kHz ru-RU Elena prompts

%package -n freeswitch-sounds-ru-RU-elena-all
Summary: FreeSWITCH ru-RU Elena prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}
Requires: freeswitch-sounds-ru-RU-elena-8000 = %{version}
Requires: freeswitch-sounds-ru-RU-elena-16000 = %{version}
Requires: freeswitch-sounds-ru-RU-elena-32000 = %{version}
Requires: freeswitch-sounds-ru-RU-elena-48000 = %{version}

%description -n freeswitch-sounds-ru-RU-elena-all
FreeSWITCH Elena prompts package that pulls in the 8KHz, 16KHz, 32KHz and 48KHz RPMs

##############################################################################
# Prep
##############################################################################

%prep
%setup -b0 -q -n ru
mkdir -p ./RU/elena
# create buildsounds-elena.sh script in working dir
echo  '#!/bin/bash

sounds_location=$1
for rate in 32000 16000 8000
do 
    for i in ascii base256 conference currency digits directory ivr misc phonetic-ascii time voicemail zrtp users
    do
	mkdir -p $sounds_location/$i/$rate
	for f in `find $sounds_location/$i/48000 -name \*.wav`
	do
	    echo "generating" $sounds_location/$i/$rate/`basename $f`
	    sox $f -r $rate $sounds_location/$i/$rate/`basename $f`
	done
    done
done' > ./RU/elena/buildsounds-elena.sh
%{__chmod} 0750 ./RU/elena/buildsounds-elena.sh

##############################################################################
# Build
##############################################################################

%build
# nothing to do here

##############################################################################
# Install
##############################################################################

%install
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}

# create the sounds directories
%{__install} -d -m 0750 %{buildroot}%{_prefix}/sounds/ru/RU/elena

pushd RU/elena
# first install the 48KHz sounds
%{__cp} -prv ./* %{buildroot}%{_prefix}/sounds/ru/RU/elena
# now resample the 48KHz ones to 8KHz, 16KHz and 32KHz
./buildsounds-elena.sh %{buildroot}%{_prefix}/sounds/ru/RU/elena
popd

##############################################################################
# Clean
##############################################################################

%clean
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}

##############################################################################
# Post
##############################################################################

%post
# generate the 8KHz, 16KHz and 32KHz prompts from the 48KHz ones
cd %{_prefix}/sounds/ru/RU/elena
./buildsounds-elena.sh %{_prefix}/sounds/ru/RU/elena

##############################################################################
# Postun
##############################################################################

%postun
# you could check if there are sound files in 8000/ or
# 16000/ or 32000/ and remove them *only* if the files
# do not belong to an rpm

##############################################################################
# Files
##############################################################################

%files
%defattr(-,root,root)
%attr(0750,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/buildsounds-elena.sh

%files -n freeswitch-sounds-ru-RU-elena-8000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/ascii/8000
#%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/base256/8000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/conference/8000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/currency/8000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/digits/8000
%attr(0750,freeswitch,daemon)   %dir    %{_prefix}/sounds/ru/RU/elena/directory/8000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/ivr/8000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/misc/8000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/phonetic-ascii/8000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/time/8000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/voicemail/8000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/zrtp/8000
%attr(0750,freeswitch,daemon)   %dir    %{_prefix}/sounds/ru/RU/elena/users/8000
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/ascii/8000/*.wav
#%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/base256/8000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/conference/8000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/currency/8000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/digits/8000/*.wav
%attr(0640,freeswitch,daemon)           %{_prefix}/sounds/ru/RU/elena/directory/8000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/ivr/8000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/misc/8000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/phonetic-ascii/8000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/time/8000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/voicemail/8000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/zrtp/8000/*.wav
%attr(0640,freeswitch,daemon)           %{_prefix}/sounds/ru/RU/elena/users/8000/*.wav

%files -n freeswitch-sounds-ru-RU-elena-16000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/ascii/16000
#%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/base256/16000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/conference/16000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/currency/16000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/digits/16000
%attr(0750,freeswitch,daemon)   %dir    %{_prefix}/sounds/ru/RU/elena/directory/16000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/ivr/16000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/misc/16000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/phonetic-ascii/16000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/time/16000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/voicemail/16000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/zrtp/16000
%attr(0750,freeswitch,daemon)   %dir    %{_prefix}/sounds/ru/RU/elena/users/16000
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/ascii/16000/*.wav
#%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/base256/16000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/conference/16000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/currency/16000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/digits/16000/*.wav
%attr(0640,freeswitch,daemon)           %{_prefix}/sounds/ru/RU/elena/directory/16000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/ivr/16000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/misc/16000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/phonetic-ascii/16000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/time/16000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/voicemail/16000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/zrtp/16000/*.wav
%attr(0640,freeswitch,daemon)           %{_prefix}/sounds/ru/RU/elena/users/16000/*.wav

%files -n freeswitch-sounds-ru-RU-elena-32000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/ascii/32000
#%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/base256/32000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/conference/32000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/currency/32000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/digits/32000
%attr(0750,freeswitch,daemon)   %dir    %{_prefix}/sounds/ru/RU/elena/directory/32000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/ivr/32000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/misc/32000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/phonetic-ascii/32000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/time/32000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/voicemail/32000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/zrtp/32000
%attr(0750,freeswitch,daemon)   %dir    %{_prefix}/sounds/ru/RU/elena/users/32000
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/ascii/32000/*.wav
#%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/base256/32000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/conference/32000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/currency/32000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/digits/32000/*.wav
%attr(0640,freeswitch,daemon)           %{_prefix}/sounds/ru/RU/elena/directory/32000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/ivr/32000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/misc/32000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/phonetic-ascii/32000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/time/32000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/voicemail/32000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/zrtp/32000/*.wav
%attr(0640,freeswitch,daemon)           %{_prefix}/sounds/ru/RU/elena/users/32000/*.wav

%files -n freeswitch-sounds-ru-RU-elena-48000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/ascii/48000
#%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/base256/48000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/conference/48000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/currency/48000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/digits/48000
%attr(0750,freeswitch,daemon)   %dir    %{_prefix}/sounds/ru/RU/elena/directory/48000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/ivr/48000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/misc/48000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/phonetic-ascii/48000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/time/48000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/voicemail/48000
%attr(0750,freeswitch,daemon)	%dir	%{_prefix}/sounds/ru/RU/elena/zrtp/48000
%attr(0750,freeswitch,daemon)   %dir    %{_prefix}/sounds/ru/RU/elena/users/48000
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/ascii/48000/*.wav
#%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/base256/48000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/conference/48000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/currency/48000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/digits/48000/*.wav
%attr(0640,freeswitch,daemon)           %{_prefix}/sounds/ru/RU/elena/directory/48000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/ivr/48000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/misc/48000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/phonetic-ascii/48000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/time/48000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/voicemail/48000/*.wav
%attr(0640,freeswitch,daemon)		%{_prefix}/sounds/ru/RU/elena/zrtp/48000/*.wav
%attr(0640,freeswitch,daemon)           %{_prefix}/sounds/ru/RU/elena/users/48000/*.wav

%files -n freeswitch-sounds-ru-RU-elena-all

##############################################################################
# Changelog
##############################################################################

%changelog
* Mon Jul 11 2011 Michal Bielicki <michal.bielicki@seventhsignal.de> - 1.0.13-1
- created out of the spec file for elena
