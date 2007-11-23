Name:         freeswitch-snapshot
Summary:      FreeSWITCH open source telephony platform
License:      MPL
Group:        Productivity/Telephony/Servers
Version:      6382
Release:      0
URL:          http://www.freeswitch.org/
Packager:     Peter Nixon
Vendor:       http://peternixon.net/
Source0:      %{name}-%{version}.tar.bz2
Source1:      modules.conf

#AutoReqProv:  no

BuildRequires: alsa-devel
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: curl-devel
BuildRequires: gcc-c++
BuildRequires: gnutls-devel
BuildRequires: libtool >= 1.5.14
BuildRequires: lzo-devel
BuildRequires: freeradius-client-snapshot-devel
BuildRequires: mysql-devel
BuildRequires: ncurses-devel
BuildRequires: openldap2-devel
BuildRequires: openssl-devel
BuildRequires: perl
BuildRequires: pkgconfig
BuildRequires: python-devel
BuildRequires: termcap
#BuildRequires: unixODBC-devel

%if %{?suse_version:1}0
%if 0%{?suse_version} > 910
#BuildRequires: autogen
%endif
%endif

# Fedora doesn't seem to have 'which' as part of the base system
%if %{?fedora_version:1}0
BuildRequires: which
%endif

%if 0%{?suse_version} > 800
#PreReq:       /usr/sbin/useradd /usr/sbin/groupadd
PreReq:       %insserv_prereq %fillup_prereq
%endif

BuildRoot:    %{_tmppath}/%{name}-%{version}-build

%description
FreeSWITCH is an open source telephony platform designed to facilitate the creation of voice and chat driven products scaling from a soft-phone up to a soft-switch.  It can be used as a simple switching engine, a media gateway or a media server to host IVR applications using simple scripts or XML to control the callflow. 

We support various communication technologies such as SIP, H.323, IAX2 and GoogleTalk making it easy to interface with other open source PBX systems such as sipX, OpenPBX, Bayonne, YATE or Asterisk.

We also support both wide and narrow band codecs making it an ideal solution to bridge legacy devices to the future. The voice channels and the conference bridge module all can operate at 8, 16 or 32 kilohertz and can bridge channels of different rates.

FreeSWITCH runs on several operating systems including Windows, Max OS X, Linux, BSD and Solaris on both 32 and 64 bit platforms.

Our developers are heavily involved in open source and have donated code and other resources to other telephony projects including sipX, Asterisk and OpenPBX.

%debug_package
%package devel
Summary:        Development package for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description devel
FreeSWITCH development files

%package codec-passthru-amr
Summary:        Pass-through AMR Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Conflicts:	codec-amr

%description codec-passthru-amr
Pass-through AMR Codec support for FreeSWITCH open source telephony platform

%package codec-passthru-g723_1
Summary:        Pass-through g723.1 Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Conflicts:	codec-g723_1

%description codec-passthru-g723_1
Pass-through g723.1 Codec support for FreeSWITCH open source telephony platform

%package codec-passthru-g729
Summary:        Pass-through g729 Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Conflicts:	codec-g729

%description codec-passthru-g729
Pass-through g729 Codec support for FreeSWITCH open source telephony platform

%prep
%setup -q

%build
#export CFLAGS="$RPM_OPT_FLAGS -fno-strict-aliasing -DLDAP_DEPRECATED -fPIC -DPIC"
export CFLAGS="$RPM_OPT_FLAGS -fPIC -DPIC"
%if 0%{?suse_version} > 1000 && 0%{?suse_version} < 1030
export CFLAGS="$CFLAGS -fstack-protector"
%endif

export VERBOSE=yes
export DESTDIR=$RPM_BUILD_ROOT/
export PKG_CONFIG_PATH=/usr/bin/pkg-config:$PKG_CONFIG_PATH
export ACLOCAL_FLAGS="-I /usr/share/aclocal"
./bootstrap.sh
%configure -C \
                --prefix=/opt/freeswitch \
                --sysconfdir=%{_sysconfdir} \
                --infodir=%{_infodir} \
                --mandir=%{_mandir} \
		--enable-core-libedit-support
#		--enable-core-odbc-support

#Create the version header file here
cat src/include/switch_version.h.in | sed "s/@SVN_VERSION@/%{version}/g" > src/include/switch_version.h
touch .noversion

cp %{SOURCE1} .

make

%install
make DESTDIR=$RPM_BUILD_ROOT install

# Create a log dir
mkdir -p $RPM_BUILD_ROOT/opt/freeswitch/log

#Install the library path config so the system can find the modules
mkdir -p $RPM_BUILD_ROOT/etc/ld.so.conf.d
cp build/freeswitch.ld.so.conf $RPM_BUILD_ROOT/etc/ld.so.conf.d/

install -D -m 744 build/freeswitch.init $RPM_BUILD_ROOT/etc/init.d/freeswitch
mkdir -p $RPM_BUILD_ROOT/usr/sbin
ln -sf /etc/init.d/freeswitch $RPM_BUILD_ROOT/usr/sbin/rcfreeswitch
install -D -m 744 build/freeswitch.sysconfig $RPM_BUILD_ROOT/etc/sysconfig/freeswitch

%pre
/usr/sbin/groupadd -r freeswitch 2> /dev/null || :
/usr/sbin/useradd -r -g freeswitch -s /bin/false -c "Freeswitch daemon" -d /opt/freeswitch/var freeswitch 2> /dev/null || :

%post
%{?run_ldconfig:%run_ldconfig}

%postun
%{?run_ldconfig:%run_ldconfig}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir %attr(750,root,root) /opt/freeswitch/db
%dir %attr(750,root,root) /opt/freeswitch/log
%dir %attr(750,root,root) /opt/freeswitch/log/xml_cdr
%dir %attr(750,root,root) /opt/freeswitch/htdocs
%dir %attr(750,root,root) /opt/freeswitch/scripts
#%dir %attr(750,root,root) /opt/freeswitch/grammer
%dir %attr(750,root,root) /opt/freeswitch/conf
%dir %attr(750,root,root) /opt/freeswitch/conf/autoload_configs
%dir %attr(750,root,root) /opt/freeswitch/conf/dialplan
%dir %attr(750,root,root) /opt/freeswitch/conf/directory
%dir %attr(750,root,root) /opt/freeswitch/conf/lang
%dir %attr(750,root,root) /opt/freeswitch/conf/lang/en
%dir %attr(750,root,root) /opt/freeswitch/conf/lang/en/demo
%dir %attr(750,root,root) /opt/freeswitch/conf/lang/en/vm
%dir %attr(750,root,root) /opt/freeswitch/conf/lang/de
%dir %attr(750,root,root) /opt/freeswitch/conf/lang/de/demo
%dir %attr(750,root,root) /opt/freeswitch/conf/lang/de/vm
%dir %attr(750,root,root) /opt/freeswitch/conf/lang/fr
%dir %attr(750,root,root) /opt/freeswitch/conf/lang/fr/demo
%dir %attr(750,root,root) /opt/freeswitch/conf/lang/fr/vm
%dir %attr(750,root,root) /opt/freeswitch/conf/sip_profiles
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/*.conf
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/autoload_configs/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/dialplan/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/directory/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/lang/en/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/lang/en/demo/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/lang/en/vm/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/lang/de/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/lang/de/demo/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/lang/de/vm/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/lang/fr/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/lang/fr/demo/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/lang/fr/vm/*.xml
%config(noreplace) %attr(750,root,root) /opt/freeswitch/conf/sip_profiles/*.xml
/etc/ld.so.conf.d/*
%{_bindir}/freeswitch
#/opt/freeswitch/bin/freeswitch
/etc/init.d/freeswitch
/etc/sysconfig/freeswitch
/usr/sbin/rcfreeswitch
#/opt/freeswitch/lib/*.so*
%{_libdir}/*.so*
/opt/freeswitch/mod/*.so*

%files codec-passthru-amr
/opt/freeswitch/mod/mod_amr.so*

%files codec-passthru-g723_1
/opt/freeswitch/mod/mod_g723_1.so*

%files codec-passthru-g729
/opt/freeswitch/mod/mod_g729.so*

%files devel
%defattr(-,root,root)
%{_libdir}/*.a
%{_libdir}/*.la
/opt/freeswitch/mod/*.a
/opt/freeswitch/mod/*.la
/opt/freeswitch/include/*.h

%changelog
* Tue Apr 24 2007 - peter+rpmspam@suntel.com.tr
- Added a debug package
- Split the passthrough codecs into separate packages
* Fri Mar 16 2007 - peter+rpmspam@suntel.com.tr
- Added devel package
* Thu Mar 15 2007 - peter+rpmspam@suntel.com.tr
- Initial RPM release
