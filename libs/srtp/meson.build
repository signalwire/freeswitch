project('libsrtp2', 'c', version: '2.4.0',
  meson_version: '>= 0.52.0',
  default_options: ['buildtype=debugoptimized'])

soversion = 1

cc = meson.get_compiler('c')
host_system = host_machine.system()

srtp2_deps = []
syslibs = []

if host_system == 'windows'
  syslibs += [cc.find_library('ws2_32')] # for socket
endif

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())
cdata.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))

check_headers = [
  'arpa/inet.h',
  'byteswap.h',
  'inttypes.h',
  'machine/types.h',
  'netinet/in.h',
  'stdint.h',
  'stdlib.h',
  'sys/int_types.h',
  'sys/socket.h',
  'sys/types.h',
  'sys/uio.h',
  'unistd.h',
]

if host_system == 'windows'
  check_headers += ['windows.h', 'winsock2.h']
endif

foreach h : check_headers
  if cc.has_header(h)
    cdata.set('HAVE_' + h.to_upper().underscorify(), true)
  endif
endforeach

check_functions = [
  'sigaction',
  'inet_aton',
  'usleep',
  'socket',
]

foreach f : check_functions
  if cc.has_function(f, dependencies: syslibs)
    cdata.set('HAVE_' + f.to_upper().underscorify(), true)
  endif
endforeach

if host_machine.endian() == 'big'
  cdata.set('WORDS_BIGENDIAN', true)
endif

# This follows the checks in configure.ac, but is it up-to-date ?!
if host_machine.cpu_family() in ['x86', 'x86_64']
  cdata.set('CPU_CISC', true, description: 'Building for a CISC machine (e.g. Intel)')
  cdata.set('HAVE_X86', true, description: 'Use x86 inlined assembly code')
else
  cdata.set('CPU_RISC', true, description: 'Building for a RISC machine (assume slow byte access)')
endif

# Pretty much all supported platforms have stdint.h nowadays
assert(cc.has_header('stdint.h'), 'stdint.h not available!')

# we'll just assume these types are available via stdint.h
foreach type : ['int8_t', 'uint8_t', 'int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'uint64_t']
  cdata.set('HAVE_' + type.to_upper().underscorify(), true)
endforeach

size_t_prefix = '''
#ifdef _WIN32
#include <crtdefs.h>
#endif
#include <sys/types.h>
'''
if not cc.has_type('size_t', prefix: size_t_prefix)
  cdata.set('size_t', 'unsigned int')
endif

# check type availability and size
foreach type : ['unsigned long', 'unsigned long long']
  if cc.has_type(type)
    cdata.set('HAVE_' + type.to_upper().underscorify(), true)
    cdata.set('SIZEOF_' + type.to_upper().underscorify(), cc.sizeof(type))
  endif
endforeach

if not cc.compiles('inline void func(); void func() { } int main() { func(); return 0; }', name: 'inline keyword check')
  if cc.compiles('__inline void func(); void func() { } int main() { func(); return 0; }', name: '__inline keyword check')
    cdata.set('inline', '__inline')
  else
    cdata.set('inline', '')
  endif
endif

if get_option('log-stdout')
  cdata.set('ERR_REPORTING_STDOUT', true)
endif

if get_option('log-file') != ''
  cdata.set('ERR_REPORTING_FILE', get_option('log-file'))
endif

if cdata.has('ERR_REPORTING_STDOUT') and cdata.has('ERR_REPORTING_FILE')
  error('The log-stdout and log-file options are mutually exclusive!')
endif

if get_option('debug-logging')
  cdata.set('ENABLE_DEBUG_LOGGING', true)
endif

use_openssl = false
use_nss = false

crypto_library = get_option('crypto-library')
if crypto_library == 'openssl'
  openssl_dep = dependency('openssl', version: '>= 1.0.1', required: true)
  srtp2_deps += [openssl_dep]
  cdata.set('GCM', true)
  cdata.set('OPENSSL', true)
  cdata.set('USE_EXTERNAL_CRYPTO', true)
  use_openssl = true
  # NOTE: This is not available in upstream OpenSSL yet. It's only in 'certain'
  # forks of OpenSSL: https://github.com/cisco/libsrtp/issues/458
  if (
    openssl_dep.type_name() != 'internal' and
    not get_option('crypto-library-kdf').disabled() and
    cc.has_function('kdf_srtp', dependencies: openssl_dep)
  )
    cdata.set('OPENSSL_KDF', true)
  elif get_option('crypto-library-kdf').enabled()
    error('KDF support has been enabled, but OpenSSL does not provide it')
  endif
elif crypto_library == 'nss'
  nss_dep = dependency('nss', version: '>= 1.0.1', required: true)
  srtp2_deps += [nss_dep]
  cdata.set('GCM', true)
  cdata.set('NSS', true)
  cdata.set('USE_EXTERNAL_CRYPTO', true)
  use_nss = true
  # TODO(RLB): Use NSS for KDF
  if get_option('crypto-library-kdf').enabled()
    error('KDF support has not been implemented for NSS')
  endif
endif

configure_file(output: 'config.h', configuration: cdata)

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

if get_option('buildtype') != 'plain'
  w_args = ['-Wstrict-prototypes']
  add_project_arguments(cc.get_supported_arguments(w_args), language: 'c')
endif

if get_option('optimization') not in ['0', 'g', 's']
  # -fexpensive-optimizations set already by default for -O2, -O3
  o_args = ['-funroll-loops']
  add_project_arguments(cc.get_supported_arguments(o_args), language: 'c')
endif

sources = files(
  'srtp/srtp.c',
  )

ciphers_sources = files(
  'crypto/cipher/cipher.c',
  'crypto/cipher/cipher_test_cases.c',
  'crypto/cipher/null_cipher.c',
  )

if use_openssl
  ciphers_sources += files(
    'crypto/cipher/aes_icm_ossl.c',
    'crypto/cipher/aes_gcm_ossl.c',
  )
elif use_nss
  ciphers_sources += files(
    'crypto/cipher/aes_icm_nss.c',
    'crypto/cipher/aes_gcm_nss.c',
  )
else
  ciphers_sources += files(
    'crypto/cipher/aes.c',
    'crypto/cipher/aes_icm.c',
  )
endif

hashes_sources = files(
  'crypto/hash/auth.c',
  'crypto/hash/auth_test_cases.c',
  'crypto/hash/null_auth.c',
  )

if use_openssl
  hashes_sources += files(
    'crypto/hash/hmac_ossl.c',
  )
elif use_nss
  hashes_sources += files(
    'crypto/hash/hmac_nss.c',
  )
else
  hashes_sources += files(
    'crypto/hash/hmac.c',
    'crypto/hash/sha1.c',
  )
endif

kernel_sources = files(
  'crypto/kernel/alloc.c',
  'crypto/kernel/crypto_kernel.c',
  'crypto/kernel/err.c',
  'crypto/kernel/key.c',
)

math_sources = files(
  'crypto/math/datatypes.c',
)

replay_sources = files(
  'crypto/replay/rdb.c',
  'crypto/replay/rdbx.c',
)

public_headers = files(
  'include/srtp.h',
  'crypto/include/auth.h',
  'crypto/include/cipher.h',
  'crypto/include/crypto_types.h',
)
install_headers(public_headers, subdir : 'srtp2')

config_incs = include_directories('.')
crypto_incs = include_directories('crypto/include')
srtp2_incs = include_directories('include')
test_incs = include_directories('test')

default_library = get_option('default_library')

libsrtp2_static = static_library('srtp2', sources, ciphers_sources, hashes_sources,
  kernel_sources, math_sources, replay_sources,
  dependencies: [srtp2_deps, syslibs],
  include_directories: [crypto_incs, srtp2_incs],
  install: default_library != 'shared')

if default_library != 'static'
  libsrtp2 = shared_library('srtp2',
    dependencies: [srtp2_deps, syslibs],
    soversion : soversion,
    vs_module_defs: 'srtp.def',
    link_whole: libsrtp2_static,
    install: true)
else
  libsrtp2 = libsrtp2_static
endif

subdir('include/srtp2') # copies public_headers into the builddir and sets public_incs

libsrtp2_dep = declare_dependency(link_with: libsrtp2,
  include_directories: public_incs)

if not get_option('tests').disabled()
  # Tests use non-public API, and when building on Windows the only symbols we
  # export are those in srtp.def, so link to the static library in that case.
  if host_system == 'windows'
    libsrtp2_for_tests = libsrtp2_static
  else
    libsrtp2_for_tests = libsrtp2
  endif
  subdir('crypto/test')
  subdir('test')
endif

if not get_option('fuzzer').disabled()
  subdir('fuzzer')
endif

if not get_option('doc').disabled()
  subdir('doc')
endif

pkgconfig = import('pkgconfig')
pkgconfig.generate(libsrtp2,
  filebase: meson.project_name(),
  name: meson.project_name(),
  version: meson.project_version(),
  description: 'Library for SRTP (Secure Realtime Transport Protocol)')
