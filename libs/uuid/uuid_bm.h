/*
**  OSSP uuid - Universally Unique Identifier
**  Copyright (c) 2004-2008 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2004-2008 The OSSP Project <http://www.ossp.org/>
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
**  uuid_bm.c: bitmask API implementation
*/

#ifndef __UUID_BM_H__
#define __UUID_BM_H__

/*
 *  Bitmask Calculation Macros (up to 32 bit only)
 *  (Notice: bit positions are counted n...0, i.e. lowest bit is position 0)
 */

/* generate a bitmask consisting of 1 bits from (and including)
   bit position `l' (left) to (and including) bit position `r' */
#define BM_MASK(l,r) \
    ((((unsigned int)1<<(((l)-(r))+1))-1)<<(r))

/* extract a value v from a word w at position `l' to `r' and return value */
#define BM_GET(w,l,r) \
    (((w)>>(r))&BM_MASK((l)-(r),0))

/* insert a value v into a word w at position `l' to `r' and return word */
#define BM_SET(w,l,r,v) \
    ((w)|(((v)&BM_MASK((l)-(r),0))<<(r)))

/* generate a single bit `b' (0 or 1) at bit position `n' */
#define BM_BIT(n,b) \
    ((b)<<(n))

/* generate a quad word octet of bits (a half byte, i.e. bit positions 3 to 0) */
#define BM_QUAD(b3,b2,b1,b0) \
    (BM_BIT(3,(b3))|BM_BIT(2,(b2))|BM_BIT(1,(b1))|BM_BIT(0,(b0)))

/* generate an octet word of bits (a byte, i.e. bit positions 7 to 0) */
#define BM_OCTET(b7,b6,b5,b4,b3,b2,b1,b0) \
    ((BM_QUAD(b7,b6,b5,b4)<<4)|BM_QUAD(b3,b2,b1,b0))

/* generate the value 2^n */
#define BM_POW2(n) \
    BM_BIT(n,1)

/* shift word w k bits to the left or to the right */
#define BM_SHL(w,k) \
    ((w)<<(k))
#define BM_SHR(w,k) \
    ((w)>>(k))

/* rotate word w (of bits n..0) k bits to the left or to the right */
#define BM_ROL(w,n,k) \
    ((BM_SHL((w),(k))&BM_MASK(n,0))|BM_SHR(((w)&BM_MASK(n,0)),(n)-(k)))
#define BM_ROR(w,n,k) \
    ((BM_SHR(((w)&BM_MASK(n,0)),(k)))|BM_SHL(((w),(n)-(k))&BM_MASK(n,0)))

#endif /* __UUID_BM_H__ */

