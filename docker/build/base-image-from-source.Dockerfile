# Dockerfile for building Debian base-image with FreeSWITCH dependencies (sources)

# Available build arguments:
#   BUILD_NUMBER - Package build number (default: 42)
#   LIBBROADVOICE_REF - Git ref for libbroadvoice (default: HEAD)
#   LIBILBC_REF - Git ref for libilbc (default: HEAD)
#   LIBSILK_REF - Git ref for libsilk (default: HEAD)
#   SPANDSP_REF - Git ref for spandsp (default: HEAD)
#   SOFIASIP_REF - Git ref for sofia-sip (default: HEAD)
#   LIBKS_REF - Git ref for libks (default: HEAD)
#   SIGNALWIRE_C_REF - Git ref for signalwire-c (default: HEAD)
#   LIBV8_PACKAGING_REF - Git ref for libv8-packaging (default: HEAD)
#
# Build the image:
#   docker build -t fs-base-image -f docker/build/base-image-from-source.Dockerfile .
#
# Build with specific tags:
#   docker build -t fs-base-image \
#     --build-arg LIBKS_REF=v1.0.0 \
#     --build-arg BUILD_NUMBER=123 \
#     -f docker/build/base-image-from-source.Dockerfile .
#
# Run the container with output directory mounted to host:
#   mkdir -p "$HOME/DEBs"
#   docker run -it --rm -v "$HOME/DEBs:/var/local/deb" -v "$(pwd):/usr/local/src/freeswitch" fs-base-image scripts/packaging/build/fsdeb.sh -b 42 -o /var/local/deb

# Stage 1: Build dependencies layer
FROM debian:bookworm AS builder

# Set bash as the default shell
SHELL ["/bin/bash", "-c"]

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV OUTPUT_DIR="/var/local/deb"

# Define build arguments with default
ARG BUILD_NUMBER=42

# Define tag arguments with defaults to HEAD
ARG LIBBROADVOICE_REF=HEAD
ARG LIBILBC_REF=HEAD
ARG LIBSILK_REF=HEAD
ARG SPANDSP_REF=HEAD
ARG SOFIASIP_REF=HEAD
ARG LIBKS_REF=HEAD
ARG SIGNALWIRE_C_REF=HEAD
ARG LIBV8_PACKAGING_REF=HEAD

# Install minimal requirements
RUN apt-get update && apt-get install -y \
    git \
    lsb-release

# Configure git to trust all directories
RUN git config --global --add safe.directory '*'

# Set base dir for cloned repositories
WORKDIR /usr/src

# Clone libbroadvoice
RUN [ -d "libbroadvoice" ] || (git clone https://github.com/freeswitch/libbroadvoice.git && \
    cd libbroadvoice && git checkout $LIBBROADVOICE_REF)

# Clone libilbc
RUN [ -d "libilbc" ] || (git clone https://github.com/freeswitch/libilbc.git && \
    cd libilbc && git checkout $LIBILBC_REF)

# Clone libsilk
RUN [ -d "libsilk" ] || (git clone https://github.com/freeswitch/libsilk.git && \
    cd libsilk && git checkout $LIBSILK_REF)

# Clone spandsp (packages branch)
RUN [ -d "spandsp" ] || (git clone --branch packages https://github.com/freeswitch/spandsp.git && \
    cd spandsp && git checkout $SPANDSP_REF)

# Clone sofia-sip
RUN [ -d "sofia-sip" ] || (git clone https://github.com/freeswitch/sofia-sip.git && \
    cd sofia-sip && git checkout $SOFIASIP_REF)

# Clone libks
RUN [ -d "libks" ] || (git clone https://github.com/signalwire/libks.git && \
    cd libks && git checkout $LIBKS_REF)

# Clone signalwire-c
RUN [ -d "signalwire-c" ] || (git clone https://github.com/signalwire/signalwire-c.git && \
    cd signalwire-c && git checkout $SIGNALWIRE_C_REF)

# Clone libv8-packaging
RUN [ -d "libv8-packaging" ] || (git clone https://github.com/freeswitch/libv8-packaging.git && \
    cd libv8-packaging && git checkout $LIBV8_PACKAGING_REF)

# Copy the build dependencies script
COPY scripts/packaging/build/dependencies/build-dependencies.sh /usr/local/bin/build-dependencies.sh
RUN chmod +x /usr/local/bin/build-dependencies.sh

# Create directories
RUN mkdir -p "${OUTPUT_DIR}" /usr/src/freeswitch

# Copy FreeSWITCH source tree
COPY . /usr/src/freeswitch

# Build the dependency packages
RUN /usr/local/bin/build-dependencies.sh -b "$BUILD_NUMBER" -o "$OUTPUT_DIR" -p "/usr/src" -s -a -r

# Stage 2: Install packages layer
FROM debian:bookworm AS installer

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /root

# Copy only the DEB files from builder stage
COPY --from=builder /var/local/deb/*.deb /tmp/debs/

# Install required tools for setting up local repo
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    apt-utils \
    dpkg-dev \
    gnupg \
    ca-certificates \
    lsb-release \
    procps \
    locales

# Create local repository directory
RUN mkdir -p /var/local/repo && \
    cp /tmp/debs/*.deb /var/local/repo/

# Generate package index
RUN cd /var/local/repo && \
    dpkg-scanpackages . > Packages && \
    gzip -9c Packages > Packages.gz

# Configure local repository
RUN echo "deb [trusted=yes] file:/var/local/repo ./" > /etc/apt/sources.list.d/local.list && \
    apt-get update

# Install all available packages from local repo
RUN ls -1 /var/local/repo/*.deb | sed -e 's|.*/||' -e 's|_.*||' | grep -Pv "\-dbgsym$" | xargs apt-get install -y --allow-downgrades -f

# Clean up
RUN apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/debs && \
    rm -rf /var/local/repo && \
    rm -f /etc/apt/sources.list.d/local.list

# Set locale
RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    locale-gen

ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

# Stage 3: Final image
FROM debian:bookworm

# Copy everything from the installer stage
COPY --from=installer / /

# Set working directory
WORKDIR /usr/local/src/freeswitch

# Set command to bash directly
SHELL ["/bin/bash", "-c"]
