#
# /etc/profile: system-wide defaults for bash(1) login shells
#
export UNAME=`uname -s`

if [ "`id -u`" = "0" ]; then
    if [ "${UNAME}" = "Linux" ]; then
	export PATH="$PATH:/opt/bin:/usr/local/bin:/usr/local/sbin:/usr/local/freeswitch/bin"
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig
    fi
    if [ "${UNAME}" = "SunOS" ]; then
	export PATH="$PATH:/opt/64/bin:/opt/bin:/usr/local/bin:/usr/local/sbin:/usr/local/freeswitch/bin"
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/opt/64/lib/pkgconfig:/usr/lib/amd64/pkgconfig:/opt/lib/pkgconfig:/usr/lib/pkgconfig
    fi
    if [ "${UNAME}" = "OpenBSD" ]; then
	export PATH="$PATH:/usr/local/freeswitch/bin"
	export AUTOCONF_VERSION=2.69
	export AUTOMAKE_VERSION=1.13
	export LIBTOOL=/usr/local/bin/libtoolize
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
    fi
    if [ "${UNAME}" = "NetBSD" ]; then
	export PATH="$PATH:/usr/local/freeswitch/bin"
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/lib/pkgconfig:/usr/pkg/lib/pkgconfig
	export PKG_PATH=ftp://199.233.217.249/pub/pkgsrc/packages/NetBSD/amd64/6.1.3/All
    fi
    if [ "${UNAME}" = "FreeBSD" ]; then
	export PATH="$PATH:/usr/local/freeswitch/bin"
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/lib/pkgconfig:/usr/pkg/lib/pkgconfig
    fi
    if [ "${UNAME}" = "DragonFly" ]; then
	export PATH="$PATH:/usr/local/freeswitch/bin"
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/lib/pkgconfig:/usr/pkg/lib/pkgconfig
    fi
fi

if [ ! -f ~/.inputrc ]; then
    export INPUTRC="/etc/inputrc"
fi

set -o emacs

export LESSCHARSET="latin1"
export LESS="-R"
export CHARSET="ISO-8859-1"
export PS1='\n\[\033[01;31m\]\u@\h\[\033[01;36m\] [\d \@] \[\033[01;33m\] \w\n\[\033[00m\]<\#>:'
export PS2="\[\033[1m\]> \[\033[0m\]"
export VISUAL=emacs
export GIT_SSL_NO_VERIFY=true

umask 022
alias e='emacs'
alias eamcs='emacs'
alias emcas='emacs'
alias meacs='emacs'
alias mecas='emacs'
alias bgit='git commit --author "Brian West <brian@freeswitch.org>"'
alias mgit='git commit --author "Mike Jerris <mike@freeswitch.org>"'
alias tgit='git commit --author "Anthony Minessale <anthm@freeswitch.org>"'
alias igit='git commit --author "Raymond Chandler <intralanman@freeswitch.org>"'
alias dp='emacs /usr/local/freeswitch/conf/dialplan/default.xml'
alias go='/usr/local/freeswitch/bin/freeswitch -nonat'
alias fstop='top -p `cat /usr/local/freeswitch/run/freeswitch.pid`'
alias fsgdb='gdb /usr/local/freeswitch/bin/freeswitch `cat /usr/local/freeswitch/run/freeswitch.pid`'
alias fscore='gdb /usr/local/freeswitch/bin/freeswitch `ls -rt core.* | tail -n1`'
alias emacs='emacs -nw'
alias jitteron='tc qdisc add dev eth0 root handle 1: netem delay 40ms 20ms ; tc qdisc add dev eth0 parent 1:1 pfifo limit 1000'
# End of file
