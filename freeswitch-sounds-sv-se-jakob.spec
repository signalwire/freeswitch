##############################################################################
# Copyright and license
##############################################################################
#
# Spec file for package freeswitch-sounds-sv-se-jakob (version 1.0.50-1)
#
# Copyright (c) 2009 Patrick Laimbock 
# Copied and modified for mod_say_sv (c) 2013 Jakob Sundberg
# Additional changes (c) 2014 Ken Rice
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

##############################################################################
# Set variables
##############################################################################

%define version 1.0.50
%define release 1

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

Summary: FreeSWITCH sv-se Jakob prompts
Name: freeswitch-sounds-sv-se-jakob
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
Requires: freeswitch-sounds-sv-se-jakob-48000
Requires: sox
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
FreeSWITCH 48kHz sv-se jakob prompts plus, during the installation,
it will also install locally generated 8KHz, 16KHz and 32KHz prompts

%package -n freeswitch-sounds-sv-se-jakob-8000
Summary: FreeSWITCH 8kHz sv-se jakob prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-sv-se-jakob-8000
FreeSWITCH 8kHz sv-se jakob prompts

%package -n freeswitch-sounds-sv-se-jakob-16000
Summary: FreeSWITCH 16kHz sv-se jakob prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-sv-se-jakob-16000
FreeSWITCH 16kHz sv-se jakob prompts

%package -n freeswitch-sounds-sv-se-jakob-32000
Summary: FreeSWITCH 32kHz sv-se jakob prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-sv-se-jakob-32000
FreeSWITCH 32kHz sv-se jakob prompts

%package -n freeswitch-sounds-sv-se-jakob-48000
Summary: FreeSWITCH 48kHz sv-se jakob prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}

%description -n freeswitch-sounds-sv-se-jakob-48000
FreeSWITCH 48kHz sv-se jakob prompts

%package -n freeswitch-sounds-sv-se-jakob-all
Summary: FreeSWITCH sv-se jakob prompts
Group: Applications/Communications
BuildArch: noarch
Requires: %{fsname}
Requires: freeswitch-sounds-sv-se-jakob-8000 = %{version}
Requires: freeswitch-sounds-sv-se-jakob-16000 = %{version}
Requires: freeswitch-sounds-sv-se-jakob-32000 = %{version}
Requires: freeswitch-sounds-sv-se-jakob-48000 = %{version}

%description -n freeswitch-sounds-sv-se-jakob-all
FreeSWITCH jakob prompts package that pulls in the 8KHz, 16KHz,
32KHz and 48KHz RPMs

##############################################################################
# Prep
##############################################################################

%prep
%setup -n sv
%setup -T -D -b 0 -n sv
%setup -T -D -b 1 -n sv
%setup -T -D -b 2 -n sv
%setup -T -D -b 3 -n sv

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
%{__install} -d -m 0750 %{buildroot}%{SOUNDSDIR}/sv/se/jakob

pushd se/jakob
# first install the 48KHz sounds
%{__cp} -prv ./* %{buildroot}%{SOUNDSDIR}/sv/se/jakob
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
# you could check if there are sound files in 8000/ or
# 16000/ or 32000/ and remove them *only* if the files
# do not belong to an rpm

##############################################################################
# Files
##############################################################################

%files
%defattr(-,root,root)

%files -n freeswitch-sounds-sv-se-jakob-8000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/ascii/8000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/base256/8000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/conference/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/currency/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/digits/8000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/sv/se/jakob/directory/8000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/ivr/8000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/misc/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/phonetic-ascii/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/time/8000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/voicemail/8000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/zrtp/8000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/ascii/8000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/base256/8000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/conference/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/currency/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/digits/8000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/sv/se/jakob/directory/8000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/ivr/8000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/misc/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/phonetic-ascii/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/time/8000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/voicemail/8000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/zrtp/8000/*.wav

%files -n freeswitch-sounds-sv-se-jakob-16000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/ascii/16000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/base256/16000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/conference/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/currency/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/digits/16000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/sv/se/jakob/directory/16000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/ivr/16000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/misc/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/phonetic-ascii/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/time/16000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/voicemail/16000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/zrtp/16000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/ascii/16000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/base256/16000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/conference/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/currency/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/digits/16000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/sv/se/jakob/directory/16000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/ivr/16000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/misc/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/phonetic-ascii/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/time/16000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/voicemail/16000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/zrtp/16000/*.wav

%files -n freeswitch-sounds-sv-se-jakob-32000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/ascii/32000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/base256/32000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/conference/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/currency/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/digits/32000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/sv/se/jakob/directory/32000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/ivr/32000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/misc/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/phonetic-ascii/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/time/32000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/voicemail/32000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/zrtp/32000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/ascii/32000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/base256/32000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/conference/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/currency/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/digits/32000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/sv/se/jakob/directory/32000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/ivr/32000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/misc/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/phonetic-ascii/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/time/32000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/voicemail/32000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/zrtp/32000/*.wav

%files -n freeswitch-sounds-sv-se-jakob-48000
%defattr(-,root,root,-)
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/ascii/48000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/base256/48000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/conference/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/currency/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/digits/48000
#%attr(0750,freeswitch,daemon)   %dir    %{SOUNDSDIR}/sv/se/jakob/directory/48000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/ivr/48000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/misc/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/phonetic-ascii/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/time/48000
%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/voicemail/48000
#%attr(0750,freeswitch,daemon)	%dir	%{SOUNDSDIR}/sv/se/jakob/zrtp/48000
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/ascii/48000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/base256/48000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/conference/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/currency/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/digits/48000/*.wav
#%attr(0640,freeswitch,daemon)           %{SOUNDSDIR}/sv/se/jakob/directory/48000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/ivr/48000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/misc/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/phonetic-ascii/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/time/48000/*.wav
%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/voicemail/48000/*.wav
#%attr(0640,freeswitch,daemon)		%{SOUNDSDIR}/sv/se/jakob/zrtp/48000/*.wav

%files -n freeswitch-sounds-sv-se-jakob-all

##############################################################################
# Changelog
##############################################################################

%changelog
* Mon Sep 15 2014 Ken Rice <krice@freeswitch.org> - 1.0.50-1
- new spec file for jakob
