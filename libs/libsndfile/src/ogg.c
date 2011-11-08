/*
** Copyright (C) 2002-2011 Erik de Castro Lopo <erikd@mega-nerd.com>
** Copyright (C) 2007 John ffitch
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

#if HAVE_EXTERNAL_LIBS

#include <ogg/ogg.h>

#include "ogg.h"

static int	ogg_close (SF_PRIVATE *psf) ;
static int	ogg_stream_classify (SF_PRIVATE *psf, OGG_PRIVATE * odata) ;
static int	ogg_page_classify (SF_PRIVATE * psf, const ogg_page * og) ;

int
ogg_open (SF_PRIVATE *psf)
{	OGG_PRIVATE* odata = calloc (1, sizeof (OGG_PRIVATE)) ;
	sf_count_t pos = psf_ftell (psf) ;
	int	error = 0 ;

	psf->container_data = odata ;
	psf->container_close = ogg_close ;

	if (psf->file.mode == SFM_RDWR)
		return SFE_BAD_MODE_RW ;

	if (psf->file.mode == SFM_READ)
		if ((error = ogg_stream_classify (psf, odata)) != 0)
			return error ;

	/* Reset everything to an initial state. */
	ogg_sync_clear (&odata->osync) ;
	ogg_stream_clear (&odata->ostream) ;
	psf_fseek (psf, pos, SEEK_SET) ;

	switch (psf->sf.format)
	{	case SF_FORMAT_OGG | SF_FORMAT_VORBIS :
			return ogg_vorbis_open (psf) ;

		case SF_FORMAT_OGGFLAC :
			free (psf->container_data) ;
			psf->container_data = NULL ;
			psf->container_close = NULL ;
			return flac_open (psf) ;

#if ENABLE_EXPERIMENTAL_CODE
		case SF_FORMAT_OGG | SF_FORMAT_SPEEX :
			return ogg_speex_open (psf) ;

		case SF_FORMAT_OGG | SF_FORMAT_PCM_16 :
		case SF_FORMAT_OGG | SF_FORMAT_PCM_24 :
			return ogg_pcm_open (psf) ;
#endif

		default :
			break ;
		} ;

	psf_log_printf (psf, "%s : mode should be SFM_READ or SFM_WRITE.\n", __func__) ;
	return SFE_INTERNAL ;
} /* ogg_open */


static int
ogg_close (SF_PRIVATE *psf)
{	OGG_PRIVATE* odata = psf->container_data ;

	ogg_sync_clear (&odata->osync) ;
	ogg_stream_clear (&odata->ostream) ;

	return 0 ;
} /* ogg_close */

static int
ogg_stream_classify (SF_PRIVATE *psf, OGG_PRIVATE* odata)
{	char *buffer ;
	int	 bytes, nn ;

	/* Call this here so it only gets called once, so no memory is leaked. */
	ogg_sync_init (&odata->osync) ;

	odata->eos = 0 ;

	/* Weird stuff happens if these aren't called. */
	ogg_stream_reset (&odata->ostream) ;
	ogg_sync_reset (&odata->osync) ;

	/*
	**	Grab some data at the head of the stream.  We want the first page
	**	(which is guaranteed to be small and only contain the Vorbis
	**	stream initial header) We need the first page to get the stream
	**	serialno.
	*/

	/* Expose the buffer */
	buffer = ogg_sync_buffer (&odata->osync, 4096L) ;

	/* Grab the part of the header that has already been read. */
	memcpy (buffer, psf->header, psf->headindex) ;
	bytes = psf->headindex ;

	/* Submit a 4k block to libvorbis' Ogg layer */
	bytes += psf_fread (buffer + psf->headindex, 1, 4096 - psf->headindex, psf) ;
	ogg_sync_wrote (&odata->osync, bytes) ;

	/* Get the first page. */
	if ((nn = ogg_sync_pageout (&odata->osync, &odata->opage)) != 1)
	{
		/* Have we simply run out of data?  If so, we're done. */
		if (bytes < 4096)
			return 0 ;

		/* Error case.  Must not be Vorbis data */
		psf_log_printf (psf, "Input does not appear to be an Ogg bitstream.\n") ;
		return SFE_MALFORMED_FILE ;
		} ;

	/*
	**	Get the serial number and set up the rest of decode.
	**	Serialno first ; use it to set up a logical stream.
	*/
	ogg_stream_clear (&odata->ostream) ;
	ogg_stream_init (&odata->ostream, ogg_page_serialno (&odata->opage)) ;

	if (ogg_stream_pagein (&odata->ostream, &odata->opage) < 0)
	{	/* Error ; stream version mismatch perhaps. */
		psf_log_printf (psf, "Error reading first page of Ogg bitstream data\n") ;
		return SFE_MALFORMED_FILE ;
		} ;

	if (ogg_stream_packetout (&odata->ostream, &odata->opacket) != 1)
	{	/* No page? must not be vorbis. */
		psf_log_printf (psf, "Error reading initial header packet.\n") ;
		return SFE_MALFORMED_FILE ;
		} ;

	odata->codec = ogg_page_classify (psf, &odata->opage) ;

	switch (odata->codec)
	{	case OGG_VORBIS :
			psf->sf.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS ;
			return 0 ;

		case OGG_FLAC :
		case OGG_FLAC0 :
			psf->sf.format = SF_FORMAT_OGGFLAC ;
			return 0 ;

		case OGG_SPEEX :
			psf->sf.format = SF_FORMAT_OGG | SF_FORMAT_SPEEX ;
			return 0 ;

		case OGG_PCM :
			psf_log_printf (psf, "Detected Ogg/PCM data. This is not supported yet.\n") ;
			return SFE_UNIMPLEMENTED ;

		default :
			break ;
		} ;

	psf_log_printf (psf, "This Ogg bitstream contains some uknown data type.\n") ;
	return SFE_UNIMPLEMENTED ;
} /* ogg_stream_classify */

/*==============================================================================
*/

static struct
{	const char *str, *name ;
	int len, codec ;
} codec_lookup [] =
{	{	"Annodex",		"Annodex",	8, OGG_ANNODEX },
	{	"AnxData",		"AnxData",	7, OGG_ANXDATA },
	{	"\177FLAC",		"Flac1",	5, OGG_FLAC },
	{	"fLaC",			"Flac0",	4, OGG_FLAC0 },
	{	"PCM     ",		"PCM",		8, OGG_PCM },
	{	"Speex",		"Speex",	5, OGG_SPEEX },
	{	"\001vorbis",	"Vorbis",	7, OGG_VORBIS },
} ;

static int
ogg_page_classify (SF_PRIVATE * psf, const ogg_page * og)
{	int k, len ;

	for (k = 0 ; k < ARRAY_LEN (codec_lookup) ; k++)
	{	if (codec_lookup [k].len > og->body_len)
			continue ;

		if (memcmp (og->body, codec_lookup [k].str, codec_lookup [k].len) == 0)
		{	psf_log_printf (psf, "Ogg stream data : %s\n", codec_lookup [k].name) ;
			psf_log_printf (psf, "Stream serialno : %010D\n", (int64_t) ogg_page_serialno (og)) ;
			return codec_lookup [k].codec ;
			} ;
		} ;

	len = og->body_len < 8 ? og->body_len : 8 ;

	psf_log_printf (psf, "Ogg_stream data : '") ;
	for (k = 0 ; k < len ; k++)
		psf_log_printf (psf, "%c", isprint (og->body [k]) ? og->body [k] : '.') ;
	psf_log_printf (psf, "'     ") ;
	for (k = 0 ; k < len ; k++)
		psf_log_printf (psf, " %02x", og->body [k] & 0xff) ;
	psf_log_printf (psf, "\n") ;

	return 0 ;
} /* ogg_page_classify */

#else /* HAVE_EXTERNAL_LIBS */

int
ogg_open	(SF_PRIVATE *psf)
{
	psf_log_printf (psf, "This version of libsndfile was compiled without Ogg/Vorbis support.\n") ;
	return SFE_UNIMPLEMENTED ;
} /* ogg_open */

#endif
