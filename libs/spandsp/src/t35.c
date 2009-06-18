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
 *
 * $Id: t35.c,v 1.31 2009/05/16 03:34:45 steveu Exp $
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
    /*! TRUE if the station ID for this vendor is reversed */
    int inverse_station_id_order;
    /*! A pointer to a list of known models from this vendor */
    const model_data_t *known_models;
} nsf_data_t;

const char *t35_country_codes[256] =
{
    "Japan",                                    /* 0x00 */
    "Albania",
    "Algeria",
    "American Samoa",
    "Germany",
    "Anguilla",
    "Antigua and Barbuda",
    "Argentina",
    "Ascension (see S. Helena)",
    "Australia",
    "Austria",
    "Bahamas",
    "Bahrain",
    "Bangladesh",
    "Barbados",
    "Belgium",
    "Belize",
    "Benin (Republic of)",
    "Bermudas",
    "Bhutan (Kingdom of)",
    "Bolivia",
    "Botswana",
    "Brazil",
    "British Antarctic Territory",
    "British Indian Ocean Territory",
    "British Virgin Islands",
    "Brunei Darussalam",
    "Bulgaria",
    "Myanmar (Union of)",
    "Burundi",
    "Byelorussia",
    "Cameroon",
    "Canada",                                   /* 0x20 */
    "Cape Verde",
    "Cayman Islands",
    "Central African Republic",
    "Chad",
    "Chile",
    "China",
    "Colombia",
    "Comoros",
    "Congo",
    "Cook Islands",
    "Costa Rica",
    "Cuba",
    "Cyprus",
    "Czech and Slovak Federal Republic",
    "Cambodia",
    "Democratic People's Republic of Korea",
    "Denmark",
    "Djibouti",
    "Dominican Republic",
    "Dominica",
    "Ecuador",
    "Egypt",
    "El Salvador",
    "Equatorial Guinea",
    "Ethiopia",
    "Falkland Islands",
    "Fiji",
    "Finland",
    "France",
    "French Polynesia",
    "French Southern and Antarctic Lands",
    "Gabon",                                        /* 0x40 */
    "Gambia",
    "Germany (Federal Republic of)",
    "Angola",
    "Ghana",
    "Gibraltar",
    "Greece",
    "Grenada",
    "Guam",
    "Guatemala",
    "Guernsey",
    "Guinea",
    "Guinea-Bissau",
    "Guayana",
    "Haiti",
    "Honduras",
    "Hong Kong",
    "Hungary (Republic of)",
    "Iceland",
    "India",
    "Indonesia",
    "Iran (Islamic Republic of)",
    "Iraq",
    "Ireland",
    "Israel",
    "Italy",
    "Cote d'Ivoire",
    "Jamaica",
    "Afghanistan",
    "Jersey",
    "Jordan",
    "Kenya",
    "Kiribati",                                     /* 0x60 */
    "Korea (Republic of)",
    "Kuwait",
    "Lao (People's Democratic Republic)",
    "Lebanon",
    "Lesotho",
    "Liberia",
    "Libya",
    "Liechtenstein",
    "Luxembourg",
    "Macau",
    "Madagascar",
    "Malaysia",
    "Malawi",
    "Maldives",
    "Mali",
    "Malta",
    "Mauritania",
    "Mauritius",
    "Mexico",
    "Monaco",
    "Mongolia",
    "Montserrat",
    "Morocco",
    "Mozambique",
    "Nauru",
    "Nepal",
    "Netherlands",
    "Netherlands Antilles",
    "New Caledonia",
    "New Zealand",
    "Nicaragua",
    "Niger",                                        /* 0x80 */
    "Nigeria",
    "Norway",
    "Oman",
    "Pakistan",
    "Panama",
    "Papua New Guinea",
    "Paraguay",
    "Peru",
    "Philippines",
    "Poland (Republic of)",
    "Portugal",
    "Puerto Rico",
    "Qatar",
    "Romania",
    "Rwanda",
    "Saint Kitts and Nevis",
    "Saint Croix",
    "Saint Helena and Ascension",
    "Saint Lucia",
    "San Marino",
    "Saint Thomas",
    "Sao Tome and Principe",
    "Saint Vincent and the Grenadines",
    "Saudi Arabia",
    "Senegal",
    "Seychelles",
    "Sierra Leone",
    "Singapore",
    "Solomon Islands",
    "Somalia",
    "South Africa",
    "Spain",                                        /* 0xA0 */
    "Sri Lanka",
    "Sudan",
    "Suriname",
    "Swaziland",
    "Sweden",
    "Switzerland",
    "Syria",
    "Tanzania",
    "Thailand",
    "Togo",
    "Tonga",
    "Trinidad and Tobago",
    "Tunisia",
    "Turkey",
    "Turks and Caicos Islands",
    "Tuvalu",
    "Uganda",
    "Ukraine",
    "United Arab Emirates",
    "United Kingdom",
    "United States",
    "Burkina Faso",
    "Uruguay",
    "U.S.S.R.",
    "Vanuatu",
    "Vatican City State",
    "Venezuela",
    "Viet Nam",
    "Wallis and Futuna",
    "Western Samoa",
    "Yemen (Republic of)",
    "Yemen (Republic of)",                          /* 0xC0 */
    "Yugoslavia",
    "Zaire",
    "Zambia",
    "Zimbabwe"
    "Slovakia",
    "Slovenia",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",                                  /* 0xD0 */
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",                                  /* 0xE0 */
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",                                  /* 0xF0 */
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "(available)",
    "Lithuania",
    "Latvia",
    "Estonia",
    "US Virgin Islands",
    "(available)",
    "(available)",
    "(Universal)",
    "Taiwan",
    "(extension)"
};

static const model_data_t Canon[] =
{
    {5, "\x80\x00\x80\x48\x00", "Faxphone B640"},
    {5, "\x80\x00\x80\x49\x10", "Fax B100"},
    {5, "\x80\x00\x8A\x49\x10", "Laser Class 9000 Series"},
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
    {10, "\x00\x00\x00\x96\x0F\x01\x03\x00\x10\x05\x02\x95\xC8\x08\x01\x49\x02                \x03", "KX-F230 or KX-FT21 or ..."},
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
static const nsf_data_t known_nsf[] =
{
    /* Japan */
    {"\x00\x00\x00", 3, "Unknown - indeterminate", TRUE, NULL},
    {"\x00\x00\x01", 3, "Anjitsu", FALSE, NULL},
    {"\x00\x00\x02", 3, "Nippon Telephone", FALSE, NULL},
    {"\x00\x00\x05", 3, "Mitsuba Electric", FALSE, NULL},
    {"\x00\x00\x06", 3, "Master Net", FALSE, NULL},
    {"\x00\x00\x09", 3, "Xerox/Toshiba", TRUE, Xerox},
    {"\x00\x00\x0A", 3, "Kokusai", FALSE, NULL},
    {"\x00\x00\x0D", 3, "Logic System International", FALSE, NULL},
    {"\x00\x00\x0E", 3, "Panasonic", FALSE, Panasonic0E},
    {"\x00\x00\x11", 3, "Canon", FALSE, Canon},
    {"\x00\x00\x15", 3, "Toyotsushen Machinery", FALSE, NULL},
    {"\x00\x00\x16", 3, "System House Mind", FALSE, NULL},
    {"\x00\x00\x19", 3, "Xerox", TRUE, NULL},
    {"\x00\x00\x1D", 3, "Hitachi Software", FALSE, NULL},
    {"\x00\x00\x21", 3, "OKI Electric/Lanier", TRUE, NULL},
    {"\x00\x00\x25", 3, "Ricoh", TRUE, Ricoh},
    {"\x00\x00\x26", 3, "Konica", FALSE, NULL},
    {"\x00\x00\x29", 3, "Japan Wireless", FALSE, NULL},
    {"\x00\x00\x2D", 3, "Sony", FALSE, NULL},
    {"\x00\x00\x31", 3, "Sharp/Olivetti", FALSE, Sharp},
    {"\x00\x00\x35", 3, "Kogyu", FALSE, NULL},
    {"\x00\x00\x36", 3, "Japan Telecom", FALSE, NULL},
    {"\x00\x00\x3D", 3, "IBM Japan", FALSE, NULL},
    {"\x00\x00\x39", 3, "Panasonic", FALSE, NULL},
    {"\x00\x00\x41", 3, "Swasaki Communication", FALSE, NULL},
    {"\x00\x00\x45", 3, "Muratec", FALSE, Muratec45},
    {"\x00\x00\x46", 3, "Pheonix", FALSE, NULL},
    {"\x00\x00\x48", 3, "Muratec", FALSE, Muratec48},	// not registered
    {"\x00\x00\x49", 3, "Japan Electric", FALSE, NULL},
    {"\x00\x00\x4D", 3, "Okura Electric", FALSE, NULL},
    {"\x00\x00\x51", 3, "Sanyo", FALSE, Sanyo},
    {"\x00\x00\x55", 3, "unknown - Japan 55", FALSE, NULL},
    {"\x00\x00\x56", 3, "Brother", FALSE, Brother},
    {"\x00\x00\x59", 3, "Fujitsu", FALSE, NULL},
    {"\x00\x00\x5D", 3, "Kuoni", FALSE, NULL},
    {"\x00\x00\x61", 3, "Casio", FALSE, NULL},
    {"\x00\x00\x65", 3, "Tateishi Electric", FALSE, NULL},
    {"\x00\x00\x66", 3, "Utax/Mita", TRUE, NULL},
    {"\x00\x00\x69", 3, "Hitachi Production", FALSE, NULL},
    {"\x00\x00\x6D", 3, "Hitachi Telecom", FALSE, NULL},
    {"\x00\x00\x71", 3, "Tamura Electric Works", FALSE, NULL},
    {"\x00\x00\x75", 3, "Tokyo Electric Corp.", FALSE, NULL},
    {"\x00\x00\x76", 3, "Advance", FALSE, NULL},
    {"\x00\x00\x79", 3, "Panasonic", FALSE, Panasonic79},
    {"\x00\x00\x7D", 3, "Seiko", FALSE, NULL},
    {"\x00\x08\x00", 3, "Daiko", FALSE, NULL},
    {"\x00\x10\x00", 3, "Funai Electric", FALSE, NULL},
    {"\x00\x20\x00", 3, "Eagle System", FALSE, NULL},
    {"\x00\x30\x00", 3, "Nippon Business Systems", FALSE, NULL},
    {"\x00\x40\x00", 3, "Comtron", FALSE, NULL},
    {"\x00\x48\x00", 3, "Cosmo Consulting", FALSE, NULL},
    {"\x00\x50\x00", 3, "Orion Electric", FALSE, NULL},
    {"\x00\x60\x00", 3, "Nagano Nippon", FALSE, NULL},
    {"\x00\x70\x00", 3, "Kyocera", FALSE, NULL},
    {"\x00\x80\x00", 3, "Kanda Networks", FALSE, NULL},
    {"\x00\x88\x00", 3, "Soft Front", FALSE, NULL},
    {"\x00\x90\x00", 3, "Arctic", FALSE, NULL},
    {"\x00\xA0\x00", 3, "Nakushima", FALSE, NULL},
    {"\x00\xB0\x00", 3, "Minolta", FALSE, NULL},
    {"\x00\xC0\x00", 3, "Tohoku Pioneer", FALSE, NULL},
    {"\x00\xD0\x00", 3, "USC", FALSE, NULL},
    {"\x00\xE0\x00", 3, "Hiboshi", FALSE, NULL},
    {"\x00\xF0\x00", 3, "Sumitomo Electric", FALSE, NULL},

    /* Germany */
    {"\x20\x09",     2, "ITK Institut für Telekommunikation GmbH & Co KG", FALSE, NULL},
    {"\x20\x11",     2, "Dr. Neuhaus Mikroelektronik", FALSE, NULL},
    {"\x20\x21",     2, "ITO Communication", FALSE, NULL},
    {"\x20\x31",     2, "mbp Kommunikationssysteme GmbH", FALSE, NULL},
    {"\x20\x41",     2, "Siemens", FALSE, NULL},
    {"\x20\x42",     2, "Deutsche Telekom AG", FALSE, NULL},
    {"\x20\x51",     2, "mps Software", FALSE, NULL},
    {"\x20\x61",     2, "Hauni Elektronik", FALSE, NULL},
    {"\x20\x71",     2, "Digitronic computersysteme gmbh", FALSE, NULL},
    {"\x20\x81\x00", 3, "Innovaphone GmbH", FALSE, NULL},
    {"\x20\x81\x40", 3, "TEDAS Gesellschaft für Telekommunikations-, Daten- und Audiosysteme mbH", FALSE, NULL},
    {"\x20\x81\x80", 3, "AVM Audiovisuelles Marketing und Computersysteme GmbH", FALSE, NULL},
    {"\x20\x81\xC0", 3, "EICON Technology Research GmbH", FALSE, NULL},
    {"\x20\xB1",     2, "Schneider Rundfunkwerke AG", FALSE, NULL},
    {"\x20\xC2",     2, "Deutsche Telekom AG", FALSE, NULL},
    {"\x20\xD1",     2, "Ferrari electronik GmbH", FALSE, NULL},
    {"\x20\xF1",     2, "DeTeWe - Deutsche Telephonwerke AG & Co", FALSE, NULL},
    {"\x20\xFF",     2, "Germany Regional Code", FALSE, NULL},

    /* China */
    {"\x64\x00\x00", 3, "unknown - China 00 00", FALSE, NULL},
    {"\x64\x01\x00", 3, "unknown - China 01 00", FALSE, NULL},
    {"\x64\x01\x01", 3, "unknown - China 01 01", FALSE, NULL},
    {"\x64\x01\x02", 3, "unknown - China 01 02", FALSE, NULL},

    /* France */
    {"\xBC\x53\x01", 3, "Minolta", FALSE, NULL},

    /* Korea */
    {"\x86\x00\x02", 3, "unknown - Korea 02", FALSE, NULL},
    {"\x86\x00\x06", 3, "unknown - Korea 06", FALSE, NULL},
    {"\x86\x00\x08", 3, "unknown - Korea 08", FALSE, NULL},
    {"\x86\x00\x0A", 3, "unknown - Korea 0A", FALSE, NULL},
    {"\x86\x00\x0E", 3, "unknown - Korea 0E", FALSE, NULL},
    {"\x86\x00\x10", 3, "Samsung", FALSE, NULL},
    {"\x86\x00\x11", 3, "unknown - Korea 11", FALSE, NULL},
    {"\x86\x00\x16", 3, "Samsung", FALSE, Samsung16},
    {"\x86\x00\x1A", 3, "unknown - Korea 1A", FALSE, NULL},
    {"\x86\x00\x40", 3, "unknown - Korea 40", FALSE, NULL},
    {"\x86\x00\x48", 3, "unknown - Korea 48", FALSE, NULL},
    {"\x86\x00\x52", 3, "unknown - Korea 52", FALSE, NULL},
    {"\x86\x00\x5A", 3, "Samsung", FALSE, Samsung5A},
    {"\x86\x00\x5E", 3, "unknown - Korea 5E", FALSE, NULL},
    {"\x86\x00\x66", 3, "unknown - Korea 66", FALSE, NULL},
    {"\x86\x00\x6E", 3, "unknown - Korea 6E", FALSE, NULL},
    {"\x86\x00\x82", 3, "unknown - Korea 82", FALSE, NULL},
    {"\x86\x00\x88", 3, "unknown - Korea 88", FALSE, NULL},
    {"\x86\x00\x8A", 3, "unknown - Korea 8A", FALSE, NULL},
    {"\x86\x00\x8C", 3, "Samsung", FALSE, Samsung8C},
    {"\x86\x00\x92", 3, "unknown - Korea 92", FALSE, NULL},
    {"\x86\x00\x98", 3, "Samsung", FALSE, NULL},
    {"\x86\x00\xA2", 3, "Samsung", FALSE, SamsungA2},
    {"\x86\x00\xA4", 3, "unknown - Korea A4", FALSE, NULL},
    {"\x86\x00\xC2", 3, "Samsung", FALSE, NULL},
    {"\x86\x00\xC9", 3, "unknown - Korea C9", FALSE, NULL},
    {"\x86\x00\xCC", 3, "unknown - Korea CC", FALSE, NULL},
    {"\x86\x00\xD2", 3, "unknown - Korea D2", FALSE, NULL},
    {"\x86\x00\xDA", 3, "Xerox", FALSE, XeroxDA},
    {"\x86\x00\xE2", 3, "unknown - Korea E2", FALSE, NULL},
    {"\x86\x00\xEC", 3, "unknown - Korea EC", FALSE, NULL},
    {"\x86\x00\xEE", 3, "unknown - Korea EE", FALSE, NULL},

    /* United Kingdom */
    {"\xB4\x00\xB0", 3, "DCE", FALSE, NULL},
    {"\xB4\x00\xB1", 3, "Hasler", FALSE, NULL},
    {"\xB4\x00\xB2", 3, "Interquad", FALSE, NULL},
    {"\xB4\x00\xB3", 3, "Comwave", FALSE, NULL},
    {"\xB4\x00\xB4", 3, "Iconographic", FALSE, NULL},
    {"\xB4\x00\xB5", 3, "Wordcraft", FALSE, NULL},
    {"\xB4\x00\xB6", 3, "Acorn", FALSE, NULL},

    /* United States */
    {"\xAD\x00\x00", 3, "Pitney Bowes", FALSE, PitneyBowes},
    {"\xAD\x00\x0C", 3, "Dialogic", FALSE, NULL},
    {"\xAD\x00\x15", 3, "Lexmark", FALSE, Lexmark},
    {"\xAD\x00\x16", 3, "JetFax", FALSE, JetFax},
    {"\xAD\x00\x24", 3, "Octel", FALSE, NULL},
    {"\xAD\x00\x36", 3, "HP", FALSE, HP},
    {"\xAD\x00\x42", 3, "FaxTalk", FALSE, NULL},
    {"\xAD\x00\x44", 3, NULL, TRUE, NULL},
    {"\xAD\x00\x46", 3, "BrookTrout", FALSE, NULL},
    {"\xAD\x00\x51", 3, "Telogy Networks", FALSE, NULL},
    {"\xAD\x00\x55", 3, "HylaFAX", FALSE, NULL},
    {"\xAD\x00\x5C", 3, "IBM", FALSE, NULL},
    {"\xAD\x00\x98", 3, "unknown - USA 98", TRUE, NULL},
    {"\xB5\x00\x01", 3, "Picturetel", FALSE, NULL},
    {"\xB5\x00\x20", 3, "Conexant", FALSE, NULL},
    {"\xB5\x00\x22", 3, "Comsat", FALSE, NULL},
    {"\xB5\x00\x24", 3, "Octel", FALSE, NULL},
    {"\xB5\x00\x26", 3, "ROLM", FALSE, NULL},
    {"\xB5\x00\x28", 3, "SOFNET", FALSE, NULL},
    {"\xB5\x00\x29", 3, "TIA TR-29 Committee", FALSE, NULL},
    {"\xB5\x00\x2A", 3, "STF Tech", FALSE, NULL},
    {"\xB5\x00\x2C", 3, "HKB", FALSE, NULL},
    {"\xB5\x00\x2E", 3, "Delrina", FALSE, NULL},
    {"\xB5\x00\x30", 3, "Dialogic", FALSE, NULL},
    {"\xB5\x00\x32", 3, "Applied Synergy", FALSE, NULL},
    {"\xB5\x00\x34", 3, "Syncro Development", FALSE, NULL},
    {"\xB5\x00\x36", 3, "Genoa", FALSE, NULL},
    {"\xB5\x00\x38", 3, "Texas Instruments", FALSE, NULL},
    {"\xB5\x00\x3A", 3, "IBM", FALSE, NULL},
    {"\xB5\x00\x3C", 3, "ViaSat", FALSE, NULL},
    {"\xB5\x00\x3E", 3, "Ericsson", FALSE, NULL},
    {"\xB5\x00\x42", 3, "Bogosian", FALSE, NULL},
    {"\xB5\x00\x44", 3, "Adobe", FALSE, NULL},
    {"\xB5\x00\x46", 3, "Fremont Communications", FALSE, NULL},
    {"\xB5\x00\x48", 3, "Hayes", FALSE, NULL},
    {"\xB5\x00\x4A", 3, "Lucent", FALSE, NULL},
    {"\xB5\x00\x4C", 3, "Data Race", FALSE, NULL},
    {"\xB5\x00\x4E", 3, "TRW", FALSE, NULL},
    {"\xB5\x00\x52", 3, "Audiofax", FALSE, NULL},
    {"\xB5\x00\x54", 3, "Computer Automation", FALSE, NULL},
    {"\xB5\x00\x56", 3, "Serca", FALSE, NULL},
    {"\xB5\x00\x58", 3, "Octocom", FALSE, NULL},
    {"\xB5\x00\x5C", 3, "Power Solutions", FALSE, NULL},
    {"\xB5\x00\x5A", 3, "Digital Sound", FALSE, NULL},
    {"\xB5\x00\x5E", 3, "Pacific Data", FALSE, NULL},
    {"\xB5\x00\x60", 3, "Commetrex", FALSE, NULL},
    {"\xB5\x00\x62", 3, "BrookTrout", FALSE, NULL},
    {"\xB5\x00\x64", 3, "Gammalink", FALSE, NULL},
    {"\xB5\x00\x66", 3, "Castelle", FALSE, NULL},
    {"\xB5\x00\x68", 3, "Hybrid Fax", FALSE, NULL},
    {"\xB5\x00\x6A", 3, "Omnifax", FALSE, NULL},
    {"\xB5\x00\x6C", 3, "HP", FALSE, NULL},
    {"\xB5\x00\x6E", 3, "Microsoft", FALSE, NULL},
    {"\xB5\x00\x72", 3, "Speaking Devices", FALSE, NULL},
    {"\xB5\x00\x74", 3, "Compaq", FALSE, NULL},
/*
    {"\xB5\x00\x76", 3, "Trust - Cryptek", FALSE, NULL},	// collision with Microsoft
*/
    {"\xB5\x00\x76", 3, "Microsoft", FALSE, NULL},		// uses LSB for country but MSB for manufacturer
    {"\xB5\x00\x78", 3, "Cylink", FALSE, NULL},
    {"\xB5\x00\x7A", 3, "Pitney Bowes", FALSE, NULL},
    {"\xB5\x00\x7C", 3, "Digiboard", FALSE, NULL},
    {"\xB5\x00\x7E", 3, "Codex", FALSE, NULL},
    {"\xB5\x00\x82", 3, "Wang Labs", FALSE, NULL},
    {"\xB5\x00\x84", 3, "Netexpress Communications", FALSE, NULL},
    {"\xB5\x00\x86", 3, "Cable-Sat", FALSE, NULL},
    {"\xB5\x00\x88", 3, "MFPA", FALSE, NULL},
    {"\xB5\x00\x8A", 3, "Telogy Networks", FALSE, NULL},
    {"\xB5\x00\x8E", 3, "Telecom Multimedia Systems", FALSE, NULL},
    {"\xB5\x00\x8C", 3, "AT&T", FALSE, NULL},
    {"\xB5\x00\x92", 3, "Nuera", FALSE, NULL},
    {"\xB5\x00\x94", 3, "K56flex", FALSE, NULL},
    {"\xB5\x00\x96", 3, "MiBridge", FALSE, NULL},
    {"\xB5\x00\x98", 3, "Xerox", FALSE, NULL},
    {"\xB5\x00\x9A", 3, "Fujitsu", FALSE, NULL},
    {"\xB5\x00\x9B", 3, "Fujitsu", FALSE, NULL},
    {"\xB5\x00\x9C", 3, "Natural Microsystems", FALSE, NULL},
    {"\xB5\x00\x9E", 3, "CopyTele", FALSE, NULL},
    {"\xB5\x00\xA2", 3, "Murata", FALSE, NULL},
    {"\xB5\x00\xA4", 3, "Lanier", FALSE, NULL},
    {"\xB5\x00\xA6", 3, "Qualcomm", FALSE, NULL},
    {"\xB5\x00\xAA", 3, "HylaFAX", FALSE, NULL},		// we did it backwards for a while
    {NULL, 0, NULL, FALSE, NULL}
};

#if 0
SPAN_DECLARE(void) nsf_find_station_id(int reverse_order)
{
    const char *id = NULL;
    int idSize = 0;
    const char *maxId = NULL;
    int maxIdSize = 0;
    const char *p;

    /* Trying to find the longest printable ASCII sequence */
    for (p = (const char *) nsf + T35_VENDOR_ID_LEN, *end = p + nsf.length();
         p < end;
         p++)
    {
        if (isprint(*p))
        {
            if (!idSize++)
                id = p;
            if (idSize > maxIdSize)
            {
                max_id = id;
                max_id_size = idSize;
            }
        }
        else
        {
            id = NULL;
            id_size = 0;
        }
    }
    
    /* Minimum acceptable id length */
    const int MinIdSize = 4;

    if (maxIdSize >= min_id_size)
    {
        stationId.resize(0);
        const char *p;
        int dir;

        if (reverseOrder)
        {
            p = maxId + maxIdSize - 1;
            dir = -1;
        }
        else
        {
            p = maxId;
            dir = 1;
        }
        for (int i = 0;  i < maxIdSize;  i++)
        {
            stationId.append(*p);
            p += dir;
        }
        station_id_decoded = TRUE;
    }
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(int) t35_decode(const uint8_t *msg, int len, const char **country, const char **vendor, const char **model)
{
    int vendor_decoded;
    const nsf_data_t *p;
    const model_data_t *pp;

    vendor_decoded = FALSE;
    if (country)
        *country = NULL;
    if (vendor)
        *vendor = NULL;
    if (model)
        *model = NULL;
    if (country)
    {
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
        switch (msg[0])
        {
        case 0x20:
            /* Force Germany */
            *country = t35_country_codes[0x04];
            break;
        case 0x64:
            /* Force China */
            *country = t35_country_codes[0x26];
            break;
        case 0x86:
            /* Force Korea */
            *country = t35_country_codes[0x61];
            break;
        case 0x2D:
            /* Force UK */
            *country = t35_country_codes[0xB4];
            break;
        case 0xAD:
            /* Force USA */
            *country = t35_country_codes[0xB5];
            break;
        case 0xBC:
            /* Force France */
            *country = t35_country_codes[0x3D];
            break;
        default:
            /* Try the country code at face value, then bit reversed */
            if (t35_country_codes[msg[0]])
                *country = t35_country_codes[msg[0]];
            else if (t35_country_codes[bit_reverse8(msg[0])])
                *country = t35_country_codes[bit_reverse8(msg[0])];
            break;
        }
    }
    for (p = known_nsf;  p->vendor_id;  p++)
    {
        if (len >= p->vendor_id_len
            &&
            memcmp(p->vendor_id, msg, p->vendor_id_len) == 0)
        {
            if (p->vendor_name  &&  vendor)
                *vendor = p->vendor_name;
            if (p->known_models  &&  model)
            {
                for (pp = p->known_models;  pp->model_id;  pp++)
                {
                    if (len == p->vendor_id_len + pp->model_id_size
                        &&
                        memcmp(pp->model_id, &msg[p->vendor_id_len], pp->model_id_size) == 0)
                    {
                        *model = pp->model_name;
                        break;
                    }
                }
            }
#if 0
            findStationId(p->inverse_station_id_order);
#endif
            vendor_decoded = TRUE;
            break;
        }
    }
#if 0
    if (!vendor_found())
        find_station_id(0);
#endif
    return vendor_decoded;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
