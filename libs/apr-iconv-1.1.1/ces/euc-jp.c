/*-
 * Copyright (c) 2000
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
 *	iconv (Charset Conversion Library) v1.0
 */

#define ICONV_INTERNAL
#include <iconv.h>

static const char * const names[] = {
	"euc-jp",
	"Extended_UNIX_Code_Packed_Format_for_Japanese",
	"csEUCPkdFmtJapanese",
	NULL
};

static const char * const *
euc_names(struct iconv_ces *ces)
{
	return names;
}

static struct iconv_ces_euc_ccs ccsattr[4] = {
	{"", 0},
	{"", 0},
	{"\x8e", 1},
	{"\x8f", 1}
};

static const struct iconv_module_depend iconv_module_depend[] = {
	{ICMOD_UC_CCS, "us-ascii", ccsattr + 0},
	{ICMOD_UC_CCS, "jis_x0208-1983", ccsattr + 1},
	{ICMOD_UC_CCS, "jis_x0201", ccsattr + 2},
	{ICMOD_UC_CCS, "jis_x0212-1990", ccsattr + 3},
        END_ICONV_MODULE_DEPEND
};

static const struct iconv_ces_desc iconv_ces_desc = {
	apr_iconv_euc_open,
	apr_iconv_euc_close,
	apr_iconv_ces_no_func,
	euc_names,
	apr_iconv_ces_nbits8,
	apr_iconv_ces_zero,
	apr_iconv_euc_convert_from_ucs,
	apr_iconv_euc_convert_to_ucs,
};

struct iconv_module_desc iconv_module = {
	ICMOD_UC_CES,
	apr_iconv_mod_noevent,
	iconv_module_depend,
	&iconv_ces_desc
};
