Summary:    iLBC is a library for the iLBC low bit rate speech codec.
Name:       ilbc
Version:    0.0.1
Release:    1
License:    Global IP Sound iLBC Public License, v2.0
Group:      System Environment/Libraries
URL:        http://www.soft-switch.org/voipcodecs
BuildRoot:  %{_tmppath}/%{name}-%{version}-root
Source:     http://www.soft-switch.org/downloads/voipcodecs/ilbc-0.0.1.tar.gz
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Docdir:     %{_prefix}/doc

BuildRequires: audiofile-devel
BuildRequires: doxygen

%description
iLBC is a library for the iLBC low bit rate speech codec.

%package devel
Summary:    iLBC development files
Group:      Development/Libraries
Requires:   ilbc = %{version}
PreReq:     /sbin/install-info

%description devel
iLBC development files.

%prep
%setup -q

%build
%configure --enable-doc --disable-static --disable-rpath
make

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
rm %{buildroot}%{_libdir}/libilbc.la

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc ChangeLog AUTHORS COPYING NEWS README 

%{_libdir}/libilbc.so.*

%{_datadir}/ilbc

%files devel
%defattr(-,root,root,-)
%doc doc/api
%{_includedir}/ilbc.h
%{_includedir}/ilbc
%{_libdir}/libilbc.so

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%changelog
* Thu Feb  7 2008 Steve Underwood <steveu@coppice.org> 0.0.1
- First pass
