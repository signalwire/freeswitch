##############################################################################
# Copyright and license
##############################################################################
#
# Spec file for package freeswitch-sounds-en-ca-june (version 1.0.51-1)
#
# Based on parts by Copyright (c) 2009 Patrick Laimbock 
# Copyright (c) 2014 FreeSWITCH.org
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

##############################################################################
# Set variables
##############################################################################

%define version	1.0.51
%define release	1

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

Summary: FreeSWITCH fr-CA June prompts
Name: freeswitch-sounds-en-ca-june
Version: %{version}
Release: %{release}%{?dist}
License: MPL
Group: Applications/Communications
Packager: Ken Rice <krice@freeswitch.org>
URL: http://www.freeswitch.org
Source0:http://files.freeswitch.org/releases/sounds/%{name}-48000-%{version}.tar.gz
Source1:http://files.freeswitch.org/releases/sounds/%{name}-32000-%{version}.tar.gz
Source2:http://files.freeswitch.org/releases/sounds/%{name}-16000-%{version}.tar.gz
Source3:http://files.freeswitch.org/releases/sounds/%{name}-8000-%{version}.tar.gz
BuildArch: noarch
BuildRequires: sox
Requires: freeswitch
Requires: freeswitch-sounds-en-ca-june-48000
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
FreeSWITCH 48kHz fr CA June prompts plus, during the installation,
it will also install locally generated 8KHz, 16KHz and 32KHz prompts

%package -n freeswitch-sounds-en-ca-june-8000
Summary: FreeSWITCH 8kHz fr CA June prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-en-ca-june-8000
FreeSWITCH 8kHz fr CA June prompts

%package -n freeswitch-sounds-en-ca-june-16000
Summary: FreeSWITCH 16kHz fr CA June prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-en-ca-june-16000
FreeSWITCH 16kHz fr CA June prompts

%package -n freeswitch-sounds-en-ca-june-32000
Summary: FreeSWITCH 32kHz fr CA June prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-en-ca-june-32000
FreeSWITCH 32kHz fr CA June prompts

%package -n freeswitch-sounds-en-ca-june-48000
Summary: FreeSWITCH 48kHz fr CA June prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-en-ca-june-48000
FreeSWITCH 48kHz fr CA June prompts

%package -n freeswitch-sounds-en-ca-june-all
Summary: FreeSWITCH fr CA June prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}
Requires: freeswitch-sounds-en-ca-june-8000 = %{version}
Requires: freeswitch-sounds-en-ca-june-16000 = %{version}
Requires: freeswitch-sounds-en-ca-june-32000 = %{version}
Requires: freeswitch-sounds-en-ca-june-48000 = %{version}

%description -n freeswitch-sounds-en-ca-june-all
FreeSWITCH Elena prompts package that pulls in the 8KHz, 16KHz, 32KHz and 48KHz RPMs

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
%{__install} -d -m 0750 %{buildroot}%{SOUNDSDIR}/en/ca/june

pushd ca/june
# first install the 48KHz sounds
%{__cp} -prv ./* %{buildroot}%{SOUNDSDIR}/en/ca/june
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

##############################################################################
# Postun
##############################################################################

%postun

##############################################################################
# Files
##############################################################################

%files
%defattr(-,root,root)

%files -n freeswitch-sounds-en-ca-june-8000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/ascii/8000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/base256/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/conference/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/currency/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/digits/8000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/ca/june/directory/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/ivr/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/misc/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/phonetic-ascii/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/time/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/voicemail/8000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/zrtp/8000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/ca/june/users/8000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/ascii/8000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/base256/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/conference/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/currency/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/digits/8000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/ca/june/directory/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/ivr/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/misc/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/phonetic-ascii/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/time/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/voicemail/8000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/zrtp/8000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/ca/june/users/8000/*.wav

%files -n freeswitch-sounds-en-ca-june-16000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/ascii/16000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/base256/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/conference/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/currency/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/digits/16000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/ca/june/directory/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/ivr/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/misc/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/phonetic-ascii/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/time/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/voicemail/16000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/zrtp/16000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/ca/june/users/16000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/ascii/16000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/base256/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/conference/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/currency/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/digits/16000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/ca/june/directory/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/ivr/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/misc/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/phonetic-ascii/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/time/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/voicemail/16000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/zrtp/16000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/ca/june/users/16000/*.wav

%files -n freeswitch-sounds-en-ca-june-32000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/ascii/32000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/base256/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/conference/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/currency/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/digits/32000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/ca/june/directory/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/ivr/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/misc/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/phonetic-ascii/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/time/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/voicemail/32000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/zrtp/32000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/ca/june/users/32000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/ascii/32000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/base256/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/conference/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/currency/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/digits/32000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/ca/june/directory/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/ivr/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/misc/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/phonetic-ascii/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/time/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/voicemail/32000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/zrtp/32000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/ca/june/users/32000/*.wav

%files -n freeswitch-sounds-en-ca-june-48000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/ascii/48000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/base256/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/conference/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/currency/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/digits/48000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/ca/june/directory/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/ivr/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/misc/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/phonetic-ascii/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/time/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/voicemail/48000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/en/ca/june/zrtp/48000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/en/ca/june/users/48000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/ascii/48000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/base256/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/conference/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/currency/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/digits/48000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/ca/june/directory/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/ivr/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/misc/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/phonetic-ascii/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/time/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/voicemail/48000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/en/ca/june/zrtp/48000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/en/ca/june/users/48000/*.wav

%files -n freeswitch-sounds-en-ca-june-all

##############################################################################
# Changelog
##############################################################################

%changelog
* Fri Apr 19 2019 Andrey Volk <andrey@signalwire.com> - 1.0.51-1
- version bump
* Fri Sep 12 2014 Ken Rice <krice@freeswitch.org> - 1.0.50-1
- created out of the spec file for june
