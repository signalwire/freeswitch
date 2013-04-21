/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t30_dis_dtc_dcs_bits.h - ITU T.30 fax control bits definitions
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

/*! \file */

#if !defined(_SPANDSP_PRIVATE_T30_DIS_DTC_DCS_BITS_H_)
#define _SPANDSP_PRIVATE_T30_DIS_DTC_DCS_BITS_H_

/* Indicates that the terminal has the Simple mode capability defined in ITU-T Rec. T.37.
   Internet address signals CIA, TSA or CSA can be sent and received. The recipient terminal
   may process or ignore this signal. */
#define T30_DIS_BIT_T37                                     1
#define T30_DCS_BIT_T37                                     1

/* Bit 2 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

/* Indicates that the terminal has the capability to communicate using ITU-T Rec. T.38.
   Internet address signals CIA, TSA or CSA can be sent and received. The recipient terminal
   may process or ignore this signal. */
#define T30_DIS_BIT_T38                                     3
#define T30_DCS_BIT_T38                                     3

/* Bit 4 set to "1" indicates 3rd Generation Mobile Network Access to the GSTN Connection.
   Bit 4 set to "0" conveys no information about the type of connection. */
#define T30_DIS_BIT_3G_MOBILE                               4
#define T30_DCS_BIT_3G_MOBILE                               4

/* Bit 5 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

/* When ISDN mode is used, in DIS/DTC bit 6 shall be set to "0". */
#define T30_DIS_BIT_V8_CAPABILITY                           6
/* Bit 6 in a DCS is "invalid", and should be set to zero */

/* When ISDN mode is used, in DIS/DTC bit 7 shall be set to "0". */
#define T30_DIS_BIT_64_OCTET_ECM_FRAMES_PREFERRED           7
/* Bit 7 in a DCS is "invalid", and should be set to zero */

/* Bit 8 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

/* Bit 9 indicates that there is a facsimile document ready to be polled from the answering
   terrminal. It is not an indication of a capability. */
#define T30_DIS_BIT_READY_TO_TRANSMIT_FAX_DOCUMENT          9
/* Bit 9 in a DCS should be set to zero */

/* In DIS/DTC bit 10 indicates that the answering terminal has receiving capabilities.
   In DCS it is a command to the receiving terminal to set itself in the receive mode. */
#define T30_DIS_BIT_READY_TO_RECEIVE_FAX_DOCUMENT           10
#define T30_DCS_BIT_RECEIVE_FAX_DOCUMENT                    10

/* Bits 11, 12, 13, 14 - modem type */

#define T30_DIS_BIT_200_200_CAPABLE                         15
#define T30_DCS_BIT_200_200                                 15

#define T30_DIS_BIT_2D_CAPABLE                              16
#define T30_DCS_BIT_2D_MODE                                 16

/* Standard facsimile terminals conforming to ITU-T Rec. T.4 must have the following capability:
   Paper length = 297 mm. */

#define T30_DIS_BIT_215MM_255MM_WIDTH_CAPABLE               17
#define T30_DCS_BIT_255MM_WIDTH                             17

#define T30_DIS_BIT_215MM_255MM_303MM_WIDTH_CAPABLE         18
#define T30_DCS_BIT_303MM_WIDTH                             18

#define T30_DIS_BIT_A4_B4_LENGTH_CAPABLE                    19
#define T30_DCS_BIT_B4_LENGTH                               19

#define T30_DIS_BIT_UNLIMITED_LENGTH_CAPABLE                20
#define T30_DCS_BIT_UNLIMITED_LENGTH                        20

/* Bits 21, 22, 23 - min scan line time */
/* When ISDN mode is used, in DIS/DTC bits 21 to 23 shall be set to "1". */
#define T30_DIS_BIT_MIN_SCAN_LINE_TIME_CAPABILITY_1         21
#define T30_DCS_BIT_MIN_SCAN_LINE_TIME_1                    21
#define T30_DIS_BIT_MIN_SCAN_LINE_TIME_CAPABILITY_2         22
#define T30_DCS_BIT_MIN_SCAN_LINE_TIME_2                    22
#define T30_DIS_BIT_MIN_SCAN_LINE_TIME_CAPABILITY_3         23
#define T30_DCS_BIT_MIN_SCAN_LINE_TIME_3                    23
#define T30_DIS_BIT_MIN_SCAN_LINE_TIME_CAPABILITY_4         24
#define T30_DCS_BIT_MIN_SCAN_LINE_TIME_4                    24

/* Bit 24 is an extension bit */

/* Bit 25 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

#define T30_DIS_BIT_UNCOMPRESSED_CAPABLE                    26
#define T30_DCS_BIT_UNCOMPRESSED_MODE                       26

/* When ISDN mode is used, in DIS/DTC bit 27 shall be set to "1". */
#define T30_DIS_BIT_ECM_CAPABLE                             27
#define T30_DCS_BIT_ECM_MODE                                27

/* Bit 28 in a DIS or DTC should be set to zero */
/* (T.30 note 7) The value of bit 28 in the DCS command is only valid when ECM is selected. */
#define T30_DCS_BIT_64_OCTET_ECM_FRAMES                     28

/* Bit 29 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

/* Bit 30 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

/* (T.30 note 9) The value of bit 31 in the DCS command is only valid when ECM is selected. */
#define T30_DIS_BIT_T6_CAPABLE                              31
#define T30_DCS_BIT_T6_MODE                                 31

/* Bit 32 is an extension bit */

#define T30_DIS_BIT_FNV_CAPABLE                             33
#define T30_DCS_BIT_FNV_CAPABLE                             33

#define T30_DIS_BIT_MULTIPLE_SELECTIVE_POLLING_CAPABLE      34
/* Bit 34 in a DCS should be set to zero */

#define T30_DIS_BIT_POLLED_SUBADDRESSING_CAPABLE            35
/* Bit 35 in a DCS should be set to zero */

#define T30_DIS_BIT_T43_CAPABLE                             36
#define T30_DCS_BIT_T43_MODE                                36

#define T30_DIS_BIT_PLANE_INTERLEAVE_CAPABLE                37
#define T30_DCS_BIT_PLANE_INTERLEAVE                        37

#define T30_DIS_BIT_G726_CAPABLE                            38
#define T30_DCS_BIT_G726                                    38

/* Bit 39 in a DIS, DTC, or DCS is "reserved for extended voice coding", so it should be set to zero */

/* Bit 40 is an extension bit */

/* This also enables R8 x 15.4/mm mode */
#define T30_DIS_BIT_200_400_CAPABLE                         41
#define T30_DCS_BIT_200_400                                 41

#define T30_DIS_BIT_300_300_CAPABLE                         42
#define T30_DCS_BIT_300_300                                 42

/* This also enables R16 x 15.4/mm mode */
#define T30_DIS_BIT_400_400_CAPABLE                         43
#define T30_DCS_BIT_400_400                                 43

/* Bits 44 and 45 are used only in conjunction with bits 15 and 43. Bit 44 in DCS, when used, shall correctly
   indicate the resolution of the transmitted document, which means that bit 44 in DCS may not always match the
   indication of bits 44 and 45 in DIS/DTC. Cross selection will cause the distortion and reduction of reproducible
   area.
   If a receiver indicates in DIS that it prefers to receive metric-based information, but the transmitter has
   only the equivalent inch-based information (or vice versa), then communication shall still take place.
   Bits 44 and 45 do not require the provision of any additional features on the terminal to indicate to the
   sending or receiving user whether the information was transmitted or received on a metric-metric, inch-inch,
   metric-inch, inch-metric basis. */

#define T30_DIS_BIT_INCH_RESOLUTION_PREFERRED               44
#define T30_DCS_BIT_INCH_RESOLUTION                         44

#define T30_DIS_BIT_METRIC_RESOLUTION_PREFERRED             45
/* Bit 45 in a DCS is "don't care", so it should be set to zero */

#define T30_DIS_BIT_MIN_SCAN_TIME_HALVES                    46
/* Bit 46 in a DCS is "don't care", so it should be set to zero */

#define T30_DIS_BIT_SELECTIVE_POLLING_CAPABLE               47
/* Bit 47 in a DCS should be set to zero */

/* Bit 48 is an extension bit */

#define T30_DIS_BIT_SUBADDRESSING_CAPABLE                   49
#define T30_DCS_BIT_SUBADDRESS_TRANSMISSION                 49

#define T30_DIS_BIT_PASSWORD                                50
#define T30_DCS_BIT_SENDER_ID_TRANSMISSION                  50

/* Bit 51 indicates that there is a data file ready to be polled from the answering terminal. It is
   not an indication of a capability. This bit is used in conjunction with bits 53, 54, 55 and 57. */
#define T30_DIS_BIT_READY_TO_TRANSMIT_DATA_FILE             51
/* Bit 51 in a DCS should be set to zero */

/* Bit 52 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

/* The binary file transfer protocol is described in ITU-T Rec. T.434.  */
#define T30_DIS_BIT_BFT_CAPABLE                             53
#define T30_DCS_BIT_BFT                                     53

#define T30_DIS_BIT_DTM_CAPABLE                             54
#define T30_DCS_BIT_DTM                                     54

#define T30_DIS_BIT_EDI_CAPABLE                             55
#define T30_DCS_BIT_EDI                                     55

/* Bit 56 is an extension bit */

#define T30_DIS_BIT_BTM_CAPABLE                             57
#define T30_DCS_BIT_BTM                                     57

/* Bit 58 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

/* Bit 59 indicates that there is a character-coded or mixed-mode document ready to be polled
   from the answering terminal. It is not an indication of a capability. This bit is used in
   conjunction with bits 60, 62 and 65. */
#define T30_DIS_BIT_READY_TO_TRANSMIT_MIXED_MODE_DOCUMENT   59
/* Bit 59 in a DCS should be set to zero */

#define T30_DIS_BIT_CHARACTER_MODE_CAPABLE                  60
#define T30_DCS_BIT_CHARACTER_MODE                          60

/* Bit 61 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

#define T30_DIS_BIT_MIXED_MODE                              62
#define T30_DCS_BIT_MIXED_MODE                              62

/* Bit 63 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

/* Bit 64 is an extension bit */

#define T30_DIS_BIT_PROCESSABLE_MODE_26                     65
/* Bit 65 in a DCS should be set to zero */

#define T30_DIS_BIT_DIGITAL_NETWORK_CAPABLE                 66
#define T30_DCS_BIT_DIGITAL_NETWORK_CAPABLE                 66

#define T30_DIS_BIT_DUPLEX_CAPABLE                          67
#define T30_DCS_BIT_DUPLEX_CAPABLE                          67

#define T30_DIS_BIT_T81_CAPABLE                             68
#define T30_DCS_BIT_T81_MODE                                68

#define T30_DIS_BIT_FULL_COLOUR_CAPABLE                     69
#define T30_DCS_BIT_FULL_COLOUR_MODE                        69

/* Bit 70 in a DCS should be set to zero */
#define T30_DCS_BIT_PREFERRED_HUFFMAN_TABLES                70

/* In a DIS/DTC frame, setting bit 71 to "0" indicates that the called terminal can only accept
   image data which has been digitized to 8 bits/pel/component for JPEG mode. This is also true for T.43
   mode if bit 36 is also set to "1". Setting bit 71 to "1" indicates that the called terminal can also accept
   image data that are digitized to 12 bits/pel/component for JPEG mode. This is also true for T.43 mode if
   bit 36 is also set to "1". In a DCS frame, setting bit 71 to "0" indicates that the calling terminal's image
   data are digitized to 8 bits/pel/component for JPEG mode. This is also true for T.43 mode if bit 36 is also
   set to "1". Setting bit 71 to "1" indicates that the calling terminal transmits image data which has been
   digitized to 12 bits/pel/component for JPEG mode. This is also true for T.43 mode if bit 36 is also set
   to "1". */
#define T30_DIS_BIT_12BIT_CAPABLE                           71
#define T30_DCS_BIT_12BIT_COMPONENT                         71

/* Bit 72 is an extension bit */

#define T30_DIS_BIT_NO_SUBSAMPLING                          73
#define T30_DCS_BIT_NO_SUBSAMPLING                          73

#define T30_DIS_BIT_CUSTOM_ILLUMINANT                       74
#define T30_DCS_BIT_CUSTOM_ILLUMINANT                       74

#define T30_DIS_BIT_CUSTOM_GAMUT_RANGE                      75
#define T30_DCS_BIT_CUSTOM_GAMUT_RANGE                      75

#define T30_DIS_BIT_NORTH_AMERICAN_LETTER_CAPABLE           76
#define T30_DCS_BIT_NORTH_AMERICAN_LETTER                   76

#define T30_DIS_BIT_NORTH_AMERICAN_LEGAL_CAPABLE            77
#define T30_DCS_BIT_NORTH_AMERICAN_LEGAL                    77

#define T30_DIS_BIT_T85_CAPABLE                             78
#define T30_DCS_BIT_T85_MODE                                78

/* (T.30 note 30) This capability should only be set if T30_DIS_BIT_T85_CAPABLE is also set */
#define T30_DIS_BIT_T85_L0_CAPABLE                          79
#define T30_DCS_BIT_T85_L0_MODE                             79

/* Bit 80 is an extension bit */

#define T30_DIS_BIT_HKM_KEY_MANAGEMENT_CAPABLE              81
#define T30_DCS_BIT_HKM_KEY_MANAGEMENT_MODE                 81

#define T30_DIS_BIT_RSA_KEY_MANAGEMENT_CAPABLE              82
#define T30_DCS_BIT_RSA_KEY_MANAGEMENT_MODE                 82

#define T30_DIS_BIT_OVERRIDE_CAPABLE                        83
#define T30_DCS_BIT_OVERRIDE_MODE                           83

#define T30_DIS_BIT_HFX40_CIPHER_CAPABLE                    84
#define T30_DCS_BIT_HFX40_CIPHER_MODE                       84

#define T30_DIS_BIT_ALTERNATIVE_CIPHER_2_CAPABLE            85
#define T30_DCS_BIT_ALTERNATIVE_CIPHER_2_MODE               85

#define T30_DIS_BIT_ALTERNATIVE_CIPHER_3_CAPABLE            86
#define T30_DCS_BIT_ALTERNATIVE_CIPHER_3_MODE               86

#define T30_DIS_BIT_HFX40_I_HASHING_CAPABLE                 87
#define T30_DCS_BIT_HFX40_I_HASHING_MODE                    87

/* Bit 88 is an extension bit */

#define T30_DIS_BIT_ALTERNATIVE_HASHING_2_CAPABLE           89
#define T30_DCS_BIT_ALTERNATIVE_HASHING_2_MODE              89

#define T30_DIS_BIT_ALTERNATIVE_HASHING_3_CAPABLE           90
#define T30_DCS_BIT_ALTERNATIVE_HASHING_3_MODE              90

/* Bit 91 in a DIS, DTC, or DCS is "reserved for suture security features", so it should be set to zero */

/* Bits 92 to 94 specify the mixed raster content mode. */

#define T30_DIS_BIT_T44_PAGE_LENGTH                         95
#define T30_DCS_BIT_T44_PAGE_LENGTH                         95

/* Bit 96 is an extension bit */

/* In a DIS/DTC frame, setting bit 97 to "0" indicates that the called terminal does not have the
   capability to accept 300 pels/25.4 mm x 300 lines/25.4 mm or 400 pels/25.4 mm x 400 lines/25.4 mm
   resolutions for colour/gray-scale images or T.44 Mixed Raster Content (MRC) mask layer.

   Setting bit 97 to "1" indicates that the called terminal does have the capability to accept
   300 pels/25.4 mm x 300 lines/25.4 mm or 400 pels/25.4 mm x 400 lines/25.4 mm resolutions for
   colour/gray-scale images and MRC mask layer. Bit 97 is valid only when bits 68 and 42 or 43
   (300 pels/25.4 mm x 300 lines/25.4 mm or 400 pels/25.4 mm x 400 lines/25.4 mm) are set to "1".

   In a DCS frame, setting bit 97 to "0" indicates that the calling terminal does not use
   300 pels/25.4 mm x 300 lines/25.4 mm or 400 pels/25.4 mm x 400 lines/25.4 mm resolutions
   for colour/gray-scale images and mask layer.

   Setting bit 97 to "1" indicates that the calling terminal uses 300 pels/25.4 mm x 300 lines/25.4 mm
   or 400 pels/25.4 mm x 400 lines/25.4 mm resolutions for colour/gray-scale images and MRC mask layer.
   Bit 97 is valid only when bits 68 and 42 or 43 (300 pels/25.4 mm x 300 lines/25.4 mm and
   400 pels/25.4 mm x 400 lines/25.4 mm) are set to "1".

   In a DIS/DTC frame, combinations of bit 42, bit 43 and bit 97 indicate that the called terminal
   has higher resolution capabilities as follows:

            Resolution capabilities (pels/25.4 mm)
    DIS/DTC     Monochrome              Colour/gray-scale
    42 43 97    300 x 300   400 x 400   300 x 300   400 x 400
     0  0  0    no          no          no          no
     1  0  0    yes         no          no          no
     0  1  0    no          yes         no          no
     1  1  0    yes         yes         no          no
     0  0  1    (invalid)
     1  0  1    yes         no          yes         no
     0  1  1    no          yes         no          yes
     1  1  1    yes         yes         yes         yes
        "yes" means that the called terminal has the corresponding capability.
        "no" means that the called terminal does not have the corresponding capability. */
#define T30_DIS_BIT_COLOUR_GRAY_300_300_400_400_CAPABLE     97
#define T30_DCS_BIT_COLOUR_GRAY_300_300_400_400             97

/* In a DIS/DTC frame, setting bit 98 to "0" indicates that the called terminal does not have the
   capability to accept 100 pels/25.4 mm x 100 lines/25.4 mm spatial resolution for colour or gray-scale
   images. Setting bit 98 to "1" indicates that the called terminal does have the capability to accept
   100 pels/25.4 mm  x 100 lines/25.4 mm spatial resolution for colour or gray-scale images. Bit 98 is valid
   only when bit 68 is set to "1". In a DCS frame, setting bit 98 to "0" indicates that the calling terminal does
   not use 100 pels/25.4 mm  x 100 lines/25.4 mm spatial resolution for colour or gray-scale images. Setting
   bit 98 to "1" indicates that the calling terminal uses 100 pels/25.4 mm x 100 lines/25.4 mm spatial
   resolution for colour or gray-scale images. */
#define T30_DIS_BIT_COLOUR_GRAY_100_100_CAPABLE             98
#define T30_DCS_BIT_COLOUR_GRAY_100_100                     98

#define T30_DIS_BIT_SIMPLE_PHASE_C_BFT_NEGOTIATIONS_CAPABLE 99
#define T30_DCS_BIT_SIMPLE_PHASE_C_BFT_NEGOTIATIONS_CAPABLE 99

#define T30_DIS_BIT_EXTENDED_BFT_NEGOTIATIONS_CAPABLE       100
/* Bit 100 in a DCS should be set to zero */

/* To provide an error recovery mechanism, when PWD/SEP/SUB/SID/PSA/IRA/ISP frames are sent with DCS or DTC,
    bits 49, 102 and 50 in DCS or bits 47, 101, 50 and 35 in DTC shall be set to "1" with the following
    meaning:

    Bit DIS                                             DTC                                                 DCS
    35  Polled sub-address capability                   Polled sub-address transmission                     Not allowed - set to "0"
    47  Selective polling capability                    Selective polling transmission                      Not allowed - set to "0"
    49  Sub-addressing capability                       Not allowed (Set to "0")                            Sub-addressing transmission
    50  Password                                        Password transmission                               Sender identification transmission
    101 Internet selective polling address capability   Internet selective polling address transmission     Not allowed - set to "0"
    102 Internet routing address capability             Not allowed (Set to "0")                            Internet routing address transmission

   Terminals conforming to the 1993 version of T.30 may set the above bits to "0" even though PWD/SEP/SUB
   frames are transmitted. */
#define T30_DIS_BIT_INTERNET_SELECTIVE_POLLING_ADDRESS      101
/* Bit 101 in a DCS should be set to zero */

#define T30_DIS_BIT_INTERNET_ROUTING_ADDRESS                102
#define T30_DCS_BIT_INTERNET_ROUTING_ADDRESS_TRANSMISSION   102

/* Bit 103 in a DIS, DTC, or DCS is "reserved", so it should be set to zero */

/* Bit 104 is an extension bit */

#define T30_DIS_BIT_600_600_CAPABLE                         105
#define T30_DCS_BIT_600_600                                 105

#define T30_DIS_BIT_1200_1200_CAPABLE                       106
#define T30_DCS_BIT_1200_1200                               106

#define T30_DIS_BIT_300_600_CAPABLE                         107
#define T30_DCS_BIT_300_600                                 107

#define T30_DIS_BIT_400_800_CAPABLE                         108
#define T30_DCS_BIT_400_800                                 108

#define T30_DIS_BIT_600_1200_CAPABLE                        109
#define T30_DCS_BIT_600_1200                                109

/* This requires that bit 105 is also set */
#define T30_DIS_BIT_COLOUR_GRAY_600_600_CAPABLE             110
#define T30_DCS_BIT_COLOUR_GRAY_600_600                     110

/* This requires that bit 106 is also set */
#define T30_DIS_BIT_COLOUR_GRAY_1200_1200_CAPABLE           111
#define T30_DCS_BIT_COLOUR_GRAY_1200_1200                   111

/* Bit 112 is an extension bit */

#define T30_DIS_BIT_ALTERNATE_DOUBLE_SIDED_CAPABLE          113
#define T30_DCS_BIT_ALTERNATE_DOUBLE_SIDED_CAPABLE          113

#define T30_DIS_BIT_CONTINUOUS_DOUBLE_SIDED_CAPABLE         114
#define T30_DCS_BIT_CONTINUOUS_DOUBLE_SIDED_CAPABLE         114

#define T30_DIS_BIT_BLACK_AND_WHITE_MRC                     115
/* Bit 115 in a DCS should be set to zero */

#define T30_DIS_BIT_T45_CAPABLE                             116
#define T30_DCS_BIT_T45_MODE                                116

/* Bits 117 to 118 specify the shared memory capability */

/* This bit defines the available colour space, when bit 92, 93 or 94 is set to "1".
   Available colour space for all combinations of bits 92, 93, 94 and 119 are shown in the following table.
   It should be noted that terminals which conform to the 2003 and earlier versions of this Recommendation
   will send LAB with "1" in bit 92, 93 or 94 even if bit 119 is set to "1".

        Available colour space for DIS/DTC bits 92, 93, 94 and 119

    92 93 94 119    Mode of T.44        Available colour space
     0  0  0  x     Not available       -
     1  0  0  0     Mode 1              LAB only
     1  0  0  1     Mode 1              YCC only
     x  1  x  0     Mode 2 or higher    LAB only
     x  x  1  0     Mode 2 or higher    LAB only
     x  1  x  1     Mode 2 or higher    LAB and YCC
     x  x  1  1     Mode 2 or higher    LAB and YCC

        Colour space for DCS bits 92, 93, 94 and 119

    92 93 94 119    Mode of T.44        Colour space
     0  0  0  x*    Not available       -
     1  0  0  0     Mode 1              LAB
     1  0  0  1     Mode 1              YCC
     x  1  x  0     Mode 2 or higher    LAB
     x  x  1  0     Mode 2 or higher    LAB
     x  1  x  1     Mode 2 or higher    YCC or mixing of YCC and LAB
     x  x  1  1     Mode 2 or higher    YCC or mixing of YCC and LAB */
#define T30_DIS_BIT_T44_COLOUR_SPACE                        119
#define T30_DCS_BIT_T44_COLOUR_SPACE                        119

/* Bit 120 is an extension bit */

/* Can only be set in the communication through the T.38 gateway, to cope with delay of network.
   T.x timer (12+-1s) should be used after emitting RNR or TNR. However, after receiving
   PPS signal in ECM mode, T.5 timer should be used. */
#define T30_DIS_BIT_T38_FLOW_CONTROL_CAPABLE                121
#define T30_DCS_BIT_T38_FLOW_CONTROL_CAPABLE                121

/* For resolutions greater than 200 lines/25.4 mm, 4.2.1.1/T.4 specifies the use of specific K
   factors for each standardized vertical resolution. To ensure backward compatibility with earlier
   versions of ITU-T Rec. T.4, bit 122 indicates when such K factors are being used. */
#define T30_DIS_BIT_K_GREATER_THAN_4                        122

/* This bit should be set to "1" if the fax device is an Internet-Aware Fax Device as defined in
   ITU-T Rec. T.38 and if it is not affected by the data signal rate indicated by the DIS and DTC
   signals when communicating with another Internet-Aware Fax Device operating in T.38 mode. This
   bit shall not be used in GSTN mode. */
#define T30_DIS_BIT_T38_FAX_CAPABLE                         123
/* This bit should be set to "1" if the fax device elects to operate in an Internet-Aware Fax mode
   as defined in ITU-T Rec. T.38 in response to a device which has set the related DIS bit to "1".
   When this bit is set to "1", the data signal rate of the modem (bits 11-14) should be set to "0". */
#define T30_DCS_BIT_T38_FAX_MODE                            123

/* Bits 124 to 126 specify the T.89 applications profile. */
#define T30_DIS_BIT_T88_CAPABILITY_1                        124
#define T30_DCS_BIT_T88_MODE_1                              124
#define T30_DIS_BIT_T88_CAPABILITY_2                        125
#define T30_DCS_BIT_T88_MODE_2                              125
#define T30_DIS_BIT_T88_CAPABILITY_3                        126
#define T30_DCS_BIT_T88_MODE_3                              126

/* When either bit of 31, 36, 38, 51, 53, 54, 55, 57, 59, 60, 62, 65, 68, 78, 79, 115, 116 and 127 is
   set to "1", ECM must be used. If the value of bit field 92 to 94 is non-zero, then ECM must be used.

   Annex K describes the optional continuous-tone colour and gray scale images mode
   (sYCC-JPEG mode) protocol. When bit 127 in DIS/DTC frame is set to "1", the called terminal has the
   capability to accept sYCC-JPEG mode. This is defined with complete independent in the colour space
   CIELAB. In addition, when bit 127 in DCS frame is set to "1", ECM must be used and bits 15, 17, 18,
   19, 20, 41, 42, 43, 45, 46, 68, 69, 71, 73, 74, 75, 76, 77, 97, 98, 105, 106, 107, 108,
   109, 110 and 111 in DCS frame are "Don't care", and should be set to "0". In the case of
   transmission of multiple images, a post message signal PPS-MPS between pages, PPS-NULL between
   partial pages and PPS-EOP following the last page should be sent from the calling terminal to the
   called terminal. */
#define T30_DIS_BIT_SYCC_T81_CAPABLE                        127
#define T30_DCS_BIT_SYCC_T81_MODE                           127

#endif
/*- End of file ------------------------------------------------------------*/
