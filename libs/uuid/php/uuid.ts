<?php
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
##  uuid.ts: PHP/Zend API test procedure (language: php)
##

##
##  INITIALIZATION
##

$php_version = $argv[1];

print "++ loading DSO uuid.so (low-level API)\n";
if (!extension_loaded('uuid')) {
    dl('modules/uuid.so');
}

print "++ loading PHP uuid.php${php_version} (high-level API)\n";
require "uuid.php${php_version}";

print "++ establishing assertion environment\n";
assert_options(ASSERT_ACTIVE, 1);
assert_options(ASSERT_WARNING, 0);
assert_options(ASSERT_QUIET_EVAL, 1);
function my_assert_handler($file, $line, $code)
{
    echo "ASSERTION FAILED: $file: $line: $code\n";
    exit(1);
}
assert_options(ASSERT_CALLBACK, 'my_assert_handler');

##
##  LOW-LEVEL API TESTING
##

print "++ testing low-level C-style API:\n";

$uuid = 42;
$rc = uuid_create(&$uuid);
assert('$rc == 0');
assert('$uuid != 42');

$rc = uuid_make($uuid, UUID_MAKE_V1);
assert('$rc == 0');

$str = "foo";
$rc = uuid_export($uuid, UUID_FMT_STR, &$str);
assert('$rc == 0');
assert('$str != "foo"');
print "UUID: $str\n";

$uuid_ns = 42;
$rc = uuid_create(&$uuid_ns);
assert('$rc == 0');

$rc = uuid_load($uuid_ns, "ns:URL");
assert('$rc == 0');

$rc = uuid_make($uuid, UUID_MAKE_V3, $uuid_ns, "http://www.ossp.org/");
assert('$rc == 0');

$str = "bar";
$rc = uuid_export($uuid, UUID_FMT_STR, &$str);
assert('$rc == 0');
assert('$str != "bar"');
#assert('$str == "02d9e6d5-9467-382e-8f9b-9300a64ac3cd"');
print "UUID: $str\n";

$rc = uuid_destroy($uuid);
assert('$rc == 0');

$rc = uuid_create(&$uuid);
assert('$rc == 0');

$rc = uuid_import($uuid, UUID_FMT_STR, $str);
assert('$rc == 0');

$str = "baz";
$rc = uuid_export($uuid, UUID_FMT_STR, &$str);
assert('$rc == 0');
assert('$str != "baz"');
#assert('$str == "02d9e6d5-9467-382e-8f9b-9300a64ac3cd"');
print "UUID: $str\n";

$clone = null;
$rc = uuid_clone($uuid, &$clone);
assert('$rc == 0');
assert('$clone != null');

$rc = uuid_destroy($uuid);
assert('$rc == 0');

$str = "quux";
$rc = uuid_export($clone, UUID_FMT_STR, &$str);
assert('$rc == 0');
assert('$str != "quux"');
#assert('$str == "02d9e6d5-9467-382e-8f9b-9300a64ac3cd"');
print "UUID: $str\n";

##
##  HIGH-LEVEL API TESTING
##

print "++ testing high-level OO-style API:\n";

$uuid = new UUID;
$uuid->make(UUID_MAKE_V1);
$str = $uuid->export(UUID_FMT_STR);
print "UUID: $str\n";

$uuid_ns = new UUID;
$uuid_ns->load("ns:URL");
$uuid->make(UUID_MAKE_V3, $uuid_ns, "http://www.ossp.org/");
$str = $uuid->export(UUID_FMT_STR);
print "UUID: $str\n";
$uuid = null;
$uuid_ns = null;

$uuid = new UUID;
$uuid->import(UUID_FMT_STR, $str);
$str = $uuid->export(UUID_FMT_STR);
print "UUID: $str\n";

if ($php_version == 4) {
    eval('$clone = $uuid->clone();');
}
else {
    eval('$clone = clone $uuid;');
}
$uuid = null;

$str = $clone->export(UUID_FMT_STR);
print "UUID: $str\n";

$clone = null;

?>
