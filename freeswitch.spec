######################################################################################################################
######################################################################################################################
#
# spec file for package freeswitch
#
# includes module(s): freeswitch-devel freeswitch-codec-passthru-amr freeswitch-codec-passthru-amrwb freeswitch-codec-passthru-g729 
#                     freeswitch-codec-passthru-g7231 freeswitch-lua freeswitch-perl freeswitch-python freeswitch-spidermonkey
#                     freeswitch-lan-de freeswitch-lang-en freeswitch-lang-fr freeswitch-lang-ru freeswitch-freetdm
#
# Initial Version Copyright (C) 2007 Peter Nixon and Michal Bielicki, All Rights Reserved.
#
# This file is part of:
# FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
# Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
#
# This file and all modifications and additions to the pristine package are under the same license as the package itself.
#
# Contributor(s): Mike Jerris
#				  Brian West
#				  Raul Fragoso
#				  Rupa Shomaker
#				  Marc Olivier Chouinard
#				  Raymond Chandler
#				  Anthony Minessale II <anthm@freeswitch.org>
#
#
# Maintainer(s): Michal Bielicki <michal.bielicki (at) ++nospam_please++ seventhsignal.de
#
######################################################################################################################
# Module build settings
%define build_sng_isdn 0
%define build_sng_ss7 0
%define build_sng_tc 0
%define build_py26_esl 0
%{?with_sang_tc:%define build_sng_tc 1 }
%{?with_sang_isdn:%define build_sng_isdn 1 }
%{?with_sang_ss7:%define build_sng_ss7 1 }
%{?with_py26_esl:%define build_py26_esl 1 }

######################################################################################################################
#
# disable rpath checking
%define __arch_install_post /usr/lib/rpm/check-buildroot
%define _prefix   /opt/freeswitch
%define prefix    %{_prefix}
%define sysconfdir	/opt/freeswitch/conf
%define _sysconfdir	%{sysconfdir}
%define logfiledir	/var/log/freeswitch
%define runtimedir	/var/run/freeswitch

Name:         	freeswitch
Summary:      	FreeSWITCH open source telephony platform
License:      	MPL
Group:        	Productivity/Telephony/Servers
Version:      	1.0.7
Release:      	trunk
URL:          	http://www.freeswitch.org/
Packager:     	Michal Bielicki
Vendor:       	http://www.freeswitch.org/

######################################################################################################################
#
#					Source files and where to get them
#
######################################################################################################################
Source0:    http://files.freeswitch.org/%{name}-%{version}.tar.bz2
Source1:	http://files.freeswitch.org/downloads/libs/celt-0.10.0.tar.gz
Source2:	http://files.freeswitch.org/downloads/libs/flite-1.3.99-latest.tar.gz
Source3:	http://files.freeswitch.org/downloads/libs/lame-3.97.tar.gz
Source4:	http://files.freeswitch.org/downloads/libs/libshout-2.2.2.tar.gz
Source5:	http://files.freeswitch.org/downloads/libs/mpg123.tar.gz
Source6:	http://files.freeswitch.org/downloads/libs/openldap-2.4.11.tar.gz
Source7:	http://files.freeswitch.org/downloads/libs/pocketsphinx-0.5.99-20091212.tar.gz
Source8:	http://files.freeswitch.org/downloads/libs/soundtouch-1.3.1.tar.gz
Source9:	http://files.freeswitch.org/downloads/libs/sphinxbase-0.4.99-20091212.tar.gz
Source10:	http://files.freeswitch.org/downloads/libs/communicator_semi_6000_20080321.tar.gz
Source11:	http://files.freeswitch.org/downloads/libs/libmemcached-0.32.tar.gz
Source12:       http://files.freeswitch.org/downloads/libs/json-c-0.9.tar.gz
Source13:       http://files.freeswitch.org/downloads/libs/opus-0.9.0.tar.gz
Prefix:        	%{prefix}


######################################################################################################################
#
#				Build Dependencies
#
######################################################################################################################

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
%if 0%{?fedora_version} >= 8
BuildRequires: perl-ExtUtils-Embed
%endif
BuildRequires: pkgconfig
%if %{_vendor} == redhat && 0%{?fedora} <= 6
BuildRequires: termcap
%endif
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
BuildRequires: libtheora-devel
BuildRequires: libxml2-devel
BuildRequires: bison
%if %{build_py26_esl}
BuildRequires: python26-devel
Requires: python26
%endif
Requires: alsa-lib
Requires: libogg
Requires: libvorbis
Requires: curl
Requires: ncurses
Requires: openssl
Requires: unixODBC
Requires: libjpeg
Requires: openldap
Requires: db4
Requires: gdbm
Requires: zlib
Requires: libtiff
Requires: python
Requires: libtheora
Requires: libxml2

%if %{?suse_version:1}0
%if 0%{?suse_version} > 910
#BuildRequires: autogen
%endif
%endif

%if 0%{?suse_version} > 800
#PreReq:       /usr/sbin/useradd /usr/sbin/groupadd
PreReq:       %insserv_prereq %fillup_prereq
%endif


######################################################################################################################
#
#					Where the packages are going to be built
#
######################################################################################################################
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


######################################################################################################################
#
#		    Sub Package definitions. Description and Runtime Requirements go here
#		What goes into which package is in the files section after the whole build enchilada
#
######################################################################################################################


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
Requires:	perl

%description	perl

%package        python
Summary:        Python support for the FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Requires:		python

%description    python


%package lang-en
Summary:	Provides english language dependand modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-en
English language phrases module and directory structure for say module and voicemail

%package lang-ru
Summary:        Provides russian language dependand modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-ru
Russian language phrases module and directory structure for say module and voicemail

%package lang-fr
Summary:        Provides french language dependend modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-fr
French language phrases module and directory structure for say module and voicemail

%package lang-de
Summary:        Provides german language dependend modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-de
German language phrases module and directory structure for say module and voicemail

%package lang-he
Summary:        Provides hebrew language dependend modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-he
Hebrew language phrases module and directory structure for say module and voicemail


%package freetdm
Summary:	Provides a unified interface to hardware TDM cards and ss7 stacks for FreeSWITCH
Group:		System/Libraries
Requires:        %{name} = %{version}-%{release}

%description freetdm
FreeTDM

%if %{build_sng_isdn}

%package freetdm-sng-isdn
Summary:	Sangoma ISDN Module for FreeTDM
Group:		System/Libraries
Requires:       %{name} = %{version}-%{release}
Requires:       %{name}-freetdm = %{version}-%{release}
Requires: wanpipe 
Requires: libsng_isdn 
BuildRequires: wanpipe 
BuildRequires: libsng_isdn 

%description freetdm-sng-isdn
Sangoma ISDN Module for freetdm

%endif

%if %{build_sng_ss7}

%package freetdm-sng-ss7
Summary:	Provides a unified interface to hardware TDM cards and ss7 stacks for FreeSWITCH, Sangoma SS7 Module
Group:		System/Libraries
Requires:        %{name} = %{version}-%{release}
Requires:       %{name}-freetdm = %{version}-%{release}
Requires: wanpipe 
Requires: libsng_ss7 
BuildRequires: wanpipe 
BuildRequires: libsng_ss7 

%description freetdm-sng-ss7
Sangoma SMG-SS7 drivers for FreeTDM

%endif


%if %{build_sng_tc}

%package sangoma-codec
Summary:	Sangoma D100 and D500 Codec Card Support
Group:		System/Libraries
Requires:        %{name} = %{version}-%{release}
Requires: sng-tc-linux
BuildRequires: sng-tc-linux

%description sangoma-codec
Sangoma D100 and D500 Codec Card Support

%endif

%package skypopen
Summary:	Skype Endpoint
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Requires:	libX11
BuildRequires:	libX11-devel

%description skypopen
This software (Skypopen) uses the Skype API but is not endorsed, certified or otherwise approved in any way by Skype.
Skypopen is an endpoint (channel driver) that uses the Skype client as an interface to the Skype network, and allows 
incoming and outgoing Skype calls to/from FreeSWITCH (that can be bridged, originated, answered, etc. as in all other 
endpoints, e.g. Sofia-SIP).

######################################################################################################################
#
#				Unpack and prepare Source archives, copy stuff around etc ..
#
######################################################################################################################

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
cp %{SOURCE11} libs/
cp %{SOURCE12} libs/
cp %{SOURCE13} libs/

######################################################################################################################
#
#						Start the Build process
#
######################################################################################################################
%build
%ifos linux
%if 0%{?suse_version} > 1000 && 0%{?suse_version} < 1030
export CFLAGS="$CFLAGS -fstack-protector"
%endif
%if 0%{?fedora_version} >= 8
export QA_RPATHS=$[ 0x0001|0x0002 ]
%endif
%endif

######################################################################################################################
#
#				Here the modules that will be build get defined
#
######################################################################################################################
######################################################################################################################
#
#						Application Modules
#
######################################################################################################################
APPLICATION_MODULES_AE="applications/mod_avmd applications/mod_callcenter applications/mod_cidlookup applications/mod_cluechoo \
                        applications/mod_commands applications/mod_conference applications/mod_curl applications/mod_db applications/mod_directory \
                        applications/mod_distributor applications/mod_dptools applications/mod_easyroute applications/mod_enum \
                        applications/mod_esf applications/mod_expr"
APPLICATION_MODULES_FM="applications/mod_fifo applications/mod_fsv applications/mod_hash applications/mod_lcr applications/mod_limit \
                        applications/mod_memcache"
APPLICATION_MODULES_NY="applications/mod_nibblebill applications/mod_redis applications/mod_rss applications/mod_snom \
                        applications/mod_soundtouch applications/mod_spandsp applications/mod_spy applications/mod_stress \
                        applications/mod_valet_parking applications/mod_vmd applications/mod_voicemail"

APPLICATIONS_MODULES="$APPLICATION_MODULES_AE $APPLICATION_MODULES_FM $APPLICATION_MODULES_NY $APPLICATION_MODULES_VZ"
######################################################################################################################
#
#				Automatic Speech Recognition and Text To Speech Modules
#
######################################################################################################################
ASR_TTS_MODULES="asr_tts/mod_pocketsphinx asr_tts/mod_flite asr_tts/mod_unimrcp"
######################################################################################################################
#
#						Codecs
#
######################################################################################################################
CODECS_MODULES="codecs/mod_bv codecs/mod_h26x codecs/mod_speex codecs/mod_celt codecs/mod_codec2 codecs/mod_ilbc codecs/mod_mp4v \
                codecs/mod_opus codecs/mod_silk codecs/mod_siren codecs/mod_theora "
#
%if %{build_sng_tc}
CODECS_MODULES+="codecs/mod_sangoma_codec"
%endif
######################################################################################################################
#
#					Dialplan Modules
#
######################################################################################################################
DIALPLANS_MODULES="dialplans/mod_dialplan_asterisk dialplans/mod_dialplan_directory dialplans/mod_dialplan_xml"
######################################################################################################################
#
#					Directory Modules
#
######################################################################################################################
DIRECTORIES_MODULES=""
######################################################################################################################
#
#						Endpoints
#
######################################################################################################################
ENDPOINTS_MODULES="endpoints/mod_dingaling endpoints/mod_loopback ../../libs/freetdm/mod_freetdm endpoints/mod_portaudio \
                   endpoints/mod_sofia endpoints/mod_skinny endpoints/mod_skypopen"
 
######################################################################################################################
#
#						Event Handlers
#
######################################################################################################################
EVENT_HANDLERS_MODULES="event_handlers/mod_cdr_csv event_handlers/mod_cdr_sqlite event_handlers/mod_event_socket event_handlers/mod_event_multicast"
######################################################################################################################
#
#					File and Audio Format Handlers
#
######################################################################################################################
FORMATS_MODULES="formats/mod_local_stream formats/mod_native_file formats/mod_portaudio_stream \
                 formats/mod_shout formats/mod_sndfile formats/mod_tone_stream"

######################################################################################################################
#
#						Embedded Languages
#
######################################################################################################################
LANGUAGES_MODULES="languages/mod_lua languages/mod_perl languages/mod_python languages/mod_spidermonkey"
######################################################################################################################
#
#						Logging Modules
#
######################################################################################################################
LOGGERS_MODULES="loggers/mod_console loggers/mod_logfile loggers/mod_syslog"
######################################################################################################################
#
#						Passthru Codecs
#
######################################################################################################################
PASSTHRU_CODEC_MODULES="codecs/mod_amr codecs/mod_amrwb codecs/mod_g723_1 codecs/mod_g729"
######################################################################################################################
#
#						Phrase engine language modules
#
######################################################################################################################
SAY_MODULES="say/mod_say_de say/mod_say_en say/mod_say_fr say/mod_say_he say/mod_say_ru"
######################################################################################################################
#
#							Timers
#
######################################################################################################################
TIMERS_MODULES=
######################################################################################################################
#
#						XML Modules
#
######################################################################################################################
XML_INT_MODULES="xml_int/mod_xml_cdr xml_int/mod_xml_curl xml_int/mod_xml_rpc"
######################################################################################################################
#
#				Create one environment variable out of all the module defs
#
######################################################################################################################
MYMODULES="$PASSTHRU_CODEC_MODULES $APPLICATIONS_MODULES $CODECS_MODULES $DIALPLANS_MODULES $DIRECTORIES_MODULES \
$ENDPOINTS_MODULES $ASR_TTS_MODULES $EVENT_HANDLERS_MODULES $FORMATS_MODULES $LANGUAGES_MODULES $LOGGERS_MODULES \
$SAY_MODULES $TIMERS_MODULES $XML_INT_MODULES"

######################################################################################################################
#
#					Create Modules build list and set variables
#
######################################################################################################################

export MODULES=$MYMODULES
test ! -f  modules.conf || rm -f modules.conf
touch modules.conf
for i in $MODULES; do echo $i >> modules.conf; done
export VERBOSE=yes
export DESTDIR=%{buildroot}/
export PKG_CONFIG_PATH=/usr/bin/pkg-config:$PKG_CONFIG_PATH
export ACLOCAL_FLAGS="-I /usr/share/aclocal"

######################################################################################################################
#
#				Bootstrap, Configure and Build the whole enchilada
#
######################################################################################################################

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
#cat src/include/switch_version.h.in | sed "s/@SVN_VERSION@/%{version}/g" > src/include/switch_version.h
#touch .noversion

%{__make}

cd libs/esl
%{__make} pymod


######################################################################################################################
#
#				Install it and create some required dirs and links
#
######################################################################################################################
%install

%{__make} DESTDIR=%{buildroot} install

# Create a log dir
%{__mkdir} -p %{buildroot}%{prefix}/log
%{__mkdir} -p %{buildroot}%{logfiledir}
%{__mkdir} -p %{buildroot}%{runtimedir}

#install the esl stuff
cd libs/esl
%{__make} DESTDIR=%{buildroot} pymod-install

%if %{build_py26_esl}
#install esl for python 26
%{__make} clean
sed -i s/python\ /python26\ /g python/Makefile
%{__make} pymod
%{__mkdir} -p %{buildroot}/usr/lib/python2.6/site-packages
%{__make} DESTDIR=%{buildroot} pymod-install
%endif

cd ../..

%ifos linux
# Install init files
# On SuSE:
%if 0%{?suse_version} > 100
%{__install} -D -m 744 build/freeswitch.init.suse %{buildroot}/etc/rc.d/init.d/freeswitch
%else
# On RedHat like
%{__install} -D -m 0755 build/freeswitch.init.redhat %{buildroot}/etc/rc.d/init.d/freeswitch
%endif
# On SuSE make /usr/sbin/rcfreeswitch a link to /etc/rc.d/init.d/freeswitch
%if 0%{?suse_version} > 100
%{__mkdir} -p %{buildroot}/usr/sbin
%{__ln_s} -f /etc/rc.d/init.d/freeswitch %{buildroot}/usr/sbin/rcfreeswitch
%endif
# Add the sysconfiguration file
%{__install} -D -m 744 build/freeswitch.sysconfig %{buildroot}/etc/sysconfig/freeswitch
# Add monit file
%{__install} -D -m 644 build/freeswitch.monitrc %{buildroot}/etc/monit.d/freeswitch.monitrc
%endif
######################################################################################################################
#
#                               Remove files that are not wanted if they exist
#
######################################################################################################################

%if %{build_sng_ss7}
#do not delete a thing
%else
%{__rm} -f %{buildroot}/%{prefix}/mod/ftmod_sangoma_ss7*
%endif
%if %{build_sng_isdn}
#do not delete a thing
%else
%{__rm} -f %{buildroot}/%{prefix}/mod/ftmod_sangoma_isdn*
%endif



######################################################################################################################
#
#			Add a freeswitch user with group daemon that will own the whole enchilada
#
######################################################################################################################
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
######################################################################################################################
#
#				On uninstallation get rid of the freeswitch user
#
######################################################################################################################
%{?run_ldconfig:%run_ldconfig}
if [ $1 -eq 0 ]; then
    userdel freeswitch || %logmsg "User \"freeswitch\" could not be deleted."
fi

%clean
%{__rm} -rf %{buildroot}

%files
######################################################################################################################
#
#			What to install where ... first set default permissions
#
######################################################################################################################
%defattr(-,freeswitch,daemon)
######################################################################################################################
#
#							Directories
#
######################################################################################################################
#
#################################### Basic Directory Structure #######################################################
#
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf
%dir %attr(0750, freeswitch, daemon) %{prefix}/db
%dir %attr(0750, freeswitch, daemon) %{prefix}/grammar
%dir %attr(0750, freeswitch, daemon) %{prefix}/htdocs
%dir %attr(0750, freeswitch, daemon) %{logfiledir}
%dir %attr(0750, freeswitch, daemon) %{runtimedir}
%dir %attr(0750, freeswitch, daemon) %{prefix}/scripts
#
#################################### Config Directory Structure #######################################################
#
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/autoload_configs
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/dialplan
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/dialplan/default
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/dialplan/public
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/dialplan/skinny-patterns
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/directory
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/directory/default
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/jingle_profiles
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/mrcp_profiles
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles/external
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/sip_profiles/internal
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/skinny_profiles
#
#################################### Grammar Directory Structure #####################################################
#
%dir %attr(0750, freeswitch, daemon) %{prefix}/grammar/model
%dir %attr(0750, freeswitch, daemon) %{prefix}/grammar/model/communicator
%dir %attr(0750, freeswitch, daemon) %{prefix}/grammar/model/wsj1
%ifos linux
%config(noreplace) %attr(0644, freeswitch, daemon) /etc/monit.d/freeswitch.monitrc
%endif
######################################################################################################################
#
#						Config Files
#
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.tpl
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.ttml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/extensions.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/mime.types
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/acl.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/alsa.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/callcenter.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/cdr_csv.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/cdr_pg_csv.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/cdr_sqlite.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/cidlookup.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/conference.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/console.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/db.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/dialplan_directory.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/dingaling.conf.xml 
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/directory.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/distributor.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/easyroute.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/enum.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/erlang_event.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/event_multicast.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/event_socket.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/fax.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/fifo.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/hash.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/ivr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/java.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/lcr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/local_stream.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/logfile.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/memcache.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/modules.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/nibblebill.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/opal.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/osp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/pocketsphinx.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/portaudio.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/post_load_modules.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/redis.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/rss.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/sangoma_codec.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/shout.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/skinny.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/sofia.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/spandsp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/switch.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/syslog.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/timezones.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/tts_commandline.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/unicall.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/unimrcp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/voicemail.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/xml_cdr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/xml_curl.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/xml_rpc.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/zeroconf.conf.xml
######################################################################################################################
#
#						Dialplans
#
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/dialplan/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/dialplan/default/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/dialplan/public/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/dialplan/skinny-patterns/*.xml
######################################################################################################################
#
#						User Directories
#
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/directory/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/directory/default/*
######################################################################################################################
#
#							IVR Menues
#
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/ivr_menus/*.xml
######################################################################################################################
#
#							Sip Profiles
#
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/internal/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/sip_profiles/external/*.xml
######################################################################################################################
#
#				Other Protocol Profiles (skinny, jingle, mrcp)
#
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/skinny_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/jingle_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/mrcp_profiles/*.xml
######################################################################################################################
#
#						Grammar Files
#
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/grammar/default.dic
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/grammar/model/communicator/*
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/grammar/model/wsj1/*
######################################################################################################################
#
#						Other Fíles
#
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/htdocs/*
%ifos linux
/etc/rc.d/init.d/freeswitch
/etc/sysconfig/freeswitch
%if 0%{?suse_version} > 100
/usr/sbin/rcfreeswitch
%endif
%endif
%ifos linux
%dir %attr(0750, root, root) /etc/monit.d
%endif
######################################################################################################################
#
#						Binaries
#
######################################################################################################################
%attr(0755, freeswitch, daemon) %{prefix}/bin/*
%{prefix}/lib/libfreeswitch*.so*
######################################################################################################################
#
#			Modules in Alphabetical Order, please keep them that way..
#
######################################################################################################################
%{prefix}/mod/mod_amrwb.so*
%{prefix}/mod/mod_avmd.so*
%{prefix}/mod/mod_bv.so*
%{prefix}/mod/mod_callcenter.so*
%{prefix}/mod/mod_cdr_csv.so*
%{prefix}/mod/mod_cdr_sqlite.so*
%{prefix}/mod/mod_celt.so*
%{prefix}/mod/mod_cidlookup.so*
%{prefix}/mod/mod_cluechoo.so*
%{prefix}/mod/mod_codec2.so*
%{prefix}/mod/mod_console.so*
%{prefix}/mod/mod_commands.so*
%{prefix}/mod/mod_conference.so*
%{prefix}/mod/mod_curl.so*
%{prefix}/mod/mod_db.so*
%{prefix}/mod/mod_dialplan_asterisk.so* 
%{prefix}/mod/mod_dialplan_directory.so* 
%{prefix}/mod/mod_dialplan_xml.so* 
%{prefix}/mod/mod_dingaling.so*
%{prefix}/mod/mod_directory.so*
%{prefix}/mod/mod_distributor.so*
%{prefix}/mod/mod_dptools.so*
%{prefix}/mod/mod_easyroute.so*
%{prefix}/mod/mod_enum.so*
%{prefix}/mod/mod_esf.so*
%{prefix}/mod/mod_event_multicast.so* 
%{prefix}/mod/mod_event_socket.so* 
%{prefix}/mod/mod_expr.so*
%{prefix}/mod/mod_fifo.so*
%{prefix}/mod/mod_flite.so*
%{prefix}/mod/mod_fsv.so*
%{prefix}/mod/mod_hash.so*
%{prefix}/mod/mod_h26x.so*
%{prefix}/mod/mod_ilbc.so*
%{prefix}/mod/mod_lcr.so*
%{prefix}/mod/mod_limit.so*
%{prefix}/mod/mod_local_stream.so*
%{prefix}/mod/mod_logfile.so*
%{prefix}/mod/mod_loopback.so*
%{prefix}/mod/mod_memcache.so*
%{prefix}/mod/mod_mp4v.so*
%{prefix}/mod/mod_native_file.so*
%{prefix}/mod/mod_nibblebill.so*
%{prefix}/mod/mod_opus.so*
%{prefix}/mod/mod_pocketsphinx.so*
%{prefix}/mod/mod_portaudio.so*
%{prefix}/mod/mod_portaudio_stream.so*
%{prefix}/mod/mod_redis.so*
%{prefix}/mod/mod_rss.so*
%{prefix}/mod/mod_shout.so*
%{prefix}/mod/mod_silk.so*
%{prefix}/mod/mod_siren.so*
%{prefix}/mod/mod_skinny.so*
%{prefix}/mod/mod_sndfile.so*
%{prefix}/mod/mod_snom.so*
%{prefix}/mod/mod_sofia.so*
%{prefix}/mod/mod_soundtouch.so*
%{prefix}/mod/mod_spandsp.so*
%{prefix}/mod/mod_speex.so*
%{prefix}/mod/mod_spy.so*
%{prefix}/mod/mod_stress.so*
%{prefix}/mod/mod_syslog.so*
%{prefix}/mod/mod_theora.so*
%{prefix}/mod/mod_tone_stream.so*
%{prefix}/mod/mod_unimrcp.so*
%{prefix}/mod/mod_valet_parking.so*
%{prefix}/mod/mod_vmd.so*
%{prefix}/mod/mod_voicemail.so*
%{prefix}/mod/mod_xml_cdr.so*
%{prefix}/mod/mod_xml_curl.so* 
%{prefix}/mod/mod_xml_rpc.so* 
######################################################################################################################
#
#						Package for the developer
#
######################################################################################################################
%files devel
%defattr(-, freeswitch, daemon)
%{prefix}/lib/*.a
%{prefix}/lib/*.la
%{prefix}/lib/pkgconfig/*
%{prefix}/mod/*.a
%{prefix}/mod/*.la
%{prefix}/include/*.h
######################################################################################################################
#
#						FreeTDM Module for TDM Interaction
#
######################################################################################################################
%files freetdm
%defattr(-, freeswitch, daemon)
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/tones.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/freetdm.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/pika.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/freetdm.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/wanpipe.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/zt.conf
%{prefix}/lib/libfreetdm.so*
%{prefix}/mod/mod_freetdm.so*
%{prefix}/mod/ftmod_skel*.so*
%{prefix}/mod/ftmod_[a-r,t-z]*.so*

%if %{build_sng_tc}
%files sangoma-codec
%defattr(-, freeswitch, daemon)
%{prefix}/mod/mod_sangoma_codec.so*
%endif

%if %{build_sng_ss7}
%files freetdm-sng-ss7
%defattr(-, freeswitch, daemon)
%{prefix}/mod/ftmod_sangoma_ss7.so*
%endif

%if %{build_sng_isdn}
%files freetdm-sng-isdn
%defattr(-, freeswitch, daemon)
%{prefix}/mod/ftmod_sangoma_isdn.so*
%endif

######################################################################################################################
#
#						Passthru Codec Modules
#
######################################################################################################################
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

######################################################################################################################
#
#					Embedded Language Modules
#
######################################################################################################################
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
%attr(0644, root, bin) /usr/lib/python*/site-packages/freeswitch.py*
%attr(0755, root, bin) /usr/lib/python*/site-packages/_ESL.so*
%attr(0755, root, bin) /usr/lib/python*/site-packages/ESL.py*
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/autoload_configs
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/autoload_configs/python.conf.xml

%files skypopen
%defattr(-,freeswitch,daemon)
%{prefix}/mod/mod_skypopen.so*
######################################################################################################################
#
#						Language Modules
#
######################################################################################################################
%files lang-en
%defattr(-, freeswitch, daemon)
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/en
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/en/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/en/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/en/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/en/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/en/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/en/dir/*.xml
%{prefix}/mod/mod_say_en.so*

%files lang-de
%defattr(-, freeswitch, daemon)
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/de
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/de/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/de/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/de/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/de/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/de/vm/*.xml
%{prefix}/mod/mod_say_de.so*

%files lang-fr
%defattr(-, freeswitch, daemon)
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/fr
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/fr/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/fr/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/fr/dir/*.xml
%{prefix}/mod/mod_say_fr.so*

%files lang-ru
%defattr(-, freeswitch, daemon)
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/ru
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/ru/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/ru/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/ru/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/ru/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/ru/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/ru/dir/*.xml
%{prefix}/mod/mod_say_ru.so*

%files lang-he
%defattr(-, freeswitch, daemon)
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/he/
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/he/demo
%dir %attr(0750, freeswitch, daemon) %{prefix}/conf/lang/he/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/he/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/he/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/he/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{prefix}/conf/lang/he/dir/*.xml
%{prefix}/mod/mod_say_he.so*

######################################################################################################################
#
#						Changelog
#
######################################################################################################################
%changelog
* Fri Apr 01 2011 - michal.bielicki@seventhsignal.de
- added hebrew language stuff
* Wed Mar 30 2011 - michal.bielicki@seventhsignal.de
- removed mod_file_string since it has been merged into dptools
* Wed Feb 16 2011 - michal.bielicki@seventhsignal.de
- added mod_skinny
- added sangoma libraries
- added sangoma codec module for D100 and D150 and D500
- added skypopen module
- fixes for ss7 freetdm modules
- added mod_opus
- added selector for sangoma modules
- addded python esl module to rpm
- some minor cleanups
- cut sangoma modules into separate rpms as addons for freetdm
* Tue Jan 18 2011 - michal.bielicki@seventhsignal.de
- Fedora adjustments
* Fri Oct 15 2010 - michal.bielicki@seventhsignal.de
- added mod_curl
* Sat Oct 09 2010 - michal.bielicki@seventhsignal.de
- added mod_silk
- added mod_codec2
- moved from openzap to freetdm to make way for inclusion of libsng_isdn and wanpipe
- added mod_freetdm
- added mod_cidlookup
- added more runtime dependencies
* Thu Sep 30 2010 - michal.bielicki@seventhsignal.de
- added mod_nibblebill to standard modules
* Sun Sep 26 2010 - michal.bielicki@seventhsignal.de
- added portaudio_stream module
- some more formating work
* Mon Jul 19 2010 - michal.bielicki@seventhsignal.de
- new hash module config file added to freeswitch.spec
* Mon Jul 19 2010 - michal.bielicki@seventhsignal.de
- Adjusted sphinxbase
- Fixed Version Revisions for head versions
- Renamed packages to head to comply with git
* Tue Jun 22 2010 - michal.bielicki@seventhsignal.de
- Added comments and made the spec file sections more transparent
- Added proper header to the Spec file
- Added Contributors
- Added Anthony's copyright for the whole package into the header
* Tue Jun 22 2010 - michal.bielicki@seventhsignal.de
- Reorganized the modules alphabeticaly
- synced SFEopensolaris and centos spec
- started to fix Run Dependencies
- added mod_say_ru which seemd to have gone missing
- added comment blocks to show the spec file structure for easier management and editing
* Mon Jun 21 2010 - michal.bielicki@seventhsignal.de
- added mod_limit shim for backwards compatibility
- added mod_hash correctly
* Sun Jun 20 2010 - michal.bielicki@seventhsignal.de
- replaced mod_limit with mod_db
- added mod_spy
- added mod_valet_parking
- addded mod_memcache
- added mod_distributor
- added mod_avmd
* Thu Apr 29 2010 - michal.bielicki@seventhsignal.de
- added osp conf file
* Fri Apr 23 2010 - michal.bielicki@seventhsignal.de
- bumped spec file vrersion up to 1.0.7-trunk for trunk
- added skinny dialplan stuff to specfile
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

