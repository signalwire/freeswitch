ARG BUILDER_IMAGE=signalwire/freeswitch-public-ci-base:bookworm-amd64

FROM ${BUILDER_IMAGE}

ARG MAINTAINER_EMAIL="andrey@signalwire.com"

LABEL org.opencontainers.image.authors="${MAINTAINER_EMAIL}"

SHELL ["/bin/bash", "-c"]

COPY --from=sofia-sip / /usr/src/sofia-sip
COPY --from=freeswitch / /usr/src/freeswitch

RUN cd /usr/src/freeswitch && \
    ./ci.sh -t unit-test -a configure -c sofia-sip -p "/usr/src/sofia-sip" && \
    ./ci.sh -t unit-test -a build -c sofia-sip -p "/usr/src/sofia-sip" && \
    ./ci.sh -t unit-test -a install -c sofia-sip -p "/usr/src/sofia-sip" && \
    ./ci.sh -t unit-test -a configure -c freeswitch -p "/usr/src/freeswitch" && \
    ./ci.sh -t unit-test -a build -c freeswitch -p "/usr/src/freeswitch" && \
    ./ci.sh -t unit-test -a install -c freeswitch -p "/usr/src/freeswitch"

WORKDIR /usr/src/freeswitch/tests/unit

ENTRYPOINT ["/usr/src/freeswitch/tests/unit/run-tests.sh"]
