#!/usr/bin/env bash
# =============================================================================
# PKB Repo Map Generator — deterministic repo tree refresh
#
# Usage:
#   ./gen_repo_map.sh --repo-root . --doc-dir doc
#   ./gen_repo_map.sh --repo-root . --stdout
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/generated_output.sh"

REPO_ROOT="."
DOC_DIR="doc"
OUTPUT_FILE=""
DEPTH=3
STDOUT_ONLY=0
EXCLUDE_PATTERN="node_modules|target|dist|build|.git|__pycache__|.DS_Store|.venv|venv"

usage() {
  cat <<'EOF'
Usage:
  gen_repo_map.sh [options]

Options:
  --repo-root <path>    Repository root to scan
  --doc-dir <path>      PKB doc directory relative to repo root or absolute path
  --output-file <path>  Explicit sidecar artifact file to write
  --depth <n>           Tree depth (default: 3)
  --exclude <pattern>   tree -I exclusion pattern
  --stdout              Print raw tree only, do not write an artifact
  -h, --help            Show this help
EOF
}

resolve_repo_map_file() {
  local doc_path="$1"
  if [[ -f "$doc_path/04-repo-map.md" ]]; then
    printf '%s\n' "04-repo-map"
    return
  fi
  if [[ -f "$doc_path/01-repo-map.md" ]]; then
    printf '%s\n' "01-repo-map"
    return
  fi
  printf '%s\n' "04-repo-map"
}

generate_tree() {
  if command -v tree >/dev/null 2>&1; then
    (
      cd "$REPO_ROOT"
      tree -d --dirsfirst -L "$DEPTH" -I "$EXCLUDE_PATTERN"
    )
    return
  fi

  python3 - "$REPO_ROOT" "$DEPTH" "$EXCLUDE_PATTERN" <<'PY'
import os
import sys

root = os.path.abspath(sys.argv[1])
max_depth = int(sys.argv[2])
excluded = {part for part in sys.argv[3].split("|") if part}

def is_excluded(name: str) -> bool:
    return name in excluded

def walk(path: str, prefix: str, depth: int) -> None:
    if depth > max_depth:
        return

    try:
        entries = sorted(
            [name for name in os.listdir(path) if os.path.isdir(os.path.join(path, name)) and not is_excluded(name)]
        )
    except OSError:
        return

    for index, name in enumerate(entries):
        connector = "└── " if index == len(entries) - 1 else "├── "
        print(f"{prefix}{connector}{name}/")
        extension = "    " if index == len(entries) - 1 else "│   "
        walk(os.path.join(path, name), prefix + extension, depth + 1)

print(".")
walk(root, "", 1)
PY
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo-root)
      REPO_ROOT="${2:-.}"
      shift 2
      ;;
    --doc-dir)
      DOC_DIR="${2:-doc}"
      shift 2
      ;;
    --output-file)
      OUTPUT_FILE="${2:-}"
      shift 2
      ;;
    --depth)
      DEPTH="${2:-3}"
      shift 2
      ;;
    --exclude)
      EXCLUDE_PATTERN="${2:-$EXCLUDE_PATTERN}"
      shift 2
      ;;
    --stdout)
      STDOUT_ONLY=1
      shift
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
if [[ "$DOC_DIR" = /* ]]; then
  DOC_PATH="$DOC_DIR"
else
  DOC_PATH="$REPO_ROOT/$DOC_DIR"
fi

TREE_OUTPUT="$(generate_tree)"

if [[ "$STDOUT_ONLY" -eq 1 ]]; then
  printf '%s\n' "$TREE_OUTPUT"
  exit 0
fi

if [[ -z "$OUTPUT_FILE" ]]; then
  REPO_MAP_BASENAME="$(resolve_repo_map_file "$DOC_PATH")"
  OUTPUT_FILE="$(generated_output_path "$DOC_PATH" "${REPO_MAP_BASENAME}.generated.md")"
fi

SCRIPT_NAME="$(script_id "$0")"
PROVENANCE_BLOCK="- Repo root: \`$REPO_ROOT\`
- Doc root: \`$DOC_PATH\`
- Tree depth: \`$DEPTH\`
- Exclude pattern: \`$EXCLUDE_PATTERN\`"
BODY_BLOCK="## Extracted Directory Tree

\`\`\`text
$TREE_OUTPUT
\`\`\`"
NEEDS_INPUT_BLOCK="- [NEEDS INPUT: describe which directories are most important to new contributors and why]
- [NEEDS INPUT: confirm key entry points, ownership boundaries, and any directories intentionally omitted from the tree]"

write_markdown_artifact "$OUTPUT_FILE" "$SCRIPT_NAME" "Repository Map Snapshot" "$PROVENANCE_BLOCK" "$BODY_BLOCK" "$NEEDS_INPUT_BLOCK"

echo "Wrote $OUTPUT_FILE"
