# -*- mode: python; -*-

VERSION = "0.4"

# --- options ----
AddOption('--test-server',
          dest='test_server',
          default='127.0.0.1',
          type='string',
          nargs=1,
          action='store',
          help='IP address of server to use for testing')

AddOption('--seed-start-port',
          dest='seed_start_port',
          default=30000,
          type='int',
          nargs=1,
          action='store',
          help='IP address of server to use for testing')

AddOption('--c99',
          dest='use_c99',
          default=False,
          action='store_true',
          help='Compile with c99 (recommended for gcc)')

AddOption('--d',
          dest='optimize',
          default=True,
          action='store_false',
          help='disable optimizations')

AddOption('--use-platform',
          dest='compile_platform',
          default='GENERIC',
          type='string',
          nargs=1,
          action='store',
          help='Compile for a specific platform to take advantage '
               ' of particular system features. For the moment, this include timeouts only.'
               ' Current options include LINUX, '
               ' GENERIC, and CUSTOM. If you specific CUSTOM, you must place a'
               ' system-specific implementation of net.h and net.c in src/platform/custom/')

import os, sys

env = Environment( ENV=os.environ )

#  ---- Docs ----
def build_docs(env, target, source):
    buildscript_path = os.path.join(os.path.abspath("docs"))
    sys.path.insert(0, buildscript_path)
    import buildscripts
    from buildscripts import docs
    docs.main()

env.Alias("docs", [], [build_docs])
env.AlwaysBuild("docs")

# ---- Platforms ----
PLATFORM_TEST_DIR = None
if "LINUX" == GetOption('compile_platform'):
    env.Append( CPPFLAGS=" -D_MONGO_USE_LINUX_SYSTEM" )
    NET_LIB = "src/platform/linux/net.c"
    PLATFORM_TEST_DIR = "test/platform/linux/"
    PLATFORM_TESTS = [ "timeouts" ]
elif "CUSTOM" == GetOption('compile_platform'):
    env.Append( CPPFLAGS=" -D_MONGO_USE_CUSTOM_SYSTEM" )
    NET_LIB = "src/platform/custom/net.c"
else:
    NET_LIB = "src/net.c"

# ---- Libraries ----
if os.sys.platform in ["darwin", "linux2"]:
    env.Append( CPPFLAGS=" -pedantic -Wall -ggdb -DMONGO_HAVE_STDINT" )
    env.Append( CPPPATH=["/opt/local/include/"] )
    env.Append( LIBPATH=["/opt/local/lib/"] )

    if GetOption('use_c99'):
        env.Append( CFLAGS=" -std=c99 " )
        env.Append( CXXDEFINES="MONGO_HAVE_STDINT" )
    else:
        env.Append( CFLAGS=" -ansi " )

    if GetOption('optimize'):
        env.Append( CPPFLAGS=" -O3 " )
        # -O3 benchmarks *significantly* faster than -O2 when disabling networking
elif 'win32' == os.sys.platform:
    env.Append( LIBS='ws2_32' )

#we shouldn't need these options in c99 mode
if not GetOption('use_c99'):
    conf = Configure(env)

    if not conf.CheckType('int64_t'):
        if conf.CheckType('int64_t', '#include <stdint.h>\n'):
            conf.env.Append( CPPDEFINES="MONGO_HAVE_STDINT" )
        elif conf.CheckType('int64_t', '#include <unistd.h>\n'):
            conf.env.Append( CPPDEFINES="MONGO_HAVE_UNISTD" )
        elif conf.CheckType('__int64'):
            conf.env.Append( CPPDEFINES="MONGO_USE__INT64" )
        elif conf.CheckType('long long int'):
            conf.env.Append( CPPDEFINES="MONGO_USE_LONG_LONG_INT" )
        else:
            print "*** what is your 64 bit int type? ****"
            Exit(1)

    env = conf.Finish()

have_libjson = False
conf = Configure(env)
if conf.CheckLib('json'):
    have_libjson = True
env = conf.Finish()

if sys.byteorder == 'big':
    env.Append( CPPDEFINES="MONGO_BIG_ENDIAN" )

env.Append( CPPPATH=["src/"] )

coreFiles = ["src/md5.c" ]
mFiles = [ "src/mongo.c", NET_LIB, "src/gridfs.c"]
bFiles = [ "src/bson.c", "src/numbers.c", "src/encoding.c"]
mLibFiles = coreFiles + mFiles + bFiles
bLibFiles = coreFiles + bFiles
m = env.Library( "mongoc" ,  mLibFiles )
b = env.Library( "bson" , bLibFiles  )
env.Default( env.Alias( "lib" , [ m[0] , b[0] ] ) )

if os.sys.platform == "linux2":
    env.Append( SHLINKFLAGS="-shared -Wl,-soname,libmongoc.so." + VERSION )
    env.Append( SHLINKFLAGS = "-shared -Wl,-soname,libbson.so." + VERSION )

dynm = env.SharedLibrary( "mongoc" , mLibFiles )
dynb = env.SharedLibrary( "bson" , bLibFiles )
env.Default( env.Alias( "sharedlib" , [ dynm[0] , dynb[0] ] ) )



# ---- Benchmarking ----
benchmarkEnv = env.Clone()
benchmarkEnv.Append( CPPDEFINES=[('TEST_SERVER', r'\"%s\"'%GetOption('test_server')),
('SEED_START_PORT', r'%d'%GetOption('seed_start_port'))] )
benchmarkEnv.Append( LIBS=[m, b] )
benchmarkEnv.Prepend( LIBPATH=["."] )
benchmarkEnv.Program( "benchmark" ,  [ "test/benchmark.c"] )

# ---- Tests ----
testEnv = benchmarkEnv.Clone()
testCoreFiles = [ ]

def run_tests( root, tests ):
    for name in tests:
        filename = "%s/%s.c" % (root, name)
        exe = "test_" + name
        test = testEnv.Program( exe , testCoreFiles + [filename]  )
        test_alias = testEnv.Alias('test', [test], test[0].abspath + ' 2> ' + os.path.devnull)
        AlwaysBuild(test_alias)

tests = Split("sizes resize endian_swap bson bson_subobject simple update errors "
"count_delete auth gridfs validate examples helpers oid functions cursors replica_set")

# Run standard tests
run_tests("test", tests)

# Run platform tests
if not PLATFORM_TEST_DIR is None:
    run_tests( PLATFORM_TEST_DIR, PLATFORM_TESTS )

if have_libjson:
    tests.append('json')
    testEnv.Append( LIBS=["json"] )

# special case for cpptest
test = testEnv.Program( 'test_cpp' , testCoreFiles + ['test/cpptest.cpp']  )
test_alias = testEnv.Alias('test', [test], test[0].abspath + ' 2> '+ os.path.devnull)
AlwaysBuild(test_alias)
