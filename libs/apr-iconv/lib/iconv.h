/*-
 * Copyright (c) 1999,2000
 *	Konstantin Chuguev.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Konstantin Chuguev
 *	and its contributors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _ICONV_H_
#define _ICONV_H_

#include "apr.h"
#include "apr_pools.h"
#include "apr_iconv.h"
#ifdef WIN32
#define ICONV_DEFAULT_PATH "iconv"
#define API_USE_BUILTIN_ALIASES /* Use the built-in charset alias table */
#else
#include "apr_iconv_private.h"  /* contains ICONV_DEFAULT_PATH */
#ifdef _OSD_POSIX
#define API_USE_BUILTIN_ALIASES /* Use the built-in charset alias table */
#endif
#endif

#include <stddef.h>

#ifdef ICONV_INTERNAL

/*
 * iconv module types
 */
#define	ICMOD_ANY	0
#define	ICMOD_LIB	1
#define	ICMOD_UC_CCS	0x100	/* UC - CCS for CES */
#define	ICMOD_UC_CES	0x101	/* UC - CES */

/*
 * iconv module flags
 */
#define	ICMODF_LOADED	0x8000

/*
 * iconv module handler events
 */
#define	ICMODEV_LOAD	1	/* module load. after dependencies resolved */
#define	ICMODEV_UNLOAD	2	/* module unload */
#define ICMODEV_DYN_LOAD    3   /* load dynamic dependencies */
#define ICMODEV_DYN_UNLOAD  4   /* unload dynamic dependencies */

struct iconv_module_depend {
	int		md_type;
	const char *	md_name;
	const void *	md_data;
};

struct iconv_module;

/* _tbl_simple.c table_load_ccs() calls apr_iconv_mod_load(...ctx) */

typedef int iconv_mod_event_t(struct iconv_module *, int, apr_pool_t *ctx);

struct iconv_module_desc {
	int		imd_type;
	iconv_mod_event_t *imd_event;
	const struct iconv_module_depend *imd_depend;
	const void *	imd_data;
};

#define END_ICONV_MODULE_DEPEND  {0, NULL, NULL}

#define	ICONV_MODULE(type,data)	struct iconv_module_desc iconv_module \
				    {(type), (data)}

struct iconv_module {
	int		im_flags;
	void *		im_handle;
	struct iconv_module_desc *im_desc;
	struct iconv_module *im_next;
	struct iconv_module *im_deplist;
	int		im_depcnt;
	const void *	im_methods;
	void *		im_data;
	const void *	im_depdata;	/* data if module loaded from dependency */
	const void *	im_args;

        /* This is module-private data. Nothing outside the module
           itself may touch it. */
        void *im_private;
};

#define	ICONV_MOD_LOAD(mod,ctx)	(mod)->im_desc->imd_event(mod, ICMODEV_LOAD,ctx)
#define	ICONV_MOD_UNLOAD(mod,ctx)	(mod)->im_desc->imd_event(mod, ICMODEV_UNLOAD,ctx)
#define	ICONV_MOD_DYN_LOAD(mod,ctx)	(mod)->im_desc->imd_event(mod, ICMODEV_DYN_LOAD,ctx)
#define	ICONV_MOD_DYN_UNLOAD(mod,ctx)	(mod)->im_desc->imd_event(mod, ICMODEV_DYN_UNLOAD,ctx)

/*
 * iconv converter definitions.
 */
typedef int iconv_open_t(const char *, const char *, void **, apr_pool_t *);
typedef int iconv_close_t(void *, apr_pool_t *);
typedef apr_status_t iconv_conv_t(void *, const unsigned char **, apr_size_t *,
	unsigned char **, apr_size_t *, apr_size_t *);

struct iconv_converter_desc {
	iconv_open_t *	icd_open;
	iconv_close_t *	icd_close;
	iconv_conv_t *	icd_conv;
};

struct iconv_converter {
	struct iconv_converter_desc *ic_desc;
	void *		ic_data;
};

/*
 * internal data types and functions used by charset conversion modules
 */

#define UCS_CHAR_ZERO_WIDTH_NBSP 0xFEFF
#define UCS_CHAR_INVALID	 0xFFFE
#define UCS_CHAR_NONE		 0xFFFF

typedef apr_uint16_t ucs2_t;	/* Unicode character [D5] */
typedef apr_uint32_t ucs4_t;	/* Unicode scalar character [D28] */
#define ucs_t    ucs4_t

/*
 * one-level coded character set conversion tables
 */
typedef struct {
	apr_uint16_t	data[128];
} iconv_ccs_convtable_7bit;	/* 7-bit charset to Unicode */

typedef struct {
	apr_uint16_t	data[256];
} iconv_ccs_convtable_8bit;	/* 8-bit charset to Unicode */

/*
 * two-level coded character set conversion tables
 */
typedef struct {
	const iconv_ccs_convtable_7bit	*data[128];
} iconv_ccs_convtable_14bit;	/* 14-bit charset to Unicode */

typedef struct {
	const iconv_ccs_convtable_8bit	*data[256];
} iconv_ccs_convtable_16bit;	/* 16-bit charset to Unicode;
				 * Unicode to any charset */

/*
 * abstract coded character set conversion table
 */
typedef union {
	iconv_ccs_convtable_7bit	_7bit;
	iconv_ccs_convtable_8bit	_8bit;
	iconv_ccs_convtable_14bit	_14bit;
	iconv_ccs_convtable_16bit	_16bit;
} iconv_ccs_convtable;

/*
 * function for table-driven conversion
 */
typedef ucs2_t iconv_ccs_converter_t (ucs2_t ch);

/*
 * charset conversion module descriptor
 */
struct iconv_ccs_desc {
	const char * const *		names;
	int				nbits;
	const iconv_ccs_convtable *	from_ucs;
	const iconv_ccs_convtable *	to_ucs;
	iconv_ccs_converter_t *		convert_from_ucs;
	iconv_ccs_converter_t *		convert_to_ucs;
};

/*
 * inline functions for use in charset conversion modules
 */
static APR_INLINE ucs2_t
iconv_ccs_convert_7bit(const iconv_ccs_convtable *table, ucs2_t ch)
{
	return ch & 0x80 ? UCS_CHAR_INVALID : table->_7bit.data[ch];
}

static APR_INLINE ucs2_t
iconv_ccs_convert_8bit(const iconv_ccs_convtable *table, ucs2_t ch)
{
	return table->_8bit.data[ch];
}

static APR_INLINE ucs2_t
iconv_ccs_convert_14bit(const iconv_ccs_convtable *table, ucs2_t ch)
{
	const iconv_ccs_convtable_7bit *sub_table;

	sub_table =  ch & 0x8080 ? NULL : table->_14bit.data[ch >> 8];
	return sub_table ? sub_table->data[ch & 0x7F] : UCS_CHAR_INVALID;
}

static APR_INLINE ucs2_t
iconv_ccs_convert_16bit(const iconv_ccs_convtable *table, ucs2_t ch)
{
	const iconv_ccs_convtable_8bit *sub_table;

	sub_table = table->_16bit.data[ch >> 8];
	return sub_table ? sub_table->data[ch & 0xFF] : UCS_CHAR_INVALID;
}

#define ICONV_CCS_CONVERT_FROM_UCS(ccsd, ch) \
        ((ccsd)->convert_from_ucs(ch))
#define ICONV_CCS_CONVERT_TO_UCS(ccsd, ch) \
        ((ccsd)->convert_to_ucs(ch))

/*
 * generic character encoding schemes and functions
 */
struct iconv_ces;

typedef int  iconv_ces_open_t(struct iconv_ces *, apr_pool_t *);
typedef	int  iconv_ces_close_t(struct iconv_ces *);
typedef	void iconv_ces_reset_t(struct iconv_ces *);
typedef	const char * const *iconv_ces_names_t(struct iconv_ces *);
typedef	int  iconv_ces_nbits_t(struct iconv_ces *);
typedef	int  iconv_ces_nbytes_t(struct iconv_ces *);

typedef apr_ssize_t iconv_ces_convert_from_ucs_t
    (struct iconv_ces *data, ucs_t in,
    unsigned char **outbuf, apr_size_t *outbytesleft);

typedef ucs_t iconv_ces_convert_to_ucs_t
    (struct iconv_ces *data,
    const unsigned char **inbuf, apr_size_t *inbytesleft);

struct iconv_ces_desc {
	iconv_ces_open_t *	open;
	iconv_ces_close_t *	close;
	iconv_ces_reset_t *	reset;
	iconv_ces_names_t *	names;
	iconv_ces_nbits_t *	nbits;
	iconv_ces_nbytes_t *	nbytes;
	iconv_ces_convert_from_ucs_t * convert_from_ucs;
	iconv_ces_convert_to_ucs_t * convert_to_ucs;
	const void *data;
};

struct iconv_ces {
	struct iconv_ces_desc *	desc;
	void *			data;
	struct iconv_module *	mod;
};

API_DECLARE_NONSTD(int)  apr_iconv_ces_open(const char *ces_name, struct iconv_ces **cespp, apr_pool_t *ctx);
API_DECLARE_NONSTD(int)  apr_iconv_ces_close(struct iconv_ces *ces, apr_pool_t *ctx);
API_DECLARE_NONSTD(int)  apr_iconv_ces_open_func(struct iconv_ces *ces, apr_pool_t *ctx);
API_DECLARE_NONSTD(int)  apr_iconv_ces_close_func(struct iconv_ces *ces);
API_DECLARE_NONSTD(void) apr_iconv_ces_reset_func(struct iconv_ces *ces);
API_DECLARE_NONSTD(void) apr_iconv_ces_no_func(struct iconv_ces *ces);
API_DECLARE_NONSTD(int)  apr_iconv_ces_nbits7(struct iconv_ces *ces);
API_DECLARE_NONSTD(int)  apr_iconv_ces_nbits8(struct iconv_ces *ces);
API_DECLARE_NONSTD(int)  apr_iconv_ces_zero(struct iconv_ces *ces);

/* Safe for _NONSTD APIs since the C caller pops the stack and the
 * apr_pool_t *cxt arg is simply ignored:
 */
#define apr_iconv_ces_open_zero (iconv_ces_open_t*)apr_iconv_ces_zero

#define iconv_char32bit(ch)	((ch) & 0xFFFF0000)

#define	ICONV_CES_OPEN(ces,ctx)	(ces)->desc->open(ces,ctx)
#define	ICONV_CES_CLOSE(ces)	(ces)->desc->close(ces)
#define ICONV_CES_RESET(ces)	(ces)->desc->reset(ces)
#define ICONV_CES_CONVERT_FROM_UCS(cesd, in, outbuf, outbytes) \
	    ((cesd)->desc->convert_from_ucs((cesd), (in), (outbuf), (outbytes)))
#define ICONV_CES_CONVERT_TO_UCS(cesd, inbuf, inbytes) \
	    ((cesd)->desc->convert_to_ucs((cesd), (inbuf), (inbytes)))

#define ICONV_CES_DRIVER_DECL(name) \
		iconv_ces_open_t iconv_##name##_open; \
		iconv_ces_close_t iconv_##name##_close; \
		iconv_ces_reset_t iconv_##name##_reset; \
		iconv_ces_nbits_t iconv_##name##_nbits; \
		iconv_ces_convert_from_ucs_t iconv_##name##_convert_from_ucs; \
		iconv_ces_convert_to_ucs_t iconv_##name##_convert_to_ucs

/*
 ************************************************
 * EUC character encoding schemes and functions *
 ************************************************
 */

typedef struct iconv_ces_euc_ccs {
	const char	*prefix;
	apr_size_t		prefixlen;
} iconv_ces_euc_ccs_t;

ICONV_CES_DRIVER_DECL(euc);

/*
 * ISO-2022 character encoding schemes and functions
 */
enum { ICONV_SHIFT_SI = 0, ICONV_SHIFT_SO, ICONV_SHIFT_SS2, ICONV_SHIFT_SS3 };

typedef struct iconv_ces_iso2022_ccs {
	int		shift;
	const char *	designator;
	apr_size_t		designatorlen;
} iconv_ces_iso2022_ccs_t;

typedef struct {
	const int *	shift_tab;
	int		nccs;
	const struct iconv_ces_iso2022_ccs *ccs;
} iconv_ces_iso2022_data;

ICONV_CES_DRIVER_DECL(iso2022);


API_DECLARE_NONSTD(int) apr_iconv_mod_load(const char *, int, const void *, struct iconv_module **, apr_pool_t *);
API_DECLARE_NONSTD(int) apr_iconv_mod_unload(struct iconv_module *,apr_pool_t *ctx);
API_DECLARE_NONSTD(int) apr_iconv_mod_noevent(struct iconv_module *mod, int event, apr_pool_t *ctx);
API_DECLARE_NONSTD(int) apr_iconv_ccs_event(struct iconv_module *mod, int event, apr_pool_t *ctx);

int  iconv_malloc(apr_size_t size, void **pp);

extern struct iconv_converter_desc iconv_uc_desc;


API_DECLARE_NONSTD(apr_status_t) apr_iconv_euc_open(struct iconv_ces *ces, apr_pool_t *ctx);
API_DECLARE_NONSTD(apr_status_t) apr_iconv_euc_close(struct iconv_ces *ces);
API_DECLARE_NONSTD(apr_ssize_t)  apr_iconv_euc_convert_from_ucs(struct iconv_ces *ces, ucs_t in,	unsigned char **outbuf, apr_size_t *outbytesleft);
API_DECLARE_NONSTD(ucs_t)  apr_iconv_euc_convert_to_ucs(struct iconv_ces *ces, const unsigned char **inbuf, apr_size_t *inbytesleft);

API_DECLARE_NONSTD(apr_status_t) apr_iconv_iso2022_open(struct iconv_ces *ces, apr_pool_t *ctx);
API_DECLARE_NONSTD(int) apr_iconv_iso2022_close(struct iconv_ces *ces);
API_DECLARE_NONSTD(void) apr_iconv_iso2022_reset(struct iconv_ces *ces);
API_DECLARE_NONSTD(apr_ssize_t) apr_iconv_iso2022_convert_from_ucs(struct iconv_ces *ces, ucs_t in, unsigned char **outbuf, apr_size_t *outbytesleft);
API_DECLARE_NONSTD(ucs_t) apr_iconv_iso2022_convert_to_ucs(struct iconv_ces *ces, const unsigned char **inbuf, apr_size_t *inbytesleft);

#endif /* ICONV_INTERNAL */

#endif /* _ICONV_H_ */
