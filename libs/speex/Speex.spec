%define name     speex
%define ver      1.2rc1
%define rel      1

Summary: An open-source, patent-free speech codec
Name: %name
Version: %ver
Release: %rel
License: BSD
Group: Application/Devel
Source: http://www.speex.org/download/%{name}-%{ver}.tar.gz
URL: http://www.speex.org/
Vendor: Speex
Packager: Jean-Marc Valin (jean-marc.valin@usherbrooke.ca)
BuildRoot: /var/tmp/%{name}-build-root
Docdir: /usr/share/doc

%description
Speex is a patent-free audio codec designed especially for voice (unlike 
Vorbis which targets general audio) signals and providing good narrowband 
and wideband quality. This project aims to be complementary to the Vorbis
codec.

%package devel
Summary:	Speex development files
Group:		Development/Libraries
Requires:	%{name} = %{version}

%description devel
Speex development files.

%changelog
* Thu Oct 03 2002 Jean-Marc Valin 
- Added devel package inspired from PLD spec file

* Tue Jul 30 2002 Fredrik Rambris <boost@users.sourceforge.net> 0.5.2
- Added buildroot and docdir and ldconfig. Makes it builadble by non-roots
  and also doesn't write to actual library paths when building.

%prep
%setup

%build
export CFLAGS='-O3'
./configure --prefix=/usr --enable-shared --enable-static
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(644,root,root,755)
%doc COPYING AUTHORS ChangeLog NEWS README
%doc doc/manual.pdf
/usr/share/man/man1/speexenc.1*
/usr/share/man/man1/speexdec.1*
%attr(755,root,root) %{_bindir}/speex*
%attr(755,root,root) %{_libdir}/libspeex*.so*

%files devel
%defattr(644,root,root,755)
%attr(755,root,root) %{_libdir}/libspeex*.la
%{_includedir}/speex/speex*.h
/usr/share/aclocal/speex.m4
%{_libdir}/pkgconfig/speex.pc
%{_libdir}/pkgconfig/speexdsp.pc
%{_libdir}/libspeex*.a
