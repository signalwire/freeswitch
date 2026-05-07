#!/usr/bin/env bash

### shfmt -w -s -ci -sr -kp -fn tests/unit/run-tests-docker.sh

#------------------------------------------------------------------------------
# Docker Test Runner
# Parallel test execution in Docker with configurable CPU and container counts
#------------------------------------------------------------------------------

# Exit on error
set -e

# Global exit status
GLOBAL_EXIT_STATUS=0

# Default values
SOFIA_SIP_PATH=""
FREESWITCH_PATH=""
IMAGE_TAG="ci.local"
BASE_IMAGE=""
MAX_CONTAINERS="1"
CPUS_PER_CONTAINER="1"
CONTAINER_IDS_FILE=$(mktemp)
OUTPUT_DIR=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
	case $1 in
		--sofia-sip-path)
			SOFIA_SIP_PATH="$2"
			shift
			;;
		--freeswitch-path)
			FREESWITCH_PATH="$2"
			shift
			;;
		--image-tag)
			IMAGE_TAG="$2"
			shift
			;;
		--base-image)
			BASE_IMAGE="$2"
			shift
			;;
		--max-containers)
			MAX_CONTAINERS="$2"
			shift
			;;
		--cpus)
			CPUS_PER_CONTAINER="$2"
			shift
			;;
		--output-dir)
			OUTPUT_DIR="$2"
			shift
			;;
		*)
			echo "Unknown parameter: $1"
			exit 1
			;;
	esac
	shift
done

# Validate paths exist
if [ ! -d "$SOFIA_SIP_PATH" ]; then
	echo "Error: Sofia-SIP path does not exist: $SOFIA_SIP_PATH"
	exit 1
fi

if [ ! -d "$FREESWITCH_PATH" ]; then
	echo "Error: FreeSWITCH path does not exist: $FREESWITCH_PATH"
	exit 1
fi

# Validate output directory is provided (required for test results)
if [ -z "$OUTPUT_DIR" ]; then
	echo "Error: Output directory must be specified with --output-dir"
	exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Define build log file
BUILD_LOG="$OUTPUT_DIR/docker_build.log"

echo "Starting Docker build..."

# Build Docker image and redirect output to log file
if ! docker build \
	--build-arg BUILDER_IMAGE="$BASE_IMAGE" \
	--build-context sofia-sip="$SOFIA_SIP_PATH" \
	--build-context freeswitch="$FREESWITCH_PATH" \
	--file "$FREESWITCH_PATH/.github/docker/debian/bookworm/amd64/CI/Dockerfile" \
	--tag "$IMAGE_TAG" \
	"$FREESWITCH_PATH/.github/docker/debian" > "$BUILD_LOG" 2>&1; then
	echo "Docker build failed! Build log:"
	cat "$BUILD_LOG"
	exit 1
fi

echo "Build completed successfully! Build log saved to: $BUILD_LOG"

# Get working directory from image
CONTAINER_WORKDIR=$(docker inspect "$IMAGE_TAG" --format='{{.Config.WorkingDir}}')
if [ -z "$CONTAINER_WORKDIR" ]; then
	echo "Error: Could not determine container working directory"
	exit 1
fi

# Start test containers with output directory mounted
echo "Starting $MAX_CONTAINERS containers..."

for i in $(seq 1 "$MAX_CONTAINERS"); do
	CTID=$(docker run --privileged \
		--cpus "$CPUS_PER_CONTAINER" \
		--volume "$OUTPUT_DIR:$CONTAINER_WORKDIR/$OUTPUT_DIR" \
		--detach \
		"$IMAGE_TAG" \
		--output-dir "$OUTPUT_DIR" \
		$MAX_CONTAINERS $i)
	echo "$CTID $i" >> "$CONTAINER_IDS_FILE"
	echo "Started container: $CTID (index: $i)"
done

echo "All containers started successfully!"

# Wait for containers to finish and collect results
echo "Waiting for containers to finish..."
while read -r line; do
	CTID=$(echo "$line" | cut -d' ' -f1)
	INDEX=$(echo "$line" | cut -d' ' -f2)

	docker wait "$CTID" > /dev/null 2>&1
	EXIT_CODE=$(docker inspect "$CTID" --format='{{.State.ExitCode}}')

	# Save container logs to output directory
	docker logs "$CTID" > "$OUTPUT_DIR/$CTID.log" 2>&1
	echo "Container logs saved to: $OUTPUT_DIR/$CTID.log"

	if [ "$EXIT_CODE" -ne 0 ]; then
		echo "Container $CTID (index: $INDEX) failed with exit code: $EXIT_CODE"
		GLOBAL_EXIT_STATUS=1

		tail -n 50 "$OUTPUT_DIR/$CTID.log"
	else
		echo "Container $CTID (index: $INDEX) completed successfully"
	fi

done < "$CONTAINER_IDS_FILE"

# Clean up temporary files
rm -f "$CONTAINER_IDS_FILE"

echo "Test outputs have been saved to: $OUTPUT_DIR"

# Exit with global status
if [ "$GLOBAL_EXIT_STATUS" -eq 0 ]; then
	echo "All containers completed successfully!"
else
	echo "One or more containers failed!"
fi

exit $GLOBAL_EXIT_STATUS
