# disable rpath checking
%define __arch_install_post /usr/lib/rpm/check-buildroot
%define _prefix   /opt/freeswitch
%define prefix    %{_prefix}
%define sysconfdir	/opt/freeswitch/conf
%define _sysconfdir	%{sysconfdir}

Name:         freeswitch
Summary:      FreeSWITCH open source telephony platform
License:      MPL
Group:        Productivity/Telephony/Servers
Version:      1.0.4
Release:      1
URL:          http://www.freeswitch.org/
Packager:     	Michal Bielicki
Vendor:       	http://www.freeswitch.org/
Source0:      	http://files.freeswitch.org/%{name}-%{version}.tar.bz2
Source1:	http://files.freeswitch.org/downloads/libs/celt-0.7.0.tar.gz
Source2:	http://files.freeswitch.org/downloads/libs/flite-1.3.99-latest.tar.gz
Source3:	http://files.freeswitch.org/downloads/libs/lame-3.97.tar.gz
Source4:	http://files.freeswitch.org/downloads/libs/libshout-2.2.2.tar.gz
Source5:	http://files.freeswitch.org/downloads/libs/mpg123.tar.gz
Source6:	http://files.freeswitch.org/downloads/libs/openldap-2.4.11.tar.gz
Source7:	http://files.freeswitch.org/downloads/libs/pocketsphinx-0.5.99-latest.tar.gz
Source8:	http://files.freeswitch.org/downloads/libs/soundtouch-1.3.1.tar.gz
Source9:	http://files.freeswitch.org/downloads/libs/sphinxbase-0.4.99-latest.tar.gz
Source10:	http://files.freeswitch.org/downloads/libs/communicator_semi_6000_20080321.tar.gz
Prefix:        %{prefix}

#AutoReqProv:  no

%if 0%{?suse_version} > 100
#BuildRequires: openldap2-devel
BuildRequires: lzo-devel
%else
BuildRequires: openldap-devel
%endif
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: curl-devel
BuildRequires: gcc-c++
BuildRequires: gnutls-devel
BuildRequires: libtool >= 1.5.17
BuildRequires: ncurses-devel
BuildRequires: openssl-devel
BuildRequires: perl
BuildRequires: pkgconfig
BuildRequires: termcap
BuildRequires: unixODBC-devel
BuildRequires: gdbm-devel
BuildRequires: db4-devel
BuildRequires: python-devel
BuildRequires: libogg-devel
BuildRequires: libvorbis-devel
BuildRequires: libjpeg-devel
#BuildRequires: mono-devel
BuildRequires: alsa-lib-devel
BuildRequires: which
BuildRequires: zlib-devel
BuildRequires: e2fsprogs-devel

%if %{?suse_version:1}0
%if 0%{?suse_version} > 910
#BuildRequires: autogen
%endif
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

We support various communication technologies such as SIP, H.323 and GoogleTalk making 
it easy to interface with other open source PBX systems such as sipX, OpenPBX, Bayonne, YATE or Asterisk.

We also support both wide and narrow band codecs making it an ideal solution to bridge legacy 
devices to the future. The voice channels and the conference bridge module all can operate 
at 8, 16 or 32 kilohertz and can bridge channels of different rates.

FreeSWITCH runs on several operating systems including Windows, Max OS X, Linux, BSD and Solaris 
on both 32 and 64 bit platforms.

Our developers are heavily involved in open source and have donated code and other resources to 
other telephony projects including sipXecs, OpenSER, Asterisk, CodeWeaver and OpenPBX.

%package devel
Summary:        Development package for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description devel
FreeSWITCH development files

%package codec-passthru-amrwb
Summary:        Pass-through AMR WideBand Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Conflicts:      codec-amrwb

%description codec-passthru-amrwb
Pass-through AMR WideBand Codec support for FreeSWITCH open source telephony platform


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

%package lua
Summary:	Lua support for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description	lua

%package	perl
Summary:	Perl support for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description	perl

%package        python
Summary:        Python support for the FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description    python


%package lang-en
Summary:	Provides english language dependand modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-en
English language phrases module and directory structure for say module and voicemail

%package lang-ru
Summary:        Provides russian language dependand modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/LibrariesRequires:        %{name} = %{version}-%{release}

%description lang-ru
Russian language phrases module and directory structure for say module and voicemail

%package lang-fr
Summary:        Provides french language dependand modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/LibrariesRequires:        %{name} = %{version}-%{release}

%description lang-fr
French language phrases module and directory structure for say module and voicemail

%package lang-de
Summary:        Provides german language dependand modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/LibrariesRequires:        %{name} = %{version}-%{release}

%description lang-de
German language phrases module and directory structure for say module and voicemail


%package openzap
Summary:	Provides a unified interface to hardware TDM cards and ss7 stacks for FreeSWITCH
Group:		System/Libraries
Requires:        %{name} = %{version}-%{release}

%description openzap
OpenZAP

%prep
%setup -b0 -q
cp %{SOURCE1} libs/
cp %{SOURCE2} libs/
cp %{SOURCE3} libs/
cp %{SOURCE4} libs/
cp %{SOURCE5} libs/
cp %{SOURCE6} libs/
cp %{SOURCE7} libs/
cp %{SOURCE8} libs/
cp %{SOURCE9} libs/
cp %{SOURCE10} libs/


%build
%ifos linux
%if 0%{?suse_version} > 1000 && 0%{?suse_version} < 1030
export CFLAGS="$CFLAGS -fstack-protector"
%endif
%if 0%{?fedora_version} >= 8
export QA_RPATHS=$[ 0x0001|0x0002 ]
%endif
%endif

PASSTHRU_CODEC_MODULES="codecs/mod_g729 codecs/mod_g723_1 codecs/mod_amr codecs/mod_amrwb"
APPLICATIONS_MODULES="applications/mod_commands applications/mod_conference applications/mod_dptools applications/mod_enum applications/mod_esf applications/mod_expr applications/mod_fifo applications/mod_limit applications/mod_rss applications/mod_voicemail applications/mod_directory applications/mod_fsv applications/mod_lcr applications/mod_easyroute applications/mod_stress applications/mod_vmd applications/mod_limit applications/mod_soundtouch applications/mod_fax"
CODECS_MODULES="codecs/mod_ilbc codecs/mod_h26x codecs/mod_voipcodecs codecs/mod_speex codecs/mod_celt codecs/mod_siren codecs/mod_bv"
DIALPLANS_MODULES="dialplans/mod_dialplan_asterisk dialplans/mod_dialplan_directory dialplans/mod_dialplan_xml"
DIRECTORIES_MODULES=""
ENDPOINTS_MODULES="endpoints/mod_dingaling endpoints/mod_portaudio endpoints/mod_sofia ../../libs/openzap/mod_openzap endpoints/mod_loopback"
ASR_TTS_MODULES="asr_tts/mod_pocketsphinx asr_tts/mod_flite asr_tts/mod_unimrcp"
EVENT_HANDLERS_MODULES="event_handlers/mod_event_multicast event_handlers/mod_event_socket event_handlers/mod_cdr_csv"
FORMATS_MODULES="formats/mod_local_stream formats/mod_native_file formats/mod_sndfile formats/mod_tone_stream formats/mod_shout formats/mod_file_string"
LANGUAGES_MODULES="languages/mod_spidermonkey languages/mod_perl languages/mod_lua languages/mod_python"
LOGGERS_MODULES="loggers/mod_console loggers/mod_logfile loggers/mod_syslog"
SAY_MODULES="say/mod_say_en say/mod_say_de say/mod_say_fr"
TIMERS_MODULES=
DISABLED_MODULES="applications/mod_soundtouch asr_tts/mod_cepstral asr_tts/mod_lumenvox event_handlers/mod_event_test event_handlers/mod_radius_cdr event_handlers/mod_zeroconf languages/mod_managed languages/mod_java say/mod_say_it say/mod_say_es say/mod_say_nl languages/mod_yaml"
XML_INT_MODULES="xml_int/mod_xml_rpc  xml_int/mod_xml_curl xml_int/mod_xml_cdr "
MYMODULES="$PASSTHRU_CODEC_MODULES $APPLICATIONS_MODULES $CODECS_MODULES $DIALPLANS_MODULES $DIRECTORIES_MODULES $ENDPOINTS_MODULES $ASR_TTS_MODULES $EVENT_HANDLERS_MODULES $FORMATS_MODULES $LANGUAGES_MODULES $LOGGERS_MODULES $SAY_MODULES $TIMERS_MODULES $XML_INT_MODULES"

export MODULES=$MYMODULES
test ! -f  modules.conf || rm -f modules.conf
touch modules.conf
for i in $MODULES; do echo $i >> modules.conf; done
export VERBOSE=yes
export DESTDIR=$RPM_BUILD_ROOT/
export PKG_CONFIG_PATH=/usr/bin/pkg-config:$PKG_CONFIG_PATH
export ACLOCAL_FLAGS="-I /usr/share/aclocal"

if test ! -f Makefile.in 
then 
   ./bootstrap.sh
fi


	%configure -C \
                --prefix=%{prefix} \
                --infodir=%{_infodir} \
                --mandir=%{_mandir} \
		--sysconfdir=%{sysconfdir} \
		--libdir=%{prefix}/lib \
		--enable-core-libedit-support \
		--enable-core-odbc-support \
%ifos linux
%if 0%{?fedora_version} >= 8
%else
                --with-libcurl \
%endif
%endif
                --with-openssl \
		%{?configure_options}

#Create the version header file here
cat src/include/switch_version.h.in | sed "s/@SVN_VERSION@/%{version}/g" > src/include/switch_version.h
touch .noversion

%{__make}

%install
# delete unsupported langugages for now
#rm -rf conf/lang/de
#rm -rf conf/lang/fr
#rm -rf $RPM_BUILD_ROOT%{prefix}/conf/lang/de
#rm -rf $RPM_BUILD_ROOT%{prefix}/conf/lang/fr

%{__make} DESTDIR=$RPM_BUILD_ROOT install

# Create a log dir
%{__mkdir} -p $RPM_BUILD_ROOT%{prefix}/log

%ifos linux
# Install init files
# On SuSE:
%if 0%{?suse_version} > 100
%{__install} -D -m 744 build/freeswitch.init.suse $RPM_BUILD_ROOT/etc/init.d/freeswitch
%else
# On RedHat like
%{__install} -D -m 0755 build/freeswitch.init.redhat $RPM_BUILD_ROOT/etc/init.d/freeswitch
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
%ifos linux
if ! /usr/bin/id freeswitch &>/dev/null; then
       /usr/sbin/useradd -r -g daemon -s /bin/false -c "The FreeSWITCH Open Source Voice Platform" -d %{prefix} freeswitch || \
                %logmsg "Unexpected error adding user \"freeswitch\". Aborting installation."
fi
%endif

%post
%{?run_ldconfig:%run_ldconfig}
# Make FHS2.0 happy
%{__mkdir} -p /etc/opt
%{__ln_s} -f %{prefix}/conf /etc%{prefix}

chkconfig --add freeswitch

%postun
%{?run_ldconfig:%run_ldconfig}
if [ $1 -eq 0 ]; then
    userdel freeswitch || %logmsg "User \"freeswitch\" could not be deleted."
fi

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%files
%defattr(-,freeswitch,daemon)
%ifos linux
%dir %attr(0750, root, root) /etc/monit.d
%endif
%dir %attr(0750, freeswitch, daemon) %{prefix}/db
%dir %attr(0750, freeswitch, daemon) %{prefix}/log
%dir %attr(0750, freeswitch, daemon) %{prefix}/run
%dir %attr(0750, freeswitch, daemon) %{prefix}/log/xml_cdr
%dir %attr(0750, freeswitch, daemon) %{prefix}/htdocs
%dir %attr(0750, freeswitch, daemon) %{prefix}/scripts
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/autoload_configs
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/dialplan
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/directory
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/directory/default
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/mrcp_profiles
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/skinny_profiles
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/dialplan/default
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/dialplan/public
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles/internal
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles/external
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/jingle_profiles
%dir %attr(0750, freeswitch, daemon) %{prefix}/grammar
%dir %attr(0750, freeswitch, daemon) %{prefix}/grammar/model
%dir %attr(0750, freeswitch, daemon) %{prefix}/grammar/model/communicator
%dir %attr(0750, freeswitch, daemon) %{prefix}/grammar/model/wsj1
%ifos linux
%config(noreplace) %attr(0644, freeswitch, daemon) /etc/monit.d/freeswitch.monitrc
%endif
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/mime.types
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.tpl
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.ttml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/m3ua.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/extensions.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/acl.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/cidlookup.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/alsa.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/conference.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/console.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/dialplan_directory.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/dingaling.conf.xml 
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/enum.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/event_multicast.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/event_socket.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/erlang_event.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/cdr_csv.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/cdr_pg_csv.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/fax.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/fifo.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/shout.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/timezones.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/ivr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/java.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/limit.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/local_stream.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/logfile.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/modules.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/pocketsphinx.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/portaudio.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/post_load_modules.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/python.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/rss.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/sangoma_codec.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/skinny.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/sofia.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/switch.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/syslog.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/voicemail.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/xml_cdr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/xml_curl.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/xml_rpc.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/zeroconf.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/easyroute.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/lcr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/opal.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/unicall.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/memcache.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/nibblebill.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/unimrcp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/directory.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/distributor.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/tts_commandline.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/dialplan/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/dialplan/default/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/dialplan/public/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/directory/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/directory/default/*
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/ivr_menus/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/internal/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/external/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/skinny_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/jingle_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/mrcp_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/grammar/default.dic
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/grammar/model/communicator/*
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/grammar/model/wsj1/*
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/htdocs/*
%ifos linux
#/etc/ld.so.conf.d/*
/etc/init.d/freeswitch
/etc/sysconfig/freeswitch
%if 0%{?suse_version} > 100
/usr/sbin/rcfreeswitch
%endif
%endif
%attr(0755, freeswitch, daemon) %{prefix}/bin/*
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
%{prefix}/mod/mod_directory.so*
%{prefix}/mod/mod_pocketsphinx.so*
%{prefix}/mod/mod_flite.so*
%{prefix}/mod/mod_ilbc.so* 
%{prefix}/mod/mod_h26x.so*
%{prefix}/mod/mod_voipcodecs.so* 
%{prefix}/mod/mod_speex.so* 
%{prefix}/mod/mod_dialplan_directory.so* 
%{prefix}/mod/mod_dialplan_xml.so* 
%{prefix}/mod/mod_dialplan_asterisk.so* 
%{prefix}/mod/mod_dingaling.so* 
%{prefix}/mod/mod_portaudio.so* 
%{prefix}/mod/mod_sofia.so* 
%{prefix}/mod/mod_cdr_csv.so*
%{prefix}/mod/mod_event_multicast.so* 
%{prefix}/mod/mod_event_socket.so* 
%{prefix}/mod/mod_file_string.so*
%{prefix}/mod/mod_native_file.so* 
%{prefix}/mod/mod_sndfile.so* 
%{prefix}/mod/mod_local_stream.so* 
%{prefix}/mod/mod_xml_rpc.so* 
%{prefix}/mod/mod_xml_curl.so* 
%{prefix}/mod/mod_xml_cdr.so* 
%{prefix}/mod/mod_fsv.so
%{prefix}/mod/mod_tone_stream.so
%{prefix}/mod/mod_amrwb.so
%{prefix}/mod/mod_celt.so
%{prefix}/mod/mod_easyroute.so
%{prefix}/mod/mod_lcr.so
%{prefix}/mod/mod_loopback.so
%{prefix}/mod/mod_siren.so
%{prefix}/mod/mod_bv.so
%{prefix}/mod/mod_stress.so
%{prefix}/mod/mod_shout.so
%{prefix}/mod/mod_fax.so
%{prefix}/mod/mod_soundtouch.so
%{prefix}/mod/mod_vmd.so
%{prefix}/mod/mod_unimrcp.so

%files openzap
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/tones.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/openzap.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/pika.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/openzap.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/wanpipe.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/zt.conf
%{prefix}/lib/libopenzap.so*
%{prefix}/mod/mod_openzap.so*
%{prefix}/mod/ozmod_analog.so*
%{prefix}/mod/ozmod_analog_em.so*
%{prefix}/mod/ozmod_isdn.so*
%{prefix}/mod/ozmod_skel.*
%{prefix}/mod/ozmod_zt.so*

%files codec-passthru-amrwb
%defattr(-,freeswitch,daemon)
%{prefix}/mod/mod_amrwb.so*

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

%files lua
%defattr(-,freeswitch,daemon)
%{prefix}/mod/mod_lua*.so*
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/autoload_configs
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/lua.conf.xml

%files perl
%defattr(-,freeswitch,daemon)
%{prefix}/mod/mod_perl*.so*
%{prefix}/perl/*
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/autoload_configs
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/perl.conf.xml

%files python
%defattr(-,freeswitch,daemon)
%{prefix}/mod/mod_python*.so*
%attr(0644, root, bin) /usr/lib/python2.4/site-packages/freeswitch.py*
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/autoload_configs
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/python.conf.xml

%files devel
%defattr(-, freeswitch, daemon)
%{prefix}/lib/*.a
%{prefix}/lib/*.la
%{prefix}/lib/pkgconfig/*
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
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/en/dir/*.xml
%{prefix}/mod/mod_say_en.so*

%files lang-de
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/de
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/de/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/de/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/de/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/de/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/de/vm/*.xml
%{prefix}/mod/mod_say_de.so*

%files lang-fr
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/fr
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/fr/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/fr/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/dir/*.xml
%{prefix}/mod/mod_say_fr.so*

%files lang-ru
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/ru
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/ru/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/ru/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/ru/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/ru/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/ru/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/ru/dir/*.xml

%changelog
* Sun Mar 28 2010 - michal.bielicki@seventhsignal.de
- added sangoma codec config file
* Wed Dec 02 2009 - michal.bielicki@seventhsignal.de
- Soundfiles are moving into a separate spec
* Wed Nov 25 2009 - brian@freeswitch.org
- added mod_bv.so
* Wed Nov 25 2009 - michal.bielicki@seventhsignal.de
- Removed mod_yaml
- added directory files to russian language
* Sat Nov 21 2009 - michal.bielicki@seventhsignal.de
- added patch by Igor Neves <neves.igor@gmail.com>: Added some checkup in %post and %postun to prevent upgrades from removing freeswitch user
* Thu Nov 18 2009 - michal.bielicki@seventhsignal.de
- added new config files for diretory and distributor
- removed sangoma boost from openzap for builds that do not inherit wanpipe while building.
* Tue Jul 24 2009 - mike@jerris.com
- removed mod_http
- removed ozmod_wanpipe
* Tue Jun 23 2009 - raulfragoso@gmail.com
- Adjusted for the latest SVN trunk (13912)
- Included new config and mod files to catch up with latest SVN
- Included new sound files for base256 and zrtp
- mod_unimrcp must be built after mod_sofia
* Mon Feb 17 2009 - michal.bielicki@halokwadrat.de
- added mod_python
- added mod_fax
- added mod_amrwb.so
- added mod_celt.so
- added mod_easyroute.so
- added mod_http.so
- added mod_lcr.so
- added mod_loopback.so
- added mod_siren.so
- added mod/mod_stress.so
- added mod_yaml.so
- added mod_shout.so
- added rpms or all sounds
- openzap is now its own rpm
- added french
- added german
- added missing dependencies
- added soundfiles with separate rpms
- added definition of all sourcefiles and added them to the SRPM
- fixes to monit file
- changes to redhat init file
* Thu May 22 2008 - michal.bielicki@voiceworks.pl
- disabled beta class language stuff
- bumped revision up to rc6
- added mod_lua
- added mod_perl
- Only bootstrap if no Makfile.in exists
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
