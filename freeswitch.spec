Name:         freeswitch
Summary:      FreeSWITCH open source telephony platform
License:      MPL
Group:        Productivity/Telephony/Servers
Version:      1.0.beta3
Release:      1
URL:          http://www.freeswitch.org/
Packager:     Michal Bielicki
Vendor:       http://www.voiceworks.pl/
Source0:      %{name}-%{version}.tar.bz2

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

BuildRoot:    %{_tmppath}/%{name}-%{version}-build

%description
FreeSWITCH is an open source telephony platform designed to facilitate the creation of voice and chat driven products scaling from a soft-phone up to a soft-switch.  It can be used as a simple switching engine, a media gateway or a media server to host IVR applications using simple scripts or XML to control the callflow. 

We support various communication technologies such as SIP, H.323, IAX2 and GoogleTalk making it easy to interface with other open source PBX systems such as sipX, OpenPBX, Bayonne, YATE or Asterisk.

We also support both wide and narrow band codecs making it an ideal solution to bridge legacy devices to the future. The voice channels and the conference bridge module all can operate at 8, 16 or 32 kilohertz and can bridge channels of different rates.

FreeSWITCH runs on several operating systems including Windows, Max OS X, Linux, BSD and Solaris on both 32 and 64 bit platforms.

Our developers are heavily involved in open source and have donated code and other resources to other telephony projects including sipX, Asterisk and OpenPBX.

%if 0%{?suse_version} > 100
%debug_package
%endif

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
#export CFLAGS="$RPM_OPT_FLAGS -fno-strict-aliasing -DLDAP_DEPRECATED -fPIC -DPIC"
#export CFLAGS="$RPM_OPT_FLAGS -fPIC -DPIC"
%if 0%{?suse_version} > 1000 && 0%{?suse_version} < 1030
export CFLAGS="$CFLAGS -fstack-protector"
%endif
PASSTHRU_CODEC_MODULES="codecs/mod_g729 codecs/mod_g723_1 codecs/mod_amr"
SPIDERMONKEY_MODULES="languages/mod_spidermonkey languages/mod_spidermonkey_core_db languages/mod_spidermonkey_odbc languages/mod_spidermonkey_socket languages/mod_spidermonkey_teletone"
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
SAY_MODULES="say/mod_say_de say/mod_say_en say/mod_say_fr"
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
                --prefix=/opt/freeswitch \
		--bindir=/opt/freeswitch/bin \
		--libdir=/opt/freeswitch/lib \
                --sysconfdir=%{_sysconfdir} \
                --infodir=%{_infodir} \
                --mandir=%{_mandir} \
		--enable-core-libedit-support \
		--enable-core-odbc-support \
                --with-openssl \
                --with-libcurl 

#Create the version header file here
cat src/include/switch_version.h.in | sed "s/@SVN_VERSION@/%{version}/g" > src/include/switch_version.h
touch .noversion

make

%install
make DESTDIR=$RPM_BUILD_ROOT install

# Create a log dir
mkdir -p $RPM_BUILD_ROOT/opt/freeswitch/log

#Install the library path config so the system can find the modules
mkdir -p $RPM_BUILD_ROOT/etc/ld.so.conf.d
cp build/freeswitch.ld.so.conf $RPM_BUILD_ROOT/etc/ld.so.conf.d/

# Install init files
# On SuSE:
%if 0%{?suse_version} > 100
install -D -m 744 build/freeswitch.init.suse $RPM_BUILD_ROOT/etc/init.d/freeswitch
%else
# On RedHat like
install -D -m 744 build/freeswitch.init.redhat $RPM_BUILD_ROOT/etc/init.d/freeswitch
%endif

# On SuSE make /usr/sbin/rcfreeswitch a link to /etc/init.d/freeswitch
%if 0%{?suse_version} > 100
mkdir -p $RPM_BUILD_ROOT/usr/sbin
ln -sf /etc/init.d/freeswitch $RPM_BUILD_ROOT/usr/sbin/rcfreeswitch
%endif

# Add the sysconfiguration file
install -D -m 744 build/freeswitch.sysconfig $RPM_BUILD_ROOT/etc/sysconfig/freeswitch

# Add monit file
install -D -m 644 build/freeswitch.monitrc $RPM_BUILD_ROOT/etc/monit.d/freeswitch.monitrc

# Add a freeswitch user with group daemon
%pre
/usr/sbin/useradd -r -g daemon -s /bin/false -c "The FreeSWITCH Open Source Voice Platform" -d /opt/freeswitch/var freeswitch 2> /dev/null || :

%post
%{?run_ldconfig:%run_ldconfig}
# Make FHS2.0 happy
mkdir -p /etc/opt
ln -sf /opt/freeswitch/conf /etc/opt/freeswitch

%postun
%{?run_ldconfig:%run_ldconfig}
rm -rf /opt/freeswitch
userdel freeswitch

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,freeswitch,daemon)
%dir %attr(750,freeswitch,daemon) /etc/monit.d
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/db
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/log
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/log/xml_cdr
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/htdocs
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/scripts
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/grammer
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/dialplan
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/directory
%dir %attr(750,freeswitch.daemon) /opt/freeswitch/conf/directory/default
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/sip_profiles
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/jingle_profiles
%config(noreplace) %attr(644,freeswitch,daemon) /etc/monit.d/freeswitch.monitrc
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/mime.types
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/*.tpl
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/*.conf
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/alsa.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/cdr.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/conference.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/console.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/dialplan_directory.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/dingaling.conf.xml 
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/enum.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/event_multicast.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/event_socket.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/cdr_csv.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/iax.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/ivr.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/java.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/limit.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/local_stream.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/logfile.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/modules.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/openmrcp.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/portaudio.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/post_load_modules.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/rss.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/sofia.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/switch.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/syslog.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/voicemail.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/wanpipe.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/woomera.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/xml_cdr.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/xml_curl.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/xml_rpc.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/zeroconf.conf.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/dialplan/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/directory/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/directory/default/*
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/sip_profiles/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/jingle_profiles/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/htdocs/*
/etc/ld.so.conf.d/*
/opt/freeswitch/bin/freeswitch
/etc/init.d/freeswitch
/etc/sysconfig/freeswitch
%if 0%{?suse_version} > 100
/usr/sbin/rcfreeswitch
%endif
/opt/freeswitch/lib/libfreeswitch*.so*
/opt/freeswitch/mod/mod_console.so*
/opt/freeswitch/mod/mod_logfile.so*
/opt/freeswitch/mod/mod_syslog.so*
/opt/freeswitch/mod/mod_commands.so*
/opt/freeswitch/mod/mod_conference.so*
/opt/freeswitch/mod/mod_dptools.so*
/opt/freeswitch/mod/mod_enum.so*
/opt/freeswitch/mod/mod_esf.so*
/opt/freeswitch/mod/mod_expr.so*
/opt/freeswitch/mod/mod_fifo.so*
/opt/freeswitch/mod/mod_limit.so*
/opt/freeswitch/mod/mod_rss.so*
#/opt/freeswitch/mod/mod_soundtouch.so*
/opt/freeswitch/mod/mod_voicemail.so*
/opt/freeswitch/mod/mod_openmrcp.so*
/opt/freeswitch/mod/mod_g711.so*
/opt/freeswitch/mod/mod_g722.so*
/opt/freeswitch/mod/mod_g726.so*
/opt/freeswitch/mod/mod_gsm.so*
/opt/freeswitch/mod/mod_ilbc.so* 
/opt/freeswitch/mod/mod_h26x.so*
/opt/freeswitch/mod/mod_l16.so* 
/opt/freeswitch/mod/mod_speex.so* 
/opt/freeswitch/mod/mod_dialplan_directory.so* 
/opt/freeswitch/mod/mod_dialplan_xml.so* 
/opt/freeswitch/mod/mod_dialplan_asterisk.so* 
/opt/freeswitch/mod/mod_dingaling.so* 
/opt/freeswitch/mod/mod_iax.so* 
/opt/freeswitch/mod/mod_portaudio.so* 
/opt/freeswitch/mod/mod_sofia.so* 
/opt/freeswitch/mod/mod_woomera.so* 
/opt/freeswitch/mod/mod_openzap.so* 
/opt/freeswitch/mod/mod_cdr_csv.so*
/opt/freeswitch/mod/mod_event_multicast.so* 
/opt/freeswitch/mod/mod_event_socket.so* 
/opt/freeswitch/mod/mod_native_file.so* 
/opt/freeswitch/mod/mod_sndfile.so* 
/opt/freeswitch/mod/mod_local_stream.so* 
/opt/freeswitch/mod/mod_xml_rpc.so* 
/opt/freeswitch/mod/mod_xml_curl.so* 
/opt/freeswitch/mod/mod_xml_cdr.so* 

%files codec-passthru-amr
%defattr(-,freeswitch,daemon)
/opt/freeswitch/mod/mod_amr.so*

%files codec-passthru-g723_1
%defattr(-,freeswitch,daemon)
/opt/freeswitch/mod/mod_g723_1.so*

%files codec-passthru-g729
%defattr(-,freeswitch,daemon)
/opt/freeswitch/mod/mod_g729.so*

%files spidermonkey
%defattr(-,freeswitch,daemon)
/opt/freeswitch/mod/mod_spidermonkey*.so*
/opt/freeswitch/lib/libjs.so*
/opt/freeswitch/lib/libnspr4.so
/opt/freeswitch/lib/libplds4.so
/opt/freeswitch/lib/libplc4.so
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/autoload_configs/spidermonkey.conf.xml

%files devel
%defattr(-,freeswitch,daemon)
/opt/freeswitch/lib/*.a
/opt/freeswitch/lib/*.la
/opt/freeswitch/mod/*.a
/opt/freeswitch/mod/*.la
/opt/freeswitch/include/*.h

%files lang-en
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/en
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/en/demo
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/en/vm
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/en/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/en/demo/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/en/vm/*.xml
/opt/freeswitch/mod/mod_say_en.so*

#%files lang-it
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/it
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/it/demo
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/it/vm
#%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/it/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/it/demo/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/it/vm/*.xml
#/opt/freeswitch/mod/mod_say_it.so*

#%files lang-es
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/es
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/es/demo
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/es/vm
#%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/es/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/es/demo/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/es/vm/*.xml
#/opt/freeswitch/mod/mod_say_es.so*


%files lang-de
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/de
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/de/demo
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/de/vm
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/de/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/de/demo/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/de/vm/*.xml
/opt/freeswitch/mod/mod_say_de.so*

#%files lang-nl
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/nl
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/nl/demo
#%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/nl/vm
#%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/nl/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/nl/demo/*.xml
#%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/nl/vm/*.xml
#/opt/freeswitch/mod/mod_say_nl.so*

%files lang-fr
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/fr
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/fr/demo
%dir %attr(750,freeswitch,daemon) /opt/freeswitch/conf/lang/fr/vm
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/fr/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/fr/demo/*.xml
%config(noreplace) %attr(640,freeswitch,daemon) /opt/freeswitch/conf/lang/fr/vm/*.xml
/opt/freeswitch/mod/mod_say_fr.so*

%changelog
* Thu Dec 5 2007 - michal.bielicki@voiceworks.pl
- put in detail configfiles in to split of spidermonkey configs
- created link from /opt/freesxwitch/conf to /etc/opt/freeswitch
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
