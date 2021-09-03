# libSRTP documentation

doxygen = find_program('doxygen', required: get_option('doc'))

if not doxygen.found()
  subdir_done()
endif

doc_config = configuration_data()
doc_config.set('LIBSRTPVERSIONNUMBER', meson.project_version())

doxyfile = configure_file(input: 'Doxyfile.in',
  output: 'Doxyfile',
  configuration: doc_config)

# can be built on demand with ninja -C builddir doc/html
doxygen_html_docs = custom_target('doc',
  build_by_default: false,
  command: [doxygen, doxyfile],
  output: ['html'])

alias_target('libsrtp2doc', doxygen_html_docs)
