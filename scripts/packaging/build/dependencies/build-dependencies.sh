#!/bin/bash

## shfmt -w -s -bn -ci -sr -fn scripts/packaging/build/dependencies/build-dependencies.sh

set -e          # Exit immediately if a command exits with a non-zero status
set -u          # Treat unset variables as an error
set -o pipefail # Return value of a pipeline is the value of the last (rightmost) command to exit with a non-zero status
set -x          # Print commands and their arguments as they are executed

# Default values
BUILD_NUMBER=${BUILD_NUMBER:-42}
LIBRARIES=()
BUILD_ALL=false
SETUP_ENV=false
OUTPUT_DIR="/var/local/deb"
SOURCE_PREFIX="/usr/src"
SETUP_LOCAL_REPO=false
CLONE_REPOS=false
GIT_PROTOCOL="ssh"

###################
# Helper Functions
###################

# Display script usage information and available options
function show_usage()
{
	echo "Usage: $0 [options] [library_names...]"
	echo "Options:"
	echo "  -h, --help            Show this help message"
	echo "  -b, --build-number N  Set build number (default: 42 or env value)"
	echo "  -a, --all             Build all libraries"
	echo "  -s, --setup           Set up build environment before building"
	echo "  -o, --output DIR      Set output directory (default: /var/local/deb)"
	echo "  -p, --prefix DIR      Set source path prefix (default: /usr/src)"
	echo "  -r, --repo            Set up local repository after building"
	echo "  -c, --clone           Clone required repositories before building"
	echo "  -g, --git-https       Use HTTPS instead of SSH for git cloning"
	echo ""
	echo "Available libraries:"
	echo "  libbroadvoice"
	echo "  libilbc"
	echo "  libsilk"
	echo "  spandsp"
	echo "  sofia-sip"
	echo "  libks"
	echo "  signalwire-c"
	echo "  libv8-packaging (or libv8)"
	echo ""
}

# Normalize library names, replacing aliases with their canonical names
function normalize_library_name()
{
	local lib_name=$1

	case $lib_name in
		libv8)
			echo "libv8-packaging"
			;;
		*)
			echo "$lib_name"
			;;
	esac
}

# Parse command line arguments and set corresponding variables
function parse_arguments()
{
	while [[ $# -gt 0 ]]; do
		case $1 in
			-h | --help)
				show_usage
				exit 0
				;;
			-b | --build-number)
				BUILD_NUMBER="$2"
				shift 2
				;;
			-a | --all)
				BUILD_ALL=true
				shift
				;;
			-s | --setup)
				SETUP_ENV=true
				shift
				;;
			-o | --output)
				OUTPUT_DIR="$2"
				shift 2
				;;
			-p | --prefix)
				SOURCE_PREFIX="$2"
				shift 2
				;;
			-r | --repo)
				SETUP_LOCAL_REPO=true
				shift
				;;
			-c | --clone)
				CLONE_REPOS=true
				shift
				;;
			-g | --git-https)
				GIT_PROTOCOL="https"
				shift
				;;
			*)
				local normalized_name=$(normalize_library_name "$1")
				LIBRARIES+=("$normalized_name")
				shift
				;;
		esac
	done

	if [ "$(id -u)" != "0" ]; then
		echo "Non-root user detected. Execution may fail."
	fi
}

# Validate the provided arguments and set defaults if needed
function validate_arguments()
{
	if [ ${#LIBRARIES[@]} -eq 0 ] && [ "$BUILD_ALL" == "false" ]; then
		echo "Error: No libraries specified"
		show_usage
		exit 1
	fi

	if [ "$BUILD_ALL" == "true" ]; then
		LIBRARIES=("libbroadvoice" "libilbc" "libsilk" "spandsp" "sofia-sip" "libks" "signalwire-c" "libv8-packaging")
	fi
}

# Set up the build environment variables and create output directory
function setup_environment()
{
	export DEBIAN_FRONTEND=noninteractive
	export BUILD_NUMBER=$BUILD_NUMBER
	export CODENAME=$(lsb_release -sc)

	mkdir -p "$OUTPUT_DIR"
}

# Install required build tools and dependencies
function setup_build_environment()
{
	export DEBIAN_FRONTEND=noninteractive

	local tmp_dir="${TMPDIR:-/tmp}"

	echo "Setting up build environment..."
	apt-get update \
		&& apt-get -y upgrade \
		&& apt-get -y install \
			build-essential \
			cmake \
			devscripts \
			lsb-release \
			docbook-xsl \
			pkg-config \
			git

	if ! git config --global --get-all safe.directory | grep -q '\*'; then
		echo "Setting git safe.directory configuration..."
		git config --global --add safe.directory '*'
	fi

	if [[ " ${LIBRARIES[@]} " =~ " libv8-packaging " ]] || [ "$BUILD_ALL" == "true" ]; then
		echo "libv8 is in the build list, checking for dependencies..."

		if [ ! -d "$SOURCE_PREFIX/libv8-packaging" ] && [ "$CLONE_REPOS" == "true" ]; then
			echo "Cloning libv8-packaging repository..."
			clone_repositories "libv8-packaging"
		fi

		if [ -d "$SOURCE_PREFIX/libv8-packaging" ] && [ -f "$SOURCE_PREFIX/libv8-packaging/build.sh" ]; then
			echo "Installing dependencies for libv8..."
			(cd "$SOURCE_PREFIX/libv8-packaging" && ./build.sh --install-deps)

			echo "Setting up Python environment for libv8..."
			(cd "$SOURCE_PREFIX/libv8-packaging" && ./build.sh --setup-pyenv)
			touch "${tmp_dir}/libv8_pyenv_setup_complete"
		else
			echo "Warning: libv8-packaging directory not found. Clone the repository first to install its dependencies."
			echo "You can use the --clone flag or run the script with -c libv8-packaging first."
		fi
	fi
}

# Clone a specific library repository using the configured protocol
clone_repositories()
{
	local lib_name=$1
	local original_dir=$(pwd)
	local repo_dir="$SOURCE_PREFIX/$lib_name"

	echo "=== Cloning $lib_name ==="

	if [ -d "$repo_dir" ]; then
		echo "Directory $repo_dir already exists, skipping clone..."
		return 0
	fi

	mkdir -p "$SOURCE_PREFIX"
	cd "$SOURCE_PREFIX"

	if [ "$GIT_PROTOCOL" == "ssh" ]; then
		FREESWITCH_BASE="git@github.com:freeswitch"
		SIGNALWIRE_BASE="git@github.com:signalwire"
	else
		FREESWITCH_BASE="https://github.com/freeswitch"
		SIGNALWIRE_BASE="https://github.com/signalwire"
	fi

	case $lib_name in
		libbroadvoice)
			git clone $FREESWITCH_BASE/libbroadvoice.git
			;;
		libilbc)
			git clone $FREESWITCH_BASE/libilbc.git
			;;
		libsilk)
			git clone $FREESWITCH_BASE/libsilk.git
			;;
		spandsp)
			git clone --branch packages $FREESWITCH_BASE/spandsp.git
			;;
		sofia-sip)
			git clone $FREESWITCH_BASE/sofia-sip.git
			;;
		libks)
			git clone $SIGNALWIRE_BASE/libks.git
			;;
		signalwire-c)
			git clone $SIGNALWIRE_BASE/signalwire-c.git
			;;
		libv8-packaging)
			git clone $FREESWITCH_BASE/libv8-packaging.git
			;;
		*)
			echo "Error: Unknown library for cloning '$lib_name'"
			return 1
			;;
	esac

	cd "$original_dir"

	echo "=== Completed cloning $lib_name ==="
}

# Set up a local Debian repository for built packages
setup_local_repository()
{
	local original_dir=$(pwd)
	local abs_output_dir=$(readlink -f "$OUTPUT_DIR")

	echo "Setting up local Debian repository in $abs_output_dir..."
	cd "$abs_output_dir"

	dpkg-scanpackages -m . | tee Packages \
		&& gzip -f -k Packages \
		&& printf "deb [trusted=yes] file:$abs_output_dir /\n" | tee /etc/apt/sources.list.d/local.list \
		&& apt-get update

	cd "$original_dir"

	echo "Local repository setup complete."
}

# Build a library using the Debian package build system
build_deb_library()
{
	local lib_name=$1
	local original_dir=$(pwd)

	echo "=== Building $lib_name ==="

	cd "$SOURCE_PREFIX/$lib_name/"

	export VERSION=$(dpkg-parsechangelog --show-field Version | cut -f1 -d'-')
	export GIT_SHA=$(git rev-parse --short HEAD)

	echo "Version: $VERSION"
	echo "Git SHA: $GIT_SHA"
	echo "Build Number: $BUILD_NUMBER"

	apt-get update \
		&& mk-build-deps \
			--install \
			--remove debian/control \
			--tool "apt-get -y --no-install-recommends" \
		&& apt-get -y -f install

	dch \
		--controlmaint \
		--distribution "${CODENAME}" \
		--force-bad-version \
		--force-distribution \
		--newversion "${VERSION}-${BUILD_NUMBER}-${GIT_SHA}~${CODENAME}" \
		"Nightly build, ${GIT_SHA}" \
		&& debuild \
			--no-tgz-check \
			--build=binary \
			--unsigned-source \
			--unsigned-changes \
		&& mv -v ../*.{deb,changes} "$OUTPUT_DIR"/.

	cd "$original_dir"

	echo "=== Completed building $lib_name ==="
}

# Build a library using the CMake build system
build_cmake_library()
{
	local lib_name=$1
	local deps=$2
	local original_dir=$(pwd)

	echo "=== Building $lib_name ==="

	cd "$SOURCE_PREFIX/$lib_name/"

	if [ -n "$deps" ]; then
		echo "Installing dependencies for $lib_name..."
		apt-get update && apt-get -y install $deps
	fi

	export GIT_SHA=$(git rev-parse --short HEAD)

	echo "Git SHA: $GIT_SHA"
	echo "Build Number: $BUILD_NUMBER"

	PACKAGE_RELEASE="${BUILD_NUMBER}.${GIT_SHA}" cmake . \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX="/usr" \
		&& make package \
		&& mv -v *.deb "$OUTPUT_DIR"/.

	cd "$original_dir"

	echo "=== Completed building $lib_name ==="
}

# Build libv8 using its own build script
build_libv8()
{
	local original_dir=$(pwd)
	local tmp_dir="${TMPDIR:-/tmp}"
	local pyenv_marker_file="${tmp_dir}/libv8_pyenv_setup_complete"

	echo "=== Building libv8 ==="

	cd "$SOURCE_PREFIX/libv8-packaging/"

	echo "Building libv8 with build number $BUILD_NUMBER, output to $OUTPUT_DIR..."
	./build.sh --build-number "$BUILD_NUMBER" --output-dir "$OUTPUT_DIR"

	cd "$original_dir"

	echo "=== Completed building libv8-packaging ==="
}

# Process dependencies and ensure they're built in the correct order
function process_dependencies()
{
	local NEEDS_LOCAL_REPO=false

	if [[ " ${LIBRARIES[@]} " =~ " signalwire-c " ]]; then
		NEEDS_LOCAL_REPO=true

		if ! [[ " ${LIBRARIES[@]} " =~ " libks " ]]; then
			echo "Adding libks as a dependency for signalwire-c"
			LIBRARIES=("libks" "${LIBRARIES[@]}")
		fi
	fi

	echo $NEEDS_LOCAL_REPO
}

# Clone all the repositories for the specified libraries
function clone_all_repos()
{
	echo "Cloning repositories using ${GIT_PROTOCOL} protocol..."
	for lib in "${LIBRARIES[@]}"; do
		clone_repositories "$lib"
	done
}

# Build all the specified libraries in the correct order
function build_all_libraries()
{
	local NEEDS_LOCAL_REPO=$(process_dependencies)

	for lib in "${LIBRARIES[@]}"; do
		case $lib in
			libbroadvoice | libilbc | libsilk | spandsp | sofia-sip)
				build_deb_library "$lib"
				;;
			libks)
				build_cmake_library "$lib" "libssl-dev uuid-dev"
				;;
			signalwire-c)
				setup_local_repository
				build_cmake_library "$lib" "libks2"
				;;
			libv8-packaging)
				build_libv8
				;;
			*)
				echo "Error: Unknown library '$lib'"
				show_usage
				exit 1
				;;
		esac
	done

	if [ "$SETUP_LOCAL_REPO" == "true" ] && [ "$NEEDS_LOCAL_REPO" == "false" ]; then
		setup_local_repository
		echo "Local Debian repository has been set up at $OUTPUT_DIR."
	fi
}

# Print a summary of the build process
function print_summary()
{
	echo "All selected libraries have been built successfully."
	echo "Output packages are in $OUTPUT_DIR/"
}

#####################
# Main Script Logic
#####################

# Parse command line arguments
parse_arguments "$@"

# Validate input arguments
validate_arguments

# Setup the build environment if flag is set
if [ "$SETUP_ENV" == "true" ]; then
	setup_build_environment
else
	echo "Skipping build environment setup (use -s or --setup to enable)"
fi

# Set up environment variables
setup_environment

echo "Using source path prefix: $SOURCE_PREFIX"

# Clone repositories if flag is set
if [ "$CLONE_REPOS" == "true" ]; then
	clone_all_repos
else
	echo "Skipping repository cloning (use -c or --clone to enable)"
fi

# Build all libraries
build_all_libraries

# Print summary
print_summary
