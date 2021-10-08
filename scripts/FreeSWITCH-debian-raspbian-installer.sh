#!/bin/sh
# (C) 2016 Ken Rice <krice@freeswitch.org>
# Licensed as per the MPL1.1
#
########################################################
# TODO: FreeSWITCH AutoStart
# TODO: Allow Selection of Source or Package Install on Debian

DIALOG=${DIALOG=dialog}
tempfile=`tempfile 2>/dev/null` || tempfile=/tmp/test$$
trap "rm -f $tempfile" 0 1 2 5 15

. /etc/os-release

install_prereqs() {
	#install the prereqs
	echo "Making sure we have the prereqs for this script to run. Please Stand by..."
	apt-get update 2>&1 >/dev/null
	apt-get install -y curl dialog git ntpdate 2>&1 >/dev/null

	# See if ntpd is running if it is, stop it set the current time as rpi has no RTC and this is needed
	# for SSL to function properly

	if pgrep "ntpd" >/dev/null ; then
		/etc/init.d/ntp stop
		ntpdate pool.ntp.org
		/etc/init.d/ntp start
	else
		ntpdate pool.ntp.org
	fi
}

welcome_screen() {
	$DIALOG --title "FreeSWITCH with LetsEncrypt AutoInstaller" --clear \
		--msgbox "This Script with automattically Install FreeSWITCH \
On your Debian 8 Jessie Machine, it will also install \
Verto Communicator and use LetsEncrypt for the required \
SSL Certificates needed for Proper WebRTC Communications.\n\n\
Please keep in mind that you will need a proper DNS \
Name pointed at this machine's public IP address along \
with ports 80 and 443 opened on the firewall. \n\n\
Additionally, you will need TCP ports 5060, 5061, 8081, \
8082 and UDP ports 16384-32768 open on your firewall for \
FreeSWITCH and Verto Communicator for function properly. \n\n\
Press <Enter> to Continue or <ESC> to abort." 19 60

	case $? in
		0)
			;;
		255)
			exit 1;;
	esac
}

fs_ver_select() {
	$DIALOG --backtitle "FreeSWITCH Version" \
		--title "RADIOLIST BOX" --clear \
		--radiolist "Which Version of FreeSWITCH are you installing? \n" 20 61 5 \
		"1"  "FreeSWITCH 1.7" ON \
		"2"  "FreeSWITCH 1.6" off 2> $tempfile

	retval=$?

	choice=`cat $tempfile`
	case $retval in
		0)
			case $choice in
				1) 
					FS_REV="master";;
				2)
					FS_REV="1.6";;
			esac;;
		1)
			exit 1;;
		255)
			exit 1;;
	esac
}

get_network_settings() {
	FQDN=`hostname -f`
	DOMAIN=`hostname -d`
	IPADDR=`ifconfig | sed -En 's/127.0.0.1//;s/.*inet (addr:)?(([0-9]*\.){3}[0-9]*).*/\2/p'|tail -n 1`
	EMAIL="hostmaster@$DOMAIN";

	dialog --title "System Setup Information" \
		--form "\nVerify or correct the Fully Qualified Domain Name and IP Address of your machine.\nAlso enter a valid Email Address for system and LetsEncrypt email alerts" 25 60 16 \
		"FQDN:" 1 1 "$FQDN" 1 25 25 40 \
		"IP Address:" 2 1 "$IPADDR" 2 25 25 30 \
		"Email Address:" 3 1 "$EMAIL" 3 25 25 40 \
		2> $tempfile
	FQDN=`head -n1 $tempfile`
	IPADDR=`tail -n2 $tempfile|head -n1`
	EMAIL=`tail -n1 $tempfile`

	retval=$?

	case $retval in
		0)
			;;
		1)
			exit 1;;
		255)
			exit 1;;
	esac
}

is_private_ip() {
	PAT='^10\.|^192\.168\.|^169\.254\.|^172\.1[6-9]\.|^172\.2[0-9]\.|^172\.3[0-1]\.'
	echo $IPADDR | egrep "$PAT"
}

verify_ip_fqdn() {
	DNSIP=`dig +noall +answer @4.2.2.2 $FQDN | awk '{print $5}'`

	dialog --title "NO DNS For this FQDN" --clear \
		--menu "The FQDN and IP Address do not match what is available in Public DNS Servers." 15 60 5 \
	1 "Continue installation without LetsEncrypt." 2 "Abort Installation" 2> $tempfile
	LE_CHOICE=`cat $tempfile`
	if [ "$IPADDR" != "$DNSIP" ]; then

		if [ "x$LE_CHOICE" = "x1" ]; then
			VIPFQDN=1
		else 
			VIPFQDN=2
		fi 
	else
		VIPFQDN=0
	fi
}

config_fs_repos() {
	curl https://files.freeswitch.org/repo/deb/debian/freeswitch_archive_g0.pub | apt-key add -
	if [ "$FS_REV" = "master" ]; then
		echo "deb http://files.freeswitch.org/repo/deb/debian-unstable/ jessie main" >/etc/apt/sources.list.d/freeswitch.list
		REPO="https://freeswitch.org/stash/scm/fs/freeswitch.git"
	elif [ "$FS_REV" = "1.6" ]; then
		echo "deb http://files.freeswitch.org/repo/deb/freeswitch-1.6/ jessie main" > /etc/apt/sources.list.d/freeswitch.list
		REPO="-b v1.6 https://silik0n@freeswitch.org/stash/scm/fs/freeswitch.git"
	fi
	apt-get update 2>&1 >/dev/null
}

get_fs_source() {
	echo "REPO = $REPO"
	if [ ! -d /usr/src/freeswitch.git ]; then
		cd /usr/src
		git clone $REPO freeswitch.git
	else
		cd /usr/src/freeswitch.git
		git clean -fdx
		git reset --hard origin/$FS_REV
		git pull
	fi
}

get_letsencrypt() {
	if [ ! -d /usr/src/letsencrypt ]; then
		cd /usr/src
		git clone https://github.com/letsencrypt/letsencrypt.git letsencrypt
	else
		git clean -fdx
		git pull
	fi
}

install_certs() {
	get_letsencrypt
	cd /usr/src/letsencrypt
	NEED_CERTS_INSTALL=1

	if [ -f /etc/letsencrypt/live/$FQDN/cert.pem ]; then
		if openssl x509 -checkend 2592000 -noout -in /etc/letsencrypt/live/$FQDN/cert.pem; then
			echo "Skipping LetsEncrypt These Certs are good for atleast 30 days."
			NEED_CERTS_INSTALL=0
		else
			echo "Renewing LetsEncrypt Certs as they will expire in the next 30 days."
			./letsencrypt-auto renew  --manual-public-ip-logging-ok
		fi
	else
		echo "Setting up LetsEncrypt and getting you some nice new Certs for this Server."
		./letsencrypt-auto run -d $FQDN --email $EMAIL
	fi

	# if we dont have the FreeSWITCH Certs Directory, make it
	if [ $NEED_CERTS_INSTALL -eq 1 ]; then

		if [ ! -d /usr/local/freeswitch/certs ]; then
			mkdir -p /usr/local/freeswitch/certs
		fi

		cat /etc/letsencrypt/live/$FQDN/cert.pem /etc/letsencrypt/live/$FQDN/privkey.pem \
			/etc/letsencrypt/live/$FQDN/chain.pem > /usr/local/freeswitch/certs/wss.pem
	fi

}

build_fs() {
	get_fs_source

	#if we already have a FreeSWITCH install from source clean out the old bins
	if [ -d /usr/local/freeswitch/bin ]; then
		rm -rf /usr/local/freeswitch/{bin,mod,lib}/*
	fi
	cd /usr/src/freeswitch.git
	if [ ! -d /usr/src/freeswitch.git/configure ]; then
		./bootstrap.sh -j
	fi
	./configure -C
	make -j$JLIMIT install
	make uhd-sounds-install
	make uhd-moh-install
}

install_vc() {
	if [ ! -d /usr/src/freeswitch.git/html5/verto/verto_communicator ]; then
		get_fs_source
	fi

	if [ ! -x /usr/sbin/apache2 ]; then
		apt-get update 2>&1 >/dev/null
		apt-get install -y apache2
	fi	

	cd /usr/src/freeswitch.git/html5/verto/verto_communicator
	apt-get update 
	apt-get install npm nodejs-legacy -y
	npm install -g grunt grunt-cli bower
	npm install
	bower --allow-root install
	grunt build
	cp -a dist /var/www/html/vc
}


freeswitch_debian_packages() {
	apt-get install -o Dpkg::Progress=1 -y freeswitch-all freeswitch-all-dbg gdb 2>&1 | awk -W interactive '/Progress/ { print }'| \
		sed -u 's/[^0-9]//g' | dialog --gauge "Please wait.\n Installing FreeSWITCH..." 10 70 0
}

freeswitch_debian_source() {
	apt-get install -o Dpkg::Progress=1 -y freeswitch-video-deps-most \
		2>&1 | awk -W interactive '/Progress/ { print }'| sed -u 's/[^0-9]//g' | \
		dialog --gauge "Please wait.\n Installing Build Requirements..." 10 70 0

	build_fs
}

freeswitch_raspbian_source() {
	apt-get install -o Dpkg::Progress=1 -y autoconf automake devscripts gawk libjpeg-dev libncurses5-dev libtool-bin python-dev \
		libtiff5-dev libperl-dev libgdbm-dev libdb-dev gettext libssl-dev libcurl4-openssl-dev libpcre3-dev libspeex-dev \
		libspeexdsp-dev libsqlite3-dev libedit-dev libldns-dev libpq-dev libsndfile-dev libopus-dev liblua5.1-0-dev 2>&1 | \
		awk -W interactive '/Progress/ { print }'| sed -u 's/[^0-9]//g' | dialog --gauge "Please wait.\n Installing Build Requirements..." 10 70 0
	build_fs

}

# install_prereqs
welcome_screen
fs_ver_select
get_network_settings

if [ "$ID" = "debian" ]; then
	## These only work on Jessie at this time
	config_fs_repos
	freeswitch_debian_source
elif [ "$ID" = "raspbian" ]; then	
	JLIMIT="3"
	freeswitch_raspbian_source
fi

install_vc

PRIVIP=$(is_private_ip)
if [ "x$PRIVIP" != "x$IPADDR" ]; then
	verify_ip_fqdn
	if [ $VIPFQDN -eq 2 ]; then
		exit 1;
	elif [ $VIPFQDN -eq 1 ]; then
		echo "Skipping LetsEncrypt\n"
	else 
		get_letsencrypt
		install_certs
	fi
else
	echo "Skipping LetsEncrypt. Since we are on a Private IP Address";
fi

