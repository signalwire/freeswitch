/* k6opt.h  vector functions optimized for MMX extensions to x86
 *
 * Copyright (C) 1999 by Stanley J. Brooks <stabro@megsinet.net>
 * 
 * Any use of this software is permitted provided that this notice is not
 * removed and that neither the authors nor the Technische Universitaet Berlin
 * are deemed to have made any representations as to the suitability of this
 * software for any purpose nor are held responsible for any defects of
 * this software.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE;
 * not even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.
 * 
 * Chicago, 03.12.1999
 * Stanley J. Brooks
 */

extern void Weighting_filter P2((e, x),
	const word	* e,	/* signal [-5..0.39.44]	IN  */
	word	* x		/* signal [0..39]	OUT */
)
;

extern longword k6maxcc P3((wt,dp,Nc_out),
	const word *wt,
	const word *dp, 
	word		* Nc_out	/* 		OUT	*/
)
;
/*
 * k6maxmin(p,n,out[])
 *  input p[n] is array of shorts (require n>0)
 *  returns (long) maximum absolute value..
 *  if out!=NULL, also returns out[0] the maximum and out[1] the minimum
 */
extern longword k6maxmin P3((p,n,out),
	const word *p,
	int n, 
	word *out	/* 		out[0] is max, out[1] is min */
)
;

extern longword k6iprod P3((p,q,n),
	const word *p,
	const word *q,
	int n
)
;

/*
 * k6vsraw(p,n,bits)
 *  input p[n] is array of shorts (require n>0)
 *  shift/round each to the right by bits>=0 bits.
 */
extern void k6vsraw P3((p,n,bits),
	const word *p,
	int n, 
	int bits
)
;

/*
 * k6vsllw(p,n,bits)
 *  input p[n] is array of shorts (require n>0)
 *  shift each to the left by bits>=0 bits.
 */
extern void k6vsllw P3((p,n,bits),
	const word *p,
	int n, 
	int bits
)
;

#if 1  /* there isn't any significant speed gain from mmx here: */
extern void Short_term_analysis_filteringx P4((u0,rp0,k_n,s),
	register word * u0,
	register word	* rp0,	/* [0..7]	IN	*/
	register int 	k_n, 	/*   k_end - k_start	*/
	register word	* s	/* [0..n-1]	IN/OUT	*/
)
;
/*
#define Short_term_analysis_filtering Short_term_analysis_filteringx
*/
#endif
