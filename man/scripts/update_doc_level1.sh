#!/usr/bin/env bash
# =============================================================================
# PKB Level-1 Auto-Update — Zero LLM Tokens
#
# Produces deterministic PKB sidecar artifacts without modifying human-authored
# PKB pages:
#   1. Refresh repo-map snapshot
#   2. Generate tech-stack inventory
#   3. Regenerate an index candidate
#   4. Run the staleness checker
#   5. Run the translation sync checker when bilingual catalogs exist
# =============================================================================

set -euo pipefail

REPO_ROOT="${1:-.}"
DOC_DIR="${2:-doc}"
REPO_ROOT="$(cd "$REPO_ROOT" && pwd)"
if [[ "$DOC_DIR" = /* ]]; then
  DOC_PATH="$DOC_DIR"
else
  DOC_PATH="$REPO_ROOT/$DOC_DIR"
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/generated_output.sh"
REPO_MAP_SCRIPT="$SCRIPT_DIR/gen_repo_map.sh"
TECH_STACK_SCRIPT="$SCRIPT_DIR/gen_tech_stack.py"
INDEX_SCRIPT="$SCRIPT_DIR/gen_index.py"
STALENESS_SCRIPT="$SCRIPT_DIR/check_pkb_staleness.py"
TRANSLATION_SYNC_SCRIPT="$SCRIPT_DIR/check_translation_sync.py"
GENERATED_DIR="$DOC_PATH/_generated"
SUMMARY_FILE="$GENERATED_DIR/level1-refresh-summary.generated.md"

mkdir -p "$GENERATED_DIR"

REPO_MAP_ARTIFACT="$GENERATED_DIR/repo-map.generated.md"
TECH_STACK_ARTIFACT="$GENERATED_DIR/03-tech-stack.generated.md"
INDEX_ARTIFACT="$GENERATED_DIR/index.generated.md"
STALENESS_ARTIFACT="$GENERATED_DIR/pkb-staleness-report.generated.md"
TRANSLATION_ARTIFACT="$GENERATED_DIR/translation-sync-report.generated.md"

artifact_lines=""
append_artifact() {
  local current="$1"
  local line="$2"
  if [[ -n "$current" ]]; then
    printf '%s\n%s' "$current" "$line"
  else
    printf '%s' "$line"
  fi
}

echo "=== PKB Level-1 Auto-Update ==="
echo "Repo root : $REPO_ROOT"
echo "Doc dir   : $DOC_PATH"
echo ""

if [[ -f "$REPO_MAP_SCRIPT" ]]; then
  echo "[1/5] Generating repo-map artifact..."
  bash "$REPO_MAP_SCRIPT" --repo-root "$REPO_ROOT" --doc-dir "$DOC_PATH" --output-file "$REPO_MAP_ARTIFACT"
  artifact_lines="$(append_artifact "$artifact_lines" "- \`$REPO_MAP_ARTIFACT\`")"
else
  echo "[1/5] gen_repo_map.sh not found, skipping"
fi

echo ""
if [[ -f "$TECH_STACK_SCRIPT" ]]; then
  echo "[2/5] Generating tech-stack artifact..."
  python3 "$TECH_STACK_SCRIPT" --repo-root "$REPO_ROOT" --doc-dir "$DOC_PATH" --output-file "$TECH_STACK_ARTIFACT"
  artifact_lines="$(append_artifact "$artifact_lines" "- \`$TECH_STACK_ARTIFACT\`")"
else
  echo "[2/5] gen_tech_stack.py not found, skipping"
fi

echo ""
if [[ -f "$INDEX_SCRIPT" && -d "$DOC_PATH" ]]; then
  echo "[3/5] Generating index candidate artifact..."
  python3 "$INDEX_SCRIPT" --doc-dir "$DOC_PATH" --output-file "$INDEX_ARTIFACT"
  artifact_lines="$(append_artifact "$artifact_lines" "- \`$INDEX_ARTIFACT\`")"
else
  echo "[3/5] gen_index.py not found or doc dir missing, skipping"
fi

echo ""
if [[ -f "$STALENESS_SCRIPT" ]]; then
  echo "[4/5] Generating staleness report artifact..."
  python3 "$STALENESS_SCRIPT" --repo-root "$REPO_ROOT" --doc-dir "$DOC_PATH" --output-file "$STALENESS_ARTIFACT" || true
  artifact_lines="$(append_artifact "$artifact_lines" "- \`$STALENESS_ARTIFACT\`")"
else
  echo "[4/5] check_pkb_staleness.py not found, skipping"
fi

echo ""
if [[ -f "$TRANSLATION_SYNC_SCRIPT" && -d "$DOC_PATH/locale/zh_CN/LC_MESSAGES" ]]; then
  echo "[5/5] Generating zh_CN translation sync report artifact..."
  python3 "$TRANSLATION_SYNC_SCRIPT" --repo-root "$REPO_ROOT" --doc-dir "$DOC_PATH" --output-file "$TRANSLATION_ARTIFACT" || true
  artifact_lines="$(append_artifact "$artifact_lines" "- \`$TRANSLATION_ARTIFACT\`")"
else
  echo "[5/5] check_translation_sync.py not found or zh_CN catalogs missing, skipping"
fi

SCRIPT_NAME="$(script_id "$0")"
PROVENANCE_BLOCK="- Repo root: \`$REPO_ROOT\`
- Doc root: \`$DOC_PATH\`
- Output root: \`$GENERATED_DIR\`"
BODY_BLOCK="## Generated Artifacts

${artifact_lines:-_No artifacts were generated._}

## Next Step

Use only the affected PKB page(s), the relevant source diff, and the artifact files above as bounded input for any targeted LLM update."
NEEDS_INPUT_BLOCK="- [NEEDS INPUT: decide which generated artifacts should be merged into human-maintained PKB pages now versus batched for a later refresh]
- [NEEDS INPUT: if an LLM is used next, choose the minimal affected docs and source snippets instead of feeding the full repo]"

write_markdown_artifact "$SUMMARY_FILE" "$SCRIPT_NAME" "Level 1 Refresh Summary" "$PROVENANCE_BLOCK" "$BODY_BLOCK" "$NEEDS_INPUT_BLOCK"

echo ""
echo "=== Level-1 update complete ==="
echo "Summary artifact: $SUMMARY_FILE"
