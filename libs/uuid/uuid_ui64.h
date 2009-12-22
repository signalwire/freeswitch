/*
**  OSSP ui64 - 64-Bit Arithmetic
**  Copyright (c) 2002-2005 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2002-2005 The OSSP Project <http://www.ossp.org/>
**
**  This file is part of OSSP ui64, a 64-bit arithmetic library
**  which can be found at http://www.ossp.org/pkg/lib/ui64/.
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
**  ui64.h: API declaration
*/

#ifndef __UI64_H__
#define __UI64_H__

#include <string.h>

#define UI64_PREFIX uuid_

/* embedding support */
#ifdef UI64_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __UI64_CONCAT(x,y) x ## y
#define UI64_CONCAT(x,y) __UI64_CONCAT(x,y)
#else
#define __UI64_CONCAT(x) x
#define UI64_CONCAT(x,y) __UI64_CONCAT(x)y
#endif
#define ui64_t     UI64_CONCAT(UI64_PREFIX,ui64_t)
#define ui64_zero  UI64_CONCAT(UI64_PREFIX,ui64_zero)
#define ui64_max   UI64_CONCAT(UI64_PREFIX,ui64_max)
#define ui64_n2i   UI64_CONCAT(UI64_PREFIX,ui64_n2i)
#define ui64_i2n   UI64_CONCAT(UI64_PREFIX,ui64_i2n)
#define ui64_s2i   UI64_CONCAT(UI64_PREFIX,ui64_s2i)
#define ui64_i2s   UI64_CONCAT(UI64_PREFIX,ui64_i2s)
#define ui64_add   UI64_CONCAT(UI64_PREFIX,ui64_add)
#define ui64_addn  UI64_CONCAT(UI64_PREFIX,ui64_addn)
#define ui64_sub   UI64_CONCAT(UI64_PREFIX,ui64_sub)
#define ui64_subn  UI64_CONCAT(UI64_PREFIX,ui64_subn)
#define ui64_mul   UI64_CONCAT(UI64_PREFIX,ui64_mul)
#define ui64_muln  UI64_CONCAT(UI64_PREFIX,ui64_muln)
#define ui64_div   UI64_CONCAT(UI64_PREFIX,ui64_div)
#define ui64_divn  UI64_CONCAT(UI64_PREFIX,ui64_divn)
#define ui64_and   UI64_CONCAT(UI64_PREFIX,ui64_and)
#define ui64_or    UI64_CONCAT(UI64_PREFIX,ui64_or)
#define ui64_xor   UI64_CONCAT(UI64_PREFIX,ui64_xor)
#define ui64_not   UI64_CONCAT(UI64_PREFIX,ui64_not)
#define ui64_rol   UI64_CONCAT(UI64_PREFIX,ui64_rol)
#define ui64_ror   UI64_CONCAT(UI64_PREFIX,ui64_ror)
#define ui64_len   UI64_CONCAT(UI64_PREFIX,ui64_len)
#define ui64_cmp   UI64_CONCAT(UI64_PREFIX,ui64_cmp)
#endif

typedef struct {
    unsigned char x[8]; /* x_0, ..., x_7 */
} ui64_t;

#define ui64_cons(x7,x6,x5,x4,x3,x2,x1,x0) \
    { { 0x##x0, 0x##x1, 0x##x2, 0x##x3, 0x##x4, 0x##x5, 0x##x6, 0x##x7 } }

/* particular values */
extern ui64_t        ui64_zero (void);
extern ui64_t        ui64_max  (void);

/* import and export via ISO-C "unsigned long" */
extern ui64_t        ui64_n2i  (unsigned long n);
extern unsigned long ui64_i2n  (ui64_t x);

/* import and export via ISO-C string of arbitrary base */
extern ui64_t        ui64_s2i  (const char *str, char **end, int base);
extern char *        ui64_i2s  (ui64_t x, char *str, size_t len, int base);

/* arithmetical operations */
extern ui64_t        ui64_add  (ui64_t x, ui64_t y, ui64_t *ov);
extern ui64_t        ui64_addn (ui64_t x, int    y, int    *ov);
extern ui64_t        ui64_sub  (ui64_t x, ui64_t y, ui64_t *ov);
extern ui64_t        ui64_subn (ui64_t x, int    y, int    *ov);
extern ui64_t        ui64_mul  (ui64_t x, ui64_t y, ui64_t *ov);
extern ui64_t        ui64_muln (ui64_t x, int    y, int    *ov);
extern ui64_t        ui64_div  (ui64_t x, ui64_t y, ui64_t *ov);
extern ui64_t        ui64_divn (ui64_t x, int    y, int    *ov);

/* bit operations */
extern ui64_t        ui64_and  (ui64_t x, ui64_t y);
extern ui64_t        ui64_or   (ui64_t x, ui64_t y);
extern ui64_t        ui64_xor  (ui64_t x, ui64_t y);
extern ui64_t        ui64_not  (ui64_t x);
extern ui64_t        ui64_rol  (ui64_t x, int s, ui64_t *ov);
extern ui64_t        ui64_ror  (ui64_t x, int s, ui64_t *ov);

/* other operations */
extern int           ui64_len  (ui64_t x);
extern int           ui64_cmp  (ui64_t x, ui64_t y);

#endif /* __UI64_H__ */

