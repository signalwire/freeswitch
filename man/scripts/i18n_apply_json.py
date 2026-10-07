#!/usr/bin/env python3
"""Apply a JSON msgid->msgstr map onto a .po file via polib."""
from __future__ import annotations

import argparse
import json
import os
import sys

import polib

PO_ROOT = os.path.join(os.path.dirname(__file__), "..", "locale", "zh_CN", "LC_MESSAGES")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("json_path")
    parser.add_argument("--po", help="Relative path under locale/zh_CN/LC_MESSAGES")
    args = parser.parse_args()
    with open(args.json_path, encoding="utf-8") as fh:
        data = json.load(fh)
    if isinstance(data, dict) and "translations" in data:
        mapping = data["translations"]
        po_rel = data.get("po") or args.po
    elif isinstance(data, dict) and "entries" in data and isinstance(data["entries"], list):
        mapping = {}
        for item in data["entries"]:
            if isinstance(item, dict) and item.get("msgstr"):
                mapping[item["msgid"]] = item["msgstr"]
        po_rel = data.get("po") or args.po
    else:
        mapping = data
        po_rel = args.po
    if not po_rel:
        print("ERROR: missing --po / json.po", file=sys.stderr)
        return 1
    po_path = os.path.join(PO_ROOT, po_rel)
    po = polib.pofile(po_path)
    filled = 0
    missing = 0
    for entry in po.untranslated_entries():
        if entry.msgid in mapping and mapping[entry.msgid]:
            entry.msgstr = mapping[entry.msgid]
            filled += 1
        else:
            missing += 1
    po.save()
    print(f"{po_rel}: +{filled} filled, {missing} still empty")
    return 0 if missing == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
