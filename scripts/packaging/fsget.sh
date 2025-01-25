#!/bin/bash

# lint: shfmt -w -s -bn -ci -sr -fn scripts/packaging/fsget.sh

set -e          # Exit immediately if a command exits with a non-zero status
set -u          # Treat unset variables as an error
set -o pipefail # Return value of a pipeline is the value of the last (rightmost) command to exit with a non-zero status

source_os_release()
{
	if [ ! -f /etc/os-release ]; then
		echo "Error: /etc/os-release not found"
		exit 1
	fi
	. /etc/os-release

	echo -n "Operating system identification:"
	[ -n "$ID" ] && echo -n " ID=$ID"
	[ -n "$VERSION_CODENAME" ] && echo -n " CODENAME=$VERSION_CODENAME"
	echo
}

setup_common()
{
	rm -f /etc/apt/sources.list.d/freeswitch.list
	apt-get update && apt-get install -y \
		apt-transport-https \
		curl \
		gnupg2 \
		grep \
		software-properties-common
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

install_freeswitch()
{
	local edition="$1"
	local action="$2"

	apt-get update

	if [ "${action}" = "install" ]; then
		echo "Installing FreeSWITCH ${edition}"
		apt-get install -y freeswitch-meta-all
		echo "------------------------------------------------------------------"
		echo " Done installing FreeSWITCH ${edition}"
		echo "------------------------------------------------------------------"
	else
		echo "------------------------------------------------------------------"
		echo " Done configuring FreeSWITCH Debian repository"
		echo "------------------------------------------------------------------"
		echo "To install FreeSWITCH ${edition} type: apt-get install -y freeswitch-meta-all"
	fi
}

if [ "$(id -u)" != "0" ]; then
	echo "Non-root user detected. Execution may fail."
fi

if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
	echo "Usage: $0 <PAT or FSA token> [[release|prerelease] [install]]"
	exit 1
fi

TOKEN=$1
RELEASE="${2:-release}"
ACTION="${3:-}"

source_os_release

if [ "${ID,,}" = "debian" ]; then
	ARCH=$(dpkg --print-architecture)

	if [[ ${TOKEN} == pat_* ]]; then
		DOMAIN="freeswitch.signalwire.com"
		GPG_KEY="/usr/share/keyrings/signalwire-freeswitch-repo.gpg"
		RPI=""

		if [ "${RELEASE,,}" = "prerelease" ]; then
			RELEASE="unstable"
		else
			RELEASE="release"
		fi

		if [ "${ARCH,,}" = "armhf" ]; then
			RPI="rpi/"
		fi

		echo "FreeSWITCH Community ($RELEASE)"

		setup_common
		configure_auth "${DOMAIN}" "" "${TOKEN}"

		curl \
			--fail \
			--netrc-file /etc/apt/auth.conf \
			--output ${GPG_KEY} \
			https://${DOMAIN}/repo/deb/${RPI}debian-release/signalwire-freeswitch-repo.gpg

		echo "deb [signed-by=${GPG_KEY}] https://${DOMAIN}/repo/deb/${RPI}debian-${RELEASE}/ ${VERSION_CODENAME} main" > /etc/apt/sources.list.d/freeswitch.list
		echo "deb-src [signed-by=${GPG_KEY}] https://${DOMAIN}/repo/deb/${RPI}debian-${RELEASE}/ ${VERSION_CODENAME} main" >> /etc/apt/sources.list.d/freeswitch.list

		install_freeswitch "Community" "${ACTION}"
	elif [[ ${TOKEN} == PT* ]]; then
		DOMAIN="fsa.freeswitch.com"
		RPI=""

		if [ "${RELEASE,,}" = "prerelease" ]; then
			RELEASE="unstable"
		else
			RELEASE="1.8"
		fi

		if [ "${ARCH,,}" = "armhf" ]; then
			RPI="-rpi"
		fi

		echo "FreeSWITCH Enterprise ($RELEASE)"

		setup_common
		configure_auth "${DOMAIN}" "" "${TOKEN}"

		curl \
			--fail \
			--netrc-file /etc/apt/auth.conf \
			https://${DOMAIN}/repo/deb/fsa${RPI}/pubkey.gpg | tee /etc/apt/trusted.gpg.d/freeswitch-enterprise.asc

		echo "deb https://${DOMAIN}/repo/deb/fsa${RPI}/ ${VERSION_CODENAME} ${RELEASE}" > /etc/apt/sources.list.d/freeswitch.list
		echo "deb-src https://${DOMAIN}/repo/deb/fsa${RPI}/ ${VERSION_CODENAME} ${RELEASE}" >> /etc/apt/sources.list.d/freeswitch.list

		install_freeswitch "Enterprise" "${ACTION}"
	else
		echo "Unrecognized token type"
	fi
else
	echo "Unrecognized OS. We support Debian only."
fi
