# Dockerfile for building Debian packages of FreeSWITCH dependencies
#
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
#   docker build -t fs-debian-package-builder -f docker/build/debs-from-source.Dockerfile .
#
# Build with specific tags:
#   docker build -t fs-debian-package-builder \
#     --build-arg LIBKS_REF=v1.0.0 \
#     --build-arg BUILD_NUMBER=123 \
#     -f docker/build/debs-from-source.Dockerfile .
#
# Run the container with output directory mounted to host:
#   mkdir -p "$HOME/DEBs"
#   docker run -v "$HOME/DEBs:/var/local/deb" fs-debian-package-builder

FROM debian:bookworm

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

# Create the entrypoint script
COPY <<EOF /usr/local/bin/entrypoint.sh
#!/bin/bash
set -e

/usr/local/bin/build-dependencies.sh -b "$BUILD_NUMBER" -o "$OUTPUT_DIR" -p "/usr/src" -s -a -r && \\
chmod +x /usr/src/freeswitch/scripts/packaging/build/fsdeb.sh && \
/usr/src/freeswitch/scripts/packaging/build/fsdeb.sh -b "$BUILD_NUMBER" -o "$OUTPUT_DIR" -w "/usr/src/freeswitch"

echo "Build completed successfully."
EOF
RUN chmod +x /usr/local/bin/entrypoint.sh

# Create directories
RUN mkdir -p "${OUTPUT_DIR}" /usr/src/freeswitch

# Copy FreeSWITCH source tree
COPY . /usr/src/freeswitch

# Set working directory
WORKDIR /usr/src

# Set entrypoint
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
