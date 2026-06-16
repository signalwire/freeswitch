# Building `libv8` Debian Package

This guide explains how to build the `libv8` Debian package.

## Prerequisites:
- Git
- Debian-based system (native or Docker)
- Only supported platform is AMD64

## Build Steps

### Clone the repository:
```bash
git clone git@github.com:freeswitch/libv8-packaging.git
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
cd libv8-packaging/
```
-- or --
```bash
cd /usr/src/libv8-packaging/
```

### Install build dependencies:
```bash
apt-get update -y \
&& apt-get install -y \
    libbz2-dev \
    libffi-dev \
    libglib2.0-dev \
    liblzma-dev \
    libncurses5-dev \
    libncursesw5-dev \
    libreadline-dev \
    libsqlite3-dev \
    libssl-dev \
    libtinfo5 \
    llvm \
    ninja-build \
    tk-dev \
    zlib1g-dev
```

### Configure build parameters:
```bash
export PYENV_VERSION_TAG=v2.4.0
export PYTHON_VERSION=2.7.18
export V8_GIT_VERSION=6.1.298
export PYENV_ROOT="/opt/pyenv"
export PATH="$PYENV_ROOT/shims:$PYENV_ROOT/bin:$BUILD_DIR/depot_tools:$PATH"
export BUILD_DIR=$(realpath $(pwd))/BUILD
```

### Clone, build and configure `pyenv`:
```bash
git clone --branch $PYENV_VERSION_TAG https://github.com/pyenv/pyenv.git $PYENV_ROOT \
    && sed -i 's|PATH="/|PATH="$PYENV_ROOT/bin/:/|g' /etc/profile \
    && $PYENV_ROOT/bin/pyenv init - | tee -a /etc/profile \
    && echo "export PATH=\"$BUILD_DIR/depot_tools:${PATH}\"" | tee -a /etc/profile
```

### Install `pyenv`:
```bash
pyenv install $PYTHON_VERSION \
    && pyenv global $PYTHON_VERSION
```

### Configure build:
```bash
mkdir -p $BUILD_DIR \
    && cp ./stub-gclient-spec $BUILD_DIR/.gclient \
    && cp -av ./debian/ $BUILD_DIR/ \
    && cd $BUILD_DIR
```

### Build binaries:
```bash
git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git \
    && gclient sync --verbose -r $V8_GIT_VERSION \
    && cd v8 \
    && gn gen out.gn --args="is_debug=true symbol_level=2 blink_symbol_level=1 v8_symbol_level=1 v8_static_library=true is_component_build=false v8_enable_i18n_support=false v8_use_external_startup_data=false" \
    && gn args out.gn --list | tee out.gn/gn_args.txt \
    && ninja -v d8 -C out.gn \
&& cd $BUILD_DIR
```

### Build Debian package:
```bash
sed -i -e "s/GIT_VERSION/$V8_GIT_VERSION/g" debian/v8-6.1_static.pc \
    && sed -i -e "s/GIT_VERSION/$V8_GIT_VERSION/g" debian/changelog \
    && sed -i -e "s/DATE/$(env LC_ALL=en_US.utf8 date '+%a, %d %b %Y %H:%M:%S %z')/g" debian/changelog \
    && sed -i -e "s/DISTRO/$(lsb_release -sc | tr -d '\n')/g" debian/changelog \
    && sed -i -e "s/BUILD_NUMBER/$BUILD_NUMBER/g" debian/changelog \
&& debuild -b -us -uc
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
