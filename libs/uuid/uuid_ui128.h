/*
**  OSSP ui128 - 128-Bit Arithmetic
**  Copyright (c) 2002-2005 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2002-2005 The OSSP Project <http://www.ossp.org/>
**
**  This file is part of OSSP ui128, a 128-bit arithmetic library
**  which can be found at http://www.ossp.org/pkg/lib/ui128/.
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
**  ui128.h: API declaration
*/

#ifndef __UI128_H__
#define __UI128_H__

#include <string.h>

#define UI128_PREFIX uuid_

/* embedding support */
#ifdef UI128_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __UI128_CONCAT(x,y) x ## y
#define UI128_CONCAT(x,y) __UI128_CONCAT(x,y)
#else
#define __UI128_CONCAT(x) x
#define UI128_CONCAT(x,y) __UI128_CONCAT(x)y
#endif
#define ui128_t     UI128_CONCAT(UI128_PREFIX,ui128_t)
#define ui128_zero  UI128_CONCAT(UI128_PREFIX,ui128_zero)
#define ui128_max   UI128_CONCAT(UI128_PREFIX,ui128_max)
#define ui128_n2i   UI128_CONCAT(UI128_PREFIX,ui128_n2i)
#define ui128_i2n   UI128_CONCAT(UI128_PREFIX,ui128_i2n)
#define ui128_s2i   UI128_CONCAT(UI128_PREFIX,ui128_s2i)
#define ui128_i2s   UI128_CONCAT(UI128_PREFIX,ui128_i2s)
#define ui128_add   UI128_CONCAT(UI128_PREFIX,ui128_add)
#define ui128_addn  UI128_CONCAT(UI128_PREFIX,ui128_addn)
#define ui128_sub   UI128_CONCAT(UI128_PREFIX,ui128_sub)
#define ui128_subn  UI128_CONCAT(UI128_PREFIX,ui128_subn)
#define ui128_mul   UI128_CONCAT(UI128_PREFIX,ui128_mul)
#define ui128_muln  UI128_CONCAT(UI128_PREFIX,ui128_muln)
#define ui128_div   UI128_CONCAT(UI128_PREFIX,ui128_div)
#define ui128_divn  UI128_CONCAT(UI128_PREFIX,ui128_divn)
#define ui128_and   UI128_CONCAT(UI128_PREFIX,ui128_and)
#define ui128_or    UI128_CONCAT(UI128_PREFIX,ui128_or)
#define ui128_xor   UI128_CONCAT(UI128_PREFIX,ui128_xor)
#define ui128_not   UI128_CONCAT(UI128_PREFIX,ui128_not)
#define ui128_rol   UI128_CONCAT(UI128_PREFIX,ui128_rol)
#define ui128_ror   UI128_CONCAT(UI128_PREFIX,ui128_ror)
#define ui128_len   UI128_CONCAT(UI128_PREFIX,ui128_len)
#define ui128_cmp   UI128_CONCAT(UI128_PREFIX,ui128_cmp)
#endif

typedef struct {
    unsigned char x[16]; /* x_0, ..., x_15 */
} ui128_t;

#define ui128_cons(x15,x14,x13,x12,x11,x10,x9,x8,x7,x6,x5,x4,x3,x2,x1,x0) \
    { { 0x##x0, 0x##x1, 0x##x2,  0x##x3,  0x##x4,  0x##x5,  0x##x6,  0x##x7, \
    { { 0x##x8, 0x##x9, 0x##x10, 0x##x11, 0x##x12, 0x##x13, 0x##x14, 0x##x15 } }

/* particular values */
extern ui128_t        ui128_zero (void);
extern ui128_t        ui128_max  (void);

/* import and export via ISO-C "unsigned long" */
extern ui128_t        ui128_n2i  (unsigned long n);
extern unsigned long  ui128_i2n  (ui128_t x);

/* import and export via ISO-C string of arbitrary base */
extern ui128_t        ui128_s2i  (const char *str, char **end, int base);
extern char *         ui128_i2s  (ui128_t x, char *str, size_t len, int base);

/* arithmetical operations */
extern ui128_t        ui128_add  (ui128_t x, ui128_t y, ui128_t *ov);
extern ui128_t        ui128_addn (ui128_t x, int     y, int     *ov);
extern ui128_t        ui128_sub  (ui128_t x, ui128_t y, ui128_t *ov);
extern ui128_t        ui128_subn (ui128_t x, int     y, int     *ov);
extern ui128_t        ui128_mul  (ui128_t x, ui128_t y, ui128_t *ov);
extern ui128_t        ui128_muln (ui128_t x, int     y, int     *ov);
extern ui128_t        ui128_div  (ui128_t x, ui128_t y, ui128_t *ov);
extern ui128_t        ui128_divn (ui128_t x, int     y, int     *ov);

/* bit operations */
extern ui128_t        ui128_and  (ui128_t x, ui128_t y);
extern ui128_t        ui128_or   (ui128_t x, ui128_t y);
extern ui128_t        ui128_xor  (ui128_t x, ui128_t y);
extern ui128_t        ui128_not  (ui128_t x);
extern ui128_t        ui128_rol  (ui128_t x, int s, ui128_t *ov);
extern ui128_t        ui128_ror  (ui128_t x, int s, ui128_t *ov);

/* other operations */
extern int            ui128_len  (ui128_t x);
extern int            ui128_cmp  (ui128_t x, ui128_t y);

#endif /* __UI128_H__ */

