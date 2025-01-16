#!/bin/bash

TOKEN=$1
RELEASE=$2
INSTALL=$3

# Source the os-release file (assuming it exists)
. /etc/os-release
echo $ID
echo $VERSION_CODENAME

if [ "${ID,,}" = "debian" ]; then
	ARCH=$(dpkg --print-architecture)
	if [[ "${TOKEN}" == pat_* ]]; then
		echo "FreeSWITCH Community"

		RPI=""

		if [ "${RELEASE,,}" = "prerelease" ]; then
			RELEASE="unstable"
		else
			RELEASE="release"
		fi

		echo $RELEASE

		if [ "${ARCH,,}" = "armhf" ]; then
			RPI="rpi/"
		fi

		rm -f /etc/apt/sources.list.d/freeswitch.list
		apt-get update && apt-get install -y gnupg2 wget software-properties-common apt-transport-https

		wget --http-user=signalwire --http-password=$TOKEN -O /usr/share/keyrings/signalwire-freeswitch-repo.gpg https://freeswitch.signalwire.com/repo/deb/${RPI}debian-release/signalwire-freeswitch-repo.gpg
		echo "machine freeswitch.signalwire.com login signalwire password $TOKEN" > /etc/apt/auth.conf
		chmod 600 /etc/apt/auth.conf
		echo "deb [signed-by=/usr/share/keyrings/signalwire-freeswitch-repo.gpg] https://freeswitch.signalwire.com/repo/deb/${RPI}debian-${RELEASE}/ ${VERSION_CODENAME} main" > /etc/apt/sources.list.d/freeswitch.list
		echo "deb-src [signed-by=/usr/share/keyrings/signalwire-freeswitch-repo.gpg] https://freeswitch.signalwire.com/repo/deb/${RPI}debian-${RELEASE}/ ${VERSION_CODENAME} main" >> /etc/apt/sources.list.d/freeswitch.list

		apt-get update
		if [ "${INSTALL}" = "install" ]; then
			echo "Installing FreeSWITCH Community"
			apt-get install -y freeswitch-meta-all
			echo "------------------------------------------------------------------"
			echo " Done installing FreeSWITCH Community"
			echo "------------------------------------------------------------------"
		else
			echo "------------------------------------------------------------------"
			echo " Done configuring FreeSWITCH Debian repository"
			echo "------------------------------------------------------------------"
			echo "To install FreeSWITCH Community type: apt-get install -y freeswitch-meta-all"
		fi
	elif [[ "${TOKEN}" == PT* ]]; then
		echo "FreeSWITCH Enterprise"

		if [ "${RELEASE,,}" = "prerelease" ]; then
			RELEASE="unstable"
		else
			RELEASE="1.8"
		fi

		echo $RELEASE

		if [ "${ARCH,,}" = "armhf" ]; then
			RPI="-rpi"
		fi

		rm -f /etc/apt/sources.list.d/freeswitch.list
		apt-get update && apt-get install -y gnupg2 wget software-properties-common apt-transport-https

		wget --http-user=signalwire --http-password=$TOKEN -O - https://fsa.freeswitch.com/repo/deb/fsa${RPI}/pubkey.gpg | apt-key add -
		echo "machine fsa.freeswitch.com login signalwire password $TOKEN" > /etc/apt/auth.conf
		chmod 600 /etc/apt/auth.conf
		echo "deb https://fsa.freeswitch.com/repo/deb/fsa${RPI}/ ${VERSION_CODENAME} ${RELEASE}" > /etc/apt/sources.list.d/freeswitch.list
		echo "deb-src https://fsa.freeswitch.com/repo/deb/fsa${RPI}/ ${VERSION_CODENAME} ${RELEASE}" >> /etc/apt/sources.list.d/freeswitch.list

		apt-get update
		if [ "${INSTALL}" = "install" ]; then
			echo "Installing FreeSWITCH Enterprise"
			apt-get install -y freeswitch-meta-all
			echo "------------------------------------------------------------------"
			echo " Done installing FreeSWITCH Enterprise"
			echo "------------------------------------------------------------------"
		else
			echo "------------------------------------------------------------------"
			echo " Done configuring FreeSWITCH Debian repository"
			echo "------------------------------------------------------------------"
			echo "To install FreeSWITCH Enterprise type: apt-get install -y freeswitch-meta-all"
		fi
	else
		echo "Unrecognized token type"
	fi
else
	echo "Unrecognized OS. We support Debian only."
fi