%define _prefix   /opt/freeswitch
%define prefix    %{_prefix}

Name:         freeswitch
Summary:      FreeSWITCH open source telephony platform
License:      MPL
Group:        Productivity/Telephony/Servers
Version:      1.0.rc1
Release:      1
URL:          http://www.freeswitch.org/
Packager:     Michal Bielicki
Vendor:       http://www.voiceworks.pl/
Source0:      %{name}-%{version}.tar.bz2
Prefix:       %{prefix}

#AutoReqProv:  no

%if 0%{?suse_version} > 100
BuildRequires: alsa-devel
#BuildRequires: openldap2-devel
BuildRequires: lzo-devel
%else
BuildRequires: alsa-lib-devel
BuildRequires: openldap-devel
%endif
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: curl-devel
BuildRequires: gcc-c++
BuildRequires: gnutls-devel
BuildRequires: libtool >= 1.5.14
BuildRequires: ncurses-devel
BuildRequires: openssl-devel
BuildRequires: perl
BuildRequires: pkgconfig
BuildRequires: termcap
BuildRequires: unixODBC-devel

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

BuildRoot:    %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
FreeSWITCH is an open source telephony platform designed to facilitate the creation of voice 
and chat driven products scaling from a soft-phone up to a soft-switch.  It can be used as a 
simple switching engine, a media gateway or a media server to host IVR applications using 
simple scripts or XML to control the callflow. 

We support various communication technologies such as SIP, H.323, IAX2 and GoogleTalk making 
it easy to interface with other open source PBX systems such as sipX, OpenPBX, Bayonne, YATE or Asterisk.

We also support both wide and narrow band codecs making it an ideal solution to bridge legacy 
devices to the future. The voice channels and the conference bridge module all can operate 
at 8, 16 or 32 kilohertz and can bridge channels of different rates.

FreeSWITCH runs on several operating systems including Windows, Max OS X, Linux, BSD and Solaris 
on both 32 and 64 bit platforms.

Our developers are heavily involved in open source and have donated code and other resources to 
other telephony projects including sipXecs, OpenSER, Asterisk, CodeWeaver and OpenPBX.

#%if 0%{?suse_version} > 100
#%debug_package
#%endif

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

%package spidermonkey
Summary:	JavaScript support for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	 %{name} = %{version}-%{release}

%description spidermonkey

%package lang-en
Summary:	Provides english language dependand modules and sounds for the FreeSwitch Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-en
English language phrases module and directory structure for say module and voicemail

#%package lang-it
#Summary:        Provides italian language dependand modules and sounds for the FreeSwitch Open Source telephone platform.
#Group:          System/Libraries
#Requires:        %{name} = %{version}-%{release}

#%description lang-it
#Italian language phrases module and directory structure for say module and voicemail

#%package lang-es
#Summary:        Provides spanish language dependand modules and sounds for the FreeSwitch Open Source telephone platform.
#Group:          System/Libraries
#Requires:        %{name} = %{version}-%{release}

#%description lang-es
#Spanish language phrases module and directory structure for say module and voicemail

%package lang-de
Summary:        Provides german language dependand modules and sounds for the FreeSwitch Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-de
German language phrases module and directory structure for say module and voicemail

#%package lang-nl
#Summary:        Provides dutch language dependand modules and sounds for the FreeSwitch Open Source telephone platform.
#Group:          System/Libraries
#Requires:        %{name} = %{version}-%{release}

#%description lang-nl
#Dutch language phrases module and directory structure for say module and voicemail

%package lang-fr
Summary:        Provides french language dependand modules and sounds for the FreeSwitch Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-fr
French language phrases module and directory structure for say module and voicemail

%prep
%setup -q

%build
%if 0%{?suse_version} > 1000 && 0%{?suse_version} < 1030
export CFLAGS="$CFLAGS -fstack-protector"
%endif
PASSTHRU_CODEC_MODULES="codecs/mod_g729 codecs/mod_g723_1 codecs/mod_amr"
SPIDERMONKEY_MODULES="languages/mod_spidermonkey languages/mod_spidermonkey_curl languages/mod_spidermonkey_core_db languages/mod_spidermonkey_odbc languages/mod_spidermonkey_socket languages/mod_spidermonkey_teletone"
APPLICATIONS_MODULES="applications/mod_commands applications/mod_conference applications/mod_dptools applications/mod_enum applications/mod_esf applications/mod_expr applications/mod_fifo applications/mod_limit applications/mod_rss applications/mod_voicemail"
ASR_TTS_MODULES="asr_tts/mod_openmrcp"
CODECS_MODULES="codecs/mod_g711 codecs/mod_g722 codecs/mod_g726 codecs/mod_gsm codecs/mod_ilbc codecs/mod_h26x codecs/mod_l16 codecs/mod_speex"
DIALPLANS_MODULES="dialplans/mod_dialplan_asterisk dialplans/mod_dialplan_directory dialplans/mod_dialplan_xml"
DIRECTORIES_MODULES=
DOTNET_MODULES=
ENDPOINTS_MODULES="endpoints/mod_dingaling endpoints/mod_iax endpoints/mod_portaudio endpoints/mod_sofia endpoints/mod_woomera ../../libs/openzap/mod_openzap"
EVENT_HANDLERS_MODULES="event_handlers/mod_event_multicast event_handlers/mod_event_socket event_handlers/mod_cdr_csv"
FORMATS_MODULES="formats/mod_local_stream formats/mod_native_file formats/mod_sndfile"
LANGUAGES_MODULES=
LOGGERS_MODULES="loggers/mod_console loggers/mod_logfile loggers/mod_syslog"
SAY_MODULES="say/mod_say_en say/mod_say_fr say/mod_say_de"
TIMERS_MODULES=
DISABLED_MODULES="applications/mod_sountouch directories/mod_ldap languages/mod_java languages/mod_python languages/mod_spidermonkey_skel ast_tts/mod_cepstral asr_tts/mod_lumenvox endpoints/mod_wanpipe event_handlers/mod_event_test event_handlers/mod_radius_cdr event_handlers/mod_zeroconf formats/mod_shout say/mod_say_it say/mod_say_es say/mod_say_nl"
XML_INT_MODULES="xml_int/mod_xml_rpc  xml_int/mod_xml_curl xml_int/mod_xml_cdr"
MYMODULES="$PASSTHRU_CODEC_MODULES $SPIDERMONKEY_MODULES $APPLICATIONS_MODULES $ASR_TTS_MODULES $CODECS_MODULES $DIALPLANS_MODULES $DIRECTORIES_MODULES $DOTNET_MODULES $ENDPOINTS_MODULES $EVENT_HANDLERS_MODULES $FORMATS_MODULES $LANGUAGES_MODULES $LOGGERS_MODULES $SAY_MODULES $TIMERS_MODULES $XML_INT_MODULES"

export MODULES=$MYMODULES
touch modules.conf
for i in $MODULES; do echo $i >> modules.conf; done
export VERBOSE=yes
export DESTDIR=$RPM_BUILD_ROOT/
export PKG_CONFIG_PATH=/usr/bin/pkg-config:$PKG_CONFIG_PATH
export ACLOCAL_FLAGS="-I /usr/share/aclocal"

./bootstrap.sh
%configure -C \
                --prefix=%{prefix} \
		--bindir=%{prefix}/bin \
		--libdir=%{prefix}/lib \
                --sysconfdir=%{_sysconfdir} \
                --infodir=%{_infodir} \
                --mandir=%{_mandir} \
		--enable-core-libedit-support \
		--enable-core-odbc-support \
%ifos linux
%if 0%{?fedora_version} >= 8
%else
                --with-libcurl \
%endif
%endif
                --with-openssl


#Create the version header file here
cat src/include/switch_version.h.in | sed "s/@SVN_VERSION@/%{version}/g" > src/include/switch_version.h
touch .noversion

%{__make}

%install
%{__make} DESTDIR=$RPM_BUILD_ROOT install

# Create a log dir
%{__mkdir} -p $RPM_BUILD_ROOT%{prefix}/log

%ifos linux
#Install the library path config so the system can find the modules
%{__mkdir} -p $RPM_BUILD_ROOT/etc/ld.so.conf.d
%{__cp} build/freeswitch.ld.so.conf $RPM_BUILD_ROOT/etc/ld.so.conf.d/
# Install init files
# On SuSE:
%if 0%{?suse_version} > 100
%{__install} -D -m 744 build/freeswitch.init.suse $RPM_BUILD_ROOT/etc/init.d/freeswitch
%else
# On RedHat like
%{__install} -D -m 744 build/freeswitch.init.redhat $RPM_BUILD_ROOT/etc/init.d/freeswitch
%endif
# On SuSE make /usr/sbin/rcfreeswitch a link to /etc/init.d/freeswitch
%if 0%{?suse_version} > 100
%{__mkdir} -p $RPM_BUILD_ROOT/usr/sbin
%{__ln_s} -f /etc/init.d/freeswitch $RPM_BUILD_ROOT/usr/sbin/rcfreeswitch
%endif
# Add the sysconfiguration file
%{__install} -D -m 744 build/freeswitch.sysconfig $RPM_BUILD_ROOT/etc/sysconfig/freeswitch
# Add monit file
%{__install} -D -m 644 build/freeswitch.monitrc $RPM_BUILD_ROOT/etc/monit.d/freeswitch.monitrc
%endif


# Add a freeswitch user with group daemon
%pre
/usr/sbin/useradd -r -g daemon -s /bin/false -c "The FreeSWITCH Open Source Voice Platform" -d %{prefix}/var freeswitch 2> /dev/null || :

%post
%{?run_ldconfig:%run_ldconfig}
# Make FHS2.0 happy
%{__mkdir} -p /etc/opt
%{__ln_s} -f %{prefix}/conf /etc%{prefix}

%postun
%{?run_ldconfig:%run_ldconfig}
%{__rm} -rf %{prefix}
userdel freeswitch

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%files
%defattr(-,freeswitch,daemon)
%ifos linux
%dir %attr(0750, freeswitch, daemon) /etc/monit.d
%endif
%dir %attr(0750, freeswitch, daemon) %{prefix}/db
%dir %attr(0750, freeswitch, daemon) %{prefix}/log
%dir %attr(0750, freeswitch, daemon) %{prefix}/log/xml_cdr
%dir %attr(0750, freeswitch, daemon) %{prefix}/htdocs
%dir %attr(0750, freeswitch, daemon) %{prefix}/scripts
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/autoload_configs
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/dialplan
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/directory
%dir %attr(0750, freeswitch. daemon) %{prefix}/conf/directory/default
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/mrcp_profiles
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles/nat
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/dialplan/extensions
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles/default
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles/outbound
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/jingle_profiles
%ifos linux
%config(noreplace) %attr(0644, freeswitch, daemon) /etc/monit.d/freeswitch.monitrc
%endif
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/mime.types
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.tpl
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.ttml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/alsa.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/conference.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/console.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/dialplan_directory.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/dingaling.conf.xml 
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/enum.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/event_multicast.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/event_socket.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/cdr_csv.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/iax.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/ivr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/java.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/limit.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/local_stream.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/logfile.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/modules.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/openmrcp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/portaudio.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/post_load_modules.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/rss.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/sofia.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/switch.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/syslog.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/voicemail.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/wanpipe.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/woomera.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/xml_cdr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/xml_curl.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/xml_rpc.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/zeroconf.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/dialplan/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/directory/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/directory/default/*
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/mrcp_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/default/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/outbound/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/nat/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/jingle_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/htdocs/*
%ifos linux
/etc/ld.so.conf.d/*
/etc/init.d/freeswitch
/etc/sysconfig/freeswitch
%if 0%{?suse_version} > 100
/usr/sbin/rcfreeswitch
%endif
%endif
%{prefix}/bin/*
%{prefix}/lib/libfreeswitch*.so*
%{prefix}/mod/mod_console.so*
%{prefix}/mod/mod_logfile.so*
%{prefix}/mod/mod_syslog.so*
%{prefix}/mod/mod_commands.so*
%{prefix}/mod/mod_conference.so*
%{prefix}/mod/mod_dptools.so*
%{prefix}/mod/mod_enum.so*
%{prefix}/mod/mod_esf.so*
%{prefix}/mod/mod_expr.so*
%{prefix}/mod/mod_fifo.so*
%{prefix}/mod/mod_limit.so*
%{prefix}/mod/mod_rss.so*
%{prefix}/mod/mod_voicemail.so*
%{prefix}/mod/mod_openmrcp.so*
%{prefix}/mod/mod_g711.so*
%{prefix}/mod/mod_g722.so*
%{prefix}/mod/mod_g726.so*
%{prefix}/mod/mod_gsm.so*
%{prefix}/mod/mod_ilbc.so* 
%{prefix}/mod/mod_h26x.so*
%{prefix}/mod/mod_l16.so* 
%{prefix}/mod/mod_speex.so* 
%{prefix}/mod/mod_dialplan_directory.so* 
%{prefix}/mod/mod_dialplan_xml.so* 
%{prefix}/mod/mod_dialplan_asterisk.so* 
%{prefix}/mod/mod_dingaling.so* 
%{prefix}/mod/mod_iax.so* 
%{prefix}/mod/mod_portaudio.so* 
%{prefix}/mod/mod_sofia.so* 
%{prefix}/mod/mod_woomera.so* 
%{prefix}/mod/mod_openzap.so* 
%{prefix}/mod/mod_cdr_csv.so*
%{prefix}/mod/mod_event_multicast.so* 
%{prefix}/mod/mod_event_socket.so* 
%{prefix}/mod/mod_native_file.so* 
%{prefix}/mod/mod_sndfile.so* 
%{prefix}/mod/mod_local_stream.so* 
%{prefix}/mod/mod_xml_rpc.so* 
%{prefix}/mod/mod_xml_curl.so* 
%{prefix}/mod/mod_xml_cdr.so* 

%files codec-passthru-amr
%defattr(-,freeswitch,daemon)
%{prefix}/mod/mod_amr.so*

%files codec-passthru-g723_1
%defattr(-,freeswitch,daemon)
%{prefix}/mod/mod_g723_1.so*

%files codec-passthru-g729
%defattr(-,freeswitch,daemon)
%{prefix}/mod/mod_g729.so*

%files spidermonkey
%defattr(-,freeswitch,daemon)
%{prefix}/mod/mod_spidermonkey*.so*
%{prefix}/lib/libjs.so*
%{prefix}/lib/libnspr4.so
%{prefix}/lib/libplds4.so
%{prefix}/lib/libplc4.so
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/autoload_configs
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/spidermonkey.conf.xml

%files devel
%defattr(-, freeswitch, daemon)
%{prefix}/lib/*.a
%{prefix}/lib/*.la
%{prefix}/mod/*.a
%{prefix}/mod/*.la
%{prefix}/include/*.h

%files lang-en
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/en
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/en/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/en/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/en/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/en/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/en/vm/*.xml
%{prefix}/mod/mod_say_en.so*

#%files lang-it
#%dir %attr(750,freeswitch,daemon) %{prefix}/conf/lang/it
#%dir %attr(750,freeswitch,daemon) %{prefix}/conf/lang/it/demo
#%dir %attr(750,freeswitch,daemon) %{prefix}/conf/lang/it/vm
#%config(noreplace) %attr(640,freeswitch,daemon) %{prefix}/conf/lang/it/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) %{prefix}/conf/lang/it/demo/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) %{prefix}/conf/lang/it/vm/*.xml
#%{prefix}/mod/mod_say_it.so*

#%files lang-es
#%dir %attr(750,freeswitch,daemon) %{prefix}/conf/lang/es
#%dir %attr(750,freeswitch,daemon) %{prefix}/conf/lang/es/demo
#%dir %attr(750,freeswitch,daemon) %{prefix}/conf/lang/es/vm
#%config(noreplace) %attr(640,freeswitch,daemon) %{prefix}/conf/lang/es/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) %{prefix}/conf/lang/es/demo/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) %{prefix}/conf/lang/es/vm/*.xml
#%{prefix}/mod/mod_say_es.so*


%files lang-de
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/de
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/de/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/de/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/de/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/de/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/de/vm/*.xml
%{prefix}/mod/mod_say_de.so*

#%files lang-nl
#%dir %attr(750,freeswitch,daemon) %{prefix}/conf/lang/nl
#%dir %attr(750,freeswitch,daemon) %{prefix}/conf/lang/nl/demo
#%dir %attr(750,freeswitch,daemon) %{prefix}/conf/lang/nl/vm
#%config(noreplace) %attr(640,freeswitch,daemon) %{prefix}/conf/lang/nl/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) %{prefix}/conf/lang/nl/demo/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) %{prefix}/conf/lang/nl/vm/*.xml
#%{prefix}/mod/mod_say_nl.so*

%files lang-fr
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/fr
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/fr/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/fr/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/vm/*.xml
%{prefix}/mod/mod_say_fr.so*

%changelog
* Mon Feb 04 2008 - michal.bielicki@voiceworks.pl
- More fixes to specfile
- First go at SFE files
* Sun Feb 03 2008 - michal.bielicki@voiceworks.pl
- abstraction of prefix
- more wrong stuff deleted
- abstraction of mkdir, mv, rm, install etc into macros
* Fri Jan 18 2008 - michal.bielicki@voiceworks.pl
- fixes, fixes and more fixes in preparation for rc1
* Thu Dec 5 2007 - michal.bielicki@voiceworks.pl
- put in detail configfiles in to split of spidermonkey configs
- created link from /opt/freesxwitch/conf to /etc%{prefix}
* Thu Nov 29 2007 - michal.bielicki@voiceworks.pl
- Added ifdefs for susealities
- Added specifics for centos/redhat
- Added specifics for fedora
- Preparing to use it for adding it to SFE packaging for solaris
- Added odbc stuff back in
- made curl default
- Separate package for mod_spidermonkey
- got rid of modules.conf and stuffed everything in MODULES env var
- got rid of handmade Cflags peter added ;)
- fixed bin and libpaths
- fixed locationof nspr and js libs
- fixed odbc requirements
- added all buildable modules
- added redhat style init file
- splitted off language dependant stuff into separate language files
- disable non complete language modules
* Tue Apr 24 2007 - peter+rpmspam@suntel.com.tr
- Added a debug package
- Split the passthrough codecs into separate packages
* Fri Mar 16 2007 - peter+rpmspam@suntel.com.tr
- Added devel package
* Thu Mar 15 2007 - peter+rpmspam@suntel.com.tr
- Initial RPM release
