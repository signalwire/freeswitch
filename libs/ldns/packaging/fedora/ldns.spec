%{?!with_python:      %global with_python      1}

%if %{with_python}
%{!?python_sitelib: %global python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}
%{!?python_sitearch: %global python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}
%endif

Summary: Lowlevel DNS(SEC) library with API
Name: ldns
Version: 1.6.9
Release: 2%{?dist}
License: BSD
Url: http://www.nlnetlabs.nl/%{name}/
Source: http://www.nlnetlabs.nl/downloads/%{name}-%{version}.tar.gz
Group: System Environment/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: libtool, autoconf, automake, gcc-c++, doxygen,
BuildRequires: perl, libpcap-devel, openssl-devel

%if %{with_python}
BuildRequires:  python-devel, swig
%endif

%description
ldns is a library with the aim to simplify DNS programing in C. All
lowlevel DNS/DNSSEC operations are supported. We also define a higher
level API which allows a programmer to (for instance) create or sign
packets.

%package devel
Summary: Development package that includes the ldns header files
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
The devel package contains the ldns library and the include files

%if %{with_python}
%package python
Summary: Python extensions for ldns
Group: Applications/System
Requires: %{name} = %{version}-%{release}

%description python
Python extensions for ldns
%endif


%prep
%setup -q 
# To built svn snapshots
#rm config.guess config.sub ltmain.sh
#aclocal
#libtoolize -c --install
#autoreconf --install

%build
%configure --disable-rpath --disable-static --with-sha2 \
%if %{with_python}
 --with-pyldns
%endif

(cd drill ; %configure --disable-rpath --disable-static --with-ldns=%{buildroot}/lib/ )
(cd examples ; %configure --disable-rpath --disable-static --with-ldns=%{buildroot}/lib/ )

make %{?_smp_mflags} 
( cd drill ; make %{?_smp_mflags} )
( cd examples ; make %{?_smp_mflags} )
make %{?_smp_mflags} doc

%install
rm -rf %{buildroot}

make DESTDIR=%{buildroot} INSTALL="%{__install} -p" install 
make DESTDIR=%{buildroot} INSTALL="%{__install} -p" install-doc

# don't package building script in doc
rm doc/doxyparse.pl
#remove doc stubs
rm -rf doc/.svn
#remove double set of man pages
rm -rf doc/man

# remove .la files
rm -rf %{buildroot}%{_libdir}/*.la %{buildroot}%{python_sitelib}/*.la
(cd drill ; make DESTDIR=%{buildroot} install)
(cd examples; make DESTDIR=%{buildroot} install)

%clean
rm -rf %{buildroot}

%files 
%defattr(-,root,root)
%{_libdir}/libldns*so.*
%{_bindir}/drill
%{_bindir}/ldnsd
#%{_bindir}/ldns-*
%{_bindir}/ldns-chaos
%{_bindir}/ldns-compare-zones
%{_bindir}/ldns-[d-z]*
%doc README LICENSE 
%{_mandir}/*/*

%files devel
%defattr(-,root,root,-)
%{_libdir}/libldns*so
%{_bindir}/ldns-config
%dir %{_includedir}/ldns
%{_includedir}/ldns/*.h
%doc doc Changelog README

%if %{with_python}
%files python
%defattr(-,root,root)
%{python_sitelib}/*
%endif

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%changelog
* Wed Mar 16 2011 Willem Toorop <willem@nlnetlabs.nl> - 1.6.9
- Upgraded to 1.6.9.

* Mon Nov 8 2010 Matthijs Mekking <matthijs@nlnetlabs.nl> - 1.6.8
- Upgraded to 1.6.8.

* Tue Aug 24 2010 Matthijs Mekking <matthijs@nlnetlabs.nl> - 1.6.7
- Upgraded to 1.6.7.

* Fri Jan 22 2010 Paul Wouters <paul@xelerance.com> - 1.6.4-2
- Fix missing _ldns.so causing ldns-python to not work
- Patch for installing ldns-python files
- Patch for rpath in ldns-python
- Don't install .a file for ldns-python

* Wed Jan 20 2010 Paul Wouters <paul@xelerance.com> - 1.6.4-1
- Upgraded to 1.6.4. 
- Added ldns-python sub package

* Fri Dec 04 2009 Paul Wouters <paul@xelerance.com> - 1.6.3-1
- Upgraded to 1.6.3, which has minor bugfixes

* Fri Nov 13 2009 Paul Wouters <paul@xelerance.com> - 1.6.2-1
- Upgraded to 1.6.2. This fixes various bugs.
  (upstream released mostly to default with sha2 for the imminent
   signed root, but we already enabled that in our builds)

* Tue Aug 25 2009 Tomas Mraz <tmraz@redhat.com> - 1.6.1-3
- rebuilt with new openssl

* Sun Aug 16 2009 Paul Wouters <paul@xelerance.com> - 1.6.1-2
- Added openssl dependancy back in, since we get more functionality
 when using openssl. Especially in 'drill'.

* Sun Aug 16 2009 Paul Wouters <paul@xelerance.com> - 1.6.1-1
- Updated to 1.6.1

* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.6.0-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Mon Jul 13 2009 Paul Wouters <paul@xelerance.com> - 1.6.0-4
- Fixed the ssl patch so it can now compile --without-ssl

* Sat Jul 11 2009 Paul Wouters <paul@xelerance.com> - 1.6.0-3
- Added patch to compile with --without-ssl
- Removed openssl dependancies
- Recompiled with --without-ssl

* Sat Jul 11 2009 Paul Wouters <paul@xelerance.com> - 1.6.0-2
- Updated to 1.6.0
- (did not yet compile with --without-ssl due to compile failures)

* Fri Jul 10 2009 Paul Wouters <paul@xelerance.com> - 1.6.0-1
- Updated to 1.6.0
- Compile without openssl

* Thu Apr 16 2009 Paul Wouters <paul@xelerance.com> - 1.5.1-4
- Memory management bug when generating a sha256 key, see:
  https://bugzilla.redhat.com/show_bug.cgi?id=493953

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.5.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Mon Feb 10 2009 Paul Wouters <paul@xelerance.com> - 1.5.1-1
- Updated to new version, 1.5.0 had a bug preventing
  zone signing.

* Mon Feb  9 2009 Paul Wouters <paul@xelerance.com> - 1.5.0-1
- Updated to new version

* Thu Feb 05 2009 Adam Tkac <atkac redhat com> - 1.4.0-3
- fixed configure flags

* Sat Jan 17 2009 Tomas Mraz <tmraz@redhat.com> - 1.4.0-2
- rebuild with new openssl

* Fri Nov  7 2008 Paul Wouters <paul@xelerance.com> - 1.4.0-1
- Updated to 1.4.0

* Wed May 28 2008 Paul Wouters <paul@xelerance.com> - 1.3.0-3
- enable SHA2 functionality

* Wed May 28 2008 Paul Wouters <paul@xelerance.com> - 1.3.0-2
- re-tag (don't do builds while renaming local repo dirs)

* Wed May 28 2008 Paul Wouters <paul@xelerance.com> - 1.3.0-1
- Updated to latest release

* Tue Feb 19 2008 Fedora Release Engineering <rel-eng@fedoraproject.org> - 1.2.2-3
- Autorebuild for GCC 4.3

* Wed Dec  5 2007 Paul Wouters <paul@xelerance.com> - 1.2.2-2
- Rebuild for new libcrypto

* Thu Nov 29 2007 Paul Wouters <paul@xelerance.com> - 1.2.2-1
- Upgraded to 1.2.2. Removed no longer needed race workaround

* Tue Nov 13 2007 Paul Wouters <paul@xelerance.com> - 1.2.1-4
- Try to fix racing ln -s statements in parallel builds

* Fri Nov  9 2007 Paul Wouters <paul@xelerance.com> - 1.2.1-3
- Added patch for ldns-read-zone that does not put @. in RRDATA

* Fri Oct 19 2007 Paul Wouters <paul@xelerance.com> - 1.2.1-2
- Use install -p to work around multilib conflicts for .h files

* Wed Oct 10 2007 Paul Wouters <paul@xelerance.com> - 1.2.1-1
- Updated to 1.2.1
- Removed patches that got moved into upstream

* Wed Aug  8 2007 Paul Wouters <paul@xelerance.com> 1.2.0-11
- Patch for ldns-key2ds to write to stdout
- Again remove extra set of man pages from doc
- own /usr/include/ldns (bug 233858)

* Wed Aug  8 2007 Paul Wouters <paul@xelerance.com> 1.2.0-10
- Added sha256 DS record patch to ldns-key2ds
- Minor tweaks for proper doc/man page installation.
- Workaround for parallel builds

* Mon Aug  6 2007 Paul Wouters <paul@xelerance.com> 1.2.0-2
- Own the /usr/include/ldns directory (bug #233858)
- Removed obsoleted patch
- Remove files form previous libtool run accidentally packages by upstream

* Mon Sep 11 2006 Paul Wouters <paul@xelerance.com> 1.0.1-4
- Commented out 1.1.0 make targets, put make 1.0.1 targets.

* Mon Sep 11 2006 Paul Wouters <paul@xelerance.com> 1.0.1-3
- Fixed changelog typo in date
- Rebuild requested for PT_GNU_HASH support from gcc
- Did not upgrade to 1.1.0 due to compile issues on x86_64

* Fri Jan  6 2006 Paul Wouters <paul@xelerance.com> 1.0.1-1
- Upgraded to 1.0.1. Removed temporary clean hack from spec file.

* Sun Dec 18 2005 Paul Wouters <paul@xelerance.com> 1.0.0-8
- Cannot use make clean because there are no Makefiles. Use hardcoded rm.

* Sun Dec 18 2005 Paul Wouters <paul@xelerance.com> 1.0.0-7
- Patched 'make clean' target to get rid of object files shipped with 1.0.0

* Sun Dec 13 2005 Paul Wouters <paul@xelerance.com> 1.0.0-6
- added a make clean for 2.3.3 since .o files were left behind upstream,
  causing failure on ppc platform

* Sun Dec 11 2005 Tom "spot" Callaway <tcallawa@redhat.com> 1.0.0-5
- minor cleanups

* Wed Oct  5 2005 Paul Wouters <paul@xelerance.com> 0.70_1205
- reworked for svn version

* Sun Sep 25 2005 Paul Wouters <paul@xelerance.com> - 0.70
- Initial version
