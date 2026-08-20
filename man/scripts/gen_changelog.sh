#!/usr/bin/env bash
# =============================================================================
# PKB changelog draft generator
#
# Usage:
#   ./gen_changelog.sh --repo-root .
#   ./gen_changelog.sh --repo-root . --range v1.2.0..HEAD
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/generated_output.sh"

REPO_ROOT="."
RANGE=""
OUTPUT_FILE=""
DOC_DIR=""

usage() {
  cat <<'EOF'
Usage:
  gen_changelog.sh [options]

Options:
  --repo-root <path>    Repository root
  --doc-dir <path>      PKB doc directory for default sidecar output
  --range <git-range>   Explicit git revision range (for example: v1.2.0..HEAD)
  --output-file <path>  Write a sidecar artifact file instead of stdout
  -h, --help            Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo-root)
      REPO_ROOT="${2:-.}"
      shift 2
      ;;
    --range)
      RANGE="${2:-}"
      shift 2
      ;;
    --doc-dir)
      DOC_DIR="${2:-}"
      shift 2
      ;;
    --output-file)
      OUTPUT_FILE="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

REPO_ROOT="$(cd "$REPO_ROOT" && pwd)"

if [[ -z "$RANGE" ]]; then
  if latest_tag="$(git -C "$REPO_ROOT" describe --tags --abbrev=0 2>/dev/null)"; then
    RANGE="$latest_tag..HEAD"
  else
    commit_count="$(git -C "$REPO_ROOT" rev-list --count HEAD)"
    if [[ "$commit_count" -gt 20 ]]; then
      RANGE="HEAD~20..HEAD"
    else
      RANGE="HEAD"
    fi
  fi
fi

features=""
fixes=""
docs=""
tests=""
build=""
chores=""
other=""

append_line() {
  local current="$1"
  local line="$2"
  if [[ -n "$current" ]]; then
    printf '%s\n%s' "$current" "$line"
  else
    printf '%s' "$line"
  fi
}

while IFS=$'\t' read -r hash subject author_date; do
  [[ -z "$hash" ]] && continue
  line="- ${subject} (${hash}, ${author_date})"
  case "$subject" in
    feat:*|feat\(*|feature:*)
      features="$(append_line "$features" "$line")"
      ;;
    fix:*|fix\(*|bugfix:*)
      fixes="$(append_line "$fixes" "$line")"
      ;;
    docs:*|doc:*)
      docs="$(append_line "$docs" "$line")"
      ;;
    test:*|tests:*)
      tests="$(append_line "$tests" "$line")"
      ;;
    build:*|ci:*|release:*)
      build="$(append_line "$build" "$line")"
      ;;
    chore:*|refactor:*|perf:*)
      chores="$(append_line "$chores" "$line")"
      ;;
    *)
      other="$(append_line "$other" "$line")"
      ;;
  esac
done < <(git -C "$REPO_ROOT" log --no-merges --pretty=format:'%h%x09%s%x09%ad' --date=short "$RANGE")

render_section() {
  local title="$1"
  local content="$2"
  [[ -z "$content" ]] && return 0
  printf '## %s\n\n' "$title"
  printf '%s\n' "$content"
  printf '\n'
}

output="# Changelog Draft

Generated from \`git log ${RANGE}\`.

"

output+="$(render_section "Features" "$features")"
output+="$(render_section "Fixes" "$fixes")"
output+="$(render_section "Documentation" "$docs")"
output+="$(render_section "Testing" "$tests")"
output+="$(render_section "Build and Release" "$build")"
output+="$(render_section "Chores and Refactors" "$chores")"
output+="$(render_section "Other Changes" "$other")"

if [[ -z "$OUTPUT_FILE" && -n "$DOC_DIR" ]]; then
  if [[ "$DOC_DIR" = /* ]]; then
    DOC_PATH="$DOC_DIR"
  else
    DOC_PATH="$REPO_ROOT/$DOC_DIR"
  fi
  OUTPUT_FILE="$(generated_output_path "$DOC_PATH" "08-build.changelog.generated.md")"
fi

if [[ -z "$OUTPUT_FILE" ]]; then
  printf '%s' "$output"
else
  SCRIPT_NAME="$(script_id "$0")"
  PROVENANCE_BLOCK="- Repo root: \`$REPO_ROOT\`
- Git range: \`$RANGE\`"
  BODY_BLOCK="## Extracted Changelog Draft

$(printf '%s\n' "$output" | sed '1,2d')"
  NEEDS_INPUT_BLOCK="- [NEEDS INPUT: summarize user-facing impact instead of repeating commit subjects verbatim]
- [NEEDS INPUT: call out any breaking changes, release notes, or deployment sequencing the commit log cannot infer]"
  write_markdown_artifact "$OUTPUT_FILE" "$SCRIPT_NAME" "Changelog Draft" "$PROVENANCE_BLOCK" "$BODY_BLOCK" "$NEEDS_INPUT_BLOCK"
  echo "Wrote $OUTPUT_FILE"
fi
