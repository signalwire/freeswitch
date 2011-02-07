UNAME=`uname`


NEEDED_PACKAGES_YUM='automake autoconf libtool screen gdb gcc-c++ compat-gcc-32 compat-gcc-32-c++ subversion ncurses-devel unixODBC-devel make wget'
NEEDED_PACAKGES_APT='automake autoconf libtool screen gdb libncurses5-dev unixodbc-dev subversion emacs22-nox gcc g++ make libjpeg-dev'
NEEDED_PACKAGES_PKG_ADD=''



echo ${UNAME} | grep "Linux" && GETTER='wget -c'
echo ${UNAME} | grep "BSD" && GETTER='fetch'


which apt-get >> /dev/null && INSTALLER='apt-get -y install' && NEEDED_PACKAGES=$NEEDED_PACAKGES_APT
which yum >> /dev/null && INSTALLER='yum -y install' && NEEDED_PACKAGES=$NEEDED_PACKAGES_YUM
which pkg_add >> /dev/null && INSTALLER='pkg_add -r' && NEEDED_PACKAGES=$NEEDED_PACKAGES_PKG_ADD


#echo $GETTER and $INSTALLER
#echo ${INSTALLER} ${NEEDED_PACKAGES}
${INSTALLER} ${NEEDED_PACKAGES}

