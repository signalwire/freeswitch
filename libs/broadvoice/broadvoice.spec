Summary:    broadvoice - a library for the BroadVoice 16 and 32 speech codecs
Name:       broadvoice
Version:    0.0.1
Release:    1
License:    LGPL2.1
Group:      System Environment/Libraries
URL:        http://www.soft-switch.org/broadvoice
BuildRoot:  %{_tmppath}/%{name}-%{version}-root
Source:     http://www.soft-switch.org/downloads/codecs/broadvoice-0.0.1.tar.gz
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: audiofile-devel
BuildRequires: doxygen

%description
broadvoice is a library for the BroadVoice 16 and 32 speech codecs.

%package devel
Summary:    BroadVoice development files
Group:      Development/Libraries
Requires:   libbroadvoice = %{version}
PreReq:     /sbin/install-info

%description devel
libbroadvoice development files.

%prep
%setup -q

%build
%configure --enable-doc --disable-static --disable-rpath
make

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
rm %{buildroot}%{_libdir}/libbroadvoice.la

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc ChangeLog AUTHORS COPYING NEWS README 

%{_libdir}/libbroadvoice.so.*

%files devel
%defattr(-,root,root,-)
%doc doc/api
%{_includedir}/broadvoice.h
%{_includedir}/broadvoice
%{_libdir}/libbroadvoice.so
%{_libdir}/pkgconfig/broadvoice.pc

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%changelog
* Sat Nov 15 2009 Steve Underwood <steveu@coppice.org> 0.0.1
- First pass
