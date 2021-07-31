#!/bin/bash
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-
##### Author: Travis Cross <tc@traviscross.com>

codename="sid"
modulelist_opt=""
modules_add=""
use_sysvinit=""
while getopts "a:c:m:p:v" o; do
  case "$o" in
    a) avoid_mods_arch="$OPTARG" ;;
    c) codename="$OPTARG" ;;
    m) modulelist_opt="$OPTARG" ;;
    p) modules_add="$modules_add $OPTARG";;
    v) use_sysvinit="true";;
  esac
done
shift $(($OPTIND-1))

if [ x${use_sysvinit} = x ]; then
    case "$codename" in
      wheezy|trusty|utopic|xenial) use_sysvinit="true";;
      *) use_sysvinit="false";;
    esac
fi

mod_dir="../src/mod"
conf_dir="../conf"
lang_dir="../conf/vanilla/lang"
fs_description="FreeSWITCH is a scalable open source cross-platform telephony platform designed to route and interconnect popular communication protocols using audio, video, text or any other form of media."
mod_build_depends="." mod_depends="." mod_recommends="." mod_suggests="."
supported_debian_distros="wheezy jessie stretch buster sid"
supported_ubuntu_distros="trusty utopic xenial"
supported_distros="$supported_debian_distros $supported_ubuntu_distros"
avoid_mods=(
  applications/mod_sms_flowroute
  applications/mod_limit
  applications/mod_mongo
  applications/mod_mp4
  applications/mod_mp4v2
  applications/mod_osp
  applications/mod_rad_auth
  applications/mod_skel
  applications/mod_cluechoo
  asr_tts/mod_cepstral
  codecs/mod_com_g729
  codecs/mod_openh264
  codecs/mod_siren
  codecs/mod_sangoma_codec
  codecs/mod_skel_codec
  endpoints/mod_gsmopen
  endpoints/mod_h323
  endpoints/mod_khomp
  endpoints/mod_opal
  endpoints/mod_reference
  endpoints/mod_skypopen
  endpoints/mod_unicall
  event_handlers/mod_smpp
  event_handlers/mod_event_zmq
  formats/mod_webm
  sdk/autotools
  xml_int/mod_xml_ldap
  xml_int/mod_xml_radius
)
avoid_mods_armhf=(
  languages/mod_v8
)
avoid_mods_sid=(
  directories/mod_ldap
)
avoid_mods_jessie=(
  directories/mod_ldap
)
avoid_mods_wheezy=(
  event_handlers/mod_amqp
  languages/mod_java
  languages/mod_managed
  applications/mod_av
  applications/mod_cv
  applications/mod_hiredis
  formats/mod_shout
  applications/mod_sonar
  applications/mod_soundtouch
  formats/mod_vlc
)
avoid_mods_trusty=(
  event_handlers/mod_amqp
  loggers/mod_raven
)
avoid_mods_utopic=(
  directories/mod_ldap
  loggers/mod_raven
)
avoid_mods_xenial=(
  event_handlers/mod_ldap
  event_handlers/mod_amqp
  asr_tts/mod_flite
  loggers/mod_raven
)
manual_pkgs=(
freeswitch-all
freeswitch
libfreeswitch1
freeswitch-meta-bare
freeswitch-meta-default
freeswitch-meta-vanilla
freeswitch-meta-sorbet
freeswitch-meta-all
freeswitch-meta-codecs
freeswitch-meta-conf
freeswitch-meta-lang
freeswitch-meta-mod-say
freeswitch-all-dbg
freeswitch-dbg
libfreeswitch1-dbg
libfreeswitch-dev
freeswitch-doc
freeswitch-lang
freeswitch-timezones
)

if [ ${use_sysvinit} = "true" ]; then
    manual_pkgs=( "${manual_pkgs[@]}" "freeswitch-sysvinit" )
else
    manual_pkgs=( "${manual_pkgs[@]}" "freeswitch-systemd" )
fi

err () {
  echo "$0 error: $1" >&2
  exit 1
}

xread () {
  local xIFS="$IFS"
  IFS=''
  read $@
  local ret=$?
  IFS="$xIFS"
  return $ret
}

intersperse () {
  local sep="$1"
  awk "
    BEGIN {
      first=1;
      sep=\"${sep}\";
    }"'
    /.*/ {
      if (first == 0) {
        printf "%s%s", sep, $0;
      } else {
        printf "%s", $0;
      }
      first=0;
    }
    END { printf "\n"; }'
}

postfix () {
  local px="$1"
  awk "
    BEGIN { px=\"${px}\"; }"'
    /.*/ { printf "%s%s\n", $0, px; }'
}

avoid_mod_filter () {
  local x="avoid_mods_$codename[@]"
  local y="avoid_mods_$avoid_mods_arch[@]"
  local -a mods=("${avoid_mods[@]}" "${!x}" "${!y}")
  for x in "${mods[@]}"; do
    if [ "$1" = "$x" ]; then
      [ "$2" = "show" ] && echo "excluding module $x" >&2
      return 1
    fi
  done
  return 0
}

modconf_filter () {
  while xread l; do
    if [ "$1" = "$l" ]; then
      [ "$2" = "show" ] && echo "including module $l" >&2
      return 0
    fi
  done < modules.conf
  return 1
}

mod_filter () {
  if test -f modules.conf; then
    modconf_filter $@
  else
    avoid_mod_filter $@
  fi
}

mod_filter_show () {
  mod_filter "$1" show
}

map_fs_modules () {
  local filterfn="$1" percatfns="$2" permodfns="$3"
  for x in $mod_dir/*; do
    test -d $x || continue
    test ! ${x##*/} = legacy || continue
    category=${x##*/} category_path=$x
    for f in $percatfns; do $f; done
    for y in $x/*; do
      module_name=${y##*/} module_path=$y
      module=$category/$module_name
      if $filterfn $category/$module; then
        [ -f ${y}/module ] && . ${y}/module
        for f in $permodfns; do $f; done
      fi
      unset module_name module_path module
    done
    unset category category_path
  done
}

map_modules () {
  local filterfn="$1" percatfns="$2" permodfns="$3"
  for x in $parse_dir/*; do
    test -d $x || continue
    category=${x##*/} category_path=$x
    for f in $percatfns; do $f; done
    for y in $x/*; do
      test -f $y || continue
      module=${y##*/} module_path=$y
      $filterfn $category/$module || continue
      module="" category="" module_name=""
      section="" description="" long_description=""
      build_depends="" depends="" recommends="" suggests=""
      distro_conflicts=""
      distro_vars=""
      for x in $supported_distros; do
        distro_vars="$distro_vars build_depends_$x"
        eval build_depends_$x=""
      done
      . $y
      [ -n "$description" ] || description="$module_name"
      [ -n "$long_description" ] || description="Adds ${module_name}."
      for f in $permodfns; do $f; done
      unset \
        module module_name module_path \
        section description long_description \
        build_depends depends recommends suggests \
        distro_conflicts $distro_vars
    done
    unset category category_path
  done
}

map_confs () {
  local fs="$1"
  for x in $conf_dir/*; do
    test ! -d $x && continue
    conf=${x##*/} conf_dir=$x
    for f in $fs; do $f; done
    unset conf conf_dir
  done
}

map_langs () {
  local fs="$1"
  for x in $lang_dir/*; do
    test ! -d $x && continue
    lang=${x##*/} lang_dir=$x
    for f in $fs; do $f; done
    unset lang lang_dir
  done
}

map_pkgs () {
  local fsx="$1"
  for x in "${manual_pkgs[@]}"; do
    $fsx $x
  done
  map_pkgs_confs () { $fsx "freeswitch-conf-${conf//_/-}"; }
  map_confs map_pkgs_confs
  map_pkgs_langs () { $fsx "freeswitch-lang-${lang//_/-}"; }
  map_langs map_pkgs_langs
  map_pkgs_mods () {
    $fsx "freeswitch-${module//_/-}"
    $fsx "freeswitch-${module//_/-}-dbg"; }
  map_modules map_pkgs_mods
}

list_pkgs () {
  list_pkgs_thunk () { printf '%s\n' "$1"; }
  map_pkgs list_pkgs_thunk
}

print_source_control () {
  local libtool_dep="libtool, libtool-bin"
  case "$codename" in
    wheezy|trusty) libtool_dep="libtool" ;;
  esac
  local debhelper_dep="debhelper (>= 8.0.0)"
  if [ ${use_sysvinit} = "false" ]; then
      debhelper_dep=${debhelper_dep}", dh-systemd"
  fi
  cat <<EOF
Source: freeswitch
Section: comm
Priority: optional
Maintainer: SignalWire, Inc <support@signalwire.com>
Build-Depends:
# for debian
 ${debhelper_dep},
# bootstrapping
 automake (>= 1.9), autoconf, ${libtool_dep},
# core build
 dpkg-dev (>= 1.15.8.12), gcc (>= 4:4.4.5), g++ (>= 4:4.4.5),
 libc6-dev (>= 2.11.3), make (>= 3.81),
 libpcre3-dev,
 libedit-dev (>= 2.11),
 libsqlite3-dev,
 libtiff5-dev,
 wget, pkg-config,
 yasm,
# core codecs
 libogg-dev, libspeex-dev, libspeexdsp-dev,
# configure options
 libssl1.0-dev | libssl-dev, unixodbc-dev, libpq-dev,
 libncurses5-dev, libjpeg62-turbo-dev | libjpeg-turbo8-dev | libjpeg62-dev | libjpeg8-dev,
 python-dev, python-all-dev, python-support (>= 0.90) | dh-python, erlang-dev, libtpl-dev (>= 1.5),
# documentation
 doxygen,
# for APR (not essential for build)
 uuid-dev, libexpat1-dev, libgdbm-dev, libdb-dev,
# used by many modules
 libcurl4-openssl-dev | libcurl4-gnutls-dev | libcurl-dev,
 bison, zlib1g-dev, libsofia-sip-ua-dev (>= 1.13.4),
 libspandsp3-dev,
# used to format the private freeswitch apt-repo key properly
 gnupg,
# module build-depends
 $(debian_wrap "${mod_build_depends}")
Standards-Version: 3.9.3
Homepage: https://freeswitch.org/
Vcs-Git: https://github.com/signalwire/freeswitch.git
Vcs-Browser: https://github.com/signalwire/freeswitch

EOF
}

print_core_control () {
cat <<EOF
Package: freeswitch-all
Architecture: amd64 armhf
Depends: freeswitch-meta-all (= \${binary:Version}), freeswitch-meta-all-dbg (= \${binary:Version})
Conflicts: freeswitch-all (<= 1.6.7)
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a package which depends on all packaged FreeSWITCH modules.

Package: freeswitch
Architecture: amd64 armhf
Depends: \${shlibs:Depends}, \${perl:Depends}, \${misc:Depends},
 libfreeswitch1 (= \${binary:Version})
Recommends:
Suggests: freeswitch-dbg
Conflicts: freeswitch-all (<= 1.6.7)
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This package contains the FreeSWITCH core.

Package: libfreeswitch1
Architecture: amd64 armhf
Depends: \${shlibs:Depends}, \${misc:Depends}
Recommends:
Suggests: libfreeswitch1-dbg
Conflicts: freeswitch-all (<= 1.6.7)
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This package contains the FreeSWITCH core library.

Package: python-esl
Section: python
Architecture: amd64 armhf
Depends: \${shlibs:Depends}, \${misc:Depends}, \${python:Depends}
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This package contains the Python binding for FreeSWITCH Event Socket Library (ESL).

Package: libesl-perl
Section: perl
Architecture: amd64 armhf
Depends: \${shlibs:Depends}, \${misc:Depends}, \${perl:Depends}
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This package contains the Perl binding for FreeSWITCH Event Socket Library (ESL).

Package: freeswitch-meta-bare
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch (= \${binary:Version})
Recommends:
 freeswitch-doc (= \${binary:Version}),
 freeswitch-mod-commands (= \${binary:Version}),
 freeswitch-init,
 freeswitch-lang (= \${binary:Version}),
 freeswitch-timezones (= \${binary:Version}),
 freeswitch-music,
 freeswitch-sounds
Suggests:
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on the packages needed for a very
 bare FreeSWITCH install.

Package: freeswitch-meta-default
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch (= \${binary:Version}),
 freeswitch-mod-commands (= \${binary:Version}),
 freeswitch-mod-conference (= \${binary:Version}),
 freeswitch-mod-db (= \${binary:Version}),
 freeswitch-mod-dptools (= \${binary:Version}),
 freeswitch-mod-fifo (= \${binary:Version}),
 freeswitch-mod-hash (= \${binary:Version}),
 freeswitch-mod-pgsql (= \${binary:Version}),
 freeswitch-mod-spandsp (= \${binary:Version}),
 freeswitch-mod-voicemail (= \${binary:Version}),
 freeswitch-mod-dialplan-xml (= \${binary:Version}),
 freeswitch-mod-loopback (= \${binary:Version}),
 freeswitch-mod-sofia (= \${binary:Version}),
 freeswitch-mod-local-stream (= \${binary:Version}),
 freeswitch-mod-native-file (= \${binary:Version}),
 freeswitch-mod-sndfile (= \${binary:Version}),
 freeswitch-mod-tone-stream (= \${binary:Version}),
 freeswitch-mod-lua (= \${binary:Version}),
 freeswitch-mod-console (= \${binary:Version}),
 freeswitch-mod-say-en (= \${binary:Version})
Recommends:
 freeswitch-init,
 freeswitch-lang (= \${binary:Version}),
 freeswitch-timezones (= \${binary:Version}),
 freeswitch-meta-codecs (= \${binary:Version}),
 freeswitch-music,
 freeswitch-sounds
Suggests:
 freeswitch-mod-cidlookup (= \${binary:Version}),
 freeswitch-mod-curl (= \${binary:Version}),
 freeswitch-mod-directory (= \${binary:Version}),
 freeswitch-mod-enum (= \${binary:Version}),
 freeswitch-mod-spy (= \${binary:Version}),
 freeswitch-mod-valet-parking (= \${binary:Version})
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on the packages needed for a
 reasonably basic FreeSWITCH install.

Package: freeswitch-meta-vanilla
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch (= \${binary:Version}),
 freeswitch-init,
 freeswitch-mod-console (= \${binary:Version}),
 freeswitch-mod-logfile (= \${binary:Version}),
 freeswitch-mod-enum (= \${binary:Version}),
 freeswitch-mod-cdr-csv (= \${binary:Version}),
 freeswitch-mod-event-socket (= \${binary:Version}),
 freeswitch-mod-sofia (= \${binary:Version}),
 freeswitch-mod-loopback (= \${binary:Version}),
 freeswitch-mod-commands (= \${binary:Version}),
 freeswitch-mod-conference (= \${binary:Version}),
 freeswitch-mod-db (= \${binary:Version}),
 freeswitch-mod-dptools (= \${binary:Version}),
 freeswitch-mod-expr (= \${binary:Version}),
 freeswitch-mod-fifo (= \${binary:Version}),
 freeswitch-mod-hash (= \${binary:Version}),
 freeswitch-mod-pgsql (= \${binary:Version}),
 freeswitch-mod-voicemail (= \${binary:Version}),
 freeswitch-mod-esf (= \${binary:Version}),
 freeswitch-mod-fsv (= \${binary:Version}),
 freeswitch-mod-valet-parking (= \${binary:Version}),
 freeswitch-mod-httapi (= \${binary:Version}),
 freeswitch-mod-dialplan-xml (= \${binary:Version}),
 freeswitch-mod-dialplan-asterisk (= \${binary:Version}),
 freeswitch-mod-spandsp (= \${binary:Version}),
 freeswitch-mod-g723-1 (= \${binary:Version}),
 freeswitch-mod-g729 (= \${binary:Version}),
 freeswitch-mod-amr (= \${binary:Version}),
 freeswitch-mod-h26x (= \${binary:Version}),
 freeswitch-mod-sndfile (= \${binary:Version}),
 freeswitch-mod-native-file (= \${binary:Version}),
 freeswitch-mod-local-stream (= \${binary:Version}),
 freeswitch-mod-tone-stream (= \${binary:Version}),
 freeswitch-mod-lua (= \${binary:Version}),
 freeswitch-mod-say-en (= \${binary:Version}),
Recommends:
 freeswitch-lang (= \${binary:Version}),
 freeswitch-timezones (= \${binary:Version}),
 freeswitch-music,
 freeswitch-sounds,
 freeswitch-conf-vanilla (= \${binary:Version}),
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on the packages needed for
 running the FreeSWITCH vanilla example configuration.

Package: freeswitch-meta-sorbet
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch (= \${binary:Version}),
Recommends:
 freeswitch-init,
 freeswitch-lang (= \${binary:Version}),
 freeswitch-timezones (= \${binary:Version}),
 freeswitch-meta-codecs (= \${binary:Version}),
 freeswitch-music,
 freeswitch-sounds,
 freeswitch-mod-abstraction (= \${binary:Version}),
 freeswitch-mod-avmd (= \${binary:Version}),
 freeswitch-mod-blacklist (= \${binary:Version}),
 freeswitch-mod-callcenter (= \${binary:Version}),
 freeswitch-mod-cidlookup (= \${binary:Version}),
 freeswitch-mod-commands (= \${binary:Version}),
 freeswitch-mod-conference (= \${binary:Version}),
 freeswitch-mod-curl (= \${binary:Version}),
 freeswitch-mod-db (= \${binary:Version}),
 freeswitch-mod-directory (= \${binary:Version}),
 freeswitch-mod-distributor (= \${binary:Version}),
 freeswitch-mod-dptools (= \${binary:Version}),
 freeswitch-mod-easyroute (= \${binary:Version}),
 freeswitch-mod-enum (= \${binary:Version}),
 freeswitch-mod-esf (= \${binary:Version}),
 freeswitch-mod-esl (= \${binary:Version}),
 freeswitch-mod-expr (= \${binary:Version}),
 freeswitch-mod-fifo (= \${binary:Version}),
 freeswitch-mod-fsk (= \${binary:Version}),
 freeswitch-mod-fsv (= \${binary:Version}),
 freeswitch-mod-hash (= \${binary:Version}),
 freeswitch-mod-httapi (= \${binary:Version}),
 freeswitch-mod-http-cache (= \${binary:Version}),
 freeswitch-mod-lcr (= \${binary:Version}),
 freeswitch-mod-nibblebill (= \${binary:Version}),
 freeswitch-mod-oreka (= \${binary:Version}),
 freeswitch-mod-pgsql (= \${binary:Version}),
 freeswitch-mod-redis (= \${binary:Version}),
 freeswitch-mod-rss (= \${binary:Version}),
 freeswitch-mod-sms (= \${binary:Version}),
 freeswitch-mod-snapshot (= \${binary:Version}),
 freeswitch-mod-snom (= \${binary:Version}),
 freeswitch-mod-sonar (= \${binary:Version}),
 freeswitch-mod-soundtouch (= \${binary:Version}),
 freeswitch-mod-spandsp (= \${binary:Version}),
 freeswitch-mod-spy (= \${binary:Version}),
 freeswitch-mod-stress (= \${binary:Version}),
 freeswitch-mod-valet-parking (= \${binary:Version}),
 freeswitch-mod-vmd (= \${binary:Version}),
 freeswitch-mod-voicemail (= \${binary:Version}),
 freeswitch-mod-voicemail-ivr (= \${binary:Version}),
 freeswitch-mod-flite (= \${binary:Version}),
 freeswitch-mod-pocketsphinx (= \${binary:Version}),
 freeswitch-mod-tts-commandline (= \${binary:Version}),
 freeswitch-mod-dialplan-xml (= \${binary:Version}),
 freeswitch-mod-loopback (= \${binary:Version}),
 freeswitch-mod-rtmp (= \${binary:Version}),
 freeswitch-mod-skinny (= \${binary:Version}),
 freeswitch-mod-sofia (= \${binary:Version}),
 freeswitch-mod-cdr-csv (= \${binary:Version}),
 freeswitch-mod-cdr-sqlite (= \${binary:Version}),
 freeswitch-mod-event-socket (= \${binary:Version}),
 freeswitch-mod-json-cdr (= \${binary:Version}),
 freeswitch-mod-local-stream (= \${binary:Version}),
 freeswitch-mod-native-file (= \${binary:Version}),
 freeswitch-mod-shell-stream (= \${binary:Version}),
 freeswitch-mod-sndfile (= \${binary:Version}),
 freeswitch-mod-tone-stream (= \${binary:Version}),
 freeswitch-mod-lua (= \${binary:Version}),
 freeswitch-mod-console (= \${binary:Version}),
 freeswitch-mod-logfile (= \${binary:Version}),
 freeswitch-mod-syslog (= \${binary:Version}),
 freeswitch-mod-say-en (= \${binary:Version}),
 freeswitch-mod-posix-timer (= \${binary:Version}),
 freeswitch-mod-timerfd (= \${binary:Version}),
 freeswitch-mod-xml-cdr (= \${binary:Version}),
 freeswitch-mod-xml-curl (= \${binary:Version}),
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which recommends most packaged FreeSWITCH
 modules except a few which aren't recommended.

Package: freeswitch-meta-all
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch (= \${binary:Version}),
 freeswitch-init,
 freeswitch-lang (= \${binary:Version}),
 freeswitch-timezones (= \${binary:Version}),
 freeswitch-meta-codecs (= \${binary:Version}),
 freeswitch-meta-conf (= \${binary:Version}),
 freeswitch-meta-lang (= \${binary:Version}),
 freeswitch-meta-mod-say (= \${binary:Version}),
 freeswitch-music,
 freeswitch-sounds,
 freeswitch-mod-abstraction (= \${binary:Version}),
 freeswitch-mod-avmd (= \${binary:Version}),
 freeswitch-mod-av (= \${binary:Version}),
 freeswitch-mod-blacklist (= \${binary:Version}),
 freeswitch-mod-callcenter (= \${binary:Version}),
 freeswitch-mod-cidlookup (= \${binary:Version}),
 freeswitch-mod-commands (= \${binary:Version}),
 freeswitch-mod-conference (= \${binary:Version}),
 freeswitch-mod-curl (= \${binary:Version}),
 freeswitch-mod-db (= \${binary:Version}),
 freeswitch-mod-directory (= \${binary:Version}),
 freeswitch-mod-distributor (= \${binary:Version}),
 freeswitch-mod-dptools (= \${binary:Version}),
 freeswitch-mod-easyroute (= \${binary:Version}),
 freeswitch-mod-enum (= \${binary:Version}),
 freeswitch-mod-esf (= \${binary:Version}),
 freeswitch-mod-esl (= \${binary:Version}),
 freeswitch-mod-expr (= \${binary:Version}),
 freeswitch-mod-fifo (= \${binary:Version}),
 freeswitch-mod-fsk (= \${binary:Version}),
 freeswitch-mod-fsv (= \${binary:Version}),
 freeswitch-mod-hash (= \${binary:Version}),
 freeswitch-mod-httapi (= \${binary:Version}),
 freeswitch-mod-http-cache (= \${binary:Version}),
 freeswitch-mod-lcr (= \${binary:Version}),
 freeswitch-mod-memcache (= \${binary:Version}),
 freeswitch-mod-nibblebill (= \${binary:Version}),
 freeswitch-mod-oreka (= \${binary:Version}),
 freeswitch-mod-mariadb (= \${binary:Version}),
 freeswitch-mod-pgsql (= \${binary:Version}),
 freeswitch-mod-png (= \${binary:Version}),
 freeswitch-mod-redis (= \${binary:Version}),
 freeswitch-mod-rss (= \${binary:Version}),
 freeswitch-mod-signalwire (= \${binary:Version}),
 freeswitch-mod-shout (= \${binary:Version}),
 freeswitch-mod-sms (= \${binary:Version}),
 freeswitch-mod-snapshot (= \${binary:Version}),
 freeswitch-mod-snom (= \${binary:Version}),
 freeswitch-mod-sonar (= \${binary:Version}),
 freeswitch-mod-soundtouch (= \${binary:Version}),
 freeswitch-mod-spandsp (= \${binary:Version}),
 freeswitch-mod-spy (= \${binary:Version}),
 freeswitch-mod-stress (= \${binary:Version}),
 freeswitch-mod-translate (= \${binary:Version}),
 freeswitch-mod-valet-parking (= \${binary:Version}),
 freeswitch-mod-video-filter (= \${binary:Version}),
 freeswitch-mod-voicemail (= \${binary:Version}),
 freeswitch-mod-voicemail-ivr (= \${binary:Version}),
 freeswitch-mod-flite (= \${binary:Version}),
 freeswitch-mod-pocketsphinx (= \${binary:Version}),
 freeswitch-mod-tts-commandline (= \${binary:Version}),
 freeswitch-mod-unimrcp (= \${binary:Version}),
 freeswitch-mod-dialplan-asterisk (= \${binary:Version}),
 freeswitch-mod-dialplan-directory (= \${binary:Version}),
 freeswitch-mod-dialplan-xml (= \${binary:Version}),
 freeswitch-mod-dingaling (= \${binary:Version}),
 freeswitch-mod-loopback (= \${binary:Version}),
 freeswitch-mod-portaudio (= \${binary:Version}),
 freeswitch-mod-rtc (= \${binary:Version}),
 freeswitch-mod-rtmp (= \${binary:Version}),
 freeswitch-mod-skinny (= \${binary:Version}),
 freeswitch-mod-sofia (= \${binary:Version}),
 freeswitch-mod-verto (= \${binary:Version}),
 freeswitch-mod-cdr-csv (= \${binary:Version}),
 freeswitch-mod-cdr-mongodb (= \${binary:Version}),
 freeswitch-mod-cdr-sqlite (= \${binary:Version}),
 freeswitch-mod-erlang-event (= \${binary:Version}),
 freeswitch-mod-event-multicast (= \${binary:Version}),
 freeswitch-mod-event-socket (= \${binary:Version}),
 freeswitch-mod-json-cdr (= \${binary:Version}),
 freeswitch-mod-kazoo (= \${binary:Version}),
 freeswitch-mod-snmp (= \${binary:Version}),
 freeswitch-mod-local-stream (= \${binary:Version}),
 freeswitch-mod-native-file (= \${binary:Version}),
 freeswitch-mod-portaudio-stream (= \${binary:Version}),
 freeswitch-mod-shell-stream (= \${binary:Version}),
 freeswitch-mod-sndfile (= \${binary:Version}),
 freeswitch-mod-tone-stream (= \${binary:Version}),
 freeswitch-mod-java (= \${binary:Version}),
 freeswitch-mod-lua (= \${binary:Version}),
 freeswitch-mod-perl (= \${binary:Version}),
 freeswitch-mod-python (= \${binary:Version}),
 freeswitch-mod-yaml (= \${binary:Version}),
 freeswitch-mod-console (= \${binary:Version}),
 freeswitch-mod-logfile (= \${binary:Version}),
 freeswitch-mod-syslog (= \${binary:Version}),
 freeswitch-mod-posix-timer (= \${binary:Version}),
 freeswitch-mod-timerfd (= \${binary:Version}),
 freeswitch-mod-xml-cdr (= \${binary:Version}),
 freeswitch-mod-xml-curl (= \${binary:Version}),
 freeswitch-mod-xml-rpc (= \${binary:Version}),
 freeswitch-mod-xml-scgi (= \${binary:Version}),
Recommends:
Suggests:
 freeswitch-mod-vmd (= \${binary:Version}),
 freeswitch-mod-vlc (= \${binary:Version}),
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which recommends or suggests all packaged
 FreeSWITCH modules.

Package: freeswitch-meta-codecs
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch (= \${binary:Version}),
 freeswitch-mod-amr (= \${binary:Version}),
 freeswitch-mod-amrwb (= \${binary:Version}),
 freeswitch-mod-b64 (= \${binary:Version}),
 freeswitch-mod-bv (= \${binary:Version}),
 freeswitch-mod-codec2 (= \${binary:Version}),
 freeswitch-mod-dahdi-codec (= \${binary:Version}),
 freeswitch-mod-g723-1 (= \${binary:Version}),
 freeswitch-mod-g729 (= \${binary:Version}),
 freeswitch-mod-h26x (= \${binary:Version}),
 freeswitch-mod-isac (= \${binary:Version}),
 freeswitch-mod-mp4v (= \${binary:Version}),
 freeswitch-mod-opus (= \${binary:Version}),
 freeswitch-mod-silk (= \${binary:Version}),
 freeswitch-mod-spandsp (= \${binary:Version}),
 freeswitch-mod-theora (= \${binary:Version}),
Suggests:
 freeswitch-mod-ilbc (= \${binary:Version}),
 freeswitch-mod-siren (= \${binary:Version})
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on the packages needed to install
 most FreeSWITCH codecs.

Package: freeswitch-meta-codecs-dbg
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch (= \${binary:Version}),
 freeswitch-mod-amr-dbg (= \${binary:Version}),
 freeswitch-mod-amrwb-dbg (= \${binary:Version}),
 freeswitch-mod-b64-dbg (= \${binary:Version}),
 freeswitch-mod-bv-dbg (= \${binary:Version}),
 freeswitch-mod-codec2-dbg (= \${binary:Version}),
 freeswitch-mod-dahdi-codec-dbg (= \${binary:Version}),
 freeswitch-mod-g723-1-dbg (= \${binary:Version}),
 freeswitch-mod-g729-dbg (= \${binary:Version}),
 freeswitch-mod-h26x-dbg (= \${binary:Version}),
 freeswitch-mod-isac-dbg (= \${binary:Version}),
 freeswitch-mod-mp4v-dbg (= \${binary:Version}),
 freeswitch-mod-opus-dbg (= \${binary:Version}),
 freeswitch-mod-silk-dbg (= \${binary:Version}),
 freeswitch-mod-spandsp-dbg (= \${binary:Version}),
 freeswitch-mod-theora-dbg (= \${binary:Version}),
Suggests:
 freeswitch-mod-ilbc-dbg (= \${binary:Version}),
 freeswitch-mod-siren-dbg (= \${binary:Version})
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on the packages needed to install
 most FreeSWITCH codecs.

Package: freeswitch-meta-conf
Architecture: amd64 armhf
Depends: \${misc:Depends},
 freeswitch-conf-curl (= \${binary:Version}),
 freeswitch-conf-insideout (= \${binary:Version}),
 freeswitch-conf-sbc (= \${binary:Version}),
 freeswitch-conf-softphone (= \${binary:Version}),
 freeswitch-conf-vanilla (= \${binary:Version}),
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on the available configuration
 examples for FreeSWITCH.

Package: freeswitch-meta-lang
Architecture: amd64 armhf
Depends: \${misc:Depends},
 freeswitch-lang-de (= \${binary:Version}),
 freeswitch-lang-en (= \${binary:Version}),
 freeswitch-lang-es (= \${binary:Version}),
 freeswitch-lang-fr (= \${binary:Version}),
 freeswitch-lang-he (= \${binary:Version}),
 freeswitch-lang-pt (= \${binary:Version}),
 freeswitch-lang-ru (= \${binary:Version}),
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on all language files for
 FreeSWITCH.

Package: freeswitch-meta-mod-say
Architecture: amd64 armhf
Depends: \${misc:Depends},
 freeswitch-mod-say-de (= \${binary:Version}),
 freeswitch-mod-say-en (= \${binary:Version}),
 freeswitch-mod-say-es (= \${binary:Version}),
 freeswitch-mod-say-fa (= \${binary:Version}),
 freeswitch-mod-say-fr (= \${binary:Version}),
 freeswitch-mod-say-he (= \${binary:Version}),
 freeswitch-mod-say-hr (= \${binary:Version}),
 freeswitch-mod-say-hu (= \${binary:Version}),
 freeswitch-mod-say-it (= \${binary:Version}),
 freeswitch-mod-say-ja (= \${binary:Version}),
 freeswitch-mod-say-nl (= \${binary:Version}),
 freeswitch-mod-say-pl (= \${binary:Version}),
 freeswitch-mod-say-pt (= \${binary:Version}),
 freeswitch-mod-say-ru (= \${binary:Version}),
 freeswitch-mod-say-th (= \${binary:Version}),
 freeswitch-mod-say-zh (= \${binary:Version}),
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on all mod_say languages for
 FreeSWITCH.

Package: freeswitch-meta-mod-say-dbg
Architecture: amd64 armhf
Depends: \${misc:Depends},
 freeswitch-mod-say-de-dbg (= \${binary:Version}),
 freeswitch-mod-say-en-dbg (= \${binary:Version}),
 freeswitch-mod-say-es-dbg (= \${binary:Version}),
 freeswitch-mod-say-fa-dbg (= \${binary:Version}),
 freeswitch-mod-say-fr-dbg (= \${binary:Version}),
 freeswitch-mod-say-he-dbg (= \${binary:Version}),
 freeswitch-mod-say-hr-dbg (= \${binary:Version}),
 freeswitch-mod-say-hu-dbg (= \${binary:Version}),
 freeswitch-mod-say-it-dbg (= \${binary:Version}),
 freeswitch-mod-say-ja-dbg (= \${binary:Version}),
 freeswitch-mod-say-nl-dbg (= \${binary:Version}),
 freeswitch-mod-say-pl-dbg (= \${binary:Version}),
 freeswitch-mod-say-pt-dbg (= \${binary:Version}),
 freeswitch-mod-say-ru-dbg (= \${binary:Version}),
 freeswitch-mod-say-th-dbg (= \${binary:Version}),
 freeswitch-mod-say-zh-dbg (= \${binary:Version}),
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on all mod_say languages for
 FreeSWITCH.

Package: freeswitch-meta-all-dbg
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch (= \${binary:Version}),
 freeswitch-meta-codecs-dbg (= \${binary:Version}),
 freeswitch-meta-mod-say (= \${binary:Version}),
 freeswitch-mod-abstraction-dbg (= \${binary:Version}),
 freeswitch-mod-avmd-dbg (= \${binary:Version}),
 freeswitch-mod-av-dbg (= \${binary:Version}),
 freeswitch-mod-blacklist-dbg (= \${binary:Version}),
 freeswitch-mod-callcenter-dbg (= \${binary:Version}),
 freeswitch-mod-cidlookup-dbg (= \${binary:Version}),
 freeswitch-mod-commands-dbg (= \${binary:Version}),
 freeswitch-mod-conference-dbg (= \${binary:Version}),
 freeswitch-mod-curl-dbg (= \${binary:Version}),
 freeswitch-mod-db-dbg (= \${binary:Version}),
 freeswitch-mod-directory-dbg (= \${binary:Version}),
 freeswitch-mod-distributor-dbg (= \${binary:Version}),
 freeswitch-mod-dptools-dbg (= \${binary:Version}),
 freeswitch-mod-easyroute-dbg (= \${binary:Version}),
 freeswitch-mod-enum-dbg (= \${binary:Version}),
 freeswitch-mod-esf-dbg (= \${binary:Version}),
 freeswitch-mod-esl-dbg (= \${binary:Version}),
 freeswitch-mod-expr-dbg (= \${binary:Version}),
 freeswitch-mod-fifo-dbg (= \${binary:Version}),
 freeswitch-mod-fsk-dbg (= \${binary:Version}),
 freeswitch-mod-fsv-dbg (= \${binary:Version}),
 freeswitch-mod-hash-dbg (= \${binary:Version}),
 freeswitch-mod-httapi-dbg (= \${binary:Version}),
 freeswitch-mod-http-cache-dbg (= \${binary:Version}),
 freeswitch-mod-lcr-dbg (= \${binary:Version}),
 freeswitch-mod-memcache-dbg (= \${binary:Version}),
 freeswitch-mod-nibblebill-dbg (= \${binary:Version}),
 freeswitch-mod-oreka-dbg (= \${binary:Version}),
 freeswitch-mod-mariadb-dbg (= \${binary:Version}),
 freeswitch-mod-pgsql-dbg (= \${binary:Version}),
 freeswitch-mod-png-dbg (= \${binary:Version}),
 freeswitch-mod-redis-dbg (= \${binary:Version}),
 freeswitch-mod-rss-dbg (= \${binary:Version}),
 freeswitch-mod-sms-dbg (= \${binary:Version}),
 freeswitch-mod-snapshot-dbg (= \${binary:Version}),
 freeswitch-mod-snom-dbg (= \${binary:Version}),
 freeswitch-mod-sonar-dbg (= \${binary:Version}),
 freeswitch-mod-soundtouch-dbg (= \${binary:Version}),
 freeswitch-mod-spandsp-dbg (= \${binary:Version}),
 freeswitch-mod-spy-dbg (= \${binary:Version}),
 freeswitch-mod-stress-dbg (= \${binary:Version}),
 freeswitch-mod-translate-dbg (= \${binary:Version}),
 freeswitch-mod-valet-parking-dbg (= \${binary:Version}),
 freeswitch-mod-video-filter-dbg (= \${binary:Version}),
 freeswitch-mod-voicemail-dbg (= \${binary:Version}),
 freeswitch-mod-voicemail-ivr-dbg (= \${binary:Version}),
 freeswitch-mod-flite-dbg (= \${binary:Version}),
 freeswitch-mod-pocketsphinx-dbg (= \${binary:Version}),
 freeswitch-mod-tts-commandline-dbg (= \${binary:Version}),
 freeswitch-mod-unimrcp-dbg (= \${binary:Version}),
 freeswitch-mod-dialplan-asterisk-dbg (= \${binary:Version}),
 freeswitch-mod-dialplan-directory-dbg (= \${binary:Version}),
 freeswitch-mod-dialplan-xml-dbg (= \${binary:Version}),
 freeswitch-mod-dingaling-dbg (= \${binary:Version}),
 freeswitch-mod-loopback-dbg (= \${binary:Version}),
 freeswitch-mod-portaudio-dbg (= \${binary:Version}),
 freeswitch-mod-rtc-dbg (= \${binary:Version}),
 freeswitch-mod-rtmp-dbg (= \${binary:Version}),
 freeswitch-mod-skinny-dbg (= \${binary:Version}),
 freeswitch-mod-sofia-dbg (= \${binary:Version}),
 freeswitch-mod-verto-dbg (= \${binary:Version}),
 freeswitch-mod-cdr-csv-dbg (= \${binary:Version}),
 freeswitch-mod-cdr-mongodb-dbg (= \${binary:Version}),
 freeswitch-mod-cdr-sqlite-dbg (= \${binary:Version}),
 freeswitch-mod-erlang-event-dbg (= \${binary:Version}),
 freeswitch-mod-event-multicast-dbg (= \${binary:Version}),
 freeswitch-mod-event-socket-dbg (= \${binary:Version}),
 freeswitch-mod-json-cdr-dbg (= \${binary:Version}),
 freeswitch-mod-kazoo-dbg (= \${binary:Version}),
 freeswitch-mod-snmp-dbg (= \${binary:Version}),
 freeswitch-mod-local-stream-dbg (= \${binary:Version}),
 freeswitch-mod-native-file-dbg (= \${binary:Version}),
 freeswitch-mod-portaudio-stream-dbg (= \${binary:Version}),
 freeswitch-mod-shell-stream-dbg (= \${binary:Version}),
 freeswitch-mod-sndfile-dbg (= \${binary:Version}),
 freeswitch-mod-tone-stream-dbg (= \${binary:Version}),
 freeswitch-mod-java-dbg (= \${binary:Version}),
 freeswitch-mod-lua-dbg (= \${binary:Version}),
 freeswitch-mod-perl-dbg (= \${binary:Version}),
 freeswitch-mod-python-dbg (= \${binary:Version}),
 freeswitch-mod-yaml-dbg (= \${binary:Version}),
 freeswitch-mod-console-dbg (= \${binary:Version}),
 freeswitch-mod-logfile-dbg (= \${binary:Version}),
 freeswitch-mod-syslog-dbg (= \${binary:Version}),
 freeswitch-mod-posix-timer-dbg (= \${binary:Version}),
 freeswitch-mod-timerfd-dbg (= \${binary:Version}),
 freeswitch-mod-xml-cdr-dbg (= \${binary:Version}),
 freeswitch-mod-xml-curl-dbg (= \${binary:Version}),
 freeswitch-mod-xml-rpc-dbg (= \${binary:Version}),
 freeswitch-mod-xml-scgi-dbg (= \${binary:Version}),
Recommends:
Suggests:
 freeswitch-mod-vmd-dbg (= \${binary:Version}),
 freeswitch-mod-vlc-dbg (= \${binary:Version}),
Description: Cross-Platform Scalable Multi-Protocol Soft Switch
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which recommends or suggests all packaged
 FreeSWITCH modules.

Package: freeswitch-all-dbg
Section: debug
Priority: optional
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch-meta-all (= \${binary:Version}), freeswitch-meta-all-dbg (= \${binary:Version})
Description: debugging symbols for FreeSWITCH
 $(debian_wrap "${fs_description}")
 .
 This package contains debugging symbols for FreeSWITCH.

Package: freeswitch-dbg
Section: debug
Priority: optional
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch (= \${binary:Version})
Description: debugging symbols for FreeSWITCH
 $(debian_wrap "${fs_description}")
 .
 This package contains debugging symbols for FreeSWITCH.

Package: libfreeswitch1-dbg
Section: debug
Priority: optional
Architecture: amd64 armhf
Depends: \${misc:Depends}, libfreeswitch1 (= \${binary:Version})
Description: debugging symbols for FreeSWITCH
 $(debian_wrap "${fs_description}")
 .
 This package contains debugging symbols for libfreeswitch1.

Package: libfreeswitch-dev
Section: libdevel
Architecture: amd64 armhf
Depends: \${misc:Depends}, freeswitch
Description: development libraries and header files for FreeSWITCH
 $(debian_wrap "${fs_description}")
 .
 This package contains include files for FreeSWITCH.

Package: freeswitch-doc
Section: doc
Architecture: amd64 armhf
Depends: \${misc:Depends}
Description: documentation for FreeSWITCH
 $(debian_wrap "${fs_description}")
 .
 This package contains Doxygen-produced documentation for FreeSWITCH.
 It may be an empty package at the moment.

## misc

## languages

Package: freeswitch-lang
Architecture: amd64 armhf
Depends: \${misc:Depends},
 freeswitch-lang-en (= \${binary:Version})
Description: Language files for FreeSWITCH
 $(debian_wrap "${fs_description}")
 .
 This is a metapackage which depends on the default language packages
 for FreeSWITCH.

## timezones

Package: freeswitch-timezones
Architecture: amd64 armhf
Depends: \${misc:Depends}
Description: Timezone files for FreeSWITCH
 $(debian_wrap "${fs_description}")
 .
 $(debian_wrap "This package includes the timezone files for FreeSWITCH.")

## startup

EOF

if [ ${use_sysvinit} = "true" ]; then
    cat <<EOF
Package: freeswitch-sysvinit
Architecture: amd64 armhf
Depends: \${misc:Depends}, lsb-base (>= 3.0-6), sysvinit | sysvinit-utils
Conflicts: freeswitch-init
Provides: freeswitch-init
Description: FreeSWITCH SysV init script
 $(debian_wrap "${fs_description}")
 .
 This package contains the SysV init script for FreeSWITCH.

EOF
else
    cat <<EOF
Package: freeswitch-systemd
Architecture: amd64 armhf
Depends: \${misc:Depends}, systemd
Conflicts: freeswitch-init, freeswitch-all (<= 1.6.7)
Provides: freeswitch-init
Description: FreeSWITCH systemd configuration
 $(debian_wrap "${fs_description}")
 .
 This package contains the systemd configuration for FreeSWITCH.

EOF
fi
}

print_mod_control () {
  local m_section="${section:-comm}"
  cat <<EOF
Package: freeswitch-${module_name//_/-}
Section: ${m_section}
Architecture: amd64 armhf
$(debian_wrap "Depends: \${shlibs:Depends}, \${misc:Depends}, libfreeswitch1 (= \${binary:Version}), ${depends}")
$(debian_wrap "Recommends: ${recommends}")
$(debian_wrap "Suggests: freeswitch-${module_name//_/-}-dbg, ${suggests}")
Conflicts: freeswitch-all (<= 1.6.7)
Description: ${description} for FreeSWITCH
 $(debian_wrap "${fs_description}")
 .
 $(debian_wrap "This package contains ${module_name} for FreeSWITCH.")
 .
 $(debian_wrap "${long_description}")

Package: freeswitch-${module_name//_/-}-dbg
Section: debug
Priority: optional
Architecture: amd64 armhf
Depends: \${misc:Depends},
 freeswitch-${module_name//_/-} (= \${binary:Version})
Description: ${description} for FreeSWITCH (debug)
 $(debian_wrap "${fs_description}")
 .
 $(debian_wrap "This package contains debugging symbols for ${module_name} for FreeSWITCH.")
 .
 $(debian_wrap "${long_description}")

EOF
}

print_mod_install () {
  cat <<EOF
/usr/lib/freeswitch/mod/${1}.so
EOF
}

print_long_filename_override () {
  local p="$1"
  cat <<EOF
# The long file names are caused by appending the nightly information.
# Since one of these packages will never end up on a Debian CD, the
# related problems with long file names will never come up here.
${p}: package-has-long-file-name *

EOF
}

print_gpl_openssl_override () {
  local p="$1"
  cat <<EOF
# We're definitely not doing this.  Nothing in FreeSWITCH has a more
# restrictive license than LGPL or MPL.
${p}: possible-gpl-code-linked-with-openssl

EOF
}

print_itp_override () {
  local p="$1"
  cat <<EOF
# We're not in Debian (yet) so we don't have an ITP bug to close.
${p}: new-package-should-close-itp-bug

EOF
}

print_common_overrides () {
  print_long_filename_override "$1"
}

print_mod_overrides () {
  print_common_overrides "$1"
  print_gpl_openssl_override "$1"
}

print_conf_overrides () {
  print_common_overrides "$1"
}

print_conf_control () {
  cat <<EOF
Package: freeswitch-conf-${conf//_/-}
Architecture: amd64 armhf
Depends: \${misc:Depends}
Conflicts: freeswitch-all (<= 1.6.7)
Description: FreeSWITCH ${conf} configuration
 $(debian_wrap "${fs_description}")
 .
 $(debian_wrap "This package contains the ${conf} configuration for FreeSWITCH.")

EOF
}

print_conf_install () {
  cat <<EOF
conf/${conf} /usr/share/freeswitch/conf
EOF
}

print_lang_overrides () {
  print_common_overrides "$1"
}

print_lang_control () {
  local lang_name="$(echo ${lang} | tr '[:lower:]' '[:upper:]')"
  case "${lang}" in
    de) lang_name="German" ;;
    en) lang_name="English" ;;
    es) lang_name="Spanish" ;;
    fr) lang_name="French" ;;
    he) lang_name="Hebrew" ;;
    pt) lang_name="Portuguese" ;;
    ru) lang_name="Russian" ;;
  esac
  cat <<EOF
Package: freeswitch-lang-${lang//_/-}
Architecture: amd64 armhf
Depends: \${misc:Depends}
Recommends: freeswitch-sounds-${lang}
Conflicts: freeswitch-all (<= 1.6.7)
Description: ${lang_name} language files for FreeSWITCH
 $(debian_wrap "${fs_description}")
 .
 $(debian_wrap "This package includes the ${lang_name} language files for FreeSWITCH.")

EOF
}

print_lang_install () {
  cat <<EOF
conf/vanilla/lang/${lang} /usr/share/freeswitch/lang
EOF
}

print_edit_warning () {
  echo "#### Do not edit!  This file is auto-generated from debian/bootstrap.sh."; echo
}

gencontrol_per_mod () {
  print_mod_control "$module_name" "$description" "$long_description" >> control
}

gencontrol_per_cat () {
  (echo "## mod/$category"; echo) >> control
}

geninstall_per_mod () {
  local f=freeswitch-${module_name//_/-}.install
  (print_edit_warning; print_mod_install "$module_name") > $f
  test -f $f.tmpl && cat $f.tmpl >> $f
}

genoverrides_per_mod () {
  local f=freeswitch-${module_name//_/-}.lintian-overrides
  (print_edit_warning; print_mod_overrides freeswitch-${module_name//_/-}) > $f
  test -f $f.tmpl && cat $f.tmpl >> $f
}

genmodulesconf () {
  genmodules_per_cat () { echo "## $category"; }
  genmodules_per_mod () { echo "$module"; }
  print_edit_warning
  map_modules 'mod_filter' 'genmodules_per_cat' 'genmodules_per_mod'
}

genconf () {
  print_conf_control >> control
  local p=freeswitch-conf-${conf//_/-}
  local f=$p.install
  (print_edit_warning; print_conf_install) > $f
  test -f $f.tmpl && cat $f.tmpl >> $f
  local f=$p.lintian-overrides
  (print_edit_warning; print_conf_overrides "$p") > $f
  test -f $f.tmpl && cat $f.tmpl >> $f
}

genlang () {
  print_lang_control >> control
  local p=freeswitch-lang-${lang//_/-}
  local f=$p.install
  (print_edit_warning; print_lang_install) > $f
  test -f $f.tmpl && cat $f.tmpl >> $f
  local f=$p.lintian-overrides
  (print_edit_warning; print_lang_overrides "$p") > $f
  test -f $f.tmpl && cat $f.tmpl >> $f
}

geninstall_perl () {
  local archlib
  eval `perl -V:archlib`
  echo $archlib/ESL.\* >libesl-perl.install
  echo $archlib/ESL/\*.\* >>libesl-perl.install
}

accumulate_mod_deps () {
  local x=""
  # build-depends
  if [ -n "$(eval echo \$build_depends_$codename)" ]; then
    x="$(eval echo \$build_depends_$codename)"
  else x="${build_depends}"; fi
  if [ -n "$x" ]; then
    if [ ! "$mod_build_depends" = "." ]; then
      mod_build_depends="${mod_build_depends}, ${x}"
    else mod_build_depends="${x}"; fi; fi
  # depends
  if [ -n "$(eval echo \$depends_$codename)" ]; then
    x="$(eval echo \$depends_$codename)"
  else x="${depends}"; fi
  x="$(echo "$x" | sed 's/, \?/\n/g' | grep -v '^freeswitch' | tr '\n' ',' | sed -e 's/,$//' -e 's/,/, /g')"
  if [ -n "$x" ]; then
    if [ ! "$mod_depends" = "." ]; then
      mod_depends="${mod_depends}, ${x}"
    else mod_depends="${x}"; fi; fi
  # recommends
  if [ -n "$(eval echo \$recommends_$codename)" ]; then
    x="$(eval echo \$recommends_$codename)"
  else x="${recommends}"; fi
  x="$(echo "$x" | sed 's/, \?/\n/g' | grep -v '^freeswitch' | tr '\n' ',' | sed -e 's/,$//' -e 's/,/, /g')"
  if [ -n "$x" ]; then
    if [ ! "$mod_recommends" = "." ]; then
      mod_recommends="${mod_recommends}, ${x}"
    else mod_recommends="${x}"; fi; fi
  # suggests
  if [ -n "$(eval echo \$suggests_$codename)" ]; then
    x="$(eval echo \$suggests_$codename)"
  else x="${suggests}"; fi
  x="$(echo "$x" | sed 's/, \?/\n/g' | grep -v '^freeswitch' | tr '\n' ',' | sed -e 's/,$//' -e 's/,/, /g')"
  if [ -n "$x" ]; then
    if [ ! "$mod_suggests" = "." ]; then
      mod_suggests="${mod_suggests}, ${x}"
    else mod_suggests="${x}"; fi; fi
}

genmodctl_new_mod () {
  grep -e "^Module: ${module}$" control-modules >/dev/null && return 0
  cat <<EOF
Module: $module
Description: $description
 $long_description
EOF
  echo
}

genmodctl_new_cat () {
  grep -e "^## mod/${category}$" control-modules >/dev/null && return 0
  cat <<EOF
## mod/$category

EOF
}

pre_parse_mod_control () {
  local fl=true ll_nl=false ll_descr=false
  while xread l; do
    if [ -z "$l" ]; then
      # is newline
      if ! $ll_nl && ! $fl; then
        echo
      fi
      ll_nl=true
      continue
    elif [ -z "${l##\#*}" ]; then
      # is comment
      continue
    elif [ -z "${l## *}" ]; then
      # is continuation line
      if ! $ll_descr; then
        echo -n "$l"
      else
        echo -n "Long-Description: $(echo "$l" | sed -e 's/^ *//')"
      fi
    else
      # is header line
      $fl || echo
      if [ "${l%%:*}" = "Description" ]; then
        ll_descr=true
        echo "Description: ${l#*: }"
        continue
      else
        echo -n "$l"
      fi
    fi
    fl=false ll_nl=false ll_descr=false
  done < control-modules
}

var_escape () {
  (echo -n \'; echo -n "$1" | sed -e "s/'/'\\\\''/g"; echo -n \')
}

parse_mod_control () {
  pre_parse_mod_control > control-modules.preparse
  local category=""
  local module_name=""
  rm -rf $parse_dir
  while xread l; do
    if [ -z "$l" ]; then
      # is newline
      continue
    fi
    local header="${l%%:*}"
    local value="${l#*: }"
    if [ "$header" = "Module" ]; then
      category="${value%%/*}"
      module_name="${value#*/}"
      mkdir -p $parse_dir/$category
      (echo "module=$(var_escape "$value")"; \
        echo "category=$(var_escape "$category")"; \
        echo "module_name=$(var_escape "$module_name")"; \
        ) >> $parse_dir/$category/$module_name
    else
      ([ -n "$category" ] && [ -n "$module_name" ]) \
        || err "unexpected header $header"
      local var_name="$(echo "$header" | sed -e 's/-/_/g' | tr '[A-Z]' '[a-z]')"
      echo "${var_name}=$(var_escape "$value")" >> $parse_dir/$category/$module_name
    fi
  done < control-modules.preparse
}

debian_wrap () {
  local fl=true
  echo "$1" | fold -s -w 69 | while xread l; do
    local v="$(echo "$l" | sed -e 's/ *$//g')"
    if $fl; then
      fl=false
      echo "$v"
    else
      echo " $v"
    fi
  done
}

genmodctl_cat () {
  (echo "## mod/$category"; echo)
}

genmodctl_mod () {
  echo "Module: $module"
  [ -n "$section" ] && echo "Section: $section"
  echo "Description: $description"
  echo "$long_description" | fold -s -w 69 | while xread l; do
    local v="$(echo "$l" | sed -e 's/ *$//g')"
    echo " $v"
  done
  [ -n "$build_depends" ] && debian_wrap "Build-Depends: $build_depends"
  for x in $supported_distros; do
    [ -n "$(eval echo \$build_depends_$x)" ] \
      && debian_wrap "Build-Depends-$x: $(eval echo \$build_depends_$x)"
  done
  [ -n "$depends" ] && debian_wrap "Depends: $depends"
  [ -n "$reccomends" ] && debian_wrap "Recommends: $recommends"
  [ -n "$suggests" ] && debian_wrap "Suggests: $suggests"
  [ -n "$distro_conflicts" ] && debian_wrap "Distro-Conflicts: $distro_conflicts"
  echo
}

set_modules_non_dfsg () {
  local len=${#avoid_mods}
  for ((i=0; i<len; i++)); do
    case "${avoid_mods[$i]}" in
      codecs/mod_siren|codecs/mod_ilbc)
        unset avoid_mods[$i]
        ;;
    esac
  done
}

unavoid_modules () {
  local len=${#avoid_mods}
  for ((i=0; i<len; i++)); do
    for x in $1; do
      if test "${avoid_mods[$i]}" = "$x"; then
        unset avoid_mods[$i]
      fi
    done
  done
}

conf_merge () {
  local of="$1" if="$2"
  if [ -s $if ]; then
    grep -v '^##\|^$' $if | while xread x; do
      touch $of
      if ! grep -e "$x" $of >/dev/null; then
        printf '%s\n' "$x" >> $of
      fi
    done
  fi
}


echo "Bootstrapping debian/ for ${codename}" >&2
echo >&2
echo "Please wait, this takes a few seconds..." >&2

test -z "$modulelist_opt" || set_modules_${modulelist_opt/-/_}
test -z "$modules_add" || unavoid_modules "$modules_add"

echo "Adding any new modules to control-modules..." >&2
parse_dir=control-modules.parse
map_fs_modules ':' 'genmodctl_new_cat' 'genmodctl_new_mod' >> control-modules
echo "Parsing control-modules..." >&2
parse_mod_control
echo "Displaying includes/excludes..." >&2
map_modules 'mod_filter_show' '' ''
echo "Generating modules_.conf..." >&2
genmodulesconf > modules_.conf
echo "Generating control-modules.gen as sanity check..." >&2
(echo "# -*- mode:debian-control -*-"; \
  echo "##### Author: Travis Cross <tc@traviscross.com>"; echo; \
  map_modules ':' 'genmodctl_cat' 'genmodctl_mod' \
  ) > control-modules.gen

echo "Accumulating dependencies from modules..." >&2
map_modules 'mod_filter' '' 'accumulate_mod_deps'
echo "Generating debian/..." >&2
> control
(print_edit_warning; print_source_control; print_core_control) >> control
echo "Generating debian/ (conf)..." >&2
(echo "### conf"; echo) >> control
map_confs 'genconf'
echo "Generating debian/ (lang)..." >&2
(echo "### lang"; echo) >> control
map_langs 'genlang'
echo "Generating debian/ (modules)..." >&2
(echo "### modules"; echo) >> control
map_modules "mod_filter" \
  "gencontrol_per_cat" \
  "gencontrol_per_mod geninstall_per_mod genoverrides_per_mod"
geninstall_perl

if [ ${use_sysvinit} = "true" ]; then
  echo -n freeswitch-sysvinit >freeswitch-init.provided_by
else
  echo -n freeswitch-systemd >freeswitch-init.provided_by
fi


echo "Generating additional lintian overrides..." >&2
grep -e '^Package:' control | while xread l; do
  m="${l#*: }"
  f=$m.lintian-overrides
  [ -s $f ] || print_edit_warning >> $f
  if ! grep -e 'package-has-long-file-name' $f >/dev/null; then
    print_long_filename_override "$m" >> $f
  fi
  if ! grep -e 'new-package-should-close-itp-bug' $f >/dev/null; then
    print_itp_override "$m" >> $f
  fi
done
for p in freeswitch libfreeswitch1; do
  f=$p.lintian-overrides
  [ -s $f ] || print_edit_warning >> $f
  print_gpl_openssl_override "$p" >> $f
done

echo "Cleaning up..." >&2
rm -f control-modules.preparse
rm -rf control-modules.parse
diff control-modules control-modules.gen >/dev/null && rm -f control-modules.gen

echo "Done bootstrapping debian/" >&2
touch .stamp-bootstrap
