#!/usr/bin/env python3
"""
Generate a deterministic tech-stack inventory from common project manifests.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import xml.etree.ElementTree as ET
import sys
from dataclasses import dataclass
from typing import Dict, Iterable, List

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from generated_output import (
    generated_output_path,
    render_json_artifact,
    render_markdown_artifact,
    script_id,
    write_text_file,
)

try:
    import tomllib  # Python 3.11+
except ModuleNotFoundError:  # pragma: no cover - Python 3.10 fallback
    tomllib = None


IGNORED_DIRS = {
    ".git",
    ".hg",
    ".svn",
    "node_modules",
    "dist",
    "build",
    "target",
    "__pycache__",
    ".venv",
    "venv",
}

MANIFEST_FILES = {
    "package.json",
    "Cargo.toml",
    "go.mod",
    "pyproject.toml",
    "pom.xml",
}


@dataclass
class TechEntry:
    name: str
    version: str
    scope: str
    source: str


def iter_manifest_paths(repo_root: str) -> Iterable[str]:
    for current_root, dirnames, filenames in os.walk(repo_root):
        dirnames[:] = [name for name in dirnames if name not in IGNORED_DIRS]
        for filename in filenames:
            if filename in MANIFEST_FILES or (
                filename.startswith("requirements") and filename.endswith(".txt")
            ):
                yield os.path.join(current_root, filename)


def relpath(path: str, repo_root: str) -> str:
    return os.path.relpath(path, repo_root)


def parse_package_json(path: str, repo_root: str) -> List[TechEntry]:
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    entries = [
        TechEntry(data.get("name", "(package)"), data.get("version", "(unknown)"), "package", relpath(path, repo_root))
    ]
    if data.get("packageManager"):
        entries.append(TechEntry("packageManager", str(data["packageManager"]), "tooling", relpath(path, repo_root)))
    for engine, version in sorted(data.get("engines", {}).items()):
        entries.append(TechEntry(f"engine:{engine}", str(version), "runtime", relpath(path, repo_root)))
    for scope_name in ("dependencies", "devDependencies", "peerDependencies", "optionalDependencies"):
        for name, version in sorted(data.get(scope_name, {}).items()):
            entries.append(TechEntry(name, str(version), scope_name, relpath(path, repo_root)))
    return entries


def extract_toml_version(raw_value: str) -> str:
    raw_value = raw_value.strip()
    quoted = re.search(r'"([^"]+)"', raw_value)
    if quoted:
        return quoted.group(1)
    version_field = re.search(r"version\s*=\s*\"([^\"]+)\"", raw_value)
    if version_field:
        return version_field.group(1)
    return raw_value or "(unknown)"


def parse_cargo_toml(path: str, repo_root: str) -> List[TechEntry]:
    if tomllib:
        with open(path, "rb") as handle:
            data = tomllib.load(handle)
        entries = [
            TechEntry(data.get("package", {}).get("name", "(crate)"), data.get("package", {}).get("version", "(unknown)"), "package", relpath(path, repo_root))
        ]
        for table_name in ("dependencies", "dev-dependencies", "build-dependencies"):
            for name, version in sorted(data.get(table_name, {}).items()):
                if isinstance(version, dict):
                    version_value = str(version.get("version", "(workspace/local)"))
                else:
                    version_value = str(version)
                entries.append(TechEntry(name, version_value, table_name, relpath(path, repo_root)))
        return entries

    entries: List[TechEntry] = []
    current_table = ""
    package_name = "(crate)"
    package_version = "(unknown)"
    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.split("#", 1)[0].strip()
            if not line:
                continue
            table_match = re.match(r"\[([^\]]+)\]", line)
            if table_match:
                current_table = table_match.group(1)
                continue
            if "=" not in line:
                continue
            key, value = [part.strip() for part in line.split("=", 1)]
            if current_table == "package":
                if key == "name":
                    package_name = extract_toml_version(value)
                elif key == "version":
                    package_version = extract_toml_version(value)
            elif current_table in {"dependencies", "dev-dependencies", "build-dependencies"}:
                entries.append(
                    TechEntry(key, extract_toml_version(value), current_table, relpath(path, repo_root))
                )
    return [TechEntry(package_name, package_version, "package", relpath(path, repo_root)), *entries]


def parse_go_mod(path: str, repo_root: str) -> List[TechEntry]:
    entries: List[TechEntry] = []
    source = relpath(path, repo_root)
    in_require_block = False

    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.split("//", 1)[0].strip()
            if not line:
                continue
            if line.startswith("module "):
                entries.append(TechEntry(line.split(maxsplit=1)[1], "(module)", "module", source))
                continue
            if line.startswith("go "):
                entries.append(TechEntry("go", line.split(maxsplit=1)[1], "runtime", source))
                continue
            if line == "require (":
                in_require_block = True
                continue
            if in_require_block and line == ")":
                in_require_block = False
                continue
            if line.startswith("require "):
                dep = line.split(maxsplit=1)[1]
                parts = dep.split()
                if len(parts) >= 2:
                    entries.append(TechEntry(parts[0], parts[1], "require", source))
                continue
            if in_require_block:
                parts = line.split()
                if len(parts) >= 2:
                    entries.append(TechEntry(parts[0], parts[1], "require", source))
    return entries


def parse_requirement_line(line: str) -> tuple[str, str]:
    clean = line.strip()
    match = re.match(r"([A-Za-z0-9_.-]+)\s*([<>=!~].+)?", clean)
    if not match:
        return clean, "(unknown)"
    name = match.group(1)
    version = match.group(2).strip() if match.group(2) else "(unpinned)"
    return name, version


def parse_requirements(path: str, repo_root: str) -> List[TechEntry]:
    entries: List[TechEntry] = []
    source = relpath(path, repo_root)
    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#") or line.startswith("-r "):
                continue
            name, version = parse_requirement_line(line)
            entries.append(TechEntry(name, version, "requirements", source))
    return entries


def parse_pyproject(path: str, repo_root: str) -> List[TechEntry]:
    source = relpath(path, repo_root)
    entries: List[TechEntry] = []

    if tomllib:
        with open(path, "rb") as handle:
            data = tomllib.load(handle)

        project = data.get("project", {})
        tool_poetry = data.get("tool", {}).get("poetry", {})
        name = project.get("name") or tool_poetry.get("name") or "(python-project)"
        version = project.get("version") or tool_poetry.get("version") or "(unknown)"
        entries.append(TechEntry(str(name), str(version), "package", source))

        requires_python = project.get("requires-python")
        if requires_python:
            entries.append(TechEntry("python", str(requires_python), "runtime", source))

        for dep in project.get("dependencies", []):
            dep_name, dep_version = parse_requirement_line(str(dep))
            entries.append(TechEntry(dep_name, dep_version, "dependencies", source))

        for name, value in sorted(tool_poetry.get("dependencies", {}).items()):
            if name == "python":
                entries.append(TechEntry("python", str(value), "runtime", source))
            else:
                entries.append(TechEntry(name, str(value), "dependencies", source))
        for name, value in sorted(tool_poetry.get("dev-dependencies", {}).items()):
            entries.append(TechEntry(name, str(value), "dev-dependencies", source))
        return entries

    with open(path, "r", encoding="utf-8") as handle:
        text = handle.read()
    name_match = re.search(r'^\s*name\s*=\s*"([^"]+)"', text, re.MULTILINE)
    version_match = re.search(r'^\s*version\s*=\s*"([^"]+)"', text, re.MULTILINE)
    entries.append(
        TechEntry(
            name_match.group(1) if name_match else "(python-project)",
            version_match.group(1) if version_match else "(unknown)",
            "package",
            source,
        )
    )
    return entries


def strip_ns(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def parse_pom_xml(path: str, repo_root: str) -> List[TechEntry]:
    source = relpath(path, repo_root)
    tree = ET.parse(path)
    root = tree.getroot()
    entries: List[TechEntry] = []

    project_name = None
    project_version = None
    for child in root:
        tag = strip_ns(child.tag)
        if tag == "artifactId" and project_name is None:
            project_name = child.text
        elif tag == "version" and project_version is None:
            project_version = child.text

    entries.append(
        TechEntry(project_name or "(maven-project)", project_version or "(unknown)", "package", source)
    )

    for dependency in root.iter():
        if strip_ns(dependency.tag) != "dependency":
            continue
        values: Dict[str, str] = {}
        for child in dependency:
            values[strip_ns(child.tag)] = (child.text or "").strip()
        group_id = values.get("groupId", "")
        artifact_id = values.get("artifactId", "(unknown)")
        version = values.get("version", "(managed)")
        scope = values.get("scope", "dependencies")
        name = f"{group_id}:{artifact_id}" if group_id else artifact_id
        entries.append(TechEntry(name, version, scope, source))
    return entries


def collect_entries(repo_root: str) -> Dict[str, List[TechEntry]]:
    sections: Dict[str, List[TechEntry]] = {}
    for path in sorted(iter_manifest_paths(repo_root)):
        filename = os.path.basename(path)
        if filename == "package.json":
            entries = parse_package_json(path, repo_root)
        elif filename == "Cargo.toml":
            entries = parse_cargo_toml(path, repo_root)
        elif filename == "go.mod":
            entries = parse_go_mod(path, repo_root)
        elif filename == "pyproject.toml":
            entries = parse_pyproject(path, repo_root)
        elif filename == "pom.xml":
            entries = parse_pom_xml(path, repo_root)
        elif filename.startswith("requirements") and filename.endswith(".txt"):
            entries = parse_requirements(path, repo_root)
        else:
            continue
        if entries:
            sections[relpath(path, repo_root)] = entries
    return sections


def render_markdown(sections: Dict[str, List[TechEntry]]) -> str:
    lines = ["# Tech Stack Inventory", ""]
    if not sections:
        lines.append("_No supported manifests found._")
        return "\n".join(lines)

    for source, entries in sections.items():
        lines.extend([f"## {source}", "", "| Name | Version | Scope |", "|------|---------|-------|"])
        for entry in sorted(entries, key=lambda item: (item.scope, item.name.lower())):
            lines.append(f"| `{entry.name}` | `{entry.version}` | `{entry.scope}` |")
        lines.append("")
    return "\n".join(lines)


def render_json(sections: Dict[str, List[TechEntry]]) -> str:
    payload = []
    for source, entries in sections.items():
        payload.append(
            {
                "source": source,
                "entries": [
                    {"name": entry.name, "version": entry.version, "scope": entry.scope}
                    for entry in entries
                ],
            }
        )
    return json.dumps(payload, indent=2)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a PKB tech-stack inventory artifact")
    parser.add_argument("--repo-root", default=".", help="Repository root")
    parser.add_argument("--doc-dir", help="PKB doc directory for default sidecar output")
    parser.add_argument("--format", choices=("markdown", "json"), default="markdown")
    parser.add_argument("--output-file", help="Optional output file")
    args = parser.parse_args()

    repo_root = os.path.abspath(args.repo_root)
    sections = collect_entries(repo_root)
    rendered = render_markdown(sections) if args.format == "markdown" else render_json(sections)

    target = args.output_file
    if not target and args.doc_dir:
        suffix = "03-tech-stack.generated.md" if args.format == "markdown" else "03-tech-stack.generated.json"
        target = generated_output_path(args.doc_dir, suffix)

    if not target:
        print(rendered)
        return

    provenance = list(sections.keys())
    needs_input = [
        "identify which dependencies are business-critical versus incidental tooling",
        "explain architectural or operational significance for unusual version constraints and local packages",
    ]
    if args.format == "markdown":
        body = rendered
        if body.startswith("# "):
            lines = body.splitlines()
            body = "\n".join(["## Extracted Inventory", "", *lines[2:]])
        artifact = render_markdown_artifact(
            script_name=script_id(__file__),
            title="Tech Stack Inventory",
            provenance=provenance,
            body=body,
            needs_input=needs_input,
        )
    else:
        artifact = render_json_artifact(
            script_name=script_id(__file__),
            payload=json.loads(rendered),
            provenance=provenance,
            needs_input=needs_input,
        )
    write_text_file(target, artifact)
    print(f"Wrote {target}")


if __name__ == "__main__":
    main()
