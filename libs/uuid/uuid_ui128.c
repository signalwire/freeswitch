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
**  ui128.c: implementation of 128-bit unsigned integer arithmetic
*/

/* own headers (part 1/2) */
#include "uuid_ac.h"

/* system headers */
#include <string.h>
#include <ctype.h>

/* own headers (part 2/2) */
#include "uuid_ui128.h"

#define UI128_BASE   256 /* 2^8 */
#define UI128_DIGITS 16  /* 8*16 = 128 bit */
#define UIXX_T(n) struct { unsigned char x[n]; }

/* fill an ui128_t with a sequence of a particular digit */
#define ui128_fill(__x, __n) \
    /*lint -save -e717*/ \
    do { int __i; \
      for (__i = 0; __i < UI128_DIGITS; __i++) \
          (__x).x[__i] = (__n); \
    } while (0) \
    /*lint -restore*/

/* the value zero */
ui128_t ui128_zero(void)
{
    ui128_t z;

    ui128_fill(z, 0);
    return z;
}

/* the maximum value */
ui128_t ui128_max(void)
{
    ui128_t z;

    ui128_fill(z, UI128_BASE-1);
    return z;
}

/* convert ISO-C "unsigned long" into internal format */
ui128_t ui128_n2i(unsigned long n)
{
    ui128_t z;
    int i;

    i = 0;
    do {
        z.x[i++] = (n % UI128_BASE);
    } while ((n /= UI128_BASE) > 0 && i < UI128_DIGITS);
    for ( ; i < UI128_DIGITS; i++)
        z.x[i] = 0;
    return z;
}

/* convert internal format into ISO-C "unsigned long";
   truncates if sizeof(unsigned long) is less than UI128_DIGITS! */
unsigned long ui128_i2n(ui128_t x)
{
    unsigned long n;
    int i;

    n = 0;
    i = (int)sizeof(n);
    /*lint -save -e774*/
    if (i > UI128_DIGITS)
        i = UI128_DIGITS;
    /*lint -restore*/
    while (--i >= 0) {
        n = (n * UI128_BASE) + x.x[i];
    }
    return n;
}

/* convert string representation of arbitrary base into internal format */
ui128_t ui128_s2i(const char *str, char **end, int base)
{
    ui128_t z;
    const char *cp;
    int carry;
    static char map[] = {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9,             /* 0...9 */
        36, 36, 36, 36, 36, 36, 36,                         /* illegal chars */
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, /* A...M */
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, /* N...Z */
        36, 36, 36, 36, 36, 36,                             /* illegal chars */
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, /* a...m */
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35  /* m...z */
    };

    ui128_fill(z, 0);
    if (str == NULL || (base < 2 || base > 36))
        return z;
    cp = str;
    while (*cp != '\0' && isspace((int)(*cp)))
        cp++;
    while (   *cp != '\0'
           && isalnum((int)(*cp))
           && map[(int)(*cp)-'0'] < base) {
        z = ui128_muln(z, base, &carry);
        if (carry)
            break;
        z = ui128_addn(z, map[(int)(*cp)-'0'], &carry);
        if (carry)
            break;
        cp++;
    }
    if (end != NULL)
        *end = (char *)cp;
    return z;
}

/* convert internal format into string representation of arbitrary base */
char *ui128_i2s(ui128_t x, char *str, size_t len, int base)
{
    static char map[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char c;
    int r;
    int n;
    int i, j;

    if (str == NULL || len < 2 || (base < 2 || base > 36))
        return NULL;
    n = ui128_len(x);
    i = 0;
    do {
        x = ui128_divn(x, base, &r);
        str[i++] = map[r];
        while (n > 1 && x.x[n-1] == 0)
            n--;
    } while (i < ((int)len-1) && (n > 1 || x.x[0] != 0));
    str[i] = '\0';
    for (j = 0; j < --i; j++) {
        c = str[j];
        str[j] = str[i];
        str[i] = c;
    }
    return str;
}

/* addition of two ui128_t */
ui128_t ui128_add(ui128_t x, ui128_t y, ui128_t *ov)
{
    ui128_t z;
    int carry;
    int i;

    carry = 0;
    for (i = 0; i < UI128_DIGITS; i++) {
        carry += (x.x[i] + y.x[i]);
        z.x[i] = (carry % UI128_BASE);
        carry /= UI128_BASE;
    }
    if (ov != NULL)
        *ov = ui128_n2i((unsigned long)carry);
    return z;
}

/* addition of an ui128_t and a single digit */
ui128_t ui128_addn(ui128_t x, int y, int *ov)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++) {
        y += x.x[i];
        z.x[i] = (y % UI128_BASE);
        y /= UI128_BASE;
    }
    if (ov != NULL)
        *ov = y;
    return z;
}

/* subtraction of two ui128_t */
ui128_t ui128_sub(ui128_t x, ui128_t y, ui128_t *ov)
{
    ui128_t z;
    int borrow;
    int i;
    int d;

    borrow = 0;
    for (i = 0; i < UI128_DIGITS; i++) {
        d = ((x.x[i] + UI128_BASE) - borrow - y.x[i]);
        z.x[i] = (d % UI128_BASE);
        borrow = (1 - (d/UI128_BASE));
    }
    if (ov != NULL)
        *ov = ui128_n2i((unsigned long)borrow);
    return z;
}

/* subtraction of an ui128_t and a single digit */
ui128_t ui128_subn(ui128_t x, int y, int *ov)
{
    ui128_t z;
    int i;
    int d;

    for (i = 0; i < UI128_DIGITS; i++) {
        d = (x.x[i] + UI128_BASE) - y;
        z.x[i] = (d % UI128_BASE);
        y = (1 - (d/UI128_BASE));
    }
    if (ov != NULL)
        *ov = y;
    return z;
}

/*
             7 3 2
         * 9 4 2 8
         ---------
           5 8 5 6
   +     1 4 6 4
   +   2 9 2 8
   + 6 5 8 8
   ---------------
   = 6 9 0 1 2 9 6
*/

ui128_t ui128_mul(ui128_t x, ui128_t y, ui128_t *ov)
{
    UIXX_T(UI128_DIGITS+UI128_DIGITS) zx;
    ui128_t z;
    int carry;
    int i, j;

    /* clear temporary result buffer */
    for (i = 0; i < (UI128_DIGITS+UI128_DIGITS); i++)
        zx.x[i] = 0;

    /* perform multiplication operation */
    for (i = 0; i < UI128_DIGITS; i++) {
        /* calculate partial product and immediately add to z */
        carry = 0;
        for (j = 0; j < UI128_DIGITS; j++) {
            carry += (x.x[i] * y.x[j]) + zx.x[i+j];
            zx.x[i+j] = (carry % UI128_BASE);
            carry /= UI128_BASE;
        }
        /* add carry to remaining digits in z */
        for ( ; j < UI128_DIGITS + UI128_DIGITS - i; j++) {
            carry += zx.x[i+j];
            zx.x[i+j] = (carry % UI128_BASE);
            carry /= UI128_BASE;
        }
    }

    /* provide result by splitting zx into z and ov */
    memcpy(z.x, zx.x, UI128_DIGITS);
    if (ov != NULL)
        memcpy((*ov).x, &zx.x[UI128_DIGITS], UI128_DIGITS);

    return z;
}

ui128_t ui128_muln(ui128_t x, int y, int *ov)
{
    ui128_t z;
    int carry;
    int i;

    carry = 0;
    for (i = 0; i < UI128_DIGITS; i++) {
        carry += (x.x[i] * y);
        z.x[i] = (carry % UI128_BASE);
        carry /= UI128_BASE;
    }
    if (ov != NULL)
        *ov = carry;
    return z;
}

/*
  =   2078 [q]
   0615367 [x] : 296 [y]
  -0592    [dq]
  -----
  = 0233
   -0000   [dq]
   -----
   = 2336
    -2072  [dq]
    -----
    = 2647
     -2308 [dq]
     -----
     = 279 [r]
 */
ui128_t ui128_div(ui128_t x, ui128_t y, ui128_t *ov)
{
    ui128_t q;
    ui128_t r;
    int i;
    int n, m;
    int ovn;

    /* determine actual number of involved digits */
    n = ui128_len(x);
    m = ui128_len(y);

    if (m == 1) {
        /* simple case #1: reduceable to ui128_divn() */
        if (y.x[0] == 0) {
            /* error case: division by zero! */
            ui128_fill(q, 0);
            ui128_fill(r, 0);
        }
        else {
            q = ui128_divn(x, y.x[0], &ovn);
            ui128_fill(r, 0);
            r.x[0] = (unsigned char)ovn;
        }

    } else if (n < m) {
        /* simple case #2: everything is in the remainder */
        ui128_fill(q, 0);
        r = x;

    } else { /* n >= m, m > 1 */
        /* standard case: x[0..n] / y[0..m] */
        UIXX_T(UI128_DIGITS+1) rx;
        UIXX_T(UI128_DIGITS+1) dq;
        ui128_t t;
        int km;
        int k;
        int qk;
        unsigned long y2;
        unsigned long r3;
        int borrow;
        int d;

        /* rx is x with a leading zero in order to make
           sure that n > m and not just n >= m */
        memcpy(rx.x, x.x, UI128_DIGITS);
        rx.x[UI128_DIGITS] = 0;

        for (k = n - m; k >= 0; k--) {
            /* efficiently compute qk by guessing
               qk := rx[k+m-2...k+m]/y[m-2...m-1] */
            km = k + m;
            y2 = (y.x[m-1]*UI128_BASE) + y.x[m-2];
            r3 = (rx.x[km]*(UI128_BASE*UI128_BASE)) +
                 (rx.x[km-1]*UI128_BASE) + rx.x[km-2];
            qk = r3 / y2;
            if (qk >= UI128_BASE)
                qk = UI128_BASE - 1;

            /* dq := y*qk (post-adjust qk if guessed incorrectly) */
            t = ui128_muln(y, qk, &ovn);
            memcpy(dq.x, t.x, UI128_DIGITS);
            dq.x[m] = (unsigned char)ovn;
            for (i = m; i > 0; i--)
                if (rx.x[i+k] != dq.x[i])
                    break;
            if (rx.x[i+k] < dq.x[i]) {
                t = ui128_muln(y, --qk, &ovn);
                memcpy(dq.x, t.x, UI128_DIGITS);
                dq.x[m] = (unsigned char)ovn;
            }

            /* store qk */
            q.x[k] = (unsigned char)qk;

            /* rx := rx - dq*(b^k) */
            borrow = 0;
            for (i = 0; i < m+1; i++) {
                d = ((rx.x[k+i] + UI128_BASE) - borrow - dq.x[i]);
                rx.x[k+i] = (d % UI128_BASE);
                borrow = (1 - (d/UI128_BASE));
            }
        }
        memcpy(r.x, rx.x, m);

        /* fill out results with leading zeros */
        for (i = n-m+1; i < UI128_DIGITS; i++)
            q.x[i] = 0;
        for (i = m; i < UI128_DIGITS; i++)
            r.x[i] = 0;
    }

    /* provide results */
    if (ov != NULL)
        *ov = r;
    return q;
}

ui128_t ui128_divn(ui128_t x, int y, int *ov)
{
    ui128_t z;
    unsigned int carry;
    int i;

    carry = 0;
    for (i = (UI128_DIGITS - 1); i >= 0; i--) {
        carry = (carry * UI128_BASE) + x.x[i];
        z.x[i] = (carry / y);
        carry %= y;
    }
    if (ov != NULL)
        *ov = carry;
    return z;
}

ui128_t ui128_and(ui128_t x, ui128_t y)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++)
        z.x[i] = (x.x[i] & y.x[i]);
    return z;
}

ui128_t ui128_or(ui128_t x, ui128_t y)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++)
        z.x[i] = (x.x[i] | y.x[i]);
    return z;
}

ui128_t ui128_xor(ui128_t x, ui128_t y)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++)
        z.x[i] = ((x.x[i] & ~(y.x[i])) | (~(x.x[i]) & (y.x[i])));
    return z;
}

ui128_t ui128_not(ui128_t x)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++)
        z.x[i] = ~(x.x[i]);
    return z;
}

ui128_t ui128_rol(ui128_t x, int s, ui128_t *ov)
{
    UIXX_T(UI128_DIGITS+UI128_DIGITS) zx;
    ui128_t z;
    int i;
    int carry;

    if (s <= 0) {
        /* no shift at all */
        if (ov != NULL)
            *ov = ui128_zero();
        return x;
    }
    else if (s > 128) {
        /* too large shift */
        if (ov != NULL)
            *ov = ui128_zero();
        return ui128_zero();
    }
    else if (s == 128) {
        /* maximum shift */
        if (ov != NULL)
            *ov = x;
        return ui128_zero();
    }
    else { /* regular shift */
        /* shift (logically) left by s/8 bytes */
        for (i = 0; i < UI128_DIGITS+UI128_DIGITS; i++)
            zx.x[i] = 0;
        for (i = 0; i < UI128_DIGITS; i++)
            zx.x[i+(s/8)] = x.x[i];
        /* shift (logically) left by remaining s%8 bits */
        s %= 8;
        if (s > 0) {
            carry = 0;
            for (i = 0; i < UI128_DIGITS+UI128_DIGITS; i++) {
                carry += (zx.x[i] * (1 << s));
                zx.x[i] = (carry % UI128_BASE);
                carry /= UI128_BASE;
            }
        }
        memcpy(z.x, zx.x, UI128_DIGITS);
        if (ov != NULL)
            memcpy((*ov).x, &zx.x[UI128_DIGITS], UI128_DIGITS);
    }
    return z;
}

ui128_t ui128_ror(ui128_t x, int s, ui128_t *ov)
{
    UIXX_T(UI128_DIGITS+UI128_DIGITS) zx;
    ui128_t z;
    int i;
    int carry;

    if (s <= 0) {
        /* no shift at all */
        if (ov != NULL)
            *ov = ui128_zero();
        return x;
    }
    else if (s > 128) {
        /* too large shift */
        if (ov != NULL)
            *ov = ui128_zero();
        return ui128_zero();
    }
    else if (s == 128) {
        /* maximum shift */
        if (ov != NULL)
            *ov = x;
        return ui128_zero();
    }
    else { /* regular shift */
        /* shift (logically) right by s/8 bytes */
        for (i = 0; i < UI128_DIGITS+UI128_DIGITS; i++)
            zx.x[i] = 0;
        for (i = 0; i < UI128_DIGITS; i++)
            zx.x[UI128_DIGITS+i-(s/8)] = x.x[i];
        /* shift (logically) right by remaining s%8 bits */
        s %= 8;
        if (s > 0) {
            carry = 0;
            for (i = (UI128_DIGITS+UI128_DIGITS - 1); i >= 0; i--) {
                carry = (carry * UI128_BASE) + zx.x[i];
                zx.x[i] = (carry / (1 << s));
                carry %= (1 << s);
            }
        }
        memcpy(z.x, &zx.x[UI128_DIGITS], UI128_DIGITS);
        if (ov != NULL)
            memcpy((*ov).x, zx.x, UI128_DIGITS);
    }
    return z;
}

int ui128_cmp(ui128_t x, ui128_t y)
{
    int i;

    i = UI128_DIGITS - 1;
    while (i > 0 && x.x[i] == y.x[i])
        i--;
    return (x.x[i] - y.x[i]);
}

int ui128_len(ui128_t x)
{
    int i;

    for (i = UI128_DIGITS; i > 1 && x.x[i-1] == 0; i--)
        ;
    return i;
}

