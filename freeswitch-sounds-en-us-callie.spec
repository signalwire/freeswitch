##############################################################################
# Copyright and license
##############################################################################
#
# Spec file for package freeswitch-sounds-en-us-callie (version 1.0.18-1)
#
# Copyright (c) 2009 Patrick Laimbock 
# Some fixes and additions (c) 2011 Michal Bielicki
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

##############################################################################
# Determine distribution
##############################################################################

# %define is_rhel5 %(test -f /etc/redhat-release && egrep -q 'release 5' /etc/redhat-release && echo 1 || echo 0)

##############################################################################
# Set variables
##############################################################################

%define version %{VERSION_NUMBER}
%define release %{BUILD_NUMBER}

%define fsname  freeswitch
# you could add a version number to be more strict

%define PREFIX          %{_prefix}
%define EXECPREFIX      %{_exec_prefix}
%define BINDIR          %{_bindir}
%define SBINDIR         %{_sbindir}
%define LIBEXECDIR      %{_libexecdir}/%{fsname}
%define SYSCONFDIR      %{_sysconfdir}/%{fsname}
%define SHARESTATEDIR   %{_sharedstatedir}/%{fsname}
%define LOCALSTATEDIR   %{_localstatedir}/lib/%{fsname}
%define LIBDIR          %{_libdir}
%define INCLUDEDIR      %{_includedir}
%define _datarootdir    %{_prefix}/share
%define DATAROOTDIR     %{_datarootdir}
%define DATADIR         %{_datadir}
%define INFODIR         %{_infodir}
%define LOCALEDIR       %{_datarootdir}/locale
%define MANDIR          %{_mandir}
%define DOCDIR          %{_defaultdocdir}/%{fsname}
%define HTMLDIR         %{_defaultdocdir}/%{fsname}/html
%define DVIDIR          %{_defaultdocdir}/%{fsname}/dvi
%define PDFDIR          %{_defaultdocdir}/%{fsname}/pdf
%define PSDIR           %{_defaultdocdir}/%{fsname}/ps
%define LOGFILEDIR      /var/log/%{fsname}
%define MODINSTDIR      %{_libdir}/%{fsname}/mod
%define RUNDIR          %{_localstatedir}/run/%{fsname}
%define DBDIR           %{LOCALSTATEDIR}/db
%define HTDOCSDIR       %{_datarootdir}/%{fsname}/htdocs
%define SOUNDSDIR       %{_datarootdir}/%{fsname}/sounds
%define GRAMMARDIR      %{_datarootdir}/%{fsname}/grammar
%define SCRIPTDIR       %{_datarootdir}/%{fsname}/scripts
%define RECORDINGSDIR   %{LOCALSTATEDIR}/recordings
%define PKGCONFIGDIR    %{_datarootdir}/%{fsname}/pkgconfig
%define HOMEDIR         %{LOCALSTATEDIR}


##############################################################################
# General
##############################################################################

Summary: FreeSWITCH en-us Callie prompts
Name: freeswitch-sounds-en-us-callie
Version: %{version}
Release: %{release}%{?dist}
License: MPL
Group: Applications/Communications
Packager: Patrick Laimbock <vc-rpms@voipconsulting.nl>
URL: http://www.freeswitch.org
Source0:http://files.freeswitch.org/%{name}-48000-%{version}.tar.gz
Source1:http://files.freeswitch.org/%{name}-32000-%{version}.tar.gz
Source2:http://files.freeswitch.org/%{name}-16000-%{version}.tar.gz
Source3:http://files.freeswitch.org/%{name}-8000-%{version}.tar.gz
BuildArch: noarch
BuildRequires: sox
Requires: freeswitch
Requires: freeswitch-sounds-en-us-callie-48000
Requires: sox
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
FreeSWITCH 48kHz en-us Callie prompts plus, during the installation,
it will also install locally generated 8KHz, 16KHz and 32KHz prompts

%package -n freeswitch-sounds-en-us-callie-8000
Summary: FreeSWITCH 8kHz en-us Callie prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-en-us-callie-8000
FreeSWITCH 8kHz en-us Callie prompts

%package -n freeswitch-sounds-en-us-callie-16000
Summary: FreeSWITCH 16kHz en-us Callie prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-en-us-callie-16000
FreeSWITCH 16kHz en-us Callie prompts

%package -n freeswitch-sounds-en-us-callie-32000
Summary: FreeSWITCH 32kHz en-us Callie prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-en-us-callie-32000
FreeSWITCH 32kHz en-us Callie prompts

%package -n freeswitch-sounds-en-us-callie-48000
Summary: FreeSWITCH 48kHz en-us Callie prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-en-us-callie-48000
FreeSWITCH 48kHz en-us Callie prompts

%package -n freeswitch-sounds-en-us-callie-all
Summary: FreeSWITCH en-us Callie prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}
Requires: freeswitch-sounds-en-us-callie-8000 = %{version}
Requires: freeswitch-sounds-en-us-callie-16000 = %{version}
Requires: freeswitch-sounds-en-us-callie-32000 = %{version}
Requires: freeswitch-sounds-en-us-callie-48000 = %{version}

%description -n freeswitch-sounds-en-us-callie-all
FreeSWITCH Callie prompts package that pulls in the 8KHz, 16KHz,
32KHz and 48KHz RPMs

##############################################################################
# Prep
##############################################################################

%prep
%setup -n en
%setup -T -D -b 0 -n en
%setup -T -D -b 1 -n en
%setup -T -D -b 2 -n en
%setup -T -D -b 3 -n en

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
%{__install} -d -m 0750 %{buildroot}%{SOUNDSDIR}/en/us/callie

pushd us/callie
# first install the 48KHz sounds
%{__cp} -prv ./* %{buildroot}%{SOUNDSDIR}/en/us/callie
# now resample the 48KHz ones to 8KHz, 16KHz and 32KHz
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

%files -n freeswitch-sounds-en-us-callie-8000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/ascii/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/base256/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/conference/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/currency/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/digits/8000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/us/callie/directory/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/ivr/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/misc/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/phonetic-ascii/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/time/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/voicemail/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/zrtp/8000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/ascii/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/base256/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/conference/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/currency/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/digits/8000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/us/callie/directory/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/ivr/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/misc/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/phonetic-ascii/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/time/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/voicemail/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/zrtp/8000/*.wav

%files -n freeswitch-sounds-en-us-callie-16000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/ascii/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/base256/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/conference/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/currency/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/digits/16000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/us/callie/directory/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/ivr/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/misc/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/phonetic-ascii/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/time/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/voicemail/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/zrtp/16000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/ascii/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/base256/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/conference/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/currency/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/digits/16000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/us/callie/directory/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/ivr/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/misc/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/phonetic-ascii/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/time/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/voicemail/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/zrtp/16000/*.wav

%files -n freeswitch-sounds-en-us-callie-32000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/ascii/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/base256/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/conference/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/currency/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/digits/32000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/us/callie/directory/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/ivr/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/misc/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/phonetic-ascii/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/time/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/voicemail/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/zrtp/32000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/ascii/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/base256/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/conference/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/currency/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/digits/32000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/us/callie/directory/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/ivr/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/misc/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/phonetic-ascii/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/time/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/voicemail/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/zrtp/32000/*.wav

%files -n freeswitch-sounds-en-us-callie-48000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/ascii/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/base256/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/conference/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/currency/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/digits/48000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/us/callie/directory/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/ivr/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/misc/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/phonetic-ascii/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/time/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/voicemail/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/us/callie/zrtp/48000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/ascii/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/base256/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/conference/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/currency/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/digits/48000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/us/callie/directory/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/ivr/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/misc/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/phonetic-ascii/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/time/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/voicemail/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/us/callie/zrtp/48000/*.wav

%files -n freeswitch-sounds-en-us-callie-all

##############################################################################
# Changelog
##############################################################################

%changelog
* Sun Mar 05 2012 Ken Rice <krice@freeswitch.org> - 1.0.18-1
- update to FHS Layout for FreeSWITCH
- bump up version
* Sun May 22 2011 Michal Bielicki <michal.bielicki@seventhsignal.de> - 1.0.16-1
- bump up version
* Tue Jan 18 2011 Michal Bielicki <michal.bielicki@seventhsignal.de> - 1.0.14-1
- bump up version
- include script into freeswitch core
- include specfile into freeswitch core
- runtime does not require sox, only building

* Thu Dec 17 2009 Patrick Laimbock <vc-rpms@voipconsulting.nl> - 1.0.12-8
- update perms and user/group to sync with the old situation

* Wed Dec 16 2009 Patrick Laimbock <vc-rpms@voipconsulting.nl> - 1.0.12-7
- make main package require freeswitch-sounds-en-us-callie-48000 and
- generate the 8KHz, 16KHz and 32KHz sounds from there
- add license to spec file

* Wed Dec 16 2009 Patrick Laimbock <vc-rpms@voipconsulting.nl> - 1.0.12-5
- put 48KHz in a separate package and let the main package Require 48KHz
- and then use the script to generate the 8KHz, 16KHz and 32KHz sounds

* Wed Dec 16 2009 Patrick Laimbock <vc-rpms@voipconsulting.nl> - 1.0.12-4
- add freeswitch-sounds-en-us-callie-all package that pulls in the 8KHz,
- 16KHz, 32KHz and 48KHz RPM packages 

* Tue Dec 15 2009 Patrick Laimbock <vc-rpms@voipconsulting.nl> - 1.0.12-3
- override subpackage name with -n so it no longer builds an empty main RPM
- rework spec file
- add sox as a requirement
- run buildsounds-callie.sh in post to generate 8KHz, 16KHz and 32KHz prompts

* Tue Dec 15 2009 Patrick Laimbock <vc-rpms@voipconsulting.nl> - 1.0.12-2
- can't override Name in subpackage so put all versions in RPM subpackages 
- with an empty main RPM package

* Tue Dec 15 2009 Patrick Laimbock <vc-rpms@voipconsulting.nl> - 1.0.12-1
- create spec file with the following requirement:
- source only contains the 48KHz sound prompts
- during build the 48KHz sound prompts are resampled to 8KHz, 16KHz and 32KHz
- the 8KHz, 16KHz, 32KHz and 48KHz sound prompts are packaged separately

