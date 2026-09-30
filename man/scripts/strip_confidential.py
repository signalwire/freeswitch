#!/usr/bin/env python3
"""
Remove confidential PKB pages from a built Sphinx site before AgentBox packaging.

Walks the source `*.md` files under --doc-dir, parses each file's
<!-- PKB-metadata --> footer for `confidentiality: L1..L5`, and deletes the
matching built artifacts (HTML page, source `.txt`, doctree pickle when
present) from --site-dir/{en,zh}/<docname>* for any page whose level meets
or exceeds --min-level (default L3).

This is a *post-build* gate. Sphinx still renders every page so a human can
preview locally; this step strips the artifacts before they reach AgentBox.

Pages without a confidentiality field default to L1 (Public) and are kept.
A page with an unrecognized level is treated as L5 (most restrictive) and
stripped, on the principle that an explicit-but-unparseable label is more
likely a typo than an intent to publish.

Exit codes:
    0 - completed successfully (whether or not anything was stripped)
    1 - input directory missing or unreadable
    2 - bad arguments (e.g. invalid --min-level)

Usage:
    python3 scripts/strip_confidential.py
    python3 scripts/strip_confidential.py --site-dir _build/site --doc-dir .
    python3 scripts/strip_confidential.py --min-level L4 --dry-run
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

LEVEL_ORDER = {"L1": 1, "L2": 2, "L3": 3, "L4": 4, "L5": 5}
DEFAULT_LEVEL = "L1"
DEFAULT_MIN_LEVEL = "L3"
UNKNOWN_LEVEL_FALLBACK = "L5"

METADATA_BLOCK_RE = re.compile(r"<!--\s*PKB-metadata\s*\n(.*?)-->", re.DOTALL)
FIELD_RE = re.compile(r"^(\w+)[ \t]*:[ \t]*(.*?)[ \t]*$", re.MULTILINE)
LANG_DIRS = ("en", "zh", "zh_CN")
HTML_SUFFIXES = (".html",)
SOURCE_SUFFIXES = (".txt",)
DOCTREE_DIR = ".doctrees"


@dataclass
class StripResult:
    docname: str
    source_md: str
    level: str
    removed: list[str]


def parse_level(text: str) -> str:
    """Return the confidentiality level declared in `text`, or DEFAULT_LEVEL."""
    m = METADATA_BLOCK_RE.search(text)
    if not m:
        return DEFAULT_LEVEL
    for fm in FIELD_RE.finditer(m.group(1)):
        if fm.group(1).strip() == "confidentiality":
            raw = fm.group(2).strip().upper()
            return raw if raw in LEVEL_ORDER else UNKNOWN_LEVEL_FALLBACK
    return DEFAULT_LEVEL


def collect_md_files(doc_dir: Path) -> list[Path]:
    """All `*.md` files under doc_dir, excluding build/_generated trees."""
    skip_parts = {"_build", "_generated", "node_modules", ".git"}
    out: list[Path] = []
    for path in sorted(doc_dir.rglob("*.md")):
        if any(part in skip_parts for part in path.relative_to(doc_dir).parts):
            continue
        out.append(path)
    return out


def docname_for(md_path: Path, doc_dir: Path) -> str:
    """Sphinx `docname` is the path relative to source root with `.md` stripped."""
    rel = md_path.relative_to(doc_dir).with_suffix("")
    return rel.as_posix()


def remove_built_artifacts(
    site_dir: Path,
    docname: str,
    dry_run: bool,
) -> list[str]:
    """Remove HTML and source artifacts for `docname` under each lang subdir.

    Returns the list of removed (or would-be-removed) paths, relative to
    `site_dir`.
    """
    removed: list[str] = []

    for lang in LANG_DIRS:
        lang_root = site_dir / lang
        if not lang_root.is_dir():
            continue

        for suffix in HTML_SUFFIXES:
            target = lang_root / f"{docname}{suffix}"
            if target.is_file():
                removed.append(str(target.relative_to(site_dir)))
                if not dry_run:
                    target.unlink()

        sources_root = lang_root / "_sources"
        for suffix in SOURCE_SUFFIXES + (".md.txt",):
            target = sources_root / f"{docname}{suffix}"
            if target.is_file():
                removed.append(str(target.relative_to(site_dir)))
                if not dry_run:
                    target.unlink()

        doctree_root = lang_root / DOCTREE_DIR
        if doctree_root.is_dir():
            for ext in (".doctree",):
                target = doctree_root / f"{docname}{ext}"
                if target.is_file():
                    removed.append(str(target.relative_to(site_dir)))
                    if not dry_run:
                        target.unlink()

    return removed


def strip(
    site_dir: Path,
    doc_dir: Path,
    min_level: str,
    dry_run: bool,
) -> list[StripResult]:
    threshold = LEVEL_ORDER[min_level]
    results: list[StripResult] = []
    for md in collect_md_files(doc_dir):
        try:
            text = md.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        level = parse_level(text)
        if LEVEL_ORDER[level] < threshold:
            continue
        docname = docname_for(md, doc_dir)
        removed = remove_built_artifacts(site_dir, docname, dry_run)
        results.append(
            StripResult(
                docname=docname,
                source_md=str(md.relative_to(doc_dir)),
                level=level,
                removed=removed,
            )
        )
    return results


def print_table(results: list[StripResult], min_level: str, dry_run: bool) -> None:
    action = "Would strip" if dry_run else "Stripped"
    if not results:
        print(f"No pages at >= {min_level}; nothing to strip.")
        return

    print()
    print(
        f"Confidentiality strip — threshold {min_level} "
        f"({'dry run' if dry_run else 'live'})"
    )
    print("=" * 72)
    col_doc = max(len("Document"), max(len(r.source_md) for r in results))
    col_lvl = max(len("Level"), max(len(r.level) for r in results))
    print(f"{'Document':<{col_doc}}  {'Level':<{col_lvl}}  Removed artifacts")
    print("-" * 72)
    for r in results:
        if r.removed:
            print(f"{r.source_md:<{col_doc}}  {r.level:<{col_lvl}}  {len(r.removed)} file(s)")
            for path in r.removed:
                print(f"{'':<{col_doc + col_lvl + 4}}  - {path}")
        else:
            print(
                f"{r.source_md:<{col_doc}}  {r.level:<{col_lvl}}  "
                f"(no built artifacts found)"
            )
    print("-" * 72)
    total_removed = sum(len(r.removed) for r in results)
    print(
        f"{action} {total_removed} artifact(s) for {len(results)} confidential page(s)."
    )
    print()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--site-dir", default="_build/site",
        help="Built Sphinx site root (default: _build/site)",
    )
    parser.add_argument(
        "--doc-dir", default=".",
        help="PKB source root containing *.md files (default: .)",
    )
    parser.add_argument(
        "--min-level", default=DEFAULT_MIN_LEVEL,
        choices=sorted(LEVEL_ORDER.keys()),
        help=f"Minimum level to strip (default: {DEFAULT_MIN_LEVEL})",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Report what would be stripped without deleting any files",
    )
    parser.add_argument(
        "--json", action="store_true", dest="output_json",
        help="Emit machine-readable JSON instead of a table",
    )
    args = parser.parse_args()

    site_dir = Path(args.site_dir)
    doc_dir = Path(args.doc_dir)

    if not doc_dir.is_dir():
        print(f"Error: doc-dir not found: {doc_dir}", file=sys.stderr)
        sys.exit(1)
    if not site_dir.is_dir():
        print(
            f"Warning: site-dir not found: {site_dir}. "
            "Nothing to strip (run 'make html-all' first if you intended to publish).",
            file=sys.stderr,
        )
        if args.output_json:
            print(json.dumps([]))
        sys.exit(0)

    results = strip(site_dir, doc_dir, args.min_level, args.dry_run)

    if args.output_json:
        print(json.dumps(
            [
                {
                    "docname": r.docname,
                    "source_md": r.source_md,
                    "level": r.level,
                    "removed": r.removed,
                }
                for r in results
            ],
            indent=2,
        ))
    else:
        print_table(results, args.min_level, args.dry_run)


if __name__ == "__main__":
    main()
