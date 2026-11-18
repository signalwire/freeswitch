# Building `spandsp` Debian Package

This guide explains how to build the `spandsp` Debian package.

## Prerequisites:
- Git
- Debian-based system (native or Docker)

## Build Steps

### Clone the repository:
```bash
git clone --branch packages git@github.com:freeswitch/spandsp.git
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
cd spandsp/
```
-- or --
```bash
cd /usr/src/spandsp/
```

### Extract Debian package version:
```bash
export VERSION=$(dpkg-parsechangelog --show-field Version | cut -f1 -d'-')
```

### Extract git hash:
```bash
export GIT_SHA=$(git rev-parse --short HEAD)
```

### Install build dependencies:
```bash
apt-get update \
&& mk-build-deps \
    --install \
    --remove debian/control \
    --tool "apt-get -y --no-install-recommends" \
&& apt-get -y -f install
```

### Update changelog:
```bash
dch \
    --controlmaint \
    --distribution "${CODENAME}" \
    --force-bad-version \
    --force-distribution \
    --newversion "${VERSION}-${BUILD_NUMBER}-${GIT_SHA}~${CODENAME}" \
    "Nightly build, ${GIT_SHA}"
```

### Build binary package:
```bash
debuild \
    --no-tgz-check \
    --build=binary \
    --unsigned-source \
    --unsigned-changes
```

### Move built packages to the output directory:
```bash
mkdir -p OUT \
&& mv -v ../*.{deb,changes} OUT/.
```
-- or --
```bash
mkdir -p /usr/src/OUT \
&& mv -v ../*.{deb,changes} /usr/src/OUT/.
```

## Output

After a successful build, the Debian packages will be available in the `OUT` directory.
