/*
**  OSSP uuid - Universally Unique Identifier
**  Copyright (c) 2004-2007 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2004-2007 The OSSP Project <http://www.ossp.org/>
**
**  This file is part of OSSP uuid, a library for the generation
**  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
**
**  Permission to use, copy, modify, and distribute this software for
**  any purpose with or without fee is hereby granted, provided that
**  the above copyright notice and this permission notice appear in all
**  copies.
**
**  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
**  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
**  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
**  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
**  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
**  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
**  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
**  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
**
**  uuid.xs: Perl Binding (Perl/XS part)
*/

#include "uuid.h"

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

MODULE = OSSP::uuid PACKAGE = OSSP::uuid

void
constant(sv)
    PREINIT:
        dXSTARG;
        STRLEN          len;
        int             i;
        static struct {
            const char *name;
            int         value;
        } constant_table[] = {
            { "UUID_VERSION", UUID_VERSION },
            { "UUID_LEN_BIN", UUID_LEN_BIN },
            { "UUID_LEN_STR", UUID_LEN_STR },
            { "UUID_LEN_SIV", UUID_LEN_SIV },
            { "UUID_RC_OK",   UUID_RC_OK   },
            { "UUID_RC_ARG",  UUID_RC_ARG  },
            { "UUID_RC_MEM",  UUID_RC_MEM  },
            { "UUID_RC_SYS",  UUID_RC_SYS  },
            { "UUID_RC_INT",  UUID_RC_INT  },
            { "UUID_RC_IMP",  UUID_RC_IMP  },
            { "UUID_MAKE_V1", UUID_MAKE_V1 },
            { "UUID_MAKE_V3", UUID_MAKE_V3 },
            { "UUID_MAKE_V4", UUID_MAKE_V4 },
            { "UUID_MAKE_V5", UUID_MAKE_V5 },
            { "UUID_MAKE_MC", UUID_MAKE_MC },
            { "UUID_FMT_BIN", UUID_FMT_BIN },
            { "UUID_FMT_STR", UUID_FMT_STR },
            { "UUID_FMT_SIV", UUID_FMT_SIV },
            { "UUID_FMT_TXT", UUID_FMT_TXT }
        };
    INPUT:
        SV             *sv;
        const char     *s = SvPV(sv, len);
    PPCODE:
        for (i = 0; i < sizeof(constant_table)/sizeof(constant_table[0]); i++) {
            if (strcmp(s, constant_table[i].name) == 0) {
                EXTEND(SP, 1);
                PUSHs(&PL_sv_undef);
                PUSHi(constant_table[i].value);
                break;
            }
        }
        if (i == sizeof(constant_table)/sizeof(constant_table[0])) {
            sv = sv_2mortal(newSVpvf("unknown contant OSSP::uuid::%s", s));
            PUSHs(sv);
        }


uuid_rc_t
uuid_create(uuid)
    PROTOTYPE:
        $
    INPUT:
        uuid_t *&uuid = NO_INIT
    CODE:
        RETVAL = uuid_create(&uuid);
    OUTPUT:
        uuid
        RETVAL

uuid_rc_t
uuid_destroy(uuid)
    PROTOTYPE:
        $
    INPUT:
        uuid_t *uuid
    CODE:
        RETVAL = uuid_destroy(uuid);
    OUTPUT:
        RETVAL

uuid_rc_t
uuid_load(uuid,name)
    PROTOTYPE:
        $$
    INPUT:
        uuid_t *uuid
        const char *name
    CODE:
        RETVAL = uuid_load(uuid, name);
    OUTPUT:
        RETVAL

uuid_rc_t
uuid_make(uuid,mode,...)
    PROTOTYPE:
        $$;$$
    INPUT:
        uuid_t *uuid
        unsigned int mode
    PREINIT:
        uuid_t *ns;
        const char *name;
    CODE:
        if ((mode & UUID_MAKE_V3) || (mode & UUID_MAKE_V5)) {
            if (items != 4)
                croak("mode UUID_MAKE_V3/UUID_MAKE_V5 requires two additional arguments to uuid_make()");
	        if (!SvROK(ST(2)))
                croak("mode UUID_MAKE_V3/UUID_MAKE_V5 requires a UUID object as namespace");
            ns   = INT2PTR(uuid_t *, SvIV((SV*)SvRV(ST(2))));
            name = (const char *)SvPV_nolen(ST(3));
            RETVAL = uuid_make(uuid, mode, ns, name);
        }
        else {
            if (items != 2)
                croak("invalid number of arguments to uuid_make()");
            RETVAL = uuid_make(uuid, mode);
        }
    OUTPUT:
        RETVAL

uuid_rc_t
uuid_isnil(uuid,result)
    PROTOTYPE:
        $$
    INPUT:
        uuid_t *uuid
        int &result = NO_INIT
    CODE:
        RETVAL = uuid_isnil(uuid, &result);
    OUTPUT:
        result
        RETVAL

uuid_rc_t
uuid_compare(uuid,uuid2,result)
    PROTOTYPE:
        $$$
    INPUT:
        uuid_t *uuid
        uuid_t *uuid2
        int &result = NO_INIT
    CODE:
        RETVAL = uuid_compare(uuid, uuid2, &result);
    OUTPUT:
        result
        RETVAL

uuid_rc_t
uuid_import(uuid,fmt,data_ptr,data_len)
    PROTOTYPE:
        $$$$
    INPUT:
        uuid_t *uuid
        uuid_fmt_t fmt
        const void *data_ptr
        size_t data_len
    CODE:
        if (ST(3) == &PL_sv_undef)
            data_len = sv_len(ST(2));
        RETVAL = uuid_import(uuid, fmt, data_ptr, data_len);
    OUTPUT:
        RETVAL

uuid_rc_t
uuid_export(uuid,fmt,data_ptr,data_len)
    PROTOTYPE:
        $$$$
    INPUT:
        uuid_t *uuid
        uuid_fmt_t fmt
        void *&data_ptr = NO_INIT
        size_t &data_len = NO_INIT
    PPCODE:
        data_ptr = NULL;
        data_len = 0;
        RETVAL = uuid_export(uuid, fmt, &data_ptr, &data_len);
        if (RETVAL == UUID_RC_OK) {
            if (fmt == UUID_FMT_SIV)
                data_len = strlen((char *)data_ptr);
            else if (fmt == UUID_FMT_STR || fmt == UUID_FMT_TXT)
                data_len--; /* Perl doesn't wish NUL-termination on strings */
            sv_setpvn(ST(2), data_ptr, data_len);
            free(data_ptr);
            if (ST(3) != &PL_sv_undef)
                sv_setuv(ST(3), (UV)data_len);
        }
        PUSHi((IV)RETVAL);

char *
uuid_error(rc)
    PROTOTYPE:
        $
    INPUT:
        uuid_rc_t rc
    CODE:
        RETVAL = uuid_error(rc);
    OUTPUT:
        RETVAL

unsigned long
uuid_version()
    PROTOTYPE:
    INPUT:
    CODE:
        RETVAL = uuid_version();
    OUTPUT:
        RETVAL

