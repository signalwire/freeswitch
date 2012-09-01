/* $Id: raw_decode.c,v 1.1 2012-06-01 21:04:22 fwarmerdam Exp $ */

/*
 * Copyright (c) 2012, Frank Warmerdam <warmerdam@pobox.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */

/*
 * TIFF Library
 *
 * The objective of this test suite is to test the JPEGRawDecode() 
 * interface via TIFReadEncodedTile().  This function with YCbCr subsampling
 * is a frequent source of bugs. 
 */

#include "tif_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H 
# include <unistd.h> 
#endif 

#include "tiffio.h"

static unsigned char cluster_0[] = { 0, 0, 2, 0, 138, 139 };
static unsigned char cluster_64[] = { 0, 0, 9, 6, 134, 119 };
static unsigned char cluster_128[] = { 44, 40, 63, 59, 230, 95 };

static int check_cluster( int cluster, unsigned char *buffer, unsigned char *expected_cluster ) {
	unsigned char *target = buffer + cluster*6;

	if (memcmp(target, expected_cluster, 6) == 0) {
		return 0;
	}

	fprintf( stderr, "Cluster %d did not match expected results.\n", cluster );
	fprintf( stderr, 
		 "Expect: %3d %3d   %3d   %3d\n"
		 "        %3d %3d\n", 
		 expected_cluster[0], expected_cluster[1],
		 expected_cluster[4], expected_cluster[5],
		 expected_cluster[2], expected_cluster[3] );
	fprintf( stderr, 
		 "   Got: %3d %3d   %3d   %3d\n"
		 "        %3d %3d\n", 
		 target[0], target[1], 
		 target[4], target[5],
		 target[2], target[3] );
	return 1;
}

static int check_rgb_pixel( int pixel, int red, int green, int blue, unsigned char *buffer ) {
	unsigned char *rgb = buffer + 3 * pixel;
	
	if( rgb[0] == red && rgb[1] == green && rgb[2] == blue ) {
		return 0;
	}

	fprintf( stderr, "Pixel %d did not match expected results.\n", pixel );
	fprintf( stderr, "Expect: %3d %3d %3d\n", red, green, blue );
	fprintf( stderr, "   Got: %3d %3d %3d\n", rgb[0], rgb[1], rgb[2] );
	return 1;
}

static int check_rgba_pixel( int pixel, int red, int green, int blue, int alpha, unsigned char *buffer ) {
	/* RGBA images are upside down - adjust for normal ordering */
	int adjusted_pixel = pixel % 128 + (127 - (pixel/128)) * 128;
	unsigned char *rgba = buffer + 4 * adjusted_pixel;
	
	if( rgba[0] == red && rgba[1] == green && rgba[2] == blue && rgba[3] == alpha ) {
		return 0;
	}

	fprintf( stderr, "Pixel %d did not match expected results.\n", pixel );
	fprintf( stderr, "Expect: %3d %3d %3d %3d\n", red, green, blue, alpha );
	fprintf( stderr, "   Got: %3d %3d %3d %3d\n", rgba[0], rgba[1], rgba[2], rgba[3] );
	return 1;
}

int
main(int argc, char **argv)
{
	TIFF		*tif;
	static const char *srcfile = "images/quad-tile.jpg.tiff";
	unsigned short h, v;
	int status;
	unsigned char *buffer;
	tsize_t sz, szout;

        (void) argc;
        (void) argv;

	tif = TIFFOpen(srcfile,"r");
	if ( tif == NULL ) {
		fprintf( stderr, "Could not open %s\n", srcfile);
		exit( 1 );
	}

	status = TIFFGetField(tif,TIFFTAG_YCBCRSUBSAMPLING, &h, &v);
	if ( status == 0 || h != 2 || v != 2) {
		fprintf( stderr, "Could not retrieve subsampling tag.\n" );
		exit(1);
	}

	/*
	 * What is the appropriate size of a YCbCr encoded tile?
	 */
	sz = TIFFTileSize(tif);
	if( sz != 24576) {
		fprintf(stderr, "tiles are %d bytes\n", (int)sz);
		exit(1);
	}

	buffer = (unsigned char *) malloc(sz);

	/*
	 * Read a tile in decompressed form, but still YCbCr subsampled.
	 */
	szout = TIFFReadEncodedTile(tif,9,buffer,sz);
	if (szout != sz) {
		fprintf( stderr, 
			 "Did not get expected result code from TIFFReadEncodedTile()(%d instead of %d)\n", 
			 (int) szout, (int) sz );
		return 1;
	}

	if( check_cluster( 0, buffer, cluster_0 )
	    || check_cluster( 64, buffer, cluster_64 )
	    || check_cluster( 128, buffer, cluster_128 ) ) {
		exit(1);
	}
	free(buffer);

	/*
	 * Read a tile using the built-in conversion to RGB format provided by the JPEG library.
	 */
	TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);

	sz = TIFFTileSize(tif);
	if( sz != 128*128*3) {
		fprintf(stderr, "tiles are %d bytes\n", (int)sz);
		exit(1);
	}

	buffer = (unsigned char *) malloc(sz);

	szout = TIFFReadEncodedTile(tif,9,buffer,sz);
	if (szout != sz) {
		fprintf( stderr, 
			 "Did not get expected result code from TIFFReadEncodedTile()(%d instead of %d)\n", 
			 (int) szout, (int) sz );
		return 1;
	}

	if (check_rgb_pixel( 0, 15, 0, 18, buffer )
	    || check_rgb_pixel( 64, 0, 0, 2, buffer )
	    || check_rgb_pixel( 512, 6, 36, 182, buffer ) ) {
		exit(1);
	}	

	free( buffer );

	TIFFClose(tif);

	/*
	 * Reopen and test reading using the RGBA interface.
	 */
	tif = TIFFOpen(srcfile,"r");
	
	sz = 128 * 128 * 4;
	buffer = (unsigned char *) malloc(sz);
	
	if (!TIFFReadRGBATile( tif, 1*128, 2*128, (uint32 *) buffer )) {
		fprintf( stderr, "TIFFReadRGBATile() returned failure code.\n" );
		return 1;
	}

	/*
	 * Currently TIFFReadRGBATile() just uses JPEGCOLORMODE_RGB so this
	 * trivally matches the last results.  Eventually we should actually
	 * accomplish it from the YCbCr subsampled buffer ourselves in which
	 * case the results may be subtly different but similar.
	 */
	if (check_rgba_pixel( 0, 15, 0, 18, 255, buffer )
	    || check_rgba_pixel( 64, 0, 0, 2, 255, buffer )
	    || check_rgba_pixel( 512, 6, 36, 182, 255, buffer ) ) {
		exit(1);
	}	

	free( buffer );
	TIFFClose(tif);
	
	exit( 0 );
}

/* vim: set ts=8 sts=8 sw=8 noet: */
