/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t35.c - ITU T.35 FAX non-standard facility processing.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * The NSF data tables are adapted from the NSF handling in HylaFAX, which
 * carries the following copyright notice:
 *
 * Created by Dmitry Bely, April 2000
 * Copyright (c) 1994-1996 Sam Leffler
 * Copyright (c) 1994-1996 Silicon Graphics, Inc.
 * HylaFAX is a trademark of Silicon Graphics
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

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif

#include "spandsp/telephony.h"
#include "spandsp/bit_operations.h"
#include "spandsp/t35.h"

/*! NSF pattern for FAX machine identification */
typedef struct
{
    /*! The number of bytes of the NSF byte string to match */
    int model_id_size;
    /*! The NSF byte string to expect */
    const char *model_id;
    /*! The model name of the FAX terminal */
    const char *model_name;
} model_data_t;

/*! NSF pattern for identifying the manufacturer of a FAX machine */
typedef struct
{
    /*! The vendor ID byte string */
    const char *vendor_id;
    /*! The length of the vendor ID byte string */
    int vendor_id_len;
    /*! The vendor's name */
    const char *vendor_name;
    /*! True if the station ID for this vendor is reversed */
    int inverse_station_id_order;
    /*! A pointer to a list of known models from this vendor */
    const model_data_t *known_models;
} nsf_data_t;

/*! T.35 country codes */
typedef struct
{
    /*! The country's name */
    const char *name;
    /*! A pointer to a list of known vendors from this country */
    const nsf_data_t *vendors;
} country_code_t;

static const model_data_t Canon[] =
{
    {5, "\x80\x00\x80\x48\x00", "Faxphone B640"},
    {5, "\x80\x00\x80\x49\x10", "Fax B100"},
    {5, "\x80\x00\x8A\x49\x10", "Laser Class 9000 Series"},
    {5, "\x80\x00\x8A\x48\x00", "Laser Class 2060"},
    {0, NULL, NULL}
};

static const model_data_t Brother[] =
{
    {9, "\x55\x55\x00\x88\x90\x80\x5F\x00\x15\x51", "Intellifax 770"},
    {9, "\x55\x55\x00\x80\xB0\x80\x00\x00\x59\xD4", "Personal fax 190"},
    {9, "\x55\x55\x00\x8C\x90\x80\xF0\x02\x20", "MFC-8600"},
    {0, NULL, NULL}
};

static const model_data_t Panasonic0E[] =
{
    {10, "\x00\x00\x00\x96\x0F\x01\x02\x00\x10\x05\x02\x95\xC8\x08\x01\x49\x02\x41\x53\x54\x47", "KX-F90"},
    {10, "\x00\x00\x00\x96\x0F\x01\x03\x00\x10\x05\x02\x95\xC8\x08\x01\x49\x02\x03", "KX-F230 or KX-FT21 or ..."},
    {10, "\x00\x00\x00\x16\x0F\x01\x03\x00\x10\x05\x02\x95\xC8\x08", "KX-F780"},
    {10, "\x00\x00\x00\x16\x0F\x01\x03\x00\x10\x00\x02\x95\x80\x08\x75\xB5", "KX-M260"},
    {10, "\x00\x00\x00\x16\x0F\x01\x02\x00\x10\x05\x02\x85\xC8\x08\xAD", "KX-F2050BS"},
    {0, NULL, NULL}
};

static const model_data_t Panasonic79[] =
{
    {10, "\x00\x00\x00\x02\x0F\x09\x12\x00\x10\x05\x02\x95\xC8\x88\x80\x80\x01", "UF-S10"},
    {10, "\x00\x00\x00\x16\x7F\x09\x13\x00\x10\x05\x16\x8D\xC0\xD0\xF8\x80\x01", "/Siemens Fax 940"},
    {10, "\x00\x00\x00\x16\x0F\x09\x13\x00\x10\x05\x06\x8D\xC0\x50\xCB", "Panafax UF-321"},
    {0, NULL, NULL}
};

static const model_data_t Ricoh[] =
{
    {10, "\x00\x00\x00\x12\x10\x0D\x02\x00\x50\x00\x2A\xB8\x2C", "/Nashuatec P394"},
    {0, NULL, NULL}
};

static const model_data_t Samsung16[] =
{
    {4, "\x00\x00\xA4\x01", "M545 6800"},
    {0, NULL, NULL}
};

static const model_data_t Samsung5A[] =
{
    {4, "\x00\x00\xC0\x00", "SF-5100"},
    {0, NULL, NULL}
};

static const model_data_t Samsung8C[] =
{
    {4, "\x00\x00\x01\x00", "SF-2010"},
    {0, NULL, NULL}
};

static const model_data_t SamsungA2[] =
{
    {4, "\x00\x00\x80\x00", "FX-4000"},
    {0, NULL, NULL}
};

static const model_data_t Sanyo[] =
{
    {10, "\x00\x00\x10\xB1\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x41\x26\xFF\xFF\x00\x00\x85\xA1", "SFX-107"},
    {10, "\x00\x00\x00\xB1\x12\xF2\x62\xB4\x82\x0A\xF2\x2A\x12\xD2\xA2\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x41\x4E\xFF\xFF\x00\x00", "MFP-510"},
    {0, NULL, NULL}
};

static const model_data_t HP[] =
{
    {5, "\x20\x00\x45\x00\x0C\x04\x70\xCD\x4F\x00\x7F\x49", "LaserJet 3150"},
    {5, "\x40\x80\x84\x01\xF0\x6A", "OfficeJet"},
    {5, "\xC0\x00\x00\x00\x00", "OfficeJet 500"},
    {5, "\xC0\x00\x00\x00\x00\x8B", "Fax-920"},
    {0, NULL, NULL}
};

static const model_data_t Sharp[] =
{
    {32, "\x00\xCE\xB8\x80\x80\x11\x85\x0D\xDD\x00\x00\xDD\xDD\x00\x00\xDD\xDD\x00\x00\x00\x00\x00\x00\x00\x00\xED\x22\xB0\x00\x00\x90\x00", "Sharp F0-10"},
    {33, "\x00\xCE\xB8\x80\x80\x11\x85\x0D\xDD\x00\x00\xDD\xDD\x00\x00\xDD\xDD\x00\x00\x00\x00\x00\x00\x00\x00\xED\x22\xB0\x00\x00\x90\x00\x8C", "Sharp UX-460"},
    {33, "\x00\x4E\xB8\x80\x80\x11\x84\x0D\xDD\x00\x00\xDD\xDD\x00\x00\xDD\xDD\x00\x00\x00\x00\x00\x00\x00\x00\xED\x22\xB0\x00\x00\x90\x00\xAD", "Sharp UX-177"},
    {33, "\x00\xCE\xB8\x00\x84\x0D\xDD\x00\x00\xDD\xDD\x00\x00\xDD\xDD\xDD\xDD\xDD\x02\x05\x28\x02\x22\x43\x29\xED\x23\x90\x00\x00\x90\x01\x00", "Sharp FO-4810"},
    {0, NULL, NULL}
};

static const model_data_t Xerox[] =
{
    {10, "\x00\x08\x2D\x43\x57\x50\x61\x75\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01\x1A\x02\x02\x10\x01\x82\x01\x30\x34", "635 Workcenter"},
    {0, NULL, NULL}
};

static const model_data_t XeroxDA[] =
{
    {4, "\x00\x00\xC0\x00", "Workcentre Pro 580"},
    {0, NULL, NULL}
};

static const model_data_t Lexmark[] =
{
    {4, "\x00\x80\xA0\x00", "X4270"},
    {0, NULL, NULL}
};

static const model_data_t JetFax[] =
{
    {6, "\x01\x00\x45\x00\x0D\x7F", "M910e"},
    {0, NULL, NULL}
};

static const model_data_t PitneyBowes[] =
{
    {6, "\x79\x91\xB1\xB8\x7A\xD8", "9550"},
    {0, NULL, NULL}
};

static const model_data_t Dialogic[] =
{
    {8, "\x56\x8B\x06\x55\x00\x15\x00\x00", "VFX/40ESC"},
    {0, NULL, NULL}
};

static const model_data_t Muratec45[] =
{
    {10, "\xF4\x91\xFF\xFF\xFF\x42\x2A\xBC\x01\x57", "M4700"},
    {0, NULL, NULL}
};

/* Muratec uses unregistered Japan code "00 00 48" */
static const model_data_t Muratec48[] =
{
    {3, "\x53\x53\x61", "M620"},
    {0, NULL, NULL}
};

/*
 * Country code first byte, then manufacturer is last two bytes. See T.35.
 * Apparently Germany issued some manufacturer codes before the two-byte
 * standard was accepted, and so some few German manufacturers are
 * identified by a single manufacturer byte.
 *
 * T.30 5.3.6.2.7 (2003) states that the NSF FIF is transmitted
 * in MSB2LSB order.  Revisions of T.30 prior to 2003 did not
 * contain explicit specification as to the transmit bit order.
 * (Although it did otherwise state that all HDLC frame data should
 * be in MSB order except as noted.)  Because CSI, TSI, and other
 * prologue frames were in LSB order by way of an exception to the
 * general rule (T.30 5.3.6.2.4-11) many manufacturers assumed that
 * NSF should also be in LSB order.  Consequently there will be
 * some country-code "masquerading" as a terminal may use the
 * proper country-code, but with an inverted bit order.
 *
 * Thus, country code x61 (Korea) turns into x86 (Papua New Guinea),
 * code xB5 (USA) turns into xAD (Tunisia), code x26 (China) turns
 * into x64 (Lebanon), code x04 (Germany) turns into x20 (Canada),
 * and code x3D (France) turns into xBC (Vietnam).
 *
 * For the most part it should be safe to identify a manufacturer
 * both with the MSB and LSB ordered bits, as the "masqueraded" country
 * is likely to not be actively assigning T.38 manufacturer codes.
 * However, some manufacturers (e.g. Microsoft) may use MSB for the
 * country code and LSB for the rest of the NSF, and so basically this
 * table must be verified and corrected against actual real-world
 * results.
 */
static const nsf_data_t vendor_00[] =
{
    /* Japan */
    {"\x00\x00", 2, "Unknown - indeterminate", true, NULL},
    {"\x00\x01", 2, "Anritsu", false, NULL},
    {"\x00\x02", 2, "Nippon Telephone", false, NULL},
    {"\x00\x05", 2, "Mitsuba Electric", false, NULL},
    {"\x00\x06", 2, "Master Net", false, NULL},
    {"\x00\x09", 2, "Xerox/Toshiba", true, Xerox},
    {"\x00\x0A", 2, "Kokusai", false, NULL},
    {"\x00\x0D", 2, "Logic System International", false, NULL},
    {"\x00\x0E", 2, "Panasonic", false, Panasonic0E},
    {"\x00\x11", 2, "Canon", false, Canon},
    {"\x00\x15", 2, "Toyotsushen Machinery", false, NULL},
    {"\x00\x16", 2, "System House Mind", false, NULL},
    {"\x00\x19", 2, "Xerox", true, NULL},
    {"\x00\x1D", 2, "Hitachi Software", false, NULL},
    {"\x00\x21", 2, "OKI Electric/Lanier", true, NULL},
    {"\x00\x25", 2, "Ricoh", true, Ricoh},
    {"\x00\x26", 2, "Konica", false, NULL},
    {"\x00\x29", 2, "Japan Wireless", false, NULL},
    {"\x00\x2D", 2, "Sony", false, NULL},
    {"\x00\x31", 2, "Sharp/Olivetti", false, Sharp},
    {"\x00\x35", 2, "Kogyu", false, NULL},
    {"\x00\x36", 2, "Japan Telecom", false, NULL},
    {"\x00\x3D", 2, "IBM Japan", false, NULL},
    {"\x00\x39", 2, "Panasonic", false, NULL},
    {"\x00\x41", 2, "Swasaki Communication", false, NULL},
    {"\x00\x45", 2, "Muratec", false, Muratec45},
    {"\x00\x46", 2, "Pheonix", false, NULL},
    {"\x00\x48", 2, "Muratec", false, Muratec48},           /* Not registered */
    {"\x00\x49", 2, "Japan Electric", false, NULL},
    {"\x00\x4D", 2, "Okura Electric", false, NULL},
    {"\x00\x51", 2, "Sanyo", false, Sanyo},
    {"\x00\x55", 2, "Unknown - Japan 55", false, NULL},
    {"\x00\x56", 2, "Brother", false, Brother},
    {"\x00\x59", 2, "Fujitsu", false, NULL},
    {"\x00\x5D", 2, "Kuoni", false, NULL},
    {"\x00\x61", 2, "Casio", false, NULL},
    {"\x00\x65", 2, "Tateishi Electric", false, NULL},
    {"\x00\x66", 2, "Utax/Mita", true, NULL},
    {"\x00\x69", 2, "Hitachi Production", false, NULL},
    {"\x00\x6D", 2, "Hitachi Telecom", false, NULL},
    {"\x00\x71", 2, "Tamura Electric Works", false, NULL},
    {"\x00\x75", 2, "Tokyo Electric Corp.", false, NULL},
    {"\x00\x76", 2, "Advance", false, NULL},
    {"\x00\x79", 2, "Panasonic", false, Panasonic79},
    {"\x00\x7D", 2, "Seiko", false, NULL},
    {"\x08\x00", 2, "Daiko", false, NULL},
    {"\x10\x00", 2, "Funai Electric", false, NULL},
    {"\x20\x00", 2, "Eagle System", false, NULL},
    {"\x30\x00", 2, "Nippon Business Systems", false, NULL},
    {"\x40\x00", 2, "Comtron", false, NULL},
    {"\x48\x00", 2, "Cosmo Consulting", false, NULL},
    {"\x50\x00", 2, "Orion Electric", false, NULL},
    {"\x60\x00", 2, "Nagano Nippon", false, NULL},
    {"\x70\x00", 2, "Kyocera", false, NULL},
    {"\x80\x00", 2, "Kanda Networks", false, NULL},
    {"\x88\x00", 2, "Soft Front", false, NULL},
    {"\x90\x00", 2, "Arctic", false, NULL},
    {"\xA0\x00", 2, "Nakushima", false, NULL},
    {"\xB0\x00", 2, "Minolta", false, NULL},
    {"\xC0\x00", 2, "Tohoku Pioneer", false, NULL},
    {"\xD0\x00", 2, "USC", false, NULL},
    {"\xE0\x00", 2, "Hiboshi", false, NULL},
    {"\xF0\x00", 2, "Sumitomo Electric", false, NULL},
    {NULL, 0, NULL, false, NULL}
};

static const nsf_data_t vendor_20[] =
{
    /* Germany */
    {"\x09",     1, "ITK Institut für Telekommunikation GmbH & Co KG", false, NULL},
    {"\x11",     1, "Dr. Neuhaus Mikroelektronik", false, NULL},
    {"\x21",     1, "ITO Communication", false, NULL},
    {"\x31",     1, "mbp Kommunikationssysteme GmbH", false, NULL},
    {"\x41",     1, "Siemens", false, NULL},
    {"\x42",     1, "Deutsche Telekom AG", false, NULL},
    {"\x51",     1, "mps Software", false, NULL},
    {"\x61",     1, "Hauni Elektronik", false, NULL},
    {"\x71",     1, "Digitronic computersysteme gmbh", false, NULL},
    {"\x81\x00", 2, "Innovaphone GmbH", false, NULL},
    {"\x81\x40", 2, "TEDAS Gesellschaft für Telekommunikations-, Daten- und Audiosysteme mbH", false, NULL},
    {"\x81\x80", 2, "AVM Audiovisuelles Marketing und Computersysteme GmbH", false, NULL},
    {"\x81\xC0", 2, "EICON Technology Research GmbH", false, NULL},
    {"\xB1",     1, "Schneider Rundfunkwerke AG", false, NULL},
    {"\xC2",     1, "Deutsche Telekom AG", false, NULL},
    {"\xD1",     1, "Ferrari electronik GmbH", false, NULL},
    {"\xF1",     1, "DeTeWe - Deutsche Telephonwerke AG & Co", false, NULL},
    {"\xFF",     1, "Germany Regional Code", false, NULL},
    {NULL, 0, NULL, false, NULL}
};

static const nsf_data_t vendor_61[] =
{
    /* Korea */
    {"\x00\x7A", 2, "Xerox", false, NULL},
    {NULL, 0, NULL, false, NULL}
};

static const nsf_data_t vendor_64[] =
{
    /* China (not Lebanon) */
    {"\x00\x00", 2, "Unknown - China 00 00", false, NULL},
    {"\x01\x00", 2, "Unknown - China 01 00", false, NULL},
    {"\x01\x01", 2, "Unknown - China 01 01", false, NULL},
    {"\x01\x02", 2, "Unknown - China 01 02", false, NULL},
    {NULL, 0, NULL, false, NULL}
};

static const nsf_data_t vendor_86[] =
{
    /* Korea (not Papua New Guinea) */
    {"\x00\x02", 2, "Unknown - Korea 02", false, NULL},
    {"\x00\x06", 2, "Unknown - Korea 06", false, NULL},
    {"\x00\x08", 2, "Unknown - Korea 08", false, NULL},
    {"\x00\x0A", 2, "Unknown - Korea 0A", false, NULL},
    {"\x00\x0E", 2, "Unknown - Korea 0E", false, NULL},
    {"\x00\x10", 2, "Samsung", false, NULL},
    {"\x00\x11", 2, "Unknown - Korea 11", false, NULL},
    {"\x00\x16", 2, "Samsung", false, Samsung16},
    {"\x00\x1A", 2, "Unknown - Korea 1A", false, NULL},
    {"\x00\x40", 2, "Unknown - Korea 40", false, NULL},
    {"\x00\x48", 2, "Unknown - Korea 48", false, NULL},
    {"\x00\x52", 2, "Unknown - Korea 52", false, NULL},
    {"\x00\x5A", 2, "Samsung", false, Samsung5A},
    {"\x00\x5E", 2, "Unknown - Korea 5E", false, NULL},
    {"\x00\x66", 2, "Unknown - Korea 66", false, NULL},
    {"\x00\x6E", 2, "Unknown - Korea 6E", false, NULL},
    {"\x00\x82", 2, "Unknown - Korea 82", false, NULL},
    {"\x00\x88", 2, "Unknown - Korea 88", false, NULL},
    {"\x00\x8A", 2, "Unknown - Korea 8A", false, NULL},
    {"\x00\x8C", 2, "Samsung", false, Samsung8C},
    {"\x00\x92", 2, "Unknown - Korea 92", false, NULL},
    {"\x00\x98", 2, "Samsung", false, NULL},
    {"\x00\xA2", 2, "Samsung", false, SamsungA2},
    {"\x00\xA4", 2, "Unknown - Korea A4", false, NULL},
    {"\x00\xC2", 2, "Samsung", false, NULL},
    {"\x00\xC9", 2, "Unknown - Korea C9", false, NULL},
    {"\x00\xCC", 2, "Unknown - Korea CC", false, NULL},
    {"\x00\xD2", 2, "Unknown - Korea D2", false, NULL},
    {"\x00\xDA", 2, "Xerox", false, XeroxDA},
    {"\x00\xE2", 2, "Unknown - Korea E2", false, NULL},
    {"\x00\xEC", 2, "Unknown - Korea EC", false, NULL},
    {"\x00\xEE", 2, "Unknown - Korea EE", false, NULL},
    {NULL, 0, NULL, false, NULL}
};

static const nsf_data_t vendor_ad[] =
{
    /* United States (not Tunisia) */
    {"\x00\x00", 2, "Pitney Bowes", false, PitneyBowes},
    {"\x00\x0C", 2, "Dialogic", false, Dialogic},
    {"\x00\x15", 2, "Lexmark", false, Lexmark},
    {"\x00\x16", 2, "JetFax", false, JetFax},
    {"\x00\x24", 2, "Octel", false, NULL},
    {"\x00\x36", 2, "HP", false, HP},
    {"\x00\x42", 2, "FaxTalk", false, NULL},
    {"\x00\x44", 2, NULL, true, NULL},
    {"\x00\x46", 2, "BrookTrout", false, NULL},
    {"\x00\x51", 2, "Telogy Networks", false, NULL},
    {"\x00\x55", 2, "HylaFAX", false, NULL},
    {"\x00\x5C", 2, "IBM", false, NULL},
    {"\x00\x98", 2, "Unknown - USA 98", true, NULL},
    {NULL, 0, NULL, false, NULL}
};

static const nsf_data_t vendor_b4[] =
{
    /* United Kingdom */
    {"\x00\xB0", 2, "DCE", false, NULL},
    {"\x00\xB1", 2, "Hasler", false, NULL},
    {"\x00\xB2", 2, "Interquad", false, NULL},
    {"\x00\xB3", 2, "Comwave", false, NULL},
    {"\x00\xB4", 2, "Iconographic", false, NULL},
    {"\x00\xB5", 2, "Wordcraft", false, NULL},
    {"\x00\xB6", 2, "Acorn", false, NULL},
    {NULL, 0, NULL, false, NULL}
};

static const nsf_data_t vendor_b5[] =
{
    /* United States */
    {"\x00\x01", 2, "Picturetel", false, NULL},
    {"\x00\x20", 2, "Conexant", false, NULL},
    {"\x00\x22", 2, "Comsat", false, NULL},
    {"\x00\x24", 2, "Octel", false, NULL},
    {"\x00\x26", 2, "ROLM", false, NULL},
    {"\x00\x28", 2, "SOFNET", false, NULL},
    {"\x00\x29", 2, "TIA TR-29 Committee", false, NULL},
    {"\x00\x2A", 2, "STF Tech", false, NULL},
    {"\x00\x2C", 2, "HKB", false, NULL},
    {"\x00\x2E", 2, "Delrina", false, NULL},
    {"\x00\x30", 2, "Dialogic", false, Dialogic},
    {"\x00\x32", 2, "Applied Synergy", false, NULL},
    {"\x00\x34", 2, "Syncro Development", false, NULL},
    {"\x00\x36", 2, "Genoa", false, NULL},
    {"\x00\x38", 2, "Texas Instruments", false, NULL},
    {"\x00\x3A", 2, "IBM", false, NULL},
    {"\x00\x3C", 2, "ViaSat", false, NULL},
    {"\x00\x3E", 2, "Ericsson", false, NULL},
    {"\x00\x42", 2, "Bogosian", false, NULL},
    {"\x00\x44", 2, "Adobe", false, NULL},
    {"\x00\x46", 2, "Fremont Communications", false, NULL},
    {"\x00\x48", 2, "Hayes", false, NULL},
    {"\x00\x4A", 2, "Lucent", false, NULL},
    {"\x00\x4C", 2, "Data Race", false, NULL},
    {"\x00\x4E", 2, "TRW", false, NULL},
    {"\x00\x52", 2, "Audiofax", false, NULL},
    {"\x00\x54", 2, "Computer Automation", false, NULL},
    {"\x00\x56", 2, "Serca", false, NULL},
    {"\x00\x58", 2, "Octocom", false, NULL},
    {"\x00\x5C", 2, "Power Solutions", false, NULL},
    {"\x00\x5A", 2, "Digital Sound", false, NULL},
    {"\x00\x5E", 2, "Pacific Data", false, NULL},
    {"\x00\x60", 2, "Commetrex", false, NULL},
    {"\x00\x62", 2, "BrookTrout", false, NULL},
    {"\x00\x64", 2, "Gammalink", false, NULL},
    {"\x00\x66", 2, "Castelle", false, NULL},
    {"\x00\x68", 2, "Hybrid Fax", false, NULL},
    {"\x00\x6A", 2, "Omnifax", false, NULL},
    {"\x00\x6C", 2, "HP", false, NULL},
    {"\x00\x6E", 2, "Microsoft", false, NULL},
    {"\x00\x72", 2, "Speaking Devices", false, NULL},
    {"\x00\x74", 2, "Compaq", false, NULL},
    {"\x00\x76", 2, "Microsoft", false, NULL},      /* Uses LSB for country but MSB for manufacturer */
    {"\x00\x78", 2, "Cylink", false, NULL},
    {"\x00\x7A", 2, "Pitney Bowes", false, NULL},
    {"\x00\x7C", 2, "Digiboard", false, NULL},
    {"\x00\x7E", 2, "Codex", false, NULL},
    {"\x00\x82", 2, "Wang Labs", false, NULL},
    {"\x00\x84", 2, "Netexpress Communications", false, NULL},
    {"\x00\x86", 2, "Cable-Sat", false, NULL},
    {"\x00\x88", 2, "MFPA", false, NULL},
    {"\x00\x8A", 2, "Telogy Networks", false, NULL},
    {"\x00\x8E", 2, "Telecom Multimedia Systems", false, NULL},
    {"\x00\x8C", 2, "AT&T", false, NULL},
    {"\x00\x92", 2, "Nuera", false, NULL},
    {"\x00\x94", 2, "K56flex", false, NULL},
    {"\x00\x96", 2, "MiBridge", false, NULL},
    {"\x00\x98", 2, "Xerox", false, NULL},
    {"\x00\x9A", 2, "Fujitsu", false, NULL},
    {"\x00\x9B", 2, "Fujitsu", false, NULL},
    {"\x00\x9C", 2, "Natural Microsystems", false, NULL},
    {"\x00\x9E", 2, "CopyTele", false, NULL},
    {"\x00\xA2", 2, "Murata", false, NULL},
    {"\x00\xA4", 2, "Lanier", false, NULL},
    {"\x00\xA6", 2, "Qualcomm", false, NULL},
    {"\x00\xAA", 2, "HylaFAX", false, NULL},
    {NULL, 0, NULL, false, NULL}
};

static const nsf_data_t vendor_bc[] =
{
    /* France (not Vietnam) */
    {"\x53\x01", 2, "Minolta", false, NULL},
    {NULL, 0, NULL, false, NULL}
};

static const country_code_t t35_country_codes[255] =
{
    {"Japan", vendor_00},                                   /* 0x00 */
    {"Albania", NULL},
    {"Algeria", NULL},
    {"American Samoa", NULL},
    {"Germany", NULL},
    {"Anguilla", NULL},
    {"Antigua and Barbuda", NULL},
    {"Argentina", NULL},
    {"Ascension (see S. Helena)", NULL},
    {"Australia", NULL},
    {"Austria", NULL},
    {"Bahamas", NULL},
    {"Bahrain", NULL},
    {"Bangladesh", NULL},
    {"Barbados", NULL},
    {"Belgium", NULL},
    {"Belize", NULL},                                       /* 0x10 */
    {"Benin (Republic of)", NULL},
    {"Bermudas", NULL},
    {"Bhutan (Kingdom of)", NULL},
    {"Bolivia", NULL},
    {"Botswana", NULL},
    {"Brazil", NULL},
    {"British Antarctic Territory", NULL},
    {"British Indian Ocean Territory", NULL},
    {"British Virgin Islands", NULL},
    {"Brunei Darussalam", NULL},
    {"Bulgaria", NULL},
    {"Myanmar (Union of)", NULL},
    {"Burundi", NULL},
    {"Byelorussia", NULL},
    {"Cameroon", NULL},
    {"Canada", vendor_20},                                  /* 0x20 */
    {"Cape Verde", NULL},
    {"Cayman Islands", NULL},
    {"Central African Republic", NULL},
    {"Chad", NULL},
    {"Chile", NULL},
    {"China", NULL},
    {"Colombia", NULL},
    {"Comoros", NULL},
    {"Congo", NULL},
    {"Cook Islands", NULL},
    {"Costa Rica", NULL},
    {"Cuba", NULL},
    {"Cyprus", NULL},
    {"Czech and Slovak Federal Republic", NULL},            /* 0x30 */
    {"Cambodia", NULL},
    {"Democratic People's Republic of Korea", NULL},
    {"Denmark", NULL},
    {"Djibouti", NULL},
    {"Dominican Republic", NULL},
    {"Dominica", NULL},
    {"Ecuador", NULL},
    {"Egypt", NULL},
    {"El Salvador", NULL},
    {"Equatorial Guinea", NULL},
    {"Ethiopia", NULL},
    {"Falkland Islands", NULL},
    {"Fiji", NULL},
    {"Finland", NULL},
    {"France", NULL},
    {"French Polynesia", NULL},
    {"French Southern and Antarctic Lands", NULL},
    {"Gabon", NULL},                                        /* 0x40 */
    {"Gambia", NULL},
    {"Germany (Federal Republic of)", NULL},
    {"Angola", NULL},
    {"Ghana", NULL},
    {"Gibraltar", NULL},
    {"Greece", NULL},
    {"Grenada", NULL},
    {"Guam", NULL},
    {"Guatemala", NULL},
    {"Guernsey", NULL},
    {"Guinea", NULL},
    {"Guinea-Bissau", NULL},
    {"Guayana", NULL},
    {"Haiti", NULL},
    {"Honduras", NULL},
    {"Hong Kong", NULL},                                    /* 0x50 */
    {"Hungary (Republic of)", NULL},
    {"Iceland", NULL},
    {"India", NULL},
    {"Indonesia", NULL},
    {"Iran (Islamic Republic of)", NULL},
    {"Iraq", NULL},
    {"Ireland", NULL},
    {"Israel", NULL},
    {"Italy", NULL},
    {"Cote d'Ivoire", NULL},
    {"Jamaica", NULL},
    {"Afghanistan", NULL},
    {"Jersey", NULL},
    {"Jordan", NULL},
    {"Kenya", NULL},
    {"Kiribati", NULL},                                     /* 0x60 */
    {"Korea (Republic of)", vendor_61},
    {"Kuwait", NULL},
    {"Lao (People's Democratic Republic)", NULL},
    {"Lebanon", vendor_64},
    {"Lesotho", NULL},
    {"Liberia", NULL},
    {"Libya", NULL},
    {"Liechtenstein", NULL},
    {"Luxembourg", NULL},
    {"Macau", NULL},
    {"Madagascar", NULL},
    {"Malaysia", NULL},
    {"Malawi", NULL},
    {"Maldives", NULL},
    {"Mali", NULL},
    {"Malta", NULL},                                        /* 0x70 */
    {"Mauritania", NULL},
    {"Mauritius", NULL},
    {"Mexico", NULL},
    {"Monaco", NULL},
    {"Mongolia", NULL},
    {"Montserrat", NULL},
    {"Morocco", NULL},
    {"Mozambique", NULL},
    {"Nauru", NULL},
    {"Nepal", NULL},
    {"Netherlands", NULL},
    {"Netherlands Antilles", NULL},
    {"New Caledonia", NULL},
    {"New Zealand", NULL},
    {"Nicaragua", NULL},
    {"Niger", NULL},                                        /* 0x80 */
    {"Nigeria", NULL},
    {"Norway", NULL},
    {"Oman", NULL},
    {"Pakistan", NULL},
    {"Panama", NULL},
    {"Papua New Guinea", vendor_86},
    {"Paraguay", NULL},
    {"Peru", NULL},
    {"Philippines", NULL},
    {"Poland (Republic of)", NULL},
    {"Portugal", NULL},
    {"Puerto Rico", NULL},
    {"Qatar", NULL},
    {"Romania", NULL},
    {"Rwanda", NULL},
    {"Saint Kitts and Nevis", NULL},                        /* 0x90 */
    {"Saint Croix", NULL},
    {"Saint Helena and Ascension", NULL},
    {"Saint Lucia", NULL},
    {"San Marino", NULL},
    {"Saint Thomas", NULL},
    {"Sao Tome and Principe", NULL},
    {"Saint Vincent and the Grenadines", NULL},
    {"Saudi Arabia", NULL},
    {"Senegal", NULL},
    {"Seychelles", NULL},
    {"Sierra Leone", NULL},
    {"Singapore", NULL},
    {"Solomon Islands", NULL},
    {"Somalia", NULL},
    {"South Africa", NULL},
    {"Spain", NULL},                                        /* 0xA0 */
    {"Sri Lanka", NULL},
    {"Sudan", NULL},
    {"Suriname", NULL},
    {"Swaziland", NULL},
    {"Sweden", NULL},
    {"Switzerland", NULL},
    {"Syria", NULL},
    {"Tanzania", NULL},
    {"Thailand", NULL},
    {"Togo", NULL},
    {"Tonga", NULL},
    {"Trinidad and Tobago", NULL},
    {"Tunisia", vendor_ad},
    {"Turkey", NULL},
    {"Turks and Caicos Islands", NULL},
    {"Tuvalu", NULL},                                       /* 0xB0 */
    {"Uganda", NULL},
    {"Ukraine", NULL},
    {"United Arab Emirates", NULL},
    {"United Kingdom", vendor_b4},
    {"United States", vendor_b5},
    {"Burkina Faso", NULL},
    {"Uruguay", NULL},
    {"U.S.S.R.", NULL},
    {"Vanuatu", NULL},
    {"Vatican City State", NULL},
    {"Venezuela", NULL},
    {"Vietnam", vendor_bc},
    {"Wallis and Futuna", NULL},
    {"Western Samoa", NULL},
    {"Yemen (Republic of)", NULL},
    {"Yemen (Republic of)", NULL},                          /* 0xC0 */
    {"Yugoslavia", NULL},
    {"Zaire", NULL},
    {"Zambia", NULL},
    {"Zimbabwe", NULL},
    {"Slovakia", NULL},
    {"Slovenia", NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},                                           /* 0xD0 */
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},                                           /* 0xE0 */
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},                                           /* 0xF0 */
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {"Lithuania", NULL},
    {"Latvia", NULL},
    {"Estonia", NULL},
    {"US Virgin Islands", NULL},
    {NULL, NULL},
    {NULL, NULL},
    {"(Universal)", NULL},
    {"Taiwan", NULL}
};

SPAN_DECLARE(int) t35_real_country_code(int country_code, int country_code_extension)
{
    if (country_code < 0  ||  country_code > 0xFF)
        return -1;
    if (country_code == 0xFF)
    {
        /* The extension code gives us the country. */
        /* Right now there are no extension codes defined by the ITU */
        return -1;
    }
    /* We need to apply realism over accuracy, though it blocks out some countries.
       It is very rare to find a machine from any country but the following:

            Japan 0x00 (no confusion)
            Germany 0x04 (0x20) (Canada/Germany confusion)
            China 0x26 (0x64) (China/Lebanon confusion)
            Korea 0x61 (0x86) (Korea/Papua New Guinea confusion)
            UK 0xB4 (0x2D) (UK/Cyprus confusion)
            USA 0xB5 (0xAD) (USA/Tunisia confusion)
            France 0x3D (0xBC) (France/Vietnam confusion)

       If we force the most likely of the two possible countries (forward or bit reversed),
       the only mixup with any realistic probability is the Canada/Germany confusion. We
       will just live with this, and force the more likely countries. */
    switch (country_code)
    {
    case 0x20:
        /* Force Germany */
    case 0x2D:
        /* Force UK */
    case 0x64:
        /* Force China */
    case 0x86:
        /* Force Korea */
    case 0xAD:
        /* Force USA */
    case 0xBC:
        /* Force France */
        country_code = bit_reverse8(country_code);
        break;
    }
    /* Try the country code at face value, then bit reversed */
    if (t35_country_codes[country_code].name)
        return country_code;
    /* If the country code is missing, its most likely the country code is reversed. */
    country_code = bit_reverse8(country_code);
    if (t35_country_codes[country_code].name)
        return country_code;
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t35_real_country_code_to_str(int country_code, int country_code_extension)
{
    int real_code;

    if ((real_code = t35_real_country_code(country_code, country_code_extension)) >= 0)
        return t35_country_codes[real_code].name;
    return NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t35_country_code_to_str(int country_code, int country_code_extension)
{
    if (country_code < 0  ||  country_code > 0xFF)
        return NULL;
    if (country_code == 0xFF)
    {
        /* The extension code gives us the country. */
        /* Right now there are no extension codes defined by the ITU */
        return NULL;
    }

    return t35_country_codes[country_code].name;
}
/*- End of function --------------------------------------------------------*/

static const nsf_data_t *find_vendor(const uint8_t *msg, int len)
{
    const nsf_data_t *vendors;
    const nsf_data_t *p;
    int real_country_code;

    if (msg[0] == 0xFF)
    {
        /* The extension code gives us the country. */
        /* Right now there are no extension codes defined by the ITU */
        return NULL;
    }
    if ((real_country_code = t35_real_country_code(msg[0], msg[1])) < 0)
        return NULL;
    if ((vendors = t35_country_codes[msg[0]].vendors) == NULL)
        return NULL;
    for (p = vendors;  p->vendor_id;  p++)
    {
        if (len >= p->vendor_id_len
            &&
            memcmp(p->vendor_id, &msg[1], p->vendor_id_len) == 0)
        {
            return p;
        }
    }
    return NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t35_vendor_to_str(const uint8_t *msg, int len)
{
    const nsf_data_t *p;

    if ((p = find_vendor(msg, len)) == NULL)
        return NULL;
    return p->vendor_name;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bool) t35_decode(const uint8_t *msg, int len, const char **country, const char **vendor, const char **model)
{
    const nsf_data_t *p;
    const model_data_t *pp;

    if (country)
        *country = t35_real_country_code_to_str(msg[0], msg[1]);
    if (vendor)
        *vendor = NULL;
    if (model)
        *model = NULL;

    if ((p = find_vendor(msg, len)) == NULL)
        return false;
    if (vendor)
        *vendor = p->vendor_name;
    if (model  &&  p->known_models)
    {
        for (pp = p->known_models;  pp->model_id;  pp++)
        {
            if (len == 1 + p->vendor_id_len + pp->model_id_size
                &&
                memcmp(&msg[1 + p->vendor_id_len], pp->model_id, pp->model_id_size) == 0)
            {
                *model = pp->model_name;
                break;
            }
        }
    }
    return true;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
