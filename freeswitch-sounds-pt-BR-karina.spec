##############################################################################
# Copyright and license
##############################################################################
#
# Spec file for package freeswitch-sounds-pt-BR-karina (version 1.0.50-1)
#
# Based on parts by Copyright (c) 2009 Patrick Laimbock 
# Copyright (c) 2014 FreeSWITCH.org
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

##############################################################################
# Set variables
##############################################################################

%define version	1.0.50
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

Summary: FreeSWITCH pt-BR Karina prompts
Name: freeswitch-sounds-pt-BR-karina
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
Requires: freeswitch-sounds-pt-BR-karina-48000
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
FreeSWITCH 48kHz fr BR Karina prompts plus, during the installation,
it will also install locally generated 8KHz, 16KHz and 32KHz prompts

%package -n freeswitch-sounds-pt-BR-karina-8000
Summary: FreeSWITCH 8kHz fr BR Karina prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-pt-BR-karina-8000
FreeSWITCH 8kHz fr BR Karina prompts

%package -n freeswitch-sounds-pt-BR-karina-16000
Summary: FreeSWITCH 16kHz fr BR Karina prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-pt-BR-karina-16000
FreeSWITCH 16kHz fr BR Karina prompts

%package -n freeswitch-sounds-pt-BR-karina-32000
Summary: FreeSWITCH 32kHz fr BR Karina prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-pt-BR-karina-32000
FreeSWITCH 32kHz fr BR Karina prompts

%package -n freeswitch-sounds-pt-BR-karina-48000
Summary: FreeSWITCH 48kHz fr BR Karina prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-pt-BR-karina-48000
FreeSWITCH 48kHz fr BR Karina prompts

%package -n freeswitch-sounds-pt-BR-karina-all
Summary: FreeSWITCH fr BR Karina prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}
Requires: freeswitch-sounds-pt-BR-karina-8000 = %{version}
Requires: freeswitch-sounds-pt-BR-karina-16000 = %{version}
Requires: freeswitch-sounds-pt-BR-karina-32000 = %{version}
Requires: freeswitch-sounds-pt-BR-karina-48000 = %{version}

%description -n freeswitch-sounds-pt-BR-karina-all
FreeSWITCH Elena prompts package that pulls in the 8KHz, 16KHz, 32KHz and 48KHz RPMs

##############################################################################
# Prep
##############################################################################

%prep
%setup -n pt
%setup -T -D -b 0 -n pt
%setup -T -D -b 1 -n pt
%setup -T -D -b 2 -n pt
%setup -T -D -b 3 -n pt

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
%{__install} -d -m 0750 %{buildroot}%{SOUNDSDIR}/pt/BR/karina

pushd BR/karina
# first install the 48KHz sounds
%{__cp} -prv ./* %{buildroot}%{SOUNDSDIR}/pt/BR/karina
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

%files -n freeswitch-sounds-pt-BR-karina-8000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/ascii/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/base256/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/conference/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/currency/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/digits/8000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/pt/BR/karina/directory/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/ivr/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/misc/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/phonetic-ascii/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/time/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/voicemail/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/zrtp/8000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/pt/BR/karina/users/8000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/ascii/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/base256/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/conference/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/currency/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/digits/8000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/pt/BR/karina/directory/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/ivr/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/misc/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/phonetic-ascii/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/time/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/voicemail/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/zrtp/8000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/pt/BR/karina/users/8000/*.wav

%files -n freeswitch-sounds-pt-BR-karina-16000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/ascii/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/base256/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/conference/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/currency/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/digits/16000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/pt/BR/karina/directory/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/ivr/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/misc/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/phonetic-ascii/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/time/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/voicemail/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/zrtp/16000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/pt/BR/karina/users/16000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/ascii/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/base256/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/conference/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/currency/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/digits/16000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/pt/BR/karina/directory/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/ivr/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/misc/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/phonetic-ascii/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/time/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/voicemail/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/zrtp/16000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/pt/BR/karina/users/16000/*.wav

%files -n freeswitch-sounds-pt-BR-karina-32000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/ascii/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/base256/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/conference/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/currency/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/digits/32000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/pt/BR/karina/directory/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/ivr/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/misc/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/phonetic-ascii/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/time/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/voicemail/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/zrtp/32000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/pt/BR/karina/users/32000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/ascii/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/base256/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/conference/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/currency/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/digits/32000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/pt/BR/karina/directory/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/ivr/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/misc/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/phonetic-ascii/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/time/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/voicemail/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/zrtp/32000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/pt/BR/karina/users/32000/*.wav

%files -n freeswitch-sounds-pt-BR-karina-48000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/ascii/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/base256/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/conference/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/currency/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/digits/48000
%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/pt/BR/karina/directory/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/ivr/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/misc/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/phonetic-ascii/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/time/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/voicemail/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/pt/BR/karina/zrtp/48000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/pt/BR/karina/users/48000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/ascii/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/base256/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/conference/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/currency/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/digits/48000/*.wav
%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/pt/BR/karina/directory/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/ivr/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/misc/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/phonetic-ascii/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/time/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/voicemail/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/pt/BR/karina/zrtp/48000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/pt/BR/karina/users/48000/*.wav

%files -n freeswitch-sounds-pt-BR-karina-all

##############################################################################
# Changelog
##############################################################################

%changelog
* Fri Sep 12 2014 Ken Rice <krice@freeswitch.org> - 1.0.50-1
- created out of the spec file for june
