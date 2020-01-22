ARG FROM_REPO=talkdesk/base
ARG FROM_TAG=1.0
FROM ${FROM_REPO}:${FROM_TAG} as builder

USER root

RUN apt-get update -qq && \
  apt-get --no-install-suggests install -y git autoconf libtool-bin g++ zlib1g-dev libjpeg-dev pkg-config libsqlite3-dev libcurl4-gnutls-dev libpcre3 libpcre3-dev libspeexdsp-dev libspeex-dev libldns-dev  libedit-dev libtiff-dev make yasm
 
ADD . freeswitch
WORKDIR freeswitch
RUN ./bootstrap.sh
RUN ./configure
RUN make mod_conference

FROM talkdesk/freeswitch-kazoo:1.10.1

COPY --from=builder /home/tduser/freeswitch/src/mod/applications/mod_conference/.libs/mod_conference.so /usr/lib/freeswitch/mod/
