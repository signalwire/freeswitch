# Build Scripts

This directory contains scripts for building FreeSWITCH.

## build-debs-native.sh
Builds Debian packages for FreeSWITCH from git tree on supported Debian distributions.

### TLDR: Example running from FreeSWITCH git root
```bash
scripts/packaging/fsget.sh pat_hFooBar
scripts/packaging/build/build-debs-native.sh -b 999 -o /tmp/fsdebs/
```

### Prerequisites
- Root access is recommended
- Supported Debian system and architecture
- FreeSWITCH git repository in the working directory
- FreeSWITCH Debian repository configured (use `fsget.sh`)

### Features
- Automated dependency installation
- FreeSWITCH Debian (source) package creation

### Usage
```bash
./build-debs-native.sh -b BUILD_NUMBER -o OUTPUT_DIR [-w WORKING_DIR]
```

Required:
- `-b`: Build number (part of package version)
- `-o`: Output directory for packages

Optional:
- `-w`: Working directory (defaults to git root, needs to be git tree)

### Output
Generates `.deb`, `.dsc`, `.changes`, and `.tar.*` files in the output directory.
