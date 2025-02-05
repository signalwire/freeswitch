#!/bin/bash

# lint: shfmt -w -s -bn -ci -sr -fn scripts/packaging/build/build-debs-native.sh

set -e          # Exit immediately if a command exits with a non-zero status
set -u          # Treat unset variables as an error
set -o pipefail # Return value of a pipeline is the value of the last (rightmost) command to exit with a non-zero status

print_usage()
{
	echo "Usage: $0 -b BUILD_NUMBER -o OUTPUT_DIR [-w WORKING_DIR]"
	exit 1
}

WORKING_DIR=$(git rev-parse --show-toplevel 2> /dev/null || pwd -P)

while getopts ":b:o:w:" opt; do
	case ${opt} in
		b) BUILD_NUMBER=$OPTARG ;;
		o) OUTPUT_DIR=$OPTARG ;;
		w) WORKING_DIR=$OPTARG ;;
		\?) print_usage ;;
	esac
done

if [ -z "${BUILD_NUMBER:-}" ] || [ -z "${OUTPUT_DIR:-}" ]; then
	print_usage
fi

if [ "$(id -u)" != "0" ]; then
	echo "Non-root user detected. Execution may fail."
fi

cd "${WORKING_DIR}" || exit 1

install_deps()
{
	apt-get update || echo "WARNING: apt-get update failed"
	apt-get install -y \
		apt-transport-https \
		debhelper \
		gnupg2 \
		build-essential \
		ca-certificates \
		curl \
		devscripts \
		dh-autoreconf \
		dos2unix \
		doxygen \
		lsb-release \
		pkg-config \
		wget || echo "WARNING: package installation failed"
}

export_vars()
{
	export CODENAME=$(lsb_release -sc)
	if ! VERSION=$(cat ./build/next-release.txt | tr -d '\n'); then
		echo "Failed to read version file" >&2
		exit 1
	fi
	export GIT_SHA=$(git rev-parse --short HEAD)
}

setup_git_local()
{
	if [ -z "$(git config user.email)" ]; then
		git config user.email "$(id -un)@localhost"
	fi
	if [ -z "$(git config user.name)" ]; then
		git config user.name "$(id -un)"
	fi
	git config --add safe.directory '*'
}

bootstrap_freeswitch()
{
	./debian/util.sh prep-create-orig -n -V${VERSION}-${BUILD_NUMBER}-${GIT_SHA} -x
	./debian/util.sh prep-create-dsc ${CODENAME}
}

install_freeswitch_deps()
{
	apt-get update || echo "WARNING: apt-get update failed"
	mk-build-deps --install --remove debian/control \
		--tool "apt-get --yes --no-install-recommends" || echo "WARNING: mk-build-deps failed"
	apt-get --yes --fix-broken install || echo "WARNING: apt-get fix-broken failed"
}

build_source_package()
{
	dch -b -M -v "${VERSION}-${BUILD_NUMBER}-${GIT_SHA}~${CODENAME}" \
		--force-distribution -D "${CODENAME}" "Nightly build, ${GIT_SHA}"

	./debian/util.sh create-orig -n -V${VERSION}-${BUILD_NUMBER}-${GIT_SHA} -x
}

build_and_move()
{
	dpkg-source --diff-ignore=.* --compression=xz --compression-level=9 --build . \
		&& debuild -b -us -uc \
		&& mkdir -p "${OUTPUT_DIR}" \
		&& mv -v ../*.{deb,dsc,changes,tar.*} "${OUTPUT_DIR}"/
}

main()
{
	install_deps
	export_vars
	setup_git_local
	bootstrap_freeswitch
	install_freeswitch_deps
	build_source_package
	build_and_move
}

main "$@"
