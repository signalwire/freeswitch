#!/usr/bin/env python3
"""
PKB Staleness Checker — Level 1 automation (zero LLM tokens).

Compares source-code file timestamps against PKB page timestamps to detect
potentially stale documentation. Git commit time is preferred when available,
with filesystem mtime as a fallback.

Usage:
    python check_pkb_staleness.py --repo-root . --doc-dir doc
    python check_pkb_staleness.py --json
    python check_pkb_staleness.py --config .pkb-source-doc-map.json

Custom config format:
[
  {"pattern": "src/**/*.ts", "docs": ["02-architecture", "06-workflows"]},
  {"pattern": "package.json", "docs": ["03-tech-stack", "08-build"]}
]
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from glob import glob
from typing import Dict, Iterable, List, Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from generated_output import render_json_artifact, render_markdown_artifact, script_id, write_text_file

DEFAULT_SOURCE_DOC_MAP = [
    # Architecture and structure
    {"pattern": "src/**/*.ts", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "src/**/*.tsx", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "src/**/*.js", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "src/**/*.jsx", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "src/**/*.py", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "src/**/*.go", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "src/**/*.rs", "docs": ["02-architecture", "05-data-and-api"]},
    {"pattern": "app/**/*.py", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "internal/**/*.go", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "cmd/**/*.go", "docs": ["02-architecture", "04-repo-map"]},
    {"pattern": "server/**/*.ts", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "services/**/*.py", "docs": ["02-architecture", "06-workflows"]},
    {"pattern": "routes/**/*.ts", "docs": ["05-data-and-api", "06-workflows"]},
    {"pattern": "controllers/**/*.ts", "docs": ["05-data-and-api", "06-workflows"]},
    {"pattern": "handlers/**/*.go", "docs": ["05-data-and-api", "06-workflows"]},
    {"pattern": "migrations/**", "docs": ["05-data-and-api"]},
    {"pattern": "proto/**", "docs": ["05-data-and-api"]},
    {"pattern": "openapi*.yaml", "docs": ["05-data-and-api"]},
    {"pattern": "openapi*.yml", "docs": ["05-data-and-api"]},
    {"pattern": "openapi*.json", "docs": ["05-data-and-api"]},
    # Tech stack and build
    {"pattern": "package.json", "docs": ["03-tech-stack", "08-build"]},
    {"pattern": "pnpm-lock.yaml", "docs": ["03-tech-stack", "08-build"]},
    {"pattern": "package-lock.json", "docs": ["03-tech-stack", "08-build"]},
    {"pattern": "yarn.lock", "docs": ["03-tech-stack", "08-build"]},
    {"pattern": "Cargo.toml", "docs": ["03-tech-stack", "08-build"]},
    {"pattern": "**/Cargo.toml", "docs": ["03-tech-stack", "08-build"]},
    {"pattern": "go.mod", "docs": ["03-tech-stack", "08-build"]},
    {"pattern": "pom.xml", "docs": ["03-tech-stack", "08-build"]},
    {"pattern": "pyproject.toml", "docs": ["03-tech-stack", "08-build", "07-conventions"]},
    {"pattern": "requirements*.txt", "docs": ["03-tech-stack", "08-build"]},
    {"pattern": "vite.config.*", "docs": ["08-build"]},
    {"pattern": "webpack.config.*", "docs": ["08-build"]},
    {"pattern": "Dockerfile*", "docs": ["08-build", "10-runbook"]},
    {"pattern": "docker-compose*.yml", "docs": ["08-build", "10-runbook"]},
    {"pattern": "docker-compose*.yaml", "docs": ["08-build", "10-runbook"]},
    {"pattern": ".github/workflows/*.yml", "docs": ["08-build"]},
    {"pattern": ".github/workflows/*.yaml", "docs": ["08-build"]},
    {"pattern": "Jenkinsfile", "docs": ["08-build"]},
    {"pattern": "Makefile", "docs": ["01-quick-start", "08-build"]},
    # Conventions
    {"pattern": ".editorconfig", "docs": ["07-conventions"]},
    {"pattern": ".eslintrc*", "docs": ["07-conventions"]},
    {"pattern": ".prettierrc*", "docs": ["07-conventions"]},
    {"pattern": "tsconfig*.json", "docs": ["07-conventions"]},
    {"pattern": "rustfmt.toml", "docs": ["07-conventions"]},
    {"pattern": ".flake8", "docs": ["07-conventions"]},
    {"pattern": "mypy.ini", "docs": ["07-conventions"]},
    # Testing
    {"pattern": "tests/**", "docs": ["09-testing"]},
    {"pattern": "test/**", "docs": ["09-testing"]},
    {"pattern": "src/**/*.test.*", "docs": ["09-testing"]},
    {"pattern": "src/**/*_test.*", "docs": ["09-testing"]},
    # Observability
    {"pattern": "monitoring/**", "docs": ["11-observability"]},
    {"pattern": "grafana/**", "docs": ["11-observability"]},
    {"pattern": "prometheus/**", "docs": ["11-observability"]},
]

DEFAULT_CONFIG_CANDIDATES = [
    ".pkb-source-doc-map.json",
    "doc/pkb-source-doc-map.json",
    "man/pkb-source-doc-map.json",
]


@dataclass
class StaleEntry:
    doc_page: str
    source_file: str
    source_mtime: datetime
    doc_mtime: datetime
    days_behind: int
    level: str


def git_last_modified(path: str, repo_root: str) -> Optional[datetime]:
    """Get the last git commit timestamp for a file."""
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
    """Get filesystem mtime."""
    try:
        return datetime.fromtimestamp(os.path.getmtime(path), tz=timezone.utc)
    except OSError:
        return None


def find_files_by_glob(pattern: str, repo_root: str) -> List[str]:
    """Expand glob pattern relative to repo root."""
    full_pattern = os.path.join(repo_root, pattern)
    matches = glob(full_pattern, recursive=True)
    return sorted(
        os.path.relpath(match, repo_root) for match in matches if os.path.isfile(match)
    )


def detect_level(days_behind: int) -> str:
    """Convert age delta to report severity."""
    if days_behind <= 3:
        return "info"
    if days_behind <= 14:
        return "warning"
    return "critical"


def load_source_doc_map(repo_root: str, config_path: Optional[str]) -> List[dict]:
    """Load a project-specific map if provided, otherwise use defaults."""
    candidates: Iterable[str]
    if config_path:
        candidates = [config_path]
    else:
        candidates = DEFAULT_CONFIG_CANDIDATES

    for candidate in candidates:
        candidate_path = candidate
        if not os.path.isabs(candidate_path):
            candidate_path = os.path.join(repo_root, candidate_path)
        if not os.path.isfile(candidate_path):
            continue
        with open(candidate_path, "r", encoding="utf-8") as handle:
            loaded = json.load(handle)
        normalized = []
        for item in loaded:
            pattern = item.get("pattern")
            docs = item.get("docs", [])
            if not pattern or not isinstance(docs, list):
                raise ValueError(f"Invalid rule in {candidate_path}: {item}")
            normalized.append({"pattern": pattern, "docs": docs})
        return normalized

    return DEFAULT_SOURCE_DOC_MAP


def collect_doc_mtimes(repo_root: str, doc_dir: str, use_git: bool) -> Dict[str, datetime]:
    """Collect mtimes for top-level PKB pages in the docs directory."""
    doc_path = doc_dir if os.path.isabs(doc_dir) else os.path.join(repo_root, doc_dir)
    doc_mtimes: Dict[str, datetime] = {}
    if not os.path.isdir(doc_path):
        return doc_mtimes

    for name in os.listdir(doc_path):
        if not name.endswith(".md"):
            continue
        rel_path = os.path.relpath(os.path.join(doc_path, name), repo_root)
        abs_path = os.path.join(doc_path, name)
        mtime = git_last_modified(rel_path, repo_root) if use_git else None
        if mtime is None:
            mtime = fs_last_modified(abs_path)
        if mtime is not None:
            doc_mtimes[name[:-3]] = mtime
    return doc_mtimes


def detect_staleness(
    repo_root: str,
    doc_dir: str = "doc",
    use_git: bool = True,
    config_path: Optional[str] = None,
) -> List[StaleEntry]:
    """Scan source files and compare timestamps against doc pages."""
    doc_mtimes = collect_doc_mtimes(repo_root, doc_dir, use_git)
    if not doc_mtimes:
        print(
            f"Warning: PKB doc directory not found or empty: {os.path.join(repo_root, doc_dir)}",
            file=sys.stderr,
        )
        return []

    rules = load_source_doc_map(repo_root, config_path)
    results: List[StaleEntry] = []

    for rule in rules:
        affected_docs = rule["docs"]
        if not affected_docs:
            continue

        for src_file in find_files_by_glob(rule["pattern"], repo_root):
            src_path = os.path.join(repo_root, src_file)
            src_mtime = git_last_modified(src_file, repo_root) if use_git else None
            if src_mtime is None:
                src_mtime = fs_last_modified(src_path)
            if src_mtime is None:
                continue

            for doc_page in affected_docs:
                doc_mtime = doc_mtimes.get(doc_page)
                if doc_mtime is None or src_mtime <= doc_mtime:
                    continue
                days_behind = max(0, (src_mtime - doc_mtime).days)
                results.append(
                    StaleEntry(
                        doc_page=doc_page,
                        source_file=src_file,
                        source_mtime=src_mtime,
                        doc_mtime=doc_mtime,
                        days_behind=days_behind,
                        level=detect_level(days_behind),
                    )
                )

    deduped: Dict[tuple, StaleEntry] = {}
    for entry in results:
        key = (entry.doc_page, entry.source_file)
        existing = deduped.get(key)
        if existing is None or entry.days_behind > existing.days_behind:
            deduped[key] = entry

    return sorted(deduped.values(), key=lambda item: (-item.days_behind, item.doc_page, item.source_file))


def serialize_entries(entries: List[StaleEntry]) -> List[dict]:
    return [
        {
            "doc_page": entry.doc_page,
            "source_file": entry.source_file,
            "days_behind": entry.days_behind,
            "level": entry.level,
            "source_mtime": entry.source_mtime.isoformat(),
            "doc_mtime": entry.doc_mtime.isoformat(),
        }
        for entry in entries
    ]


def render_markdown_report(entries: List[StaleEntry]) -> str:
    if not entries:
        return "All tracked PKB pages are up-to-date.\n"

    lines = [
        "## Staleness Report",
        "",
        "| Level | Days Behind | Doc Page | Source File |",
        "|-------|-------------|----------|-------------|",
    ]
    for entry in entries:
        lines.append(
            f"| `{entry.level}` | `{entry.days_behind}` | `{entry.doc_page}` | `{entry.source_file}` |"
        )
    lines.append("")
    return "\n".join(lines)


def print_report(entries: List[StaleEntry], as_json: bool = False) -> None:
    """Print the staleness report."""
    if as_json:
        print(json.dumps(serialize_entries(entries), indent=2))
        return

    if not entries:
        print("All tracked PKB pages are up-to-date!")
        return

    icons = {"info": "INFO", "warning": "WARN", "critical": "CRIT"}
    print(f"\nPKB Staleness Report - {len(entries)} item(s)\n")
    print(f"{'Level':<10} {'Days':>5}  {'Doc Page':<25} {'Source File'}")
    print("-" * 96)
    for entry in entries:
        icon = icons.get(entry.level, "INFO")
        print(
            f"[{icon}] {entry.level:<7} {entry.days_behind:>5}d  "
            f"{entry.doc_page:<25} {entry.source_file}"
        )

    critical = [entry for entry in entries if entry.level == "critical"]
    warning = [entry for entry in entries if entry.level == "warning"]
    info = [entry for entry in entries if entry.level == "info"]
    print(f"\nSummary: {len(critical)} critical, {len(warning)} warning, {len(info)} info")

    if critical:
        print("\nCritical pages needing immediate review:")
        for doc_page in sorted({entry.doc_page for entry in critical}):
            print(f"  - {doc_page}.md")


def main() -> None:
    parser = argparse.ArgumentParser(description="PKB Staleness Checker")
    parser.add_argument("--repo-root", default=".", help="Repository root path")
    parser.add_argument("--doc-dir", default="doc", help="Doc directory relative to repo root")
    parser.add_argument("--config", help="Optional JSON mapping file for source-to-doc rules")
    parser.add_argument("--no-git", action="store_true", help="Use filesystem mtime instead of git time")
    parser.add_argument("--json", action="store_true", help="Output JSON format")
    parser.add_argument("--output-file", help="Optional report artifact file")
    args = parser.parse_args()

    repo_root = os.path.abspath(args.repo_root)
    entries = detect_staleness(
        repo_root=repo_root,
        doc_dir=args.doc_dir,
        use_git=not args.no_git,
        config_path=args.config,
    )
    if args.output_file:
        payload = serialize_entries(entries)
        provenance = [args.doc_dir]
        if args.config:
            provenance.append(args.config)
        needs_input = [
            "decide which stale pages need immediate refresh versus batched update in the next PR, sprint, or release",
            "for each flagged page, choose whether a deterministic artifact is sufficient or a targeted LLM pass is needed",
        ]
        artifact = (
            render_json_artifact(
                script_name=script_id(__file__),
                payload=payload,
                provenance=provenance,
                needs_input=needs_input,
            )
            if args.json
            else render_markdown_artifact(
                script_name=script_id(__file__),
                title="PKB Staleness Report",
                provenance=provenance,
                body=render_markdown_report(entries),
                needs_input=needs_input,
            )
        )
        write_text_file(args.output_file, artifact)
        print(f"Wrote {args.output_file}")
    else:
        print_report(entries, as_json=args.json)

    if any(entry.level == "critical" for entry in entries):
        sys.exit(2)
    if any(entry.level == "warning" for entry in entries):
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
