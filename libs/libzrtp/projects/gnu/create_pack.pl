#!/usr/bin/perl

use Getopt::Std;

getopts("l:ehs", \%args);

if ($args{h})
{
	print "Usage: create_pack.pl [OPTION]...\n\n";
	print "  -l file      write down list of files\n";
	print "  -e           enterprise version\n";
	print "  -s           dont add version suffix to package name\n";
	print "  -h           this help\n\n";
	exit 1;
}

#to create list of files:
if ($args{l})
{
	create_files_list($args{l});
	exit 1;
}

$enterprise = 0;
if ($args{e}) 
{
	$enterprise = 1;
}

if ($args{s})
{
	$packdir="libzrtp";
}
else
{
	$LIBZRTP_VERSION=`cat ../../include/zrtp_version.h | grep 'LIBZRTP_VERSION_STR' | awk '{print \$3, \$4}' | sed 's/"v\\(.*\\) \\(.*\\)"/\\1.\\2/'`;
	chomp($LIBZRTP_VERSION);
	$packdir="libzrtp-$LIBZRTP_VERSION";
}

if (-d $packdir) 
{
	`rm -rf $packdir`
}

mkdir $packdir;
create_array();

foreach $file(@array) 
{
	if (!$enterprise && 
			(($file =~ m/\/enterprise/i) ||
	  		 ($file =~ m/_ec.*(proj|sln)/i) ||
	  		 ($file =~ m/_EC.*(WIN)/i) ||
	  		 ($file =~ m/\/xcode/i)))
	{
		print "$file skipped\n";
		next;
	}
	
	$path = "../../" . $file;
	if (!-e $path)
	{
		print "[ERROR]: file $file doesn't exist!\n";
		`rm -rf $packdir`;
		exit -1;
	}
	if (-d $path)
	{
		mkdir "$packdir/$file";
	}
	else 
	{
#		print "copying $path file\n";
		`cp $path $packdir/$file`
	}
}

if (!$enterprise)
{
	`cp -f ../../projects/win/libzrtp_not_ec.vcproj $packdir/projects/win/libzrtp.vcproj`;
	`cp -f ../../projects/win_ce/libzrtp_wince_not_ec.vcproj $packdir/projects/win_ce/libzrtp_wince.vcproj`;
	`cp -f ../../projects/win_kernel/MAKEFILE_NOT_EC.WIN64 $packdir/projects/win_kernel/MAKEFILE.WIN64`;
	`cp -f ../../projects/win_kernel/MAKEFILE_NOT_EC.WIN32 $packdir/projects/win_kernel/MAKEFILE.WIN32`;
	
	`rm $packdir/include/zrtp_ec.h`;
#	`rm $packdir/include/zrtp_iface_cache.h`;
	`rm $packdir/src/zrtp_crypto_ecdsa.c`;
	`rm $packdir/src/zrtp_crypto_ec.c`;
#	`rm $packdir/src/zrtp_engine_driven.c`;
	`rm $packdir/src/zrtp_crypto_ecdh.c`;
#	`rm $packdir/src/zrtp_iface_cache.c`;
}

	  
`find $packdir -name "._*" -delete`;

$pack_name = $packdir;
if ($enterprise)
{
    $pack_name = $pack_name . "-ec";
}

$system = `uname -a`;
if ($system =~ m/darwin/i)
{
    `rm -rf $pack_name.zip`;
    `zip -r $pack_name.zip $packdir`;
}
else
{
    `rm -rf $pack_name.tar.gz`;
    `tar -zcvf $pack_name.tar.gz $packdir`;
}
`rm -rf $packdir`;
print "package was created\n";

#for item in $array; do
#  echo "item:"$'\t'"$item"

sub create_files_list()
{
  	$path = `pwd`;
	chop($path);
	`cd ../..;find . -not -path *svn* -print | awk '{printf \"\\t\\t\\"%s\\",\\n\", \$1} ' > $path/$_[0];cd $path`;
}
  

sub create_array() 
{
	@array =
	(
		"./ChangeLog",
		"./README",
		"./AUTHORS",
		"./projects",
		"./projects/gnu",
		"./projects/gnu/Makefile.am",
		"./projects/gnu/Makefile.in",
		"./projects/gnu/COPYING",
		"./projects/gnu/aclocal.m4",
		"./projects/gnu/configure",
		"./projects/gnu/README",
		"./projects/gnu/AUTHORS",
		"./projects/gnu/configure.in",
		"./projects/gnu/INSTALL",
		"./projects/gnu/autoreconf.sh",
		"./projects/gnu/config",
		"./projects/gnu/config/config.guess",
		"./projects/gnu/config/config.sub",
		"./projects/gnu/config/config.h.in",
		"./projects/gnu/config/install-sh",
		"./projects/gnu/config/missing",
		"./projects/gnu/config/prefix_config.m4",
		"./projects/gnu/config/depcomp",
		"./projects/gnu/NEWS",
		"./projects/gnu/Makefile.in",
		"./projects/gnu/build",
		"./projects/gnu/build/Makefile.am",
		"./projects/gnu/build/Makefile.in",
		"./projects/gnu/build/test",
		"./projects/gnu/build/test/Makefile.am",
		"./projects/gnu/build/test/Makefile.in",
		"./projects/gnu/ChangeLog",
		"./projects/xcode",
		"./projects/xcode/libzrtp.xcodeproj",
		"./projects/xcode/libzrtp.xcodeproj/project.pbxproj",
		"./projects/xcode/libzrtp_test.xcodeproj",
		"./projects/xcode/libzrtp_test.xcodeproj/project.pbxproj",
		"./projects/win_kernel",
		"./projects/win_kernel/MAKEFILE.WIN64",
		"./projects/win_kernel/MAKEFILE.WIN32",
		"./projects/win",
		"./projects/win/libzrtp.vcproj",
		"./projects/win/libzrtp.sln",
		"./projects/win/libzrtp_test.vcproj",
		"./projects/win_ce",
		"./projects/win_ce/libzrtp_test_wince.vcproj",
		"./projects/win_ce/libzrtp_wince.sln",
		"./projects/win_ce/libzrtp_wince.vcproj",
		"./projects/symbian",
		"./projects/symbian/bld.bat",
		"./projects/symbian/bld.inf",
		"./projects/symbian/bldgcce.bat",
		"./projects/symbian/libzrtp.mmp",
		"./projects/symbian/zrtp_iface_symb.cpp",
		"./src",
		"./src/zrtp.c",
		"./src/zrtp_crc.c",
		"./src/zrtp_crypto_aes.c",
		"./src/zrtp_crypto_atl.c",
		"./src/zrtp_crypto_hash.c",
		"./src/zrtp_crypto_pk.c",
		"./src/zrtp_crypto_sas.c",
		"./src/zrtp_datatypes.c",
		"./src/zrtp_engine.c",
		"./src/zrtp_iface_scheduler.c",
		"./src/zrtp_iface_sys.c",
		"./src/zrtp_initiator.c",
		"./src/zrtp_legal.c",
		"./src/zrtp_list.c",
		"./src/zrtp_log.c",
		"./src/zrtp_pbx.c",
		"./src/zrtp_protocol.c",
		"./src/zrtp_responder.c",
		"./src/zrtp_rng.c",
		"./src/zrtp_srtp_builtin.c",
		"./src/zrtp_srtp_dm.c",
		"./src/zrtp_string.c",
		"./src/zrtp_utils.c",
		"./src/zrtp_utils_proto.c",
		"./src/zrtp_crypto_ecdsa.c",
		"./src/zrtp_crypto_ec.c",
		"./src/zrtp_engine_driven.c",
		"./src/zrtp_crypto_ecdh.c",
		"./src/zrtp_iface_cache.c",
		"./doc",
		"./include",
		"./include/zrtp.h",
		"./include/zrtp_base.h",
		"./include/zrtp_config.h",
		"./include/zrtp_config_user.h",
		"./include/zrtp_config_win.h",
		"./include/zrtp_config_symbian.h",
		"./include/zrtp_crypto.h",
		"./include/zrtp_engine.h",
		"./include/zrtp_error.h",
		"./include/zrtp_ec.h",
		"./include/zrtp_iface.h",
		"./include/zrtp_iface_cache.h",
		"./include/zrtp_iface_system.h",
		"./include/zrtp_iface_scheduler.h",
		"./include/zrtp_legal.h",
		"./include/zrtp_list.h",
		"./include/zrtp_log.h",
		"./include/zrtp_pbx.h",
		"./include/zrtp_protocol.h",
		"./include/zrtp_srtp.h",
		"./include/zrtp_srtp_builtin.h",
		"./include/zrtp_string.h",
		"./include/zrtp_types.h",
		"./include/zrtp_version.h",
		"./third_party",
		"./third_party/bnlib",
		"./third_party/bnlib/lbnmem.c",
		"./third_party/bnlib/lbn00.c",
		"./third_party/bnlib/bn16.c",
		"./third_party/bnlib/bn32.c",
		"./third_party/bnlib/bn.c",
		"./third_party/bnlib/lbnppc.h",
		"./third_party/bnlib/bnsize00.h",
		"./third_party/bnlib/lbn32.h",
		"./third_party/bnlib/lbn80386.h",
		"./third_party/bnlib/lbn68020.h",
		"./third_party/bnlib/germtest",
		"./third_party/bnlib/jacobi.h",
		"./third_party/bnlib/bn00.c",
		"./third_party/bnlib/bnconfig.h",
		"./third_party/bnlib/lbn8086.h",
		"./third_party/bnlib/bntest00.c",
		"./third_party/bnlib/germain.c",
		"./third_party/bnlib/lbn960jx.h",
		"./third_party/bnlib/sizetest.c",
		"./third_party/bnlib/config.cache",
		"./third_party/bnlib/bn68000.c",
		"./third_party/bnlib/lbnalpha.h",
		"./third_party/bnlib/cputime.h",
		"./third_party/bnlib/legal.c",
		"./third_party/bnlib/configure.lineno",
		"./third_party/bnlib/configure",
		"./third_party/bnlib/bnprint.c",
		"./third_party/bnlib/bn8086.c",
		"./third_party/bnlib/lbn68020.c",
		"./third_party/bnlib/README.bntest",
		"./third_party/bnlib/lbn8086.asm",
		"./third_party/bnlib/lbn16.c",
		"./third_party/bnlib/lbn32.c",
		"./third_party/bnlib/legal.h",
		"./third_party/bnlib/configure.in",
		"./third_party/bnlib/lbn960jx.s",
		"./third_party/bnlib/prime.h",
		"./third_party/bnlib/bninit16.c",
		"./third_party/bnlib/bninit32.c",
		"./third_party/bnlib/files",
		"./third_party/bnlib/ppcasm.h",
		"./third_party/bnlib/lbn.h",
		"./third_party/bnlib/README.bn",
		"./third_party/bnlib/bnintern.doc",
		"./third_party/bnlib/sieve.c",
		"./third_party/bnlib/bn16.h",
		"./third_party/bnlib/bn32.h",
		"./third_party/bnlib/bnprint.h",
		"./third_party/bnlib/sieve.h",
		"./third_party/bnlib/cfg",
		"./third_party/bnlib/lbn68000.h",
		"./third_party/bnlib/lbnalpha.s",
		"./third_party/bnlib/bntest16.c",
		"./third_party/bnlib/bntest32.c",
		"./third_party/bnlib/cfg.debug",
		"./third_party/bnlib/lbnmem.h",
		"./third_party/bnlib/germtest.c",
		"./third_party/bnlib/prime.c",
		"./third_party/bnlib/lbn68000.c",
		"./third_party/bnlib/config.log",
		"./third_party/bnlib/germain.h",
		"./third_party/bnlib/kludge.h",
		"./third_party/bnlib/Makefile.in",
		"./third_party/bnlib/test",
		"./third_party/bnlib/test/primetest.c",
		"./third_party/bnlib/test/rsaglue.h",
		"./third_party/bnlib/test/randpool.c",
		"./third_party/bnlib/test/keys.c",
		"./third_party/bnlib/test/primes.doc",
		"./third_party/bnlib/test/rsatest.c",
		"./third_party/bnlib/test/posix.h",
		"./third_party/bnlib/test/legal.c",
		"./third_party/bnlib/test/README.rsatest",
		"./third_party/bnlib/test/rsaglue.c",
		"./third_party/bnlib/test/kbmsdos.c",
		"./third_party/bnlib/test/keygen.c",
		"./third_party/bnlib/test/README.dsatest",
		"./third_party/bnlib/test/types.h",
		"./third_party/bnlib/test/random.c",
		"./third_party/bnlib/test/md5.c",
		"./third_party/bnlib/test/userio.h",
		"./third_party/bnlib/test/md5.h",
		"./third_party/bnlib/test/dsatest.c",
		"./third_party/bnlib/test/pt.c",
		"./third_party/bnlib/test/dhtest.c",
		"./third_party/bnlib/test/sha.h",
		"./third_party/bnlib/test/keygen.h",
		"./third_party/bnlib/test/noise.h",
		"./third_party/bnlib/test/first.h",
		"./third_party/bnlib/test/README.dhtest",
		"./third_party/bnlib/test/randtest.c",
		"./third_party/bnlib/test/randpool.h",
		"./third_party/bnlib/test/random.h",
		"./third_party/bnlib/test/sha.c",
		"./third_party/bnlib/test/noise.c",
		"./third_party/bnlib/test/kbunix.c",
		"./third_party/bnlib/test/kludge.h",
		"./third_party/bnlib/test/keys.h",
		"./third_party/bnlib/test/usuals.h",
		"./third_party/bnlib/test/kb.h",
		"./third_party/bnlib/CHANGES",
		"./third_party/bnlib/bnconfig.hin",
		"./third_party/bnlib/lbn80386.asm",
		"./third_party/bnlib/jacobi.c",
		"./third_party/bnlib/config.status",
		"./third_party/bnlib/lbn16.h",
		"./third_party/bnlib/lbn80386.s",
		"./third_party/bnlib/lbn68360.s",
		"./third_party/bnlib/bignum-ARM",
		"./third_party/bnlib/bignum-ARM/lbnmem.c",
		"./third_party/bnlib/bignum-ARM/sha256_core.s",
		"./third_party/bnlib/bignum-ARM/lbnarm.h",
		"./third_party/bnlib/bignum-ARM/config.h",
		"./third_party/bnlib/bignum-ARM/cputime.h",
		"./third_party/bnlib/bignum-ARM/lbn16.c",
		"./third_party/bnlib/bignum-ARM/lbnarm.s",
		"./third_party/bnlib/bignum-ARM/README-small-memory",
		"./third_party/bnlib/bignum-ARM/sha256_arm.c",
		"./third_party/bnlib/bignum-ARM/lbn.h",
		"./third_party/bnlib/bignum-ARM/bntest16.c",
		"./third_party/bnlib/bignum-ARM/lbnmem.h",
		"./third_party/bnlib/bignum-ARM/kludge.h",
		"./third_party/bnlib/bignum-ARM/lbn16.h",
		"./third_party/bnlib/bn.doc",
		"./third_party/bnlib/lbnppc.c",
		"./third_party/bnlib/bn.h",
		"./third_party/bgaes",
		"./third_party/bgaes/sha1.h",
		"./third_party/bgaes/sha1.c",
		"./third_party/bgaes/brg_types.h",
		"./third_party/bgaes/aestab.c",
		"./third_party/bgaes/aestab.h",
		"./third_party/bgaes/sha2.h",
		"./third_party/bgaes/aes_modes.c",
		"./third_party/bgaes/aescrypt.c",
		"./third_party/bgaes/bg2zrtp.h",
		"./third_party/bgaes/aeskey.c",
		"./third_party/bgaes/sha2.c",
		"./third_party/bgaes/aes.h",
		"./third_party/bgaes/aesopt.h",
		"./test",
		"./test/README",
		"./test/pc",
		"./test/pc/zrtp_test_core.c",
		"./test/pc/zrtp_test_core.h",
		"./test/pc/zrtp_test_crypto.c",
		"./test/pc/zrtp_test_queue.c",
		"./test/pc/zrtp_test_queue.h",
		"./test/pc/zrtp_test_ui.c",
		"./test/win_ce",
		"./test/win_ce/libzrtp_test_GUI.cpp",
		"./test/win_ce/libzrtp_test_GUI.h",
		"./test/win_ce/libzrtp_test_GUI.ico",
		"./test/win_ce/libzrtp_test_GUIppc.rc",
		"./test/win_ce/libzrtp_test_GUIppc.rc2",
		"./test/win_ce/libzrtp_test_GUIsp.rc",
		"./test/win_ce/libzrtp_test_GUIsp.rc2",
		"./test/win_ce/ReadMe.txt",
		"./test/win_ce/resourceppc.h",
		"./test/win_ce/resourcesp.h",
		"./test/win_ce/stdafx.cpp",
		"./test/win_ce/stdafx.h",
		"./doc",
		"./doc/img",
		"./doc/manuals",
		"./doc/manuals/howto.dox",
		"./doc/manuals/main.dox",
		"./doc/manuals/rng.dox",
		"./doc/out",
		"./doc/out/html",
		"./doc/out/html/zfone.jpg",
		"./doc/Doxyfile",
		"./doc/doxygen.css",
		"./doc/footer.html",
		"./doc/header.html"
	)
}
