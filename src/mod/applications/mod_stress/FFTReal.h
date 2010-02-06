/*****************************************************************************
*                                                                            *
*       DIGITAL SIGNAL PROCESSING TOOLS                                      *
*       Version 1.03, 2001/06/15                                             *
*       (c) 1999 - Laurent de Soras                                          *
*                                                                            *
*       FFTReal.h                                                            *
*       Fourier transformation of real number arrays.                        *
*       Portable ISO C++                                                     *
*                                                                            *
* Tab = 3                                                                    *
*****************************************************************************/



#if defined (FFTReal_CURRENT_HEADER)
#error Recursive inclusion of FFTReal header file.
#endif
#define	FFTReal_CURRENT_HEADER

#if ! defined (FFTReal_HEADER_INCLUDED)
#define	FFTReal_HEADER_INCLUDED



#if defined (_MSC_VER)
#pragma pack (push, 8)
#endif // _MSC_VER



class FFTReal {

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

  public:

	/* Change this typedef to use a different floating point type in your FFTs
	   (i.e. float, double or long double). */
	typedef float flt_t;

	explicit FFTReal(const long length);
	    ~FFTReal();
	void do_fft(flt_t f[], const flt_t x[]) const;
	void do_ifft(const flt_t f[], flt_t x[]) const;
	void rescale(flt_t x[]) const;



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

  private:

	/* Bit-reversed look-up table nested class */
	      class BitReversedLUT {
	  public:
		explicit BitReversedLUT(const int nbr_bits);
		   ~BitReversedLUT();
		const long *get_ptr() const {
			return (_ptr);
	  } private:
		long *_ptr;
	};

	/* Trigonometric look-up table nested class */
	class TrigoLUT {
	  public:
		explicit TrigoLUT(const int nbr_bits);
		   ~TrigoLUT();
		const flt_t *get_ptr(const int level) const {
			return (_ptr + (1L << (level - 1)) - 4);
		};
	  private:
		      flt_t *_ptr;
	};

	const BitReversedLUT _bit_rev_lut;
	const TrigoLUT _trigo_lut;
	const flt_t _sqrt2_2;
	const long _length;
	const int _nbr_bits;
	flt_t *_buffer_ptr;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

  private:

	FFTReal(const FFTReal & other);
	const FFTReal & operator =(const FFTReal & other);
	int operator ==(const FFTReal & other);
	int operator !=(const FFTReal & other);
};



#if defined (_MSC_VER)
#pragma pack (pop)
#endif // _MSC_VER



#endif // FFTReal_HEADER_INCLUDED

#undef FFTReal_CURRENT_HEADER



/*****************************************************************************

	LEGAL

	Source code may be freely used for any purpose, including commercial
	applications. Programs must display in their "About" dialog-box (or
	documentation) a text telling they use these routines by Laurent de Soras.
	Modified source code can be distributed, but modifications must be clearly
	indicated.

	CONTACT

	Laurent de Soras
	92 avenue Albert 1er
	92500 Rueil-Malmaison
	France

	ldesoras@club-internet.fr

*****************************************************************************/



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
