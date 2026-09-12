#!/usr/bin/env python3
"""Copy identity msgids (code/URLs) into msgstr, dump remaining as JSON."""
from __future__ import annotations

import json
import os
import re
import sys

import polib

PO_ROOT = os.path.join(os.path.dirname(__file__), "..", "locale", "zh_CN", "LC_MESSAGES")
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "_generated", "i18n")

IDENTITY_RE = [
    re.compile(r"^https?://\S+$"),
    re.compile(r"^`[^`]+`$"),
    re.compile(r"^v?\d+\.\d+[\w.+-]*$"),
    re.compile(r"^[\w./+-]+\.(c|h|md|xml|yml|yaml|sh|ac|am|in|txt|conf|po|py|ts)$", re.I),
    re.compile(r"^\$[A-Z_][A-Z0-9_]*$"),
    re.compile(r"^/[A-Za-z0-9_./-]+$"),
]


def is_identity(msgid: str) -> bool:
    s = msgid.strip()
    if not s:
        return True
    if s in {":", "|", "—", "-", "–", "...", "…"}:
        return True
    for rx in IDENTITY_RE:
        if rx.match(s):
            return True
    return False


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    copied = remaining = 0
    for dirpath, _, files in os.walk(PO_ROOT):
        for name in files:
            if not name.endswith(".po"):
                continue
            path = os.path.join(dirpath, name)
            po = polib.pofile(path)
            leftover = []
            changed = False
            for entry in po:
                if entry.obsolete or not entry.msgid:
                    continue
                if entry.msgstr:
                    continue
                if is_identity(entry.msgid):
                    entry.msgstr = entry.msgid
                    copied += 1
                    changed = True
                else:
                    leftover.append(entry.msgid)
                    remaining += 1
            if changed:
                po.save()
            rel = os.path.relpath(path, PO_ROOT)
            stem = rel[:-3].replace(os.sep, "__")
            out = os.path.join(OUT_DIR, f"{stem}.json")
            with open(out, "w", encoding="utf-8") as fh:
                json.dump({"po": rel, "entries": leftover}, fh, ensure_ascii=False, indent=2)
            print(f"{rel}: {len(leftover)} remaining")
    print(f"identity copied={copied} remaining={remaining}")


if __name__ == "__main__":
    sys.exit(main())
