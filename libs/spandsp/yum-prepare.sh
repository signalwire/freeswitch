:

#
# Install the things which need adding to a fresh Fedora or Centos install to make it ready to build
# spandsp and its test suite
#
yum groupinstall "Development tools"
yum install fftw-devel \
            libtiff-tools \
            libtiff-devel \
            libjpeg-turbo-devel \
            libpcap-devel \
            libxml2-devel \
            libsndfile-devel \
            fltk-devel \
            fltk-fluid \
            libstdc++-devel \
            libstdc++-static \
            sox \
            gcc-c++ \
            libtool \
            autconf \
            automake \
            m4 \
            netpbm \
            netpbm-progs
