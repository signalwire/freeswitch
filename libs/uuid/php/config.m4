dnl
dnl  OSSP uuid - Universally Unique Identifier
dnl  Copyright (c) 2004-2007 Ralf S. Engelschall <rse@engelschall.com>
dnl  Copyright (c) 2004-2007 The OSSP Project <http://www.ossp.org/>
dnl
dnl  This file is part of OSSP uuid, a library for the generation
dnl  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
dnl
dnl  Permission to use, copy, modify, and distribute this software for
dnl  any purpose with or without fee is hereby granted, provided that
dnl  the above copyright notice and this permission notice appear in all
dnl  copies.
dnl
dnl  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
dnl  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
dnl  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
dnl  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
dnl  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
dnl  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
dnl  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
dnl  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
dnl  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
dnl  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
dnl  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
dnl  SUCH DAMAGE.
dnl
dnl  config.m4: PHP/Zend API build-time configuration (language: m4)
dnl

PHP_ARG_ENABLE(uuid, OSSP uuid module,
[  --enable-uuid             Enable OSSP uuid extension module.])

if test "$PHP_UUID" != "no"; then
    PHP_NEW_EXTENSION(uuid, uuid.c, $ext_shared)
    AC_DEFINE(HAVE_UUID, 1, [Have OSSP uuid library])
    PHP_ADD_LIBPATH([..], )
    PHP_ADD_LIBRARY([uuid],, UUID_SHARED_LIBADD)
    PHP_ADD_INCLUDE([..])
    PHP_SUBST(UUID_SHARED_LIBADD)

    dnl  avoid linking conflict with a potentially existing uuid_create(3) in libc
    AC_CHECK_FUNC(uuid_create,[
        SAVE_LDFLAGS="$LDFLAGS"
        LDFLAGS="$LDFLAGS -Wl,-Bsymbolic"
        AC_TRY_LINK([],[], [EXTRA_LDFLAGS="$EXTRA_LDFLAGS -Wl,-Bsymbolic"])
        LDFLAGS="$SAVE_LDFLAGS"])
fi

