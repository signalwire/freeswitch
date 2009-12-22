##
##  OSSP uuid - Universally Unique Identifier
##  Copyright (c) 2004-2007 Ralf S. Engelschall <rse@engelschall.com>
##  Copyright (c) 2004-2007 The OSSP Project <http://www.ossp.org/>
##
##  This file is part of OSSP uuid, a library for the generation
##  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
##
##  Permission to use, copy, modify, and distribute this software for
##  any purpose with or without fee is hereby granted, provided that
##  the above copyright notice and this permission notice appear in all
##  copies.
##
##  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
##  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
##  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
##  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
##  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
##  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
##  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
##  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
##  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
##  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
##  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
##  SUCH DAMAGE.
##
##  uuid.ts: Perl Binding (Perl test suite part)
##

use Test::More tests => 36;

##
##  Module Loading
##

BEGIN {
    use_ok('OSSP::uuid');
};
BEGIN {
    use OSSP::uuid qw(:all);
    ok(defined(UUID_VERSION), "UUID_VERSION");
    ok(UUID_RC_OK == 0, "UUID_RC_OK");
};

##
##  C-Style API
##

my ($rc, $result, $uuid, $uuid_ns, $str, $ptr, $len);

$rc = uuid_create($uuid);
ok($rc == UUID_RC_OK, "uuid_create (1)");
$rc = uuid_create($uuid_ns);
ok($rc == UUID_RC_OK, "uuid_create (2)");

$rc = uuid_isnil($uuid, $result);
ok(($rc == UUID_RC_OK and $result == 1), "uuid_isnil (1)");
$rc = uuid_isnil($uuid_ns, $result);
ok(($rc == UUID_RC_OK and $result == 1), "uuid_isnil (2)");
$rc = uuid_compare($uuid, $uuid_ns, $result);
ok(($rc == UUID_RC_OK and $result == 0), "uuid_compare (1)");
$rc = uuid_export($uuid, UUID_FMT_STR, $ptr, $len);
ok((    $rc == UUID_RC_OK
    and $ptr eq "00000000-0000-0000-0000-000000000000"
    and $len == UUID_LEN_STR), "uuid_export (1)");

$rc = uuid_load($uuid_ns, "ns:URL");
ok($rc == UUID_RC_OK, "uuid_load (1)");
$rc = uuid_export($uuid_ns, UUID_FMT_STR, $ptr, $len);
ok((    $rc == UUID_RC_OK
    and $ptr eq "6ba7b811-9dad-11d1-80b4-00c04fd430c8"
    and $len == UUID_LEN_STR), "uuid_export (2)");

$rc = uuid_make($uuid, UUID_MAKE_V3, $uuid_ns, "http://www.ossp.org/");
ok($rc == UUID_RC_OK, "uuid_make (1)");
$rc = uuid_export($uuid, UUID_FMT_STR, $ptr, $len);
ok((    $rc == UUID_RC_OK
    and $ptr eq "02d9e6d5-9467-382e-8f9b-9300a64ac3cd"
    and $len == UUID_LEN_STR), "uuid_export (3)");

$rc = uuid_export($uuid, UUID_FMT_BIN, $ptr, $len);
ok((    $rc == UUID_RC_OK
    and $len == UUID_LEN_BIN), "uuid_export (4)");
$rc = uuid_import($uuid_ns, UUID_FMT_BIN, $ptr, $len);
ok($rc == UUID_RC_OK, "uuid_import (1)");
$rc = uuid_export($uuid_ns, UUID_FMT_STR, $ptr, $len);
ok((    $rc == UUID_RC_OK
    and $ptr eq "02d9e6d5-9467-382e-8f9b-9300a64ac3cd"
    and $len == UUID_LEN_STR), "uuid_export (5)");
$rc = uuid_export($uuid_ns, UUID_FMT_SIV, $ptr, $len);
ok((    $rc == UUID_RC_OK
    and $ptr eq "3789866285607910888100818383505376205"
    and $len <= UUID_LEN_SIV), "uuid_export (6)");

$rc = uuid_destroy($uuid_ns);
ok($rc == UUID_RC_OK, "uuid_destroy (1)");
$rc = uuid_destroy($uuid);
ok($rc == UUID_RC_OK, "uuid_destroy (2)");

##
##  OO-style API
##

$uuid = new OSSP::uuid;
ok(defined($uuid), "new OSSP::uuid (1)");
$uuid_ns = new OSSP::uuid;
ok(defined($uuid_ns), "new OSSP::uuid (2)");

$rc = $uuid->isnil();
ok((defined($rc) and $rc == 1), "isnil (1)");
$rc = $uuid_ns->isnil();
ok((defined($rc) and $rc == 1), "isnil (2)");

$rc = $uuid->compare($uuid_ns);
ok((defined($rc) and $rc == 0), "compare (1)");

$ptr = $uuid->export("str");
ok((    defined($ptr)
    and $ptr eq "00000000-0000-0000-0000-000000000000"
    and length($ptr) == UUID_LEN_STR), "export (1)");

$rc = $uuid_ns->load("ns:URL");
ok(defined($rc), "uuid_load (1)");
$ptr = $uuid_ns->export("str");
ok((    defined($ptr)
    and $ptr eq "6ba7b811-9dad-11d1-80b4-00c04fd430c8"
    and length($ptr) == UUID_LEN_STR), "export (2)");

$rc = $uuid->make("v3", $uuid_ns, "http://www.ossp.org/");
ok(defined($rc), "make (1)");
$ptr = $uuid->export("str");
ok((    defined($ptr)
    and $ptr eq "02d9e6d5-9467-382e-8f9b-9300a64ac3cd"
    and length($ptr) == UUID_LEN_STR), "export (3)");

$ptr = $uuid->export("bin");
ok((    defined($ptr)
    and length($ptr) == UUID_LEN_BIN), "export (4)");
$rc = $uuid_ns->import("bin", $ptr);
ok(defined($rc), "import (1)");
$ptr = $uuid_ns->export("str");
ok((    defined($ptr)
    and $ptr eq "02d9e6d5-9467-382e-8f9b-9300a64ac3cd"
    and length($ptr) == UUID_LEN_STR), "export (5)");

undef $uuid;
undef $uuid_ns;

##
##  TIE API
##

$uuid = new OSSP::uuid;

tie my $var, 'OSSP::uuid::tie';

my $val_get1 = $var;
my $val_get2 = $var;
ok($val_get1 ne $val_get2, "subsequent generation");

$uuid->import("str", $val_get1);
my $val_cmp1 = $uuid->export("str");
$uuid->import("str", $val_get2);
my $val_cmp2 = $uuid->export("str");
ok($val_get1 eq $val_cmp1, "validity comparison 1");
ok($val_get2 eq $val_cmp2, "validity comparison 2");

$var = [ "v3", "ns:URL", "http://www.ossp.org/" ];
$val_get1 = $var;
ok($val_get1 eq "02d9e6d5-9467-382e-8f9b-9300a64ac3cd", "generation of UUID v3");

