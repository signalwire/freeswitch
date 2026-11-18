# FreeSWITCH Build Dependencies

This directory is organized into subfolders, where each subfolder contains build instructions for a single Debian package. These packages are dependencies required to build FreeSWITCH's Debian packages.

## Recommended order of building:

- [libbroadvoice](libbroadvoice/README.md)
- [libilbc](libilbc/README.md)
- [libsilk](libsilk/README.md)
- [spandsp](spandsp/README.md)
- [sofia-sip](sofia-sip/README.md)
- [libks](libks/README.md)
- [signalwire-c](signalwire-c/README.md)
- [libv8](libv8/README.md) (only for `AMD64`)

## Build Dependencies Script

A convenient script `build-dependencies.sh` is provided to automate the building of dependencies. This script offers several options to customize the build process.

### Usage

```bash
./build-dependencies.sh [options] [library_names...]
```

### Options

- `-h, --help`: Show the help message
- `-b, --build-number N`: Set build number (default: 42 or env value)
- `-a, --all`: Build all libraries
- `-s, --setup`: Set up build environment before building
- `-o, --output DIR`: Set output directory (default: /var/local/deb)
- `-p, --prefix DIR`: Set source path prefix (default: /usr/src)
- `-r, --repo`: Set up local repository after building
- `-c, --clone`: Clone required repositories before building
- `-g, --git-https`: Use HTTPS instead of SSH for git cloning

### Examples

Set up environment, clone repositories, and build all dependencies:
```bash
./build-dependencies.sh --build-number 123 --setup --all --repo --clone
```

Complete build with all options (setup environment, build all libraries, create local repo, clone repos with HTTPS):
```bash
./build-dependencies.sh --build-number 123 --setup --all --repo --clone --git-https
```

Build specific libraries with full automation:
```bash
./build-dependencies.sh --build-number 123 --setup --repo --clone --git-https libks signalwire-c
```

### Running in Docker

You can run the build script inside a Docker container for a clean, isolated build environment:

```bash
docker run -it -v $(pwd):/root/scripts debian:bookworm bash -c "cd /root/scripts && bash"
```
