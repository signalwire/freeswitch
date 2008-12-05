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

export LESSCHARSET="latin1"
export LESS="-R"
export CHARSET="ISO-8859-1"
export PS1='\n\[\033[01;31m\]\u@\h\[\033[01;36m\] [\d \@] \[\033[01;33m\] \w\n\[\033[00m\]<\#>:'
export PS2="\[\033[1m\]> \[\033[0m\]"
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig
alias icvs='cvs -d :ext:anthm@cvs.sourceforge.net:/cvsroot/iaxclient'
export CVS_RSH=ssh
export VISUAL=emacs


umask 022

# End of file
