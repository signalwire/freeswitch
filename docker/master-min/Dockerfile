# vim:set ft=dockerfile:
ARG DEBIAN_VERSION=bookworm
FROM debian:${DEBIAN_VERSION} as stage
ENV LANG en_US.utf8

# ARGs are cleared after every FROM
# see: https://docs.docker.com/engine/reference/builder/#understand-how-arg-and-from-interact
ARG DEBIAN_VERSION
ARG TOKEN
ARG WITHOUT_PERL="true"
ARG WITHOUT_PYTHON="true"
ARG WITHOUT_JAVA="true"

# By default, install the full set of FreeSWITCH packages.  Specify an alternative with:
#   --build-arg="FS_META_PACKAGE=freeswitch-meta-vanilla"
# alternatives include:
#   freeswitch-meta-bare
#   freeswitch-meta-vanilla
#   freeswitch-meta-sorbet
#   freeswitch-meta-all-dbg
ARG FS_META_PACKAGE=freeswitch-meta-all

# Source Dockerfile:
# https://github.com/docker-library/postgres/blob/master/9.4/Dockerfile

# make the "en_US.UTF-8" locale so freeswitch will be utf-8 enabled by default
RUN apt-get update -qq  && \ 
    apt-get dist-upgrade -y && \
    apt-get install -y --no-install-recommends locales ca-certificates gnupg2 gcc libc-dev patch wget curl && \
    localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8 && \
    curl -o /busybox.deb http://ftp.us.debian.org/debian/pool/main/b/busybox/busybox_1.36.1-6_amd64.deb &&  \
    dpkg -i /busybox.deb && rm /busybox.deb && \ 
    curl -o /usr/local/bin/su-exec.c https://raw.githubusercontent.com/ncopa/su-exec/master/su-exec.c && \
    gcc -Wall /usr/local/bin/su-exec.c -o/usr/local/bin/su-exec && \
    chown root:root /usr/local/bin/su-exec && \
    chmod 0755 /usr/local/bin/su-exec && \
    rm /usr/local/bin/su-exec.c

# https://freeswitch.org/confluence/display/FREESWITCH/Debian
# https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/Installation/Linux/Debian_67240088/
RUN wget --no-verbose --http-user=signalwire --http-password=${TOKEN} \
      -O /usr/share/keyrings/signalwire-freeswitch-repo.gpg \
      https://freeswitch.signalwire.com/repo/deb/debian-release/signalwire-freeswitch-repo.gpg \
    && echo "machine freeswitch.signalwire.com login signalwire password ${TOKEN}" > /etc/apt/auth.conf \
    && echo "deb [signed-by=/usr/share/keyrings/signalwire-freeswitch-repo.gpg] https://freeswitch.signalwire.com/repo/deb/debian-release/ ${DEBIAN_VERSION} main" > /etc/apt/sources.list.d/freeswitch.list \
    && apt-get -qq update \
    && apt-get install -y ${FS_META_PACKAGE} \
    && rm /etc/apt/auth.conf \
    && apt-get purge -y --auto-remove wget gcc libc-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/* 

#Create the new root in a separate layer 
COPY make_root_fs.sh freeswitch-config.patch /
RUN if [ "$WITHOUT_PERL" = "false" ] ; then sed -i 's/WITHOUT_PERL=.*/WITHOUT_PERL=\"false\"/' make_root_fs.sh; fi && \
    if [ "$WITHOUT_PYTHON" = "false" ] ; then sed -i 's/WITHOUT_PYTHON=.*/WITHOUT_PYTHON=\"false\"/' make_root_fs.sh; fi && \
    if [ "$WITHOUT_JAVA" = "false" ] ; then sed -i 's/WITHOUT_JAVA=.*/WITHOUT_JAVA=\"false\"/' make_root_fs.sh; fi && \
    sh /make_root_fs.sh

FROM scratch
ENV LANG en_US.utf8

COPY --from=stage /tmp/newroot /
COPY docker-entrypoint.sh healthcheck.sh sounds_version.txt /

# explicitly set user/group IDs
ARG FREESWITCH_UID=499
ARG FREESWITCH_GID=499
RUN groupadd -r freeswitch --gid=${FREESWITCH_GID} && useradd -r -g freeswitch --uid=${FREESWITCH_UID} freeswitch && \
    mkdir -p /var/log/freeswitch && mkdir -p /var/run/freeswitch

## Ports
# Document ports used by this container
### 8021 fs_cli, 5060 5061 5080 5081 sip and sips, 5066 ws, 7443 wss, 8081 8082 verto, 16384-32768, 64535-65535 rtp
EXPOSE 8021/tcp
EXPOSE 5060/tcp 5060/udp 5080/tcp 5080/udp
EXPOSE 5061/tcp 5061/udp 5081/tcp 5081/udp
EXPOSE 5066/tcp
EXPOSE 7443/tcp
EXPOSE 8081/tcp 8082/tcp
EXPOSE 64535-65535/udp
EXPOSE 16384-32768/udp

# Volumes
## Freeswitch Configuration
VOLUME ["/etc/freeswitch"]
## Tmp so we can get core dumps out
VOLUME ["/tmp"]

# Limits Configuration
COPY    build/freeswitch.limits.conf /etc/security/limits.d/

# Healthcheck to make sure the service is running
SHELL       ["/bin/bash", "-c"]
HEALTHCHECK --interval=15s --timeout=5s \
    CMD  fs_cli -x status | grep -q ^UP || exit 1

ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["freeswitch"]
