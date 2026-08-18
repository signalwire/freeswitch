#!/usr/bin/env python3
"""Generate a simple bilingual landing page for deployed docs."""

from __future__ import annotations

import argparse
from pathlib import Path


def build_html(default_language: str) -> str:
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Project Documentation</title>
  <style>
    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      background: #f8fafc;
      color: #1f2933;
    }}
    main {{
      max-width: 640px;
      padding: 2rem;
      border-radius: 12px;
      background: white;
      box-shadow: 0 12px 40px rgba(15, 23, 42, 0.08);
      text-align: center;
    }}
    .links {{
      display: flex;
      gap: 1rem;
      justify-content: center;
      margin-top: 1.5rem;
      flex-wrap: wrap;
    }}
    a {{
      padding: 0.75rem 1rem;
      border-radius: 999px;
      text-decoration: none;
      font-weight: 600;
      color: #8d5b00;
      background: #fff7d6;
      border: 1px solid #f5c542;
    }}
  </style>
  <script>
    const prefersChinese = navigator.language && navigator.language.toLowerCase().startsWith("zh");
    const target = prefersChinese ? "./zh/" : "./{default_language}/";
    window.location.replace(target);
  </script>
</head>
<body>
  <main>
    <h1>Project Documentation</h1>
    <p>Select a language if automatic redirection does not happen.</p>
    <div class="links">
      <a href="./en/">English</a>
      <a href="./zh/">中文</a>
    </div>
  </main>
</body>
</html>
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site-dir", required=True, help="Directory to write landing page into")
    parser.add_argument("--default-language", default="en")
    args = parser.parse_args()

    site_dir = Path(args.site_dir)
    site_dir.mkdir(parents=True, exist_ok=True)
    (site_dir / "index.html").write_text(
        build_html(default_language=args.default_language),
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
