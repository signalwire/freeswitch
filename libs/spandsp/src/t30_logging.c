/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t30_logging.c - ITU T.30 FAX transfer processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/queue.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/fsk.h"
#include "spandsp/v29rx.h"
#include "spandsp/v29tx.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/timezone.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/image_translate.h"
#include "spandsp/t81_t82_arith_coding.h"
#include "spandsp/t85.h"
#include "spandsp/t42.h"
#if defined(SPANDSP_SUPPORT_T43)
#include "spandsp/t43.h"
#endif
#include "spandsp/t4_t6_decode.h"
#include "spandsp/t4_t6_encode.h"
#include "spandsp/t30_fcf.h"
#include "spandsp/t35.h"
#include "spandsp/t30.h"
#include "spandsp/t30_logging.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/timezone.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#include "spandsp/private/t42.h"
#if defined(SPANDSP_SUPPORT_T43)
#include "spandsp/private/t43.h"
#endif
#include "spandsp/private/t4_t6_decode.h"
#include "spandsp/private/t4_t6_encode.h"
#include "spandsp/private/image_translate.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"
#include "spandsp/private/t30.h"

#include "t30_local.h"

/*! Value string pair structure */
typedef struct
{
    int val;
    const char *str;
} value_string_t;

enum
{
    DISBIT1 = 0x01,
    DISBIT2 = 0x02,
    DISBIT3 = 0x04,
    DISBIT4 = 0x08,
    DISBIT5 = 0x10,
    DISBIT6 = 0x20,
    DISBIT7 = 0x40,
    DISBIT8 = 0x80
};

SPAN_DECLARE(const char *) t30_completion_code_to_str(int result)
{
    switch (result)
    {
    case T30_ERR_OK:
        return "OK";
    case T30_ERR_CEDTONE:
        return "The CED tone exceeded 5s";
    case T30_ERR_T0_EXPIRED:
        return "Timed out waiting for initial communication";
    case T30_ERR_T1_EXPIRED:
        return "Timed out waiting for the first message";
    case T30_ERR_T3_EXPIRED:
        return "Timed out waiting for procedural interrupt";
    case T30_ERR_HDLC_CARRIER:
        return "The HDLC carrier did not stop in a timely manner";
    case T30_ERR_CANNOT_TRAIN:
        return "Failed to train with any of the compatible modems";
    case T30_ERR_OPER_INT_FAIL:
        return "Operator intervention failed";
    case T30_ERR_INCOMPATIBLE:
        return "Far end is not compatible";
    case T30_ERR_RX_INCAPABLE:
        return "Far end is not able to receive";
    case T30_ERR_TX_INCAPABLE:
        return "Far end is not able to transmit";
    case T30_ERR_NORESSUPPORT:
        return "Far end cannot receive at the resolution of the image";
    case T30_ERR_NOSIZESUPPORT:
        return "Far end cannot receive at the size of image";
    case T30_ERR_UNEXPECTED:
        return "Unexpected message received";
    case T30_ERR_TX_BADDCS:
        return "Received bad response to DCS or training";
    case T30_ERR_TX_BADPG:
        return "Received a DCN from remote after sending a page";
    case T30_ERR_TX_ECMPHD:
        return "Invalid ECM response received from receiver";
    case T30_ERR_TX_GOTDCN:
        return "Received a DCN while waiting for a DIS";
    case T30_ERR_TX_INVALRSP:
        return "Invalid response after sending a page";
    case T30_ERR_TX_NODIS:
        return "Received other than DIS while waiting for DIS";
    case T30_ERR_TX_PHBDEAD:
        return "Received no response to DCS or TCF";
    case T30_ERR_TX_PHDDEAD:
        return "No response after sending a page";
    case T30_ERR_TX_T5EXP:
        return "Timed out waiting for receiver ready (ECM mode)";
    case T30_ERR_RX_ECMPHD:
        return "Invalid ECM response received from transmitter";
    case T30_ERR_RX_GOTDCS:
        return "DCS received while waiting for DTC";
    case T30_ERR_RX_INVALCMD:
        return "Unexpected command after page received";
    case T30_ERR_RX_NOCARRIER:
        return "Carrier lost during fax receive";
    case T30_ERR_RX_NOEOL:
        return "Timed out while waiting for EOL (end Of line)";
    case T30_ERR_RX_NOFAX:
        return "Timed out while waiting for first line";
    case T30_ERR_RX_T2EXPDCN:
        return "Timer T2 expired while waiting for DCN";
    case T30_ERR_RX_T2EXPD:
        return "Timer T2 expired while waiting for phase D";
    case T30_ERR_RX_T2EXPFAX:
        return "Timer T2 expired while waiting for fax page";
    case T30_ERR_RX_T2EXPMPS:
        return "Timer T2 expired while waiting for next fax page";
    case T30_ERR_RX_T2EXPRR:
        return "Timer T2 expired while waiting for RR command";
    case T30_ERR_RX_T2EXP:
        return "Timer T2 expired while waiting for NSS, DCS or MCF";
    case T30_ERR_RX_DCNWHY:
        return "Unexpected DCN while waiting for DCS or DIS";
    case T30_ERR_RX_DCNDATA:
        return "Unexpected DCN while waiting for image data";
    case T30_ERR_RX_DCNFAX:
        return "Unexpected DCN while waiting for EOM, EOP or MPS";
    case T30_ERR_RX_DCNPHD:
        return "Unexpected DCN after EOM or MPS sequence";
    case T30_ERR_RX_DCNRRD:
        return "Unexpected DCN after RR/RNR sequence";
    case T30_ERR_RX_DCNNORTN:
        return "Unexpected DCN after requested retransmission";
    case T30_ERR_FILEERROR:
        return "TIFF/F file cannot be opened";
    case T30_ERR_NOPAGE:
        return "TIFF/F page not found";
    case T30_ERR_BADTIFF:
        return "TIFF/F format is not compatible";
    case T30_ERR_BADPAGE:
        return "TIFF/F page number tag missing";
    case T30_ERR_BADTAG:
        return "Incorrect values for TIFF/F tags";
    case T30_ERR_BADTIFFHDR:
        return "Bad TIFF/F header - incorrect values in fields";
    case T30_ERR_NOMEM:
        return "Cannot allocate memory for more pages";
    case T30_ERR_RETRYDCN:
        return "Disconnected after permitted retries";
    case T30_ERR_CALLDROPPED:
        return "The call dropped prematurely";
    case T30_ERR_NOPOLL:
        return "Poll not accepted";
    case T30_ERR_IDENT_UNACCEPTABLE:
        return "Ident not accepted";
    case T30_ERR_PSA_UNACCEPTABLE:
        return "Polled sub-address not accepted";
    case T30_ERR_SEP_UNACCEPTABLE:
        return "Selective polling address not accepted";
    case T30_ERR_SID_UNACCEPTABLE:
        return "Sender identification not accepted";
    case T30_ERR_PWD_UNACCEPTABLE:
        return "Password not accepted";
    case T30_ERR_SUB_UNACCEPTABLE:
        return "Sub-address not accepted";
    case T30_ERR_TSA_UNACCEPTABLE:
        return "Transmitting subscriber internet address not accepted";
    case T30_ERR_IRA_UNACCEPTABLE:
        return "Internet routing address not accepted";
    case T30_ERR_CIA_UNACCEPTABLE:
        return "Calling subscriber internet address not accepted";
    case T30_ERR_ISP_UNACCEPTABLE:
        return "Internet selective polling address not accepted";
    case T30_ERR_CSA_UNACCEPTABLE:
        return "Called subscriber internet address not accepted";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_frametype(uint8_t x)
{
    switch (x)
    {
    case T30_DIS:
        return "DIS";
    case T30_CSI:
        return "CSI";
    case T30_NSF:
        return "NSF";
    case T30_DTC:
        return "DTC";
    case T30_CIG:
        return "CIG";
    case T30_NSC:
        return "NSC";
    case T30_PWD:
        return "PWD";
    case T30_SEP:
        return "SEP";
    case T30_PSA:
        return "PSA";
    case T30_CIA:
        return "CIA";
    case T30_ISP:
        return "ISP";
    case T30_DCS:
    case T30_DCS | 0x01:
        return "DCS";
    case T30_TSI:
    case T30_TSI | 0x01:
        return "TSI";
    case T30_NSS:
    case T30_NSS | 0x01:
        return "NSS";
    case T30_SUB:
    case T30_SUB | 0x01:
        return "SUB";
    case T30_SID:
    case T30_SID | 0x01:
        return "SID";
    case T30_CTC:
    case T30_CTC | 0x01:
        return "CTC";
    case T30_TSA:
    case T30_TSA | 0x01:
        return "TSA";
    case T30_IRA:
    case T30_IRA | 0x01:
        return "IRA";
    case T30_CFR:
    case T30_CFR | 0x01:
        return "CFR";
    case T30_FTT:
    case T30_FTT | 0x01:
        return "FTT";
    case T30_CTR:
    case T30_CTR | 0x01:
        return "CTR";
    case T30_CSA:
    case T30_CSA | 0x01:
        return "CSA";
    case T30_EOM:
    case T30_EOM | 0x01:
        return "EOM";
    case T30_MPS:
    case T30_MPS | 0x01:
        return "MPS";
    case T30_EOP:
    case T30_EOP | 0x01:
        return "EOP";
    case T30_PRI_EOM:
    case T30_PRI_EOM | 0x01:
        return "PRI-EOM";
    case T30_PRI_MPS:
    case T30_PRI_MPS | 0x01:
        return "PRI-MPS";
    case T30_PRI_EOP:
    case T30_PRI_EOP | 0x01:
        return "PRI-EOP";
    case T30_EOS:
    case T30_EOS | 0x01:
        return "EOS";
    case T30_PPS:
    case T30_PPS | 0x01:
        return "PPS";
    case T30_EOR:
    case T30_EOR | 0x01:
        return "EOR";
    case T30_RR:
    case T30_RR | 0x01:
        return "RR";
    case T30_MCF:
    case T30_MCF | 0x01:
        return "MCF";
    case T30_RTP:
    case T30_RTP | 0x01:
        return "RTP";
    case T30_RTN:
    case T30_RTN | 0x01:
        return "RTN";
    case T30_PIP:
    case T30_PIP | 0x01:
        return "PIP";
    case T30_PIN:
    case T30_PIN | 0x01:
        return "PIN";
    case T30_PPR:
    case T30_PPR | 0x01:
        return "PPR";
    case T30_RNR:
    case T30_RNR | 0x01:
        return "RNR";
    case T30_ERR:
    case T30_ERR | 0x01:
        return "ERR";
    case T30_FDM:
    case T30_FDM | 0x01:
        return "FDM";
    case T30_DCN:
    case T30_DCN | 0x01:
        return "DCN";
    case T30_CRP:
    case T30_CRP | 0x01:
        return "CRP";
    case T30_FNV:
    case T30_FNV | 0x01:
        return "FNV";
    case T30_TNR:
    case T30_TNR | 0x01:
        return "TNR";
    case T30_TR:
    case T30_TR | 0x01:
        return "TR";
    case T30_TK:
        return "TK";
    case T30_RK:
        return "RK";
#if 0
    case T30_PSS:
        return "PSS";
#endif
    case T30_DES:
        return "DES";
    case T30_DEC:
        return "DEC";
    case T30_DER:
        return "DER";
#if 0
    case T30_DTR:
        return "DTR";
#endif
    case T30_DNK:
    case T30_DNK | 0x01:
        return "DNK";
    case T30_PID:
    case T30_PID | 0x01:
        return "PID";
    case T30_NULL:
        return "NULL";
    case T4_FCD:
        return "FCD";
    case T4_CCD:
        return "CCD";
    case T4_RCP:
        return "RCP";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

static void octet_reserved_bit(logging_state_t *log,
                               const uint8_t *msg,
                               int bit_no,
                               int expected)
{
    char s[10] = ".... ....";
    int bit;
    uint8_t octet;
    
    /* Break out the octet and the bit number within it. */
    octet = msg[((bit_no - 1) >> 3) + 3];
    bit_no = (bit_no - 1) & 7;
    /* Now get the actual bit. */
    bit = (octet >> bit_no) & 1;
    /* Is it what it should be. */
    if (bit ^ expected)
    {
        /* Only log unexpected values. */
        s[7 - bit_no + ((bit_no < 4)  ?  1  :  0)] = (uint8_t) (bit + '0');
        span_log(log, SPAN_LOG_FLOW, "  %s= Unexpected state for reserved bit: %d\n", s, bit);
    }
}
/*- End of function --------------------------------------------------------*/

static void octet_bit_field(logging_state_t *log,
                            const uint8_t *msg,
                            int bit_no,
                            const char *desc,
                            const char *yeah,
                            const char *neigh)
{
    char s[10] = ".... ....";
    int bit;
    uint8_t octet;
    const char *tag;

    /* Break out the octet and the bit number within it. */
    octet = msg[((bit_no - 1) >> 3) + 3];
    bit_no = (bit_no - 1) & 7;
    /* Now get the actual bit. */
    bit = (octet >> bit_no) & 1;
    /* Edit the bit string for display. */
    s[7 - bit_no + ((bit_no < 4)  ?  1  :  0)] = (uint8_t) (bit + '0');
    /* Find the right tag to display. */
    if (bit)
    {
        if ((tag = yeah) == NULL)
            tag = "Set";
    }
    else
    {
        if ((tag = neigh) == NULL)
            tag = "Not set";
    }
    /* Eh, voila! */
    span_log(log, SPAN_LOG_FLOW, "  %s= %s: %s\n", s, desc, tag);
}
/*- End of function --------------------------------------------------------*/

static void octet_field(logging_state_t *log,
                        const uint8_t *msg,
                        int start,
                        int end,
                        const char *desc,
                        const value_string_t tags[])
{
    char s[10] = ".... ....";
    int i;
    uint8_t octet;
    const char *tag;
    
    /* Break out the octet and the bit number range within it. */
    octet = msg[((start - 1) >> 3) + 3];
    start = (start - 1) & 7;
    end = ((end - 1) & 7) + 1;

    /* Edit the bit string for display. */
    for (i = start;  i < end;  i++)
        s[7 - i + ((i < 4)  ?  1  :  0)] = (uint8_t) ((octet >> i) & 1) + '0';

    /* Find the right tag to display. */
    octet = (uint8_t) ((octet >> start) & ((0xFF + (1 << (end - start))) & 0xFF));
    tag = "Invalid";
    for (i = 0;  tags[i].str;  i++)
    {
        if (octet == tags[i].val)
        {
            tag = tags[i].str;
            break;
        }
    }
    /* Eh, voila! */
    span_log(log, SPAN_LOG_FLOW, "  %s= %s: %s\n", s, desc, tag);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_decode_dis_dtc_dcs(t30_state_t *s, const uint8_t *pkt, int len)
{
    logging_state_t *log;
    uint8_t frame_type;
    static const value_string_t available_signalling_rate_tags[] =
    {
        { 0x00, "V.27 ter fall-back mode" },
        { 0x01, "V.29" },
        { 0x02, "V.27 ter" },
        { 0x03, "V.27 ter and V.29" },
        { 0x0B, "V.27 ter, V.29, and V.17" },
        { 0x06, "Reserved" },
        { 0x0A, "Reserved" },
        { 0x0E, "Reserved" },
        { 0x0F, "Reserved" },
        { 0x04, "Not used" },
        { 0x05, "Not used" },
        { 0x08, "Not used" },
        { 0x09, "Not used" },
        { 0x0C, "Not used" },
        { 0x0D, "Not used" },
        { 0x00, NULL }
    };
    static const value_string_t selected_signalling_rate_tags[] =
    {
        { 0x00, "V.27ter 2400bps" },
        { 0x01, "V.29, 9600bps" },
        { 0x02, "V.27ter 4800bps" },
        { 0x03, "V.29 7200bps" },
        { 0x08, "V.17 14400bps" },
        { 0x09, "V.17 9600bps" },
        { 0x0A, "V.17 12000bps" },
        { 0x0B, "V.17 7200bps" },
        { 0x05, "Reserved" },
        { 0x07, "Reserved" },
        { 0x0C, "Reserved" },
        { 0x0D, "Reserved" },
        { 0x0E, "Reserved" },
        { 0x0F, "Reserved" },
        { 0x00, NULL }
    };
    static const value_string_t available_scan_line_length_tags[] =
    {
        { 0x00, "215mm +- 1%" },
        { 0x01, "215mm +- 1% and 255mm +- 1%" },
        { 0x02, "215mm +- 1%, 255mm +- 1% and 303mm +- 1%" },
        { 0x00, NULL }
    };
    static const value_string_t selected_scan_line_length_tags[] =
    {
        { 0x00, "215mm +- 1%" },
        { 0x01, "255mm +- 1%" },
        { 0x02, "303mm +- 1%" },
        { 0x00, NULL }
    };
    static const value_string_t available_recording_length_tags[] =
    {
        { 0x00, "A4 (297mm)" },
        { 0x01, "A4 (297mm) and B4 (364mm)" },
        { 0x02, "Unlimited" },
        { 0x00, NULL }
    };
    static const value_string_t selected_recording_length_tags[] =
    {
        { 0x00, "A4 (297mm)" },
        { 0x01, "B4 (364mm)" },
        { 0x02, "Unlimited" },
        { 0x00, NULL }
    };
    static const value_string_t available_minimum_scan_line_time_tags[] =
    {
        { 0x00, "20ms at 3.85 l/mm; T7.7 = T3.85" },
        { 0x01, "5ms at 3.85 l/mm; T7.7 = T3.85" },
        { 0x02, "10ms at 3.85 l/mm; T7.7 = T3.85" },
        { 0x03, "20ms at 3.85 l/mm; T7.7 = 1/2 T3.85" },
        { 0x04, "40ms at 3.85 l/mm; T7.7 = T3.85" },
        { 0x05, "40ms at 3.85 l/mm; T7.7 = 1/2 T3.85" },
        { 0x06, "10ms at 3.85 l/mm; T7.7 = 1/2 T3.85" },
        { 0x07, "0ms at 3.85 l/mm; T7.7 = T3.85" },
        { 0x00, NULL }
    };
    static const value_string_t selected_minimum_scan_line_time_tags[] =
    {
        { 0x00, "20ms" },
        { 0x01, "5ms" },
        { 0x02, "10ms" },
        { 0x04, "40ms" },
        { 0x07, "0ms" },
        { 0x00, NULL }
    };
    static const value_string_t shared_data_memory_capacity_tags[] =
    {
        { 0x00, "Not available" },
        { 0x01, "Level 2 = 2.0 Mbytes" },
        { 0x02, "Level 1 = 1.0 Mbytes" },
        { 0x03, "Level 3 = unlimited (i.e. >= 32 Mbytes)" },
        { 0x00, NULL }
    };
    static const value_string_t t89_profile_tags[] =
    {
        { 0x00, "Not used" },
        { 0x01, "Profiles 2 and 3" },
        { 0x02, "Profile 2" },
        { 0x04, "Profile 1" },
        { 0x06, "Profile 3" },
        { 0x03, "Reserved" },
        { 0x05, "Reserved" },
        { 0x07, "Reserved" },
        { 0x00, NULL }
    };
    static const value_string_t t44_mixed_raster_content_tags[] =
    {
        { 0x00, "0" },
        { 0x01, "1" },
        { 0x02, "2" },
        { 0x32, "3" },
        { 0x04, "4" },
        { 0x05, "5" },
        { 0x06, "6" },
        { 0x07, "7" },
        { 0x00, NULL }
    };

    if (!span_log_test(&s->logging, SPAN_LOG_FLOW))
        return;
    frame_type = pkt[2] & 0xFE;
    log = &s->logging;
    if (len <= 2)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }
    
    span_log(log, SPAN_LOG_FLOW, "%s:\n", t30_frametype(pkt[2]));
    if (len <= 3)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }
    octet_bit_field(log, pkt, 1, "Store and forward Internet fax (T.37)", NULL, NULL);
    octet_reserved_bit(log, pkt, 2, 0);
    octet_bit_field(log, pkt, 3, "Real-time Internet fax (T.38)", NULL, NULL);
    octet_bit_field(log, pkt, 4, "3G mobile network", NULL, NULL);
    octet_reserved_bit(log, pkt, 5, 0);
    if (frame_type == T30_DCS)
    {
        octet_reserved_bit(log, pkt, 6, 0);
        octet_reserved_bit(log, pkt, 7, 0);
    }
    else
    {
        octet_bit_field(log, pkt, 6, "V.8 capabilities", NULL, NULL);
        octet_bit_field(log, pkt, 7, "Preferred octets", "64 octets", "256 octets");
    }
    octet_reserved_bit(log, pkt, 8, 0);
    if (len <= 4)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }
    
    if (frame_type == T30_DCS)
    {
        octet_reserved_bit(log, pkt, 9, 0);
        octet_bit_field(log, pkt, 10, "Receive fax", NULL, NULL);
        octet_field(log, pkt, 11, 14, "Selected data signalling rate", selected_signalling_rate_tags);
    }
    else
    {
        octet_bit_field(log, pkt, 9, "Ready to transmit a fax document (polling)", NULL, NULL);
        octet_bit_field(log, pkt, 10, "Can receive fax", NULL, NULL);
        octet_field(log, pkt, 11, 14, "Supported data signalling rates", available_signalling_rate_tags);
    }
    octet_bit_field(log, pkt, 15, "R8x7.7lines/mm and/or 200x200pels/25.4mm", NULL, NULL);
    octet_bit_field(log, pkt, 16, "2-D coding", NULL, NULL);
    if (len <= 5)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    if (frame_type == T30_DCS)
    {
        octet_field(log, pkt, 17, 18, "Recording width", selected_scan_line_length_tags);
        octet_field(log, pkt, 19, 20, "Recording length", selected_recording_length_tags);
        octet_field(log, pkt, 21, 23, "Minimum scan line time", selected_minimum_scan_line_time_tags);
    }
    else
    {
        octet_field(log, pkt, 17, 18, "Recording width", available_scan_line_length_tags);
        octet_field(log, pkt, 19, 20, "Recording length", available_recording_length_tags);
        octet_field(log, pkt, 21, 23, "Receiver's minimum scan line time", available_minimum_scan_line_time_tags);
    }
    octet_bit_field(log, pkt, 24, "Extension indicator", NULL, NULL);
    if (!(pkt[5] & DISBIT8))
        return;
    if (len <= 6)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_reserved_bit(log, pkt, 25, 0);
    octet_bit_field(log, pkt, 26, "Compressed/uncompressed mode", "Uncompressed", "Compressed");
    octet_bit_field(log, pkt, 27, "Error correction mode (ECM)", "ECM", "Non-ECM");
    if (frame_type == T30_DCS)
        octet_bit_field(log, pkt, 28, "Frame size", "64 octets", "256 octets");
    else
        octet_reserved_bit(log, pkt, 28, 0);
    octet_reserved_bit(log, pkt, 29, 0);
    octet_reserved_bit(log, pkt, 30, 0);
    octet_bit_field(log, pkt, 31, "T.6 coding", NULL, NULL);
    octet_bit_field(log, pkt, 32, "Extension indicator", NULL, NULL);
    if (!(pkt[6] & DISBIT8))
        return;
    if (len <= 7)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 33, "\"Field not valid\" supported", NULL, NULL);
    if (frame_type == T30_DCS)
    {
        octet_reserved_bit(log, pkt, 34, 0);
        octet_reserved_bit(log, pkt, 35, 0);
    }
    else
    {
        octet_bit_field(log, pkt, 34, "Multiple selective polling", NULL, NULL);
        octet_bit_field(log, pkt, 35, "Polled sub-address", NULL, NULL);
    }
    octet_bit_field(log, pkt, 36, "T.43 coding", NULL, NULL);
    octet_bit_field(log, pkt, 37, "Plane interleave", NULL, NULL);
    octet_bit_field(log, pkt, 38, "Voice coding with 32kbit/s ADPCM (Rec. G.726)", NULL, NULL);
    octet_bit_field(log, pkt, 39, "Reserved for the use of extended voice coding set", NULL, NULL);
    octet_bit_field(log, pkt, 40, "Extension indicator", NULL, NULL);
    if (!(pkt[7] & DISBIT8))
        return;
    if (len <= 8)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }
    octet_bit_field(log, pkt, 41, "R8x15.4lines/mm", NULL, NULL);
    octet_bit_field(log, pkt, 42, "300x300pels/25.4mm", NULL, NULL);
    octet_bit_field(log, pkt, 43, "R16x15.4lines/mm and/or 400x400pels/25.4mm", NULL, NULL);
    if (frame_type == T30_DCS)
    {
        octet_bit_field(log, pkt, 44, "Resolution type selection", "Inch", "Metric");
        octet_reserved_bit(log, pkt, 45, 0);
        octet_reserved_bit(log, pkt, 46, 0);
        octet_reserved_bit(log, pkt, 47, 0);
    }
    else
    {
        octet_bit_field(log, pkt, 44, "Inch-based resolution preferred", NULL, NULL);
        octet_bit_field(log, pkt, 45, "Metric-based resolution preferred", NULL, NULL);
        octet_bit_field(log, pkt, 46, "Minimum scan line time for higher resolutions", "T15.4 = 1/2 T7.7", "T15.4 = T7.7");
        octet_bit_field(log, pkt, 47, "Selective polling", NULL, NULL);
    }
    octet_bit_field(log, pkt, 48, "Extension indicator", NULL, NULL);
    if (!(pkt[8] & DISBIT8))
        return;
    if (len <= 9)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 49, "Sub-addressing", NULL, NULL);
    if (frame_type == T30_DCS)
    {
        octet_bit_field(log, pkt, 50, "Sender identification transmission", NULL, NULL);
        octet_reserved_bit(log, pkt, 51, 0);
    }
    else
    {
        octet_bit_field(log, pkt, 50, "Password", NULL, NULL);
        octet_bit_field(log, pkt, 51, "Ready to transmit a data file (polling)", NULL, NULL);
    }
    octet_reserved_bit(log, pkt, 52, 0);
    octet_bit_field(log, pkt, 53, "Binary file transfer (BFT)", NULL, NULL);
    octet_bit_field(log, pkt, 54, "Document transfer mode (DTM)", NULL, NULL);
    octet_bit_field(log, pkt, 55, "Electronic data interchange (EDI)", NULL, NULL);
    octet_bit_field(log, pkt, 56, "Extension indicator", NULL, NULL);
    if (!(pkt[9] & DISBIT8))
        return;
    if (len <= 10)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 57, "Basic transfer mode (BTM)", NULL, NULL);
    octet_reserved_bit(log, pkt, 58, 0);
    if (frame_type == T30_DCS)
        octet_reserved_bit(log, pkt, 59, 0);
    else
        octet_bit_field(log, pkt, 59, "Ready to transfer a character or mixed mode document (polling)", NULL, NULL);
    octet_bit_field(log, pkt, 60, "Character mode", NULL, NULL);
    octet_reserved_bit(log, pkt, 61, 0);
    octet_bit_field(log, pkt, 62, "Mixed mode (Annex E/T.4)", NULL, NULL);
    octet_reserved_bit(log, pkt, 63, 0);
    octet_bit_field(log, pkt, 64, "Extension indicator", NULL, NULL);
    if (!(pkt[10] & DISBIT8))
        return;
    if (len <= 11)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 65, "Processable mode 26 (Rec. T.505)", NULL, NULL);
    octet_bit_field(log, pkt, 66, "Digital network capability", NULL, NULL);
    octet_bit_field(log, pkt, 67, "Duplex capability", "Full", "Half only");
    if (frame_type == T30_DCS)
        octet_bit_field(log, pkt, 68, "Full colour mode", NULL, NULL);
    else
        octet_bit_field(log, pkt, 68, "JPEG coding", NULL, NULL);
    octet_bit_field(log, pkt, 69, "Full colour mode", NULL, NULL);
    if (frame_type == T30_DCS)
        octet_bit_field(log, pkt, 70, "Preferred Huffman tables", NULL, NULL);
    else
        octet_reserved_bit(log, pkt, 70, 0);
    octet_bit_field(log, pkt, 71, "12bits/pel component", NULL, NULL);
    octet_bit_field(log, pkt, 72, "Extension indicator", NULL, NULL);
    if (!(pkt[11] & DISBIT8))
        return;
    if (len <= 12)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 73, "No subsampling (1:1:1)", NULL, NULL);
    octet_bit_field(log, pkt, 74, "Custom illuminant", NULL, NULL);
    octet_bit_field(log, pkt, 75, "Custom gamut range", NULL, NULL);
    octet_bit_field(log, pkt, 76, "North American Letter (215.9mm x 279.4mm)", NULL, NULL);
    octet_bit_field(log, pkt, 77, "North American Legal (215.9mm x 355.6mm)", NULL, NULL);
    octet_bit_field(log, pkt, 78, "Single-progression sequential coding (Rec. T.85) basic", NULL, NULL);
    octet_bit_field(log, pkt, 79, "Single-progression sequential coding (Rec. T.85) optional L0", NULL, NULL);
    octet_bit_field(log, pkt, 80, "Extension indicator", NULL, NULL);
    if (!(pkt[12] & DISBIT8))
        return;
    if (len <= 13)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 81, "HKM key management", NULL, NULL);
    octet_bit_field(log, pkt, 82, "RSA key management", NULL, NULL);
    octet_bit_field(log, pkt, 83, "Override", NULL, NULL);
    octet_bit_field(log, pkt, 84, "HFX40 cipher", NULL, NULL);
    octet_bit_field(log, pkt, 85, "Alternative cipher number 2", NULL, NULL);
    octet_bit_field(log, pkt, 86, "Alternative cipher number 3", NULL, NULL);
    octet_bit_field(log, pkt, 87, "HFX40-I hashing", NULL, NULL);
    octet_bit_field(log, pkt, 88, "Extension indicator", NULL, NULL);
    if (!(pkt[13] & DISBIT8))
        return;
    if (len <= 14)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 89, "Alternative hashing system 2", NULL, NULL);
    octet_bit_field(log, pkt, 90, "Alternative hashing system 3", NULL, NULL);
    octet_bit_field(log, pkt, 91, "Reserved for future security features", NULL, NULL);
    octet_field(log, pkt, 92, 94, "T.44 (Mixed Raster Content)", t44_mixed_raster_content_tags);
    octet_bit_field(log, pkt, 95, "Page length maximum stripe size for T.44 (Mixed Raster Content)", NULL, NULL);
    octet_bit_field(log, pkt, 96, "Extension indicator", NULL, NULL);
    if (!(pkt[14] & DISBIT8))
        return;
    if (len <= 15)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 97, "Colour/gray-scale 300pels/25.4mm x 300lines/25.4mm or 400pels/25.4mm x 400lines/25.4mm resolution", NULL, NULL);
    octet_bit_field(log, pkt, 98, "100pels/25.4mm x 100lines/25.4mm for colour/gray scale", NULL, NULL);
    octet_bit_field(log, pkt, 99, "Simple phase C BFT negotiations", NULL, NULL);
    if (frame_type == T30_DCS)
    {
        octet_reserved_bit(log, pkt, 100, 0);
        octet_reserved_bit(log, pkt, 101, 0);
    }
    else
    {
        octet_bit_field(log, pkt, 100, "Extended BFT Negotiations capable", NULL, NULL);
        octet_bit_field(log, pkt, 101, "Internet Selective Polling address (ISP)", NULL, NULL);
    }
    octet_bit_field(log, pkt, 102, "Internet Routing Address (IRA)", NULL, NULL);
    octet_reserved_bit(log, pkt, 103, 0);
    octet_bit_field(log, pkt, 104, "Extension indicator", NULL, NULL);
    if (!(pkt[15] & DISBIT8))
        return;
    if (len <= 16)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 105, "600pels/25.4mm x 600lines/25.4mm", NULL, NULL);
    octet_bit_field(log, pkt, 106, "1200pels/25.4mm x 1200lines/25.4mm", NULL, NULL);
    octet_bit_field(log, pkt, 107, "300pels/25.4mm x 600lines/25.4mm", NULL, NULL);
    octet_bit_field(log, pkt, 108, "400pels/25.4mm x 800lines/25.4mm", NULL, NULL);
    octet_bit_field(log, pkt, 109, "600pels/25.4mm x 1200lines/25.4mm", NULL, NULL);
    octet_bit_field(log, pkt, 110, "Colour/gray scale 600pels/25.4mm x 600lines/25.4mm", NULL, NULL);
    octet_bit_field(log, pkt, 111, "Colour/gray scale 1200pels/25.4mm x 1200lines/25.4mm", NULL, NULL);
    octet_bit_field(log, pkt, 112, "Extension indicator", NULL, NULL);
    if (!(pkt[16] & DISBIT8))
        return;
    if (len <= 17)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 113, "Double sided printing capability (alternate mode)", NULL, NULL);
    octet_bit_field(log, pkt, 114, "Double sided printing capability (continuous mode)", NULL, NULL);
    if (frame_type == T30_DCS)
        octet_bit_field(log, pkt, 115, "Black and white mixed raster content profile (MRCbw)", NULL, NULL);
    else
        octet_reserved_bit(log, pkt, 115, 0);
    octet_bit_field(log, pkt, 116, "T.45 (run length colour encoded)", NULL, NULL);
    octet_field(log, pkt, 117, 118, "Shared memory", shared_data_memory_capacity_tags);
    octet_bit_field(log, pkt, 119, "T.44 colour space", NULL, NULL);
    octet_bit_field(log, pkt, 120, "Extension indicator", NULL, NULL);
    if (!(pkt[17] & DISBIT8))
        return;
    if (len <= 18)
    {
        span_log(log, SPAN_LOG_FLOW, "  Frame is short\n");
        return;
    }

    octet_bit_field(log, pkt, 121, "Flow control capability for T.38 communication", NULL, NULL);
    octet_bit_field(log, pkt, 122, "K>4", NULL, NULL);
    octet_bit_field(log, pkt, 123, "Internet aware T.38 mode fax (not affected by data signal rate bits)", NULL, NULL);
    octet_field(log, pkt, 124, 126, "T.89 (Application profiles for ITU-T Rec T.88)", t89_profile_tags);
    octet_bit_field(log, pkt, 127, "sYCC-JPEG coding", NULL, NULL);
    octet_bit_field(log, pkt, 128, "Extension indicator", NULL, NULL);
    if (!(pkt[18] & DISBIT8))
        return;

    span_log(log, SPAN_LOG_FLOW, "  Extended beyond the current T.30 specification!\n");
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
