Name:           sofia-sip
Version:        1.13.5
Release:        1%{?dist}
Summary:        Sofia SIP User-Agent library

License:        LGPLv2+
URL:            https://github.com/freeswitch/sofia-sip
Source0:        https://files.freeswitch.org/downloads/libs/%{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  openssl-devel >= 0.9.7
BuildRequires:  glib2-devel >=  2.4
BuildRequires:  lksctp-tools-devel
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  make
BuildRequires:  libtool >= 1.5.17

%description
Sofia SIP is a RFC-3261-compliant library for SIP user agents and
other network elements.  The Session Initiation Protocol (SIP) is an
application-layer control (signaling) protocol for creating,
modifying, and terminating sessions with one or more
participants. These sessions include Internet telephone calls,
multimedia distribution, and multimedia conferences.

%package devel
Summary:        Sofia-SIP Development Package
Requires:       sofia-sip = %{version}-%{release}
Requires:       pkgconfig

%description devel
Development package for Sofia SIP UA library.

%package glib
Summary:        Glib bindings for Sofia-SIP 
Requires:       sofia-sip = %{version}-%{release}

%description glib
GLib interface to Sofia SIP User Agent library.

%package glib-devel
Summary:        Glib bindings for Sofia SIP development files
Requires:       sofia-sip-glib = %{version}-%{release}
Requires:       sofia-sip-devel = %{version}-%{release}
Requires:       pkgconfig

%description  glib-devel
Development package for Sofia SIP UA Glib library. This package
includes libraries and include files for developing glib programs
using Sofia SIP.

%package utils
Summary:        Sofia-SIP Command Line Utilities
Requires:       sofia-sip = %{version}-%{release}

%description utils
Command line utilities for the Sofia SIP UA library.

%prep
%setup0 -q -n sofia-sip-%{version}%{?work:work%{work}}


%build
./autogen.sh
%configure --disable-rpath --disable-static --without-doxygen --disable-stun
make %{?_smp_mflags}
#make doxygen

%check
#TPORT_DEBUG=9 TPORT_TEST_HOST=0.0.0.0 make check

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
find %{buildroot} -name \*.la -delete
find %{buildroot} -name \*.h.in -delete
find . -name installdox -delete

%files
%doc AUTHORS ChangeLog ChangeLog.ext-trees COPYING COPYRIGHTS
%doc README README.developers RELEASE TODO 
%{_libdir}/libsofia-sip-ua.so.*

%files devel
#%doc libsofia-sip-ua/docs/html
%dir %{_includedir}/sofia-sip-1.13
%dir %{_includedir}/sofia-sip-1.13/sofia-sip
%{_includedir}/sofia-sip-1.13/sofia-sip/*.h
%dir %{_includedir}/sofia-sip-1.13/sofia-resolv
%{_includedir}/sofia-sip-1.13/sofia-resolv/*.h
%{_libdir}/libsofia-sip-ua.so
%{_libdir}/pkgconfig/sofia-sip-ua.pc
%{_datadir}/sofia-sip

%files glib
%{_libdir}/libsofia-sip-ua-glib.so.*

%files glib-devel
#%doc libsofia-sip-ua-glib/docs/html
%{_includedir}/sofia-sip-1.13/sofia-sip/su_source.h
%{_libdir}/libsofia-sip-ua-glib.so
%{_libdir}/pkgconfig/sofia-sip-ua-glib.pc

%files utils
%{_bindir}/*
#%{_mandir}/man1/*.1*

%changelog
* Wed Aug 12 2020 FreeSWITCH Project <andrey@freeswitch.com> - 1.13.0-2
- Fix packaging with glib
* Tue Jul 28 2020 FreeSWITCH Project <andrey@freeswitch.com> - 1.13.0-1
- Initial release for the FreeSWITCH Project
