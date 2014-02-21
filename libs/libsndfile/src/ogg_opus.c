/*
** Copyright (C) 2013 Erik de Castro Lopo <erikd@mega-nerd.com>
**
** This program is free software ; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation ; either version 2.1 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY ; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program ; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#include "sfconfig.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "sndfile.h"
#include "sfendian.h"
#include "common.h"

#if (ENABLE_EXPERIMENTAL_CODE && HAVE_EXTERNAL_LIBS)

#include <ogg/ogg.h>

#include "ogg.h"

typedef struct
{	int32_t serialno ;


	void * state ;
} OPUS_PRIVATE ;

static int	ogg_opus_read_header (SF_PRIVATE * psf) ;
static int	ogg_opus_close (SF_PRIVATE *psf) ;

int
ogg_opus_open (SF_PRIVATE *psf)
{	OGG_PRIVATE* odata = psf->container_data ;
	OPUS_PRIVATE* oopus = calloc (1, sizeof (OPUS_PRIVATE)) ;
	int	error = 0 ;

	if (odata == NULL)
	{	psf_log_printf (psf, "%s : odata is NULL???\n", __func__) ;
		return SFE_INTERNAL ;
		} ;

	psf->codec_data = oopus ;
	if (oopus == NULL)
		return SFE_MALLOC_FAILED ;

	if (psf->file.mode == SFM_RDWR)
		return SFE_BAD_MODE_RW ;

	if (psf->file.mode == SFM_READ)
	{	/* Call this here so it only gets called once, so no memory is leaked. */
		ogg_sync_init (&odata->osync) ;

		if ((error = ogg_opus_read_header (psf)))
			return error ;

#if 0
		psf->read_short		= ogg_opus_read_s ;
		psf->read_int		= ogg_opus_read_i ;
		psf->read_float		= ogg_opus_read_f ;
		psf->read_double	= ogg_opus_read_d ;
		psf->sf.frames		= ogg_opus_length (psf) ;
#endif
		} ;

	psf->codec_close = ogg_opus_close ;

	if (psf->file.mode == SFM_WRITE)
	{
#if 0
		/* Set the default oopus quality here. */
		vdata->quality = 0.4 ;

		psf->write_header	= ogg_opus_write_header ;
		psf->write_short	= ogg_opus_write_s ;
		psf->write_int		= ogg_opus_write_i ;
		psf->write_float	= ogg_opus_write_f ;
		psf->write_double	= ogg_opus_write_d ;
#endif

		psf->sf.frames = SF_COUNT_MAX ; /* Unknown really */
		psf->strings.flags = SF_STR_ALLOW_START ;
		} ;

	psf->bytewidth = 1 ;
	psf->blockwidth = psf->bytewidth * psf->sf.channels ;

#if 0
	psf->seek = ogg_opus_seek ;
	psf->command = ogg_opus_command ;
#endif

	/* FIXME, FIXME, FIXME : Hack these here for now and correct later. */
	psf->sf.format = SF_FORMAT_OGG | SF_FORMAT_SPEEX ;
	psf->sf.sections = 1 ;

	psf->datalength = 1 ;
	psf->dataoffset = 0 ;
	/* End FIXME. */

	return error ;
} /* ogg_opus_open */

static int
ogg_opus_read_header (SF_PRIVATE * UNUSED (psf))
{
	return 0 ;
} /* ogg_opus_read_header */

static int
ogg_opus_close (SF_PRIVATE * UNUSED (psf))
{


	return 0 ;
} /* ogg_opus_close */


#else /* ENABLE_EXPERIMENTAL_CODE && HAVE_EXTERNAL_LIBS */

int
ogg_opus_open (SF_PRIVATE *psf)
{
	psf_log_printf (psf, "This version of libsndfile was compiled without Ogg/Opus support.\n") ;
	return SFE_UNIMPLEMENTED ;
} /* ogg_opus_open */

#endif
