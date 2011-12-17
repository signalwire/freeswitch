%define prefix	/opt/freeswitch
%define _prefix	%{prefix}


Summary:	FreeSWITCH Music on Hold soundfiles
Name:		freeswitch-sounds-music
Version:	1.0.8
Release:	1%{?dist}
License:	MPL
Group:		Productivity/Telephony/Servers
Packager:	Joseph L. Casale <jcasale@activenetwerx.com>
URL:		http://www.freeswitch.org
Source0:	http://files.freeswitch.org/%{name}-8000-%{version}.tar.gz
Source1:        http://files.freeswitch.org/%{name}-16000-%{version}.tar.gz
Source2:        http://files.freeswitch.org/%{name}-32000-%{version}.tar.gz
Source3:        http://files.freeswitch.org/%{name}-48000-%{version}.tar.gz
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
%{__install} -d -m 0750 %{buildroot}/%{_prefix}/sounds/music/{8000,16000,32000,48000}
%{__cp} -prv ./{8000,16000,32000,48000} %{buildroot}%{_prefix}/sounds/music


%clean
%{__rm} -rf %{buildroot}


%post


%postun


%files


%files -n freeswitch-sounds-music-8000
%defattr(-,root,root,-)
%dir	%{_prefix}/sounds/music/8000
%{_prefix}/sounds/music/8000/*.wav


%files -n freeswitch-sounds-music-16000
%defattr(-,root,root,-)
%dir	%{_prefix}/sounds/music/16000
%{_prefix}/sounds/music/16000/*.wav


%files -n freeswitch-sounds-music-32000
%defattr(-,root,root,-)
%dir	%{_prefix}/sounds/music/32000
%{_prefix}/sounds/music/32000/*.wav


%files -n freeswitch-sounds-music-48000
%defattr(-,root,root,-)
%dir	%{_prefix}/sounds/music/48000
%{_prefix}/sounds/music/48000/*.wav


%changelog
* Sat Jul 16 2011 Joseph Casale <jcasale@activenetwerx.com> 1.0.8-1
- Initial release