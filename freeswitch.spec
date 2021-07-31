######################################################################################################################
#
# spec file for package freeswitch
#
# includes module(s): freeswitch-devel freeswitch-codec-passthru-amr freeswitch-codec-passthru-amrwb freeswitch-codec-passthru-g729 
#                     freeswitch-codec-passthru-g7231 freeswitch-lua freeswitch-mariadb freeswitch-pgsql freeswitch-perl freeswitch-python freeswitch-v8 freeswitch-signalwire
#                     freeswitch-lan-de freeswitch-lang-en freeswitch-lang-fr freeswitch-lang-hu freeswitch-lang-ru
#		      and others
#
# Initial Version Copyright (C) 2007 Peter Nixon and Michal Bielicki, All Rights Reserved.
#
# This file is part of:
# FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
# Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
#
# This file and all modifications and additions to the pristine package are under the same license as the package itself.
#
# Contributor(s): Mike Jerris
#                 Brian West
#                 Anthony Minessale II <anthm@freeswitch.org>
#                 Raul Fragoso
#                 Rupa Shomaker
#                 Marc Olivier Chouinard
#                 Raymond Chandler
#                 Ken Rice <krice@freeswitch.org>
#                 Chris Rienzo <crienzo@grasshopper.com>
#
# Maintainer(s): SignalWire, Inc <support@signalwire.com>
#
######################################################################################################################
# Module build settings
%define build_sng_isdn 0
%define build_sng_ss7 0
%define build_sng_tc 0
%define build_py26_esl 0
%define build_timerfd 0
%define build_mod_esl 0
%define build_mod_rayo 1
%define build_mod_ssml 1
%define build_mod_v8 0

%{?with_sang_tc:%define build_sng_tc 1 }
%{?with_sang_isdn:%define build_sng_isdn 1 }
%{?with_sang_ss7:%define build_sng_ss7 1 }
%{?with_py26_esl:%define build_py26_esl 1 }
%{?with_timerfd:%define build_timerfd 1 }
%{?with_mod_esl:%define build_mod_esl 1 }
%{?with_mod_v8:%define build_mod_v8 1 }

%define nonparsedversion 1.7.0
%define version %(echo '%{nonparsedversion}' | sed 's/-//g')
%define release 1

######################################################################################################################
#
# disable rpath checking
#%define __arch_install_post /usr/lib/rpm/check-buildroot
#%define _prefix   /usr
#%define prefix    %{_prefix}
#%define sysconfdir	/etc/freeswitch
#%define _sysconfdir	%{sysconfdir}
#%define logfiledir	/var/log/freeswitch
#%define _logfiledir	%{logfiledir}
#%define runtimedir	/var/run/freeswitch
#%define _runtimedir	%{runtimedir}

######################################################################################################################
# Layout of packages FHS (Redhat/SUSE), FS (Standard FreeSWITCH layout using /usr/local), OPT (/opt based layout)
%define packagelayout	FHS

%define	PREFIX		%{_prefix}
%define EXECPREFIX	%{_exec_prefix}
%define BINDIR		%{_bindir}
%define SBINDIR		%{_sbindir}
%define LIBEXECDIR	%{_libexecdir}/%name
%define SYSCONFDIR	%{_sysconfdir}/%name
%define SHARESTATEDIR	%{_sharedstatedir}/%name
%define LOCALSTATEDIR	%{_localstatedir}/lib/%name
%define LIBDIR		%{_libdir}
%define INCLUDEDIR	%{_includedir}
%define _datarootdir	%{_prefix}/share
%define DATAROOTDIR	%{_datarootdir}
%define DATADIR		%{_datadir}
%define INFODIR		%{_infodir}
%define LOCALEDIR	%{_datarootdir}/locale
%define MANDIR		%{_mandir}
%define DOCDIR		%{_defaultdocdir}/%name
%define HTMLDIR		%{_defaultdocdir}/%name/html
%define DVIDIR		%{_defaultdocdir}/%name/dvi
%define PDFDIR		%{_defaultdocdir}/%name/pdf
%define PSDIR		%{_defaultdocdir}/%name/ps
%define LOGFILEDIR	/var/log/%name
%define MODINSTDIR	%{_libdir}/%name/mod
%define RUNDIR		%{_localstatedir}/run/%name
%define DBDIR		%{LOCALSTATEDIR}/db
%define HTDOCSDIR	%{_datarootdir}/%name/htdocs
%define SOUNDSDIR	%{_datarootdir}/%name/sounds
%define GRAMMARDIR	%{_datarootdir}/%name/grammar
%define SCRIPTDIR	%{_datarootdir}/%name/scripts
%define RECORDINGSDIR	%{LOCALSTATEDIR}/recordings
%define PKGCONFIGDIR	%{_datarootdir}/%name/pkgconfig
%define HOMEDIR		%{LOCALSTATEDIR}


Name:         	freeswitch
Summary:      	FreeSWITCH open source telephony platform
License:      	MPL1.1
Group:        	Productivity/Telephony/Servers
Version:	%{version}
Release:	%{release}%{?dist}
URL:          	http://www.freeswitch.org/
Packager:     	Ken Rice
Vendor:       	http://www.freeswitch.org/

######################################################################################################################
#
#					Source files and where to get them
#
######################################################################################################################
Source0:        http://files.freeswitch.org/%{name}-%{nonparsedversion}.tar.bz2
Source1:	http://files.freeswitch.org/downloads/libs/freeradius-client-1.1.7.tar.gz
Source2:	http://files.freeswitch.org/downloads/libs/communicator_semi_6000_20080321.tar.gz
Source3:	http://files.freeswitch.org/downloads/libs/pocketsphinx-0.8.tar.gz
Source4:	http://files.freeswitch.org/downloads/libs/sphinxbase-0.8.tar.gz
Prefix:        	%{prefix}


######################################################################################################################
#
#				Build Dependencies
#
######################################################################################################################

%if 0%{?suse_version} > 100
BuildRequires: lzo-devel
%endif
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: curl-devel >= 7.19
BuildRequires: gcc-c++
BuildRequires: libtool >= 1.5.17
BuildRequires: openssl-devel >= 1.0.1e
BuildRequires: sofia-sip-devel >= 1.13.4
BuildRequires: spandsp3-devel >= 3.0
BuildRequires: pcre-devel 
BuildRequires: speex-devel 
BuildRequires: sqlite-devel >= 3.6.20
BuildRequires: libtiff-devel
BuildRequires: libedit-devel
BuildRequires: yasm
BuildRequires: pkgconfig
BuildRequires: unixODBC-devel
BuildRequires: libjpeg-devel
BuildRequires: which
BuildRequires: zlib-devel
BuildRequires: libxml2-devel
BuildRequires: libsndfile-devel
Requires: curl >= 7.19
Requires: pcre
Requires: speex
Requires: sqlite >= 3.6.20
Requires: libtiff
Requires: libedit
Requires: openssl >= 1.0.1e
Requires: unixODBC
Requires: libjpeg
Requires: zlib
Requires: libxml2
Requires: libsndfile

%if 0%{?rhel} == 7
# to build mariadb module required gcc >= 4.9 (more details GH #1046)
# On CentOS 7 dist you can install fresh gcc using command
# yum install centos-release-scl && yum install devtoolset-9
BuildRequires: devtoolset-9
%endif
%if 0%{?rhel} == 8
# we want use fresh gcc on RHEL 8 based dists
# On CentOS 8 dist you can install fresh gcc using command
# dnf install gcc-toolset-9
BuildRequires: gcc-toolset-9
%endif

%if 0%{?suse_version} > 800
PreReq:       %insserv_prereq %fillup_prereq
%endif

%if 0%{?fedora}
BuildRequires: gumbo-parser-devel
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

######################################################################################################################
#				FreeSWITCH Application Modules
######################################################################################################################
%package application-abstraction
Summary:	FreeSWITCH mod_abstraction
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-abstraction
Provide an abstraction to FreeSWITCH API calls

%package application-avmd
Summary:	FreeSWITCH voicemail detector
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-avmd
Provide an voicemail beep detector for FreeSWITCH

%package application-blacklist
Summary:	FreeSWITCH blacklist module
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-blacklist
Provide black/white listing of various fields used for routing calls in 
FreeSWITCH

%package application-callcenter
Summary:	FreeSWITCH mod_callcenter Call Queuing Application
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-callcenter
Provide Automated Call Distribution capabilities for FreeSWITCH

%package application-cidlookup
Summary:	FreeSWITCH mod_cidlookup 
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-cidlookup
Provide FreeSWITCH access to third party CallerID Name Databases via HTTP

%package application-conference
Summary:	FreeSWITCH mod_conference
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-conference
Provide FreeSWITCH Conference Bridge Services. 

%package application-curl
Summary:	FreeSWITCH mod_curl
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-curl
Provide FreeSWITCH dialplan access to CURL

%package application-db
Summary:	FreeSWITCH mod_db
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-db
mod_db implements an API and dialplan interface to a database backend for 
FreeSWITCH.  The database can either be in sqlite or ODBC.  It also provides 
support for group dialing and provides database backed limit interface. 

%package application-directory
Summary:	FreeSWITCH mod_directory
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-directory
Provides FreeSWITCH mod_directory, a dial by name directory application.

%package application-distributor
Summary:	FreeSWITCH mod_distributor
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-distributor
Provides FreeSWITCH mod_distributor, a simple round-robin style distribution
to call gateways.

%package application-easyroute
Summary:	FreeSWITCH mod_easyroute
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-easyroute
Provides FreeSWITCH mod_easyroute, a simple, easy to use DB Backed DID routing 
Engine. Uses ODBC to connect to the DB of your choice.

%package application-enum
Summary:	FreeSWITCH mod_enum
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  ldns-devel

%description application-enum
Provides FreeSWITCH mod_enum, a ENUM dialplan, with API and Dialplan extensions 
supporting ENUM lookups.

%package application-esf
Summary:	FreeSWITCH mod_esf
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-esf
Provides FreeSWITCH mod_esf, Extra Sip Functionality such as Multicast Support

%if %{build_mod_esl}
%package application-esl
Summary:	FreeSWITCH mod_esl
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-esl
Provides FreeSWITCH mod_esl, add api commands for remote ESL commands
%endif

%package application-expr
Summary:	FreeSWITCH mod_expr
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-expr
Provides FreeSWITCH mod_expr, implements Brian Allen Vanderburg's ExprEval 
expression evaluation library for FreeSWITCH.

%package application-fifo
Summary:	FreeSWITCH mod_fifo
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-fifo
Provides FreeSWITCH mod_fifo, a parking-like app which allows you to make 
custom call queues

%package application-fsk
Summary:	FreeSWITCH mod_fsk
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-fsk
Provides FreeSWITCH mod_fsk, a module to send and receive information via 
Frequency-shift keying

%package application-fsv
Summary:	FreeSWITCH mod_fsv
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-fsv
Provides FreeSWITCH mod_fsk, implements functions to record and play back video

%package application-hash
Summary:	FreeSWITCH mod_hash
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-hash
Provides FreeSWITCH mod_hash, implements an API and application interface for 
manipulating a hash table. It also provides a limit backend. 

%package application-httapi
Summary:	FreeSWITCH mod_httapi
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-httapi
Provides FreeSWITCH mod_httapi, provides an HTTP based Telephony API using a 
standard FreeSWITCH application interface as well as a cached http file format 
interface

%package application-http-cache
Summary:	FreeSWITCH mod_http_cache
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-http-cache
Provides FreeSWITCH mod_http_cache, allows one to make a HTTP GET request to 
cache a document. The primary use case is to download and cache audio files 
from a web server. 

%package application-lcr
Summary:	FreeSWITCH mod_lcr
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-lcr
Provides FreeSWITCH mod_lcr, provide basic Least Cost Routing Services

%package application-limit
Summary:	FreeSWITCH mod_limit
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-limit
Provides FreeSWITCH mod_limit, provide application to limit both concurrent and call per time period

%package application-memcache
Summary:	FreeSWITCH mod_memcache
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  libmemcached-devel

%description application-memcache
Provides FreeSWITCH mod_memcache, implements an API interface to memcached which
is a "high-performance, distributed memory object caching system, generic in 
nature, but intended for use in speeding up dynamic web applications by 
alleviating database load." 

%package application-mongo
Summary:	FreeSWITCH mod_mongo
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
BuildRequires:  mongo-c-driver-devel

%description application-mongo
Provides FreeSWITCH mod_mongo, which implements an API interface to mongodb.

%package application-nibblebill
Summary:	FreeSWITCH mod_nibblebill
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-nibblebill
Provides FreeSWITCH mod_nibblebill, provides a credit/debit module for 
FreeSWITCH to allow real-time debiting of credit or cash from a database 
while calls are in progress.

%package application-rad_auth
Summary:	FreeSWITCH mod_rad_auth
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-rad_auth
Provides FreeSWITCH mod_rad_auth, authentication via RADIUS protocol from FreeSWITCH dialplan

%package application-redis
Summary:	FreeSWITCH mod_redis
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-redis
Provides FreeSWITCH mod_redis, access to the redis key value pair db system from
FreeSWITCH

%package application-rss
Summary:	FreeSWITCH mod_rss
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-rss
Provides FreeSWITCH mod_rss, edisrse and read an XML based RSS feed, then read
the entries aloud via a TTS engine

%package application-signalwire
Summary:	FreeSWITCH mod_signalwire
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  libks signalwire-client-c

%description application-signalwire
Provides FreeSWITCH mod_signalwire

%package application-sms
Summary:	FreeSWITCH mod_sms
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-sms
Provides FreeSWITCH mod_sms, provide a way to route messages in freeswitch, 
potentially allowing one to build a powerful chatting system like in XMPP using 
using SIP SIMPLE on SIP clients

%package application-snapshot
Summary:	FreeSWITCH mod_snapshot
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-snapshot
Provides FreeSWITCH mod_snapshot, allows recording a sliding window of audio 
and taking snapshots to disk. 

%package application-snom
Summary:	FreeSWITCH mod_snom
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-snom
Provides FreeSWITCH mod_snom, an application for controlling the functionality 
and appearance of the programmable softkeys on Snom phones

%package application-soundtouch
Summary:	FreeSWITCH mod_soundtouch
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  soundtouch-devel >= 1.7.1

%description application-soundtouch
Provides FreeSWITCH mod_soundtouch, uses the soundtouch library, which can do
pitch shifting and other audio effects, so you can pipe the audio of a call
(or any other channel audio) through this module and achieve those effects. You
can specifically adjust pitch, rate, and tempo.

%package application-spy
Summary:	FreeSWITCH mod_spy
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-spy
Provides FreeSWITCH mod_spy, implements userspy application which provides 
persistent eavesdrop on all channels bridged to a certain user

%package application-stress
Summary:	FreeSWITCH mod_stress
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-stress
Provides FreeSWITCH mod_stress. mod_stress attempts to detect stress in a 
person's voice and generates FreeSWITCH events based on that data. 

%package application-translate
Summary:	FreeSWITCH mod_translate
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-translate
Provide an number translation to FreeSWITCH API calls

%package application-valet_parking
Summary:	FreeSWITCH mod_valet_parking
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-valet_parking
Provides FreeSWITCH mod_valet_parking. Provides 'Call Parking' in the switch
as opposed to on the phone and allows for a number of options to handle call
retrieval

%package application-video_filter
Summary:	FreeSWITCH video filter bugs
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-video_filter
Provide a chromakey video filter media bug

%package application-voicemail
Summary:	FreeSWITCH mod_voicemail
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-voicemail
Provides FreeSWITCH mod_voicemail. Implements Voicemail Application 

%package application-voicemail-ivr
Summary:	FreeSWITCH mod_voicemail_ivr
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description application-voicemail-ivr
Provides FreeSWITCH mod_voicemail_ivr. Provides a custimizable audio navigation 
system for backend voicemail systems

######################################################################################################################
#				FreeSWITCH ASR TTS Modules
######################################################################################################################

%package asrtts-flite
Summary:	FreeSWITCH mod_flite
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Requires:       flite >= 2.0.0
BuildRequires:  flite-devel >= 2.0.0

%description asrtts-flite
Provides FreeSWITCH mod_flite, a interface to the flite text to speech engine

%package asrtts-pocketsphinx
Summary:	FreeSWITCH mod_pocketsphinx
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  bison

%description asrtts-pocketsphinx
Provides FreeSWITCH mod_pocketsphinx, a interface to the OpenSource 
Pocketsphinx speech recognition engine

%package asrtts-tts-commandline
Summary:	FreeSWITCH mod_tts_commandline
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description asrtts-tts-commandline
Provides FreeSWITCH mod_tts_commandline, Run a command line and play the 
output file.

%package asrtts-unimrcp
Summary:	FreeSWITCH mod_unimrcp
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description asrtts-unimrcp
Provides FreeSWITCH mod_unimrcp, allows communication with Media Resource 
Control Protocol (MRCP) servers

######################################################################################################################
#				FreeSWITCH Codec Modules
######################################################################################################################

%package codec-passthru-amr
Summary:        Pass-through AMR Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Conflicts:	codec-amr

%description codec-passthru-amr
Pass-through AMR Codec support for FreeSWITCH open source telephony platform

%package codec-passthru-amrwb
Summary:        Pass-through AMR WideBand Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Conflicts:      codec-amrwb

%description codec-passthru-amrwb
Pass-through AMR WideBand Codec support for FreeSWITCH open source telephony platform

%package codec-bv
Summary:        BroadVoice16 and BroadVoice32 WideBand Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  broadvoice-devel

%description codec-bv
BroadVoice16 and BroadVoice32 WideBand Codec support for FreeSWITCH open source telephony platform

%package codec-codec2
Summary:        Codec2 Narrow Band Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  codec2-devel

%description codec-codec2
CODEC2 narrow band codec support for FreeSWITCH open source telephony platform.
CODEC2 was created by the developers of Speex.

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
Conflicts:	codec-com-g729

%description codec-passthru-g729
Pass-through g729 Codec support for FreeSWITCH open source telephony platform

%package codec-h26x
Summary:        H.263/H.264 Video Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description codec-h26x
H.263/H.264 Video Codec support for FreeSWITCH open source telephony platform

%package codec-ilbc
Summary:        iLCB Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Requires:       ilbc2
BuildRequires:  ilbc2-devel 


%description codec-ilbc
iLBC Codec support for FreeSWITCH open source telephony platform

%package codec-isac
Summary:        iSAC Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description codec-isac
iSAC Codec support for FreeSWITCH open source telephony platform

%package codec-vpx
Summary:        vp8 Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description codec-vpx
iSAC Codec support for FreeSWITCH open source telephony platform

%package codec-mp4v
Summary:        MP4V Video Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description codec-mp4v
MP4V Video Codec support for FreeSWITCH open source telephony platform

%package codec-opus
Summary:        Opus Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Requires:       opus >= 1.1
BuildRequires:  opus-devel >= 1.1

%description codec-opus
OPUS Codec support for FreeSWITCH open source telephony platform

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

%package codec-silk
Summary:        Silk Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  libsilk-devel

%description codec-silk
Silk Codec (from Skype) support for FreeSWITCH open source telephony platform

%package codec-siren
Summary:        Siren Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  g722_1-devel

%description codec-siren
Siren Codec support for FreeSWITCH open source telephony platform. Using 
mod_siren in a commercial product will require you to acquire a patent license
directly from Polycom(R) for your company. 
see http://www.polycom.com/usa/en/company/about_us/technology/siren_g7221/siren_g7221.html 
and http://www.polycom.com/usa/en/company/about_us/technology/siren14_g7221c/siren14_g7221c.html 
At the time of this packaging, Polycom does not charge for licensing.

%package codec-theora
Summary:        Theora Video Codec support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description codec-theora
Theora Video Codec support for FreeSWITCH open source telephony platform.

######################################################################################################################
#				FreeSWITCH Database Modules
######################################################################################################################

%package database-mariadb
Summary:	MariaDB native support for FreeSWITCH
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	mariadb-connector-c
BuildRequires:	mariadb-connector-c-devel

%description database-mariadb
MariaDB native support for FreeSWITCH.

%package database-pgsql
Summary:	PostgreSQL native support for FreeSWITCH
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	postgresql-libs
BuildRequires:	postgresql-devel

%description database-pgsql
PostgreSQL native support for FreeSWITCH.

######################################################################################################################
#				FreeSWITCH Directory Modules
######################################################################################################################

#%package directory-ldap
#Summary:        LDAP Directory support for FreeSWITCH open source telephony platform
#Group:          System/Libraries
#Requires:       %{name} = %{version}-%{release}

#%description directory-ldap
#LDAP Directory support for FreeSWITCH open source telephony platform.

######################################################################################################################
#				FreeSWITCH Endpoint Modules
######################################################################################################################

%package endpoint-dingaling
Summary:        Generic XMPP support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description endpoint-dingaling
XMPP support for FreeSWITCH open source telephony platform. Allows FreeSWITCH
to be used as a client for GoogleTalk or other XMPP Servers.

#%package endpoint-gsmopen
#Summary:        Generic GSM endpoint support for FreeSWITCH open source telephony platform
#Group:          System/Libraries
#Requires:       %{name} = %{version}-%{release}
#
#%description endpoint-gsmopen
#GSMopen is an endpoint (channel driver) that allows an SMS to be sent or 
#received from FreeSWITCH as well as incoming and outgoing GSM voice calls.
#SMS is handled via the standard CHAT API in FreeSWITCH.

#%package endpoint-h323
#Summary:        H.323 endpoint support for FreeSWITCH open source telephony platform
#Group:          System/Libraries
#Requires:       %{name} = %{version}-%{release}
#
#%description endpoint-h323
#H.323 endpoint support for FreeSWITCH open source telephony platform

#%package endpoint-khomp
#Summary:        khomp endpoint support for FreeSWITCH open source telephony platform
#Group:          System/Libraries
#Requires:       %{name} = %{version}-%{release}
#
#%description endpoint-khomp
#Khomp hardware endpoint support for FreeSWITCH open source telephony platform.

%package endpoint-portaudio
Summary:        PortAudio endpoint support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Requires:	alsa-lib
BuildRequires:	alsa-lib-devel
BuildRequires:	portaudio-devel

%description endpoint-portaudio
PortAudio endpoint support for FreeSWITCH open source telephony platform.

%package endpoint-rtmp
Summary:        RTPM Endpoint support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description endpoint-rtmp
RTMP Endpoint support for FreeSWITCH open source telephony platform. Allows FreeSWITCH
to be used from a RTMP client. See http://wiki.freeswitch.org/wiki/Mod_rtmp#Flex_Client
for the OpenSouce FreeSWITCH backed Client.

%package endpoint-skinny
Summary:        Skinny/SCCP endpoint support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description endpoint-skinny
SCCP/Skinny support for FreeSWITCH open source telephony platform.

%package endpoint-verto
Summary:        Verto endpoint support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description endpoint-verto
Verto protocol support for FreeSWITCH open source telephony platform.

%package endpoint-rtc
Summary:        Verto endpoint support for FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description endpoint-rtc
Verto protocol support for FreeSWITCH open source telephony platform.

######################################################################################################################
#				FreeSWITCH Event Handler Modules
######################################################################################################################

%package event-cdr-mongodb
Summary:	MongoDB CDR Logger for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:       %{name} = %{version}-%{release}
BuildRequires:  mongo-c-driver-devel

%description event-cdr-mongodb
MongoDB CDR Logger for FreeSWITCH

%package event-cdr-pg-csv
Summary:	PostgreSQL CDR Logger for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	postgresql-libs
BuildRequires:	postgresql-devel

%description event-cdr-pg-csv
PostgreSQL CDR Logger for FreeSWITCH.

%package event-cdr-sqlite
Summary:	SQLite CDR Logger for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	 %{name} = %{version}-%{release}

%description event-cdr-sqlite
SQLite CDR Logger for FreeSWITCH.

%package event-erlang-event
Summary:	Erlang Event Module for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	 %{name} = %{version}-%{release}
Requires:	erlang
BuildRequires:	erlang

%description event-erlang-event
Erlang Event Module for FreeSWITCH.

%package event-format-cdr
Summary:        JSON and XML Logger for the FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description event-format-cdr
JSON and XML Logger for the FreeSWITCH open source telephony platform

%package kazoo
Summary:	Kazoo Module for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	erlang
BuildRequires:	erlang

%description kazoo
Kazoo Module for FreeSWITCH.

%package event-multicast
Summary:	Multicast Event System for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description event-multicast
Multicast Event System for FreeSWITCH.

#%package event-zmq
#Summary:	ZeroMQ Event System for the FreeSWITCH open source telephony platform
#Group:		System/Libraries
#Requires:	 %{name} = %{version}-%{release}
#
#%description event-zmq
#ZeroMQ Event System for FreeSWITCH.

%package event-json-cdr
Summary:	JSON CDR Logger for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description event-json-cdr
JSON CDR Logger for FreeSWITCH.

%package event-radius-cdr
Summary:        RADIUS Logger for the FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description event-radius-cdr
RADIUS Logger for the FreeSWITCH open source telephony platform

%if %{build_mod_rayo}
%package event-rayo
Summary:        Rayo (XMPP 3PCC) server for the FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:	%{name} = %{version}-%{release}

%description event-rayo
Rayo 3PCC for FreeSWITCH.  http://rayo.org   http://xmpp.org/extensions/xep-0327.html
Rayo is an XMPP protocol extension for third-party control of telephone calls.
%endif

%package event-snmp
Summary:	SNMP stats reporter for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	net-snmp
BuildRequires:	net-snmp-devel

%description event-snmp
SNMP stats reporter for the FreeSWITCH open source telephony platform

######################################################################################################################
#				FreeSWITCH Logger Modules
######################################################################################################################

%package logger-graylog2
Summary:	GELF logger for Graylog2 and Logstash
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description logger-graylog2
GELF logger for Graylog2 and Logstash

######################################################################################################################
#				FreeSWITCH Media Format Modules
######################################################################################################################

%package format-local-stream
Summary:	Local File Streamer for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description format-local-stream
Local File Streamer for FreeSWITCH. It streams files from a directory and 
multiple channels connected to the same stream will hear the same (looped) 
file playback .. similar to a shoutcast stream. Useful for Music-on-hold type 
scenarios. 

%package format-native-file
Summary:	Native Media File support for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description format-native-file
The native file module is designed to make it easy to play sound files where no
transcoding is necessary. The default FreeSWITCH sound files are in wav format.
Generally, these require transcoding when being played to callers. However, if
a native format sound file is available then FreeSWITCH can use it. 

%package format-portaudio-stream
Summary:	PortAudio Media Steam support for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
BuildRequires:	portaudio-devel

%description format-portaudio-stream
Portaudio Streaming interface Audio for FreeSWITCH

%package format-shell-stream
Summary:	Implements Media Steaming from arbitrary shell commands for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description format-shell-stream
Mod shell stream is a FreeSWITCH module to allow you to stream audio from an 
arbitrary shell command. You could use it to read audio from a database, from 
a soundcard, etc. 

%package format-mod-shout
Summary:	Implements Media Steaming from arbitrary shell commands for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	libshout >= 2.2.2
Requires:	libmpg123 >= 1.20.1
Requires:	lame
BuildRequires:	libshout-devel >= 2.2.2
BuildRequires:	libmpg123-devel >= 1.20.1
BuildRequires:	lame-devel

%description format-mod-shout
Mod Shout is a FreeSWITCH module to allow you to stream audio from MP3s or a i
shoutcast stream.

%package format-opusfile
Summary:	Plays Opus encoded files
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	opusfile >= 0.5
BuildRequires:	opusfile-devel >= 0.5

%description format-opusfile
Mod Opusfile is a FreeSWITCH module to allow you to play Opus encoded files

%if %{build_mod_ssml}
%package format-ssml
Summary:        Adds Speech Synthesis Markup Language (SSML) parser format for the FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description format-ssml
mod_ssml is a FreeSWITCH module that renders SSML into audio.  This module requires a text-to-speech module for speech synthesis.
%endif

%package format-tone-stream
Summary:	Implements TGML Tone Generation for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description format-tone-stream
Implements TGML Tone Generation for the FreeSWITCH open source telephony platform

######################################################################################################################
#				FreeSWITCH Programming Language Modules
######################################################################################################################

%package lua
Summary:	Lua support for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
BuildRequires:  lua-devel

%description	lua

%package	perl
Summary:	Perl support for the FreeSWITCH open source telephony platform
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	perl
BuildRequires:	perl-devel
BuildRequires:	perl-ExtUtils-Embed

%description	perl

%package        python
Summary:        Python support for the FreeSWITCH open source telephony platform
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}
Requires:       python
BuildRequires:  python-devel

%description    python

%if %{build_mod_v8}
%package v8
Summary:	JavaScript support for the FreeSWITCH open source telephony platform, using Google V8 JavaScript engine
Group:		System/Libraries
Requires:	%{name} = %{version}-%{release}

%description v8
%endif

######################################################################################################################
#				FreeSWITCH Say Modules
######################################################################################################################

%package lang-en
Summary:	Provides english language dependent modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-en
English language phrases module and directory structure for say module and voicemail

%package lang-ru
Summary:        Provides russian language dependent modules and speech config for the FreeSWITCH Open Source telephone platform.
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

%package lang-es
Summary:        Provides Spanish language dependend modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-es
Spanish language phrases module and directory structure for say module and voicemail

%package lang-pt
Summary:        Provides Portuguese language dependend modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-pt
Portuguese language phrases module and directory structure for say module and voicemail

%package lang-sv
Summary:        Provides Swedish language dependend modules and speech config for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description lang-sv
Swedish language phrases module and directory structure for say module and voicemail

######################################################################################################################
#				FreeSWITCH Timer Modules
######################################################################################################################

%package timer-posix
Summary:        Provides posix timer for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description timer-posix
Provides posix timer for the FreeSWITCH Open Source telephone platform.

%if %{build_timerfd}
%package timer-timerfd
Summary:        Provides Linux Timerfs based timer for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description timer-timerfd
Provides Linux Timerfs based timer for the FreeSWITCH Open Source telephone 
platform.
%endif

######################################################################################################################
#				FreeSWITCH XML INT Modules
######################################################################################################################

%package xml-cdr
Summary:        Provides XML CDR interface for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description xml-cdr
Provides XML CDR interface for the FreeSWITCH Open Source telephone platform.

%package xml-curl
Summary:        Provides XML Curl interface for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:       %{name} = %{version}-%{release}

%description xml-curl
Provides XML Curl interface for the FreeSWITCH Open Source telephone platform.
Pull dynamic XML configs for FreeSWITCH over HTTP.

%package xml-rpc
Summary:        Provides XML-RPC interface for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:        %{name} = %{version}-%{release}

%description xml-rpc
Provides XML-RPC interface for the FreeSWITCH Open Source telephone platform.

######################################################################################################################
#			FreeSWITCH ESL language modules
######################################################################################################################

%package	-n perl-ESL
Summary:	The Perl ESL module allows for native interaction with FreeSWITCH over the event socket interface.
Group:		System Environment/Libraries

%description	-n perl-ESL
The Perl ESL module allows for native interaction with FreeSWITCH over the event socket interface.

%package	-n python-ESL
Summary:	The Python ESL module allows for native interaction with FreeSWITCH over the event socket interface.
Group:		System Environment/Libraries
Requires:	python
BuildRequires:	python-devel

%description	-n python-ESL
The Python ESL module allows for native interaction with FreeSWITCH over the event socket interface.

######################################################################################################################
#				FreeSWITCH basic config module
######################################################################################################################

%package config-vanilla
Summary:        Basic vanilla config set for the FreeSWITCH Open Source telephone platform.
Group:          System/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	freeswitch-application-abstraction
Requires:	freeswitch-application-avmd
Requires:	freeswitch-application-blacklist
Requires:	freeswitch-application-callcenter
Requires:	freeswitch-application-cidlookup
Requires:	freeswitch-application-conference
Requires:	freeswitch-application-curl
Requires:	freeswitch-application-db
Requires:	freeswitch-application-directory
Requires:	freeswitch-application-distributor
Requires:	freeswitch-application-easyroute
Requires:	freeswitch-application-enum
Requires:	freeswitch-application-esf
Requires:	freeswitch-application-expr
Requires:	freeswitch-application-fifo
Requires:	freeswitch-application-fsk
Requires:	freeswitch-application-fsv
Requires:	freeswitch-application-hash
Requires:	freeswitch-application-httapi
Requires:	freeswitch-application-http-cache
Requires:	freeswitch-application-lcr
Requires:	freeswitch-application-limit
Requires:	freeswitch-application-memcache
Requires:	freeswitch-application-nibblebill
Requires:	freeswitch-application-redis
Requires:	freeswitch-application-rss
Requires:	freeswitch-application-signalwire
Requires:	freeswitch-application-sms
Requires:	freeswitch-application-snapshot
Requires:	freeswitch-application-snom
Requires:	freeswitch-application-soundtouch
Requires:	freeswitch-application-spy
Requires:	freeswitch-application-stress
Requires:	freeswitch-application-valet_parking
Requires:	freeswitch-application-video_filter
Requires:	freeswitch-application-voicemail
Requires:	freeswitch-application-voicemail-ivr
Requires:	freeswitch-codec-passthru-amr
Requires:	freeswitch-codec-bv
Requires:	freeswitch-codec-passthru-g723_1
Requires:	freeswitch-codec-passthru-g729
Requires:	freeswitch-codec-h26x
Requires:	freeswitch-codec-ilbc
Requires:	freeswitch-codec-siren
Requires:	freeswitch-database-pgsql
Requires:	freeswitch-format-local-stream
Requires:	freeswitch-format-native-file
Requires:	freeswitch-format-portaudio-stream
Requires:	freeswitch-format-tone-stream
Requires:	freeswitch-lang-en

%description config-vanilla
Basic vanilla config set for the FreeSWITCH Open Source telephone platform.

######################################################################################################################
#
#				Unpack and prepare Source archives, copy stuff around etc ..
#
######################################################################################################################

%prep
%setup -b0 -q -n %{name}-%{nonparsedversion}
cp %{SOURCE1} libs/
cp %{SOURCE2} libs/
cp %{SOURCE3} libs/
cp %{SOURCE4} libs/

#Hotfix for redefined %_sysconfdir
sed -ie 's:confdir="${sysconfdir}/freeswitch":confdir="$sysconfdir":' ./configure.ac

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
APPLICATION_MODULES_AC="applications/mod_abstraction applications/mod_avmd applications/mod_blacklist \
			applications/mod_callcenter  applications/mod_cidlookup \
			applications/mod_commands applications/mod_conference applications/mod_curl"
APPLICATION_MODULES_DE="applications/mod_db applications/mod_directory applications/mod_distributor \
			applications/mod_dptools applications/mod_easyroute applications/mod_enum applications/mod_esf \
			applications/mod_expr "

%if %{build_mod_esl}
APPLICATION_MODULES_DE+="applications/mod_esl"
%endif

APPLICATION_MODULES_FR="applications/mod_fifo applications/mod_fsk applications/mod_fsv applications/mod_hash \
			applications/mod_httapi applications/mod_http_cache applications/mod_lcr applications/mod_limit \
			applications/mod_memcache applications/mod_mongo applications/mod_nibblebill applications/mod_rad_auth \
			applications/mod_redis applications/mod_rss "

APPLICATION_MODULES_SZ="applications/mod_signalwire applications/mod_sms applications/mod_snapshot applications/mod_snom applications/mod_soundtouch \
			applications/mod_spandsp applications/mod_spy applications/mod_stress \
			applications/mod_valet_parking applications/mod_translate applications/mod_voicemail \
			applications/mod_voicemail_ivr applications/mod_video_filter"

APPLICATIONS_MODULES="$APPLICATION_MODULES_AC $APPLICATION_MODULES_DE $APPLICATION_MODULES_FR $APPLICATION_MODULES_SZ"

######################################################################################################################
#
#				Automatic Speech Recognition and Text To Speech Modules
#
######################################################################################################################
ASR_TTS_MODULES="asr_tts/mod_flite asr_tts/mod_pocketsphinx asr_tts/mod_tts_commandline asr_tts/mod_unimrcp"

######################################################################################################################
#
#						Codecs
#
######################################################################################################################
CODECS_MODULES="codecs/mod_amr codecs/mod_amrwb codecs/mod_bv codecs/mod_codec2 codecs/mod_g723_1 \
		codecs/mod_g729 codecs/mod_h26x codecs/mod_ilbc codecs/mod_isac codecs/mod_mp4v codecs/mod_opus codecs/mod_silk \
		codecs/mod_siren codecs/mod_theora"
#
%if %{build_sng_tc}
CODECS_MODULES+="codecs/mod_sangoma_codec"
%endif

######################################################################################################################
#
#					Database Modules
#
######################################################################################################################
DATABASES_MODULES="databases/mod_mariadb databases/mod_pgsql"

######################################################################################################################
#
#					Dialplan Modules
#
######################################################################################################################
DIALPLANS_MODULES="dialplans/mod_dialplan_directory dialplans/mod_dialplan_xml"
#DISABLED DIALPLANS dialplans/mod_dialplan_asterisk 
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
ENDPOINTS_MODULES="endpoints/mod_dingaling \
			endpoints/mod_loopback endpoints/mod_portaudio endpoints/mod_rtmp \
			endpoints/mod_skinny endpoints/mod_verto endpoints/mod_rtc endpoints/mod_sofia"

## DISABLED MODULES DUE TO BUILD ISSUES endpoints/mod_gsmopen endpoints/mod_h323 endpoints/mod_khomp 
 
######################################################################################################################
#
#						Event Handlers
#
######################################################################################################################
EVENT_HANDLERS_MODULES="event_handlers/mod_cdr_csv event_handlers/mod_cdr_pg_csv event_handlers/mod_cdr_sqlite \
			event_handlers/mod_cdr_mongodb event_handlers/mod_format_cdr event_handlers/mod_erlang_event event_handlers/mod_event_multicast \
			event_handlers/mod_event_socket event_handlers/mod_json_cdr event_handlers/mod_kazoo event_handlers/mod_radius_cdr \
			event_handlers/mod_snmp"
%if %{build_mod_rayo}
EVENT_HANDLERS_MODULES+=" event_handlers/mod_rayo"
%endif

#### BUILD ISSUES NET RESOLVED FOR RELEASE event_handlers/mod_event_zmq 
######################################################################################################################
#
#					File and Audio Format Handlers
#
######################################################################################################################
FORMATS_MODULES="formats/mod_local_stream formats/mod_native_file formats/mod_opusfile formats/mod_portaudio_stream \
                 formats/mod_shell_stream formats/mod_shout formats/mod_sndfile formats/mod_tone_stream"
%if %{build_mod_ssml}
FORMATS_MODULES+=" formats/mod_ssml"
%endif

######################################################################################################################
#
#						Embedded Languages
#
######################################################################################################################
LANGUAGES_MODULES="languages/mod_lua languages/mod_perl languages/mod_python "
%if %{build_mod_v8}
LANGUAGES_MODULES+="languages/mod_v8"
%endif

######################################################################################################################
#
#						Logging Modules
#
######################################################################################################################
LOGGERS_MODULES="loggers/mod_console loggers/mod_graylog2 loggers/mod_logfile loggers/mod_syslog"

######################################################################################################################
#
#						Phrase engine language modules
#
######################################################################################################################
SAY_MODULES="say/mod_say_de say/mod_say_en say/mod_say_es say/mod_say_pt say/mod_say_fr say/mod_say_he say/mod_say_ru say/mod_say_sv"

######################################################################################################################
#
#							Timers
#
######################################################################################################################
TIMERS_MODULES="timers/mod_posix_timer "
%if %{build_timerfd}
TIMERS_MODULES+="timers/mod_timerfd"
%endif

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
MYMODULES="$APPLICATIONS_MODULES $CODECS_MODULES $DATABASES_MODULES $DIALPLANS_MODULES $DIRECTORIES_MODULES \
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

%if 0%{?rhel} == 7
# to build mod_mariadb we need gcc >= 4.9 (more details GH #1046)
export CFLAGS="$CFLAGS -Wno-error=expansion-to-defined"
. /opt/rh/devtoolset-9/enable
%endif
%if 0%{?rhel} == 8
# we want use fresh gcc on RHEL 8 based dists
. /opt/rh/gcc-toolset-9/enable
%endif

######################################################################################################################
#
#				Bootstrap, Configure and Build the whole enchilada
#
######################################################################################################################

if test -f bootstrap.sh
then 
   ./bootstrap.sh
else
   ./rebootstrap.sh
fi

autoreconf --force --install

%configure -C \
--prefix=%{PREFIX} \
--exec-prefix=%{EXECPREFIX} \
--bindir=%{BINDIR} \
--sbindir=%{SBINDIR} \
--libexecdir=%{LIBEXECDIR} \
--sharedstatedir=%{SHARESTATEDIR} \
--localstatedir=%{_localstatedir} \
--libdir=%{LIBDIR} \
--includedir=%{INCLUDEDIR} \
--datadir=%{DATADIR} \
--infodir=%{INFODIR} \
--mandir=%{MANDIR} \
--with-logfiledir=%{LOGFILEDIR} \
--with-modinstdir=%{MODINSTDIR} \
--with-rundir=%{RUNDIR} \
--with-dbdir=%{DBDIR} \
--with-htdocsdir=%{HTDOCSDIR} \
--with-soundsdir=%{SOUNDSDIR} \
--enable-core-odbc-support \
--enable-core-libedit-support \
--with-grammardir=%{GRAMMARDIR} \
--with-scriptdir=%{SCRIPTDIR} \
--with-recordingsdir=%{RECORDINGSDIR} \
--with-pkgconfigdir=%{PKGCONFIGDIR} \
--with-odbc \
--with-erlang \
--with-openssl \
--enable-zrtp \
%{?configure_options}

unset MODULES
%{__make}

cd libs/esl
%{__make} pymod
%{__make} perlmod


######################################################################################################################
#
#				Install it and create some required dirs and links
#
######################################################################################################################
%install
%if 0%{?rhel} == 7
# to build mod_mariadb we need gcc >= 4.9
. /opt/rh/devtoolset-9/enable
%endif
%if 0%{?rhel} == 8
# we want use fresh gcc on RHEL 8 based dists
. /opt/rh/gcc-toolset-9/enable
%endif


%{__make} DESTDIR=%{buildroot} install

# Create a log dir
%{__mkdir} -p %{buildroot}%{prefix}/log
%{__mkdir} -p %{buildroot}%{logfiledir}
%{__mkdir} -p %{buildroot}%{runtimedir}
%{__mkdir} -p %{buildroot}%{_localstatedir}/cache/freeswitch

#install the esl stuff
cd libs/esl
%{__make} DESTDIR=%{buildroot} pymod-install
%{__make} DESTDIR=%{buildroot} perlmod-install

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
%if "%{?_unitdir}" == ""
# On RedHat like
%{__install} -D -m 0755 build/freeswitch.init.redhat %{buildroot}/etc/rc.d/init.d/freeswitch
%else
# systemd
%{__install} -Dpm 0644 build/freeswitch.service %{buildroot}%{_unitdir}/freeswitch.service
%{__install} -Dpm 0644 build/freeswitch-tmpfiles.conf %{buildroot}%{_tmpfilesdir}/freeswitch.conf
%endif
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
%{__rm} -f %{buildroot}/%{MODINSTDIR}/ftmod_sangoma_ss7*
%endif
%if %{build_sng_isdn}
#do not delete a thing
%else
%{__rm} -f %{buildroot}/%{MODINSTDIR}/ftmod_sangoma_isdn*
%endif



######################################################################################################################
#
#			Add a freeswitch user with group daemon that will own the whole enchilada
#
######################################################################################################################
%pre
%ifos linux
if ! /usr/bin/id freeswitch &>/dev/null; then
       /usr/sbin/useradd -r -g daemon -s /bin/false -c "The FreeSWITCH Open Source Voice Platform" -d %{LOCALSTATEDIR} freeswitch || \
                %logmsg "Unexpected error adding user \"freeswitch\". Aborting installation."
fi
%endif

%post
%{?run_ldconfig:%run_ldconfig}
# Make FHS2.0 happy
# %{__mkdir} -p /etc/opt
# %{__ln_s} -f %{sysconfdir} /etc%{prefix}

chown freeswitch:daemon /var/log/freeswitch /var/run/freeswitch

%if "%{?_unitdir}" == ""
chkconfig --add freeswitch
%else
%tmpfiles_create freeswitch
/usr/bin/systemctl -q enable freeswitch.service
%endif

%preun
%{?systemd_preun freeswitch.service}

%postun
%{?systemd_postun freeswitch.service}
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
%defattr(-,root,root)

######################################################################################################################
#
#							Directories
#
######################################################################################################################
#
#################################### Basic Directory Structure #######################################################
#
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}
%dir %attr(0750, freeswitch, daemon) %{LOCALSTATEDIR}
%dir %attr(0750, freeswitch, daemon) %{LOCALSTATEDIR}/images
%dir %attr(0750, freeswitch, daemon) %{DBDIR}
%dir %attr(0755, -, -) %{GRAMMARDIR}
%dir %attr(0755, -, -) %{HTDOCSDIR}
%dir %attr(0750, freeswitch, daemon) %{logfiledir}
%dir %attr(0750, freeswitch, daemon) %{runtimedir}
%dir %attr(0755, -, -) %{SCRIPTDIR}
#
#################################### Config Directory Structure #######################################################
#
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/autoload_configs
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/dialplan
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/dialplan/default
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/dialplan/public
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/dialplan/skinny-patterns
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/directory
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/directory/default
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/jingle_profiles
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/mrcp_profiles
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/sip_profiles
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/sip_profiles/external
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/sip_profiles/external-ipv6
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/skinny_profiles
#
#################################### Grammar Directory Structure #####################################################
#
%dir %attr(0755, -, -) %{GRAMMARDIR}/model
%dir %attr(0755, -, -) %{GRAMMARDIR}/model/communicator

######################################################################################################################
#
#						Other Files
#
######################################################################################################################
%config(noreplace) %attr(0644,-,-) %{HTDOCSDIR}/*
%ifos linux
%if "%{?_unitdir}" == ""
/etc/rc.d/init.d/freeswitch
%else
%{_unitdir}/freeswitch.service
%{_tmpfilesdir}/freeswitch.conf
%endif
%config(noreplace) /etc/sysconfig/freeswitch
%if 0%{?suse_version} > 100
/usr/sbin/rcfreeswitch
%endif
%endif
%ifos linux
%dir %attr(0750,-,-) /etc/monit.d
%config(noreplace) %attr(0644,-,-) /etc/monit.d/freeswitch.monitrc
%endif
%{LOCALSTATEDIR}/images/*

######################################################################################################################
#
#						Binaries
#
######################################################################################################################
%attr(0755,-,-) %{prefix}/bin/*
%{LIBDIR}/libfreeswitch*.so*
######################################################################################################################
#
#			Modules in Alphabetical Order, please keep them that way..
#
######################################################################################################################
%{MODINSTDIR}/mod_cdr_csv.so*
%{MODINSTDIR}/mod_console.so*
%{MODINSTDIR}/mod_commands.so*
%{MODINSTDIR}/mod_dialplan_directory.so* 
%{MODINSTDIR}/mod_dialplan_xml.so* 
%{MODINSTDIR}/mod_dptools.so*
%{MODINSTDIR}/mod_event_socket.so*
%{MODINSTDIR}/mod_logfile.so*
%{MODINSTDIR}/mod_loopback.so*
%{MODINSTDIR}/mod_native_file.so*
%{MODINSTDIR}/mod_sndfile.so*
%{MODINSTDIR}/mod_sofia.so*
%{MODINSTDIR}/mod_spandsp.so*
%{MODINSTDIR}/mod_syslog.so*
%{MODINSTDIR}/mod_tone_stream.so*
%{MODINSTDIR}/mod_xml_rpc.so* 
######################################################################################################################
#
#						Package for the developer
#
######################################################################################################################
%files devel
%{LIBDIR}/*.a
%{LIBDIR}/*.la
%{PKGCONFIGDIR}/*
%{MODINSTDIR}/*.*a
%{INCLUDEDIR}/*.h
%{INCLUDEDIR}/test/*.h


######################################################################################################################
#						Vanilla Config Files
######################################################################################################################
%files config-vanilla
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/*.tpl
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/*.ttml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/extensions.conf
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/mime.types
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/abstraction.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/acl.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/amr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/amrwb.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/alsa.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/amqp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/av.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/avmd.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/blacklist.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/callcenter.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/cdr_csv.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/cdr_mongodb.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/cdr_pg_csv.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/cdr_sqlite.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/cepstral.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/cidlookup.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/conference.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/conference_layouts.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/console.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/curl.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/db.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/dialplan_directory.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/dingaling.conf.xml 
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/directory.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/distributor.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/easyroute.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/enum.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/erlang_event.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/event_multicast.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/event_socket.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/fax.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/fifo.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/format_cdr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/graylog2.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/hash.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/hiredis.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/httapi.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/http_cache.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/ivr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/java.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/kazoo.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/lcr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/local_stream.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/logfile.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/memcache.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/modules.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/mongo.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/msrp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/nibblebill.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/opal.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/oreka.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/osp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/pocketsphinx.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/portaudio.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/post_load_modules.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/pre_load_modules.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/presence_map.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/redis.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/rss.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/rtmp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/sangoma_codec.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/shout.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/skinny.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/smpp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/sms_flowroute.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/sndfile.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/sofia.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/spandsp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/switch.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/syslog.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/timezones.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/translate.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/tts_commandline.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/unicall.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/unimrcp.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/verto.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/voicemail.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/voicemail_ivr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/vpx.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/xml_cdr.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/xml_curl.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/xml_rpc.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/xml_scgi.conf.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/zeroconf.conf.xml
######################################################################################################################
#						Chatplans
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/chatplan/default.xml
######################################################################################################################
#						Dialplans
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/dialplan/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/dialplan/default/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/dialplan/public/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/dialplan/skinny-patterns/*.xml
######################################################################################################################
#						Fonts
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{_datadir}/freeswitch/fonts/*.ttf
%config(noreplace) %attr(0640, freeswitch, daemon) %{_datadir}/freeswitch/fonts/OFL.txt
%config(noreplace) %attr(0640, freeswitch, daemon) %{_datadir}/freeswitch/fonts/README.fonts
######################################################################################################################
#						User Directories
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/directory/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/directory/default/*
######################################################################################################################
#							IVR Menus
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/ivr_menus/*.xml
######################################################################################################################
#							Sip Profiles
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/sip_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/sip_profiles/external/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/sip_profiles/external-ipv6/*.xml
######################################################################################################################
#				Other Protocol Profiles (skinny, jingle, mrcp)
######################################################################################################################
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/skinny_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/jingle_profiles/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/mrcp_profiles/*.xml
######################################################################################################################
#						Grammar Files
######################################################################################################################
%config(noreplace) %attr(0644, -, -) %{GRAMMARDIR}/default.dic
%config(noreplace) %attr(0644, -, -) %{GRAMMARDIR}/model/communicator/*

### END OF config-vanilla

######################################################################################################################
#
#						Application Packages
#
######################################################################################################################
%files application-abstraction
%{MODINSTDIR}/mod_abstraction.so*

%files application-avmd
%{MODINSTDIR}/mod_avmd.so*

%files application-blacklist
%{MODINSTDIR}/mod_blacklist.so*

%files application-callcenter
%{MODINSTDIR}/mod_callcenter.so*

%files application-cidlookup
%{MODINSTDIR}/mod_cidlookup.so*

%files application-conference
%{MODINSTDIR}/mod_conference.so*

%files application-curl
%{MODINSTDIR}/mod_curl.so*

%files application-db
%{MODINSTDIR}/mod_db.so*

%files application-directory
%{MODINSTDIR}/mod_directory.so*

%files application-distributor
%{MODINSTDIR}/mod_distributor.so*

%files application-easyroute
%{MODINSTDIR}/mod_easyroute.so*

%files application-enum
%{MODINSTDIR}/mod_enum.so*

%files application-esf
%{MODINSTDIR}/mod_esf.so*

%if %{build_mod_esl}
%files application-esl
%{MODINSTDIR}/mod_esl.so*
%endif

%files application-expr
%{MODINSTDIR}/mod_expr.so*

%files application-fifo
%{MODINSTDIR}/mod_fifo.so*

%files application-fsk
%{MODINSTDIR}/mod_fsk.so*

%files application-fsv
%{MODINSTDIR}/mod_fsv.so*

%files application-hash
%{MODINSTDIR}/mod_hash.so*

%files application-httapi
%{MODINSTDIR}/mod_httapi.so*

%files application-http-cache
%dir %attr(0750, freeswitch, daemon) %{_localstatedir}/cache/freeswitch
%{MODINSTDIR}/mod_http_cache.so*

%files application-lcr
%{MODINSTDIR}/mod_lcr.so*

%files application-limit
%{MODINSTDIR}/mod_limit.so*

%files application-memcache
%{MODINSTDIR}/mod_memcache.so*

%files application-mongo
%{MODINSTDIR}/mod_mongo.so*

%files application-nibblebill
%{MODINSTDIR}/mod_nibblebill.so*

%files application-rad_auth
%{MODINSTDIR}/mod_rad_auth.so*

%files application-redis
%{MODINSTDIR}/mod_redis.so*

%files application-rss
%{MODINSTDIR}/mod_rss.so*

%files application-signalwire
%{MODINSTDIR}/mod_signalwire.so*

%files application-sms
%{MODINSTDIR}/mod_sms.so*

%files application-snapshot
%{MODINSTDIR}/mod_snapshot.so*

%files application-snom
%{MODINSTDIR}/mod_snom.so*

%files application-soundtouch
%{MODINSTDIR}/mod_soundtouch.so*

%files application-spy
%{MODINSTDIR}/mod_spy.so*

%files application-stress
%{MODINSTDIR}/mod_stress.so*

%files application-translate
%{MODINSTDIR}/mod_translate.so*

%files application-valet_parking
%{MODINSTDIR}/mod_valet_parking.so*

%files application-video_filter
%{MODINSTDIR}/mod_video_filter.so*

%files application-voicemail
%{MODINSTDIR}/mod_voicemail.so*

%files application-voicemail-ivr
%{MODINSTDIR}/mod_voicemail_ivr.so*

######################################################################################################################
#
#						ASR TTS Packages
#
######################################################################################################################
%files asrtts-flite
%{MODINSTDIR}/mod_flite.so*

%files asrtts-pocketsphinx
%{MODINSTDIR}/mod_pocketsphinx.so*

%files asrtts-tts-commandline
%{MODINSTDIR}/mod_tts_commandline.so*

%files asrtts-unimrcp
%{MODINSTDIR}/mod_unimrcp.so*

######################################################################################################################
#
#						CODEC Packages
#
######################################################################################################################

%files codec-passthru-amr
%{MODINSTDIR}/mod_amr.so*

%files codec-passthru-amrwb
%{MODINSTDIR}/mod_amrwb.so*

%files codec-bv
%{MODINSTDIR}/mod_bv.so*

%files codec-codec2
%{MODINSTDIR}/mod_codec2.so*


%files codec-passthru-g723_1
%{MODINSTDIR}/mod_g723_1.so*

%files codec-passthru-g729
%{MODINSTDIR}/mod_g729.so*

%files codec-h26x
%{MODINSTDIR}/mod_h26x.so*

%files codec-ilbc
%{MODINSTDIR}/mod_ilbc.so*

%files codec-isac
%{MODINSTDIR}/mod_isac.so*

%files codec-mp4v
%{MODINSTDIR}/mod_mp4v.so*

%files codec-opus
%{MODINSTDIR}/mod_opus.so*
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/opus.conf.xml

%if %{build_sng_tc}
%files sangoma-codec
%{MODINSTDIR}/mod_sangoma_codec.so*
%endif

%files codec-silk
%{MODINSTDIR}/mod_silk.so*

%files codec-siren
%{MODINSTDIR}/mod_siren.so*

%files codec-theora
%{MODINSTDIR}/mod_theora.so*

######################################################################################################################
#
#						FreeSWITCH Database Modules
#
######################################################################################################################

%files database-mariadb
%{MODINSTDIR}/mod_mariadb.so*

%files database-pgsql
%{MODINSTDIR}/mod_pgsql.so*

######################################################################################################################
#
#						FreeSWITCH Directory Modules
#
######################################################################################################################

#%files directory-ldap
#%{MODINSTDIR}/mod_ldap.so*

######################################################################################################################
#
#						FreeSWITCH endpoint Modules
#
######################################################################################################################

%files endpoint-dingaling
%{MODINSTDIR}/mod_dingaling.so*

#%files endpoint-gsmopen
#%{MODINSTDIR}/mod_gsmopen.so*

#%files endpoint-h323
#%{MODINSTDIR}/mod_h323.so*

#%files endpoint-khomp
#%{MODINSTDIR}/mod_khomp.so*

%files endpoint-portaudio
%{MODINSTDIR}/mod_portaudio.so*

%files endpoint-rtmp
%{MODINSTDIR}/mod_rtmp.so*

%files endpoint-skinny
%{MODINSTDIR}/mod_skinny.so*

%files endpoint-verto
%{MODINSTDIR}/mod_verto.so*

%files endpoint-rtc
%{MODINSTDIR}/mod_rtc.so*


######################################################################################################################
#
#					Event Modules
#
######################################################################################################################

%files event-cdr-mongodb
%{MODINSTDIR}/mod_cdr_mongodb.so*

%files event-cdr-pg-csv
%{MODINSTDIR}/mod_cdr_pg_csv.so*

%files event-cdr-sqlite
%{MODINSTDIR}/mod_cdr_sqlite.so*

%files event-erlang-event
%{MODINSTDIR}/mod_erlang_event.so*

%files event-format-cdr
%{MODINSTDIR}/mod_format_cdr.so*

%files event-multicast
%{MODINSTDIR}/mod_event_multicast.so*

#%files event-zmq
#%{MODINSTDIR}/mod_xmq.so*

%files event-json-cdr
%{MODINSTDIR}/mod_json_cdr.so*

%files kazoo
%{MODINSTDIR}/mod_kazoo.so*

%files event-radius-cdr
%{MODINSTDIR}/mod_radius_cdr.so*

%if %{build_mod_rayo}
%files event-rayo 
%{MODINSTDIR}/mod_rayo.so*
%endif

%files event-snmp
%{MODINSTDIR}/mod_snmp.so*

######################################################################################################################
#
#					Event Modules
#
######################################################################################################################

%files format-local-stream
%{MODINSTDIR}/mod_local_stream.so*

%files format-native-file
%{MODINSTDIR}/mod_native_file.so*

%files format-opusfile
%{MODINSTDIR}/mod_opusfile.so*

%files format-portaudio-stream
%{MODINSTDIR}/mod_portaudio_stream.so*

%files format-shell-stream
%{MODINSTDIR}/mod_shell_stream.so*

%files format-mod-shout
%{MODINSTDIR}/mod_shout.so*

%if %{build_mod_ssml}
%files format-ssml
%{MODINSTDIR}/mod_ssml.so*
%endif

%files format-tone-stream
%{MODINSTDIR}/mod_tone_stream.so*

######################################################################################################################
#
#					Embedded Language Modules
#
######################################################################################################################
%files lua
%{MODINSTDIR}/mod_lua*.so*
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/lua.conf.xml

%files perl
%{MODINSTDIR}/mod_perl*.so*
%{prefix}/perl/*
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/perl.conf.xml

%files python
%{MODINSTDIR}/mod_python*.so*
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/python.conf.xml

%if %{build_mod_v8}
%files v8
%{MODINSTDIR}/mod_v8*.so*
%{LIBDIR}/libv8.so
%{LIBDIR}/libicui18n.so
%{LIBDIR}/libicuuc.so
%endif
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/autoload_configs/v8.conf.xml

######################################################################################################################
#
#						Language Modules
#
######################################################################################################################
%files lang-en
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/en
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/en/demo
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/en/vm
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/en/dir
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/en/ivr
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/en/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/en/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/en/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/en/dir/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/en/ivr/*.xml
%{MODINSTDIR}/mod_say_en.so*

%files lang-de
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/de
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/de/demo
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/de/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/de/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/de/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/de/vm/*.xml
%{MODINSTDIR}/mod_say_de.so*

%files lang-fr
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/fr
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/fr/demo
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/fr/vm
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/fr/dir
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/fr/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/fr/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/fr/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/fr/dir/*.xml
%{MODINSTDIR}/mod_say_fr.so*

%files lang-ru
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/ru
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/ru/demo
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/ru/vm
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/ru/dir
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/ru/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/ru/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/ru/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/ru/dir/*.xml
%{MODINSTDIR}/mod_say_ru.so*

%files lang-he
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/he/
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/he/demo
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/he/vm
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/he/dir
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/he/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/he/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/he/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/he/dir/*.xml
%{MODINSTDIR}/mod_say_he.so*

%files lang-es
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/es
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/es/demo
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/es/vm
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/es/dir
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/es/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/es/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/es/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/es/dir/*.xml
%{MODINSTDIR}/mod_say_es.so*

%files lang-pt
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/pt
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/pt/demo
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/pt/vm
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/pt/dir
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/pt/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/pt/demo/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/pt/vm/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/pt/dir/*.xml
%{MODINSTDIR}/mod_say_pt.so*

%files lang-sv
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/sv
%dir %attr(0750, freeswitch, daemon) %{sysconfdir}/lang/sv/vm
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/sv/*.xml
%config(noreplace) %attr(0640, freeswitch, daemon) %{sysconfdir}/lang/sv/vm/*.xml
%{MODINSTDIR}/mod_say_sv.so*

######################################################################################################################
#
#						Logger Modules
#
######################################################################################################################

%files logger-graylog2
%{MODINSTDIR}/mod_graylog2.so*

######################################################################################################################
#
#					Timer Modules
#
######################################################################################################################

%files timer-posix
%{MODINSTDIR}/mod_posix_timer.so*

%if %{build_timerfd}
%files timer-timerfd
%{MODINSTDIR}/mod_timerfd.so*
%endif

######################################################################################################################
#
#					XMLINT  Modules
#
######################################################################################################################

%files xml-cdr
%{MODINSTDIR}/mod_xml_cdr.so*

%files xml-curl
%{MODINSTDIR}/mod_xml_curl.so*

######################################################################################################################
#			FreeSWITCH ESL language modules
######################################################################################################################

%files	-n perl-ESL
%defattr(644,root,root,755)
%{perl_archlib}/ESL.pm
%{perl_archlib}/ESL.so
%{perl_archlib}/ESL.la
%dir %{perl_archlib}/ESL
%{perl_archlib}/ESL/Dispatch.pm
%{perl_archlib}/ESL/IVR.pm

%files	-n python-ESL
%attr(0644, root, bin) /usr/lib*/python*/site-packages/freeswitch.py*
%attr(0755, root, bin) /usr/lib*/python*/site-packages/_ESL.so*
%attr(0755, root, bin) /usr/lib*/python*/site-packages/ESL.py*

######################################################################################################################
#
#						Changelog
#
######################################################################################################################
%changelog
* Fri Jan 31 2020 - Andrey Volk
- Add sndfile.conf.xml
* Tue Apr 23 2019 - Andrey Volk
- Fix build for Stack 20.x
* Tue Dec 11 2018 - Andrey Volk
- add mod_signalwire
* Sun Mar 13 2016 - Matthew Vale
- add perl and python ESL language module packages
* Thu Jul 09 2015 - Artur Zaprzała
- add systemd service file for CentOS 7
* Thu Jun 25 2015 - s.safarov@gmail.com
- Dependencies of mod_shout were declared
* Mon Jun 22 2015 - krice@freeswitch.org
- disable mod_shout until we can figure out the correct system deps for RPM based platforms
* Wed Jun 17 2015 - krice@freeswitch.org
- Update libvpx2 dep requirement
* Thu Jun 04 2015 - s.safarov@gmail.com
- Build dependences declared
- mod_rad_auth, mod_radius_cdr, mod_format_cdr modules declared
* Tue Nov 04 2014 - crienzo@grasshopper.com
- add mod_graylog2 and mod_mongo
* Thu Sep 11 2014 - krice@freeswitch.org
- add and fix mod_verto and mod_rtc
* Mon Jun 02 2014 - krice@freeswitch.org
- remove mod_spidermoney as its been deprecated
* Fri Feb 21 2014 - crienzo@grasshopper.com
- change file owner to root
* Wed Feb 19 2014 - crienzo@grasshopper.com
- remove mod_speex
* Sun Feb 02 2014 - jakob@mress.se
- add support for building Swedish say language module
* Mon Jan 13 2014 - peter@olssononline.se
- Add mod_v8
* Mon Dec 09 2013 - crienzo@grasshopper.com
- Add mod_ssml, mod_rayo
- Fix build on master
* Fri Jun 28 2013 - krice@freeswitch.org
- Add module for VP8
* Wed Jun 19 2013 - krice@freeswitch.org
- tweak files included for vanilla configs
* Wed Sep 19 2012 - krice@freeswitch.org
- Add support for Spanish and Portuguese say language modules
* Thu Jan 26 2012 - krice@freeswitch.org
- complete rework of spec file
* Tue Jun 14 2011 - michal.bielicki@seventhsignal.de
- added mod_http_cache
* Tue Jun 14 2011 - michal.bielicki@seventhsignal.de
- added mod_rtmp
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
- added python esl module to rpm
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
- some more formatting work
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
- Reorganized the modules alphabetically
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
- added mod_memcache
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
* Wed Nov 18 2009 - michal.bielicki@seventhsignal.de
- added new config files for directory and distributor
- removed sangoma boost from openzap for builds that do not inherit wanpipe while building.
* Fri Jul 24 2009 - mike@jerris.com
- removed mod_http
- removed ozmod_wanpipe
* Tue Jun 23 2009 - raulfragoso@gmail.com
- Adjusted for the latest SVN trunk (13912)
- Included new config and mod files to catch up with latest SVN
- Included new sound files for base256 and zrtp
- mod_unimrcp must be built after mod_sofia
* Tue Feb 17 2009 - michal.bielicki@halokwadrat.de
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
* Wed Dec 5 2007 - michal.bielicki@voiceworks.pl
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
- split off language dependent stuff into separate language files
- disable non complete language modules
* Tue Apr 24 2007 - peter+rpmspam@suntel.com.tr
- Added a debug package
- Split the passthrough codecs into separate packages
* Fri Mar 16 2007 - peter+rpmspam@suntel.com.tr
- Added devel package
* Thu Mar 15 2007 - peter+rpmspam@suntel.com.tr
- Initial RPM release

