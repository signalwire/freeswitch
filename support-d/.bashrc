#
# FreeSWITCH Dev .bashrc
#
export UNAME=`uname -s`

if [ "`id -u`" = "0" ]; then
    if [ "${UNAME}" = "Linux" ]; then
	if [ -d /usr/lib/ccache ]; then
	    export PATH="/usr/lib/ccache:$PATH"
	fi
	export PATH="$PATH:/opt/bin:/usr/local/bin:/usr/local/sbin:/usr/local/freeswitch/bin"
	if [ -d /usr/src/freeswitch.git/support-d/utils ]; then
	    export PATH="/usr/src/freeswitch.git/support-d/utils:$PATH"
	fi
	export PCVAR=`find /usr -name freeswitch.pc| grep -v build/freeswitch.pc`
	if [ -n "$PCVAR" ]; then
	    export PCDIR=${PCVAR%/*}
	    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$PCDIR:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig
	    export localstatedir=`pkg-config freeswitch --variable=localstatedir`
	    export sysconfdir=`pkg-config freeswitch --variable=sysconfdir`
	    export logfiledir=`pkg-config freeswitch --variable=logfiledir`
	    export grammardir=`pkg-config freeswitch --variable=grammardir`
	    export runtimedir=`pkg-config freeswitch --variable=runtimedir`
	    export libexecdir=`pkg-config freeswitch --variable=libexecdir`
	    export exec_prefix=`pkg-config freeswitch --variable=exec_prefix`
	    export modulesdir=`pkg-config freeswitch --variable=modulesdir`
	    export bindir=`pkg-config freeswitch --variable=bindir`
	    export dbdir=`pkg-config freeswitch --variable=dbdir`
	    export recordingsdir=`pkg-config freeswitch --variable=recordingsdir`
	    export prefix=`pkg-config freeswitch --variable=prefix`
	    export libdir=`pkg-config freeswitch --variable=libdir`
	    export scriptdir=`pkg-config freeswitch --variable=scriptdir`
	    export includedir=`pkg-config freeswitch --variable=includedir`
	    export htdocsdir=`pkg-config freeswitch --variable=htdocsdir`
	    export soundsdir=`pkg-config freeswitch --variable=soundsdir`
	else
	    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig
	fi
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
# BE GONE SATAN!
#export PROMPT_COMMAND="history -a; history -c; history -r; ${PROMPT_COMMAND}"
export HISTSIZE=5000
export TERM=xterm-256color
export LESSCHARSET="latin1"
export LESS="-R"
export CHARSET="ISO-8859-1"
export PS1='\n\[\033[01;31m\]\u@\h\[\033[01;36m\] [\d \@] \[\033[01;33m\] \w\n\[\033[00m\]<\#>:'
export PS2="\[\033[1m\]> \[\033[0m\]"
if [ -f ~/.viplease ]; then
    if [ -f /usr/bin/vim ]; then
	export VISUAL=vim
    else 
	export VISUAL=vi
    fi
else
    export VISUAL=emacs
fi
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
alias dp='emacs $sysconfdir/dialplan/default.xml'
alias go='$bindir/freeswitch -nonat'
alias fstop='top -p `cat $runtimedir/freeswitch.pid`'
alias fsgcore='gcore `cat $runtimedir/freeswitch.pid`'
alias fsgdb='gdb $bindir/freeswitch `cat $runtimedir/freeswitch.pid`'
alias fscore='gdb $bindir/freeswitch `ls -rt core.* | tail -n1`'
alias emacs='emacs -nw'

if [ "${UNAME}" = "Linux" ]; then
    alias govg='valgrind --tool=memcheck --log-file=vg.log --leak-check=full --leak-resolution=high --show-reachable=yes .libs/freeswitch -vg'
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
