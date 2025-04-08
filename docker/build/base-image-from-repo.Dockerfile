# Dockerfile for building Debian base-image with FreeSWITCH dependencies (repo)

# Build the image:
#   docker build -t fs-base-image --build-arg FS_TOKEN=your_token_here -f docker/build/base-image-from-repo.Dockerfile .
#
# Or build using secrets (recommended for tokens):
#   docker build -t fs-base-image --secret id=fs_token,env=FS_TOKEN -f docker/build/base-image-from-repo.Dockerfile .

# Stage 1: Install FreeSWITCH using fsget.sh
FROM debian:bookworm AS installer

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Define build arguments
ARG FS_TOKEN
ARG FS_RELEASE=release
ARG FS_INSTALL=install

# Set working directory
WORKDIR /tmp

# Copy fsget.sh script
COPY scripts/packaging/fsget.sh /usr/local/bin/fsget.sh
RUN chmod +x /usr/local/bin/fsget.sh

# Install required tools
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    procps \
    locales

# Install FreeSWITCH using fsget.sh with token from secret or build arg
RUN --mount=type=secret,id=fs_token,target=/run/secrets/fs_token \
    TOKEN=${FS_TOKEN:-$(cat /run/secrets/fs_token 2>/dev/null || echo "")} && \
    if [ -z "$TOKEN" ]; then \
        echo "Error: No token provided. Use --build-arg FS_TOKEN=your_token or --secret id=fs_token,env=FS_TOKEN" && \
        exit 1; \
    fi && \
    /usr/local/bin/fsget.sh "$TOKEN" "$FS_RELEASE" "$FS_INSTALL"

# Set locale
RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    locale-gen

ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

# Clean up
RUN apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    rm -f /etc/apt/sources.list.d/freeswitch.list && \
    rm -f /etc/apt/auth.conf && \
    rm -f /usr/local/bin/fsget.sh

# Stage 2: Final image with minimal layers
FROM debian:bookworm

# Copy everything from the installer stage
COPY --from=installer / /

# Set working directory
WORKDIR /usr/local/src/freeswitch

# Set command to bash directly
SHELL ["/bin/bash", "-c"]
