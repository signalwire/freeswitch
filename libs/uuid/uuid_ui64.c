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
**  ui64.c: implementation of 64-bit unsigned integer arithmetic
*/

/* own headers (part 1/2) */
#include "uuid_ac.h"

/* system headers */
#include <string.h>
#include <ctype.h>

/* own headers (part 2/2) */
#include "uuid_ui64.h"

#define UI64_BASE   256 /* 2^8 */
#define UI64_DIGITS 8   /* 8*8 = 64 bit */
#define UIXX_T(n) struct { unsigned char x[n]; }

/* fill an ui64_t with a sequence of a particular digit */
#define ui64_fill(__x, __n) \
    /*lint -save -e717*/ \
    do { int __i; \
      for (__i = 0; __i < UI64_DIGITS; __i++) \
          (__x).x[__i] = (__n); \
    } while (0) \
    /*lint -restore*/

/* the value zero */
ui64_t ui64_zero(void)
{
    ui64_t z;

    ui64_fill(z, 0);
    return z;
}

/* the maximum value */
ui64_t ui64_max(void)
{
    ui64_t z;

    ui64_fill(z, UI64_BASE-1);
    return z;
}

/* convert ISO-C "unsigned long" into internal format */
ui64_t ui64_n2i(unsigned long n)
{
    ui64_t z;
    int i;

    i = 0;
    do {
        z.x[i++] = (n % UI64_BASE);
    } while ((n /= UI64_BASE) > 0 && i < UI64_DIGITS);
    for ( ; i < UI64_DIGITS; i++)
        z.x[i] = 0;
    return z;
}

/* convert internal format into ISO-C "unsigned long";
   truncates if sizeof(unsigned long) is less than UI64_DIGITS! */
unsigned long ui64_i2n(ui64_t x)
{
    unsigned long n;
    int i;

    n = 0;
    i = (int)sizeof(n);
    /*lint -save -e774*/
    if (i > UI64_DIGITS)
        i = UI64_DIGITS;
    /*lint -restore*/
    while (--i >= 0) {
        n = (n * UI64_BASE) + x.x[i];
    }
    return n;
}

/* convert string representation of arbitrary base into internal format */
ui64_t ui64_s2i(const char *str, char **end, int base)
{
    ui64_t z;
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

    ui64_fill(z, 0);
    if (str == NULL || (base < 2 || base > 36))
        return z;
    cp = str;
    while (*cp != '\0' && isspace((int)(*cp)))
        cp++;
    while (   *cp != '\0'
           && isalnum((int)(*cp))
           && map[(int)(*cp)-'0'] < base) {
        z = ui64_muln(z, base, &carry);
        if (carry)
            break;
        z = ui64_addn(z, map[(int)(*cp)-'0'], &carry);
        if (carry)
            break;
        cp++;
    }
    if (end != NULL)
        *end = (char *)cp;
    return z;
}

/* convert internal format into string representation of arbitrary base */
char *ui64_i2s(ui64_t x, char *str, size_t len, int base)
{
    static char map[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char c;
    int r;
    int n;
    int i, j;

    if (str == NULL || len < 2 || (base < 2 || base > 36))
        return NULL;
    n = ui64_len(x);
    i = 0;
    do {
        x = ui64_divn(x, base, &r);
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

/* addition of two ui64_t */
ui64_t ui64_add(ui64_t x, ui64_t y, ui64_t *ov)
{
    ui64_t z;
    int carry;
    int i;

    carry = 0;
    for (i = 0; i < UI64_DIGITS; i++) {
        carry += (x.x[i] + y.x[i]);
        z.x[i] = (carry % UI64_BASE);
        carry /= UI64_BASE;
    }
    if (ov != NULL)
        *ov = ui64_n2i((unsigned long)carry);
    return z;
}

/* addition of an ui64_t and a single digit */
ui64_t ui64_addn(ui64_t x, int y, int *ov)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++) {
        y += x.x[i];
        z.x[i] = (y % UI64_BASE);
        y /= UI64_BASE;
    }
    if (ov != NULL)
        *ov = y;
    return z;
}

/* subtraction of two ui64_t */
ui64_t ui64_sub(ui64_t x, ui64_t y, ui64_t *ov)
{
    ui64_t z;
    int borrow;
    int i;
    int d;

    borrow = 0;
    for (i = 0; i < UI64_DIGITS; i++) {
        d = ((x.x[i] + UI64_BASE) - borrow - y.x[i]);
        z.x[i] = (d % UI64_BASE);
        borrow = (1 - (d/UI64_BASE));
    }
    if (ov != NULL)
        *ov = ui64_n2i((unsigned long)borrow);
    return z;
}

/* subtraction of an ui64_t and a single digit */
ui64_t ui64_subn(ui64_t x, int y, int *ov)
{
    ui64_t z;
    int i;
    int d;

    for (i = 0; i < UI64_DIGITS; i++) {
        d = (x.x[i] + UI64_BASE) - y;
        z.x[i] = (d % UI64_BASE);
        y = (1 - (d/UI64_BASE));
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

ui64_t ui64_mul(ui64_t x, ui64_t y, ui64_t *ov)
{
    UIXX_T(UI64_DIGITS+UI64_DIGITS) zx;
    ui64_t z;
    int carry;
    int i, j;

    /* clear temporary result buffer */
    for (i = 0; i < (UI64_DIGITS+UI64_DIGITS); i++)
        zx.x[i] = 0;

    /* perform multiplication operation */
    for (i = 0; i < UI64_DIGITS; i++) {
        /* calculate partial product and immediately add to z */
        carry = 0;
        for (j = 0; j < UI64_DIGITS; j++) {
            carry += (x.x[i] * y.x[j]) + zx.x[i+j];
            zx.x[i+j] = (carry % UI64_BASE);
            carry /= UI64_BASE;
        }
        /* add carry to remaining digits in z */
        for ( ; j < UI64_DIGITS + UI64_DIGITS - i; j++) {
            carry += zx.x[i+j];
            zx.x[i+j] = (carry % UI64_BASE);
            carry /= UI64_BASE;
        }
    }

    /* provide result by splitting zx into z and ov */
    memcpy(z.x, zx.x, UI64_DIGITS);
    if (ov != NULL)
        memcpy((*ov).x, &zx.x[UI64_DIGITS], UI64_DIGITS);

    return z;
}

ui64_t ui64_muln(ui64_t x, int y, int *ov)
{
    ui64_t z;
    int carry;
    int i;

    carry = 0;
    for (i = 0; i < UI64_DIGITS; i++) {
        carry += (x.x[i] * y);
        z.x[i] = (carry % UI64_BASE);
        carry /= UI64_BASE;
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
ui64_t ui64_div(ui64_t x, ui64_t y, ui64_t *ov)
{
    ui64_t q;
    ui64_t r;
    int i;
    int n, m;
    int ovn;

    /* determine actual number of involved digits */
    n = ui64_len(x);
    m = ui64_len(y);

    if (m == 1) {
        /* simple case #1: reduceable to ui64_divn() */
        if (y.x[0] == 0) {
            /* error case: division by zero! */
            ui64_fill(q, 0);
            ui64_fill(r, 0);
        }
        else {
            q = ui64_divn(x, y.x[0], &ovn);
            ui64_fill(r, 0);
            r.x[0] = (unsigned char)ovn;
        }

    } else if (n < m) {
        /* simple case #2: everything is in the remainder */
        ui64_fill(q, 0);
        r = x;

    } else { /* n >= m, m > 1 */
        /* standard case: x[0..n] / y[0..m] */
        UIXX_T(UI64_DIGITS+1) rx;
        UIXX_T(UI64_DIGITS+1) dq;
        ui64_t t;
        int km;
        int k;
        int qk;
        unsigned long y2;
        unsigned long r3;
        int borrow;
        int d;

        /* rx is x with a leading zero in order to make
           sure that n > m and not just n >= m */
        memcpy(rx.x, x.x, UI64_DIGITS);
        rx.x[UI64_DIGITS] = 0;

        for (k = n - m; k >= 0; k--) {
            /* efficiently compute qk by guessing
               qk := rx[k+m-2...k+m]/y[m-2...m-1] */
            km = k + m;
            y2 = (y.x[m-1]*UI64_BASE) + y.x[m-2];
            r3 = (rx.x[km]*(UI64_BASE*UI64_BASE)) +
                 (rx.x[km-1]*UI64_BASE) + rx.x[km-2];
            qk = r3 / y2;
            if (qk >= UI64_BASE)
                qk = UI64_BASE - 1;

            /* dq := y*qk (post-adjust qk if guessed incorrectly) */
            t = ui64_muln(y, qk, &ovn);
            memcpy(dq.x, t.x, UI64_DIGITS);
            dq.x[m] = (unsigned char)ovn;
            for (i = m; i > 0; i--)
                if (rx.x[i+k] != dq.x[i])
                    break;
            if (rx.x[i+k] < dq.x[i]) {
                t = ui64_muln(y, --qk, &ovn);
                memcpy(dq.x, t.x, UI64_DIGITS);
                dq.x[m] = (unsigned char)ovn;
            }

            /* store qk */
            q.x[k] = (unsigned char)qk;

            /* rx := rx - dq*(b^k) */
            borrow = 0;
            for (i = 0; i < m+1; i++) {
                d = ((rx.x[k+i] + UI64_BASE) - borrow - dq.x[i]);
                rx.x[k+i] = (d % UI64_BASE);
                borrow = (1 - (d/UI64_BASE));
            }
        }
        memcpy(r.x, rx.x, m);

        /* fill out results with leading zeros */
        for (i = n-m+1; i < UI64_DIGITS; i++)
            q.x[i] = 0;
        for (i = m; i < UI64_DIGITS; i++)
            r.x[i] = 0;
    }

    /* provide results */
    if (ov != NULL)
        *ov = r;
    return q;
}

ui64_t ui64_divn(ui64_t x, int y, int *ov)
{
    ui64_t z;
    unsigned int carry;
    int i;

    carry = 0;
    for (i = (UI64_DIGITS - 1); i >= 0; i--) {
        carry = (carry * UI64_BASE) + x.x[i];
        z.x[i] = (carry / y);
        carry %= y;
    }
    if (ov != NULL)
        *ov = carry;
    return z;
}

ui64_t ui64_and(ui64_t x, ui64_t y)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++)
        z.x[i] = (x.x[i] & y.x[i]);
    return z;
}

ui64_t ui64_or(ui64_t x, ui64_t y)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++)
        z.x[i] = (x.x[i] | y.x[i]);
    return z;
}

ui64_t ui64_xor(ui64_t x, ui64_t y)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++)
        z.x[i] = ((x.x[i] & ~(y.x[i])) | (~(x.x[i]) & (y.x[i])));
    return z;
}

ui64_t ui64_not(ui64_t x)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++)
        z.x[i] = ~(x.x[i]);
    return z;
}

ui64_t ui64_rol(ui64_t x, int s, ui64_t *ov)
{
    UIXX_T(UI64_DIGITS+UI64_DIGITS) zx;
    ui64_t z;
    int i;
    int carry;

    if (s <= 0) {
        /* no shift at all */
        if (ov != NULL)
            *ov = ui64_zero();
        return x;
    }
    else if (s > 64) {
        /* too large shift */
        if (ov != NULL)
            *ov = ui64_zero();
        return ui64_zero();
    }
    else if (s == 64) {
        /* maximum shift */
        if (ov != NULL)
            *ov = x;
        return ui64_zero();
    }
    else { /* regular shift */
        /* shift (logically) left by s/8 bytes */
        for (i = 0; i < UI64_DIGITS+UI64_DIGITS; i++)
            zx.x[i] = 0;
        for (i = 0; i < UI64_DIGITS; i++)
            zx.x[i+(s/8)] = x.x[i];
        /* shift (logically) left by remaining s%8 bits */
        s %= 8;
        if (s > 0) {
            carry = 0;
            for (i = 0; i < UI64_DIGITS+UI64_DIGITS; i++) {
                carry += (zx.x[i] * (1 << s));
                zx.x[i] = (carry % UI64_BASE);
                carry /= UI64_BASE;
            }
        }
        memcpy(z.x, zx.x, UI64_DIGITS);
        if (ov != NULL)
            memcpy((*ov).x, &zx.x[UI64_DIGITS], UI64_DIGITS);
    }
    return z;
}

ui64_t ui64_ror(ui64_t x, int s, ui64_t *ov)
{
    UIXX_T(UI64_DIGITS+UI64_DIGITS) zx;
    ui64_t z;
    int i;
    int carry;

    if (s <= 0) {
        /* no shift at all */
        if (ov != NULL)
            *ov = ui64_zero();
        return x;
    }
    else if (s > 64) {
        /* too large shift */
        if (ov != NULL)
            *ov = ui64_zero();
        return ui64_zero();
    }
    else if (s == 64) {
        /* maximum shift */
        if (ov != NULL)
            *ov = x;
        return ui64_zero();
    }
    else { /* regular shift */
        /* shift (logically) right by s/8 bytes */
        for (i = 0; i < UI64_DIGITS+UI64_DIGITS; i++)
            zx.x[i] = 0;
        for (i = 0; i < UI64_DIGITS; i++)
            zx.x[UI64_DIGITS+i-(s/8)] = x.x[i];
        /* shift (logically) right by remaining s%8 bits */
        s %= 8;
        if (s > 0) {
            carry = 0;
            for (i = (UI64_DIGITS+UI64_DIGITS - 1); i >= 0; i--) {
                carry = (carry * UI64_BASE) + zx.x[i];
                zx.x[i] = (carry / (1 << s));
                carry %= (1 << s);
            }
        }
        memcpy(z.x, &zx.x[UI64_DIGITS], UI64_DIGITS);
        if (ov != NULL)
            memcpy((*ov).x, zx.x, UI64_DIGITS);
    }
    return z;
}

int ui64_cmp(ui64_t x, ui64_t y)
{
    int i;

    i = UI64_DIGITS - 1;
    while (i > 0 && x.x[i] == y.x[i])
        i--;
    return (x.x[i] - y.x[i]);
}

int ui64_len(ui64_t x)
{
    int i;

    for (i = UI64_DIGITS; i > 1 && x.x[i-1] == 0; i--)
        ;
    return i;
}

