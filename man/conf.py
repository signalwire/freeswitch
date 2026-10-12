# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
#
# Author: Walter Fan (walterfan@ustc.etu)

import os
import sys

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information
# Project identity is filled in by init_pkb.sh --sphinx at initialization time.

project = 'FreeSWITCH'
copyright = '2026, Zoom Video Communications'
author = 'Walter Fan'
release = '1.0'
language = os.environ.get("SPHINX_LANGUAGE", "en")

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'myst_parser',           # MyST markdown parser
    'sphinx.ext.autodoc',    # Auto-generate docs from docstrings
    'sphinx.ext.napoleon',   # Support for NumPy and Google style docstrings
    'sphinx.ext.viewcode',   # Add links to source code
    'sphinx.ext.intersphinx',# Link to other project's documentation
    'sphinx.ext.todo',       # Support for todo items
    'sphinx.ext.graphviz',   # Support for Graphviz graphs
    'sphinx_copybutton',     # Add copy button to code blocks
    'sphinx_design',         # Design components (cards, tabs, etc.)
    'sphinxcontrib.mermaid', # Mermaid diagrams (primary)
    # Uncomment below and install plantuml.jar to enable PlantUML support:
    # 'sphinxcontrib.plantuml',
]

# MyST parser configuration
myst_enable_extensions = [
    "amsmath",          # LaTeX math
    "colon_fence",      # ::: fence for directives
    "deflist",          # Definition lists
    "dollarmath",       # $...$ for inline math
    "fieldlist",        # Field lists
    "html_admonition",  # HTML admonitions
    "html_image",       # HTML images
    "linkify",          # Auto-detect URLs
    "replacements",     # Text replacements
    "smartquotes",      # Smart quotes
    "strikethrough",    # ~~strikethrough~~
    "substitution",     # {{ variable }}
    "tasklist",         # - [ ] task lists
]

# MyST parser settings
myst_heading_anchors = 3  # Auto-generate anchors for headings up to level 3
myst_footnote_transition = True
myst_dmath_double_inline = True
myst_fence_as_directive = ["mermaid"]  # treat ```mermaid fences as Sphinx directives

templates_path = ['_templates']
exclude_patterns = ['_build', '_generated', 'Thumbs.db', '.DS_Store', '**.ipynb_checkpoints', 'README.md']
locale_dirs = ['locale/']
gettext_compact = False
gettext_uuid = True

# The suffix(es) of source filenames.
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

# The master document
master_doc = 'index'

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'  # Read the Docs theme
# html_theme = 'furo'  # Alternative: Modern, clean theme

html_static_path = ['_static']
html_context = {
    "available_languages": [
        {"code": "en", "route": "en", "label": "English"},
        {"code": "zh_CN", "route": "zh", "label": "中文"},
    ],
    "language_routes": {
        "en": "en",
        "zh_CN": "zh",
    },
    "current_language": language,
}

# Theme options
html_theme_options = {
    'navigation_depth': 4,
    'collapse_navigation': False,
    'sticky_navigation': True,
    'includehidden': True,
    'titles_only': False,
}

# Custom CSS
html_css_files = [
    'custom.css',
]

# Logo and favicon
# html_logo = '_static/logo.png'
# html_favicon = '_static/favicon.ico'

# -- Options for LaTeX output ------------------------------------------------

latex_elements = {
    'papersize': 'a4paper',
    'pointsize': '10pt',
    'preamble': '',
    'figure_align': 'htbp',
}

# Grouping the document tree into LaTeX files
latex_documents = [
    (master_doc, 'project.tex', 'Project Documentation',
     author, 'manual'),
]

# -- Options for manual page output ------------------------------------------

man_pages = [
    (master_doc, 'project', 'Project Documentation',
     [author], 1)
]

# -- Options for Texinfo output ----------------------------------------------

texinfo_documents = [
    (master_doc, 'project', 'Project Documentation',
     author, 'project', 'Project description.',
     'Miscellaneous'),
]

# -- Extension configuration -------------------------------------------------

# sphinx.ext.intersphinx
intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
}

# sphinx.ext.todo
todo_include_todos = True

# sphinx.ext.autodoc
autodoc_default_options = {
    'members': True,
    'member-order': 'bysource',
    'special-members': '__init__',
    'undoc-members': True,
    'exclude-members': '__weakref__'
}

# -- Translate MyST document titles via doctree-resolved event ---------------
# MyST-parser's first `# heading` becomes a title node that Sphinx's i18n
# transform does not translate. This hook patches the title after all other
# transforms have run.

def _translate_titles(app, doctree, docname):
    """Translate top-level title nodes that Sphinx's i18n missed."""
    if app.config.language == 'en':
        return
    import gettext as _gettext
    from docutils import nodes
    try:
        t = _gettext.translation(
            docname, localedir=str(app.srcdir / 'locale'),
            languages=[app.config.language])
    except FileNotFoundError:
        return
    for node in doctree.traverse(nodes.title):
        orig = node.astext()
        translated = t.gettext(orig)
        if translated != orig:
            node.clear()
            node += nodes.Text(translated)
        break  # only patch the first (document) title


# -- Render PKB-metadata footer visibly in HTML output -----------------------
# The <!-- PKB-metadata ... --> comment in each .md file is invisible in HTML.
# This hook reads it from the source file and appends a styled info table.

import re as _re
from pathlib import Path as _Path

_META_RE = _re.compile(
    r'<!--\s*PKB-metadata\s*\n(.*?)-->',
    _re.DOTALL,
)

_LABEL_MAP = {
    'last_updated': ('Last Updated', '最后更新'),
    'commit': ('Commit', '提交'),
    'updated_by': ('Updated By', '更新者'),
    'review_status': ('Review Status', '审核状态'),
    'review_score': ('Review Score', '审核评分'),
    'reviewed_by': ('Reviewed By', '审核人'),
    'confidentiality': ('Confidentiality', '保密等级'),
}

_STATUS_ZH = {'pending': '待审核', 'approved': '已审核'}

_LEVEL_LABELS = {
    'L1': ('L1 · Public', 'L1 · 公开'),
    'L2': ('L2 · Internal', 'L2 · 内部'),
    'L3': ('L3 · Confidential', 'L3 · 机密'),
    'L4': ('L4 · Secret', 'L4 · 秘密'),
    'L5': ('L5 · Top Secret', 'L5 · 绝密'),
}


def _inject_pkb_metadata(app, doctree, docname):
    """Append a visible metadata footer parsed from the source file."""
    from docutils import nodes

    source_path = _Path(app.srcdir) / f'{docname}.md'
    if not source_path.exists():
        return
    text = source_path.read_text(encoding='utf-8')
    m = _META_RE.search(text)
    if not m:
        return

    is_zh = app.config.language != 'en'
    lang_idx = 1 if is_zh else 0

    meta = {}
    for line in m.group(1).strip().splitlines():
        line = line.strip()
        if ':' in line:
            key, _, val = line.partition(':')
            meta[key.strip()] = val.strip()

    if not meta:
        return

    rows = []
    for key, labels in _LABEL_MAP.items():
        if key not in meta:
            continue
        label = labels[lang_idx]
        value = meta[key]
        if key == 'review_status' and is_zh:
            value = _STATUS_ZH.get(value, value)
        if key == 'review_score':
            value = f'{value}/5'
        if key == 'review_status':
            css = 'pkb-status-approved' if value in ('approved', '已审核') else 'pkb-status-pending'
            value_node = nodes.raw('', f'<span class="{css}">{value}</span>', format='html')
        elif key == 'confidentiality':
            level = value.strip().upper() if value else 'L1'
            if level not in _LEVEL_LABELS:
                level = 'L1'
            display = _LEVEL_LABELS[level][lang_idx]
            css = f'pkb-level pkb-level-{level.lower()}'
            value_node = nodes.raw('', f'<span class="{css}">{display}</span>', format='html')
        else:
            value_node = None
        rows.append((label, value, value_node))

    tgroup = nodes.tgroup(cols=2)
    tgroup += nodes.colspec(colwidth=30)
    tgroup += nodes.colspec(colwidth=70)

    tbody = nodes.tbody()
    for label, value, value_node in rows:
        row = nodes.row()
        entry_label = nodes.entry()
        entry_label += nodes.strong(text=label)
        row += entry_label
        entry_value = nodes.entry()
        if value_node is not None:
            entry_value += value_node
        else:
            entry_value += nodes.paragraph(text=value)
        row += entry_value
        tbody += row
    tgroup += tbody

    table = nodes.table(classes=['pkb-metadata-table'])
    table += tgroup

    title_text = '文档元信息' if is_zh else 'Document Metadata'
    container = nodes.container(classes=['pkb-metadata-footer'])
    title_para = nodes.paragraph()
    title_para += nodes.strong(text=title_text)
    container += title_para
    container += table

    doctree += nodes.transition()
    doctree += container


def setup(app):
    app.connect('doctree-resolved', _translate_titles)
    app.connect('doctree-resolved', _inject_pkb_metadata)

# -- Custom configuration ----------------------------------------------------

html_show_sourcelink = False
html_show_sphinx = False
add_module_names = False

# -- Mermaid configuration ---------------------------------------------------

# Mermaid version (use latest stable)
mermaid_version = "10.6.1"

# Mermaid initialization config
mermaid_init_js = """
mermaid.initialize({
    startOnLoad: true,
    theme: 'default',
    securityLevel: 'loose',
    logLevel: 'error',
    flowchart: {
        useMaxWidth: true,
        htmlLabels: true,
        curve: 'basis'
    },
    sequence: {
        useMaxWidth: true,
        diagramMarginX: 50,
        diagramMarginY: 10,
        actorMargin: 50,
        width: 150,
        height: 65,
        boxMargin: 10,
        boxTextMargin: 5,
        noteMargin: 10,
        messageMargin: 35,
        mirrorActors: true,
        bottomMarginAdj: 1,
        useMaxWidth: true,
        rightAngles: false,
        showSequenceNumbers: false
    },
    gantt: {
        titleTopMargin: 25,
        barHeight: 20,
        barGap: 4,
        topPadding: 50,
        leftPadding: 75,
        gridLineStartPadding: 35,
        fontSize: 11,
        numberSectionStyles: 4,
        axisFormat: '%Y-%m-%d'
    }
});
"""

# -- PlantUML configuration (disabled by default) ----------------------------
# To enable PlantUML:
#   1. Uncomment 'sphinxcontrib.plantuml' in the extensions list above
#   2. Uncomment one of the plantuml options below
#   3. Ensure plantuml.jar is available or use the server URL
#
# Option 1: Local jar file
# plantuml = 'java -jar /path/to/plantuml.jar'
# plantuml_output_format = 'svg'
#
# Option 2: PlantUML server (recommended for CI/CD)
# plantuml = 'java -jar plantuml.jar'
# plantuml_output_format = 'svg'
#
# Option 3: Public PlantUML server (not recommended for production)
# plantuml_server_url = 'http://www.plantuml.com/plantuml'
# plantuml_output_format = 'svg'

# -- Graphviz configuration --------------------------------------------------

graphviz_output_format = 'svg'
