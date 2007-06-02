
/*
 *	bell202.h
 *
 *	Copyright (c) 2005 Robert Krten.  All Rights Reserved.
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions
 *	are met:
 *
 *	1. Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	2. Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 *	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 *	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *	ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 *	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *	OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *	SUCH DAMAGE.
 *
 *	This module contains the manifest constants and declarations for
 *	the Bell-202 1200 baud FSK modem.
 *
 *	2005 03 20	R. Krten		created
*/

#ifndef	__BELL202_H__
#define	__BELL202_H__

typedef struct dsp_bell202_attr_s
{
	int					sample_rate;					// sample rate in HZ
	void				(*bithandler) (void *, int);	// bit handler
	void				*bithandler_arg;				// arbitrary ID passed to bithandler as first argument
	void				(*bytehandler) (void *, int);	// byte handler
	void				*bytehandler_arg;				// arbitrary ID passed to bytehandler as first argument
}	dsp_bell202_attr_t;

typedef struct
{
	dsp_bell202_attr_t	attr;							// attributes structure
	double				*correlates [4];				// one for each of sin/cos for mark/space
	int					corrsize;						// correlate size (also number of samples in ring buffer)
	double				*buffer;						// sample ring buffer
	int					ringstart;						// ring buffer start offset
	double				cellpos;						// bit cell position
	double				celladj;						// bit cell adjustment for each sample
	int					previous_bit;					// previous bit (for detecting a transition to sync-up cell position)
	int					current_bit;					// current bit
	int					downsampling_count;				// number of samples to skip
	int					current_downsample;				// current skip count
}	dsp_bell202_handle_t;

/*
 *	Function prototypes
 *
 *	General calling order is:
 *		a) create the attributes structure (dsp_bell202_attr_init)
 *		b) initialize fields in the attributes structure (dsp_bell202_attr_set_*)
 *		c) create a Bell-202 handle (dsp_bell202_create)
 *		d) feed samples through the handler (dsp_bell202_sample)
*/

extern	void					dsp_bell202_attr_init (dsp_bell202_attr_t *attributes);

extern  void					(*dsp_bell202_attr_get_bithandler (dsp_bell202_attr_t *attributes, void **bithandler_arg)) (void *, int);
extern	void					dsp_bell202_attr_set_bithandler (dsp_bell202_attr_t *attributes, void (*bithandler) (void *, int ), void *bithandler_arg);
extern  void					(*dsp_bell202_attr_get_bytehandler (dsp_bell202_attr_t *attributes, void **bytehandler_arg)) (void *, int);
extern	void					dsp_bell202_attr_set_bytehandler (dsp_bell202_attr_t *attributes, void (*bytehandler) (void *, int ), void *bytehandler_arg);
extern	int						dsp_bell202_attr_get_samplerate (dsp_bell202_attr_t *attributes);
extern	int						dsp_bell202_attr_set_samplerate (dsp_bell202_attr_t *attributes, int samplerate);

extern	dsp_bell202_handle_t *	dsp_bell202_create (dsp_bell202_attr_t *attributes);
extern	void					dsp_bell202_destroy (dsp_bell202_handle_t *handle);

extern	void					dsp_bell202_sample (dsp_bell202_handle_t *handle, double normalized_sample);

#endif	// __BELL202_H__

