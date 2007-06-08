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

#ifndef	__FSK_H__
#define	__FSK_H__
#include "uart.h"

typedef struct {
    int freq_space;		/* Frequency of the 0 bit				*/
    int freq_mark;		/* Frequency of the 1 bit				*/
    int baud_rate;		/* baud rate for the modem				*/
} fsk_modem_definition_t;

/* Must be kept in sync with fsk_modem_definitions array in fsk.c	*/
/* V.23 definitions: http://www.itu.int/rec/recommendation.asp?type=folders&lang=e&parent=T-REC-V.23 */
typedef enum {
    FSK_V23_FORWARD_MODE1 = 0,	/* Maximum 600 bps for long haul	*/
    FSK_V23_FORWARD_MODE2,		/* Standard 1200 bps V.23			*/
    FSK_V23_BACKWARD,			/* 75 bps return path for V.23		*/
    FSK_BELL202					/* Bell 202 half-duplex 1200 bps	*/
} fsk_modem_types_t;

typedef enum {
	FSK_STATE_CHANSEIZE = 0,
	FSK_STATE_CARRIERSIG,
	FSK_STATE_DATA
} fsk_state_t;

typedef struct dsp_fsk_attr_s
{
	int					sample_rate;					/* sample rate in HZ */
	bithandler_func_t	bithandler;						/* bit handler */
	void				*bithandler_arg;				/* arbitrary ID passed to bithandler as first argument */
	bytehandler_func_t	bytehandler;					/* byte handler */
	void				*bytehandler_arg;				/* arbitrary ID passed to bytehandler as first argument */
}	dsp_fsk_attr_t;

typedef struct
{
	fsk_state_t			state;
	dsp_fsk_attr_t		attr;							/* attributes structure */
	double				*correlates[4];					/* one for each of sin/cos for mark/space */
	int					corrsize;						/* correlate size (also number of samples in ring buffer) */
	double				*buffer;						/* sample ring buffer */
	int					ringstart;						/* ring buffer start offset */
	double				cellpos;						/* bit cell position */
	double				celladj;						/* bit cell adjustment for each sample */
	int					previous_bit;					/* previous bit (for detecting a transition to sync-up cell position) */
	int					current_bit;					/* current bit */
	int					last_bit;
	int					downsampling_count;				/* number of samples to skip */
	int					current_downsample;				/* current skip count */
	int					conscutive_state_bits;			/* number of bits in a row that matches the pattern for the current state */
}	dsp_fsk_handle_t;

/*
 *	Function prototypes
 *
 *	General calling order is:
 *		a) create the attributes structure (dsp_fsk_attr_init)
 *		b) initialize fields in the attributes structure (dsp_fsk_attr_set_*)
 *		c) create a Bell-202 handle (dsp_fsk_create)
 *		d) feed samples through the handler (dsp_fsk_sample)
*/

void					dsp_fsk_attr_init(dsp_fsk_attr_t *attributes);

bithandler_func_t		dsp_fsk_attr_get_bithandler(dsp_fsk_attr_t *attributes, void **bithandler_arg);
void					dsp_fsk_attr_set_bithandler(dsp_fsk_attr_t *attributes, bithandler_func_t bithandler, void *bithandler_arg);
bytehandler_func_t		dsp_fsk_attr_get_bytehandler(dsp_fsk_attr_t *attributes, void **bytehandler_arg);
void					dsp_fsk_attr_set_bytehandler(dsp_fsk_attr_t *attributes, bytehandler_func_t bytehandler, void *bytehandler_arg);
int						dsp_fsk_attr_get_samplerate(dsp_fsk_attr_t *attributes);
int						dsp_fsk_attr_set_samplerate(dsp_fsk_attr_t *attributes, int samplerate);

dsp_fsk_handle_t *	dsp_fsk_create(dsp_fsk_attr_t *attributes);
void					dsp_fsk_destroy(dsp_fsk_handle_t **handle);

void					dsp_fsk_sample(dsp_fsk_handle_t *handle, double normalized_sample);

extern fsk_modem_definition_t fsk_modem_definitions[];

#endif	

