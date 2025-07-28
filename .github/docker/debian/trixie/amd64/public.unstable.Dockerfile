ARG BUILDER_IMAGE=debian:trixie-20250520

FROM ${BUILDER_IMAGE} AS builder

ARG MAINTAINER_NAME="Andrey Volk"
ARG MAINTAINER_EMAIL="andrey@signalwire.com"

ARG BUILD_NUMBER=42
ARG GIT_SHA=0000000000

ARG DATA_DIR=/data
ARG CODENAME=trixie

LABEL org.opencontainers.image.authors="${MAINTAINER_EMAIL}"

SHELL ["/bin/bash", "-c"]

RUN apt-get -q update && \
    DEBIAN_FRONTEND=noninteractive apt-get -yq install \
        apt-transport-https \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        debhelper \
        devscripts \
        dh-autoreconf \
        dos2unix \
        doxygen \
        git \
        graphviz \
        libglib2.0-dev \
        libssl-dev \
        lsb-release \
        pkg-config \
        wget

RUN update-ca-certificates --fresh

RUN echo "export CODENAME=${CODENAME}" | tee ~/.env && \
    chmod +x ~/.env

RUN git config --global --add safe.directory '*' \
    && git config --global user.name "${MAINTAINER_NAME}" \
    && git config --global user.email "${MAINTAINER_EMAIL}"

# Bootstrap and Build
COPY . ${DATA_DIR}
WORKDIR ${DATA_DIR}
RUN echo "export VERSION=$(cat ./build/next-release.txt | tr -d '\n')" | tee -a ~/.env

RUN . ~/.env && ./debian/util.sh prep-create-orig -n -V${VERSION}-${BUILD_NUMBER}-${GIT_SHA} -x
RUN . ~/.env && ./debian/util.sh prep-create-dsc ${CODENAME}

RUN --mount=type=secret,id=REPO_PASSWORD,required=true \
    sha512sum /run/secrets/REPO_PASSWORD && \
    curl -sSL https://freeswitch.org/fsget | \
        bash -s $(cat /run/secrets/REPO_PASSWORD) prerelease && \
    apt-get --quiet update && \
    mk-build-deps \
        --install \
        --remove debian/control \
        --tool "apt-get -o Debug::pkgProblemResolver=yes --yes --no-install-recommends" && \
    apt-get --yes --fix-broken install && \
    rm -f /etc/apt/auth.conf

ENV DEB_BUILD_OPTIONS="parallel=1"
RUN . ~/.env && dch -b -M -v "${VERSION}-${BUILD_NUMBER}-${GIT_SHA}~${CODENAME}" \
  --force-distribution -D "${CODENAME}" "Nightly build, ${GIT_SHA}"

RUN . ~/.env && ./debian/util.sh create-orig -n -V${VERSION}-${BUILD_NUMBER}-${GIT_SHA} -x

RUN dpkg-source \
        --diff-ignore=.* \
        --compression=xz \
        --compression-level=9 \
        --build \
    . \
    && debuild -b -us -uc \
    && mkdir OUT \
    && mv -v ../*.{deb,dsc,changes,tar.*} OUT/.

# Artifacts image (mandatory part, the resulting image must have a single filesystem layer)
FROM scratch
COPY --from=builder /data/OUT/ /
