#!/usr/bin/env python3
"""
PKB Review Status — read-only summary of document review states.

Scans PKB markdown files for <!-- PKB-metadata --> footers and reports
review_status (pending/approved) and review_score (0-5) for each doc.

Exit code is non-zero when any doc has review_status: pending (useful for
CI gates that require all docs to be human-approved before deployment).

Usage:
    python3 scripts/pkb_review_status.py --doc-dir .
    python3 scripts/pkb_review_status.py --doc-dir . --json
    python3 scripts/pkb_review_status.py --doc-dir . --strict
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import List, Optional


@dataclass
class DocReview:
    file: str
    review_status: str  # "pending", "approved", or "unknown"
    review_score: int
    reviewed_by: str
    last_updated: str
    updated_by: str
    confidentiality: str  # "L1".."L5"; defaults to L1 when unset


METADATA_BLOCK_RE = re.compile(
    r"<!--\s*PKB-metadata\s*\n(.*?)\n\s*-->",
    re.DOTALL,
)

FIELD_RE = re.compile(r"^(\w+):[ \t]*(.*?)[ \t]*$", re.MULTILINE)

PKB_NUMBERED_PAGES = re.compile(r"^\d{2}-.*\.md$")
KNOWN_PKB_FILES = {
    "index.md", "ai-guide.md", "README.md", "CHANGELOG.md",
    "appendix-01-faq.md", "appendix-02-glossary.md", "diagrams-guide.md",
}

VALID_LEVELS = {"L1", "L2", "L3", "L4", "L5"}
DEFAULT_LEVEL = "L1"


def is_pkb_page(filename: str) -> bool:
    return (
        PKB_NUMBERED_PAGES.match(filename) is not None
        or filename in KNOWN_PKB_FILES
    )


def parse_metadata(content: str) -> dict[str, str]:
    m = METADATA_BLOCK_RE.search(content)
    if not m:
        return {}
    fields = {}
    for fm in FIELD_RE.finditer(m.group(1)):
        fields[fm.group(1)] = fm.group(2)
    return fields


def scan_docs(doc_dir: str) -> List[DocReview]:
    results: List[DocReview] = []
    doc_path = Path(doc_dir)

    md_files = sorted(doc_path.glob("*.md"))
    for md in md_files:
        if not is_pkb_page(md.name):
            continue
        content = md.read_text(encoding="utf-8", errors="replace")
        meta = parse_metadata(content)
        if not meta:
            results.append(DocReview(
                file=md.name,
                review_status="unknown",
                review_score=0,
                reviewed_by="",
                last_updated="",
                updated_by="",
                confidentiality=DEFAULT_LEVEL,
            ))
            continue

        raw_level = meta.get("confidentiality", DEFAULT_LEVEL).strip().upper()
        level = raw_level if raw_level in VALID_LEVELS else DEFAULT_LEVEL

        results.append(DocReview(
            file=md.name,
            review_status=meta.get("review_status", "unknown"),
            review_score=int(meta.get("review_score", "0")),
            reviewed_by=meta.get("reviewed_by", ""),
            last_updated=meta.get("last_updated", ""),
            updated_by=meta.get("updated_by", ""),
            confidentiality=level,
        ))

    return results


def print_table(docs: List[DocReview]) -> None:
    hdr_file = "File"
    hdr_status = "Status"
    hdr_score = "Score"
    hdr_level = "Level"
    hdr_reviewer = "Reviewer"
    hdr_updated = "Updated"

    col_file = max(len(hdr_file), max((len(d.file) for d in docs), default=4))
    col_status = max(len(hdr_status), 8)
    col_score = len(hdr_score)
    col_level = max(len(hdr_level), 5)
    col_reviewer = max(len(hdr_reviewer), max((len(d.reviewed_by) for d in docs), default=0), 4)
    col_updated = max(len(hdr_updated), 10)

    header = (
        f"{hdr_file:<{col_file}}  "
        f"{hdr_status:<{col_status}}  "
        f"{hdr_score:<{col_score}}  "
        f"{hdr_level:<{col_level}}  "
        f"{hdr_reviewer:<{col_reviewer}}  "
        f"{hdr_updated}"
    )
    sep = "=" * len(header)

    print()
    print("PKB Review Status")
    print(sep)
    print(header)
    print("-" * len(header))

    for d in docs:
        status_display = d.review_status.upper() if d.review_status == "pending" else d.review_status
        if d.review_status == "unknown":
            status_display = "NO META"
        score_display = f"{d.review_score}/5"
        print(
            f"{d.file:<{col_file}}  "
            f"{status_display:<{col_status}}  "
            f"{score_display:<{col_score}}  "
            f"{d.confidentiality:<{col_level}}  "
            f"{d.reviewed_by:<{col_reviewer}}  "
            f"{d.last_updated}"
        )

    print("-" * len(header))
    total = len(docs)
    approved = sum(1 for d in docs if d.review_status == "approved")
    pending = sum(1 for d in docs if d.review_status in ("pending", "unknown"))
    scores = [d.review_score for d in docs if d.review_status == "approved"]
    avg = sum(scores) / len(scores) if scores else 0.0
    restricted = sum(1 for d in docs if d.confidentiality in ("L3", "L4", "L5"))
    print(
        f"Total: {total} docs | Approved: {approved} | Pending: {pending} | "
        f"Avg approved score: {avg:.1f}/5 | Restricted (L3+): {restricted}"
    )
    print()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--doc-dir", default=".",
        help="PKB root directory containing *.md files (default: .)",
    )
    parser.add_argument(
        "--json", action="store_true", dest="output_json",
        help="Output JSON instead of a table",
    )
    parser.add_argument(
        "--strict", action="store_true",
        help="Exit with code 1 if any doc is not approved (for CI gates)",
    )
    args = parser.parse_args()

    if not os.path.isdir(args.doc_dir):
        print(f"Error: directory not found: {args.doc_dir}", file=sys.stderr)
        sys.exit(2)

    docs = scan_docs(args.doc_dir)
    if not docs:
        print("No PKB documents found.", file=sys.stderr)
        sys.exit(2)

    if args.output_json:
        print(json.dumps([asdict(d) for d in docs], indent=2))
    else:
        print_table(docs)

    if args.strict:
        pending = [d for d in docs if d.review_status != "approved"]
        if pending:
            print(f"FAIL: {len(pending)} doc(s) not approved:", file=sys.stderr)
            for d in pending:
                print(f"  - {d.file} (status: {d.review_status})", file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
