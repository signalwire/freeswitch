# Building `signalwire-c` Debian Package

This guide explains how to build the `signalwire-c` Debian package.

## Prerequisites:
- Git
- Debian-based system (native or Docker)
- LibKS

## Build Steps

### Clone the repository:
```bash
git clone git@github.com:signalwire/signalwire-c.git
```

### (Optionally) Use Docker to build packages for Debian `Bookworm`:
```bash
docker run -it -v $(pwd):/usr/src/ debian:bookworm bash -c "cd /usr/src/ && bash"
```

### Set non-interactive frontend for APT:
```bash
export DEBIAN_FRONTEND=noninteractive
```

### Install required build tools:
```bash
apt-get update \
&& apt-get -y upgrade \
&& apt-get -y install \
    build-essential \
    cmake \
    devscripts \
    lsb-release \
    docbook-xsl \
    pkg-config
```

### Set build number (modify as needed):
```bash
export BUILD_NUMBER=42
```
> Note: The build number (42) used in this guide is arbitrary. You can modify it as needed for your build process.

### Set Debian codename:
```bash
export CODENAME=$(lsb_release -sc)
```

### Configure git safety setting:
```bash
git config --global --add safe.directory '*'
```

### Navigate to the source directory:
```bash
cd signalwire-c/
```
-- or --
```bash
cd /usr/src/signalwire-c/
```

### Extract git hash:
```bash
export GIT_SHA=$(git rev-parse --short HEAD)
```

### (Optionally) Use local file-based Debian repository to install `libks` dependency:
```bash
cd OUT/ \
&& dpkg-scanpackages . | tee OUT/Packages \
&& gzip -f OUT/Packages \
&& printf "deb [trusted=yes] file:$(realpath $(pwd)) ./\n" | tee /etc/apt/sources.list.d/local.list
```
-- or --
```bash
cd /usr/src/OUT/ \
&& dpkg-scanpackages . | tee /usr/src/OUT/Packages \
&& gzip -f /usr/src/OUT/Packages \
&& printf "deb [trusted=yes] file:/usr/src/OUT ./\n" | tee /etc/apt/sources.list.d/local.list
```

### Install build dependencies:
```bash
apt-get update \
&& apt-get -y install \
    libks2
```

### Build binary package:
```bash
PACKAGE_RELEASE="${BUILD_NUMBER}.${GIT_SHA}" cmake . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX="/usr" \
&& make package
```

### Move built packages to the output directory:
```bash
mkdir -p OUT \
&& mv -v *.deb OUT/.
```
-- or --
```bash
mkdir -p /usr/src/OUT \
&& mv -v *.deb /usr/src/OUT/.
```

## Output

After a successful build, the Debian packages will be available in the `OUT` directory.
