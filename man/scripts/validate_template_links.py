#!/usr/bin/env python3
"""
Validate PKB template cross-links against the canonical Sphinx index page set.

This catches stale numbering such as old compact-layout targets that no longer
match the standard 00-12 PKB layout.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import asdict, dataclass
from typing import List, Sequence

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from generated_output import render_json_artifact, render_markdown_artifact, script_id, write_text_file

TOCTREE_START = "```{toctree}"
FENCE = "```"
LINK_RE = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
EXTERNAL_PREFIXES = ("http://", "https://", "mailto:", "#")
DEPRECATED_TARGETS = {
    "01-repo-map.md": "04-repo-map.md",
    "03-workflows.md": "06-workflows.md",
    "04-data-and-api.md": "05-data-and-api.md",
    "05-conventions.md": "07-conventions.md",
    "06-runbook.md": "10-runbook.md",
    "07-testing.md": "09-testing.md",
}
IGNORED_TEMPLATE_FILES = {"diagrams-guide.md"}


@dataclass
class LinkIssue:
    file: str
    line: int
    target: str
    issue: str
    suggested_target: str | None = None


def to_doc_path(entry: str) -> str:
    normalized = entry.strip()
    if not normalized:
        return normalized
    if normalized.endswith(".md"):
        return normalized
    return f"{normalized}.md"


def load_canonical_targets(index_file: str) -> set[str]:
    targets: set[str] = set()
    in_toctree = False

    with open(index_file, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if line == TOCTREE_START:
                in_toctree = True
                continue
            if in_toctree and line == FENCE:
                in_toctree = False
                continue
            if not in_toctree:
                continue
            if not line or line.startswith(":"):
                continue
            targets.add(to_doc_path(line))

    return targets


def normalize_target(target: str) -> str:
    cleaned = target.strip()
    cleaned = cleaned.split("#", 1)[0].split("?", 1)[0]
    return cleaned


def is_external(target: str) -> bool:
    return target.startswith(EXTERNAL_PREFIXES) or "://" in target


def iter_template_files(template_dir: str) -> Sequence[str]:
    return sorted(
        os.path.join(template_dir, name)
        for name in os.listdir(template_dir)
        if name.endswith(".md") and name not in IGNORED_TEMPLATE_FILES
    )


def validate_links(template_dir: str, canonical_targets: set[str]) -> List[LinkIssue]:
    issues: List[LinkIssue] = []

    for path in iter_template_files(template_dir):
        rel_path = os.path.relpath(path, template_dir)
        with open(path, "r", encoding="utf-8") as handle:
            for line_no, raw_line in enumerate(handle, start=1):
                for raw_target in LINK_RE.findall(raw_line):
                    target = normalize_target(raw_target)
                    if not target or is_external(target):
                        continue
                    if target in DEPRECATED_TARGETS:
                        issues.append(
                            LinkIssue(
                                file=rel_path,
                                line=line_no,
                                target=target,
                                issue="deprecated_target",
                                suggested_target=DEPRECATED_TARGETS[target],
                            )
                        )
                        continue
                    if target not in canonical_targets:
                        issues.append(
                            LinkIssue(
                                file=rel_path,
                                line=line_no,
                                target=target,
                                issue="unknown_target",
                            )
                        )

    return issues


def render_markdown_report(issues: List[LinkIssue], canonical_targets: set[str]) -> str:
    if not issues:
        lines = [
            "All checked template links align with the canonical PKB page set.",
            "",
            "## Canonical Targets",
            "",
        ]
        lines.extend([f"- `{target}`" for target in sorted(canonical_targets)])
        lines.append("")
        return "\n".join(lines)

    lines = [
        "## Validation Issues",
        "",
        "| File | Line | Target | Issue | Suggested target |",
        "|------|------|--------|-------|------------------|",
    ]
    for issue in issues:
        suggestion = f"`{issue.suggested_target}`" if issue.suggested_target else ""
        lines.append(
            f"| `{issue.file}` | `{issue.line}` | `{issue.target}` | `{issue.issue}` | {suggestion} |"
        )
    lines.append("")
    return "\n".join(lines)


def print_report(issues: List[LinkIssue]) -> None:
    if not issues:
        print("All checked template links align with the canonical PKB page set.")
        return

    print("\nTemplate Link Validation Report\n")
    print(f"{'File':<28} {'Line':>5}  {'Target':<24} Issue")
    print("-" * 96)
    for issue in issues:
        suffix = f" -> {issue.suggested_target}" if issue.suggested_target else ""
        print(f"{issue.file:<28} {issue.line:>5}  {issue.target:<24} {issue.issue}{suffix}")
    print(f"\nSummary: {len(issues)} issue(s)")


def main() -> None:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    skill_root = os.path.dirname(script_dir)
    default_template_dir = os.path.join(skill_root, "templates", "docs")
    default_index_file = os.path.join(skill_root, "templates", "sphinx", "index.md")

    parser = argparse.ArgumentParser(description="Validate PKB template links against the canonical page set")
    parser.add_argument("--template-dir", default=default_template_dir, help="Directory containing template markdown files")
    parser.add_argument("--index-file", default=default_index_file, help="Canonical Sphinx index.md used as the page-set source of truth")
    parser.add_argument("--json", action="store_true", help="Output JSON")
    parser.add_argument("--output-file", help="Optional artifact output file")
    args = parser.parse_args()

    template_dir = os.path.abspath(args.template_dir)
    index_file = os.path.abspath(args.index_file)
    if not os.path.isdir(template_dir):
        raise SystemExit(f"Template directory not found: {template_dir}")
    if not os.path.isfile(index_file):
        raise SystemExit(f"Index file not found: {index_file}")

    canonical_targets = load_canonical_targets(index_file)
    if not canonical_targets:
        raise SystemExit(f"No canonical toctree targets found in {index_file}")

    issues = validate_links(template_dir, canonical_targets)

    if args.output_file:
        payload = {
            "canonical_targets": sorted(canonical_targets),
            "issues": [asdict(issue) for issue in issues],
        }
        provenance = [
            os.path.relpath(template_dir, skill_root),
            os.path.relpath(index_file, skill_root),
        ]
        needs_input = [
            "decide whether any intentional legacy cross-links should remain as plain text instead of markdown links",
            "if the standard PKB numbering changes, update templates/sphinx/index.md before changing template links",
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
                title="Template Link Validation Report",
                provenance=provenance,
                body=render_markdown_report(issues, canonical_targets),
                needs_input=needs_input,
            )
        )
        write_text_file(args.output_file, artifact)
        print(f"Wrote {args.output_file}")
    else:
        if args.json:
            print(json.dumps({"canonical_targets": sorted(canonical_targets), "issues": [asdict(issue) for issue in issues]}, indent=2))
        else:
            print_report(issues)

    sys.exit(2 if issues else 0)


if __name__ == "__main__":
    main()
