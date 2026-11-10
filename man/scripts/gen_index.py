#!/usr/bin/env python3
"""
PKB index generator.

Scans a PKB docs directory and produces an `index.md` candidate using the
standard 00-12 page layout. This is deterministic and safe to run in CI or as
a local helper.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from datetime import date
from typing import Iterable, List

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from generated_output import generated_output_path, render_markdown_artifact, script_id, write_text_file

SECTIONS = [
    ("Getting Started", ["00-overview", "01-quick-start"]),
    ("Design & Structure", ["02-architecture", "03-tech-stack", "04-repo-map", "05-data-and-api", "06-workflows"]),
    ("Development", ["07-conventions", "08-build", "09-testing"]),
    ("Operations", ["10-runbook", "11-observability", "12-document"]),
]


def slug_to_title(slug: str) -> str:
    parts = slug.replace("_", "-").split("-")
    return " ".join(part.capitalize() for part in parts if part)


def detect_title(doc_dir: str, explicit_title: str | None) -> str:
    if explicit_title:
        return explicit_title
    repo_name = os.path.basename(os.path.abspath(os.path.join(doc_dir, os.pardir)))
    return f"{slug_to_title(repo_name)} Knowledge Base"


def git_head_short(doc_dir: str) -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=doc_dir,
            capture_output=True,
            text=True,
            timeout=10,
            check=True,
        )
        return result.stdout.strip()
    except Exception:
        return "[git rev-parse --short HEAD]"


def existing_docs(doc_dir: str) -> set[str]:
    return {name[:-3] for name in os.listdir(doc_dir) if name.endswith(".md")}


def emit_toctree(caption: str, entries: Iterable[str]) -> str:
    joined = "\n".join(entries)
    return (
        "```{toctree}\n"
        ":maxdepth: 2\n"
        f":caption: {caption}\n\n"
        f"{joined}\n"
        "```\n"
    )


def build_index(doc_dir: str, title: str) -> str:
    docs = existing_docs(doc_dir)
    blocks: List[str] = [
        f"# {title}",
        "",
        "<!-- maintained-by: human+ai -->",
        "",
        "This directory is the Project Knowledge Base for the repository.",
        "",
    ]

    for caption, ordered_entries in SECTIONS:
        present = [entry for entry in ordered_entries if entry in docs]
        if present:
            blocks.append(emit_toctree(caption, present))

    appendix = sorted(name for name in docs if name.startswith("appendix-"))
    appendix_entries: List[str] = appendix[:]
    if "ai-guide" in docs:
        appendix_entries.append("ai-guide")
    if os.path.isfile(os.path.join(doc_dir, "adr", "index.md")):
        appendix_entries.append("adr/index")
    if os.path.isfile(os.path.join(doc_dir, "changes", "index.md")):
        appendix_entries.append("changes/index")
    if appendix_entries:
        blocks.append(emit_toctree("Appendix", appendix_entries))

    blocks.extend(
        [
            "---",
            "<!-- PKB-metadata",
            f"last_updated: {date.today().isoformat()}",
            f"commit: {git_head_short(doc_dir)}",
            "updated_by: human+ai",
            "-->",
            "",
        ]
    )
    return "\n".join(blocks)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a PKB index.md candidate")
    parser.add_argument("--doc-dir", default="doc", help="PKB doc directory")
    parser.add_argument("--title", help="Optional explicit index title")
    parser.add_argument("--stdout", action="store_true", help="Print raw generated index candidate")
    parser.add_argument("--output-file", help="Optional sidecar artifact file")
    args = parser.parse_args()

    doc_dir = os.path.abspath(args.doc_dir)
    if not os.path.isdir(doc_dir):
        raise SystemExit(f"Doc directory not found: {doc_dir}")

    content = build_index(doc_dir, detect_title(doc_dir, args.title))
    if args.stdout:
        print(content)
        return

    target = args.output_file or generated_output_path(doc_dir, "index.generated.md")
    artifact = render_markdown_artifact(
        script_name=script_id(__file__),
        title="Index Regeneration Candidate",
        provenance=[os.path.relpath(doc_dir, os.getcwd()) if not os.path.isabs(args.doc_dir) else doc_dir],
        body="\n".join(
            [
                "## Proposed `index.md`",
                "",
                "```md",
                content.rstrip(),
                "```",
            ]
        ),
        needs_input=[
            "confirm the reader-facing project title if the repo slug is not the intended display name",
            "review whether any current toctree ordering intentionally deviates from the standard numbered layout",
        ],
    )
    write_text_file(target, artifact)
    print(f"Wrote {target}")


if __name__ == "__main__":
    main()
