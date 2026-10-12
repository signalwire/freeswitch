#!/usr/bin/env python3
"""
PKB Bulk PO Translator — apply dictionary-based translations to .po files.

Uses the `polib` library to safely parse and modify .po files, handling
multi-line msgid/msgstr entries, metadata headers, and fuzzy flags correctly.

Usage:
    python3 scripts/translate_po.py [--po-dir locale/zh_CN/LC_MESSAGES] [--stats]

Workflow:
    1. Define translation dictionaries in TRANSLATIONS below (msgid -> msgstr).
    2. Run this script to apply translations to all matching .po files.
    3. Run `make intl-build` to compile .mo files.
    4. Run `make html-zh` to rebuild Chinese HTML.

To add translations for a new file, add a new entry to TRANSLATIONS:
    TRANSLATIONS['filename-without-ext'] = {
        "English msgid text": "Chinese msgstr text",
        ...
    }
"""

from __future__ import annotations

import argparse
import glob
import os
import sys

try:
    import polib
except ImportError:
    print("polib not found. Install it: pip install polib (or poetry add --group dev polib)")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Translation dictionaries: one dict per .po file (keyed by filename stem).
# The key is the exact msgid text; the value is the msgstr Chinese translation.
# Multi-line msgid entries are represented as a single joined string (polib
# handles the .po wire format transparently).
# ---------------------------------------------------------------------------

TRANSLATIONS: dict[str, dict[str, str]] = {
    # Example:
    # '00-overview': {
    #     "Project Overview": "项目概述",
    #     "**Call CSMS HTTP API directly** — get secrets...":
    #         "**直接调用 CSMS HTTP API** — 从服务器获取密钥...",
    # },
}


def apply_translations(po_dir: str) -> int:
    """Apply all dictionary translations to .po files. Returns count of filled entries."""
    total_filled = 0
    for name, trans_dict in TRANSLATIONS.items():
        po_path = os.path.join(po_dir, f"{name}.po")
        if not os.path.exists(po_path):
            print(f"  SKIP {name}.po (not found)")
            continue
        po = polib.pofile(po_path)
        filled = 0
        for entry in po.untranslated_entries():
            if entry.msgid in trans_dict:
                entry.msgstr = trans_dict[entry.msgid]
                filled += 1
        if filled > 0:
            po.save()
            print(f"  {name}.po: +{filled} translations applied")
            total_filled += filled
    return total_filled


def show_stats(po_dir: str) -> None:
    """Print translation coverage for all .po files."""
    print("\nTranslation coverage:")
    overall_translated = 0
    overall_total = 0
    for po_path in sorted(glob.glob(os.path.join(po_dir, "*.po"))):
        po = polib.pofile(po_path)
        total = len([e for e in po if not e.obsolete])
        translated = len(po.translated_entries())
        untrans = len(po.untranslated_entries())
        pct = po.percent_translated()
        name = os.path.basename(po_path)
        status = "DONE" if pct == 100 else f"{untrans} empty"
        print(f"  {name:40s} {pct:5.1f}%  ({status})")
        overall_translated += translated
        overall_total += total
    if overall_total > 0:
        overall_pct = overall_translated / overall_total * 100
        print(f"\n  Overall: {overall_translated}/{overall_total} ({overall_pct:.1f}%)")


def main() -> None:
    parser = argparse.ArgumentParser(description="Bulk-translate .po files using dictionary mappings")
    parser.add_argument(
        "--po-dir",
        default="locale/zh_CN/LC_MESSAGES",
        help="Path to .po files directory (default: locale/zh_CN/LC_MESSAGES)",
    )
    parser.add_argument(
        "--stats",
        action="store_true",
        help="Only show translation statistics without applying changes",
    )
    args = parser.parse_args()

    if not os.path.isdir(args.po_dir):
        print(f"ERROR: Directory not found: {args.po_dir}")
        sys.exit(1)

    if args.stats:
        show_stats(args.po_dir)
        return

    if not TRANSLATIONS:
        print("No translations defined in TRANSLATIONS dictionary.")
        print("Edit this script to add translation mappings, then re-run.")
        print("\nShowing current stats instead:\n")
        show_stats(args.po_dir)
        return

    filled = apply_translations(args.po_dir)
    print(f"\nTotal: +{filled} translations applied")
    show_stats(args.po_dir)


if __name__ == "__main__":
    main()
