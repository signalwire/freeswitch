/*

$Log$
Revision 1.15  2004/06/26 03:50:14  markster
Merge source cleanups (bug #1911)

Revision 1.14  2003/02/12 13:59:15  matteo
mer feb 12 14:56:57 CET 2003

Revision 1.1.1.1  2003/02/12 13:59:15  matteo
mer feb 12 14:56:57 CET 2003

Revision 1.2  2000/01/05 08:20:39  markster
Some OSS fixes and a few lpc changes to make it actually work

 * Revision 1.2  1996/08/20  20:26:28  jaf
 * Any typedef defining a type that was used in lpc10_encoder_state or
 * lpc10_decoder_state struct's was commented out here and added to
 * lpc10.h.
 *
 * Revision 1.1  1996/08/19  22:32:13  jaf
 * Initial revision
 *

*/

/*
 * f2c.h
 *
 * SCCS ID:  @(#)f2c.h 1.2 96/05/19
 */

/* f2c.h  --  Standard Fortran to C header file */

/**  barf  [ba:rf]  2.  "He suggested using FORTRAN, and everybody barfed."

	- From The Shogakukan DICTIONARY OF NEW ENGLISH (Second edition) */

#ifndef F2C_INCLUDE
#define F2C_INCLUDE

#include "lpc10.h"

/*typedef long int integer;*/
/*typedef INT32 integer;*/
/*typedef short int shortint;*/
/*typedef INT16 shortint;*/
/*typedef float real;*/
/* doublereal only used for function arguments to sqrt, exp, etc. */
typedef double doublereal;
/* 32 bits seems wasteful, but there really aren't that many logical
 * variables around, and making them 32 bits could avoid word
 * alignment problems, perhaps.  */
/*typedef long int logical;*/
/*typedef INT32 logical;*/
/* The following types are not used in the translated C code for the
 * LPC-10 coder, but they might be needed by the definitions down
 * below, so they don't cause compilation errors.  */
typedef char *address;
typedef struct { real r, i; } complex;
typedef struct { doublereal r, i; } doublecomplex;
typedef short int shortlogical;
typedef char logical1;
typedef char integer1;
/* typedef long long longint; */ /* system-dependent */

#define TRUE_ (1)
#define FALSE_ (0)

/* Extern is for use with -E */
#ifndef Extern
#define Extern extern
#endif

/* I/O stuff */

#ifdef f2c_i2
/* for -i2 */
typedef short flag;
typedef short ftnlen;
typedef short ftnint;
#else
typedef long int flag;
typedef long int ftnlen;
typedef long int ftnint;
#endif

/*external read, write*/
typedef struct
{	flag cierr;
	ftnint ciunit;
	flag ciend;
	char *cifmt;
	ftnint cirec;
} cilist;

/*internal read, write*/
typedef struct
{	flag icierr;
	char *iciunit;
	flag iciend;
	char *icifmt;
	ftnint icirlen;
	ftnint icirnum;
} icilist;

/*open*/
typedef struct
{	flag oerr;
	ftnint ounit;
	char *ofnm;
	ftnlen ofnmlen;
	char *osta;
	char *oacc;
	char *ofm;
	ftnint orl;
	char *oblnk;
} olist;

/*close*/
typedef struct
{	flag cerr;
	ftnint cunit;
	char *csta;
} cllist;

/*rewind, backspace, endfile*/
typedef struct
{	flag aerr;
	ftnint aunit;
} alist;

/* inquire */
typedef struct
{	flag inerr;
	ftnint inunit;
	char *infile;
	ftnlen infilen;
	ftnint	*inex;	/*parameters in standard's order*/
	ftnint	*inopen;
	ftnint	*innum;
	ftnint	*innamed;
	char	*inname;
	ftnlen	innamlen;
	char	*inacc;
	ftnlen	inacclen;
	char	*inseq;
	ftnlen	inseqlen;
	char 	*indir;
	ftnlen	indirlen;
	char	*infmt;
	ftnlen	infmtlen;
	char	*inform;
	ftnint	informlen;
	char	*inunf;
	ftnlen	inunflen;
	ftnint	*inrecl;
	ftnint	*innrec;
	char	*inblank;
	ftnlen	inblanklen;
} inlist;

#define VOID void

union Multitype {	/* for multiple entry points */
	integer1 g;
	shortint h;
	integer i;
	/* longint j; */
	real r;
	doublereal d;
	complex c;
	doublecomplex z;
	};

typedef union Multitype Multitype;

/*typedef long int Long;*/	/* No longer used; formerly in Namelist */

struct Vardesc {	/* for Namelist */
	char *name;
	char *addr;
	ftnlen *dims;
	int  type;
	};
typedef struct Vardesc Vardesc;

struct Namelist {
	char *name;
	Vardesc **vars;
	int nvars;
	};
typedef struct Namelist Namelist;

#define abs(x) ((x) >= 0 ? (x) : -(x))
#define dabs(x) (doublereal)abs(x)
#define min(a,b) ((a) <= (b) ? (a) : (b))
#define max(a,b) ((a) >= (b) ? (a) : (b))
#define dmin(a,b) (doublereal)min(a,b)
#define dmax(a,b) (doublereal)max(a,b)

/* procedure parameter types for -A and -C++ */

#define F2C_proc_par_types 1
#ifdef __cplusplus
typedef int /* Unknown procedure type */ (*U_fp)(...);
typedef shortint (*J_fp)(...);
typedef integer (*I_fp)(...);
typedef real (*R_fp)(...);
typedef doublereal (*D_fp)(...), (*E_fp)(...);
typedef /* Complex */ VOID (*C_fp)(...);
typedef /* Double Complex */ VOID (*Z_fp)(...);
typedef logical (*L_fp)(...);
typedef shortlogical (*K_fp)(...);
typedef /* Character */ VOID (*H_fp)(...);
typedef /* Subroutine */ int (*S_fp)(...);
#else
typedef int /* Unknown procedure type */ (*U_fp)(VOID);
typedef shortint (*J_fp)(VOID);
typedef integer (*I_fp)(VOID);
typedef real (*R_fp)(VOID);
typedef doublereal (*D_fp)(VOID), (*E_fp)(VOID);
typedef /* Complex */ VOID (*C_fp)(VOID);
typedef /* Double Complex */ VOID (*Z_fp)(VOID);
typedef logical (*L_fp)(VOID);
typedef shortlogical (*K_fp)(VOID);
typedef /* Character */ VOID (*H_fp)(VOID);
typedef /* Subroutine */ int (*S_fp)(VOID);
#endif
/* E_fp is for real functions when -R is not specified */
typedef VOID C_f;	/* complex function */
typedef VOID H_f;	/* character function */
typedef VOID Z_f;	/* double complex function */
typedef doublereal E_f;	/* real function with -R not specified */

/* undef any lower-case symbols that your C compiler predefines, e.g.: */

#ifndef Skip_f2c_Undefs
#undef cray
#undef gcos
#undef mc68010
#undef mc68020
#undef mips
#undef pdp11
#undef sgi
#undef sparc
#undef sun
#undef sun2
#undef sun3
#undef sun4
#undef u370
#undef u3b
#undef u3b2
#undef u3b5
#undef unix
#undef vax
#endif

#ifdef KR_headers
extern integer pow_ii(ap, bp);
extern double r_sign(a,b);
extern integer i_nint(x);
#else
extern integer pow_ii(integer *ap, integer *bp);
extern double r_sign(real *a, real *b);
extern integer i_nint(real *x);

#endif
#ifdef P_R_O_T_O_T_Y_P_E_S
extern int bsynz_(real *coef, integer *ip, integer *iv, 
                  real *sout, real *rms, real *ratio, real *g2pass,
                  struct lpc10_decoder_state *st);
extern int chanwr_(integer *order, integer *ipitv, integer *irms,
                   integer *irc, integer *ibits, struct lpc10_encoder_state *st);
extern int chanrd_(integer *order, integer *ipitv, integer *irms,
                   integer *irc, integer *ibits);
extern int chanwr_0_(int n__, integer *order, integer *ipitv, 
                     integer *irms, integer *irc, integer *ibits,
                     struct lpc10_encoder_state *st);
extern int dcbias_(integer *len, real *speech, real *sigout);
extern int decode_(integer *ipitv, integer *irms, integer *irc, 
                   integer *voice, integer *pitch, real *rms,
                   real *rc, struct lpc10_decoder_state *st);
extern int deemp_(real *x, integer *n, struct lpc10_decoder_state *st);
extern int difmag_(real *speech, integer *lpita, integer *tau, integer *ltau,
                   integer *maxlag, real *amdf, integer *minptr, integer *maxptr);
extern int dyptrk_(real *amdf, integer *ltau, integer *
                   minptr, integer *voice, integer *pitch, integer *midx,
                   struct lpc10_encoder_state *st);
extern int encode_(integer *voice, integer *pitch, real *rms, real *rc,
                   integer *ipitch, integer *irms, integer *irc);
extern int energy_(integer *len, real *speech, real *rms);
extern int ham84_(integer *input, integer *output, integer *errcnt);
extern int hp100_(real *speech, integer *start, integer *end,
		  struct lpc10_encoder_state *st);
extern int inithp100_(void);
extern int invert_(integer *order, real *phi, real *psi, real *rc);
extern int irc2pc_(real *rc, real *pc, integer *order, real *gprime, real *g2pass);
extern int ivfilt_(real *lpbuf, real *ivbuf, integer *len, integer *nsamp, real *ivrc);
extern int lpcdec_(integer *bits, real *speech);
extern int initlpcdec_(void);
extern int lpcenc_(real *speech, integer *bits);
extern int initlpcenc_(void);
extern int lpfilt_(real *inbuf, real *lpbuf, integer *len, integer *nsamp);
extern integer median_(integer *d1, integer *d2, integer *d3);
extern int mload_(integer *order, integer *awins, integer *awinf, real *speech, real *phi, real *psi);
extern int onset_(real *pebuf, integer *osbuf, integer *osptr, integer *oslen, integer *sbufl, integer *sbufh, integer *lframe, struct lpc10_encoder_state *st);
extern int pitsyn_(integer *order, integer *voice, integer *pitch, real *rms, real *rc, integer *lframe, integer *ivuv, integer *ipiti, real *rmsi, real *rci, integer *nout, real *ratio, struct lpc10_decoder_state *st);
extern int placea_(integer *ipitch, integer *voibuf, integer *obound, integer *af, integer *vwin, integer *awin, integer *ewin, integer *lframe, integer *maxwin);
extern int placev_(integer *osbuf, integer *osptr, integer *oslen, integer *obound, integer *vwin, integer *af, integer *lframe, integer *minwin, integer *maxwin, integer *dvwinl, integer *dvwinh);
extern int preemp_(real *inbuf, real *pebuf, integer *nsamp, real *coef, real *z__);
extern int prepro_(real *speech, integer *length,
		   struct lpc10_encoder_state *st);
extern int decode_(integer *ipitv, integer *irms, integer *irc, integer *voice, integer *pitch, real *rms, real *rc, struct lpc10_decoder_state *st);
extern integer random_(struct lpc10_decoder_state *st);
extern int rcchk_(integer *order, real *rc1f, real *rc2f);
extern int synths_(integer *voice, integer *pitch, real *rms, real *rc, real *speech, integer *k, struct lpc10_decoder_state *st);
extern int tbdm_(real *speech, integer *lpita, integer *tau, integer *ltau, real *amdf, integer *minptr, integer *maxptr, integer *mintau);
extern int voicin_(integer *vwin, real *inbuf, real *lpbuf, integer *buflim, integer *half, real *minamd, real *maxamd, integer *mintau, real *ivrc, integer *obound, integer *voibuf, integer *af, struct lpc10_encoder_state *st);
extern int vparms_(integer *vwin, real *inbuf, real *lpbuf, integer *buflim, integer *half, real *dither, integer *mintau, integer *zc, integer *lbe, integer *fbe, real *qs, real *rc1, real *ar_b__, real *ar_f__);

#endif


#endif /* ! defined F2C_INCLUDE */
