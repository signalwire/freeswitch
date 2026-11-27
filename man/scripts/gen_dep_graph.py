#!/usr/bin/env python3
"""
Generate a local dependency graph in Mermaid or JSON format.

This script uses lightweight static analysis and groups dependencies by path
prefix so the output stays readable for PKB docs.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from collections import Counter
from typing import Dict, Iterable, List, Optional, Set, Tuple

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

SOURCE_EXTENSIONS = {".js", ".jsx", ".ts", ".tsx", ".py", ".rs", ".go"}
JS_IMPORT_RE = re.compile(
    r"""(?:
        import\s+(?:.+?\s+from\s+)?["']([^"']+)["']|
        export\s+.+?\s+from\s+["']([^"']+)["']|
        require\(\s*["']([^"']+)["']\s*\)
    )""",
    re.VERBOSE,
)
PY_RELATIVE_IMPORT_RE = re.compile(r"^\s*from\s+(\.+[A-Za-z0-9_\.]*)\s+import\s+(.+)$", re.MULTILINE)
RUST_MOD_RE = re.compile(r"^\s*mod\s+([A-Za-z_]\w*)\s*;", re.MULTILINE)
RUST_CRATE_USE_RE = re.compile(r"^\s*use\s+crate::([A-Za-z0-9_:]+)", re.MULTILINE)
GO_IMPORT_BLOCK_RE = re.compile(r"import\s*\((.*?)\)", re.DOTALL)
GO_IMPORT_LINE_RE = re.compile(r'^\s*"([^"]+)"\s*$', re.MULTILINE)
GO_SINGLE_IMPORT_RE = re.compile(r'import\s+"([^"]+)"')


def iter_source_files(repo_root: str) -> Iterable[str]:
    for current_root, dirnames, filenames in os.walk(repo_root):
        dirnames[:] = [name for name in dirnames if name not in IGNORED_DIRS]
        for filename in filenames:
            if os.path.splitext(filename)[1] in SOURCE_EXTENSIONS:
                yield os.path.join(current_root, filename)


def collect_known_files(repo_root: str) -> Set[str]:
    return {os.path.abspath(path) for path in iter_source_files(repo_root)}


def resolve_js_import(current_file: str, spec: str, known_files: Set[str]) -> Optional[str]:
    if not spec.startswith("."):
        return None
    base = os.path.abspath(os.path.join(os.path.dirname(current_file), spec))
    candidates = [
        base,
        f"{base}.ts",
        f"{base}.tsx",
        f"{base}.js",
        f"{base}.jsx",
        os.path.join(base, "index.ts"),
        os.path.join(base, "index.tsx"),
        os.path.join(base, "index.js"),
        os.path.join(base, "index.jsx"),
    ]
    for candidate in candidates:
        if candidate in known_files:
            return candidate
    return None


def resolve_python_relative(current_file: str, dotted: str, known_files: Set[str]) -> Optional[str]:
    dots = len(dotted) - len(dotted.lstrip("."))
    module_path = dotted[dots:]
    base_dir = os.path.dirname(current_file)
    for _ in range(max(0, dots - 1)):
        base_dir = os.path.dirname(base_dir)
    target = base_dir
    if module_path:
        target = os.path.join(target, *module_path.split("."))
    candidates = [
        f"{target}.py",
        os.path.join(target, "__init__.py"),
    ]
    for candidate in candidates:
        if os.path.abspath(candidate) in known_files:
            return os.path.abspath(candidate)
    return None


def find_cargo_src_root(current_file: str) -> Optional[str]:
    directory = os.path.dirname(os.path.abspath(current_file))
    while True:
        cargo_toml = os.path.join(directory, "Cargo.toml")
        src_dir = os.path.join(directory, "src")
        if os.path.isfile(cargo_toml) and os.path.isdir(src_dir):
            return src_dir
        parent = os.path.dirname(directory)
        if parent == directory:
            return None
        directory = parent


def resolve_rust_crate_use(current_file: str, path_spec: str, known_files: Set[str]) -> Optional[str]:
    src_root = find_cargo_src_root(current_file)
    if not src_root:
        return None
    parts = [part for part in path_spec.split("::") if part]
    if not parts:
        return None
    current = os.path.join(src_root, *parts)
    candidates = [
        f"{current}.rs",
        os.path.join(current, "mod.rs"),
    ]
    for candidate in candidates:
        if os.path.abspath(candidate) in known_files:
            return os.path.abspath(candidate)
    # Fallback to the directory/module prefix if exact symbol path does not resolve.
    while parts:
        parts.pop()
        if not parts:
            break
        current = os.path.join(src_root, *parts)
        candidates = [f"{current}.rs", os.path.join(current, "mod.rs")]
        for candidate in candidates:
            if os.path.abspath(candidate) in known_files:
                return os.path.abspath(candidate)
    return None


def extract_js_edges(current_file: str, known_files: Set[str]) -> Set[str]:
    with open(current_file, "r", encoding="utf-8", errors="ignore") as handle:
        text = handle.read()
    deps = set()
    for match in JS_IMPORT_RE.findall(text):
        spec = next((value for value in match if value), "")
        target = resolve_js_import(current_file, spec, known_files)
        if target:
            deps.add(target)
    return deps


def extract_python_edges(current_file: str, known_files: Set[str]) -> Set[str]:
    with open(current_file, "r", encoding="utf-8", errors="ignore") as handle:
        text = handle.read()
    deps = set()
    for dotted, _imported in PY_RELATIVE_IMPORT_RE.findall(text):
        target = resolve_python_relative(current_file, dotted, known_files)
        if target:
            deps.add(target)
    return deps


def extract_rust_edges(current_file: str, known_files: Set[str]) -> Set[str]:
    with open(current_file, "r", encoding="utf-8", errors="ignore") as handle:
        text = handle.read()
    deps = set()
    current_dir = os.path.dirname(current_file)

    for name in RUST_MOD_RE.findall(text):
        for candidate in (os.path.join(current_dir, f"{name}.rs"), os.path.join(current_dir, name, "mod.rs")):
            if os.path.abspath(candidate) in known_files:
                deps.add(os.path.abspath(candidate))

    for path_spec in RUST_CRATE_USE_RE.findall(text):
        target = resolve_rust_crate_use(current_file, path_spec, known_files)
        if target:
            deps.add(target)
    return deps


def read_go_module(repo_root: str) -> Optional[str]:
    go_mod = os.path.join(repo_root, "go.mod")
    if not os.path.isfile(go_mod):
        return None
    with open(go_mod, "r", encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("module "):
                return line.split(maxsplit=1)[1].strip()
    return None


def extract_go_edges(current_file: str, repo_root: str, go_module: Optional[str]) -> Set[str]:
    if not go_module:
        return set()

    with open(current_file, "r", encoding="utf-8", errors="ignore") as handle:
        text = handle.read()

    imports = set(GO_SINGLE_IMPORT_RE.findall(text))
    for block in GO_IMPORT_BLOCK_RE.findall(text):
        imports.update(GO_IMPORT_LINE_RE.findall(block))

    deps = set()
    for imp in imports:
        if not imp.startswith(go_module):
            continue
        suffix = imp[len(go_module):].lstrip("/")
        if not suffix:
            continue
        target = os.path.abspath(os.path.join(repo_root, suffix))
        deps.add(target)
    return deps


def group_label(rel_path: str, depth: int) -> str:
    normalized = rel_path.replace(os.sep, "/")
    parts = [part for part in normalized.split("/") if part]
    if not parts:
        return normalized
    if "." in parts[-1]:
        parts = parts[:-1] or [os.path.splitext(normalized)[0]]
    if len(parts) <= depth:
        return "/".join(parts)
    return "/".join(parts[:depth])


def collect_edges(repo_root: str, depth: int) -> Counter[Tuple[str, str]]:
    repo_root = os.path.abspath(repo_root)
    known_files = collect_known_files(repo_root)
    go_module = read_go_module(repo_root)
    edge_counter: Counter[Tuple[str, str]] = Counter()

    for current_file in known_files:
        ext = os.path.splitext(current_file)[1]
        deps: Set[str] = set()
        if ext in {".js", ".jsx", ".ts", ".tsx"}:
            deps = extract_js_edges(current_file, known_files)
        elif ext == ".py":
            deps = extract_python_edges(current_file, known_files)
        elif ext == ".rs":
            deps = extract_rust_edges(current_file, known_files)
        elif ext == ".go":
            deps = extract_go_edges(current_file, repo_root, go_module)

        source_label = group_label(os.path.relpath(current_file, repo_root), depth)
        for dep in deps:
            dep_label = group_label(os.path.relpath(dep, repo_root), depth)
            if dep_label and source_label and dep_label != source_label:
                edge_counter[(source_label, dep_label)] += 1

    return edge_counter


def make_node_id(index: int) -> str:
    return f"node_{index}"


def render_mermaid(edges: Counter[Tuple[str, str]], max_edges: int) -> str:
    if not edges:
        return "graph LR\n    empty[\"No local dependencies detected\"]\n"

    most_common = edges.most_common(max_edges)
    labels = sorted({src for (src, _), _count in most_common} | {dst for (_src, dst), _count in most_common})
    node_ids = {label: make_node_id(index) for index, label in enumerate(labels, start=1)}

    lines = ["graph LR"]
    for label in labels:
        lines.append(f'    {node_ids[label]}["{label}"]')
    for (src, dst), count in most_common:
        label = f"|{count}|" if count > 1 else ""
        lines.append(f"    {node_ids[src]} -->{label} {node_ids[dst]}")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a PKB dependency graph artifact")
    parser.add_argument("--repo-root", default=".", help="Repository root")
    parser.add_argument("--doc-dir", help="PKB doc directory for default sidecar output")
    parser.add_argument("--group-depth", type=int, default=2, help="How many path segments to keep when grouping nodes")
    parser.add_argument("--max-edges", type=int, default=80, help="Maximum edges in the rendered graph")
    parser.add_argument("--format", choices=("mermaid", "json"), default="mermaid")
    parser.add_argument("--output-file", help="Optional output file")
    args = parser.parse_args()

    repo_root = os.path.abspath(args.repo_root)
    edges = collect_edges(repo_root, args.group_depth)
    if args.format == "json":
        payload = [
            {"source": src, "target": dst, "count": count}
            for (src, dst), count in edges.most_common(args.max_edges)
        ]
        rendered = json.dumps(payload, indent=2)
    else:
        payload = None
        rendered = render_mermaid(edges, args.max_edges)

    target = args.output_file
    if not target and args.doc_dir:
        suffix = "02-architecture.dep-graph.generated.md" if args.format == "mermaid" else "02-architecture.dep-graph.generated.json"
        target = generated_output_path(args.doc_dir, suffix)

    if not target:
        print(rendered)
        return

    provenance = [
        os.path.relpath(repo_root, os.getcwd()) if not os.path.isabs(args.repo_root) else repo_root,
        f"group-depth={args.group_depth}",
        f"max-edges={args.max_edges}",
    ]
    needs_input = [
        "explain which high-weight edges matter architecturally and whether any represent undesirable coupling",
        "decide whether omitted external services or generated code should be documented separately",
    ]
    if args.format == "mermaid":
        artifact = render_markdown_artifact(
            script_name=script_id(__file__),
            title="Dependency Graph Snapshot",
            provenance=provenance,
            body="\n".join(["## Mermaid Graph", "", "```mermaid", rendered.rstrip(), "```"]),
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
