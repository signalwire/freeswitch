##
##  OSSP uuid - Universally Unique Identifier
##  Copyright (c) 2004-2007 Ralf S. Engelschall <rse@engelschall.com>
##  Copyright (c) 2004-2007 The OSSP Project <http://www.ossp.org/>
##  Copyright (c) 2004 Piotr Roszatycki <dexter@debian.org>
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
##  uuid_compat.ts: Data::UUID Backward Compatibility Perl API (Perl test suite part)
##

use Test::More tests => 14;

BEGIN {
    use_ok('Data::UUID');
    use Data::UUID;
};

ok($ug = new Data::UUID);

ok($uuid1 = $ug->create());
ok($uuid2 = $ug->to_hexstring($uuid1));
ok($uuid3 = $ug->from_string($uuid2));
ok($ug->compare($uuid1, $uuid3) == 0);

ok($uuid4 = $ug->to_b64string($uuid1));
ok($uuid5 = $ug->to_b64string($uuid3));
ok($uuid4 eq $uuid5);

ok($uuid6 = $ug->from_b64string($uuid5));
ok($ug->compare($uuid6, $uuid1) == 0);

ok($uuid7 = NameSpace_URL);
ok($uuid8 = $ug->from_string("6ba7b811-9dad-11d1-80b4-00c04fd430c8"));
ok($ug->compare($uuid7, $uuid8) == 0);

