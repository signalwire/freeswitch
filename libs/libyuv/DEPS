vars = {
  # Override root_dir in your .gclient's custom_vars to specify a custom root
  # folder name.
  'root_dir': 'libyuv',
  'extra_gyp_flag': '-Dextra_gyp_flag=0',
  'chromium_git': 'https://chromium.googlesource.com',

  # Roll the Chromium Git hash to pick up newer versions of all the
  # dependencies and tools linked to in setup_links.py.
  'chromium_revision': '1d144ca7f86e0c684c67d6c1b6d5414ca9074615',
}

# NOTE: Prefer revision numbers to tags for svn deps. Use http rather than
# https; the latter can cause problems for users behind proxies.
deps = {
  Var('root_dir') + '/third_party/gflags/src':
    Var('chromium_git') + '/external/gflags/src@e7390f9185c75f8d902c05ed7d20bb94eb914d0c', # from svn revision 82
}

# Define rules for which include paths are allowed in our source.
include_rules = [ '+gflags' ]

hooks = [
  {
    # Clone chromium and its deps.
    'name': 'sync chromium',
    'pattern': '.',
    'action': ['python', '-u', Var('root_dir') + '/sync_chromium.py',
               '--target-revision', Var('chromium_revision')],
  },
  {
    # Create links to shared dependencies in Chromium.
    'name': 'setup_links',
    'pattern': '.',
    'action': ['python', Var('root_dir') + '/setup_links.py'],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    'pattern': '.',
    'action': ['python', Var('root_dir') + '/gyp_libyuv'],
  },
]
