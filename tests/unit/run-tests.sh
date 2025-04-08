#!/usr/bin/env bash

### shfmt -w -s -ci -sr -kp -fn tests/unit/run-tests.sh

#------------------------------------------------------------------------------
# Test Runner Script
# Runs tests in chunks, distributing them across processors
# Supports moving test artifacts to specified output directory
#------------------------------------------------------------------------------

# Exit on error, undefined vars, and propagate pipe failures
set -euo pipefail

# Global variable for test failures
declare -i TESTS_FAILED=0

# Function to show usage
show_usage()
{
	echo "Usage: $0 [chunks] [chunk_number] [options]"
	echo "  chunks        : Number of chunks to split tests into (default: 1)"
	echo "  chunk_number  : Which chunk to run (default: 1)"
	echo "Options:"
	echo "  --dry-run     : Show test distribution without running tests"
	echo "  --output-dir  : Directory to store test artifacts (will be created if needed)"
	echo "  -h|--help     : Show this help message"
	exit 1
}

# Function to validate numeric input
validate_number()
{
	local val=$1
	local name=$2

	if ! [[ $val =~ ^[0-9]+$ ]]; then
		echo "Error: $name must be a positive number, got: $val"
		exit 1
	fi

	if [ "$val" -lt 1 ]; then
		echo "Error: $name must be greater than 0, got: $val"
		exit 1
	fi
}

# Function to format duration in human-readable form
format_duration()
{
	local duration=$1
	local minutes=$((duration / 60))
	local seconds=$((duration % 60))
	printf "%02d:%02d" $minutes $seconds
}

# Function to move test artifacts to output directory
move_artifacts()
{
	local output_dir=$1

	# Create output directory if it doesn't exist
	mkdir -p "$output_dir"

	# Move HTML logs and backtrace files if they exist
	# Using || true to prevent script failure if no files match
	(mv log_run-tests_*.html "$output_dir" 2> /dev/null || true)
	(mv backtrace_*.txt "$output_dir" 2> /dev/null || true)

	# Check if any files were moved
	if [ -n "$(ls -A "$output_dir" 2> /dev/null)" ]; then
		echo "Test artifacts moved to: $output_dir"
	else
		echo "No test artifacts found to move"
	fi
}

# Parse command line arguments
chunks=1
chunk_number=1
dry_run=false
output_dir=""

while [[ $# -gt 0 ]]; do
	case $1 in
		--dry-run)
			dry_run=true
			shift
			;;
		--output-dir)
			if [ -n "${2:-}" ]; then
				output_dir="$2"
				shift 2
			else
				echo "Error: --output-dir requires a directory path"
				exit 1
			fi
			;;
		-h | --help)
			show_usage
			;;
		*)
			if [[ $chunks -eq 1 ]]; then
				chunks=$1
			elif [[ $chunk_number -eq 1 ]]; then
				chunk_number=$1
			else
				echo "Error: Unknown argument $1"
				show_usage
			fi
			shift
			;;
	esac
done

# Validate numeric inputs
validate_number "$chunks" "chunks"
validate_number "$chunk_number" "chunk_number"

# Validate chunk parameters
if [ "$chunk_number" -gt "$chunks" ]; then
	echo "Error: chunk_number ($chunk_number) cannot be greater than total chunks ($chunks)"
	exit 1
fi

# Get list of tests from make
echo "Fetching test list..."
TESTS=$(make -s -C ../.. print_tests 2>/dev/null) || {
	echo "Error: Failed to fetch test list"
	exit 1
}

# Split tests into array
IFS=$'\n' read -d '' -r -a all_tests <<< "$TESTS" || true # || true to handle last line without newline

# Check if any tests were found
if [ ${#all_tests[@]} -eq 0 ]; then
	echo "Error: No tests found!"
	exit 1
fi

# Get total number of tests from array length
total_tests=${#all_tests[@]}

# Select tests for this chunk
chunk_tests=()
for ((i = chunk_number - 1; i < total_tests; i += chunks)); do
	chunk_tests+=("${all_tests[$i]}")
done

# Size of this chunk
chunk_size=${#chunk_tests[@]}

# Print execution information
echo ""
echo "Test Distribution Information:"
echo "Total tests found: $total_tests"
echo "Chunk size: $chunk_size"
echo "Running chunk $chunk_number/$chunks"
if [ -n "$output_dir" ]; then
	echo "Artifacts will be stored in: $output_dir"
fi
echo ""
echo "Tests to be executed:"
for i in "${!chunk_tests[@]}"; do
	printf "%3d) %s\n" "$((i + 1))" "${chunk_tests[$i]}"
done
echo ""

# Exit here if dry run
if [ "$dry_run" = true ]; then
	echo "Dry run complete. Use without --dry-run to execute tests."
	exit 0
fi

# Record start time
start_time=$(date +%s)

# Run tests sequentially within the chunk
if ! make -f run-tests.mk TEST_LIST="${chunk_tests[*]}"; then
	TESTS_FAILED=1
fi

# Move artifacts if output directory was specified
if [ -n "$output_dir" ]; then
	move_artifacts "$output_dir"
fi

# Record end time and calculate duration
end_time=$(date +%s)
duration=$((end_time - start_time))

# Print timing results and statistics
echo ""
echo "Execution Summary for chunk $chunk_number/$chunks:"
echo "Started at : $(date -d @$start_time '+%Y-%m-%d %H:%M:%S')"
echo "Finished at: $(date -d @$end_time '+%Y-%m-%d %H:%M:%S')"
echo "Duration   : $(format_duration $duration)"
echo "Status     : $([ $TESTS_FAILED -eq 0 ] && echo "SUCCESS" || echo "FAILED")"
echo ""

# Exit with appropriate status code
exit $TESTS_FAILED
