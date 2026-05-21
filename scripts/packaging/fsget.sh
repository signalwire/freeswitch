#!/bin/bash

# lint: shfmt -w -s -bn -ci -sr -fn scripts/packaging/fsget.sh

set -e          # Exit immediately if a command exits with a non-zero status
set -u          # Treat unset variables as an error
set -o pipefail # Return value of a pipeline is the value of the last (rightmost) command to exit with a non-zero status

print_usage()
{
	cat << EOF
Usage: $0 [TOKEN] [release|prerelease] [install|source|build-dep|showsrc]

  TOKEN        PAT (pat_*, ghapat_*) or FSA token (PT*). Optional if the
               FreeSWITCH apt repository is already configured.
  release      Use the stable repository (default when TOKEN is provided).
  prerelease   Use the unstable repository.

Actions (optional):
  install      Install freeswitch-meta-all after configuring the repository.
  source       Download FreeSWITCH source package for the build architecture.
  build-dep    Install build dependencies for FreeSWITCH on the build architecture.
  showsrc      Print the FreeSWITCH source version for the build architecture and exit.

If TOKEN is omitted the repository is assumed to be configured already and the
action is executed against the existing configuration.

Examples:
  $0 pat_xxxxx release install
  $0 PT_xxxxx prerelease build-dep
  $0 build-dep
  $0 showsrc
EOF
}

source_os_release()
{
	if [ ! -f /etc/os-release ]; then
		echo "Error: /etc/os-release not found"
		exit 1
	fi
	. /etc/os-release
}

print_os_identification()
{
	echo -n "Operating system identification:"
	[ -n "$ID" ] && echo -n " ID=$ID"
	[ -n "$VERSION_CODENAME" ] && echo -n " CODENAME=$VERSION_CODENAME"
	echo
}

setup_common()
{
	rm -f /etc/apt/sources.list.d/freeswitch.list
	rm -f /etc/apt/sources.list.d/freeswitch.sources
	apt-get update && apt-get install -y \
		apt-transport-https \
		curl \
		gnupg2 \
		grep
}

configure_auth()
{
	local domain=$1
	local username=${2:-signalwire}
	local token=$3
	if ! grep -q "machine ${domain}" /etc/apt/auth.conf; then
		echo "machine ${domain} login ${username} password ${token}" >> /etc/apt/auth.conf
		chmod 600 /etc/apt/auth.conf
	fi
}

# `apt-get source` extracts the source tree with `dpkg-source`, which lives in
# `dpkg-dev` — only pull that package in when we actually need extraction.
# Callers must run `apt-get update` before invoking this (the `source` action
# already does so as part of the version lookup).
ensure_dpkg_source()
{
	if ! command -v dpkg-source > /dev/null 2>&1; then
		apt-get install -y dpkg-dev
	fi
}

# FreeSWITCH source versions encode build arch in the version string (e.g.
# 1.10.X-release.YYYYMMDDHHMMSS+1.debNN-amd64). Plain `apt-get source freeswitch`
# or `apt-get build-dep freeswitch` may pick a candidate that does not match the
# current architecture, so we filter showsrc output by the dpkg architecture.
get_freeswitch_source_version()
{
	apt-cache showsrc freeswitch \
		| grep "^Version" \
		| grep "$(dpkg --print-architecture)" \
		| awk '{print $2}' \
		| head -n 1
}

run_action()
{
	local edition="$1"
	local action="$2"
	local label="FreeSWITCH"
	[ -n "${edition}" ] && label="FreeSWITCH ${edition}"

	case "${action}" in
		"")
			echo "------------------------------------------------------------------"
			echo " Done configuring ${label} Debian repository"
			echo "------------------------------------------------------------------"
			echo "To install ${label} type: apt-get install -y freeswitch-meta-all"
			;;
		install)
			apt-get update
			echo "Installing ${label}"
			apt-get install -y freeswitch-meta-all
			echo "------------------------------------------------------------------"
			echo " Done installing ${label}"
			echo "------------------------------------------------------------------"
			;;
		source)
			apt-get update
			local version
			version=$(get_freeswitch_source_version)
			if [ -z "${version}" ]; then
				echo "Error: Unable to determine FreeSWITCH source version for $(dpkg --print-architecture)"
				exit 1
			fi
			ensure_dpkg_source
			echo "Downloading ${label} source (version: ${version})"
			apt-get source "freeswitch=${version}"
			echo "------------------------------------------------------------------"
			echo " Done downloading ${label} source (version: ${version})"
			echo "------------------------------------------------------------------"
			;;
		build-dep)
			apt-get update
			local version
			version=$(get_freeswitch_source_version)
			if [ -z "${version}" ]; then
				echo "Error: Unable to determine FreeSWITCH source version for $(dpkg --print-architecture)"
				exit 1
			fi
			echo "Installing ${label} build dependencies (version: ${version})"
			apt-get build-dep -y "freeswitch=${version}"
			echo "------------------------------------------------------------------"
			echo " Done installing ${label} build dependencies (version: ${version})"
			echo "------------------------------------------------------------------"
			;;
		showsrc)
			local version
			version=$(get_freeswitch_source_version)
			if [ -z "${version}" ]; then
				echo "Error: Unable to determine FreeSWITCH source version for $(dpkg --print-architecture)"
				exit 1
			fi
			echo "${version}"
			;;
		*)
			echo "Error: Unknown action '${action}'"
			print_usage
			exit 1
			;;
	esac
}

if [ "$(id -u)" != "0" ]; then
	echo "Non-root user detected. Execution may fail."
fi

if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
	print_usage
	exit 1
fi

TOKEN=""
RELEASE="release"
ACTION=""

for arg in "$@"; do
	case "${arg}" in
		release | prerelease)
			RELEASE="${arg}"
			;;
		install | source | build-dep | showsrc)
			ACTION="${arg}"
			;;
		pat_* | ghapat_* | PT* | not_a_real_token_but_a_dummy_token_*)
			TOKEN="${arg}"
			;;
		*)
			echo "Error: Unrecognized argument '${arg}'"
			print_usage
			exit 1
			;;
	esac
done

if [ -z "${TOKEN}" ] && [ -z "${ACTION}" ]; then
	echo "Error: Either a TOKEN or an action must be specified."
	print_usage
	exit 1
fi

source_os_release

if [ "${ID,,}" != "debian" ]; then
	echo "Unrecognized OS. We support Debian only."
	exit 1
fi

# `showsrc` is meant to be machine-readable (a single version line); keep its
# stdout clean. All other paths print the OS identification for context.
if [ "${ACTION}" != "showsrc" ]; then
	print_os_identification
fi

ARCH=$(dpkg --print-architecture)
EDITION=""

if [ -n "${TOKEN}" ]; then
	if [[ ${TOKEN} == pat_* || ${TOKEN} == ghapat_* || ${TOKEN} == not_a_real_token_but_a_dummy_token_* ]]; then
		DOMAIN="freeswitch.signalwire.com"
		GPG_KEY="/usr/share/keyrings/signalwire-freeswitch-repo.gpg"
		RPI=""
		EDITION="Community"

		if [ "${RELEASE,,}" = "prerelease" ]; then
			RELEASE="unstable"
		else
			RELEASE="release"
		fi

		if [ "${ARCH,,}" = "armhf" ]; then
			RPI="rpi/"
		fi

		echo "FreeSWITCH ${EDITION} (${RELEASE})"

		setup_common
		configure_auth "${DOMAIN}" "" "${TOKEN}"

		curl \
			--fail \
			--netrc-file /etc/apt/auth.conf \
			--output ${GPG_KEY} \
			https://${DOMAIN}/repo/deb/${RPI}debian-release/signalwire-freeswitch-repo.gpg

		echo "Types: deb deb-src" > /etc/apt/sources.list.d/freeswitch.sources
		echo "URIs: https://${DOMAIN}/repo/deb/${RPI}debian-${RELEASE}/" >> /etc/apt/sources.list.d/freeswitch.sources
		echo "Suites: ${VERSION_CODENAME}" >> /etc/apt/sources.list.d/freeswitch.sources
		echo "Components: main" >> /etc/apt/sources.list.d/freeswitch.sources
		echo "Signed-By: ${GPG_KEY}" >> /etc/apt/sources.list.d/freeswitch.sources
	elif [[ ${TOKEN} == PT* ]]; then
		DOMAIN="fsa.freeswitch.com"
		GPG_KEY="/usr/share/keyrings/signalwire-freeswitch-advantage-repo.gpg"
		RPI=""
		EDITION="Enterprise"

		if [ "${RELEASE,,}" = "prerelease" ]; then
			RELEASE="unstable"
		else
			RELEASE="1.8"
		fi

		if [ "${ARCH,,}" = "armhf" ]; then
			RPI="-rpi"
		fi

		echo "FreeSWITCH ${EDITION} (${RELEASE})"

		setup_common
		configure_auth "${DOMAIN}" "" "${TOKEN}"

		curl \
			--fail \
			--netrc-file /etc/apt/auth.conf \
			--output ${GPG_KEY} \
			https://${DOMAIN}/repo/deb/fsa${RPI}/keyring.gpg

		echo "Types: deb deb-src" > /etc/apt/sources.list.d/freeswitch.sources
		echo "URIs: https://${DOMAIN}/repo/deb/fsa${RPI}/" >> /etc/apt/sources.list.d/freeswitch.sources
		echo "Suites: ${VERSION_CODENAME}" >> /etc/apt/sources.list.d/freeswitch.sources
		echo "Components: ${RELEASE}" >> /etc/apt/sources.list.d/freeswitch.sources
		echo "Signed-By: ${GPG_KEY}" >> /etc/apt/sources.list.d/freeswitch.sources
	fi
fi

run_action "${EDITION}" "${ACTION}"
