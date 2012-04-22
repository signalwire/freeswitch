#!/usr/bin/env python
#
# USAGE: gen-build.py TYPE
#
# where TYPE is one of: make, dsp, vcproj
#
# It reads build.conf from the current directory, and produces its output
# into the current directory.
#


import os
import ConfigParser
import getopt
import string
import glob
import re

#import ezt

#
# legal platforms: aix, beos, netware, os2, os390, unix, win32
# 'make' users: aix, beos, os2, os390, unix
#
PLATFORMS = [ 'aix', 'beos', 'netware', 'os2', 'os390', 'unix', 'win32' ]
MAKE_PLATFORMS = [
  ('unix', None),
  ('aix', 'unix'),
  ('beos', 'unix'),
  ('os2', 'unix'),
  ('os390', 'unix'),
  ]
# note: MAKE_PLATFORMS is an ordered set. we want to generate unix symbols
#       first, so that the later platforms can reference them.


def main():
  parser = ConfigParser.ConfigParser()
  parser.read('build.conf')

  headers = get_files(parser.get('options', 'headers'))

  # compute the relevant headers, along with the implied includes
  legal_deps = { }
  for fname in headers:
    legal_deps[os.path.basename(fname)] = fname

  h_deps = { }
  for fname in headers:
    h_deps[os.path.basename(fname)] = extract_deps(fname, legal_deps)
  resolve_deps(h_deps)

  f = open('build-outputs.mk', 'w')
  f.write('# DO NOT EDIT. AUTOMATICALLY GENERATED.\n\n')

  # write out the platform-independent files
  files = get_files(parser.get('options', 'paths'))
  objects, dirs = write_objects(f, legal_deps, h_deps, files)
  f.write('\nOBJECTS_all = %s\n\n' % string.join(objects))

  # for each platform and each subdirectory holding platform-specific files,
  # write out their compilation rules, and an OBJECT_<subdir>_<plat> symbol.
  for platform, parent in MAKE_PLATFORMS:

    # record the object symbols to build for each platform
    group = [ '$(OBJECTS_all)' ]

    for subdir in string.split(parser.get('options', 'platform_dirs')):
      path = '%s/%s' % (subdir, platform)
      if not os.path.exists(path):
        # this subdir doesn't have a subdir for this platform, so we'll
        # use the parent-platform's set of symbols
        if parent:
          group.append('$(OBJECTS_%s_%s)' % (subdir, parent))
        continue

      # remember that this directory has files/objects
      dirs[path] = None

      # write out the compilation lines for this subdir
      files = get_files(path + '/*.c')
      objects, _unused = write_objects(f, legal_deps, h_deps, files)

      symname = 'OBJECTS_%s_%s' % (subdir, platform)

      # and write the symbol for the whole group
      f.write('\n%s = %s\n\n' % (symname, string.join(objects)))

      # and include that symbol in the group
      group.append('$(%s)' % symname)

    # write out a symbol which contains the necessary files
    f.write('OBJECTS_%s = %s\n\n' % (platform, string.join(group)))

  f.write('HEADERS = $(top_srcdir)/%s\n\n' % string.join(headers, ' $(top_srcdir)/'))
  f.write('SOURCE_DIRS = %s $(EXTRA_SOURCE_DIRS)\n\n' % string.join(dirs.keys()))

  # Build a list of all necessary directories in build tree
  alldirs = { }
  for dir in dirs.keys():
    d = dir
    while d:
      alldirs[d] = None
      d = os.path.dirname(d)

  # Sort so 'foo' is before 'foo/bar'
  keys = alldirs.keys()
  keys.sort()
  f.write('BUILD_DIRS = %s\n\n' % string.join(keys))

  f.write('.make.dirs: $(srcdir)/build-outputs.mk\n' \
          '\t@for d in $(BUILD_DIRS); do test -d $$d || mkdir $$d; done\n' \
          '\t@echo timestamp > $@\n')


def write_objects(f, legal_deps, h_deps, files):
  dirs = { }
  objects = [ ]

  for file in files:
    assert file[-2:] == '.c'
    obj = file[:-2] + '.lo'
    objects.append(obj)

    dirs[os.path.dirname(file)] = None

    # what headers does this file include, along with the implied headers
    deps = extract_deps(file, legal_deps)
    for hdr in deps.keys():
      deps.update(h_deps.get(hdr, {}))

    f.write('%s: %s .make.dirs %s\n' % (obj, file, string.join(deps.values())))

  return objects, dirs


def extract_deps(fname, legal_deps):
  "Extract the headers this file includes."
  deps = { }
  for line in open(fname).readlines():
    if line[:8] != '#include':
      continue
    inc = _re_include.match(line).group(1)
    if inc in legal_deps.keys():
      deps[inc] = legal_deps[inc]
  return deps
_re_include = re.compile('#include *["<](.*)[">]')


def resolve_deps(header_deps):
  "Alter the provided dictionary to flatten includes-of-includes."
  altered = 1
  while altered:
    altered = 0
    for hdr, deps in header_deps.items():
      # print hdr, deps
      start = len(deps)
      for dep in deps.keys():
        deps.update(header_deps.get(dep, {}))
      if len(deps) != start:
        altered = 1


def get_files(patterns):
  files = [ ]
  for pat in string.split(patterns):
    files.extend(glob.glob(pat))
  return files


if __name__ == '__main__':
  main()
