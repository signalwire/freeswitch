%define fsname		freeswitch
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


Summary:	FreeSWITCH Music on Hold soundfiles
Name:		freeswitch-sounds-music
Version:	1.0.50
Release:	2%{?dist}
License:	MPL
Group:		Productivity/Telephony/Servers
Packager:	Joseph L. Casale <jcasale@activenetwerx.com>
URL:		http://www.freeswitch.org
Source0:	http://files.freeswitch.org/releases/sounds/%{name}-8000-%{version}.tar.gz
Source1:        http://files.freeswitch.org/releases/sounds/%{name}-16000-%{version}.tar.gz
Source2:        http://files.freeswitch.org/releases/sounds/%{name}-32000-%{version}.tar.gz
Source3:        http://files.freeswitch.org/releases/sounds/%{name}-48000-%{version}.tar.gz
BuildArch:	noarch
BuildRequires:	bash
Requires:	freeswitch
Requires:       freeswitch-sounds-music-8000 = %{version}
Requires:       freeswitch-sounds-music-16000 = %{version}
Requires:       freeswitch-sounds-music-32000 = %{version}
Requires:       freeswitch-sounds-music-48000 = %{version}
BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

%description
FreeSWITCH Music On Hold soundfiles package that installs the 8KHz, 16KHz,
32KHz and 48KHz RPMs


%package -n freeswitch-sounds-music-8000
Summary:        FreeSWITCH 8kHz Music On Hold soundfiles
Group:          Productivity/Telephony/Servers
BuildArch:      noarch
Requires:       freeswitch

%description -n freeswitch-sounds-music-8000
FreeSWITCH 8kHz Music On Hold soundfiles


%package -n freeswitch-sounds-music-16000
Summary:        FreeSWITCH 16kHz Music On Hold soundfiles
Group:          Productivity/Telephony/Servers
BuildArch:      noarch
Requires:       freeswitch

%description -n freeswitch-sounds-music-16000
FreeSWITCH 16kHz Music On Hold soundfiles


%package -n freeswitch-sounds-music-32000
Summary:        FreeSWITCH 32kHz Music On Hold soundfiles
Group:          Productivity/Telephony/Servers
BuildArch:      noarch
Requires:       freeswitch

%description -n freeswitch-sounds-music-32000
FreeSWITCH 32kHz Music On Hold soundfiles


%package -n freeswitch-sounds-music-48000
Summary:        FreeSWITCH 48kHz Music On Hold soundfiles
Group:          Productivity/Telephony/Servers
BuildArch:      noarch
Requires:       freeswitch

%description -n freeswitch-sounds-music-48000
FreeSWITCH 48kHz Music On Hold soundfiles


%prep
%setup -n music
%setup -T -D -b 1 -n music
%setup -T -D -b 2 -n music
%setup -T -D -b 3 -n music


%build


%install
%{__rm} -rf %{buildroot}
%{__install} -d -m 0750 %{buildroot}/%{SOUNDSDIR}/music/{8000,16000,32000,48000}
%{__cp} -prv ./{8000,16000,32000,48000} %{buildroot}%{SOUNDSDIR}/music


%clean
%{__rm} -rf %{buildroot}


%post


%postun


%files


%files -n freeswitch-sounds-music-8000
%defattr(-,root,root,-)
%dir	%{SOUNDSDIR}/music/8000
%{SOUNDSDIR}/music/8000/*.wav


%files -n freeswitch-sounds-music-16000
%defattr(-,root,root,-)
%dir	%{SOUNDSDIR}/music/16000
%{SOUNDSDIR}/music/16000/*.wav


%files -n freeswitch-sounds-music-32000
%defattr(-,root,root,-)
%dir	%{SOUNDSDIR}/music/32000
%{SOUNDSDIR}/music/32000/*.wav


%files -n freeswitch-sounds-music-48000
%defattr(-,root,root,-)
%dir	%{SOUNDSDIR}/music/48000
%{SOUNDSDIR}/music/48000/*.wav


%changelog
* Sat Jul 16 2011 Joseph Casale <jcasale@activenetwerx.com> 1.0.8-2
- Fix up for FreeSWITCH FHS and AutoBuild System
* Sat Jul 16 2011 Joseph Casale <jcasale@activenetwerx.com> 1.0.8-1
- Initial release
