#!/usr/bin/env python3
"""
PKB Translation Sync Checker — zero LLM token bilingual freshness check.

Checks whether Chinese gettext catalogs under `locale/zh_CN/LC_MESSAGES/`
appear in sync with the English Markdown source pages.

Signals reported:
  - missing `.po` files
  - source newer than translation
  - POT newer than PO revision
  - untranslated entries (`msgstr ""`)
  - fuzzy entries
  - obsolete entries (`#~`)

Exit codes:
  0 = clean
  1 = warnings only
  2 = at least one critical issue
"""

from __future__ import annotations

import argparse
import ast
import json
import os
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Dict, Iterable, List, Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from generated_output import render_json_artifact, render_markdown_artifact, script_id, write_text_file

IGNORED_DOC_DIRS = {"locale", "_build", "_static", "_templates", "_generated", "functions"}
DATE_FORMATS = [
    "%Y-%m-%d %H:%M%z",
    "%Y-%m-%d %H:%M:%S%z",
    "%Y-%m-%d %H:%M%z\n",
]


@dataclass
class PoEntry:
    msgid: str = ""
    msgstr: str = ""
    flags: List[str] = field(default_factory=list)
    obsolete: bool = False


@dataclass
class TranslationStatus:
    doc_page: str
    source_file: str
    po_file: str
    level: str
    issues: List[str]
    source_mtime: Optional[str] = None
    po_mtime: Optional[str] = None
    pot_creation_date: Optional[str] = None
    po_revision_date: Optional[str] = None
    untranslated_count: int = 0
    fuzzy_count: int = 0
    obsolete_count: int = 0
    days_behind: int = 0

def git_last_modified(path: str, repo_root: str) -> Optional[datetime]:
    try:
        result = subprocess.run(
            ["git", "log", "-1", "--format=%aI", "--", path],
            capture_output=True,
            text=True,
            cwd=repo_root,
            timeout=10,
        )
        if result.returncode == 0 and result.stdout.strip():
            return datetime.fromisoformat(result.stdout.strip())
    except (subprocess.TimeoutExpired, FileNotFoundError, ValueError):
        pass
    return None


def fs_last_modified(path: str) -> Optional[datetime]:
    try:
        return datetime.fromtimestamp(os.path.getmtime(path), tz=timezone.utc)
    except OSError:
        return None


def best_mtime(repo_root: str, path: str, use_git: bool) -> Optional[datetime]:
    rel_path = os.path.relpath(path, repo_root)
    mtime = git_last_modified(rel_path, repo_root) if use_git else None
    if mtime is None:
        mtime = fs_last_modified(path)
    return mtime


def iter_source_docs(doc_dir: str) -> Iterable[str]:
    for current_root, dirnames, filenames in os.walk(doc_dir):
        dirnames[:] = [name for name in dirnames if name not in IGNORED_DOC_DIRS]
        for filename in filenames:
            if filename.endswith(".md"):
                yield os.path.join(current_root, filename)


def unquote_po_string(value: str) -> str:
    value = value.strip()
    if not value:
        return ""
    try:
        return ast.literal_eval(value)
    except Exception:
        if value.startswith('"') and value.endswith('"'):
            return value[1:-1]
        return value


def parse_po_entries(path: str) -> List[PoEntry]:
    with open(path, "r", encoding="utf-8", errors="ignore") as handle:
        lines = handle.readlines()

    entries: List[PoEntry] = []
    block: List[str] = []

    def flush() -> None:
        nonlocal block
        if not block:
            return
        entries.append(parse_po_block(block))
        block = []

    for line in lines:
        if line.strip() == "":
            flush()
        else:
            block.append(line.rstrip("\n"))
    flush()
    return entries


def parse_po_block(lines: List[str]) -> PoEntry:
    obsolete = all(line.startswith("#~") or not line.strip() for line in lines)
    normalized = [line[3:] if line.startswith("#~ ") else line for line in lines]

    entry = PoEntry(obsolete=obsolete)
    current_field = None

    for line in normalized:
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("#,"):
            flags = stripped[2:].split(",")
            entry.flags.extend(flag.strip() for flag in flags if flag.strip())
            continue
        if stripped.startswith("msgid "):
            current_field = "msgid"
            entry.msgid = unquote_po_string(stripped[6:])
            continue
        if stripped.startswith("msgstr "):
            current_field = "msgstr"
            entry.msgstr = unquote_po_string(stripped[7:])
            continue
        if stripped.startswith('"'):
            if current_field == "msgid":
                entry.msgid += unquote_po_string(stripped)
            elif current_field == "msgstr":
                entry.msgstr += unquote_po_string(stripped)

    return entry


def extract_header_map(entries: List[PoEntry]) -> Dict[str, str]:
    if not entries or entries[0].msgid != "":
        return {}

    header = {}
    for raw_line in entries[0].msgstr.split("\n"):
        if ":" not in raw_line:
            continue
        key, value = raw_line.split(":", 1)
        header[key.strip()] = value.strip()
    return header


def parse_header_datetime(value: Optional[str]) -> Optional[datetime]:
    if not value:
        return None
    for fmt in DATE_FORMATS:
        try:
            return datetime.strptime(value, fmt)
        except ValueError:
            continue
    return None


def level_for_translation(
    *,
    missing_po: bool,
    untranslated_count: int,
    fuzzy_count: int,
    header_stale: bool,
    source_newer: bool,
) -> str:
    if missing_po or untranslated_count > 0:
        return "critical"
    if fuzzy_count > 0 or header_stale or source_newer:
        return "warning"
    return "info"


def detect_translation_sync(
    repo_root: str,
    doc_dir: str,
    locale_root: Optional[str],
    lang: str,
    use_git: bool,
) -> List[TranslationStatus]:
    repo_root = os.path.abspath(repo_root)
    doc_path = doc_dir if os.path.isabs(doc_dir) else os.path.join(repo_root, doc_dir)
    if locale_root:
        locale_path = locale_root if os.path.isabs(locale_root) else os.path.join(repo_root, locale_root)
    else:
        locale_path = os.path.join(doc_path, "locale")
    po_root = os.path.join(locale_path, lang, "LC_MESSAGES")

    if not os.path.isdir(doc_path):
        print(f"Warning: doc directory not found: {doc_path}", file=sys.stderr)
        return []

    results: List[TranslationStatus] = []
    for source_file in sorted(iter_source_docs(doc_path)):
        source_rel = os.path.relpath(source_file, doc_path)
        po_rel = os.path.splitext(source_rel)[0] + ".po"
        po_file = os.path.join(po_root, po_rel)
        source_mtime = best_mtime(repo_root, source_file, use_git)

        issues: List[str] = []
        untranslated_count = 0
        fuzzy_count = 0
        obsolete_count = 0
        days_behind = 0
        pot_creation_raw = None
        po_revision_raw = None
        po_mtime = None
        header_stale = False
        source_newer = False

        if not os.path.isfile(po_file):
            issues.append("missing translation catalog")
            level = "critical"
        else:
            po_mtime = best_mtime(repo_root, po_file, use_git)
            entries = parse_po_entries(po_file)
            header = extract_header_map(entries)
            pot_creation_raw = header.get("POT-Creation-Date")
            po_revision_raw = header.get("PO-Revision-Date")
            pot_creation = parse_header_datetime(pot_creation_raw)
            po_revision = parse_header_datetime(po_revision_raw)

            for entry in entries[1:]:
                if entry.obsolete:
                    obsolete_count += 1
                    continue
                if "fuzzy" in entry.flags:
                    fuzzy_count += 1
                if entry.msgid and not entry.msgstr.strip():
                    untranslated_count += 1

            if pot_creation and po_revision and po_revision < pot_creation:
                header_stale = True
                issues.append("PO-Revision-Date is older than POT-Creation-Date")

            if source_mtime and po_mtime and source_mtime > po_mtime:
                source_newer = True
                days_behind = max(0, (source_mtime - po_mtime).days)
                issues.append("English source is newer than Chinese catalog")

            if untranslated_count:
                issues.append(f"{untranslated_count} untranslated entr{'y' if untranslated_count == 1 else 'ies'}")
            if fuzzy_count:
                issues.append(f"{fuzzy_count} fuzzy entr{'y' if fuzzy_count == 1 else 'ies'}")
            if obsolete_count:
                issues.append(f"{obsolete_count} obsolete entr{'y' if obsolete_count == 1 else 'ies'}")

            if not issues:
                issues.append("translation appears in sync")

            level = level_for_translation(
                missing_po=False,
                untranslated_count=untranslated_count,
                fuzzy_count=fuzzy_count,
                header_stale=header_stale,
                source_newer=source_newer,
            )
            if obsolete_count and level == "info":
                level = "info"

        results.append(
            TranslationStatus(
                doc_page=os.path.splitext(source_rel)[0].replace(os.sep, "/"),
                source_file=os.path.relpath(source_file, repo_root),
                po_file=os.path.relpath(po_file, repo_root) if os.path.isabs(po_file) else po_file,
                level=level,
                issues=issues,
                source_mtime=source_mtime.isoformat() if source_mtime else None,
                po_mtime=po_mtime.isoformat() if po_mtime else None,
                pot_creation_date=pot_creation_raw,
                po_revision_date=po_revision_raw,
                untranslated_count=untranslated_count,
                fuzzy_count=fuzzy_count,
                obsolete_count=obsolete_count,
                days_behind=days_behind,
            )
        )

    return results


def serialize_results(results: List[TranslationStatus]) -> List[dict]:
    return [result.__dict__ for result in results]


def render_markdown_report(results: List[TranslationStatus]) -> str:
    if not results:
        return "No source docs found to check.\n"

    actionable = [result for result in results if result.level != "info" or result.issues != ["translation appears in sync"]]
    if not actionable:
        return "All checked Chinese translations appear in sync with English sources.\n"

    lines = [
        "## Translation Sync Report",
        "",
        "| Level | Page | Issues |",
        "|-------|------|--------|",
    ]
    for result in actionable:
        lines.append(f"| `{result.level}` | `{result.doc_page}` | {'; '.join(result.issues)} |")
    lines.append("")
    return "\n".join(lines)


def print_report(results: List[TranslationStatus], as_json: bool) -> None:
    if as_json:
        print(json.dumps(serialize_results(results), indent=2))
        return

    if not results:
        print("No source docs found to check.")
        return

    actionable = [result for result in results if result.level != "info" or result.issues != ["translation appears in sync"]]
    if not actionable:
        print("All checked Chinese translations appear in sync with English sources.")
        return

    print("\nPKB Translation Sync Report\n")
    print(f"{'Level':<10} {'Page':<28} Issues")
    print("-" * 96)
    for result in actionable:
        print(f"{result.level:<10} {result.doc_page:<28} {', '.join(result.issues)}")

    critical = [result for result in actionable if result.level == "critical"]
    warning = [result for result in actionable if result.level == "warning"]
    info = [result for result in actionable if result.level == "info"]
    print(f"\nSummary: {len(critical)} critical, {len(warning)} warning, {len(info)} info")

    if critical:
        print("\nCritical pages:")
        for result in critical:
            print(f"  - {result.doc_page}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Check whether zh_CN translations are in sync with English docs")
    parser.add_argument("--repo-root", default=".", help="Repository root")
    parser.add_argument("--doc-dir", default="doc", help="English PKB doc directory")
    parser.add_argument("--locale-root", help="Locale root directory (defaults to <doc-dir>/locale)")
    parser.add_argument("--lang", default="zh_CN", help="Locale language code")
    parser.add_argument("--no-git", action="store_true", help="Use filesystem mtime instead of git time")
    parser.add_argument("--json", action="store_true", help="Output JSON report")
    parser.add_argument("--output-file", help="Optional report artifact file")
    args = parser.parse_args()

    results = detect_translation_sync(
        repo_root=os.path.abspath(args.repo_root),
        doc_dir=args.doc_dir,
        locale_root=args.locale_root,
        lang=args.lang,
        use_git=not args.no_git,
    )
    if args.output_file:
        provenance = [args.doc_dir]
        if args.locale_root:
            provenance.append(args.locale_root)
        needs_input = [
            "decide which missing or stale translations must be fixed before publish versus tracked for follow-up",
            "review fuzzy or untranslated entries whose final wording depends on product terminology or UX nuance",
        ]
        artifact = (
            render_json_artifact(
                script_name=script_id(__file__),
                payload=serialize_results(results),
                provenance=provenance,
                needs_input=needs_input,
            )
            if args.json
            else render_markdown_artifact(
                script_name=script_id(__file__),
                title="Translation Sync Report",
                provenance=provenance,
                body=render_markdown_report(results),
                needs_input=needs_input,
            )
        )
        write_text_file(args.output_file, artifact)
        print(f"Wrote {args.output_file}")
    else:
        print_report(results, as_json=args.json)

    if any(result.level == "critical" for result in results):
        sys.exit(2)
    if any(result.level == "warning" for result in results):
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
