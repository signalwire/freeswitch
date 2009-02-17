/*
 * iLBC - a library for the iLBC codec
 *
 * packing.h - The iLBC low bit rate speech codec.
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * iLBC code supplied in RFC3951.
 *
 * Original code Copyright (C) The Internet Society (2004).
 * All changes to produce this version Copyright (C) 2008 by Steve Underwood
 * All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: packing.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __PACKING_H
#define __PACKING_H

void packsplit(int *index,              /* (i) the value to split */
               int *firstpart,          /* (o) the value specified by most
                                               significant bits */
               int *rest,               /* (o) the value specified by least
                                               significant bits */
               int bitno_firstpart,     /* (i) number of bits in most
                                               significant part */
               int bitno_total          /* (i) number of bits in full range
                                               of value */
);

void packcombine(int *index,            /* (i/o) the msb value in the combined value out */
                 int rest,              /* (i) the lsb value */
                 int bitno_rest);       /* (i) the number of bits in the lsb part */

void dopack(uint8_t **bitstream,        /* (i/o) on entrance pointer to
                                                 place in bitstream to pack
                                                 new data, on exit pointer
                                                 to place in bitstream to
                                                 pack future data */
            int index,                  /* (i) the value to pack */
            int bitno,                  /* (i) the number of bits that the
                                               value will fit within */
            int *pos);                  /* (i/o) write position in the current byte */

void unpack(const uint8_t **bitstream,  /* (i/o) on entrance pointer to
                                                 place in bitstream to
                                                 unpack new data from, on
                                                 exit pointer to place in
                                                 bitstream to unpack future
                                                 data from */
            int *index,                 /* (o) resulting value */
            int bitno,                  /* (i) number of bits used to
                                               represent the value */
            int *pos);                  /* (i/o) read position in the current byte */

#endif
