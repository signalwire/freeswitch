#
# Copyright 2014 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This is a copy of WebRTC's gflags.gyp.

{
  'variables': {
    'gflags_root': '<(DEPTH)/third_party/gflags',
    'conditions': [
      ['OS=="win"', {
        'gflags_gen_arch_root': '<(gflags_root)/gen/win',
      }, {
        'gflags_gen_arch_root': '<(gflags_root)/gen/posix',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'gflags',
      'type': 'static_library',
      'include_dirs': [
        '<(gflags_gen_arch_root)/include/private',  # For config.h
        '<(gflags_gen_arch_root)/include',  # For configured files.
        '<(gflags_root)/src',  # For everything else.
      ],
      'defines': [
        # These macros exist so flags and symbols are properly
        # exported when building DLLs. Since we don't build DLLs, we
        # need to disable them.
        'GFLAGS_DLL_DECL=',
        'GFLAGS_DLL_DECLARE_FLAG=',
        'GFLAGS_DLL_DEFINE_FLAG=',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(gflags_gen_arch_root)/include',  # For configured files.
          '<(gflags_root)/src',  # For everything else.
        ],
        'defines': [
          'GFLAGS_DLL_DECL=',
          'GFLAGS_DLL_DECLARE_FLAG=',
          'GFLAGS_DLL_DEFINE_FLAG=',
        ],
      },
      'sources': [
        'src/gflags.cc',
        'src/gflags_completions.cc',
        'src/gflags_reporting.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'src/windows/port.cc',
          ],
          # Suppress warnings about WIN32_LEAN_AND_MEAN and size_t truncation.
          'msvs_disabled_warnings': [4005, 4267],
        }],
        # TODO(andrew): Look into fixing this warning upstream:
        # http://code.google.com/p/webrtc/issues/detail?id=760
        ['OS=="win" and clang==1', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions!': [
                '-Wheader-hygiene',  # Suppress warning about using namespace.
              ],
              'AdditionalOptions': [
                '-Wno-unused-local-typedef',  # Suppress unused private typedef.
              ],
            },
          },
        }],
        ['clang==1', {
          'cflags': ['-Wno-unused-local-typedef',],
          'cflags!': ['-Wheader-hygiene',],
          'xcode_settings': {
            'WARNING_CFLAGS': ['-Wno-unused-local-typedef',],
            'WARNING_CFLAGS!': ['-Wheader-hygiene',],
          },
        }],
      ],
    },
  ],
}

