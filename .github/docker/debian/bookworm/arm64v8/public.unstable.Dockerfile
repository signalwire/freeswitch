ARG BUILDER_IMAGE=arm64v8/debian:bookworm-20240513

FROM --platform=linux/arm64 ${BUILDER_IMAGE} AS builder

ARG MAINTAINER_NAME="Andrey Volk"
ARG MAINTAINER_EMAIL="andrey@signalwire.com"

# Credentials
ARG REPO_DOMAIN=freeswitch.signalwire.com
ARG REPO_USERNAME=user

ARG BUILD_NUMBER=42
ARG GIT_SHA=0000000000

ARG DATA_DIR=/data
ARG CODENAME=bookworm
ARG GPG_KEY="/usr/share/keyrings/signalwire-freeswitch-repo.gpg"

MAINTAINER "${MAINTAINER_NAME} <${MAINTAINER_EMAIL}>"

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

RUN . ~/.env && cat <<EOF > /etc/apt/sources.list.d/freeswitch.list
deb [signed-by=${GPG_KEY}] https://${REPO_DOMAIN}/repo/deb/debian-unstable ${CODENAME} main
deb-src [signed-by=${GPG_KEY}] https://${REPO_DOMAIN}/repo/deb/debian-unstable ${CODENAME} main
EOF

RUN git config --global --add safe.directory '*' \
    && git config --global user.name "${MAINTAINER_NAME}" \
    && git config --global user.email "${MAINTAINER_EMAIL}"

# Bootstrap and Build
COPY . ${DATA_DIR}
WORKDIR ${DATA_DIR}
RUN echo "export VERSION=$(cat ./build/next-release.txt | tr -d '\n')" | tee -a ~/.env

RUN . ~/.env && ./debian/util.sh prep-create-orig -n -V${VERSION}-${BUILD_NUMBER}-${GIT_SHA} -x
RUN . ~/.env && ./debian/util.sh prep-create-dsc -a arm64 ${CODENAME}

RUN --mount=type=secret,id=REPO_PASSWORD,required=true \
    printf "machine ${REPO_DOMAIN} "  > /etc/apt/auth.conf && \
    printf "login ${REPO_USERNAME} " >> /etc/apt/auth.conf && \
    printf "password "               >> /etc/apt/auth.conf && \
    cat /run/secrets/REPO_PASSWORD   >> /etc/apt/auth.conf && \
    sha512sum /run/secrets/REPO_PASSWORD && \
    curl \
        --fail \
        --netrc-file /etc/apt/auth.conf \
        --output ${GPG_KEY} \
        https://${REPO_DOMAIN}/repo/deb/debian-unstable/signalwire-freeswitch-repo.gpg && \
    file ${GPG_KEY} && \
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
