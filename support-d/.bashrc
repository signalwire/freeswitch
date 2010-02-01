#
# /etc/profile: system-wide defaults for bash(1) login shells
#

if [ "`id -u`" = "0" ]; then
    export PATH="/sbin:/usr/sbin:/bin:/usr/bin:/usr/X11R6/bin:/opt/bin:/usr/local/bin:/usr/local/sbin"
else
    export PATH="/bin:/usr/bin:/usr/X11R6/bin:/opt/bin:/usr/local/bin:/usr/local/sbin"
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
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig
export VISUAL=emacs

umask 022
alias e='emacs'
alias tcommit='svn commit --no-auth-cache --username=anthm'
alias mcommit='svn commit --no-auth-cache --username=mikej'
alias bcommit='svn commit --no-auth-cache --username=brian'
alias icommit='svn commit --no-auth-cache --username=intralanman'
alias dp='emacs /usr/local/freeswitch/conf/dialplan/default.xml'
alias fstop='top -p `cat /usr/local/freeswitch/run/freeswitch.pid`'
alias fsgdb='gdb /usr/local/freeswitch/bin/freeswitch `cat /usr/local/freeswitch/run/freeswitch.pid`'
alias fscore='gdb /usr/local/freeswitch/bin/freeswitch `ls -rt core.* | tail -n1`'
alias emacs='emacs -nw'
# End of file
