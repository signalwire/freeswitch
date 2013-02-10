/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 */

#include "zrtp.h"


#define _ZTU_ "zrtp ecdh"

static unsigned get_pbits(zrtp_pk_scheme_t *self)
{
	switch (self->base.id) {
		case ZRTP_PKTYPE_EC256P:
			return 256;
			break;
		case ZRTP_PKTYPE_EC384P:
			return 384;
			break;
		case ZRTP_PKTYPE_EC521P:
			return 521;
			break;
		default:
			return 0;
	}
}

/*============================================================================*/
/*    Shared Elliptic Curve functions                                         */
/*                                                                            */
/*    The Elliptic Curve DH algorithm and key generation is from              */
/*    NIST SP 800-56A.  The curves used are from NSA Suite B, which           */
/*    uses the same curves as ECDSA defined by FIPS 186-3, and are            */
/*    also defined in RFC 4753, sections 3.1 through 3.3.                     */
/*    The validation procedures are from NIST SP 800-56A section 5.6.2.6,     */
/*    method 3, ECC Partial Validation.                                       */
/*============================================================================*/


/*----------------------------------------------------------------------------*/
static zrtp_status_t zrtp_ecdh_init(void *s) {
    return zrtp_status_ok;
}

static zrtp_status_t zrtp_ecdh_free(void *s) {
    return zrtp_status_ok;
}


/*----------------------------------------------------------------------------*/
/* Return dh_cc->pv holding public value and dh_cc->sv holding secret value   */
/* The public value is an elliptic curve point encoded as the x part shifted  */
/* left Pbits bits and or'd with the y part.                                  */
/*----------------------------------------------------------------------------*/
static zrtp_status_t zrtp_ecdh_initialize( zrtp_pk_scheme_t *self,
										   zrtp_dh_crypto_context_t *dh_cc)
{
	zrtp_status_t s = zrtp_status_fail;
	struct BigNum P, Gx, Gy, n;
	struct BigNum pkx, pky;	
	unsigned ec_bytes = 0;
	unsigned pbits = 0;
	struct zrtp_ec_params ec_params;
	zrtp_time_t start_ts = zrtp_time_now();
	
	if (!self || !dh_cc) {
		return zrtp_status_bad_param;
	}
	
	pbits = get_pbits(self);
	if (!pbits) {
		return zrtp_status_bad_param;
	}
	
	zrtp_ec_init_params(&ec_params, pbits);
	
	ec_bytes = (ec_params.ec_bits+7) / 8;

	bnBegin(&P);
	bnInsertBigBytes(&P, ec_params.P_data, 0, ec_bytes );
	bnBegin(&Gx);
	bnInsertBigBytes(&Gx, ec_params.Gx_data, 0, ec_bytes );
	bnBegin(&Gy);
	bnInsertBigBytes(&Gy, ec_params.Gy_data, 0, ec_bytes );
	bnBegin(&n);
	bnInsertBigBytes(&n, ec_params.n_data, 0, ec_bytes );

	bnBegin(&pkx);
	bnBegin(&pky);
	bnBegin(&dh_cc->sv);
	s = zrtp_ec_random_point( self->base.zrtp, &P, &n, &Gx, &Gy,
							  &pkx, &pky, &dh_cc->sv,
							  NULL, 0);
		
	if (zrtp_status_ok == s)
	{
		bnBegin(&dh_cc->pv);
		bnCopy (&dh_cc->pv, &pkx);
		bnLShift (&dh_cc->pv, pbits);
		bnAdd (&dh_cc->pv, &pky);
	}
		
	bnEnd (&pkx);
	bnEnd (&pky);
	bnEnd (&P);
	bnEnd (&Gx);
	bnEnd (&Gy);
	bnEnd (&n);
	
	ZRTP_LOG(1,(_ZTU_,"\tDH TEST: zrtp_ecdh_initialize() for %.4s was executed by %llums.\n", self->base.type, zrtp_time_now()-start_ts));
	return s;
}


/*----------------------------------------------------------------------------*/
/* Compute the shared dhresult as the X coordinate of the EC point.           */
/*----------------------------------------------------------------------------*/
static zrtp_status_t zrtp_ecdh_compute( zrtp_pk_scheme_t *self,
										zrtp_dh_crypto_context_t *dh_cc,										
										struct BigNum *dhresult,
										struct BigNum *pv)
{
	struct BigNum P;
	struct BigNum pkx, pky, rsltx, rslty;
	unsigned ec_bytes = 0;
	unsigned pbits = 0;
	struct zrtp_ec_params ec_params;
	zrtp_time_t start_ts = zrtp_time_now();
	
	if (!self || !dh_cc || !dhresult || !pv) {
		return zrtp_status_bad_param;
	}
	
	pbits = get_pbits(self);
	if (!pbits) {
		return zrtp_status_bad_param;
	}
	
	zrtp_ec_init_params(&ec_params, pbits);
	
	ec_bytes = (ec_params.ec_bits+7) / 8;
	
    bnBegin(&P);
    bnInsertBigBytes( &P, ec_params.P_data, 0, ec_bytes );

	bnBegin (&pkx);
	bnBegin (&pky);
	bnBegin (&rsltx);
	bnBegin (&rslty);

	bnSetQ (&pkx, 1);
	bnLShift (&pkx, pbits);
	bnMod (&pky, pv, &pkx);
	bnCopy (&pkx, pv);
	bnRShift (&pkx, pbits);

	zrtp_ecMul (&rsltx, &rslty, &dh_cc->sv, &pkx, &pky, &P);
	bnCopy (dhresult, &rsltx);

	bnEnd (&pkx);
	bnEnd (&pky);
	bnEnd (&rsltx);
	bnEnd (&rslty);
	bnEnd (&P);
    
	ZRTP_LOG(1,(_ZTU_,"\tDH TEST: zrtp_ecdh_compute() for %.4s was executed by %llums.\n", self->base.type, zrtp_time_now()-start_ts));
    return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
/* ECC Partial Validation per NIST SP800-56A section 5.6.2.6                  */
/*----------------------------------------------------------------------------*/
static zrtp_status_t zrtp_ecdh_validate( zrtp_pk_scheme_t *self,
										 struct BigNum *pv)
{
	zrtp_status_t s = zrtp_status_fail;
	struct BigNum P, b;
	struct BigNum t1, t2;
	struct BigNum pkx, pky, bnzero;
	unsigned ec_bytes = 0;
	unsigned pbits = 0;
	struct zrtp_ec_params ec_params;
	zrtp_time_t start_ts = zrtp_time_now();
	
	if (!self || !pv) {
		return zrtp_status_bad_param;
	}
	
	pbits = get_pbits(self);
	if (!pbits) {
		return zrtp_status_bad_param;
	}
	
	zrtp_ec_init_params(&ec_params, pbits);
	
	ec_bytes = (ec_params.ec_bits+7) / 8;

    bnBegin(&P);
    bnInsertBigBytes( &P, ec_params.P_data, 0, ec_bytes );
    bnBegin(&b);
    bnInsertBigBytes( &b, ec_params.b_data, 0, ec_bytes );

	bnBegin (&t1);
	bnBegin (&t2);
	bnBegin (&pkx);
	bnBegin (&pky);
	bnBegin (&bnzero);

	bnSetQ (&pkx, 1);
	bnLShift (&pkx, pbits);
	bnMod (&pky, pv, &pkx);
	bnCopy (&pkx, pv);
	bnRShift (&pkx, pbits);

	do{
	/* Represent point at infinity by (0, 0), make sure it's not that */
		if (bnCmp (&pkx, &bnzero) == 0 && bnCmp (&pky, &bnzero) == 0) {
			break;
		}
	/* Check coordinates within range */
		if (bnCmp (&pkx, &bnzero) < 0 || bnCmp (&pkx, &P) >= 0) {
			break;
		}
		if (bnCmp (&pky, &bnzero) < 0 || bnCmp (&pky, &P) >= 0) {
			break;
		}

		/* Check that point satisfies EC equation y^2 = x^3 - 3x + b, mod P */
		bnSquareMod_ (&t1, &pky, &P);
		bnSquareMod_ (&t2, &pkx, &P);
		bnSubQMod_ (&t2, 3, &P);
		bnMulMod_ (&t2, &t2, &pkx, &P);
		bnAddMod_ (&t2, &b, &P);
		if (bnCmp (&t1, &t2) != 0) {
			break;
		}
		
		s = zrtp_status_ok;
	} while (0);

	bnEnd (&t1);
	bnEnd (&t2);
	bnEnd (&pkx);
	bnEnd (&pky);
	bnEnd (&bnzero);
	bnEnd (&P);
	bnEnd (&b);
	
	ZRTP_LOG(1,(_ZTU_,"\tDH TEST: zrtp_ecdh_validate() for %.4s was executed by %llums.\n", self->base.type, zrtp_time_now()-start_ts));
    return s;
}


/*============================================================================*/
/*    P-256, 384, 521 (FIPS 186-3) support.  See RFC 4753 3.1, 3.2, 3.3		  */
/*============================================================================*/

static uint8_t sv256_data[] = {
	0x81, 0x42, 0x64, 0x14, 0x5F, 0x2F, 0x56, 0xF2,
	0xE9, 0x6A, 0x8E, 0x33, 0x7A, 0x12, 0x84, 0x99,
	0x3F, 0xAF, 0x43, 0x2A, 0x5A, 0xBC, 0xE5, 0x9E,
	0x86, 0x7B, 0x72, 0x91, 0xD5, 0x07, 0xA3, 0xAF
};
static uint8_t pvx256_data[] = {
	0x2A, 0xF5, 0x02, 0xF3, 0xBE, 0x89, 0x52, 0xF2,
	0xC9, 0xB5, 0xA8, 0xD4, 0x16, 0x0D, 0x09, 0xE9,
	0x71, 0x65, 0xBE, 0x50, 0xBC, 0x42, 0xAE, 0x4A,
	0x5E, 0x8D, 0x3B, 0x4B, 0xA8, 0x3A, 0xEB, 0x15
};
static uint8_t pvy256_data[] = {
	0xEB, 0x0F, 0xAF, 0x4C, 0xA9, 0x86, 0xC4, 0xD3,
	0x86, 0x81, 0xA0, 0xF9, 0x87, 0x2D, 0x79, 0xD5,
	0x67, 0x95, 0xBD, 0x4B, 0xFF, 0x6E, 0x6D, 0xE3,
	0xC0, 0xF5, 0x01, 0x5E, 0xCE, 0x5E, 0xFD, 0x85
};

static uint8_t sv384_data[] = {
	0xD2, 0x73, 0x35, 0xEA, 0x71, 0x66, 0x4A, 0xF2,
	0x44, 0xDD, 0x14, 0xE9, 0xFD, 0x12, 0x60, 0x71,
	0x5D, 0xFD, 0x8A, 0x79, 0x65, 0x57, 0x1C, 0x48,
	0xD7, 0x09, 0xEE, 0x7A, 0x79, 0x62, 0xA1, 0x56,
	0xD7, 0x06, 0xA9, 0x0C, 0xBC, 0xB5, 0xDF, 0x29,
	0x86, 0xF0, 0x5F, 0xEA, 0xDB, 0x93, 0x76, 0xF1
};
static uint8_t pvx384_data[] = {
	0x79, 0x31, 0x48, 0xF1, 0x78, 0x76, 0x34, 0xD5,
	0xDA, 0x4C, 0x6D, 0x90, 0x74, 0x41, 0x7D, 0x05,
	0xE0, 0x57, 0xAB, 0x62, 0xF8, 0x20, 0x54, 0xD1,
	0x0E, 0xE6, 0xB0, 0x40, 0x3D, 0x62, 0x79, 0x54,
	0x7E, 0x6A, 0x8E, 0xA9, 0xD1, 0xFD, 0x77, 0x42,
	0x7D, 0x01, 0x6F, 0xE2, 0x7A, 0x8B, 0x8C, 0x66
};
static uint8_t pvy384_data[] = {
	0xC6, 0xC4, 0x12, 0x94, 0x33, 0x1D, 0x23, 0xE6,
	0xF4, 0x80, 0xF4, 0xFB, 0x4C, 0xD4, 0x05, 0x04,
	0xC9, 0x47, 0x39, 0x2E, 0x94, 0xF4, 0xC3, 0xF0,
	0x6B, 0x8F, 0x39, 0x8B, 0xB2, 0x9E, 0x42, 0x36,
	0x8F, 0x7A, 0x68, 0x59, 0x23, 0xDE, 0x3B, 0x67,
	0xBA, 0xCE, 0xD2, 0x14, 0xA1, 0xA1, 0xD1, 0x28
};

static uint8_t sv521_data[] = {
	0x01, 0x13, 0xF8, 0x2D, 0xA8, 0x25, 0x73, 0x5E,
	0x3D, 0x97, 0x27, 0x66, 0x83, 0xB2, 0xB7, 0x42,
	0x77, 0xBA, 0xD2, 0x73, 0x35, 0xEA, 0x71, 0x66,
	0x4A, 0xF2, 0x43, 0x0C, 0xC4, 0xF3, 0x34, 0x59,
	0xB9, 0x66, 0x9E, 0xE7, 0x8B, 0x3F, 0xFB, 0x9B,
	0x86, 0x83, 0x01, 0x5D, 0x34, 0x4D, 0xCB, 0xFE,
	0xF6, 0xFB, 0x9A, 0xF4, 0xC6, 0xC4, 0x70, 0xBE,
	0x25, 0x45, 0x16, 0xCD, 0x3C, 0x1A, 0x1F, 0xB4,
	0x73, 0x62
};
static uint8_t pvx521_data[] = {
	0x01, 0xEB, 0xB3, 0x4D, 0xD7, 0x57, 0x21, 0xAB,
	0xF8, 0xAD, 0xC9, 0xDB, 0xED, 0x17, 0x88, 0x9C,
	0xBB, 0x97, 0x65, 0xD9, 0x0A, 0x7C, 0x60, 0xF2,
	0xCE, 0xF0, 0x07, 0xBB, 0x0F, 0x2B, 0x26, 0xE1,
	0x48, 0x81, 0xFD, 0x44, 0x42, 0xE6, 0x89, 0xD6,
	0x1C, 0xB2, 0xDD, 0x04, 0x6E, 0xE3, 0x0E, 0x3F,
	0xFD, 0x20, 0xF9, 0xA4, 0x5B, 0xBD, 0xF6, 0x41,
	0x3D, 0x58, 0x3A, 0x2D, 0xBF, 0x59, 0x92, 0x4F,
	0xD3, 0x5C
};
static uint8_t pvy521_data[] = {
	0x00, 0xF6, 0xB6, 0x32, 0xD1, 0x94, 0xC0, 0x38,
	0x8E, 0x22, 0xD8, 0x43, 0x7E, 0x55, 0x8C, 0x55,
	0x2A, 0xE1, 0x95, 0xAD, 0xFD, 0x15, 0x3F, 0x92,
	0xD7, 0x49, 0x08, 0x35, 0x1B, 0x2F, 0x8C, 0x4E,
	0xDA, 0x94, 0xED, 0xB0, 0x91, 0x6D, 0x1B, 0x53,
	0xC0, 0x20, 0xB5, 0xEE, 0xCA, 0xED, 0x1A, 0x5F,
	0xC3, 0x8A, 0x23, 0x3E, 0x48, 0x30, 0x58, 0x7B,
	0xB2, 0xEE, 0x34, 0x89, 0xB3, 0xB4, 0x2A, 0x5A,
	0x86, 0xA4
};

zrtp_status_t zrtp_ecdh_selftest(zrtp_pk_scheme_t *self)
{
	zrtp_status_t s = zrtp_status_fail;
	struct BigNum P, Gx, Gy, n, sv;
	struct BigNum pkx, pky;	
	unsigned ec_bytes = 0;
	unsigned pbits = 0;
	struct zrtp_ec_params ec_params;
	
	zrtp_time_t start_ts = 0;
	
	uint8_t *sv_data	= NULL;
	size_t sv_data_len	= 0;
	uint8_t *pvx_data	= NULL;
	size_t pvx_data_len = 0;
	uint8_t *pvy_data	= NULL;
	size_t pvy_data_len = 0;
	
	if (!self) {
		return zrtp_status_bad_param;
	}
	
	ZRTP_LOG(3, (_ZTU_, "PKS %.4s testing... ", self->base.type));
	
	switch (self->base.id) {
		case ZRTP_PKTYPE_EC256P:
			sv_data			= sv256_data;
			sv_data_len		= sizeof(sv256_data);
			pvx_data		= pvx256_data;
			pvx_data_len	= sizeof(pvx256_data);
			pvy_data		= pvy256_data;
			pvy_data_len	= sizeof(pvy256_data);
			break;
		case ZRTP_PKTYPE_EC384P:
			sv_data			= sv384_data;
			sv_data_len		= sizeof(sv384_data);
			pvx_data		= pvx384_data;
			pvx_data_len	= sizeof(pvx384_data);
			pvy_data		= pvy384_data;
			pvy_data_len	= sizeof(pvy384_data);
			break;
		case ZRTP_PKTYPE_EC521P:
			sv_data			= sv521_data;
			sv_data_len		= sizeof(sv521_data);
			pvx_data		= pvx521_data;
			pvx_data_len	= sizeof(pvx521_data);
			pvy_data		= pvy521_data;
			pvy_data_len	= sizeof(pvy521_data);
			break;
		default:
			return 0;
	}		
	
	pbits = get_pbits(self);
	if (!pbits) {
		return zrtp_status_bad_param;
	}
	
	zrtp_ec_init_params(&ec_params, pbits);
	
	ec_bytes = (ec_params.ec_bits+7) / 8;
	
	bnBegin(&P);
	bnInsertBigBytes(&P, ec_params.P_data, 0, ec_bytes );
	bnBegin(&Gx);
	bnInsertBigBytes(&Gx, ec_params.Gx_data, 0, ec_bytes );
	bnBegin(&Gy);
	bnInsertBigBytes(&Gy, ec_params.Gy_data, 0, ec_bytes );
	bnBegin(&n);
	bnInsertBigBytes(&n, ec_params.n_data, 0, ec_bytes );
	
	bnBegin(&pkx);
	bnBegin(&pky);
	bnBegin(&sv);
	s = zrtp_ec_random_point( self->base.zrtp, &P, &n, &Gx, &Gy,
							  &pkx, &pky, &sv,
							  sv_data, sv_data_len);
	if (zrtp_status_ok == s)
	{
		struct BigNum pkx1, pky1;
		
		bnBegin(&pkx1); bnBegin(&pky1);
		bnInsertBigBytes(&pkx1, pvx_data, 0, pvx_data_len);
		bnInsertBigBytes(&pky1, pvy_data, 0, pvy_data_len);
		s = (bnCmp (&pkx1, &pkx) == 0 && bnCmp (&pky1, &pky) == 0) ? zrtp_status_ok : zrtp_status_fail;
		bnEnd(&pkx1);
		bnEnd(&pky1);	
	}
	
	bnEnd (&pkx);
	bnEnd (&pky);
	bnEnd (&P);
	bnEnd (&Gx);
	bnEnd (&Gy);
	bnEnd (&n);
	bnEnd (&sv);
	
	if (zrtp_status_ok == s) {
	zrtp_status_t s = zrtp_status_ok;
	zrtp_dh_crypto_context_t alice_cc;
	zrtp_dh_crypto_context_t bob_cc;
	struct BigNum alice_k;
	struct BigNum bob_k;
	
	start_ts = zrtp_time_now();
	
	bnBegin(&alice_k);
	bnBegin(&bob_k);
	
	do {	
		/* Both sides initalise DH schemes and compute secret and public values. */
		s = self->initialize(self, &alice_cc);
		if (zrtp_status_ok != s) {
			break;
		}
		s = self->initialize(self, &bob_cc);
		if (zrtp_status_ok != s) {
			break;
		}
		
		/* Both sides validate public values. (to provide exact performance estimation) */
		s = self->validate(self, &bob_cc.pv);
		if (zrtp_status_ok != s) {
			break;
		}
		s = self->validate(self, &alice_cc.pv);
		if (zrtp_status_ok != s) {
			break;
		}
		
		/* Compute secret keys and compare them. */
		s = self->compute(self, &alice_cc, &alice_k, &bob_cc.pv);
		if (zrtp_status_ok != s) {
			break;
		}
		s= self->compute(self, &bob_cc, &bob_k, &alice_cc.pv);
		if (zrtp_status_ok != s) {
			break;
		}
				
		s = (0 == bnCmp(&alice_k, &bob_k)) ? zrtp_status_ok : zrtp_status_algo_fail;
	} while (0);

	bnEnd(&alice_k);
	bnEnd(&bob_k);
	}
	ZRTP_LOGC(3, ("%s (%llu ms)\n", zrtp_log_status2str(s), (zrtp_time_now()-start_ts)/2));	
	
	return s;
}


/*============================================================================*/
/*    Public Key support													  */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_defaults_ec_pkt(zrtp_global_t* zrtp)
{
    zrtp_pk_scheme_t* ec256p = zrtp_sys_alloc(sizeof(zrtp_pk_scheme_t));
    zrtp_pk_scheme_t* ec384p = zrtp_sys_alloc(sizeof(zrtp_pk_scheme_t));
    zrtp_pk_scheme_t* ec521p = zrtp_sys_alloc(sizeof(zrtp_pk_scheme_t));
    
	if (!ec256p || !ec384p || !ec521p) {
		if(ec256p) {
			zrtp_sys_free(ec256p);
		}
		if(ec384p) {
			zrtp_sys_free(ec384p);
		}
		if(ec521p) {
			zrtp_sys_free(ec521p);
		}
		return zrtp_status_alloc_fail;
	}

    zrtp_memset(ec256p, 0, sizeof(zrtp_pk_scheme_t));
    zrtp_memcpy(ec256p->base.type, ZRTP_EC256P, ZRTP_COMP_TYPE_SIZE);
	ec256p->base.id		= ZRTP_PKTYPE_EC256P;
    ec256p->base.zrtp	= zrtp;
    ec256p->sv_length	= 256/8;
    ec256p->pv_length	= 2*256/8;
    ec256p->base.init 	= zrtp_ecdh_init;
    ec256p->base.free	= zrtp_ecdh_free;
    ec256p->initialize	= zrtp_ecdh_initialize;
    ec256p->compute		= zrtp_ecdh_compute;
    ec256p->validate	= zrtp_ecdh_validate;
	ec256p->self_test	= zrtp_ecdh_selftest;

    zrtp_memset(ec384p, 0, sizeof(zrtp_pk_scheme_t));
    zrtp_memcpy(ec384p->base.type, ZRTP_EC384P, ZRTP_COMP_TYPE_SIZE);
	ec384p->base.id		= ZRTP_PKTYPE_EC384P;
    ec384p->base.zrtp	= zrtp;
    ec384p->sv_length	= 384/8;
    ec384p->pv_length	= 2*384/8;
    ec384p->base.init 	= zrtp_ecdh_init;
    ec384p->base.free	= zrtp_ecdh_free;
    ec384p->initialize	= zrtp_ecdh_initialize;
    ec384p->compute		= zrtp_ecdh_compute;
    ec384p->validate	= zrtp_ecdh_validate;
	ec384p->self_test	= zrtp_ecdh_selftest;
	

    zrtp_memset(ec521p, 0, sizeof(zrtp_pk_scheme_t));
    zrtp_memcpy(ec521p->base.type, ZRTP_EC521P, ZRTP_COMP_TYPE_SIZE);
	ec521p->base.id		= ZRTP_PKTYPE_EC521P;
    ec521p->base.zrtp	= zrtp;
    ec521p->sv_length	= 528/8;
    ec521p->pv_length	= 2*528/8;
    ec521p->base.init 	= zrtp_ecdh_init;
    ec521p->base.free	= zrtp_ecdh_free;
    ec521p->initialize	= zrtp_ecdh_initialize;
    ec521p->compute		= zrtp_ecdh_compute;
    ec521p->validate	= zrtp_ecdh_validate;
	ec521p->self_test	= zrtp_ecdh_selftest;

    zrtp_comp_register(ZRTP_CC_PKT, ec256p, zrtp);
    zrtp_comp_register(ZRTP_CC_PKT, ec384p, zrtp);
    zrtp_comp_register(ZRTP_CC_PKT, ec521p, zrtp);

    return zrtp_status_ok;
}
