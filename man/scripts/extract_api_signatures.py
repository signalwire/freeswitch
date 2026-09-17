#!/usr/bin/env python3
"""
Extract API signatures from common Tauri and REST patterns.

This is heuristic by design: it prefers deterministic extraction of obvious
signatures over deep framework-specific parsing.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import asdict, dataclass
from typing import Iterable, List

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from generated_output import (
    generated_output_path,
    render_json_artifact,
    render_markdown_artifact,
    script_id,
    write_text_file,
)

IGNORED_DIRS = {
    ".git",
    "node_modules",
    "dist",
    "build",
    "target",
    "__pycache__",
    ".venv",
    "venv",
}

SOURCE_EXTENSIONS = {".rs", ".ts", ".tsx", ".js", ".jsx", ".py", ".go"}
HTTP_METHODS = {"get", "post", "put", "patch", "delete", "options", "head"}

TAURI_ATTR_RE = re.compile(r"#\[\s*(?:tauri::)?command\b")
RUST_FN_RE = re.compile(r"(?:pub\s+)?(?:async\s+)?fn\s+([A-Za-z_]\w*)\s*\(")
JS_ROUTE_RE = re.compile(
    r"\b(?:router|app|server|api)\.(get|post|put|patch|delete|options|head)\(\s*['\"]([^'\"]+)['\"]"
)
PY_DECORATOR_RE = re.compile(r"@\s*(?:app|router|bp|api)\.(get|post|put|patch|delete|options|head)\(\s*['\"]([^'\"]+)['\"]")
PY_DEF_RE = re.compile(r"(?:async\s+def|def)\s+([A-Za-z_]\w*)\s*\(")
GO_ROUTE_RE = re.compile(r"\b[A-Za-z_]\w*\.(GET|POST|PUT|PATCH|DELETE|OPTIONS|HEAD)\(\s*\"([^\"]+)\"")


@dataclass
class Signature:
    kind: str
    method: str
    target: str
    name: str
    signature: str
    file: str
    line: int


def iter_source_files(repo_root: str) -> Iterable[str]:
    for current_root, dirnames, filenames in os.walk(repo_root):
        dirnames[:] = [name for name in dirnames if name not in IGNORED_DIRS]
        for filename in filenames:
            if os.path.splitext(filename)[1] in SOURCE_EXTENSIONS:
                yield os.path.join(current_root, filename)


def read_lines(path: str) -> List[str]:
    with open(path, "r", encoding="utf-8", errors="ignore") as handle:
        return handle.readlines()


def extract_tauri_signatures(path: str, repo_root: str) -> List[Signature]:
    lines = read_lines(path)
    results: List[Signature] = []
    for index, line in enumerate(lines):
        if not TAURI_ATTR_RE.search(line):
            continue
        signature_lines = []
        for next_index in range(index + 1, min(index + 8, len(lines))):
            candidate = lines[next_index].strip()
            if not candidate:
                continue
            signature_lines.append(candidate)
            if ")" in candidate or "{" in candidate:
                break
        combined = " ".join(signature_lines)
        fn_match = re.search(r"(?:pub\s+)?(?:async\s+)?fn\s+([A-Za-z_]\w*)\s*\(", combined)
        if fn_match:
            name = fn_match.group(1)
            results.append(
                Signature(
                    kind="tauri",
                    method="COMMAND",
                    target=name,
                    name=name,
                    signature=combined,
                    file=os.path.relpath(path, repo_root),
                    line=index + 1,
                )
            )
    return results


def extract_js_routes(path: str, repo_root: str) -> List[Signature]:
    results: List[Signature] = []
    for index, line in enumerate(read_lines(path), start=1):
        match = JS_ROUTE_RE.search(line)
        if not match:
            continue
        method = match.group(1).upper()
        route = match.group(2)
        results.append(
            Signature(
                kind="rest",
                method=method,
                target=route,
                name=f"{method} {route}",
                signature=line.strip(),
                file=os.path.relpath(path, repo_root),
                line=index,
            )
        )
    return results


def extract_python_routes(path: str, repo_root: str) -> List[Signature]:
    lines = read_lines(path)
    results: List[Signature] = []
    pending = None
    for index, line in enumerate(lines, start=1):
        decorator = PY_DECORATOR_RE.search(line)
        if decorator:
            pending = (decorator.group(1).upper(), decorator.group(2), index)
            continue
        if pending is None:
            continue
        def_match = PY_DEF_RE.search(line)
        if def_match:
            method, route, decorator_line = pending
            results.append(
                Signature(
                    kind="rest",
                    method=method,
                    target=route,
                    name=def_match.group(1),
                    signature=line.strip(),
                    file=os.path.relpath(path, repo_root),
                    line=decorator_line,
                )
            )
            pending = None
        elif line.strip() and not line.lstrip().startswith("@"):
            pending = None
    return results


def extract_go_routes(path: str, repo_root: str) -> List[Signature]:
    results: List[Signature] = []
    for index, line in enumerate(read_lines(path), start=1):
        match = GO_ROUTE_RE.search(line)
        if not match:
            continue
        method = match.group(1).upper()
        route = match.group(2)
        results.append(
            Signature(
                kind="rest",
                method=method,
                target=route,
                name=f"{method} {route}",
                signature=line.strip(),
                file=os.path.relpath(path, repo_root),
                line=index,
            )
        )
    return results


def collect_signatures(repo_root: str) -> List[Signature]:
    signatures: List[Signature] = []
    for path in iter_source_files(repo_root):
        ext = os.path.splitext(path)[1]
        if ext == ".rs":
            signatures.extend(extract_tauri_signatures(path, repo_root))
        elif ext in {".ts", ".tsx", ".js", ".jsx"}:
            signatures.extend(extract_js_routes(path, repo_root))
        elif ext == ".py":
            signatures.extend(extract_python_routes(path, repo_root))
        elif ext == ".go":
            signatures.extend(extract_go_routes(path, repo_root))
    return sorted(signatures, key=lambda item: (item.kind, item.file, item.line))


def render_markdown(signatures: List[Signature]) -> str:
    tauri = [item for item in signatures if item.kind == "tauri"]
    rest = [item for item in signatures if item.kind == "rest"]
    lines = ["# API Signatures", ""]

    if tauri:
        lines.extend(["## Tauri Commands", "", "| Name | Signature | File |", "|------|-----------|------|"])
        for item in tauri:
            lines.append(
                f"| `{item.name}` | `{item.signature}` | `{item.file}:{item.line}` |"
            )
        lines.append("")

    if rest:
        lines.extend(["## REST Endpoints", "", "| Method | Route | Signature | File |", "|--------|-------|-----------|------|"])
        for item in rest:
            lines.append(
                f"| `{item.method}` | `{item.target}` | `{item.signature}` | `{item.file}:{item.line}` |"
            )
        lines.append("")

    if not signatures:
        lines.append("_No supported Tauri commands or REST routes were found._")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract API signatures for PKB sidecar artifacts")
    parser.add_argument("--repo-root", default=".", help="Repository root")
    parser.add_argument("--doc-dir", help="PKB doc directory for default sidecar output")
    parser.add_argument("--format", choices=("markdown", "json"), default="markdown")
    parser.add_argument("--output-file", help="Optional output file")
    args = parser.parse_args()

    repo_root = os.path.abspath(args.repo_root)
    signatures = collect_signatures(repo_root)
    payload = [asdict(item) for item in signatures]
    rendered = render_markdown(signatures) if args.format == "markdown" else json.dumps(payload, indent=2)

    target = args.output_file
    if not target and args.doc_dir:
        suffix = "05-data-and-api.signatures.generated.md" if args.format == "markdown" else "05-data-and-api.signatures.generated.json"
        target = generated_output_path(args.doc_dir, suffix)

    if not target:
        print(rendered)
        return

    provenance = sorted({item["file"] for item in payload})
    needs_input = [
        "confirm which extracted signatures are user-facing contracts versus internal-only helpers",
        "add missing framework-specific routes or command semantics that the heuristic extractor cannot infer",
    ]
    if args.format == "markdown":
        body = rendered
        if body.startswith("# "):
            lines = body.splitlines()
            body = "\n".join(["## Extracted Signatures", "", *lines[2:]])
        artifact = render_markdown_artifact(
            script_name=script_id(__file__),
            title="API Signature Inventory",
            provenance=provenance,
            body=body,
            needs_input=needs_input,
        )
    else:
        artifact = render_json_artifact(
            script_name=script_id(__file__),
            payload=payload,
            provenance=provenance,
            needs_input=needs_input,
        )
    write_text_file(target, artifact)
    print(f"Wrote {target}")


if __name__ == "__main__":
    main()
