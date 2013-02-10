/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 */

#include "zrtp.h"

/* We don't have digital signatures ready yet. */
#if 0

/* Size of extra random data to approximate a uniform distribution mod n */
#define UNIFORMBYTES	8

/*============================================================================*/
/*    Shared Elliptic Curve functions                                         */
/*                                                                            */
/*    The Elliptic Curve DSA algorithm, key generation, and curves are        */
/*    from FIPS 186-3.  The curves used are                                   */
/*    also defined in RFC 4753, sections 3.1 through 3.3.                     */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Return dsa_cc->pv holding public value and dsa_cc->sv holding secret value */
/* The public value is an elliptic curve point encoded as the x part shifted  */
/* left Pbits bits and or'd with the y part.                                  */
/*----------------------------------------------------------------------------*/
static zrtp_status_t ECDSA_keygen( struct zrtp_sig_scheme *self,
									zrtp_dsa_crypto_context_t *dsa_cc,
									zrtp_ec_params_t *ec_params,
#ifdef ZRTP_TEST_VECTORS
									uint8_t *sv_data, size_t sv_data_len,
									uint8_t *pvx_data, size_t pvx_data_len,
									uint8_t *pvy_data, size_t pvy_data_len,
#endif
									unsigned Pbits )
{
	zrtp_status_t s = zrtp_status_fail;
	struct BigNum P, Gx, Gy, n;
	struct BigNum pkx, pky;
	unsigned ec_bytes;

	if (!ec_params)
		return zrtp_status_bad_param;

	ec_bytes = (ec_params->ec_bits+7) / 8;

	do
	{
	if (!self || !dsa_cc)
	{
		s = zrtp_status_bad_param;
		break;
	}

    bnBegin(&P);
    bnInsertBigBytes( &P, ec_params->P_data, 0, ec_bytes );
    bnBegin(&Gx);
    bnInsertBigBytes( &Gx, ec_params->Gx_data, 0, ec_bytes );
    bnBegin(&Gy);
    bnInsertBigBytes( &Gy, ec_params->Gy_data, 0, ec_bytes );
    bnBegin(&n);
    bnInsertBigBytes( &n, ec_params->n_data, 0, ec_bytes );

	bnBegin(&pkx);
	bnBegin(&pky);
	bnBegin(&dsa_cc->sv);
	s = zrtp_ec_random_point( self->base.zrtp_global, &P, &n, &Gx, &Gy,
#ifdef ZRTP_TEST_VECTORS
							  sv_data, sv_data_len,
							  pvx_data, pvx_data_len,
							  pvy_data, pvy_data_len,
#endif
							  &pkx, &pky, &dsa_cc->sv );
	if ( s != zrtp_status_ok )
		break;
	s = zrtp_status_fail;

	bnBegin(&dsa_cc->pv);
	bnCopy (&dsa_cc->pv, &pkx);
	bnLShift (&dsa_cc->pv, Pbits);
	bnAdd (&dsa_cc->pv, &pky);
	bnEnd (&pkx);
	bnEnd (&pky);
	bnEnd (&P);
	bnEnd (&Gx);
	bnEnd (&Gy);
	bnEnd (&n);

	s = zrtp_status_ok;
	} while (0);

	return s;
}


/*----------------------------------------------------------------------------*/
/* Sign the specified hash value - must be size matching the curve            */
/*----------------------------------------------------------------------------*/
static zrtp_status_t ECDSA_sign( struct zrtp_sig_scheme *self,
											  zrtp_dsa_crypto_context_t *dsa_cc,
											  zrtp_ec_params_t *ec_params,
#ifdef ZRTP_TEST_VECTORS
											  uint8_t *k_data, size_t k_data_len,
											  uint8_t *rx_data, size_t rx_data_len,
											  uint8_t *ry_data, size_t ry_data_len,
											  uint8_t *s_data, size_t s_data_len,
#endif
											  uint8_t *hash, uint32_t hash_len,
											  struct BigNum *dsasig )
{
	zrtp_status_t s = zrtp_status_fail;
	struct BigNum P, Gx, Gy, n;
	struct BigNum h, s1, k, rx, ry, kinv, pkx, pky;
	unsigned ec_bytes;
	
	if (!ec_params)
		return zrtp_status_bad_param;
	
	ec_bytes = (ec_params->ec_bits+7) / 8;

	do
	{
	if (!self || !dsa_cc)
	{
		s = zrtp_status_bad_param;
		break;
	}

    bnBegin(&P);
    bnInsertBigBytes( &P, ec_params->P_data, 0, ec_bytes );
    bnBegin(&Gx);
    bnInsertBigBytes( &Gx, ec_params->Gx_data, 0, ec_bytes );
    bnBegin(&Gy);
    bnInsertBigBytes( &Gy, ec_params->Gy_data, 0, ec_bytes );
    bnBegin(&n);
    bnInsertBigBytes( &n, ec_params->n_data, 0, ec_bytes );

	/* Hash to bignum */
    bnBegin(&h);
    bnInsertBigBytes( &h, hash, 0, hash_len );
	bnMod (&h, &h, &P);

	/* Unpack signing key */
	bnBegin(&pkx);
	bnBegin(&pky);
	bnSetQ (&pkx, 1);
	bnLShift (&pkx, ec_bytes*8);
	bnMod (&pky, &dsa_cc->pv, &pkx);
	bnCopy (&pkx, &dsa_cc->pv);
	bnRShift (&pkx, ec_bytes*8);

	/* Choose signature secret k value */
    bnBegin(&rx);
    bnBegin(&ry);
    bnBegin(&k);
	s = zrtp_ec_random_point( self->base.zrtp_global, &P, &n, &Gx, &Gy,
#ifdef ZRTP_TEST_VECTORS
							  k_data, k_data_len,
							  rx_data, rx_data_len,
							  ry_data, ry_data_len,
#endif
							  &rx, &ry, &k );
	if ( s != zrtp_status_ok )
		break;
	s = zrtp_status_fail;

#ifndef ZRTP_TEST_VECTORS
	/* For further randomness we are going to add the secret key to k */
	bnAddMod_ (&k, &dsa_cc->sv, &n);
	zrtp_ecAdd (&rx, &ry, &rx, &ry, &pkx, &pky, &P);
#endif

	/* Perform the signature */
	bnBegin (&s1);
	bnMulMod_ (&s1, &rx, &dsa_cc->sv, &n);
	bnAddMod_ (&s1, &h, &n);
	bnBegin (&kinv);
	bnInv (&kinv, &k, &n);
	bnMulMod_ (&s1, &s1, &kinv, &n);

#ifdef ZRTP_TEST_VECTORS
	if (k_data_len != 0)
	{
		/* rx is checked in ec_random_point */
		struct BigNum s2;
		int ok;
		bnBegin(&s2);
		bnInsertBigBytes(&s2, s_data, 0, s_data_len);
		ok = (bnCmp (&s1, &s2) == 0);
		bnEnd(&s2);
		if (!ok)
			break;
	}
#endif

	/* Combine r, s into dsasig */
	bnBegin(dsasig);
	bnCopy (dsasig, &rx);
	bnLShift (dsasig, ec_bytes*8);
	bnAdd (dsasig, &s1);
	bnEnd (&rx);
	bnEnd (&ry);
	bnEnd (&k);
	bnEnd (&kinv);
	bnEnd (&s1);
	bnEnd (&h);
	bnEnd (&pkx);
	bnEnd (&pky);
	bnEnd (&P);
	bnEnd (&Gx);
	bnEnd (&Gy);
	bnEnd (&n);

	s = zrtp_status_ok;
	} while (0);

	return s;
}


/*----------------------------------------------------------------------------*/
/* Verify a signature value - hash must be size matching the curve            */
/* Signing key should be in peer_pv entry of dsa_cc                           */
/*----------------------------------------------------------------------------*/
static zrtp_status_t ECDSA_verify( struct zrtp_sig_scheme *self,
											  zrtp_dsa_crypto_context_t *dsa_cc,
											  zrtp_ec_params_t *ec_params,
											  uint8_t *hash, uint32_t hash_len,
											  struct BigNum *dsasig )
{
	zrtp_status_t s = zrtp_status_fail;
	struct BigNum P, Gx, Gy, n;
	struct BigNum rx, ry, pkx, pky, r, s1, sinv, u1, u2, u1x, u2x, u1y, u2y, h;
	unsigned ec_bytes;
	
	if (!ec_params)
		return zrtp_status_bad_param;
		
	ec_bytes = (ec_params->ec_bits+7) / 8;

	do
	{
	if (!self || !dsa_cc)
	{
		s = zrtp_status_bad_param;
		break;
	}

    bnBegin(&P);
    bnInsertBigBytes( &P, ec_params->P_data, 0, ec_bytes );
    bnBegin(&Gx);
    bnInsertBigBytes( &Gx, ec_params->Gx_data, 0, ec_bytes );
    bnBegin(&Gy);
    bnInsertBigBytes( &Gy, ec_params->Gy_data, 0, ec_bytes );
    bnBegin(&n);
    bnInsertBigBytes( &n, ec_params->n_data, 0, ec_bytes );

	/* hash */
    bnBegin(&h);
    bnInsertBigBytes( &h, hash, 0, hash_len );
	bnMod (&h, &h, &P);

	/* Unpack sig */
	bnBegin(&r);
	bnBegin(&s1);
	bnSetQ (&r, 1);
	bnLShift (&r, ec_bytes*8);
	bnMod (&s1, dsasig, &r);
	bnCopy (&r, dsasig);
	bnRShift (&r, ec_bytes*8);

	/* Unpack signing key */
	bnBegin(&pkx);
	bnBegin(&pky);
	bnSetQ (&pkx, 1);
	bnLShift (&pkx, ec_bytes*8);
	bnMod (&pky, &dsa_cc->peer_pv, &pkx);
	bnCopy (&pkx, &dsa_cc->peer_pv);
	bnRShift (&pkx, ec_bytes*8);

	/* Verify signature */
	bnBegin (&sinv);
	bnInv (&sinv, &s1, &n);
	bnBegin (&u1);
	bnBegin (&u2);
	bnMulMod_ (&u1, &sinv, &h, &n);
	bnMulMod_ (&u2, &sinv, &r, &n);

	bnBegin (&u1x);
	bnBegin (&u1y);
	bnBegin (&u2x);
	bnBegin (&u2y);
	bnBegin (&rx);
	bnBegin (&ry);
	zrtp_ecMul (&u1x, &u1y, &u1, &Gx, &Gy, &P);
	zrtp_ecMul (&u2x, &u2y, &u2, &pkx, &pky, &P);
	zrtp_ecAdd (&rx, &ry, &u1x, &u1y, &u2x, &u2y, &P);

	if (bnCmp (&rx, &r) == 0) {
		s = zrtp_status_ok;
	} else {
		s = zrtp_status_fail;
	}

	/* Clean up */
	bnEnd (&rx);
	bnEnd (&ry);
	bnEnd (&r);
	bnEnd (&s1);
	bnEnd (&sinv);
	bnEnd (&u1);
	bnEnd (&u1x);
	bnEnd (&u1y);
	bnEnd (&u2);
	bnEnd (&u2x);
	bnEnd (&u2y);
	bnEnd (&h);
	bnEnd (&pkx);
	bnEnd (&pky);
	bnEnd (&P);
	bnEnd (&Gx);
	bnEnd (&Gy);
	bnEnd (&n);

	} while (0);

	return s;
}



/*----------------------------------------------------------------------------*/
static zrtp_status_t EC_dummy(void *s)
{
    return zrtp_status_ok;
}


/*============================================================================*/
/*    P-256 (FIPS 186-3) support.  See RFC 4753, section 3.1.				  */
/*============================================================================*/

/* Test vectors from RFC4754 */
#ifdef ZRTP_TEST_VECTORS
static uint8_t sv256_data[] = {
	0xDC, 0x51, 0xD3, 0x86, 0x6A, 0x15, 0xBA, 0xCD,
	0xE3, 0x3D, 0x96, 0xF9, 0x92, 0xFC, 0xA9, 0x9D,
	0xA7, 0xE6, 0xEF, 0x09, 0x34, 0xE7, 0x09, 0x75,
	0x59, 0xC2, 0x7F, 0x16, 0x14, 0xC8, 0x8A, 0x7F,
};
static uint8_t pvx256_data[] = {
	0x24, 0x42, 0xA5, 0xCC, 0x0E, 0xCD, 0x01, 0x5F,
	0xA3, 0xCA, 0x31, 0xDC, 0x8E, 0x2B, 0xBC, 0x70,
	0xBF, 0x42, 0xD6, 0x0C, 0xBC, 0xA2, 0x00, 0x85,
	0xE0, 0x82, 0x2C, 0xB0, 0x42, 0x35, 0xE9, 0x70,
};
static uint8_t pvy256_data[] = {
	0x6F, 0xC9, 0x8B, 0xD7, 0xE5, 0x02, 0x11, 0xA4,
	0xA2, 0x71, 0x02, 0xFA, 0x35, 0x49, 0xDF, 0x79,
	0xEB, 0xCB, 0x4B, 0xF2, 0x46, 0xB8, 0x09, 0x45,
	0xCD, 0xDF, 0xE7, 0xD5, 0x09, 0xBB, 0xFD, 0x7D,
};

static uint8_t k256_data[] = {
	0x9E, 0x56, 0xF5, 0x09, 0x19, 0x67, 0x84, 0xD9,
	0x63, 0xD1, 0xC0, 0xA4, 0x01, 0x51, 0x0E, 0xE7,
	0xAD, 0xA3, 0xDC, 0xC5, 0xDE, 0xE0, 0x4B, 0x15,
	0x4B, 0xF6, 0x1A, 0xF1, 0xD5, 0xA6, 0xDE, 0xCE,
};
static uint8_t rx256_data[] = {
	0xCB, 0x28, 0xE0, 0x99, 0x9B, 0x9C, 0x77, 0x15,
	0xFD, 0x0A, 0x80, 0xD8, 0xE4, 0x7A, 0x77, 0x07,
	0x97, 0x16, 0xCB, 0xBF, 0x91, 0x7D, 0xD7, 0x2E,
	0x97, 0x56, 0x6E, 0xA1, 0xC0, 0x66, 0x95, 0x7C,
};
static uint8_t ry256_data[] = {
	0x2B, 0x57, 0xC0, 0x23, 0x5F, 0xB7, 0x48, 0x97,
	0x68, 0xD0, 0x58, 0xFF, 0x49, 0x11, 0xC2, 0x0F,
	0xDB, 0xE7, 0x1E, 0x36, 0x99, 0xD9, 0x13, 0x39,
	0xAF, 0xBB, 0x90, 0x3E, 0xE1, 0x72, 0x55, 0xDC,
};

static uint8_t h256_data[] = {
	0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
	0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
	0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
	0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD,
};
static uint8_t s256_data[] = {
	0x86, 0xFA, 0x3B, 0xB4, 0xE2, 0x6C, 0xAD, 0x5B,
	0xF9, 0x0B, 0x7F, 0x81, 0x89, 0x92, 0x56, 0xCE,
	0x75, 0x94, 0xBB, 0x1E, 0xA0, 0xC8, 0x92, 0x12,
	0x74, 0x8B, 0xFF, 0x3B, 0x3D, 0x5B, 0x03, 0x15,
};


#endif

/*----------------------------------------------------------------------------*/
/* Return dsa_cc->pv holding public value and dsa_cc->sv holding secret value   */
/* The public value is an elliptic curve point encoded as the x part shifted  */
/* left 256 bits and or'd with the y part.                                    */
/*----------------------------------------------------------------------------*/
static zrtp_status_t EC256P_keygen( struct zrtp_sig_scheme *self,
									    zrtp_dsa_crypto_context_t *dsa_cc )
{
	struct zrtp_ec_params params;
	zrtp_ec_init_params(&params, 256);
	return ECDSA_keygen(self, dsa_cc, &params,
#ifdef ZRTP_TEST_VECTORS
		sv256_data, sizeof(sv256_data),
		pvx256_data, sizeof(pvx256_data),
		pvy256_data, sizeof(pvy256_data),
#endif
		256);
}


/*----------------------------------------------------------------------------*/
/* Sign the specified hash value                                              */
/*----------------------------------------------------------------------------*/
static zrtp_status_t EC256P_sign( struct zrtp_sig_scheme *self,
											  zrtp_dsa_crypto_context_t *dsa_cc,
											  uint8_t *hash, uint32_t hash_len,
											  struct BigNum *dsasig )
{
	struct zrtp_ec_params params;
	zrtp_ec_init_params(&params, 256);
	return ECDSA_sign(self, dsa_cc, &params,
#ifdef ZRTP_TEST_VECTORS
		k256_data, sizeof(k256_data),
		rx256_data, sizeof(rx256_data),
		ry256_data, sizeof(ry256_data),
		s256_data, sizeof(s256_data),
		h256_data, sizeof(h256_data),
#else
		hash, hash_len,
#endif
	dsasig);
}


/*----------------------------------------------------------------------------*/
/* Verify the signature on the hash value                                     */
/*----------------------------------------------------------------------------*/
static zrtp_status_t EC256P_verify(struct zrtp_sig_scheme *self,
											  zrtp_dsa_crypto_context_t *dsa_cc,
											  uint8_t *hash, uint32_t hash_len,
											  struct BigNum *dsasig )
{
	struct zrtp_ec_params params;
	zrtp_ec_init_params(&params, 256);
	return ECDSA_verify(self, dsa_cc, &params,
#ifdef ZRTP_TEST_VECTORS
		h256_data, sizeof(h256_data),
#else
		hash, hash_len,
#endif
		dsasig);
}



/*============================================================================*/
/*    P-384 (FIPS 186-3) support.  See RFC 4753, section 3.2.				  */
/*============================================================================*/



/*----------------------------------------------------------------------------*/
/* Return dsa_cc->pv holding public value and dsa_cc->sv holding secret value   */
/* The public value is an elliptic curve point encoded as the x part shifted  */
/* left 384 bits and or'd with the y part.                                    */
/*----------------------------------------------------------------------------*/
static zrtp_status_t EC384P_keygen( struct zrtp_sig_scheme *self,
								    zrtp_dsa_crypto_context_t *dsa_cc )
{
	struct zrtp_ec_params params;
	zrtp_ec_init_params(&params, 384);
	return ECDSA_keygen(self, dsa_cc, &params,
#ifdef ZRTP_TEST_VECTORS
		0, 0, 0, 0, 0, 0,
#endif
		384);
}


/*----------------------------------------------------------------------------*/
/* Sign the specified hash value                                              */
/*----------------------------------------------------------------------------*/
static zrtp_status_t EC384P_sign( struct zrtp_sig_scheme *self,
											  zrtp_dsa_crypto_context_t *dsa_cc,
											  uint8_t *hash, uint32_t hash_len,
											  struct BigNum *dsasig )
{
	struct zrtp_ec_params params;
	zrtp_ec_init_params(&params, 384);
	return ECDSA_sign(self, dsa_cc, &params,
#ifdef ZRTP_TEST_VECTORS
		0, 0, 0, 0, 0, 0, 0, 0,
#endif
		hash, hash_len, dsasig);
}


/*----------------------------------------------------------------------------*/
/* Verify the signature on the hash value                                     */
/*----------------------------------------------------------------------------*/
static zrtp_status_t EC384P_verify(struct zrtp_sig_scheme *self,
											  zrtp_dsa_crypto_context_t *dsa_cc,
											  uint8_t *hash, uint32_t hash_len,
											  struct BigNum *dsasig )
{
	struct zrtp_ec_params params;
	zrtp_ec_init_params(&params, 384);
	return ECDSA_verify(self, dsa_cc, &params, hash, hash_len, dsasig);
}



/*============================================================================*/
/*    P-521 (FIPS 186-3) support.  See RFC 4753, section 3.3.				  */
/*============================================================================*/


/*----------------------------------------------------------------------------*/
/* Return dsa_cc->pv holding public value and dsa_cc->sv holding secret value   */
/* The public value is an elliptic curve point encoded as the x part shifted  */
/* left 528 bits (note, not 521) and or'd with the y part.                    */
/*----------------------------------------------------------------------------*/
static zrtp_status_t EC521P_keygen( struct zrtp_sig_scheme *self,
									    zrtp_dsa_crypto_context_t *dsa_cc )
{
	struct zrtp_ec_params params;
	zrtp_ec_init_params(&params, 521);
	return ECDSA_keygen(self, dsa_cc, &params,
#ifdef ZRTP_TEST_VECTORS
		0, 0, 0, 0, 0, 0,
#endif
		528);
}


/*----------------------------------------------------------------------------*/
/* Sign the specified hash value                                              */
/*----------------------------------------------------------------------------*/
static zrtp_status_t EC521P_sign( struct zrtp_sig_scheme *self,
											  zrtp_dsa_crypto_context_t *dsa_cc,
											  uint8_t *hash, uint32_t hash_len,
											  struct BigNum *dsasig )
{
	struct zrtp_ec_params params;
	zrtp_ec_init_params(&params, 521);
	return ECDSA_sign(self, dsa_cc, &params,
#ifdef ZRTP_TEST_VECTORS
		0, 0, 0, 0, 0, 0, 0, 0,
#endif
		hash, hash_len, dsasig);
}


/*----------------------------------------------------------------------------*/
/* Verify the signature on the hash value                                     */
/*----------------------------------------------------------------------------*/
static zrtp_status_t EC521P_verify(struct zrtp_sig_scheme *self,
											  zrtp_dsa_crypto_context_t *dsa_cc,
											  uint8_t *hash, uint32_t hash_len,
											  struct BigNum *dsasig )
{
	struct zrtp_ec_params params;
	zrtp_ec_init_params(&params, 521);
	return ECDSA_verify(self, dsa_cc, &params, hash, hash_len, dsasig);
}



/*============================================================================*/
/*    Public Key support													  */
/*============================================================================*/


/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_defaults_sig(zrtp_global_ctx_t* zrtp_global)
{
    zrtp_sig_scheme_t* ec256p = zrtp_sys_alloc(sizeof(zrtp_sig_scheme_t));
    zrtp_sig_scheme_t* ec384p = zrtp_sys_alloc(sizeof(zrtp_sig_scheme_t));
    zrtp_sig_scheme_t* ec521p = zrtp_sys_alloc(sizeof(zrtp_sig_scheme_t));
    
	if (!ec256p || !ec384p || !ec521p)
	{
		if(ec256p) zrtp_sys_free(ec256p);
		if(ec384p) zrtp_sys_free(ec384p);
		if(ec521p) zrtp_sys_free(ec521p);
		return zrtp_status_alloc_fail;
	}

    zrtp_memset(ec256p, 0, sizeof(zrtp_sig_scheme_t));
    zrtp_memcpy(ec256p->base.type, ZRTP_EC256P, ZRTP_COMP_TYPE_SIZE);
	ec256p->base.id				= ZRTP_SIGTYPE_EC256P;
    ec256p->base.zrtp_global	= zrtp_global;
    ec256p->sv_length			= 256/8;
    ec256p->pv_length			= 2*256/8;
    ec256p->base.init 			= EC_dummy;
    ec256p->base.free			= EC_dummy;
    ec256p->generate_key		= EC256P_keygen;
    ec256p->sign				= EC256P_sign;
    ec256p->verify				= EC256P_verify;

    zrtp_memset(ec384p, 0, sizeof(zrtp_sig_scheme_t));
    zrtp_memcpy(ec384p->base.type, ZRTP_EC384P, ZRTP_COMP_TYPE_SIZE);
	ec384p->base.id				= ZRTP_SIGTYPE_EC384P;
    ec384p->base.zrtp_global	= zrtp_global;
    ec384p->sv_length			= 384/8;
    ec384p->pv_length			= 2*384/8;
    ec384p->base.init 			= EC_dummy;
    ec384p->base.free			= EC_dummy;
    ec384p->generate_key		= EC384P_keygen;
    ec384p->sign				= EC384P_sign;
    ec384p->verify				= EC384P_verify;

    zrtp_memset(ec521p, 0, sizeof(zrtp_sig_scheme_t));
    zrtp_memcpy(ec521p->base.type, ZRTP_EC521P, ZRTP_COMP_TYPE_SIZE);
	ec521p->base.id				= ZRTP_SIGTYPE_EC521P;
    ec521p->base.zrtp_global	= zrtp_global;
    ec521p->sv_length			= 528/8;
    ec521p->pv_length			= 2*528/8;
    ec521p->base.init 			= EC_dummy;
    ec521p->base.free			= EC_dummy;
    ec521p->generate_key		= EC521P_keygen;
    ec521p->sign				= EC521P_sign;
    ec521p->verify				= EC521P_verify;

    zrtp_register_comp(ZRTP_CC_SIG, ec256p, zrtp_global);
    zrtp_register_comp(ZRTP_CC_SIG, ec384p, zrtp_global);
    zrtp_register_comp(ZRTP_CC_SIG, ec521p, zrtp_global);

    return zrtp_status_ok;
}

#endif /* don't have disgital signature ready for the moment*/
