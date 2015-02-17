#
# FreeSWITCH Dev .bashrc
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
	export PKG_PATH=http://openbsd.mirrors.pair.com/`uname -r`/packages/`machine -a`/
    fi
    if [ "${UNAME}" = "NetBSD" ]; then
	export PATH="$PATH:/usr/local/freeswitch/bin"
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/lib/pkgconfig:/usr/pkg/lib/pkgconfig
	export PKG_PATH=http://ftp.netbsd.org/pub/pkgsrc/packages/NetBSD/`uname -m`/`uname -r`/All/
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

if [ "${UNAME}" = "Darwin" ]; then
    if [ -d ~/src/depot_tools ]; then
	export PATH=$PATH:~/src/depot_tools
    fi
    if [ -d "/Applications/Chromium.app" ]; then
	alias chromium='CHROME_LOG_FILE=chrome.log /Applications/Chromium.app/Contents/MacOS/Chromium --args --enable-usermedia-screen-capturing --usermedia-screen-capturing  --enable-logging --v=1 --vmodule=*source*/talk/*=5 2>&1 | tee console.log'
    fi
    if [ -d "/Applications/Google Chrome Canary.app" ]; then
	alias canary='CHROME_LOG_FILE=chrome.log /Applications/Google\ Chrome\ Canary.app/Contents/MacOS/Google\ Chrome\ Canary --args --enable-usermedia-screen-capturing --usermedia-screen-capturing  --enable-logging --v=1 --vmodule=*source*/talk/*=5 2>&1 | tee console.log'
    fi
    if [ -d "/Applications/Google Chrome.app" ]; then
	alias chrome='CHROME_LOG_FILE=chrome.log /Applications/Google\ Chrome.app/Concd outtents/MacOS/Google\ Chrome --args --enable-usermedia-screen-capturing --usermedia-screen-capturing  --enable-logging --v=1 --vmodule=*source*/talk/*=5 2>&1 | tee console.log'
    fi
fi

if [ ! -f ~/.inputrc ]; then
    export INPUTRC="/etc/inputrc"
fi

set -o emacs
export PROMPT_COMMAND="history -a; history -c; history -r; ${PROMPT_COMMAND}"
export HISTSIZE=5000
export TERM=xterm-256color
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
alias mgit='git commit --author "Mike Jerris <mike@jerris.com>"'
alias tgit='git commit --author "Anthony Minessale <anthm@freeswitch.org>"'
alias dp='emacs /usr/local/freeswitch/conf/dialplan/default.xml'
alias go='/usr/local/freeswitch/bin/freeswitch -nonat'
alias fstop='top -p `cat /usr/local/freeswitch/run/freeswitch.pid`'
alias fsgdb='gdb /usr/local/freeswitch/bin/freeswitch `cat /usr/local/freeswitch/run/freeswitch.pid`'
alias fscore='gdb /usr/local/freeswitch/bin/freeswitch `ls -rt core.* | tail -n1`'
alias emacs='emacs -nw'

if [ "${UNAME}" = "Linux" ]; then
    alias jitteron='tc qdisc add dev eth0 root handle 1: netem delay 40ms 20ms ; tc qdisc add dev eth0 parent 1:1 pfifo limit 1000'
    alias jitteroff='tc qdisc del dev eth0 root netem'
fi

# Auto Update the .bashrc if hostname contains freeswitch.org
if [[ $(hostname) =~ "freeswitch.org" ]]; then
    if [ -f /usr/src/freeswitch.git/support-d/.bashrc ]; then  
	/usr/bin/diff --brief <(sort /usr/src/freeswitch.git/support-d/.bashrc) <(sort ~/.bashrc) >/dev/null
	if [ $? -eq 1 ]; then
	    /bin/cp -f /usr/src/freeswitch.git/support-d/.bashrc ~/
	    echo ".bashrc updated."
	    source ~/.bashrc
	fi
    fi
fi
# End Auto Update

# End of file
