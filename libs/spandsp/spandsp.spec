Summary:    A DSP library for telephony.
Name:       spandsp
Version:    0.0.5
Release:    1
License:    GPL
Group:      System Environment/Libraries
URL:        http://www.soft-switch.org/spandsp
BuildRoot:  %{_tmppath}/%{name}-%{version}-root
Source:     http://www.soft-switch.org/downloads/spandsp/spandsp-0.0.5.tar.gz
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Docdir:     %{_prefix}/doc

BuildRequires: libtiff-devel
BuildRequires: audiofile-devel
BuildRequires: doxygen

%description
SpanDSP is a library of DSP functions for telephony, in the 8000
sample per second world of E1s, T1s, and higher order PCM channels. It
contains low level functions, such as basic filters. It also contains
higher level functions, such as cadenced supervisory tone detection,
and a complete software FAX machine. The software has been designed to
avoid intellectual property issues, using mature techniques where all
relevant patents have expired. See the file DueDiligence for important
information about these intellectual property issues.

%package devel
Summary:    SpanDSP development files
Group:      Development/Libraries
Requires:   spandsp = %{version}
Requires:   libtiff-devel
PreReq:     /sbin/install-info

%description devel
SpanDSP development files.

%prep
%setup -q

%build
%configure --enable-doc --disable-static --disable-rpath
make

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
rm %{buildroot}%{_libdir}/libspandsp.la

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc DueDiligence ChangeLog AUTHORS COPYING NEWS README 

%{_libdir}/libspandsp.so.*

%{_datadir}/spandsp

%files devel
%defattr(-,root,root,-)
%doc doc/api
%{_includedir}/spandsp.h
%{_includedir}/spandsp
%{_libdir}/libspandsp.so

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%changelog
* Mon 23 Jun 2008 Steve Underwood <steveu@coppice.org> 0.0.5-1
- Cleared out the dependency on libxml2

* Sun Dec 31 2006 Steve Underwood <steveu@coppice.org> 0.0.3-1
- Preparing for 0.0.3 release

* Sat Oct 16 2004 Steve Underwood <steveu@coppice.org> 0.0.2-1
- Preparing for 0.0.2 release

* Thu Apr 15 2004 Steve Underwood <steveu@coppice.org> 0.0.1-1
- Initial version
