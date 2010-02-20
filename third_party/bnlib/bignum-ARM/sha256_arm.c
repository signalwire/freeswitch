#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define DEBUG 0

/*
 * For code size reasons, this doesn't even try to support
 * input sizes >= 2^32 bits = 2^29 bytes
 */
struct sha256_state {
	uint32_t iv[8];	/* a, b, c, d, e, f, g, h */
	uint32_t w[64];	/* Fill in first 16 with ntohl(input) */
	uint32_t bytes;
};

/* Rotate right macro.  GCC can usually get this right. */
#define ROTR(x,s) ((x)>>(s) | (x)<<(32-(s)))

#if 1
/*
 * An implementation of SHA-256 for register-starved architectures like
 * x86 or perhaps the MSP430.  (Although the latter's lack of a multi-bit
 * shifter will doom its performance no matter what.)
 * This code is also quite small.
 *
 * If you have 12 32-bit registers to work with, loading the 8 state
 * variables into registers is probably faster.  If you have 28 registers
 * or so, you can put the input block into registers as well.
 *
 * The key idea is to notice that each round consumes one word from the
 * key schedule w[i], computes a new a, and shifts all the other state
 * variables down one position, discarding the old h.
 *
 * So if we store the state vector in reverse order h..a, immediately
 * before w[i], then a single base pointer can be incremented to advance
 * to the next round.
 */
void
sha256_transform(uint32_t p[76])
{
	static uint32_t const k[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
		0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
		0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
		0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
		0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
		0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
		0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
		0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
		0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
		0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
		0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
		0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};
	/*
	 * Look, ma, only 6 local variables including p!
	 * Too bad they're so overloaded it's impossible to give them
	 * meaningful names.
	 */
	register uint32_t const *kp;
	register uint32_t a, s, t, u;

	/* Step 1: Expand the 16 words of w[], at p[8..23] into 64 words */
	for (u = 8; u < 8+64-16; u++) {
		/* w[i] = s1(w[i-2]) + w[i-7] + s0(w[i-15]) + w[i-16] */
		/* Form s0(x) = (x >>> 7) ^ (x >>> 18) ^ (x >> 3) */
		s = t = p[u+1];
		s = ROTR(s, 18-7);
		s ^= t;
		s = ROTR(s, 7);
		s ^= t >> 3;
		/* Form s1(x) = (x >>> 17) ^ (x >>> 19) ^ (x >> 10) */
		a = t = p[u+14];
		a = ROTR(a, 19-17);
		a ^= t;
		a = ROTR(a, 17);
		a ^= t >> 10;

		p[u+16] = s + a + p[u] + p[u+9];
	}

	/* Step 2: Copy the initial values of d, c, b, a out of the way */
	p[72] = p[4];
	p[73] = p[5];
	p[74] = p[6];
	p[75] = a = p[7];

	/*
	 * Step 3: The big loop.
	 * We maintain p[0..7] = h..a, and p[8] is w[i]
	 */
	kp = k;

	do {
		/* T1 = h + S1(e) + Ch(e,f,g) + k[i] + w[i] */
		/* Form Ch(e,f,g) = g ^ (e & (f ^ g)) */
		s = t = p[1];	/* g */
		s ^= p[2];	/* f ^ g */
		s &= u = p[3];	/* e & (f ^ g) */
		s ^= t;
		/* Form S1(e) = (e >>> 6) ^ (e >>> 11) ^ (e >>> 25) */
		t = u;
		u = ROTR(u, 25-11);
		u ^= t;
		u = ROTR(u, 11-6);
		u ^= t;
		u = ROTR(u, 6);
		s += u;
		/* Now add other things to t1 */
		s += p[0] + p[8] + *kp;	/* h + w[i] + kp[i] */
		/* Round function: e = d + T1 */
		p[4] += s;
		/* a = t1 + (t2 = S0(a) + Maj(a,b,c) */
		/* Form S0(a) = (a >>> 2) ^ (a >>> 13) ^ (a >>> 22) */
		t = a;
		t = ROTR(t, 22-13);
		t ^= a;
		t = ROTR(t, 13-2);
		t ^= a;
		t = ROTR(t, 2);
		s += t;
		/* Form Maj(a,b,c) = (a & b) + (c & (a ^ b)) */
		t = a;
		u = p[6];	/* b */
		a ^= u;		/* a ^ b */
		u &= t;		/* a & b */
		a &= p[5];	/* c & (a + b) */
		s += u;
		a += s;	/* Sum final result into a */

		/* Now store new a on top of w[i] and shift... */
		p[8] = a;
		p++;
#if DEBUG 
		/* If debugging, print out the state variables each round */
		printf("%2u:", kp-k);
		for (t = 8; t--; )
			printf(" %08x", p[t]);
		putchar('\n');
#endif
	} while (++kp != k+64);

	/* Now, do the final summation. */
	p -= 64;
	/*
	 * Now, the final h..a are in p[64..71], and the initial values
	 * are in p[0..7].  Except that p[4..7] got trashed in the loop
	 * above, so use the copies we made.
	 */
	p[0] += p[64];
	p[1] += p[65];
	p[2] += p[66];
	p[3] += p[67];
	p[4] = p[68] + p[72];
	p[5] = p[69] + p[73];
	p[6] = p[70] + p[74];
	p[7] = a     + p[75];
}

#else

/* A space-optimized ARM assembly implementation */
void sha256_transform(uint32_t p[8+64]);

#endif

/* Initial values H0..H7 for SHA-256, and SHA-224. */
static uint32_t const sha256_iv[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};
#if 0
static uint32_t const sha224_iv[8] = {
	0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
	0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4
};
#endif

void
sha256_begin(struct sha256_state *s)
{
	memcpy(s->iv, sha256_iv, sizeof sha256_iv);
	s->bytes = 0;
}

#include <netinet/in.h>	/* For ntohl, htonl */

void
sha256_hash(unsigned char const *data, size_t len, struct sha256_state *s)
{
	unsigned space = 64 - (unsigned)s->bytes % 64;
	unsigned i;

	s->bytes += len;

	while (len >= space) {
		memcpy((unsigned char *)s->w + 64 - space, data, space);
		len -= space;
		space = 64;
		for (i = 0; i < 16; i++)
			s->w[i] = ntohl(s->w[i]);
		sha256_transform(s->iv);
	}
	memcpy((unsigned char *)s->w + 64 - space, data, len);
}

void
sha256_end(unsigned char hash[32], struct sha256_state *s)
{
	static unsigned char const padding[64] = { 0x80, 0, 0 /* ,... */ };
	uint32_t bytes = s->bytes;
	unsigned i;

	/* Add trailing bit padding. */
	sha256_hash(padding, 64 - ((bytes+8) & 63), s);
	assert(s->bytes % 64 == 56);

	/* Byte-swap and hash final block */
	for (i = 0; i < 14; i++)
		s->w[i] = ntohl(s->w[i]);
	s->w[14] = 0;	/* We don't even try */
	s->w[15] = s->bytes << 3;
	sha256_transform(s->iv);

	for (i = 0; i < 8; i++)
		s->iv[i] = htonl(s->iv[i]);
	memcpy(hash, s->iv, sizeof s->iv);
	memset(s, 0, sizeof *s);	/* Good cryptographic hygiene */
}

void
sha256(unsigned char hash[32], const unsigned char *data, size_t len)
{
	struct sha256_state s;
	sha256_begin(&s);
	sha256_hash(data, len, &s);
	sha256_end(hash, &s);
}
