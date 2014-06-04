/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t30.c - ITU T.30 FAX transfer processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Steve Underwood
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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
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
#include "spandsp/t43.h"
#include "spandsp/t4_t6_decode.h"
#include "spandsp/t4_t6_encode.h"
#include "spandsp/t30_fcf.h"
#include "spandsp/t35.h"
#include "spandsp/t30.h"
#include "spandsp/t30_api.h"
#include "spandsp/t30_logging.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/timezone.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#include "spandsp/private/t42.h"
#include "spandsp/private/t43.h"
#include "spandsp/private/t4_t6_decode.h"
#include "spandsp/private/t4_t6_encode.h"
#include "spandsp/private/image_translate.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"
#include "spandsp/private/t30.h"
#include "spandsp/private/t30_dis_dtc_dcs_bits.h"

#include "t30_local.h"

/*! The maximum permitted number of retries of a single command allowed. */
#define MAX_COMMAND_TRIES   3

/*! The maximum permitted number of retries of a single response request allowed. This
    is not specified in T.30. However, if you don't apply some limit a messed up FAX
    terminal could keep you retrying all day. Its a backstop protection. */
#define MAX_RESPONSE_TRIES  6

/* T.30 defines the following call phases:
   Phase A: Call set-up.
       Exchange of CNG, CED and the called terminal identification.
   Phase B: Pre-message procedure for identifying and selecting the required facilities.
       Capabilities negotiation, and training, up the the confirmation to receive.
   Phase C: Message transmission (includes phasing and synchronization where appropriate).
       Transfer of the message at high speed.
   Phase D: Post-message procedure, including end-of-message and confirmation and multi-document procedures.
       End of message and acknowledgement.
   Phase E: Call release
       Final call disconnect. */
enum
{
    T30_PHASE_IDLE = 0,     /* Freshly initialised */
    T30_PHASE_A_CED,        /* Doing the CED (answer) sequence */
    T30_PHASE_A_CNG,        /* Doing the CNG (caller) sequence */
    T30_PHASE_B_RX,         /* Receiving pre-message control messages */
    T30_PHASE_B_TX,         /* Transmitting pre-message control messages */
    T30_PHASE_C_NON_ECM_RX, /* Receiving a document message in non-ECM mode */
    T30_PHASE_C_NON_ECM_TX, /* Transmitting a document message in non-ECM mode */
    T30_PHASE_C_ECM_RX,     /* Receiving a document message in ECM (HDLC) mode */
    T30_PHASE_C_ECM_TX,     /* Transmitting a document message in ECM (HDLC) mode */
    T30_PHASE_D_RX,         /* Receiving post-message control messages */
    T30_PHASE_D_TX,         /* Transmitting post-message control messages */
    T30_PHASE_E,            /* In phase E */
    T30_PHASE_CALL_FINISHED /* Call completely finished */
};

static const char *phase_names[] =
{
    "IDLE",
    "A_CED",
    "A_CNG",
    "B_RX",
    "B_TX",
    "C_NON_ECM_RX",
    "C_NON_ECM_TX",
    "C_ECM_RX",
    "C_ECM_TX",
    "D_RX",
    "D_TX",
    "E",
    "CALL_FINISHED"
};

/* These state names are modelled after places in the T.30 flow charts. */
enum
{
    T30_STATE_ANSWERING = 1,
    T30_STATE_B,
    T30_STATE_C,
    T30_STATE_D,
    T30_STATE_D_TCF,
    T30_STATE_D_POST_TCF,
    T30_STATE_F_TCF,
    T30_STATE_F_CFR,
    T30_STATE_F_FTT,
    T30_STATE_F_DOC_NON_ECM,
    T30_STATE_F_POST_DOC_NON_ECM,
    T30_STATE_F_DOC_ECM,
    T30_STATE_F_POST_DOC_ECM,
    T30_STATE_F_POST_RCP_MCF,
    T30_STATE_F_POST_RCP_PPR,
    T30_STATE_F_POST_RCP_RNR,
    T30_STATE_R,
    T30_STATE_T,
    T30_STATE_I,
    T30_STATE_II,
    T30_STATE_II_Q,
    T30_STATE_III_Q_MCF,
    T30_STATE_III_Q_RTP,
    T30_STATE_III_Q_RTN,
    T30_STATE_IV,
    T30_STATE_IV_PPS_NULL,
    T30_STATE_IV_PPS_Q,
    T30_STATE_IV_PPS_RNR,
    T30_STATE_IV_CTC,
    T30_STATE_IV_EOR,
    T30_STATE_IV_EOR_RNR,
    T30_STATE_CALL_FINISHED
};

static const char *state_names[] =
{
    "NONE",
    "ANSWERING",
    "B",
    "C",
    "D",
    "D_TCF",
    "D_POST_TCF",
    "F_TCF",
    "F_CFR",
    "F_FTT",
    "F_DOC_NON_ECM",
    "F_POST_DOC_NON_ECM",
    "F_DOC_ECM",
    "F_POST_DOC_ECM",
    "F_POST_RCP_MCF",
    "F_POST_RCP_PPR",
    "F_POST_RCP_RNR",
    "R",
    "T",
    "I",
    "II",
    "II_Q",
    "III_Q_MCF",
    "III_Q_RTP",
    "III_Q_RTN",
    "IV",
    "IV_PPS_NULL",
    "IV_PPS_Q",
    "IV_PPS_RNR",
    "IV_CTC",
    "IV_EOR",
    "IV_EOR_RNR",
    "CALL_FINISHED"
};

enum
{
    T30_MIN_SCAN_20MS = 0,
    T30_MIN_SCAN_5MS = 1,
    T30_MIN_SCAN_10MS = 2,
    T30_MIN_SCAN_40MS = 4,
    T30_MIN_SCAN_0MS = 7,
};

enum
{
    T30_MODE_SEND_DOC = 1,
    T30_MODE_RECEIVE_DOC
};

/*! These are internal assessments of received image quality, used to determine whether we
    continue, retrain, or abandon the call. This is only relevant to non-ECM operation. */
enum
{
    T30_COPY_QUALITY_PERFECT = 0,
    T30_COPY_QUALITY_GOOD,
    T30_COPY_QUALITY_POOR,
    T30_COPY_QUALITY_BAD
};

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

/*! There are high level indications of what is happening at any instant, to guide the cleanup
    continue, retrain, or abandoning of the call. */
enum
{
    OPERATION_IN_PROGRESS_NONE = 0,
    OPERATION_IN_PROGRESS_T4_RX,
    OPERATION_IN_PROGRESS_T4_TX,
    OPERATION_IN_PROGRESS_POST_T4_RX,
    OPERATION_IN_PROGRESS_POST_T4_TX
};

/* All timers specified in milliseconds */

/*! Time-out T0 defines the amount of time an automatic calling terminal waits for the called terminal
    to answer the call.
    T0 begins after the dialling of the number is completed and is reset:
    a) when T0 times out; or
    b) when timer T1 is started; or
    c) if the terminal is capable of detecting any condition which indicates that the call will not be
       successful, when such a condition is detected.
    The recommended value of T0 is 60+-5s. However, when it is anticipated that a long call set-up
    time may be encountered, an alternative value of up to 120s may be used.
    NOTE - National regulations may require the use of other values for T0. */
#define DEFAULT_TIMER_T0                60000

/*! Time-out T1 defines the amount of time two terminals will continue to attempt to identify each
    other. T1 is 35+-5s, begins upon entering phase B, and is reset upon detecting a valid signal or
    when T1 times out.
    For operating methods 3 and 4 (see 3.1), the calling terminal starts time-out T1 upon reception of
    the V.21 modulation scheme.
    For operating method 4 bis a (see 3.1), the calling terminal starts time-out T1 upon starting
    transmission using the V.21 modulation scheme.
    Annex A says T1 is also the timeout to be used for the receipt of the first HDLC frame after the
    start of high speed flags in ECM mode. This seems a strange reuse of the T1 name, so we distinguish
    it here by calling it T1A. */
#define DEFAULT_TIMER_T1                35000
#define DEFAULT_TIMER_T1A               35000

/*! Time-out T2 makes use of the tight control between commands and responses to detect the loss of
    command/response synchronization. T2 is 6+-1s, and begins when initiating a command search
    (e.g., the first entrance into the "command received" subroutine, reference flow diagram in section 5.2).
    T2 is reset when an HDLC flag is received or when T2 times out. */
#define DEFAULT_TIMER_T2                7000

/*! Once HDLC flags begin, T2 is reset, and a 3s timer begins. This timer is unnamed in T.30. Here we
    term it T2A. No tolerance is specified for this timer. T2A specifies the maximum time to wait for the
    end of a frame, after the initial flag has been seen. */
#define DEFAULT_TIMER_T2A               3000

/*! If the HDLC carrier falls during reception, we need to apply a minimum time before continuing. If we
    don't, there are circumstances where we could continue and reply before the incoming signals have
    really finished. E.g. if a bad DCS is received in a DCS-TCF sequence, we need wait for the TCF
    carrier to pass, before continuing. This timer is specified as 200ms, but no tolerance is specified.
    It is unnamed in T.30. Here we term it T2B */
#define DEFAULT_TIMER_T2B               200

/*! Time-out T3 defines the amount of time a terminal will attempt to alert the local operator in
    response to a procedural interrupt. Failing to achieve operator intervention, the terminal will
    discontinue this attempt and shall issue other commands or responses. T3 is 10+-5s, begins on the
    first detection of a procedural interrupt command/response signal (i.e., PIN/PIP or PRI-Q) and is
    reset when T3 times out or when the operator initiates a line request. */
#define DEFAULT_TIMER_T3                15000

/*! Time-out T4 defines the amount of time a terminal will wait for flags to begin, when waiting for a
    response from a remote terminal. T2 is 3s +-15%, and begins when initiating a response search
    (e.g., the first entrance into the "response received" subroutine, reference flow diagram in section 5.2).
    T4 is reset when an HDLC flag is received or when T4 times out.
    NOTE - For manual FAX units, the value of timer T4 may be either 3.0s +-15% or 4.5s +-15%.
    If the value of 4.5s is used, then after detection of a valid response to the first DIS, it may
    be reduced to 3.0s +-15%. T4 = 3.0s +-15% for automatic units. */
#define DEFAULT_TIMER_T4                3450

/*! Once HDLC flags begin, T4 is reset, and a 3s timer begins. This timer is unnamed in T.30. Here we
    term it T4A. No tolerance is specified for this timer. T4A specifies the maximum time to wait for the
    end of a frame, after the initial flag has been seen. Note that a different timer is used for the fast
    HDLC in ECM mode, to provide time for physical paper handling. */
#define DEFAULT_TIMER_T4A               3000

/*! If the HDLC carrier falls during reception, we need to apply a minimum time before continuing. if we
    don't, there are circumstances where we could continue and reply before the incoming signals have
    really finished. E.g. if a bad DCS is received in a DCS-TCF sequence, we need wait for the TCF
    carrier to pass, before continuing. This timer is specified as 200ms, but no tolerance is specified.
    It is unnamed in T.30. Here we term it T4B */
#define DEFAULT_TIMER_T4B               200

/*! Time-out T5 is defined for the optional T.4 error correction mode. Time-out T5 defines the amount
    of time waiting for clearance of the busy condition of the receiving terminal. T5 is 60+-5s and
    begins on the first detection of the RNR response. T5 is reset when T5 times out or the MCF or PIP
    response is received or when the ERR or PIN response is received in the flow control process after
    transmitting the EOR command. If the timer T5 has expired, the DCN command is transmitted for
    call release. */
#define DEFAULT_TIMER_T5                65000

/*! (Annex C - ISDN) Time-out T6 defines the amount of time two terminals will continue to attempt to
    identify each other. T6 is 5+-0.5s. The timeout begins upon entering Phase B, and is reset upon
    detecting a valid signal, or when T6 times out. */
#define DEFAULT_TIMER_T6                5000

/*! (Annex C - ISDN) Time-out T7 is used to detect loss of command/response synchronization. T7 is 6+-1s.
    The timeout begins when initiating a command search (e.g., the first entrance into the "command received"
    subroutine - see flow diagram in C.5) and is reset upon detecting a valid signal or when T7 times out. */
#define DEFAULT_TIMER_T7                7000

/*! (Annex C - ISDN) Time-out T8 defines the amount of time waiting for clearance of the busy condition
    of the receiving terminal. T8 is 10+-1s. The timeout begins on the first detection of the combination
    of no outstanding corrections and the RNR response. T8 is reset when T8 times out or MCF response is
    received. If the timer T8 expires, a DCN command is transmitted for call release. */
#define DEFAULT_TIMER_T8                10000

/*! Final time we allow for things to flush through the system, before we disconnect, in milliseconds.
    200ms should be fine for a PSTN call. For a T.38 call something longer is desirable. */
#define FINAL_FLUSH_TIME                1000

/*! The number of PPRs received before CTC or EOR is sent in ECM mode. T.30 defines this as 4,
    but it could be varied, and the Japanese spec, for example, does make this value a
    variable. */
#define PPR_LIMIT_BEFORE_CTC_OR_EOR     4

/* HDLC message header byte values */
#define ADDRESS_FIELD                   0xFF
#define CONTROL_FIELD_NON_FINAL_FRAME   0x03
#define CONTROL_FIELD_FINAL_FRAME       0x13

enum
{
    TIMER_IS_IDLE = 0,
    TIMER_IS_T2,
    TIMER_IS_T1A,
    TIMER_IS_T2A,
    TIMER_IS_T2B,
    TIMER_IS_T2C,
    TIMER_IS_T4,
    TIMER_IS_T4A,
    TIMER_IS_T4B,
    TIMER_IS_T4C
};

/* Start points in the fallback table for different capabilities */
/*! The starting point in the modem fallback sequence if V.17 is allowed */
#define T30_V17_FALLBACK_START          0
/*! The starting point in the modem fallback sequence if V.17 is not allowed */
#define T30_V29_FALLBACK_START          3
/*! The starting point in the modem fallback sequence if V.29 is not allowed */
#define T30_V27TER_FALLBACK_START       6

static const struct
{
    int bit_rate;
    int modem_type;
    int which;
    uint8_t dcs_code;
} fallback_sequence[] =
{
    {14400, T30_MODEM_V17,      T30_SUPPORT_V17,    DISBIT6},
    {12000, T30_MODEM_V17,      T30_SUPPORT_V17,    (DISBIT6 | DISBIT4)},
    { 9600, T30_MODEM_V17,      T30_SUPPORT_V17,    (DISBIT6 | DISBIT3)},
    { 9600, T30_MODEM_V29,      T30_SUPPORT_V29,    DISBIT3},
    { 7200, T30_MODEM_V17,      T30_SUPPORT_V17,    (DISBIT6 | DISBIT4 | DISBIT3)},
    { 7200, T30_MODEM_V29,      T30_SUPPORT_V29,    (DISBIT4 | DISBIT3)},
    { 4800, T30_MODEM_V27TER,   T30_SUPPORT_V27TER, DISBIT4},
    { 2400, T30_MODEM_V27TER,   T30_SUPPORT_V27TER, 0},
    {    0, 0,                  0,                  0}
};

static void queue_phase(t30_state_t *s, int phase);
static void set_phase(t30_state_t *s, int phase);
static void set_state(t30_state_t *s, int state);
static void shut_down_hdlc_tx(t30_state_t *s);
static void send_frame(t30_state_t *s, const uint8_t *fr, int frlen);
static void send_simple_frame(t30_state_t *s, int type);
static void send_dcn(t30_state_t *s);
static void repeat_last_command(t30_state_t *s);
static void disconnect(t30_state_t *s);
static void decode_20digit_msg(t30_state_t *s, char *msg, const uint8_t *pkt, int len);
static void decode_url_msg(t30_state_t *s, char *msg, const uint8_t *pkt, int len);
static int decode_nsf_nss_nsc(t30_state_t *s, uint8_t *msg[], const uint8_t *pkt, int len);
static void set_min_scan_time(t30_state_t *s);
static int build_dcs(t30_state_t *s);
static void timer_t2_start(t30_state_t *s);
static void timer_t2a_start(t30_state_t *s);
static void timer_t2b_start(t30_state_t *s);
static void timer_t4_start(t30_state_t *s);
static void timer_t4a_start(t30_state_t *s);
static void timer_t4b_start(t30_state_t *s);
static void timer_t2_t4_stop(t30_state_t *s);

/*! Test a specified bit within a DIS, DTC or DCS frame */
#define test_ctrl_bit(s,bit) ((s)[3 + ((bit - 1)/8)] & (1 << ((bit - 1)%8)))
/*! Set a specified bit within a DIS, DTC or DCS frame */
#define set_ctrl_bit(s,bit) (s)[3 + ((bit - 1)/8)] |= (1 << ((bit - 1)%8))
/*! Set a specified block of bits within a DIS, DTC or DCS frame */
#define set_ctrl_bits(s,val,bit) (s)[3 + ((bit - 1)/8)] |= ((val) << ((bit - 1)%8))
/*! Clear a specified bit within a DIS, DTC or DCS frame */
#define clr_ctrl_bit(s,bit) (s)[3 + ((bit - 1)/8)] &= ~(1 << ((bit - 1)%8))

static int find_fallback_entry(int dcs_code)
{
    int i;

    /* The table is short, and not searched often, so a brain-dead linear scan seems OK */
    for (i = 0;  fallback_sequence[i].bit_rate;  i++)
    {
        if (fallback_sequence[i].dcs_code == dcs_code)
            break;
    }
    if (fallback_sequence[i].bit_rate == 0)
        return -1;
    return i;
}
/*- End of function --------------------------------------------------------*/

static int step_fallback_entry(t30_state_t *s)
{
    while (fallback_sequence[++s->current_fallback].bit_rate)
    {
        if ((fallback_sequence[s->current_fallback].which & s->current_permitted_modems))
            break;
    }
    if (fallback_sequence[s->current_fallback].bit_rate == 0)
    {
        /* Reset the fallback sequence */
        s->current_fallback = 0;
        return -1;
    }
    /* We need to update the minimum scan time, in case we are in non-ECM mode. */
    set_min_scan_time(s);
    /* Now we need to rebuild the DCS message we will send. */
    build_dcs(s);
    return s->current_fallback;
}
/*- End of function --------------------------------------------------------*/

static int terminate_operation_in_progress(t30_state_t *s)
{
    /* Make sure any FAX in progress is tidied up. If the tidying up has
       already happened, repeating it here is harmless. */
    switch (s->operation_in_progress)
    {
    case OPERATION_IN_PROGRESS_T4_TX:
        t4_tx_release(&s->t4.tx);
        s->operation_in_progress = OPERATION_IN_PROGRESS_POST_T4_TX;
        break;
    case OPERATION_IN_PROGRESS_T4_RX:
        t4_rx_release(&s->t4.rx);
        s->operation_in_progress = OPERATION_IN_PROGRESS_POST_T4_RX;
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int tx_start_page(t30_state_t *s)
{
    if (t4_tx_start_page(&s->t4.tx))
    {
        terminate_operation_in_progress(s);
        return -1;
    }
    s->ecm_block = 0;
    s->error_correcting_mode_retries = 0;
    span_log(&s->logging, SPAN_LOG_FLOW, "Starting page %d of transfer\n", s->tx_page_number + 1);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int tx_end_page(t30_state_t *s)
{
    s->retries = 0;
    if (t4_tx_end_page(&s->t4.tx) == 0)
    {
        s->tx_page_number++;
        s->ecm_block = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_start_page(t30_state_t *s)
{
    int i;

    t4_rx_set_image_width(&s->t4.rx, s->image_width);
    t4_rx_set_sub_address(&s->t4.rx, s->rx_info.sub_address);
    t4_rx_set_dcs(&s->t4.rx, s->rx_dcs_string);
    t4_rx_set_far_ident(&s->t4.rx, s->rx_info.ident);
    t4_rx_set_vendor(&s->t4.rx, s->vendor);
    t4_rx_set_model(&s->t4.rx, s->model);

    t4_rx_set_rx_encoding(&s->t4.rx, s->line_compression);
    t4_rx_set_x_resolution(&s->t4.rx, s->x_resolution);
    t4_rx_set_y_resolution(&s->t4.rx, s->y_resolution);

    if (t4_rx_start_page(&s->t4.rx))
        return -1;
    /* Clear the ECM buffer */
    for (i = 0;  i < 256;  i++)
        s->ecm_len[i] = -1;
    s->ecm_block = 0;
    s->ecm_frames = -1;
    s->ecm_frames_this_tx_burst = 0;
    s->error_correcting_mode_retries = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_end_page(t30_state_t *s)
{
    if (t4_rx_end_page(&s->t4.rx) == 0)
    {
        s->rx_page_number++;
        s->ecm_block = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void report_rx_ecm_page_result(t30_state_t *s)
{
    t4_stats_t stats;

    /* This is only used for ECM pages, as copy_quality() does a similar job for non-ECM
       pages as a byproduct of assessing copy quality. */
    t4_rx_get_transfer_statistics(&s->t4.rx, &stats);
    span_log(&s->logging, SPAN_LOG_FLOW, "Page no = %d\n", stats.pages_transferred);
    span_log(&s->logging, SPAN_LOG_FLOW, "Image size = %d x %d pixels\n", stats.width, stats.length);
    span_log(&s->logging, SPAN_LOG_FLOW, "Image resolution = %d/m x %d/m\n", stats.x_resolution, stats.y_resolution);
    span_log(&s->logging, SPAN_LOG_FLOW, "Compression = %s (%d)\n", t4_compression_to_str(stats.compression), stats.compression);
    span_log(&s->logging, SPAN_LOG_FLOW, "Compressed image size = %d bytes\n", stats.line_image_size);
}
/*- End of function --------------------------------------------------------*/

static int copy_quality(t30_state_t *s)
{
    t4_stats_t stats;
    int quality;

    t4_rx_get_transfer_statistics(&s->t4.rx, &stats);
    /* There is no specification for judging copy quality. However, we need to classify
       it at three levels, to control what we do next: OK; tolerable, but retrain;
       intolerable. */
    /* Based on the thresholds used in the TSB85 tests, we consider:
            <5% bad rows is OK
            <15% bad rows to be tolerable, but retrain
            >15% bad rows to be intolerable
     */
    /* This is called before the page is confirmed, so we need to add one to get the page
       number right */
    span_log(&s->logging, SPAN_LOG_FLOW, "Page no = %d\n", stats.pages_transferred + 1);
    span_log(&s->logging, SPAN_LOG_FLOW, "Image size = %d x %d pixels\n", stats.width, stats.length);
    span_log(&s->logging, SPAN_LOG_FLOW, "Image resolution = %d/m x %d/m\n", stats.x_resolution, stats.y_resolution);
    span_log(&s->logging, SPAN_LOG_FLOW, "Compression = %s (%d)\n", t4_compression_to_str(stats.compression), stats.compression);
    span_log(&s->logging, SPAN_LOG_FLOW, "Compressed image size = %d bytes\n", stats.line_image_size);
    span_log(&s->logging, SPAN_LOG_FLOW, "Bad rows = %d\n", stats.bad_rows);
    span_log(&s->logging, SPAN_LOG_FLOW, "Longest bad row run = %d\n", stats.longest_bad_row_run);
    /* Don't treat a page as perfect because it has zero bad rows out of zero total rows. A zero row
       page has got to be some kind of total page failure. */
    if (stats.bad_rows == 0  &&  stats.length != 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Page quality is perfect\n");
        quality = T30_COPY_QUALITY_PERFECT;
    }
    else if (stats.bad_rows*20 < stats.length)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Page quality is good\n");
        quality = T30_COPY_QUALITY_GOOD;
    }
    else if (stats.bad_rows*20 < stats.length*3)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Page quality is poor\n");
        quality = T30_COPY_QUALITY_POOR;
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Page quality is bad\n");
        quality = T30_COPY_QUALITY_BAD;
    }
    return quality;
}
/*- End of function --------------------------------------------------------*/

static void report_tx_result(t30_state_t *s, int result)
{
    t4_stats_t stats;

    if (span_log_test(&s->logging, SPAN_LOG_FLOW))
    {
        t4_tx_get_transfer_statistics(&s->t4.tx, &stats);
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "%s - delivered %d pages\n",
                 (result)  ?  "Success"  :  "Failure",
                 stats.pages_transferred);
    }
}
/*- End of function --------------------------------------------------------*/

static void release_resources(t30_state_t *s)
{
    if (s->tx_info.nsf)
    {
        span_free(s->tx_info.nsf);
        s->tx_info.nsf = NULL;
    }
    s->tx_info.nsf_len = 0;
    if (s->tx_info.nsc)
    {
        span_free(s->tx_info.nsc);
        s->tx_info.nsc = NULL;
    }
    s->tx_info.nsc_len = 0;
    if (s->tx_info.nss)
    {
        span_free(s->tx_info.nss);
        s->tx_info.nss = NULL;
    }
    s->tx_info.nss_len = 0;
    if (s->tx_info.tsa)
    {
        span_free(s->tx_info.tsa);
        s->tx_info.tsa = NULL;
    }
    if (s->tx_info.ira)
    {
        span_free(s->tx_info.ira);
        s->tx_info.ira = NULL;
    }
    if (s->tx_info.cia)
    {
        span_free(s->tx_info.cia);
        s->tx_info.cia = NULL;
    }
    if (s->tx_info.isp)
    {
        span_free(s->tx_info.isp);
        s->tx_info.isp = NULL;
    }
    if (s->tx_info.csa)
    {
        span_free(s->tx_info.csa);
        s->tx_info.csa = NULL;
    }

    if (s->rx_info.nsf)
    {
        span_free(s->rx_info.nsf);
        s->rx_info.nsf = NULL;
    }
    s->rx_info.nsf_len = 0;
    if (s->rx_info.nsc)
    {
        span_free(s->rx_info.nsc);
        s->rx_info.nsc = NULL;
    }
    s->rx_info.nsc_len = 0;
    if (s->rx_info.nss)
    {
        span_free(s->rx_info.nss);
        s->rx_info.nss = NULL;
    }
    s->rx_info.nss_len = 0;
    if (s->rx_info.tsa)
    {
        span_free(s->rx_info.tsa);
        s->rx_info.tsa = NULL;
    }
    if (s->rx_info.ira)
    {
        span_free(s->rx_info.ira);
        s->rx_info.ira = NULL;
    }
    if (s->rx_info.cia)
    {
        span_free(s->rx_info.cia);
        s->rx_info.cia = NULL;
    }
    if (s->rx_info.isp)
    {
        span_free(s->rx_info.isp);
        s->rx_info.isp = NULL;
    }
    if (s->rx_info.csa)
    {
        span_free(s->rx_info.csa);
        s->rx_info.csa = NULL;
    }
}
/*- End of function --------------------------------------------------------*/

static uint8_t check_next_tx_step(t30_state_t *s)
{
    int res;
    int more;

    res = t4_tx_next_page_has_different_format(&s->t4.tx);
    if (res == 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "More pages to come with the same format\n");
        return (s->local_interrupt_pending)  ?  T30_PRI_MPS  :  T30_MPS;
    }
    if (res > 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "More pages to come with a different format\n");
        s->tx_start_page = t4_tx_get_current_page_in_file(&s->t4.tx) + 1;
        return (s->local_interrupt_pending)  ?  T30_PRI_EOM  :  T30_EOM;
    }
    /* Call a user handler, if one is set, to check if another document is to be sent.
       If so, we send an EOM, rather than an EOP. Then we will renegotiate, and the new
       document will begin. */
    if (s->document_handler)
        more = s->document_handler(s->document_user_data, 0);
    else
        more = false;
    if (more)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Another document to send\n");
        //if (test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_MULTIPLE_SELECTIVE_POLLING_CAPABLE))
        //    return T30_EOS;
        return (s->local_interrupt_pending)  ?  T30_PRI_EOM  :  T30_EOM;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "No more pages to send\n");
    return (s->local_interrupt_pending)  ?  T30_PRI_EOP  :  T30_EOP;
}
/*- End of function --------------------------------------------------------*/

static int get_partial_ecm_page(t30_state_t *s)
{
    int i;
    int len;

    s->ppr_count = 0;
    s->ecm_progress = 0;
    /* Fill our partial page buffer with a partial page. Use the negotiated preferred frame size
       as the basis for the size of the frames produced. */
    /* We fill the buffer with complete HDLC frames, ready to send out. */
    /* The frames are all marked as not being final frames. When sent, the are followed by a partial
       page signal, which is marked as the final frame. */
    for (i = 3;  i < 32 + 3;  i++)
        s->ecm_frame_map[i] = 0xFF;
    for (i = 0;  i < 256;  i++)
    {
        s->ecm_len[i] = -1;
        s->ecm_data[i][0] = ADDRESS_FIELD;
        s->ecm_data[i][1] = CONTROL_FIELD_NON_FINAL_FRAME;
        s->ecm_data[i][2] = T4_FCD;
        /* These frames contain a frame sequence number within the partial page (one octet) followed
           by some image data. */
        s->ecm_data[i][3] = (uint8_t) i;
        if (s->document_get_handler)
            len = s->document_get_handler(s->document_get_user_data, &s->ecm_data[i][4], s->octets_per_ecm_frame);
        else
            len = t4_tx_get(&s->t4.tx, &s->ecm_data[i][4], s->octets_per_ecm_frame);
        if (len < s->octets_per_ecm_frame)
        {
            /* The document is not big enough to fill the entire buffer */
            /* We need to pad to a full frame, as most receivers expect that. */
            if (len > 0)
            {
                memset(&s->ecm_data[i][4 + len], 0, s->octets_per_ecm_frame - len);
                s->ecm_len[i++] = (int16_t) (s->octets_per_ecm_frame + 4);
            }
            s->ecm_frames = i;
            span_log(&s->logging, SPAN_LOG_FLOW, "Partial document buffer contains %d frames (%d per frame)\n", i, s->octets_per_ecm_frame);
            s->ecm_at_page_end = true;
            return i;
        }
        s->ecm_len[i] = (int16_t) (4 + len);
    }
    /* We filled the entire buffer */
    s->ecm_frames = 256;
    span_log(&s->logging, SPAN_LOG_FLOW, "Partial page buffer full (%d per frame)\n", s->octets_per_ecm_frame);
    s->ecm_at_page_end = (t4_tx_image_complete(&s->t4.tx) == SIG_STATUS_END_OF_DATA);
    return 256;
}
/*- End of function --------------------------------------------------------*/

static int send_next_ecm_frame(t30_state_t *s)
{
    int i;
    uint8_t frame[3];

    if (s->ecm_current_tx_frame < s->ecm_frames)
    {
        /* Search for the next frame, within the current partial page, which has
           not been tagged as transferred OK. */
        for (i = s->ecm_current_tx_frame;  i < s->ecm_frames;  i++)
        {
            if (s->ecm_len[i] >= 0)
            {
                send_frame(s, s->ecm_data[i], s->ecm_len[i]);
                s->ecm_current_tx_frame = i + 1;
                s->ecm_frames_this_tx_burst++;
                return 0;
            }
        }
        s->ecm_current_tx_frame = s->ecm_frames;
    }
    if (s->ecm_current_tx_frame < s->ecm_frames + 3)
    {
        /* We have sent all the FCD frames. Send three RCP frames, as per
           T.4/A.1 and T.4/A.2. The repeats are to minimise the risk of a bit
           error stopping the receiving end from recognising the RCP. */
        s->ecm_current_tx_frame++;
        /* The RCP frame is an odd man out, as its a simple 1 byte control
           frame, but is specified to not have the final bit set. It doesn't
           seem to have the DIS received bit set, either. */
        frame[0] = ADDRESS_FIELD;
        frame[1] = CONTROL_FIELD_NON_FINAL_FRAME;
        frame[2] = T4_RCP;
        send_frame(s, frame, 3);
        /* In case we are just after a CTC/CTR exchange, which kicked us back
           to long training */
        s->short_train = true;
        return 0;
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

static void send_rr(t30_state_t *s)
{
    if (s->current_status != T30_ERR_TX_T5EXP)
        send_simple_frame(s, T30_RR);
    else
        send_dcn(s);
}
/*- End of function --------------------------------------------------------*/

static int send_first_ecm_frame(t30_state_t *s)
{
    s->ecm_current_tx_frame = 0;
    s->ecm_frames_this_tx_burst = 0;
    return send_next_ecm_frame(s);
}
/*- End of function --------------------------------------------------------*/

static void print_frame(t30_state_t *s, const char *io, const uint8_t *msg, int len)
{
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "%s %s with%s final frame tag\n",
             io,
             t30_frametype(msg[2]),
             (msg[1] & 0x10)  ?  ""  :  "out");
    span_log_buf(&s->logging, SPAN_LOG_FLOW, io, msg, len);
}
/*- End of function --------------------------------------------------------*/

static void shut_down_hdlc_tx(t30_state_t *s)
{
    if (s->send_hdlc_handler)
        s->send_hdlc_handler(s->send_hdlc_user_data, NULL, 0);
}
/*- End of function --------------------------------------------------------*/

static void send_frame(t30_state_t *s, const uint8_t *msg, int len)
{
    print_frame(s, "Tx: ", msg, len);

    if (s->real_time_frame_handler)
        s->real_time_frame_handler(s->real_time_frame_user_data, false, msg, len);
    if (s->send_hdlc_handler)
        s->send_hdlc_handler(s->send_hdlc_user_data, msg, len);
}
/*- End of function --------------------------------------------------------*/

static void send_simple_frame(t30_state_t *s, int type)
{
    uint8_t frame[3];

    /* The simple command/response frames are always final frames */
    frame[0] = ADDRESS_FIELD;
    frame[1] = CONTROL_FIELD_FINAL_FRAME;
    frame[2] = (uint8_t) (type | s->dis_received);
    send_frame(s, frame, 3);
}
/*- End of function --------------------------------------------------------*/

static void send_20digit_msg_frame(t30_state_t *s, int cmd, char *msg)
{
    size_t len;
    int p;
    uint8_t frame[23];

    len = strlen(msg);
    p = 0;
    frame[p++] = ADDRESS_FIELD;
    frame[p++] = CONTROL_FIELD_NON_FINAL_FRAME;
    frame[p++] = (uint8_t) (cmd | s->dis_received);
    while (len > 0)
        frame[p++] = msg[--len];
    while (p < 23)
        frame[p++] = ' ';
    send_frame(s, frame, 23);
}
/*- End of function --------------------------------------------------------*/

static int send_nsf_frame(t30_state_t *s)
{
    /* Only send if there is an NSF message to send. */
    if (s->tx_info.nsf  &&  s->tx_info.nsf_len)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending user supplied NSF - %d octets\n", s->tx_info.nsf_len);
        s->tx_info.nsf[0] = ADDRESS_FIELD;
        s->tx_info.nsf[1] = CONTROL_FIELD_NON_FINAL_FRAME;
        s->tx_info.nsf[2] = T30_NSF;
        send_frame(s, s->tx_info.nsf, s->tx_info.nsf_len + 3);
        return true;
    }
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_nss_frame(t30_state_t *s)
{
    /* Only send if there is an NSF message to send. */
    if (s->tx_info.nss  &&  s->tx_info.nss_len)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending user supplied NSS - %d octets\n", s->tx_info.nss_len);
        s->tx_info.nss[0] = ADDRESS_FIELD;
        s->tx_info.nss[1] = CONTROL_FIELD_NON_FINAL_FRAME;
        s->tx_info.nss[2] = (uint8_t) (T30_NSS | s->dis_received);
        send_frame(s, s->tx_info.nss, s->tx_info.nss_len + 3);
        return true;
    }
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_nsc_frame(t30_state_t *s)
{
    /* Only send if there is an NSF message to send. */
    if (s->tx_info.nsc  &&  s->tx_info.nsc_len)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending user supplied NSC - %d octets\n", s->tx_info.nsc_len);
        s->tx_info.nsc[0] = ADDRESS_FIELD;
        s->tx_info.nsc[1] = CONTROL_FIELD_NON_FINAL_FRAME;
        s->tx_info.nsc[2] = T30_NSC;
        send_frame(s, s->tx_info.nsc, s->tx_info.nsc_len + 3);
        return true;
    }
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_ident_frame(t30_state_t *s, uint8_t cmd)
{
    if (s->tx_info.ident[0])
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending ident '%s'\n", s->tx_info.ident);
        /* 'cmd' should be T30_TSI, T30_CIG or T30_CSI */
        send_20digit_msg_frame(s, cmd, s->tx_info.ident);
        return true;
    }
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_psa_frame(t30_state_t *s)
{
    if (test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_POLLED_SUBADDRESSING_CAPABLE)  &&  s->tx_info.polled_sub_address[0])
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending polled sub-address '%s'\n", s->tx_info.polled_sub_address);
        send_20digit_msg_frame(s, T30_PSA, s->tx_info.polled_sub_address);
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_POLLED_SUBADDRESSING_CAPABLE);
        return true;
    }
    clr_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_POLLED_SUBADDRESSING_CAPABLE);
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_sep_frame(t30_state_t *s)
{
    if (test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_SELECTIVE_POLLING_CAPABLE)  &&  s->tx_info.selective_polling_address[0])
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending selective polling address '%s'\n", s->tx_info.selective_polling_address);
        send_20digit_msg_frame(s, T30_SEP, s->tx_info.selective_polling_address);
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_SELECTIVE_POLLING_CAPABLE);
        return true;
    }
    clr_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_SELECTIVE_POLLING_CAPABLE);
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_sid_frame(t30_state_t *s)
{
    /* Only send if there is an ID to send. */
    if (test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_PASSWORD)  &&  s->tx_info.sender_ident[0])
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending sender identification '%s'\n", s->tx_info.sender_ident);
        send_20digit_msg_frame(s, T30_SID, s->tx_info.sender_ident);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_SENDER_ID_TRANSMISSION);
        return true;
    }
    clr_ctrl_bit(s->dcs_frame, T30_DCS_BIT_SENDER_ID_TRANSMISSION);
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_pwd_frame(t30_state_t *s)
{
    /* Only send if there is a password to send. */
    if (test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_PASSWORD)  &&  s->tx_info.password[0])
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending password '%s'\n", s->tx_info.password);
        send_20digit_msg_frame(s, T30_PWD, s->tx_info.password);
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_PASSWORD);
        return true;
    }
    clr_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_PASSWORD);
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_sub_frame(t30_state_t *s)
{
    /* Only send if there is a sub-address to send. */
    if (test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_SUBADDRESSING_CAPABLE)  &&  s->tx_info.sub_address[0])
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending sub-address '%s'\n", s->tx_info.sub_address);
        send_20digit_msg_frame(s, T30_SUB, s->tx_info.sub_address);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_SUBADDRESS_TRANSMISSION);
        return true;
    }
    clr_ctrl_bit(s->dcs_frame, T30_DCS_BIT_SUBADDRESS_TRANSMISSION);
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_tsa_frame(t30_state_t *s)
{
    if ((test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T37)  ||  test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T38))  &&  0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending transmitting subscriber internet address '%s'\n", "");
        return true;
    }
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_ira_frame(t30_state_t *s)
{
    if (test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_INTERNET_ROUTING_ADDRESS)  &&  0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending internet routing address '%s'\n", "");
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INTERNET_ROUTING_ADDRESS_TRANSMISSION);
        return true;
    }
    clr_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INTERNET_ROUTING_ADDRESS_TRANSMISSION);
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_cia_frame(t30_state_t *s)
{
    if ((test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T37)  ||  test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T38))  &&  0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending calling subscriber internet address '%s'\n", "");
        return true;
    }
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_isp_frame(t30_state_t *s)
{
    if (test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_INTERNET_SELECTIVE_POLLING_ADDRESS)  &&  0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending internet selective polling address '%s'\n", "");
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_INTERNET_SELECTIVE_POLLING_ADDRESS);
        return true;
    }
    clr_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_INTERNET_SELECTIVE_POLLING_ADDRESS);
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_csa_frame(t30_state_t *s)
{
#if 0
    if (("in T.37 mode"  ||  "in T.38 mode")  &&  0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending called subscriber internet address '%s'\n", "");
        return true;
    }
#endif
    return false;
}
/*- End of function --------------------------------------------------------*/

static int send_pps_frame(t30_state_t *s)
{
    uint8_t frame[7];

    frame[0] = ADDRESS_FIELD;
    frame[1] = CONTROL_FIELD_FINAL_FRAME;
    frame[2] = (uint8_t) (T30_PPS | s->dis_received);
    frame[3] = (s->ecm_at_page_end)  ?  ((uint8_t) (s->next_tx_step | s->dis_received))  :  T30_NULL;
    frame[4] = (uint8_t) (s->tx_page_number & 0xFF);
    frame[5] = (uint8_t) (s->ecm_block & 0xFF);
    frame[6] = (uint8_t) ((s->ecm_frames_this_tx_burst == 0)  ?  0  :  (s->ecm_frames_this_tx_burst - 1));
    span_log(&s->logging, SPAN_LOG_FLOW, "Sending PPS + %s\n", t30_frametype(frame[3]));
    send_frame(s, frame, 7);
    return frame[3] & 0xFE;
}
/*- End of function --------------------------------------------------------*/

int t30_build_dis_or_dtc(t30_state_t *s)
{
    int i;

    /* Build a skeleton for the DIS and DTC messages. This will be edited for
       the dynamically changing capabilities (e.g. can receive) just before
       it is sent. It might also be edited if the application changes our
       capabilities (e.g. disabling fine mode). Right now we set up all the
       unchanging stuff about what we are capable of doing. */
    s->local_dis_dtc_frame[0] = ADDRESS_FIELD;
    s->local_dis_dtc_frame[1] = CONTROL_FIELD_FINAL_FRAME;
    s->local_dis_dtc_frame[2] = (uint8_t) (T30_DIS | s->dis_received);
    for (i = 3;  i < T30_MAX_DIS_DTC_DCS_LEN;  i++)
        s->local_dis_dtc_frame[i] = 0x00;

    /* Always say 256 octets per ECM frame preferred, as 64 is never used in the
       real world. */
    if ((s->iaf & T30_IAF_MODE_T37))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T37);
    if ((s->iaf & T30_IAF_MODE_T38))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T38);
    /* No 3G mobile  */
    /* No V.8 */
    /* 256 octets preferred - don't bother making this optional, as everything uses 256 */
    /* Ready to transmit a fax (polling) will be determined separately, and this message edited. */
    /* Ready to receive a fax will be determined separately, and this message edited. */
    /* With no modems set we are actually selecting V.27ter fallback at 2400bps */
    if ((s->supported_modems & T30_SUPPORT_V27TER))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_MODEM_TYPE_2);
    if ((s->supported_modems & T30_SUPPORT_V29))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_MODEM_TYPE_1);
    /* V.17 is only valid when combined with V.29 and V.27ter, so if we enable V.17 we force the others too. */
    if ((s->supported_modems & T30_SUPPORT_V17))
        s->local_dis_dtc_frame[4] |= (DISBIT6 | DISBIT4 | DISBIT3);

    /* 215mm wide is always supported */
    if ((s->supported_image_sizes & T4_SUPPORT_WIDTH_303MM))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_215MM_255MM_303MM_WIDTH_CAPABLE);
    else if ((s->supported_image_sizes & T4_SUPPORT_WIDTH_255MM))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_215MM_255MM_WIDTH_CAPABLE);

    /* A4 is always supported. */
    if ((s->supported_image_sizes & T4_SUPPORT_LENGTH_UNLIMITED))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_UNLIMITED_LENGTH_CAPABLE);
    else if ((s->supported_image_sizes & T4_SUPPORT_LENGTH_B4))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_A4_B4_LENGTH_CAPABLE);
    if ((s->supported_image_sizes & T4_SUPPORT_LENGTH_US_LETTER))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_NORTH_AMERICAN_LETTER_CAPABLE);
    if ((s->supported_image_sizes & T4_SUPPORT_LENGTH_US_LEGAL))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_NORTH_AMERICAN_LEGAL_CAPABLE);

    /* No scan-line padding required, but some may be specified by the application. */
    set_ctrl_bits(s->local_dis_dtc_frame, s->local_min_scan_time_code, T30_DIS_BIT_MIN_SCAN_LINE_TIME_CAPABILITY_1);

    if ((s->supported_compressions & T4_COMPRESSION_T4_2D))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_2D_CAPABLE);
    if ((s->supported_compressions & T4_COMPRESSION_NONE))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_UNCOMPRESSED_CAPABLE);
    if (s->ecm_allowed)
    {
        /* ECM allowed */
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_ECM_CAPABLE);

        /* Only offer the option of fancy compression schemes, if we are
           also offering the ECM option needed to support them. */
        if ((s->supported_compressions & T4_COMPRESSION_T6))
            set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T6_CAPABLE);
        if ((s->supported_compressions & T4_COMPRESSION_T85))
        {
            set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T85_CAPABLE);
            /* Bit 79 set with bit 78 clear is invalid, so only check for L0
               support here. */
            if ((s->supported_compressions & T4_COMPRESSION_T85_L0))
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T85_L0_CAPABLE);
        }

        //if ((s->supported_compressions & T4_COMPRESSION_T88))
        //{
        //    set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T88_CAPABILITY_1);
        //    set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T88_CAPABILITY_2);
        //    set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T88_CAPABILITY_3);
        //}

        if ((s->supported_compressions & (T4_COMPRESSION_COLOUR | T4_COMPRESSION_GRAYSCALE)))
        {
            if ((s->supported_compressions & T4_COMPRESSION_COLOUR))
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_FULL_COLOUR_CAPABLE);

            if ((s->supported_compressions & T4_COMPRESSION_T42_T81))
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T81_CAPABLE);
            if ((s->supported_compressions & T4_COMPRESSION_T43))
            {
                /* Note 25 of table 2/T.30 */
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T81_CAPABLE);
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T43_CAPABLE);
                /* No plane interleave */
            }
            if ((s->supported_compressions & T4_COMPRESSION_T45))
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T45_CAPABLE);
            if ((s->supported_compressions & T4_COMPRESSION_SYCC_T81))
            {
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T81_CAPABLE);
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_SYCC_T81_CAPABLE);
            }

            if ((s->supported_compressions & T4_COMPRESSION_12BIT))
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_12BIT_CAPABLE);

            if ((s->supported_compressions & T4_COMPRESSION_NO_SUBSAMPLING))
                set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_NO_SUBSAMPLING);

            /* No custom illuminant */
            /* No custom gamut range */
        }
    }
    if ((s->supported_t30_features & T30_SUPPORT_FIELD_NOT_VALID))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_FNV_CAPABLE);
    if ((s->supported_t30_features & T30_SUPPORT_MULTIPLE_SELECTIVE_POLLING))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_MULTIPLE_SELECTIVE_POLLING_CAPABLE);
    if ((s->supported_t30_features & T30_SUPPORT_POLLED_SUB_ADDRESSING))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_POLLED_SUBADDRESSING_CAPABLE);
    if ((s->supported_t30_features & T30_SUPPORT_SELECTIVE_POLLING))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_SELECTIVE_POLLING_CAPABLE);
    if ((s->supported_t30_features & T30_SUPPORT_SUB_ADDRESSING))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_SUBADDRESSING_CAPABLE);
    if ((s->supported_t30_features & T30_SUPPORT_IDENTIFICATION))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_PASSWORD);

    /* No G.726 */
    /* No extended voice coding */
    /* Superfine minimum scan line time pattern follows fine */

    /* Ready to transmit a data file (polling) */
    if (s->tx_file[0])
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_READY_TO_TRANSMIT_DATA_FILE);

    /* No simple phase C BFT negotiations */
    /* No extended BFT negotiations */
    /* No Binary file transfer (BFT) */
    /* No Document transfer mode (DTM) */
    /* No Electronic data interchange (EDI) */
    /* No Basic transfer mode (BTM) */

    /* No mixed mode (polling) */
    /* No character mode */
    /* No mixed mode (T.4/Annex E) */
    /* No mode 26 (T.505) */
    /* No digital network capability */
    /* No duplex operation */

    /* No HKM key management */
    /* No RSA key management */
    /* No override */
    /* No HFX40 cipher */
    /* No alternative cipher number 2 */
    /* No alternative cipher number 3 */
    /* No HFX40-I hashing */
    /* No alternative hashing system number 2 */
    /* No alternative hashing system number 3 */

    /* No T.44 (mixed raster content) */
    /* No page length maximum strip size for T.44 (mixed raster content) */

    if ((s->supported_t30_features & T30_SUPPORT_INTERNET_SELECTIVE_POLLING_ADDRESS))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_INTERNET_SELECTIVE_POLLING_ADDRESS);
    if ((s->supported_t30_features & T30_SUPPORT_INTERNET_ROUTING_ADDRESS))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_INTERNET_ROUTING_ADDRESS);

    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_1200_1200))
    {
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_1200_1200_CAPABLE);
        if ((s->supported_colour_resolutions & T4_RESOLUTION_1200_1200))
            set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_1200_1200_CAPABLE);
    }
    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_600_1200))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_600_1200_CAPABLE);
    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_600_600))
    {
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_600_600_CAPABLE);
        if ((s->supported_colour_resolutions & T4_RESOLUTION_600_600))
            set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_600_600_CAPABLE);
    }
    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_400_800))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_400_800_CAPABLE);
    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_R16_SUPERFINE))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_400_400_CAPABLE);
    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_400_400))
    {
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_400_400_CAPABLE);
        if ((s->supported_colour_resolutions & T4_RESOLUTION_400_400))
            set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_300_300_400_400_CAPABLE);
    }
    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_300_600))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_300_600_CAPABLE);
    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_300_300))
    {
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_300_300_CAPABLE);
        if ((s->supported_colour_resolutions & T4_RESOLUTION_300_300))
            set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_300_300_400_400_CAPABLE);
    }
    if ((s->supported_bilevel_resolutions & (T4_RESOLUTION_200_400 | T4_RESOLUTION_R8_SUPERFINE)))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_200_400_CAPABLE);
    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_R8_FINE))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_200_200_CAPABLE);
    if ((s->supported_bilevel_resolutions & T4_RESOLUTION_200_200))
    {
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_200_200_CAPABLE);
        if ((s->supported_colour_resolutions & T4_RESOLUTION_200_200))
            set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_FULL_COLOUR_CAPABLE);
    }
    /* Standard FAX resolution bi-level image support goes without saying */
    if ((s->supported_colour_resolutions & T4_RESOLUTION_100_100))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_100_100_CAPABLE);

    if ((s->supported_bilevel_resolutions & (T4_RESOLUTION_R8_STANDARD | T4_RESOLUTION_R8_FINE | T4_RESOLUTION_R8_SUPERFINE | T4_RESOLUTION_R16_SUPERFINE)))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_METRIC_RESOLUTION_PREFERRED);
    if ((s->supported_bilevel_resolutions & (T4_RESOLUTION_200_100 | T4_RESOLUTION_200_200 | T4_RESOLUTION_200_400 | T4_RESOLUTION_300_300 | T4_RESOLUTION_300_600 | T4_RESOLUTION_400_400 | T4_RESOLUTION_400_800 | T4_RESOLUTION_600_600 | T4_RESOLUTION_600_1200 | T4_RESOLUTION_1200_1200)))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_INCH_RESOLUTION_PREFERRED);

    /* No double sided printing (alternate mode) */
    /* No double sided printing (continuous mode) */

    /* No black and white mixed raster content profile */
    /* No shared data memory */
    /* No T.44 colour space */

    if ((s->iaf & T30_IAF_MODE_FLOW_CONTROL))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T38_FLOW_CONTROL_CAPABLE);
    /* No k > 4 */
    if ((s->iaf & T30_IAF_MODE_CONTINUOUS_FLOW))
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_T38_FAX_CAPABLE);
    /* No T.88/T.89 profile */
    s->local_dis_dtc_len = 19;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int set_dis_or_dtc(t30_state_t *s)
{
    /* Whether we use a DIS or a DTC is determined by whether we have received a DIS.
       We just need to edit the prebuilt message. */
    s->local_dis_dtc_frame[2] = (uint8_t) (T30_DIS | s->dis_received);
    /* If we have a file name to receive into, then we are receive capable */
    if (s->rx_file[0])
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_READY_TO_RECEIVE_FAX_DOCUMENT);
    else
        clr_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_READY_TO_RECEIVE_FAX_DOCUMENT);
    /* If we have a file name to transmit, then we are ready to transmit (polling) */
    if (s->tx_file[0])
        set_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_READY_TO_TRANSMIT_FAX_DOCUMENT);
    else
        clr_ctrl_bit(s->local_dis_dtc_frame, T30_DIS_BIT_READY_TO_TRANSMIT_FAX_DOCUMENT);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int prune_dis_dtc(t30_state_t *s)
{
    int i;

    /* Find the last octet that is really needed, set the extension bits, and trim the message length */
    for (i = T30_MAX_DIS_DTC_DCS_LEN - 1;  i >= 6;  i--)
    {
        /* Strip the top bit */
        s->local_dis_dtc_frame[i] &= (DISBIT1 | DISBIT2 | DISBIT3 | DISBIT4 | DISBIT5 | DISBIT6 | DISBIT7);
        /* Check if there is some real message content here */
        if (s->local_dis_dtc_frame[i])
            break;
    }
    s->local_dis_dtc_len = i + 1;
    /* Fill in any required extension bits */
    s->local_dis_dtc_frame[i] &= ~DISBIT8;
    for (i--;  i > 4;  i--)
        s->local_dis_dtc_frame[i] |= DISBIT8;
    t30_decode_dis_dtc_dcs(s, s->local_dis_dtc_frame, s->local_dis_dtc_len);
    return s->local_dis_dtc_len;
}
/*- End of function --------------------------------------------------------*/

static int build_dcs(t30_state_t *s)
{
    int i;
    int use_bilevel;
    int image_type;

    /* Reacquire page information, in case the image was resized, flattened, etc. */
    s->current_page_resolution = t4_tx_get_tx_resolution(&s->t4.tx);
    s->x_resolution = t4_tx_get_tx_x_resolution(&s->t4.tx);
    s->y_resolution = t4_tx_get_tx_y_resolution(&s->t4.tx);
    s->image_width = t4_tx_get_tx_image_width(&s->t4.tx);
    image_type = t4_tx_get_tx_image_type(&s->t4.tx);

    /* Make a DCS frame based on local issues and the latest received DIS/DTC frame.
       Negotiate the result based on what both parties can do. */
    s->dcs_frame[0] = ADDRESS_FIELD;
    s->dcs_frame[1] = CONTROL_FIELD_FINAL_FRAME;
    s->dcs_frame[2] = (uint8_t) (T30_DCS | s->dis_received);
    for (i = 3;  i < T30_MAX_DIS_DTC_DCS_LEN;  i++)
        s->dcs_frame[i] = 0x00;

    /* We have a file to send, so tell the far end to go into receive mode. */
    set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_RECEIVE_FAX_DOCUMENT);

#if 0
    /* Check for T.37 simple mode. */
    if ((s->iaf & T30_IAF_MODE_T37)  &&  test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T37))
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_T37);
    /* Check for T.38 mode. */
    if ((s->iaf & T30_IAF_MODE_T38)  &&  test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T38))
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_T38);
#endif

    /* Set to required modem rate */
    s->dcs_frame[4] |= fallback_sequence[s->current_fallback].dcs_code;

    /* Select the compression to use. */
    use_bilevel = true;
    set_ctrl_bits(s->dcs_frame, s->min_scan_time_code, T30_DCS_BIT_MIN_SCAN_LINE_TIME_1);
    switch (s->line_compression)
    {
    case T4_COMPRESSION_T4_1D:
        /* There is nothing to set to select this encoding. */
        break;
    case T4_COMPRESSION_T4_2D:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_2D_MODE);
        break;
    case T4_COMPRESSION_T6:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_T6_MODE);
        break;
    case T4_COMPRESSION_T85:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_T85_MODE);
        break;
    case T4_COMPRESSION_T85_L0:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_T85_L0_MODE);
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        break;
#endif
    case T4_COMPRESSION_T42_T81:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_T81_MODE);
        if (image_type == T4_IMAGE_TYPE_COLOUR_8BIT  ||  image_type == T4_IMAGE_TYPE_COLOUR_12BIT)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_FULL_COLOUR_MODE);
        if (image_type == T4_IMAGE_TYPE_GRAY_12BIT  ||  image_type == T4_IMAGE_TYPE_COLOUR_12BIT)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_12BIT_COMPONENT);
        //if (???????? & T4_COMPRESSION_NO_SUBSAMPLING))
        //    set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_NO_SUBSAMPLING);
        //if (???????? & T4_COMPRESSION_?????))
        //    set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_PREFERRED_HUFFMAN_TABLES);
        set_ctrl_bits(s->dcs_frame, T30_MIN_SCAN_0MS, T30_DCS_BIT_MIN_SCAN_LINE_TIME_1);
        use_bilevel = false;
        break;
    case T4_COMPRESSION_T43:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_T43_MODE);
        if (image_type == T4_IMAGE_TYPE_COLOUR_8BIT  ||  image_type == T4_IMAGE_TYPE_COLOUR_12BIT)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_FULL_COLOUR_MODE);
        if (image_type == T4_IMAGE_TYPE_GRAY_12BIT  ||  image_type == T4_IMAGE_TYPE_COLOUR_12BIT)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_12BIT_COMPONENT);
        set_ctrl_bits(s->dcs_frame, T30_MIN_SCAN_0MS, T30_DCS_BIT_MIN_SCAN_LINE_TIME_1);
        use_bilevel = false;
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        use_bilevel = false;
        break;
#endif
#if defined(SPANDSP_SUPPORT_SYCC_T81)
    case T4_COMPRESSION_SYCC_T81:
        use_bilevel = false;
        break;
#endif
    default:
        set_ctrl_bits(s->dcs_frame, T30_MIN_SCAN_0MS, T30_DCS_BIT_MIN_SCAN_LINE_TIME_1);
        break;
    }

    /* Set the image width */
    switch (s->line_width_code)
    {
    case T4_SUPPORT_WIDTH_215MM:
        span_log(&s->logging, SPAN_LOG_FLOW, "Image width is A4 at %ddpm x %ddpm\n", s->x_resolution, s->y_resolution);
        /* No width related bits need to be set. */
        break;
    case T4_SUPPORT_WIDTH_255MM:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_255MM_WIDTH);
        span_log(&s->logging, SPAN_LOG_FLOW, "Image width is B4 at %ddpm x %ddpm\n", s->x_resolution, s->y_resolution);
        break;
    case T4_SUPPORT_WIDTH_303MM:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_303MM_WIDTH);
        span_log(&s->logging, SPAN_LOG_FLOW, "Image width is A3 at %ddpm x %ddpm\n", s->x_resolution, s->y_resolution);
        break;
    }

    /* Set the image length */
    /* If the other end supports unlimited length, then use that. Otherwise, if the other end supports
       B4 use that, as its longer than the default A4 length. */
    if ((s->mutual_image_sizes & T4_SUPPORT_LENGTH_UNLIMITED))
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_UNLIMITED_LENGTH);
    else if ((s->mutual_image_sizes & T4_SUPPORT_LENGTH_B4))
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_B4_LENGTH);
    else if ((s->mutual_image_sizes & T4_SUPPORT_LENGTH_US_LETTER))
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_NORTH_AMERICAN_LETTER);
    else if ((s->mutual_image_sizes & T4_SUPPORT_LENGTH_US_LEGAL))
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_NORTH_AMERICAN_LEGAL);

    /* Set the Y resolution bits */
    switch (s->current_page_resolution)
    {
    case T4_RESOLUTION_1200_1200:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_1200_1200);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        if (!use_bilevel)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_COLOUR_GRAY_1200_1200);
        break;
    case T4_RESOLUTION_600_1200:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_600_1200);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        break;
    case T4_RESOLUTION_600_600:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_600_600);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        if (!use_bilevel)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_COLOUR_GRAY_600_600);
        break;
    case T4_RESOLUTION_400_800:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_400_800);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        break;
    case T4_RESOLUTION_400_400:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_400_400);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        if (!use_bilevel)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_COLOUR_GRAY_300_300_400_400);
        break;
    case T4_RESOLUTION_300_600:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_300_600);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        break;
    case T4_RESOLUTION_300_300:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_300_300);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        if (!use_bilevel)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_COLOUR_GRAY_300_300_400_400);
        break;
    case T4_RESOLUTION_200_400:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_200_400);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        break;
    case T4_RESOLUTION_200_200:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_200_200);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        if (!use_bilevel)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_FULL_COLOUR_MODE);
        break;
    case T4_RESOLUTION_200_100:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        break;
    case T4_RESOLUTION_100_100:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_INCH_RESOLUTION);
        if (!use_bilevel)
            set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_COLOUR_GRAY_100_100);
        break;
    case T4_RESOLUTION_R16_SUPERFINE:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_400_400);
        break;
    case T4_RESOLUTION_R8_SUPERFINE:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_200_400);
        break;
    case T4_RESOLUTION_R8_FINE:
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_200_200);
        break;
    case T4_RESOLUTION_R8_STANDARD:
        /* Nothing special to set */
        break;
    }

    if (s->error_correcting_mode)
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_ECM_MODE);

    if ((s->iaf & T30_IAF_MODE_FLOW_CONTROL)  &&  test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T38_FLOW_CONTROL_CAPABLE))
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_T38_FLOW_CONTROL_CAPABLE);

    if ((s->iaf & T30_IAF_MODE_CONTINUOUS_FLOW)  &&  test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T38_FAX_CAPABLE))
    {
        /* Clear the modem type bits, in accordance with note 77 of Table 2/T.30 */
        clr_ctrl_bit(s->local_dis_dtc_frame, T30_DCS_BIT_MODEM_TYPE_1);
        clr_ctrl_bit(s->local_dis_dtc_frame, T30_DCS_BIT_MODEM_TYPE_2);
        clr_ctrl_bit(s->local_dis_dtc_frame, T30_DCS_BIT_MODEM_TYPE_3);
        clr_ctrl_bit(s->local_dis_dtc_frame, T30_DCS_BIT_MODEM_TYPE_4);
        set_ctrl_bit(s->dcs_frame, T30_DCS_BIT_T38_FAX_MODE);
    }
    s->dcs_len = 19;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int prune_dcs(t30_state_t *s)
{
    int i;

    /* Find the last octet that is really needed, set the extension bits, and trim the message length */
    for (i = T30_MAX_DIS_DTC_DCS_LEN - 1;  i >= 6;  i--)
    {
        /* Strip the top bit */
        s->dcs_frame[i] &= (DISBIT1 | DISBIT2 | DISBIT3 | DISBIT4 | DISBIT5 | DISBIT6 | DISBIT7);
        /* Check if there is some real message content here */
        if (s->dcs_frame[i])
            break;
    }
    s->dcs_len = i + 1;
    /* Fill in any required extension bits */
    s->local_dis_dtc_frame[i] &= ~DISBIT8;
    for (i--  ;  i > 4;  i--)
        s->dcs_frame[i] |= DISBIT8;
    t30_decode_dis_dtc_dcs(s, s->dcs_frame, s->dcs_len);
    return s->dcs_len;
}
/*- End of function --------------------------------------------------------*/

static int analyze_rx_dis_dtc(t30_state_t *s, const uint8_t *msg, int len)
{
    t30_decode_dis_dtc_dcs(s, msg, len);
    if (len < 6)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Short DIS/DTC frame\n");
        return -1;
    }

    if (msg[2] == T30_DIS)
        s->dis_received = true;

    /* Make a local copy of the message, padded to the maximum possible length with zeros. This allows
       us to simply pick out the bits, without worrying about whether they were set from the remote side. */
    if (len > T30_MAX_DIS_DTC_DCS_LEN)
        len = T30_MAX_DIS_DTC_DCS_LEN;
    memcpy(s->far_dis_dtc_frame, msg, len);
    if (len < T30_MAX_DIS_DTC_DCS_LEN)
        memset(s->far_dis_dtc_frame + len, 0, T30_MAX_DIS_DTC_DCS_LEN - len);

    s->error_correcting_mode = (s->ecm_allowed  &&  test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_ECM_CAPABLE));
    /* Always use 256 octets per ECM frame, whatever the other end says it is capable of */
    s->octets_per_ecm_frame = 256;

    /* Now we know if we are going to use ECM, select the compressions which we can use. */
    s->mutual_compressions = s->supported_compressions;
    if (!s->error_correcting_mode)
    {
        /* Remove any compression schemes which need error correction to work. */
        s->mutual_compressions &= (0xFF800000 | T4_COMPRESSION_NONE | T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D);
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_2D_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_T4_2D;
    }
    else
    {
        /* Check the bi-level capabilities */
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_2D_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_T4_2D;
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T6_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_T6;
        /* T.85 L0 capable without T.85 capable is an invalid combination, so let
           just zap both capabilities if the far end is not T.85 capable. */
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T85_CAPABLE))
            s->mutual_compressions &= ~(T4_COMPRESSION_T85 | T4_COMPRESSION_T85_L0);
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T85_L0_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_T85_L0;

        /* Check for full colour or only gray-scale from the multi-level codecs */
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_FULL_COLOUR_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_COLOUR;

        /* Check the colour capabilities */
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T81_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_T42_T81;
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_SYCC_T81_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_SYCC_T81;
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T43_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_T43;
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_T45_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_T45;

        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_12BIT_CAPABLE))
            s->mutual_compressions &= ~T4_COMPRESSION_12BIT;
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_NO_SUBSAMPLING))
            s->mutual_compressions &= ~T4_COMPRESSION_NO_SUBSAMPLING;

        /* bit74 custom illuminant */
        /* bit75 custom gamut range */
    }

    s->mutual_bilevel_resolutions = s->supported_bilevel_resolutions;
    s->mutual_colour_resolutions = s->supported_colour_resolutions;
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_1200_1200_CAPABLE))
    {
        s->mutual_bilevel_resolutions &= ~T4_RESOLUTION_1200_1200;
        s->mutual_colour_resolutions &= ~T4_RESOLUTION_1200_1200;
    }
    else
    {
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_1200_1200_CAPABLE))
            s->mutual_colour_resolutions &= ~T4_RESOLUTION_1200_1200;
    }
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_600_1200_CAPABLE))
        s->mutual_bilevel_resolutions &= ~T4_RESOLUTION_600_1200;
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_600_600_CAPABLE))
    {
        s->mutual_bilevel_resolutions &= ~T4_RESOLUTION_600_600;
        s->mutual_colour_resolutions &= ~T4_RESOLUTION_600_600;
    }
    else
    {
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_600_600_CAPABLE))
            s->mutual_colour_resolutions &= ~T4_RESOLUTION_600_600;
    }
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_400_800_CAPABLE))
        s->mutual_bilevel_resolutions &= ~T4_RESOLUTION_400_800;
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_400_400_CAPABLE))
    {
        s->mutual_bilevel_resolutions &= ~(T4_RESOLUTION_400_400 | T4_RESOLUTION_R16_SUPERFINE);
        s->mutual_colour_resolutions &= ~T4_RESOLUTION_400_400;
    }
    else
    {
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_300_300_400_400_CAPABLE))
            s->mutual_colour_resolutions &= ~T4_RESOLUTION_400_400;
    }
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_300_600_CAPABLE))
        s->mutual_bilevel_resolutions &= ~T4_RESOLUTION_300_600;
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_300_300_CAPABLE))
    {
        s->mutual_bilevel_resolutions &= ~T4_RESOLUTION_300_300;
        s->mutual_colour_resolutions &= ~T4_RESOLUTION_300_300;
    }
    else
    {
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_300_300_400_400_CAPABLE))
            s->mutual_colour_resolutions &= ~T4_RESOLUTION_300_300;
    }
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_200_400_CAPABLE))
        s->mutual_bilevel_resolutions &= ~(T4_RESOLUTION_200_400 | T4_RESOLUTION_R8_SUPERFINE);
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_200_200_CAPABLE))
    {
        s->mutual_bilevel_resolutions &= ~(T4_RESOLUTION_200_200 | T4_RESOLUTION_R8_FINE);
        s->mutual_colour_resolutions &= ~T4_RESOLUTION_200_200;
    }
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_INCH_RESOLUTION_PREFERRED))
        s->mutual_bilevel_resolutions &= ~T4_RESOLUTION_200_100;
    /* Never suppress T4_RESOLUTION_R8_STANDARD */
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_COLOUR_GRAY_100_100_CAPABLE))
        s->mutual_colour_resolutions &= ~T4_RESOLUTION_100_100;

    s->mutual_image_sizes = s->supported_image_sizes;
    /* 215mm wide is always supported */
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_215MM_255MM_303MM_WIDTH_CAPABLE))
    {
        s->mutual_image_sizes &= ~T4_SUPPORT_WIDTH_303MM;
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_215MM_255MM_WIDTH_CAPABLE))
            s->mutual_image_sizes &= ~T4_SUPPORT_WIDTH_255MM;
    }
    /* A4 is always supported. */
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_UNLIMITED_LENGTH_CAPABLE))
    {
        s->mutual_image_sizes &= ~T4_SUPPORT_LENGTH_UNLIMITED;
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_A4_B4_LENGTH_CAPABLE))
            s->mutual_image_sizes &= ~T4_SUPPORT_LENGTH_B4;
    }
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_NORTH_AMERICAN_LETTER_CAPABLE))
        s->mutual_image_sizes &= ~T4_SUPPORT_LENGTH_US_LETTER;
    if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_NORTH_AMERICAN_LEGAL_CAPABLE))
        s->mutual_image_sizes &= ~T4_SUPPORT_LENGTH_US_LEGAL;

    switch (s->far_dis_dtc_frame[4] & (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3))
    {
    case (DISBIT6 | DISBIT4 | DISBIT3):
        if ((s->supported_modems & T30_SUPPORT_V17))
        {
            s->current_permitted_modems = T30_SUPPORT_V17 | T30_SUPPORT_V29 | T30_SUPPORT_V27TER;
            s->current_fallback = T30_V17_FALLBACK_START;
            break;
        }
        /* Fall through */
    case (DISBIT4 | DISBIT3):
        if ((s->supported_modems & T30_SUPPORT_V29))
        {
            s->current_permitted_modems = T30_SUPPORT_V29 | T30_SUPPORT_V27TER;
            s->current_fallback = T30_V29_FALLBACK_START;
            break;
        }
        /* Fall through */
    case DISBIT4:
        s->current_permitted_modems = T30_SUPPORT_V27TER;
        s->current_fallback = T30_V27TER_FALLBACK_START;
        break;
    case 0:
        s->current_permitted_modems = T30_SUPPORT_V27TER;
        s->current_fallback = T30_V27TER_FALLBACK_START + 1;
        break;
    case DISBIT3:
        if ((s->supported_modems & T30_SUPPORT_V29))
        {
            /* TODO: this doesn't allow for skipping the V.27ter modes */
            s->current_permitted_modems = T30_SUPPORT_V29;
            s->current_fallback = T30_V29_FALLBACK_START;
            break;
        }
        /* Fall through */
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Remote does not support a compatible modem\n");
        /* We cannot talk to this machine! */
        t30_set_status(s, T30_ERR_INCOMPATIBLE);
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int analyze_rx_dcs(t30_state_t *s, const uint8_t *msg, int len)
{
    /* The following treats a width field of 11 like 10, which does what note 6 of Table 2/T.30
       says we should do with the invalid value 11. */
    static const int widths[6][4] =
    {
        { T4_WIDTH_100_A4,  T4_WIDTH_100_B4,  T4_WIDTH_100_A3,  T4_WIDTH_100_A3}, /* 100/inch */
        { T4_WIDTH_200_A4,  T4_WIDTH_200_B4,  T4_WIDTH_200_A3,  T4_WIDTH_200_A3}, /* 200/inch / R8 resolution */
        { T4_WIDTH_300_A4,  T4_WIDTH_300_B4,  T4_WIDTH_300_A3,  T4_WIDTH_300_A3}, /* 300/inch resolution */
        { T4_WIDTH_400_A4,  T4_WIDTH_400_B4,  T4_WIDTH_400_A3,  T4_WIDTH_400_A3}, /* 400/inch / R16 resolution */
        { T4_WIDTH_600_A4,  T4_WIDTH_600_B4,  T4_WIDTH_600_A3,  T4_WIDTH_600_A3}, /* 600/inch resolution */
        {T4_WIDTH_1200_A4, T4_WIDTH_1200_B4, T4_WIDTH_1200_A3, T4_WIDTH_1200_A3}  /* 1200/inch resolution */
    };
    uint8_t dcs_frame[T30_MAX_DIS_DTC_DCS_LEN];
    int i;
    int x;

    t30_decode_dis_dtc_dcs(s, msg, len);
    if (len < 6)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Short DCS frame\n");
        return -1;
    }

    /* Make an ASCII string format copy of the message, for logging in the
       received file. This string does not include the frame header octets. */
    sprintf(s->rx_dcs_string, "%02X", bit_reverse8(msg[3]));
    for (i = 4;  i < len;  i++)
        sprintf(s->rx_dcs_string + 3*i - 10, " %02X", bit_reverse8(msg[i]));

    /* Make a local copy of the message, padded to the maximum possible length with zeros. This allows
       us to simply pick out the bits, without worrying about whether they were set from the remote side. */
    if (len > T30_MAX_DIS_DTC_DCS_LEN)
        len = T30_MAX_DIS_DTC_DCS_LEN;
    memcpy(dcs_frame, msg, len);
    if (len < T30_MAX_DIS_DTC_DCS_LEN)
        memset(dcs_frame + len, 0, T30_MAX_DIS_DTC_DCS_LEN - len);

    s->error_correcting_mode = (test_ctrl_bit(dcs_frame, T30_DCS_BIT_ECM_MODE) != 0);
    s->octets_per_ecm_frame = test_ctrl_bit(dcs_frame, T30_DCS_BIT_64_OCTET_ECM_FRAMES)  ?  256  :  64;

    s->x_resolution = -1;
    s->y_resolution = -1;
    s->current_page_resolution = 0;
    s->line_compression = -1;
    x = -1;
    if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_T81_MODE)
        ||
        test_ctrl_bit(dcs_frame, T30_DCS_BIT_T43_MODE)
        ||
        test_ctrl_bit(dcs_frame, T30_DCS_BIT_T45_MODE)
        ||
        test_ctrl_bit(dcs_frame, T30_DCS_BIT_SYCC_T81_MODE))
    {
        /* Gray scale or colour image */

        /* Note 35 of Table 2/T.30 */
        if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_FULL_COLOUR_MODE))
        {
            if ((s->supported_colour_resolutions & T4_COMPRESSION_COLOUR))
            {
                /* We are going to work in full colour mode */
            }
        }

        if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_12BIT_COMPONENT))
        {
            if ((s->supported_colour_resolutions & T4_COMPRESSION_12BIT))
            {
                /* We are going to work in 12 bit mode */
            }
        }

        if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_NO_SUBSAMPLING))
        {
            //???? = T4_COMPRESSION_NO_SUBSAMPLING;
        }

        if (!test_ctrl_bit(dcs_frame, T30_DCS_BIT_PREFERRED_HUFFMAN_TABLES))
        {
            //???? = T4_COMPRESSION_T42_T81_HUFFMAN;
        }

        if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_COLOUR_GRAY_1200_1200))
        {
            if ((s->supported_colour_resolutions & T4_RESOLUTION_1200_1200))
            {
                s->x_resolution = T4_X_RESOLUTION_1200;
                s->y_resolution = T4_Y_RESOLUTION_1200;
                s->current_page_resolution = T4_RESOLUTION_1200_1200;
                x = 5;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_COLOUR_GRAY_600_600))
        {
            if ((s->supported_colour_resolutions & T4_RESOLUTION_600_600))
            {
                s->x_resolution = T4_X_RESOLUTION_600;
                s->y_resolution = T4_Y_RESOLUTION_600;
                s->current_page_resolution = T4_RESOLUTION_600_600;
                x = 4;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_400_400))
        {
            if ((s->supported_colour_resolutions & T4_RESOLUTION_400_400))
            {
                s->x_resolution = T4_X_RESOLUTION_400;
                s->y_resolution = T4_Y_RESOLUTION_400;
                s->current_page_resolution = T4_RESOLUTION_400_400;
                x = 3;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_300_300))
        {
            if ((s->supported_colour_resolutions & T4_RESOLUTION_300_300))
            {
                s->x_resolution = T4_X_RESOLUTION_300;
                s->y_resolution = T4_Y_RESOLUTION_300;
                s->current_page_resolution = T4_RESOLUTION_300_300;
                x = 2;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_200_200))
        {
            if ((s->supported_colour_resolutions & T4_RESOLUTION_200_200))
            {
                s->x_resolution = T4_X_RESOLUTION_200;
                s->y_resolution = T4_Y_RESOLUTION_200;
                s->current_page_resolution = T4_RESOLUTION_200_200;
                x = 1;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_COLOUR_GRAY_100_100))
        {
            if ((s->supported_colour_resolutions & T4_RESOLUTION_100_100))
            {
                s->x_resolution = T4_X_RESOLUTION_100;
                s->y_resolution = T4_Y_RESOLUTION_100;
                s->current_page_resolution = T4_RESOLUTION_100_100;
                x = 0;
            }
        }

        /* Check which compression the far end has decided to use. */
        if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_T81_MODE))
        {
            if ((s->supported_compressions & T4_COMPRESSION_T42_T81))
                s->line_compression = T4_COMPRESSION_T42_T81;
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_T43_MODE))
        {
            if ((s->supported_compressions & T4_COMPRESSION_T43))
                s->line_compression = T4_COMPRESSION_T43;
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_T45_MODE))
        {
            if ((s->supported_compressions & T4_COMPRESSION_T45))
                s->line_compression = T4_COMPRESSION_T45;
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_SYCC_T81_MODE))
        {
            if ((s->supported_compressions & T4_COMPRESSION_SYCC_T81))
                s->line_compression = T4_COMPRESSION_SYCC_T81;
        }
    }
    else
    {
        /* Bi-level image */
        if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_1200_1200))
        {
            if ((s->supported_bilevel_resolutions & T4_RESOLUTION_1200_1200))
            {
                s->x_resolution = T4_X_RESOLUTION_1200;
                s->y_resolution = T4_Y_RESOLUTION_1200;
                s->current_page_resolution = T4_RESOLUTION_1200_1200;
                x = 5;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_600_1200))
        {
            if ((s->supported_bilevel_resolutions & T4_RESOLUTION_600_1200))
            {
                s->x_resolution = T4_X_RESOLUTION_600;
                s->y_resolution = T4_Y_RESOLUTION_1200;
                s->current_page_resolution = T4_RESOLUTION_600_1200;
                x = 4;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_600_600))
        {
            if ((s->supported_bilevel_resolutions & T4_RESOLUTION_600_600))
            {
                s->x_resolution = T4_X_RESOLUTION_600;
                s->y_resolution = T4_Y_RESOLUTION_600;
                s->current_page_resolution = T4_RESOLUTION_600_600;
                x = 4;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_400_800))
        {
            if ((s->supported_bilevel_resolutions & T4_RESOLUTION_400_800))
            {
                s->x_resolution = T4_X_RESOLUTION_400;
                s->y_resolution = T4_Y_RESOLUTION_800;
                s->current_page_resolution = T4_RESOLUTION_400_800;
                x = 3;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_400_400))
        {
            if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_INCH_RESOLUTION))
            {
                if ((s->supported_bilevel_resolutions & T4_RESOLUTION_400_400))
                {
                    s->x_resolution = T4_X_RESOLUTION_400;
                    s->y_resolution = T4_Y_RESOLUTION_400;
                    s->current_page_resolution = T4_RESOLUTION_400_400;
                    x = 3;
                }
            }
            else
            {
                if ((s->supported_bilevel_resolutions & T4_RESOLUTION_R16_SUPERFINE))
                {
                    s->x_resolution = T4_X_RESOLUTION_R16;
                    s->y_resolution = T4_Y_RESOLUTION_SUPERFINE;
                    s->current_page_resolution = T4_RESOLUTION_R16_SUPERFINE;
                    x = 3;
                }
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_300_600))
        {
            if ((s->supported_bilevel_resolutions & T4_RESOLUTION_300_600))
            {
                s->x_resolution = T4_X_RESOLUTION_300;
                s->y_resolution = T4_Y_RESOLUTION_600;
                s->current_page_resolution = T4_RESOLUTION_300_600;
                x = 2;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_300_300))
        {
            if ((s->supported_bilevel_resolutions & T4_RESOLUTION_300_300))
            {
                s->x_resolution = T4_X_RESOLUTION_300;
                s->y_resolution = T4_Y_RESOLUTION_300;
                s->current_page_resolution = T4_RESOLUTION_300_300;
                x = 2;
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_200_400))
        {
            if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_INCH_RESOLUTION))
            {
                if ((s->supported_bilevel_resolutions & T4_RESOLUTION_200_400))
                {
                    s->x_resolution = T4_X_RESOLUTION_200;
                    s->y_resolution = T4_Y_RESOLUTION_400;
                    s->current_page_resolution = T4_RESOLUTION_200_400;
                    x = 1;
                }
            }
            else
            {
                if ((s->supported_bilevel_resolutions & T4_RESOLUTION_R8_SUPERFINE))
                {
                    s->x_resolution = T4_X_RESOLUTION_R8;
                    s->y_resolution = T4_Y_RESOLUTION_SUPERFINE;
                    s->current_page_resolution = T4_RESOLUTION_R8_SUPERFINE;
                    x = 1;
                }
            }
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_200_200))
        {
            if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_INCH_RESOLUTION))
            {
                if ((s->supported_bilevel_resolutions & T4_RESOLUTION_200_200))
                {
                    s->x_resolution = T4_X_RESOLUTION_200;
                    s->y_resolution = T4_Y_RESOLUTION_200;
                    s->current_page_resolution = T4_RESOLUTION_200_200;
                    x = 1;
                }
            }
            else
            {
                if ((s->supported_bilevel_resolutions & T4_RESOLUTION_R8_FINE))
                {
                    s->x_resolution = T4_X_RESOLUTION_R8;
                    s->y_resolution = T4_Y_RESOLUTION_FINE;
                    s->current_page_resolution = T4_RESOLUTION_R8_FINE;
                    x = 1;
                }
            }
        }
        else
        {
            if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_INCH_RESOLUTION))
            {
                s->x_resolution = T4_X_RESOLUTION_200;
                s->y_resolution = T4_Y_RESOLUTION_100;
                s->current_page_resolution = T4_RESOLUTION_200_100;
                x = 1;
            }
            else
            {
                s->x_resolution = T4_X_RESOLUTION_R8;
                s->y_resolution = T4_Y_RESOLUTION_STANDARD;
                s->current_page_resolution = T4_RESOLUTION_R8_STANDARD;
                x = 1;
            }
        }

        /* Check which compression the far end has decided to use. */
        if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_T88_MODE_1)
            ||
            test_ctrl_bit(dcs_frame, T30_DCS_BIT_T88_MODE_2)
            ||
            test_ctrl_bit(dcs_frame, T30_DCS_BIT_T88_MODE_3))
        {
            if ((s->supported_compressions & T4_COMPRESSION_T88))
                s->line_compression = T4_COMPRESSION_T88;
        }
        if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_T85_L0_MODE))
        {
            if ((s->supported_compressions & T4_COMPRESSION_T85_L0))
                s->line_compression = T4_COMPRESSION_T85_L0;
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_T85_MODE))
        {
            if ((s->supported_compressions & T4_COMPRESSION_T85))
                s->line_compression = T4_COMPRESSION_T85;
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_T6_MODE))
        {
            if ((s->supported_compressions & T4_COMPRESSION_T6))
                s->line_compression = T4_COMPRESSION_T6;
        }
        else if (test_ctrl_bit(dcs_frame, T30_DCS_BIT_2D_MODE))
        {
            if ((s->supported_compressions & T4_COMPRESSION_T4_2D))
                s->line_compression = T4_COMPRESSION_T4_2D;
        }
        else
        {
            if ((s->supported_compressions & T4_COMPRESSION_T4_1D))
                s->line_compression = T4_COMPRESSION_T4_1D;
        }
    }

    if (s->line_compression == -1)
    {
        t30_set_status(s, T30_ERR_INCOMPATIBLE);
        return -1;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "Far end selected compression %s (%d)\n", t4_compression_to_str(s->line_compression), s->line_compression);

    if (x < 0)
    {
        t30_set_status(s, T30_ERR_NORESSUPPORT);
        return -1;
    }

    s->image_width = widths[x][dcs_frame[5] & (DISBIT2 | DISBIT1)];
    /* We don't care that much about the image length control bits. Just accept what arrives */

    if (!test_ctrl_bit(dcs_frame, T30_DCS_BIT_RECEIVE_FAX_DOCUMENT))
        span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Remote is not requesting receive in DCS\n");

    if ((s->current_fallback = find_fallback_entry(dcs_frame[4] & (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3))) < 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Remote asked for a modem standard we do not support\n");
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void send_dcn(t30_state_t *s)
{
    queue_phase(s, T30_PHASE_D_TX);
    set_state(s, T30_STATE_C);
    send_simple_frame(s, T30_DCN);
}
/*- End of function --------------------------------------------------------*/

static void return_to_phase_b(t30_state_t *s, int with_fallback)
{
    /* This is what we do after things like T30_EOM is exchanged. */
    if (s->calling_party)
        set_state(s, T30_STATE_T);
    else
        set_state(s, T30_STATE_R);
}
/*- End of function --------------------------------------------------------*/

static int send_dis_or_dtc_sequence(t30_state_t *s, int start)
{
    /* (NSF) (CSI) DIS */
    /* (NSC) (CIG) (PWD) (SEP) (PSA) (CIA) (ISP) DTC */
    if (start)
    {
        set_dis_or_dtc(s);
        set_state(s, T30_STATE_R);
        s->step = 0;
    }
    if (!s->dis_received)
    {
        /* DIS sequence */
        switch (s->step)
        {
        case 0:
            s->step++;
            if (send_nsf_frame(s))
                break;
            /* Fall through */
        case 1:
            s->step++;
            if (send_ident_frame(s, T30_CSI))
                break;
            /* Fall through */
        case 2:
            s->step++;
            prune_dis_dtc(s);
            send_frame(s, s->local_dis_dtc_frame, s->local_dis_dtc_len);
            break;
        case 3:
            s->step++;
            shut_down_hdlc_tx(s);
            break;
        default:
            return -1;
        }
    }
    else
    {
        /* DTC sequence */
        switch (s->step)
        {
        case 0:
            s->step++;
            if (send_nsc_frame(s))
                break;
            /* Fall through */
        case 1:
            s->step++;
            if (send_ident_frame(s, T30_CIG))
                break;
            /* Fall through */
        case 2:
            s->step++;
            if (send_pwd_frame(s))
                break;
            /* Fall through */
        case 3:
            s->step++;
            if (send_sep_frame(s))
                break;
            /* Fall through */
        case 4:
            s->step++;
            if (send_psa_frame(s))
                break;
            /* Fall through */
        case 5:
            s->step++;
            if (send_cia_frame(s))
                break;
            /* Fall through */
        case 6:
            s->step++;
            if (send_isp_frame(s))
                break;
            /* Fall through */
        case 7:
            s->step++;
            prune_dis_dtc(s);
            send_frame(s, s->local_dis_dtc_frame, s->local_dis_dtc_len);
            break;
        case 8:
            s->step++;
            shut_down_hdlc_tx(s);
            break;
        default:
            return -1;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int send_dcs_sequence(t30_state_t *s, int start)
{
    /* (NSS) (TSI) (SUB) (SID) (TSA) (IRA) DCS */
    /* Schedule training after the messages */
    if (start)
    {
        set_state(s, T30_STATE_D);
        s->step = 0;
    }
    switch (s->step)
    {
    case 0:
        s->step++;
        if (send_nss_frame(s))
            break;
        /* Fall through */
    case 1:
        s->step++;
        if (send_ident_frame(s, T30_TSI))
            break;
        /* Fall through */
    case 2:
        s->step++;
        if (send_sub_frame(s))
            break;
        /* Fall through */
    case 3:
        s->step++;
        if (send_sid_frame(s))
            break;
        /* Fall through */
    case 4:
        s->step++;
        if (send_tsa_frame(s))
            break;
        /* Fall through */
    case 5:
        s->step++;
        if (send_ira_frame(s))
            break;
        /* Fall through */
    case 6:
        s->step++;
        prune_dcs(s);
        send_frame(s, s->dcs_frame, s->dcs_len);
        break;
    case 7:
        s->step++;
        shut_down_hdlc_tx(s);
        break;
    default:
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int send_cfr_sequence(t30_state_t *s, int start)
{
    /* (CSA) CFR */
    /* CFR is usually a simple frame, but can become a sequence with Internet
       FAXing. */
    if (start)
    {
        s->step = 0;
    }
    switch (s->step)
    {
    case 0:
        s->step++;
        if (send_csa_frame(s))
            break;
        /* Fall through */
    case 1:
        s->step++;
        send_simple_frame(s, T30_CFR);
        break;
    case 2:
        s->step++;
        shut_down_hdlc_tx(s);
        break;
    default:
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void disconnect(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Disconnecting\n");
    /* Make sure any FAX in progress is tidied up. If the tidying up has
       already happened, repeating it here is harmless. */
    terminate_operation_in_progress(s);
    s->timer_t0_t1 = 0;
    s->timer_t2_t4 = 0;
    s->timer_t3 = 0;
    s->timer_t5 = 0;
    set_phase(s, T30_PHASE_E);
    set_state(s, T30_STATE_B);
}
/*- End of function --------------------------------------------------------*/

static void set_min_scan_time(t30_state_t *s)
{
    /* Translation between the codes for the minimum scan times the other end needs,
       and the codes for what we say will be used. We need 0 minimum. */
    static const uint8_t translate_min_scan_time[3][8] =
    {
        {T30_MIN_SCAN_20MS, T30_MIN_SCAN_5MS, T30_MIN_SCAN_10MS, T30_MIN_SCAN_20MS, T30_MIN_SCAN_40MS, T30_MIN_SCAN_40MS, T30_MIN_SCAN_10MS, T30_MIN_SCAN_0MS}, /* normal */
        {T30_MIN_SCAN_20MS, T30_MIN_SCAN_5MS, T30_MIN_SCAN_10MS, T30_MIN_SCAN_10MS, T30_MIN_SCAN_40MS, T30_MIN_SCAN_20MS, T30_MIN_SCAN_5MS,  T30_MIN_SCAN_0MS}, /* fine */
        {T30_MIN_SCAN_10MS, T30_MIN_SCAN_5MS, T30_MIN_SCAN_5MS,  T30_MIN_SCAN_5MS,  T30_MIN_SCAN_20MS, T30_MIN_SCAN_10MS, T30_MIN_SCAN_5MS,  T30_MIN_SCAN_0MS}  /* superfine, when half fine time is selected */
    };
    /* Translation between the codes for the minimum scan time we will use, and milliseconds. */
    static const int min_scan_times[8] =
    {
        20, 5, 10, 0, 40, 0, 0, 0
    };
    int min_bits_field;
    int min_row_bits;

    /* Set the minimum scan time bits */
    if (s->error_correcting_mode)
        min_bits_field = T30_MIN_SCAN_0MS;
    else
        min_bits_field = (s->far_dis_dtc_frame[5] >> 4) & 7;
    switch (s->y_resolution)
    {
    case T4_Y_RESOLUTION_SUPERFINE:
    case T4_Y_RESOLUTION_400:
        s->min_scan_time_code = translate_min_scan_time[(test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_MIN_SCAN_TIME_HALVES))  ?  2  :  1][min_bits_field];
        break;
    case T4_Y_RESOLUTION_FINE:
    case T4_Y_RESOLUTION_200:
        s->min_scan_time_code = translate_min_scan_time[1][min_bits_field];
        break;
    case T4_Y_RESOLUTION_STANDARD:
    case T4_Y_RESOLUTION_100:
        s->min_scan_time_code = translate_min_scan_time[0][min_bits_field];
        break;
    default:
        s->min_scan_time_code = T30_MIN_SCAN_0MS;
        break;
    }
    if ((s->iaf & T30_IAF_MODE_NO_FILL_BITS))
        min_row_bits = 0;
    else
        min_row_bits = (fallback_sequence[s->current_fallback].bit_rate*min_scan_times[s->min_scan_time_code])/1000;
    span_log(&s->logging, SPAN_LOG_FLOW, "Minimum bits per row will be %d\n", min_row_bits);
    t4_tx_set_min_bits_per_row(&s->t4.tx, min_row_bits);
}
/*- End of function --------------------------------------------------------*/

static int start_sending_document(t30_state_t *s)
{
    int res;

    if (s->tx_file[0] == '\0')
    {
        /* There is nothing to send */
        span_log(&s->logging, SPAN_LOG_FLOW, "No document to send\n");
        return -1;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "Start sending document\n");
    if (t4_tx_init(&s->t4.tx, s->tx_file, s->tx_start_page, s->tx_stop_page) == NULL)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Cannot open source TIFF file '%s'\n", s->tx_file);
        t30_set_status(s, T30_ERR_FILEERROR);
        return -1;
    }
    s->operation_in_progress = OPERATION_IN_PROGRESS_T4_TX;

    t4_tx_set_local_ident(&s->t4.tx, s->tx_info.ident);
    t4_tx_set_header_info(&s->t4.tx, s->header_info);
    if (s->use_own_tz)
        t4_tx_set_header_tz(&s->t4.tx, &s->tz);

    t4_tx_get_pages_in_file(&s->t4.tx);

    if ((res = t4_tx_set_tx_image_format(&s->t4.tx,
                                         s->mutual_compressions,
                                         s->mutual_image_sizes,
                                         s->mutual_bilevel_resolutions,
                                         s->mutual_colour_resolutions)) < 0)
    {
        switch (res)
        {
        case T4_IMAGE_FORMAT_INCOMPATIBLE:
            span_log(&s->logging, SPAN_LOG_WARNING, "Cannot negotiate an image format\n");
            t30_set_status(s, T30_ERR_BADTIFFHDR);
            break;
        case T4_IMAGE_FORMAT_NOSIZESUPPORT:
            span_log(&s->logging, SPAN_LOG_WARNING, "Cannot negotiate an image size\n");
            t30_set_status(s, T30_ERR_NOSIZESUPPORT);
            break;
        case T4_IMAGE_FORMAT_NORESSUPPORT:
            span_log(&s->logging, SPAN_LOG_WARNING, "Cannot negotiate an image resolution\n");
            t30_set_status(s, T30_ERR_NORESSUPPORT);
            break;
        default:
            span_log(&s->logging, SPAN_LOG_WARNING, "Cannot negotiate an image mode\n");
            t30_set_status(s, T30_ERR_BADTIFF);
            break;
        }
        return -1;
    }
    s->line_image_type = t4_tx_get_tx_image_type(&s->t4.tx);
    s->line_compression = t4_tx_get_tx_compression(&s->t4.tx);
    s->image_width = t4_tx_get_tx_image_width(&s->t4.tx);
    s->line_width_code = t4_tx_get_tx_image_width_code(&s->t4.tx);

    s->x_resolution = t4_tx_get_tx_x_resolution(&s->t4.tx);
    s->y_resolution = t4_tx_get_tx_y_resolution(&s->t4.tx);
    s->current_page_resolution = t4_tx_get_tx_resolution(&s->t4.tx);

    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "Choose image type %s (%d), compression %s (%d)\n",
             t4_image_type_to_str(s->line_image_type),
             s->line_image_type,
             t4_compression_to_str(s->line_compression),
             s->line_compression);

    /* The minimum scan time to be used can't be evaluated until we know the Y resolution. */
    set_min_scan_time(s);

    if (tx_start_page(s))
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Something seems to be wrong in the file\n");
        t30_set_status(s, T30_ERR_BADTIFFHDR);
        return -1;
    }

    if (s->error_correcting_mode)
    {
        if (get_partial_ecm_page(s) == 0)
            span_log(&s->logging, SPAN_LOG_WARNING, "No image data to send\n");
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int restart_sending_document(t30_state_t *s)
{
    t4_tx_restart_page(&s->t4.tx);
    s->retries = 0;
    s->ecm_block = 0;
    send_dcs_sequence(s, true);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int start_receiving_document(t30_state_t *s)
{
    if (s->rx_file[0] == '\0')
    {
        /* There is nothing to receive to */
        span_log(&s->logging, SPAN_LOG_FLOW, "No document to receive\n");
        return -1;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "Start receiving document\n");
    queue_phase(s, T30_PHASE_B_TX);
    s->ecm_block = 0;
    send_dis_or_dtc_sequence(s, true);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void unexpected_non_final_frame(t30_state_t *s, const uint8_t *msg, int len)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected %s frame in state %s\n", t30_frametype(msg[2]), state_names[s->state]);
    if (s->current_status == T30_ERR_OK)
        t30_set_status(s, T30_ERR_UNEXPECTED);
}
/*- End of function --------------------------------------------------------*/

static void unexpected_final_frame(t30_state_t *s, const uint8_t *msg, int len)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected %s frame in state %s\n", t30_frametype(msg[2]), state_names[s->state]);
    if (s->current_status == T30_ERR_OK)
        t30_set_status(s, T30_ERR_UNEXPECTED);
    send_dcn(s);
}
/*- End of function --------------------------------------------------------*/

static void unexpected_frame_length(t30_state_t *s, const uint8_t *msg, int len)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected %s frame length - %d\n", t30_frametype(msg[0]), len);
    if (s->current_status == T30_ERR_OK)
        t30_set_status(s, T30_ERR_UNEXPECTED);
    send_dcn(s);
}
/*- End of function --------------------------------------------------------*/

static int process_rx_dis_dtc(t30_state_t *s, const uint8_t *msg, int len)
{
    int new_status;

    queue_phase(s, T30_PHASE_B_TX);
    if (analyze_rx_dis_dtc(s, msg, len) < 0)
    {
        send_dcn(s);
        return -1;
    }
    if (s->phase_b_handler)
    {
        new_status = s->phase_b_handler(s->phase_b_user_data, msg[2]);
        if (new_status != T30_ERR_OK)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Application rejected DIS/DTC - '%s'\n", t30_completion_code_to_str(new_status));
            t30_set_status(s, new_status);
            /* TODO: If FNV is allowed, process it here */
            send_dcn(s);
            return -1;
        }
    }
    /* Try to send something */
    if (s->tx_file[0])
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Trying to send file '%s'\n", s->tx_file);
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_READY_TO_RECEIVE_FAX_DOCUMENT))
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "%s far end cannot receive\n", t30_frametype(msg[2]));
            t30_set_status(s, T30_ERR_RX_INCAPABLE);
            send_dcn(s);
            return -1;
        }
        if (start_sending_document(s))
        {
            send_dcn(s);
            return -1;
        }
        if (build_dcs(s))
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "The far end is incompatible\n", s->tx_file);
            send_dcn(s);
            return -1;
        }
        /* Start document transmission */
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "Put document with modem (%d) %s at %dbps\n",
                 fallback_sequence[s->current_fallback].modem_type,
                 t30_modem_to_str(fallback_sequence[s->current_fallback].modem_type),
                 fallback_sequence[s->current_fallback].bit_rate);
        s->retries = 0;
        send_dcs_sequence(s, true);
        return 0;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "%s nothing to send\n", t30_frametype(msg[2]));
    /* ... then try to receive something */
    if (s->rx_file[0])
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Trying to receive file '%s'\n", s->rx_file);
        if (!test_ctrl_bit(s->far_dis_dtc_frame, T30_DIS_BIT_READY_TO_TRANSMIT_FAX_DOCUMENT))
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "%s far end cannot transmit\n", t30_frametype(msg[2]));
            t30_set_status(s, T30_ERR_TX_INCAPABLE);
            send_dcn(s);
            return -1;
        }
        if (start_receiving_document(s))
        {
            send_dcn(s);
            return -1;
        }
        if (set_dis_or_dtc(s))
        {
            t30_set_status(s, T30_ERR_INCOMPATIBLE);
            send_dcn(s);
            return -1;
        }
        s->retries = 0;
        send_dis_or_dtc_sequence(s, true);
        return 0;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "%s nothing to receive\n", t30_frametype(msg[2]));
    /* There is nothing to do, or nothing we are able to do. */
    send_dcn(s);
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_dcs(t30_state_t *s, const uint8_t *msg, int len)
{
    int new_status;

    if (analyze_rx_dcs(s, msg, len) < 0)
    {
        send_dcn(s);
        return -1;
    }

    if (s->phase_b_handler)
    {
        new_status = s->phase_b_handler(s->phase_b_user_data, msg[2]);
        if (new_status != T30_ERR_OK)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Application rejected DCS - '%s'\n", t30_completion_code_to_str(new_status));
            t30_set_status(s, new_status);
            /* TODO: If FNV is allowed, process it here */
            send_dcn(s);
            return -1;
        }
    }
    /* Start document reception */
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "Get document with modem (%d) %s at %dbps\n",
             fallback_sequence[s->current_fallback].modem_type,
             t30_modem_to_str(fallback_sequence[s->current_fallback].modem_type),
             fallback_sequence[s->current_fallback].bit_rate);
    if (s->rx_file[0] == '\0')
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "No document to receive\n");
        t30_set_status(s, T30_ERR_FILEERROR);
        send_dcn(s);
        return -1;
    }
    if (s->operation_in_progress != OPERATION_IN_PROGRESS_T4_RX)
    {
        if (t4_rx_init(&s->t4.rx, s->rx_file, s->supported_output_compressions) == NULL)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "Cannot open target TIFF file '%s'\n", s->rx_file);
            t30_set_status(s, T30_ERR_FILEERROR);
            send_dcn(s);
            return -1;
        }
        s->operation_in_progress = OPERATION_IN_PROGRESS_T4_RX;
    }
    if (!(s->iaf & T30_IAF_MODE_NO_TCF))
    {
        /* TCF is always sent with long training */
        s->short_train = false;
        set_state(s, T30_STATE_F_TCF);
        queue_phase(s, T30_PHASE_C_NON_ECM_RX);
        timer_t2_start(s);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int send_response_to_pps(t30_state_t *s)
{
    queue_phase(s, T30_PHASE_D_TX);
    if (s->rx_ecm_block_ok)
    {
        set_state(s, T30_STATE_F_POST_RCP_MCF);
        send_simple_frame(s, T30_MCF);
        return true;
    }
    /* We need to send the PPR frame we have created, to try to fill in the missing/bad data. */
    set_state(s, T30_STATE_F_POST_RCP_PPR);
    s->ecm_frame_map[0] = ADDRESS_FIELD;
    s->ecm_frame_map[1] = CONTROL_FIELD_FINAL_FRAME;
    s->ecm_frame_map[2] = (uint8_t) (T30_PPR | s->dis_received);
    send_frame(s, s->ecm_frame_map, 3 + 32);
    return false;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_pps(t30_state_t *s, const uint8_t *msg, int len)
{
    int page;
    int block;
    int frames;
    int i;
    int j;
    int frame_no;
    int first_bad_frame;
    int first;
    int expected_len;
    int res;

    if (len < 7)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Bad PPS message length %d.\n", len);
        return -1;
    }
    s->last_pps_fcf2 = msg[3] & 0xFE;

    /* The frames count is not well specified in T.30. In practice it seems it might be the
       number of frames in the current block, or it might be the number of frames in the
       current burst of transmission. For a burst of resent frames this would make it smaller
       than the actual size of the block. If we only accept the number when it exceeds
       previous values, we should get the real number of frames in the block. */
    frames = msg[6] + 1;
    block = msg[5];
    page = msg[4];

    if (s->ecm_frames < 0)
    {
        /* First time. Take the number and believe in it. */
        s->ecm_frames = frames;
    }
    else
    {
        /* If things have gone wrong, the far end might try to send us zero FCD
           frames. It can't represent zero in the block count field, so it might
           put zero there, or it might simplistically insert (blocks - 1), and put
           0xFF there. Beware of this. */
        if (frames == 0xFF)
        {
            /* This is probably zero, erroneously rolled over to the maximum count */
            frames = 0;
        }
    }
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "Received PPS + %s - page %d, block %d, %d frames\n",
             t30_frametype(msg[3]),
             page,
             block,
             frames);
    /* Check that we have received the page and block we expected. If the far end missed
       our last response, it might have repeated the previous chunk. */
    if ((s->rx_page_number & 0xFF) != page  ||  (s->ecm_block & 0xFF) != block)
    {
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "ECM rx page/block mismatch - expected %d/%d, but received %d/%d.\n",
                 (s->rx_page_number & 0xFF),
                 (s->ecm_block & 0xFF),
                 page,
                 block);
        /* Look for this being a repeat, because the other end missed the last response
           we sent - which would have been a T30_MCF - If the block is for the previous
           page, or the previous block of the current page, we can assume we have hit this
           condition. */
        if (((s->rx_page_number & 0xFF) == page  &&  ((s->ecm_block - 1) & 0xFF) == block)
            ||
            (((s->rx_page_number - 1) & 0xFF) == page  &&  s->ecm_block == 0))
        {
            /* This must be a repeat of the last thing the far end sent, while we are expecting
               the first transfer of a new block. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Looks like a repeat from the previous page/block - send MCF again.\n");
            /* Clear the ECM buffer */
            for (i = 0;  i < 256;  i++)
                s->ecm_len[i] = -1;
            s->ecm_frames = -1;
            queue_phase(s, T30_PHASE_D_TX);
            set_state(s, T30_STATE_F_POST_RCP_MCF);
            send_simple_frame(s, T30_MCF);
        }
        else
        {
            /* Give up */
            t30_set_status(s, T30_ERR_RX_ECMPHD);
            send_dcn(s);
        }
        return 0;
    }

    /* Build a bit map of which frames we now have stored OK */
    first_bad_frame = 256;
    first = true;
    expected_len = 256;
    for (i = 0;  i < 32;  i++)
    {
        s->ecm_frame_map[i + 3] = 0;
        for (j = 0;  j < 8;  j++)
        {
            frame_no = (i << 3) + j;
            if (s->ecm_len[frame_no] >= 0)
            {
                /* The correct pattern of frame lengths is they will all be 64 or 256 octets long, except the
                   last one. The last one might the same length as all the others, or it might be exactly the
                   right length to contain the last chunk of the data. That is, some people pad at the end,
                   and some do not. */
                /* Vet the frames which are present, to detect any with inappropriate lengths. This might seem
                   like overkill, as the frames must have had good CRCs to get this far. However, in the real
                   world there are systems, especially T.38 ones, which give bad frame lengths, and which screw
                   up communication unless you apply these checks. From experience, if you find a frame has a
                   suspect length, and demand retransmission, there is a good chance the new copy will be alright. */
                if (frame_no < s->ecm_frames - 1)
                {
                    /* Expect all frames, except the last one, to follow the length of the first one */
                    if (first)
                    {
                        /* Use the length of the first frame as our model for what the length should be */
                        if (s->ecm_len[frame_no] == 64)
                            expected_len = 64;
                        first = false;
                    }
                    /* Check the length is consistent with the first frame */
                    if (s->ecm_len[frame_no] != expected_len)
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Bad length ECM frame - %d\n", s->ecm_len[frame_no]);
                        s->ecm_len[frame_no] = -1;
                    }
                }
            }
            if (s->ecm_len[frame_no] < 0)
            {
                s->ecm_frame_map[i + 3] |= (1 << j);
                if (frame_no < first_bad_frame)
                    first_bad_frame = frame_no;
                if (frame_no < s->ecm_frames)
                    s->error_correcting_mode_retries++;
            }
        }
    }
    s->rx_ecm_block_ok = (first_bad_frame >= s->ecm_frames);
    if (s->rx_ecm_block_ok)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Partial page OK - committing block %d, %d frames\n", s->ecm_block, s->ecm_frames);
        /* Deliver the ECM data */
        for (i = 0;  i < s->ecm_frames;  i++)
        {
            if (s->document_put_handler)
                res = s->document_put_handler(s->document_put_user_data, s->ecm_data[i], s->ecm_len[i]);
            else
                res = t4_rx_put(&s->t4.rx, s->ecm_data[i], s->ecm_len[i]);
            if (res != T4_DECODE_MORE_DATA)
            {
                /* This is the end of the document */
                if (res != T4_DECODE_OK)
                    span_log(&s->logging, SPAN_LOG_FLOW, "Document ended with status %d\n", res);
                break;
            }
        }
        /* Clear the ECM buffer */
        for (i = 0;  i < 256;  i++)
            s->ecm_len[i] = -1;
        s->ecm_block++;
        s->ecm_frames = -1;

        switch (s->last_pps_fcf2)
        {
        case T30_NULL:
            /* We can accept only this partial page. */
            break;
        default:
            /* We can accept and confirm the whole page. */
            s->next_rx_step = s->last_pps_fcf2;
            rx_end_page(s);
            report_rx_ecm_page_result(s);
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, s->last_pps_fcf2);
            rx_start_page(s);
            break;
        }
    }

    switch (s->last_pps_fcf2)
    {
    case T30_PRI_MPS:
    case T30_PRI_EOM:
    case T30_PRI_EOP:
        if (s->remote_interrupts_allowed)
        {
        }
        /* Fall through */
    case T30_NULL:
    case T30_MPS:
    case T30_EOM:
    case T30_EOS:
    case T30_EOP:
        if (s->receiver_not_ready_count > 0)
        {
            s->receiver_not_ready_count--;
            queue_phase(s, T30_PHASE_D_TX);
            set_state(s, T30_STATE_F_POST_RCP_RNR);
            send_simple_frame(s, T30_RNR);
        }
        else
        {
            if (send_response_to_pps(s))
            {
                switch (s->last_pps_fcf2)
                {
                case T30_PRI_EOP:
                case T30_EOP:
                    span_log(&s->logging, SPAN_LOG_FLOW, "End of procedure detected\n");
                    s->end_of_procedure_detected = true;
                    break;
                }
            }
        }
        break;
    default:
        unexpected_final_frame(s, msg, len);
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void process_rx_ppr(t30_state_t *s, const uint8_t *msg, int len)
{
    int i;
    int j;
    int frame_no;
    uint8_t frame[4];

    if (len != 3 + 256/8)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for PPR bits - %d\n", (len - 3)*8);
        /* This frame didn't get corrupted in transit, because its CRC is OK. It was sent bad
           and there is little possibility that causing a retransmission will help. It is best
           to just give up. */
        t30_set_status(s, T30_ERR_TX_ECMPHD);
        disconnect(s);
        return;
    }
    /* Check which frames are OK, and mark them as OK. */
    for (i = 0;  i < 32;  i++)
    {
        for (j = 0;  j < 8;  j++)
        {
            frame_no = (i << 3) + j;
            /* Tick off the frames they are not complaining about as OK */
            if ((msg[i + 3] & (1 << j)) == 0)
            {
                if (s->ecm_len[frame_no] >= 0)
                    s->ecm_progress++;
                s->ecm_len[frame_no] = -1;
            }
            else
            {
                if (frame_no < s->ecm_frames)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Frame %d to be resent\n", frame_no);
                    s->error_correcting_mode_retries++;
                }
#if 0
                /* Diagnostic: See if the other end is complaining about something we didn't even send this time. */
                if (s->ecm_len[frame_no] < 0)
                    span_log(&s->logging, SPAN_LOG_FLOW, "PPR contains complaint about frame %d, which was not sent\n", frame_no);
#endif
            }
        }
    }
    if (++s->ppr_count >= PPR_LIMIT_BEFORE_CTC_OR_EOR)
    {
        /* Continue to correct? */
        /* Continue only if we have been making progress */
        s->ppr_count = 0;
        if (s->ecm_progress)
        {
            s->ecm_progress = 0;
            queue_phase(s, T30_PHASE_D_TX);
            set_state(s, T30_STATE_IV_CTC);
            send_simple_frame(s, T30_CTC);
        }
        else
        {
            set_state(s, T30_STATE_IV_EOR);
            queue_phase(s, T30_PHASE_D_TX);
            frame[0] = ADDRESS_FIELD;
            frame[1] = CONTROL_FIELD_FINAL_FRAME;
            frame[2] = (uint8_t) (T30_EOR | s->dis_received);
            frame[3] = (s->ecm_at_page_end)  ?  ((uint8_t) (s->next_tx_step | s->dis_received))  :  T30_NULL;
            span_log(&s->logging, SPAN_LOG_FLOW, "Sending EOR + %s\n", t30_frametype(frame[3]));
            send_frame(s, frame, 4);
        }
    }
    else
    {
        /* Initiate resending of the remainder of the frames. */
        set_state(s, T30_STATE_IV);
        queue_phase(s, T30_PHASE_C_ECM_TX);
        send_first_ecm_frame(s);
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_fcd(t30_state_t *s, const uint8_t *msg, int len)
{
    int frame_no;

    /* Facsimile coded data */
    switch (s->state)
    {
    case T30_STATE_F_DOC_ECM:
        if (len > 4 + 256)
        {
            /* For other frame types we kill the call on an unexpected frame length. For FCD frames it is better to just ignore
               the frame, and let retries sort things out. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected %s frame length - %d\n", t30_frametype(msg[0]), len);
        }
        else
        {
            frame_no = msg[3];
            /* Just store the actual image data, and record its length */
            span_log(&s->logging, SPAN_LOG_FLOW, "Storing ECM frame %d, length %d\n", frame_no, len - 4);
            memcpy(&s->ecm_data[frame_no][0], &msg[4], len - 4);
            s->ecm_len[frame_no] = (int16_t) (len - 4);
            /* In case we are just after a CTC/CTR exchange, which kicked us back to long training */
            s->short_train = true;
        }
        /* We have received something, so any missing carrier status is out of date */
        if (s->current_status == T30_ERR_RX_NOCARRIER)
            t30_set_status(s, T30_ERR_OK);
        break;
    default:
        unexpected_non_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_rcp(t30_state_t *s, const uint8_t *msg, int len)
{
    /* Return to control for partial page. These might come through with or without the final frame tag.
       Here we deal with the "no final frame tag" case. */
    switch (s->state)
    {
    case T30_STATE_F_DOC_ECM:
        set_state(s, T30_STATE_F_POST_DOC_ECM);
        queue_phase(s, T30_PHASE_D_RX);
        timer_t2_start(s);
        /* We have received something, so any missing carrier status is out of date */
        if (s->current_status == T30_ERR_RX_NOCARRIER)
            t30_set_status(s, T30_ERR_OK);
        break;
    case T30_STATE_F_POST_DOC_ECM:
        /* Just ignore this. It must be an extra RCP. Several are usually sent, to maximise the chance
           of receiving a correct one. */
        timer_t2_start(s);
        break;
    default:
        unexpected_non_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_fnv(t30_state_t *s, const uint8_t *msg, int len)
{
    logging_state_t *log;
    const char *x;

    /* Field not valid */
    /* TODO: analyse the message, as per 5.3.6.2.13 */
    if (!span_log_test(&s->logging, SPAN_LOG_FLOW))
        return;
    log = &s->logging;

    if ((msg[3] & 0x01))
        span_log(log, SPAN_LOG_FLOW, "  Incorrect password (PWD).\n");
    if ((msg[3] & 0x02))
        span_log(log, SPAN_LOG_FLOW, "  Selective polling reference (SEP) not known.\n");
    if ((msg[3] & 0x04))
        span_log(log, SPAN_LOG_FLOW, "  Sub-address (SUB) not known.\n");
    if ((msg[3] & 0x08))
        span_log(log, SPAN_LOG_FLOW, "  Sender identity (SID) not known.\n");
    if ((msg[3] & 0x10))
        span_log(log, SPAN_LOG_FLOW, "  Secure fax error.\n");
    if ((msg[3] & 0x20))
        span_log(log, SPAN_LOG_FLOW, "  Transmitting subscriber identity (TSI) not accepted.\n");
    if ((msg[3] & 0x40))
        span_log(log, SPAN_LOG_FLOW, "  Polled sub-address (PSA) not known.\n");
    if (len > 4  &&  (msg[3] & DISBIT8))
    {
        if ((msg[4] & 0x01))
            span_log(log, SPAN_LOG_FLOW, "  BFT negotiations request not accepted.\n");
        if ((msg[4] & 0x02))
            span_log(log, SPAN_LOG_FLOW, "  Internet routing address (IRA) not known.\n");
        if ((msg[4] & 0x04))
            span_log(log, SPAN_LOG_FLOW, "  Internet selective polling address (ISP) not known.\n");
    }
    if (len > 5)
    {
        span_log(log, SPAN_LOG_FLOW, "  FNV sequence number %d.\n", msg[5]);
    }
    if (len > 6)
    {
        switch (msg[6])
        {
        case T30_PWD:
            x = "Incorrect password (PWD)";
            break;
        case T30_SEP:
            x = "Selective polling reference (SEP) not known";
            break;
        case T30_SUB:
        case T30_SUB | 0x01:
            x = "Sub-address (SUB) not known";
            break;
        case T30_SID:
        case T30_SID | 0x01:
            x = "Sender identity (SID) not known";
            break;
        case T30_SPI:
            x = "Secure fax error";
            break;
        case T30_TSI:
        case T30_TSI | 0x01:
            x = "Transmitting subscriber identity (TSI) not accepted";
            break;
        case T30_PSA:
            x = "Polled sub-address (PSA) not known";
            break;
        default:
            x = "???";
            break;
        }
        span_log(log, SPAN_LOG_FLOW, "  FNV diagnostic info type %s.\n", x);
    }
    if (len > 7)
    {
        span_log(log, SPAN_LOG_FLOW, "  FNV length %d.\n", msg[7]);
    }
    /* We've decoded it, but we don't yet know how to deal with it, so treat it as unexpected */
    unexpected_final_frame(s, msg, len);
}
/*- End of function --------------------------------------------------------*/

static void process_state_answering(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    /* We should be sending the TCF data right now */
    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DIS:
        /* TODO: This is a fudge to allow for starting up in T.38, where the other end has
           seen DIS by analogue modem means, and has immediately sent DIS/DTC. We might have
           missed useful info, like TSI, but just accept things and carry on form now. */
        span_log(&s->logging, SPAN_LOG_FLOW, "DIS/DTC before DIS\n");
        process_rx_dis_dtc(s, msg, len);
        break;
    case T30_DCS:
        /* TODO: This is a fudge to allow for starting up in T.38, where the other end has
           seen DIS by analogue modem means, and has immediately sent DCS. We might have
           missed useful info, like TSI, but just accept things and carry on form now. */
        span_log(&s->logging, SPAN_LOG_FLOW, "DCS before DIS\n");
        process_rx_dcs(s, msg, len);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_TX_GOTDCN);
        disconnect(s);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_b(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DCN:
        /* Just ignore any DCN's which appear at this stage. */
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_c(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DCN:
        /* Just ignore any DCN's which appear at this stage. */
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_d(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    /* We should be sending the DCS sequence right now */
    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DCN:
        t30_set_status(s, T30_ERR_TX_BADDCS);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_d_tcf(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    /* We should be sending the TCF data right now */
    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DCN:
        t30_set_status(s, T30_ERR_TX_BADDCS);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_d_post_tcf(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_CFR:
        /* Trainability test succeeded. Send the document. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Trainability test succeeded\n");
        s->retries = 0;
        s->short_train = true;
        if (s->error_correcting_mode)
        {
            set_state(s, T30_STATE_IV);
            queue_phase(s, T30_PHASE_C_ECM_TX);
            send_first_ecm_frame(s);
        }
        else
        {
            set_state(s, T30_STATE_I);
            queue_phase(s, T30_PHASE_C_NON_ECM_TX);
        }
        break;
    case T30_FTT:
        /* Trainability test failed. Try again. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Trainability test failed\n");
        s->retries = 0;
        s->short_train = false;
        if (step_fallback_entry(s) < 0)
        {
            /* We have fallen back as far as we can go. Give up. */
            t30_set_status(s, T30_ERR_CANNOT_TRAIN);
            send_dcn(s);
            break;
        }
        queue_phase(s, T30_PHASE_B_TX);
        send_dcs_sequence(s, true);
        break;
    case T30_DIS:
        /* It appears they didn't see what we sent - retry the TCF */
        if (++s->retries >= MAX_COMMAND_TRIES)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Too many retries. Giving up.\n");
            t30_set_status(s, T30_ERR_RETRYDCN);
            send_dcn(s);
            break;
        }
        span_log(&s->logging, SPAN_LOG_FLOW, "Retry number %d\n", s->retries);
        queue_phase(s, T30_PHASE_B_TX);
        /* TODO: should we reassess the new DIS message, and possibly adjust the DCS we use? */
        send_dcs_sequence(s, true);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_TX_BADDCS);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_f_tcf(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    /* We should be receiving TCF right now, not HDLC messages */
    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_f_cfr(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    /* We're waiting for a response to the CFR we sent */
    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DCS:
        /* If we received another DCS, they must have missed our CFR */
        process_rx_dcs(s, msg, len);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_f_ftt(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    /* We're waiting for a response to the FTT we sent */
    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DCS:
        process_rx_dcs(s, msg, len);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_f_doc_non_ecm(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    /* If we are getting HDLC messages, and we have not moved to the _POST_DOC_NON_ECM
       state, it looks like either:
        - we didn't see the image data carrier properly, or
        - they didn't see our T30_CFR, and are repeating the DCS/TCF sequence.
        - they didn't see out T30_MCF, and are repeating the end of page message. */
    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DIS:
        process_rx_dis_dtc(s, msg, len);
        break;
    case T30_DCS:
        process_rx_dcs(s, msg, len);
        break;
    case T30_PRI_MPS:
        if (s->remote_interrupts_allowed)
        {
        }
        /* Fall through */
    case T30_MPS:
        /* Treat this as a bad quality page. */
        if (s->phase_d_handler)
            s->phase_d_handler(s->phase_d_user_data, fcf);
        s->next_rx_step = msg[2] & 0xFE;
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_III_Q_RTN);
        send_simple_frame(s, T30_RTN);
        break;
    case T30_PRI_EOM:
        if (s->remote_interrupts_allowed)
        {
        }
        /* Fall through */
    case T30_EOM:
    case T30_EOS:
        /* Treat this as a bad quality page. */
        if (s->phase_d_handler)
            s->phase_d_handler(s->phase_d_user_data, fcf);
        s->next_rx_step = msg[2] & 0xFE;
        /* Return to phase B */
        queue_phase(s, T30_PHASE_B_TX);
        set_state(s, T30_STATE_III_Q_RTN);
        send_simple_frame(s, T30_RTN);
        break;
    case T30_PRI_EOP:
        if (s->remote_interrupts_allowed)
        {
        }
        /* Fall through */
    case T30_EOP:
        /* Treat this as a bad quality page. */
        if (s->phase_d_handler)
            s->phase_d_handler(s->phase_d_user_data, fcf);
        s->next_rx_step = msg[2] & 0xFE;
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_III_Q_RTN);
        send_simple_frame(s, T30_RTN);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_RX_DCNDATA);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        t30_set_status(s, T30_ERR_RX_INVALCMD);
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void assess_copy_quality(t30_state_t *s, uint8_t fcf)
{
    int quality;
    
    quality = copy_quality(s);
    switch (quality)
    {
    case T30_COPY_QUALITY_PERFECT:
    case T30_COPY_QUALITY_GOOD:
        rx_end_page(s);
        break;
    case T30_COPY_QUALITY_POOR:
        rx_end_page(s);
        break;
    case T30_COPY_QUALITY_BAD:
        /* Some people want to keep even the bad pages */
        if (s->keep_bad_pages)
            rx_end_page(s);
        break;
    }

    if (s->phase_d_handler)
        s->phase_d_handler(s->phase_d_user_data, fcf);
    if (fcf == T30_EOP)
        terminate_operation_in_progress(s);
    else
        rx_start_page(s);

    switch (quality)
    {
    case T30_COPY_QUALITY_PERFECT:
    case T30_COPY_QUALITY_GOOD:
        set_state(s, T30_STATE_III_Q_MCF);
        send_simple_frame(s, T30_MCF);
        break;
    case T30_COPY_QUALITY_POOR:
        set_state(s, T30_STATE_III_Q_RTP);
        send_simple_frame(s, T30_RTP);
        break;
    case T30_COPY_QUALITY_BAD:
        set_state(s, T30_STATE_III_Q_RTN);
        send_simple_frame(s, T30_RTN);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_f_post_doc_non_ecm(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_PRI_MPS:
        if (s->remote_interrupts_allowed)
        {
        }
        /* Fall through */
    case T30_MPS:
        s->next_rx_step = fcf;
        queue_phase(s, T30_PHASE_D_TX);
        assess_copy_quality(s, fcf);
        break;
    case T30_PRI_EOM:
        if (s->remote_interrupts_allowed)
        {
        }
        /* Fall through */
    case T30_EOM:
    case T30_EOS:
        s->next_rx_step = fcf;
        /* Return to phase B */
        queue_phase(s, T30_PHASE_B_TX);
        assess_copy_quality(s, fcf);
        break;
    case T30_PRI_EOP:
        if (s->remote_interrupts_allowed)
        {
        }
        /* Fall through */
    case T30_EOP:
        span_log(&s->logging, SPAN_LOG_FLOW, "End of procedure detected\n");
        s->end_of_procedure_detected = true;
        s->next_rx_step = fcf;
        queue_phase(s, T30_PHASE_D_TX);
        assess_copy_quality(s, fcf);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_RX_DCNFAX);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        t30_set_status(s, T30_ERR_RX_INVALCMD);
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_f_doc_and_post_doc_ecm(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;
    uint8_t fcf2;

    /* This actually handles 2 states - _DOC_ECM and _POST_DOC_ECM - as they are very similar */
    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DIS:
        process_rx_dis_dtc(s, msg, len);
        break;
    case T30_DCS:
        process_rx_dcs(s, msg, len);
        break;
    case T4_RCP:
        /* Return to control for partial page. These might come through with or without the final frame tag.
           Here we deal with the "final frame tag" case. */
        process_rx_rcp(s, msg, len);
        break;
    case T30_EOR:
        if (len != 4)
        {
            unexpected_frame_length(s, msg, len);
            break;
        }
        fcf2 = msg[3] & 0xFE;
        span_log(&s->logging, SPAN_LOG_FLOW, "Received EOR + %s\n", t30_frametype(msg[3]));
        switch (fcf2)
        {
        case T30_PRI_EOP:
        case T30_PRI_EOM:
        case T30_PRI_MPS:
            if (s->remote_interrupts_allowed)
            {
                /* TODO: Alert operator */
            }
            /* Fall through */
        case T30_NULL:
        case T30_EOP:
        case T30_EOM:
        case T30_EOS:
        case T30_MPS:
            s->next_rx_step = fcf2;
            queue_phase(s, T30_PHASE_D_TX);
            set_state(s, T30_STATE_F_DOC_ECM);
            send_simple_frame(s, T30_ERR);
            break;
        default:
            unexpected_final_frame(s, msg, len);
            break;
        }
        break;
    case T30_PPS:
        process_rx_pps(s, msg, len);
        break;
    case T30_CTC:
        /* T.30 says we change back to long training here */
        s->short_train = false;
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_F_DOC_ECM);
        send_simple_frame(s, T30_CTR);
        break;
    case T30_RR:
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_RX_DCNDATA);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        t30_set_status(s, T30_ERR_RX_INVALCMD);
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_f_post_rcp_mcf(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    case T30_DCN:
        disconnect(s);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_f_post_rcp_ppr(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_f_post_rcp_rnr(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_RR:
        if (s->receiver_not_ready_count > 0)
        {
            s->receiver_not_ready_count--;
            queue_phase(s, T30_PHASE_D_TX);
            set_state(s, T30_STATE_F_POST_RCP_RNR);
            send_simple_frame(s, T30_RNR);
        }
        else
        {
            /* Now we send the deferred response */
            if (send_response_to_pps(s))
            {
                switch (s->last_pps_fcf2)
                {
                case T30_PRI_EOP:
                case T30_EOP:
                    span_log(&s->logging, SPAN_LOG_FLOW, "End of procedure detected\n");
                    s->end_of_procedure_detected = true;
                    break;
                }
            }
        }
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_r(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DIS:
        process_rx_dis_dtc(s, msg, len);
        break;
    case T30_DCS:
        process_rx_dcs(s, msg, len);
        break;
    case T30_DCN:
        /* Received a DCN while waiting for a DIS or DCN */
        t30_set_status(s, T30_ERR_RX_DCNWHY);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_t(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_DIS:
        process_rx_dis_dtc(s, msg, len);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_TX_GOTDCN);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        t30_set_status(s, T30_ERR_TX_NODIS);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_i(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_ii(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_ii_q(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_PIP:
        if (s->remote_interrupts_allowed)
        {
            s->retries = 0;
            if (s->phase_d_handler)
            {
                s->phase_d_handler(s->phase_d_user_data, fcf);
                s->timer_t3 = ms_to_samples(DEFAULT_TIMER_T3);
            }
        }
        /* Fall through */
    case T30_MCF:
        switch (s->next_tx_step)
        {
        case T30_PRI_MPS:
        case T30_MPS:
            tx_end_page(s);
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, fcf);
            /* Transmit the next page */
            if (tx_start_page(s))
            {
                /* TODO: recover */
                break;
            }
            set_state(s, T30_STATE_I);
            queue_phase(s, T30_PHASE_C_NON_ECM_TX);
            break;
        case T30_PRI_EOM:
        case T30_EOM:
        case T30_EOS:
            tx_end_page(s);
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, fcf);
            terminate_operation_in_progress(s);
            report_tx_result(s, true);
            return_to_phase_b(s, false);
            break;
        case T30_PRI_EOP:
        case T30_EOP:
            tx_end_page(s);
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, fcf);
            terminate_operation_in_progress(s);
            send_dcn(s);
            report_tx_result(s, true);
            break;
        }
        break;
    case T30_RTP:
        s->rtp_events++;
        switch (s->next_tx_step)
        {
        case T30_PRI_MPS:
        case T30_MPS:
            tx_end_page(s);
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, fcf);
            if (tx_start_page(s))
            {
                /* TODO: recover */
                break;
            }
            /* Send fresh training, and then the next page */
            if (step_fallback_entry(s) < 0)
            {
                /* We have fallen back as far as we can go. Give up. */
                t30_set_status(s, T30_ERR_CANNOT_TRAIN);
                send_dcn(s);
                break;
            }
            queue_phase(s, T30_PHASE_B_TX);
            restart_sending_document(s);
            break;
        case T30_PRI_EOM:
        case T30_EOM:
        case T30_EOS:
            tx_end_page(s);
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, fcf);
            t4_tx_release(&s->t4.tx);
            /* TODO: should go back to T, and resend */
            return_to_phase_b(s, true);
            break;
        case T30_PRI_EOP:
        case T30_EOP:
            tx_end_page(s);
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, fcf);
            t4_tx_release(&s->t4.tx);
            send_dcn(s);
            break;
        }
        break;
    case T30_PIN:
        if (s->remote_interrupts_allowed)
        {
            s->retries = 0;
            if (s->phase_d_handler)
            {
                s->phase_d_handler(s->phase_d_user_data, fcf);
                s->timer_t3 = ms_to_samples(DEFAULT_TIMER_T3);
            }
        }
        /* Fall through */
    case T30_RTN:
        s->rtn_events++;
        switch (s->next_tx_step)
        {
        case T30_PRI_MPS:
        case T30_MPS:
            s->retries = 0;
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, fcf);
            if (!s->retransmit_capable)
            {
                /* Send the next page, regardless of the problem with the current one. */
                if (tx_start_page(s))
                {
                    /* TODO: recover */
                    break;
                }
            }
            /* Send fresh training */
            if (step_fallback_entry(s) < 0)
            {
                /* We have fallen back as far as we can go. Give up. */
                t30_set_status(s, T30_ERR_CANNOT_TRAIN);
                send_dcn(s);
                break;
            }
            queue_phase(s, T30_PHASE_B_TX);
            restart_sending_document(s);
            break;
        case T30_PRI_EOM:
        case T30_EOM:
        case T30_EOS:
            s->retries = 0;
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, fcf);
            if (s->retransmit_capable)
            {
                /* Wait for DIS */
            }
            else
            {
                return_to_phase_b(s, true);
            }
            break;
        case T30_PRI_EOP:
        case T30_EOP:
            s->retries = 0;
            if (s->phase_d_handler)
                s->phase_d_handler(s->phase_d_user_data, fcf);
            if (s->retransmit_capable)
            {
                /* Send fresh training, and then repeat the last page */
                if (step_fallback_entry(s) < 0)
                {
                    /* We have fallen back as far as we can go. Give up. */
                    t30_set_status(s, T30_ERR_CANNOT_TRAIN);
                    send_dcn(s);
                    break;
                }
                queue_phase(s, T30_PHASE_B_TX);
                restart_sending_document(s);
            }
            else
            {
                send_dcn(s);
            }
            break;
        }
        break;
    case T30_DCN:
        switch (s->next_tx_step)
        {
        case T30_PRI_MPS:
        case T30_PRI_EOM:
        case T30_MPS:
        case T30_EOM:
        case T30_EOS:
            /* Unexpected DCN after EOM, EOS or MPS sequence */
            t30_set_status(s, T30_ERR_RX_DCNPHD);
            break;
        default:
            t30_set_status(s, T30_ERR_TX_BADPG);
            break;
        }
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        t30_set_status(s, T30_ERR_TX_INVALRSP);
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iii_q_mcf(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_EOP:
    case T30_EOM:
    case T30_EOS:
    case T30_MPS:
        /* Looks like they didn't see our signal. Repeat it */
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_III_Q_MCF);
        send_simple_frame(s, T30_MCF);
        break;
    case T30_DIS:
        if (msg[2] == T30_DTC)
            process_rx_dis_dtc(s, msg, len);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    case T30_DCN:
        disconnect(s);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iii_q_rtp(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_EOP:
    case T30_EOM:
    case T30_EOS:
    case T30_MPS:
        /* Looks like they didn't see our signal. Repeat it */
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_III_Q_RTP);
        send_simple_frame(s, T30_RTP);
        break;
    case T30_DIS:
        if (msg[2] == T30_DTC)
            process_rx_dis_dtc(s, msg, len);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iii_q_rtn(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_EOP:
    case T30_EOM:
    case T30_EOS:
    case T30_MPS:
        /* Looks like they didn't see our signal. Repeat it */
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_III_Q_RTN);
        send_simple_frame(s, T30_RTN);
        break;
    case T30_DIS:
        if (msg[2] == T30_DTC)
            process_rx_dis_dtc(s, msg, len);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_RX_DCNNORTN);
        disconnect(s);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iv(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iv_pps_null(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_MCF:
        s->retries = 0;
        s->timer_t5 = 0;
        /* Is there more of the current page to get, or do we move on? */
        span_log(&s->logging, SPAN_LOG_FLOW, "Is there more to send? - %d %d\n", s->ecm_frames, s->ecm_len[255]);
        if (!s->ecm_at_page_end  &&  get_partial_ecm_page(s) > 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Additional image data to send\n");
            s->ecm_block++;
            set_state(s, T30_STATE_IV);
            queue_phase(s, T30_PHASE_C_ECM_TX);
            send_first_ecm_frame(s);
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Moving on to the next page\n");
            switch (s->next_tx_step)
            {
            case T30_PRI_MPS:
            case T30_MPS:
                tx_end_page(s);
                if (s->phase_d_handler)
                    s->phase_d_handler(s->phase_d_user_data, fcf);
                if (tx_start_page(s))
                {
                    /* TODO: recover */
                    break;
                }
                if (get_partial_ecm_page(s) > 0)
                {
                    set_state(s, T30_STATE_IV);
                    queue_phase(s, T30_PHASE_C_ECM_TX);
                    send_first_ecm_frame(s);
                }
                break;
            case T30_PRI_EOM:
            case T30_EOM:
            case T30_EOS:
                tx_end_page(s);
                if (s->phase_d_handler)
                    s->phase_d_handler(s->phase_d_user_data, fcf);
                terminate_operation_in_progress(s);
                report_tx_result(s, true);
                return_to_phase_b(s, false);
                break;
            case T30_PRI_EOP:
            case T30_EOP:
                tx_end_page(s);
                if (s->phase_d_handler)
                    s->phase_d_handler(s->phase_d_user_data, fcf);
                terminate_operation_in_progress(s);
                send_dcn(s);
                report_tx_result(s, true);
                break;
            }
        }
        break;
    case T30_PPR:
        process_rx_ppr(s, msg, len);
        break;
    case T30_RNR:
        if (s->timer_t5 == 0)
            s->timer_t5 = ms_to_samples(DEFAULT_TIMER_T5);
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_IV_PPS_RNR);
        send_rr(s);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_TX_BADPG);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        t30_set_status(s, T30_ERR_TX_ECMPHD);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iv_pps_q(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_PIP:
        if (s->remote_interrupts_allowed)
        {
            s->retries = 0;
            if (s->phase_d_handler)
            {
                s->phase_d_handler(s->phase_d_user_data, fcf);
                s->timer_t3 = ms_to_samples(DEFAULT_TIMER_T3);
            }
        }
        /* Fall through */
    case T30_MCF:
        s->retries = 0;
        s->timer_t5 = 0;
        /* Is there more of the current page to get, or do we move on? */
        span_log(&s->logging, SPAN_LOG_FLOW, "Is there more to send? - %d %d\n", s->ecm_frames, s->ecm_len[255]);
        if (!s->ecm_at_page_end  &&  get_partial_ecm_page(s) > 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Additional image data to send\n");
            s->ecm_block++;
            set_state(s, T30_STATE_IV);
            queue_phase(s, T30_PHASE_C_ECM_TX);
            send_first_ecm_frame(s);
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Moving on to the next page\n");
            switch (s->next_tx_step)
            {
            case T30_PRI_MPS:
            case T30_MPS:
                tx_end_page(s);
                if (s->phase_d_handler)
                    s->phase_d_handler(s->phase_d_user_data, fcf);
                if (tx_start_page(s))
                {
                    /* TODO: recover */
                    break;
                }
                if (get_partial_ecm_page(s) > 0)
                {
                    set_state(s, T30_STATE_IV);
                    queue_phase(s, T30_PHASE_C_ECM_TX);
                    send_first_ecm_frame(s);
                }
                break;
            case T30_PRI_EOM:
            case T30_EOM:
            case T30_EOS:
                tx_end_page(s);
                if (s->phase_d_handler)
                    s->phase_d_handler(s->phase_d_user_data, fcf);
                terminate_operation_in_progress(s);
                report_tx_result(s, true);
                return_to_phase_b(s, false);
                break;
            case T30_PRI_EOP:
            case T30_EOP:
                tx_end_page(s);
                if (s->phase_d_handler)
                    s->phase_d_handler(s->phase_d_user_data, fcf);
                terminate_operation_in_progress(s);
                send_dcn(s);
                report_tx_result(s, true);
                break;
            }
        }
        break;
    case T30_RNR:
        if (s->timer_t5 == 0)
            s->timer_t5 = ms_to_samples(DEFAULT_TIMER_T5);
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_IV_PPS_RNR);
        send_rr(s);
        break;
    case T30_PPR:
        process_rx_ppr(s, msg, len);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_TX_BADPG);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    case T30_PIN:
        if (s->remote_interrupts_allowed)
        {
            s->retries = 0;
            if (s->phase_d_handler)
            {
                s->phase_d_handler(s->phase_d_user_data, fcf);
                s->timer_t3 = ms_to_samples(DEFAULT_TIMER_T3);
            }
        }
        /* Fall through */
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        t30_set_status(s, T30_ERR_TX_ECMPHD);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iv_pps_rnr(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_PIP:
        if (s->remote_interrupts_allowed)
        {
            s->retries = 0;
            if (s->phase_d_handler)
            {
                s->phase_d_handler(s->phase_d_user_data, fcf);
                s->timer_t3 = ms_to_samples(DEFAULT_TIMER_T3);
            }
        }
        /* Fall through */
    case T30_MCF:
        s->retries = 0;
        s->timer_t5 = 0;
        /* Is there more of the current page to get, or do we move on? */
        span_log(&s->logging, SPAN_LOG_FLOW, "Is there more to send? - %d %d\n", s->ecm_frames, s->ecm_len[255]);
        if (!s->ecm_at_page_end  &&  get_partial_ecm_page(s) > 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Additional image data to send\n");
            s->ecm_block++;
            set_state(s, T30_STATE_IV);
            queue_phase(s, T30_PHASE_C_ECM_TX);
            send_first_ecm_frame(s);
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Moving on to the next page\n");
            switch (s->next_tx_step)
            {
            case T30_PRI_MPS:
            case T30_MPS:
                tx_end_page(s);
                if (s->phase_d_handler)
                    s->phase_d_handler(s->phase_d_user_data, fcf);
                if (tx_start_page(s))
                {
                    /* TODO: recover */
                    break;
                }
                if (get_partial_ecm_page(s) > 0)
                {
                    set_state(s, T30_STATE_IV);
                    queue_phase(s, T30_PHASE_C_ECM_TX);
                    send_first_ecm_frame(s);
                }
                break;
            case T30_PRI_EOM:
            case T30_EOM:
            case T30_EOS:
                tx_end_page(s);
                if (s->phase_d_handler)
                    s->phase_d_handler(s->phase_d_user_data, fcf);
                terminate_operation_in_progress(s);
                report_tx_result(s, true);
                return_to_phase_b(s, false);
                break;
            case T30_PRI_EOP:
            case T30_EOP:
                tx_end_page(s);
                if (s->phase_d_handler)
                    s->phase_d_handler(s->phase_d_user_data, fcf);
                terminate_operation_in_progress(s);
                send_dcn(s);
                report_tx_result(s, true);
                break;
            }
        }
        break;
    case T30_RNR:
        if (s->timer_t5 == 0)
            s->timer_t5 = ms_to_samples(DEFAULT_TIMER_T5);
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_IV_PPS_RNR);
        send_rr(s);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_RX_DCNRRD);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    case T30_PIN:
        if (s->remote_interrupts_allowed)
        {
            s->retries = 0;
            if (s->phase_d_handler)
            {
                s->phase_d_handler(s->phase_d_user_data, fcf);
                s->timer_t3 = ms_to_samples(DEFAULT_TIMER_T3);
            }
        }
        /* Fall through */
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iv_ctc(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_CTR:
        /* Valid response to a CTC received */
        /* T.30 says we change back to long training here */
        s->short_train = false;
        /* Initiate resending of the remainder of the frames. */
        set_state(s, T30_STATE_IV);
        queue_phase(s, T30_PHASE_C_ECM_TX);
        send_first_ecm_frame(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iv_eor(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_RNR:
        if (s->timer_t5 == 0)
            s->timer_t5 = ms_to_samples(DEFAULT_TIMER_T5);
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_IV_EOR_RNR);
        send_rr(s);
        break;
    case T30_ERR:
        /* TODO: Continue with the next message if MPS or EOM? */
        t30_set_status(s, T30_ERR_RETRYDCN);
        s->timer_t5 = 0;
        send_dcn(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    case T30_PIN:
        if (s->remote_interrupts_allowed)
        {
            s->retries = 0;
            if (s->phase_d_handler)
            {
                s->phase_d_handler(s->phase_d_user_data, fcf);
                s->timer_t3 = ms_to_samples(DEFAULT_TIMER_T3);
            }
        }
        /* Fall through */
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_iv_eor_rnr(t30_state_t *s, const uint8_t *msg, int len)
{
    uint8_t fcf;

    fcf = msg[2] & 0xFE;
    switch (fcf)
    {
    case T30_RNR:
        if (s->timer_t5 == 0)
            s->timer_t5 = ms_to_samples(DEFAULT_TIMER_T5);
        queue_phase(s, T30_PHASE_D_TX);
        set_state(s, T30_STATE_IV_EOR_RNR);
        send_rr(s);
        break;
    case T30_ERR:
        /* TODO: Continue with the next message if MPS or EOM? */
        t30_set_status(s, T30_ERR_RETRYDCN);
        s->timer_t5 = 0;
        send_dcn(s);
        break;
    case T30_DCN:
        t30_set_status(s, T30_ERR_RX_DCNRRD);
        disconnect(s);
        break;
    case T30_CRP:
        repeat_last_command(s);
        break;
    case T30_FNV:
        process_rx_fnv(s, msg, len);
        break;
    case T30_PIN:
        if (s->remote_interrupts_allowed)
        {
            s->retries = 0;
            if (s->phase_d_handler)
            {
                s->phase_d_handler(s->phase_d_user_data, fcf);
                s->timer_t3 = ms_to_samples(DEFAULT_TIMER_T3);
            }
        }
        /* Fall through */
    default:
        /* We don't know what to do with this. */
        unexpected_final_frame(s, msg, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_state_call_finished(t30_state_t *s, const uint8_t *msg, int len)
{
    /* Simply ignore anything which comes in when we have declared the call
       to have finished. */
}
/*- End of function --------------------------------------------------------*/

static void process_rx_control_msg(t30_state_t *s, const uint8_t *msg, int len)
{
    /* We should only get good frames here. */
    print_frame(s, "Rx: ", msg, len);
    if (s->real_time_frame_handler)
        s->real_time_frame_handler(s->real_time_frame_user_data, true, msg, len);

    if ((msg[1] & 0x10) == 0)
    {
        /* This is not a final frame */
        /* It seems we should not restart the command or response timer when exchanging HDLC image
           data. If the modem looses sync in the middle of the image, we should just wait until
           the carrier goes away before proceeding. */
        if (s->phase != T30_PHASE_C_ECM_RX)
        {
            /* Restart the command or response timer, T2 or T4 */
            switch (s->timer_t2_t4_is)
            {
            case TIMER_IS_T1A:
            case TIMER_IS_T2:
            case TIMER_IS_T2A:
            case TIMER_IS_T2B:
                timer_t2a_start(s);
                break;
            case TIMER_IS_T4:
            case TIMER_IS_T4A:
            case TIMER_IS_T4B:
                timer_t4a_start(s);
                break;
            }
        }
        /* The following handles all the message types we expect to get without
           a final frame tag. If we get one that T.30 says we should not expect
           in a particular context, its pretty harmless, so don't worry. */
        switch (msg[2] & 0xFE)
        {
        case (T30_CSI & 0xFE):
            /* Called subscriber identification or Calling subscriber identification (T30_CIG) */
            /* OK in (NSF) (CSI) DIS */
            /* OK in (NSC) (CIG) DTC */
            /* OK in (PWD) (SEP) (CIG) DTC */
            decode_20digit_msg(s, s->rx_info.ident, &msg[2], len - 2);
            break;
        case (T30_NSF & 0xFE):
            if (msg[2] == T30_NSF)
            {
                /* Non-standard facilities */
                /* OK in (NSF) (CSI) DIS */
                t35_decode(&msg[3], len - 3, &s->country, &s->vendor, &s->model);
                if (s->country)
                    span_log(&s->logging, SPAN_LOG_FLOW, "The remote was made in '%s'\n", s->country);
                if (s->vendor)
                    span_log(&s->logging, SPAN_LOG_FLOW, "The remote was made by '%s'\n", s->vendor);
                if (s->model)
                    span_log(&s->logging, SPAN_LOG_FLOW, "The remote is a '%s'\n", s->model);
                s->rx_info.nsf_len = decode_nsf_nss_nsc(s, &s->rx_info.nsf, &msg[2], len - 2);
            }
            else
            {
                /* NSC - Non-standard facilities command */
                /* OK in (NSC) (CIG) DTC */
                s->rx_info.nsc_len = decode_nsf_nss_nsc(s, &s->rx_info.nsc, &msg[2], len - 2);
            }
            break;
        case (T30_PWD & 0xFE):
            if (msg[2] == T30_PWD)
            {
                /* Password */
                /* OK in (SUB) (SID) (SEP) (PWD) (TSI) DCS */
                /* OK in (SUB) (SID) (SEP) (PWD) (CIG) DTC */
                decode_20digit_msg(s, s->rx_info.password, &msg[2], len - 2);
            }
            else
            {
                unexpected_non_final_frame(s, msg, len);
            }
            break;
        case (T30_SEP & 0xFE):
            if (msg[2] == T30_SEP)
            {
                /* Selective polling address */
                /* OK in (PWD) (SEP) (CIG) DTC */
                decode_20digit_msg(s, s->rx_info.selective_polling_address, &msg[2], len - 2);
            }
            else
            {
                unexpected_non_final_frame(s, msg, len);
            }
            break;
        case (T30_PSA & 0xFE):
            if (msg[2] == T30_PSA)
            {
                /* Polled sub-address */
                decode_20digit_msg(s, s->rx_info.polled_sub_address, &msg[2], len - 2);
            }
            else
            {
                unexpected_non_final_frame(s, msg, len);
            }
            break;
        case (T30_CIA & 0xFE):
            if (msg[2] == T30_CIA)
            {
                /* Calling subscriber internet address */
                decode_url_msg(s, NULL, &msg[2], len - 2);
            }
            else
            {
                unexpected_non_final_frame(s, msg, len);
            }
            break;
        case (T30_ISP & 0xFE):
            if (msg[2] == T30_ISP)
            {
                /* Internet selective polling address */
                decode_url_msg(s, NULL, &msg[2], len - 2);
            }
            else
            {
                unexpected_non_final_frame(s, msg, len);
            }
            break;
        case (T30_TSI & 0xFE):
            /* Transmitting subscriber identity */
            /* OK in (PWD) (SUB) (TSI) DCS */
            decode_20digit_msg(s, s->rx_info.ident, &msg[2], len - 2);
            break;
        case (T30_NSS & 0xFE):
            /* Non-standard facilities set-up */
            s->rx_info.nss_len = decode_nsf_nss_nsc(s, &s->rx_info.nss, &msg[2], len - 2);
            break;
        case (T30_SUB & 0xFE):
            /* Sub-address */
            /* OK in (PWD) (SUB) (TSI) DCS */
            decode_20digit_msg(s, s->rx_info.sub_address, &msg[2], len - 2);
            break;
        case (T30_SID & 0xFE):
            /* Sender Identification */
            /* OK in (SUB) (SID) (SEP) (PWD) (TSI) DCS */
            /* OK in (SUB) (SID) (SEP) (PWD) (CIG) DTC */
            decode_20digit_msg(s, s->rx_info.sender_ident, &msg[2], len - 2);
            break;
        case (T30_CSA & 0xFE):
            /* Calling subscriber internet address */
            decode_url_msg(s, NULL, &msg[2], len - 2);
            break;
        case (T30_TSA & 0xFE):
            /* Transmitting subscriber internet address */
            decode_url_msg(s, NULL, &msg[2], len - 2);
            break;
        case (T30_IRA & 0xFE):
            /* Internet routing address */
            decode_url_msg(s, NULL, &msg[2], len - 2);
            break;
        case T4_FCD:
            process_rx_fcd(s, msg, len);
            break;
        case T4_RCP:
            process_rx_rcp(s, msg, len);
            break;
        default:
            unexpected_non_final_frame(s, msg, len);
            break;
        }
    }
    else
    {
        /* This is a final frame */
        /* Once we have any successful message from the far end, we
           cancel timer T1 */
        s->timer_t0_t1 = 0;

        /* The following handles context sensitive message types, which should
           occur at the end of message sequences. They should, therefore have
           the final frame flag set. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Rx final frame in state %s\n", state_names[s->state]);

        switch (s->state)
        {
        case T30_STATE_ANSWERING:
            process_state_answering(s, msg, len);
            break;
        case T30_STATE_B:
            process_state_b(s, msg, len);
            break;
        case T30_STATE_C:
            process_state_c(s, msg, len);
            break;
        case T30_STATE_D:
            process_state_d(s, msg, len);
            break;
        case T30_STATE_D_TCF:
            process_state_d_tcf(s, msg, len);
            break;
        case T30_STATE_D_POST_TCF:
            process_state_d_post_tcf(s, msg, len);
            break;
        case T30_STATE_F_TCF:
            process_state_f_tcf(s, msg, len);
            break;
        case T30_STATE_F_CFR:
            process_state_f_cfr(s, msg, len);
            break;
        case T30_STATE_F_FTT:
            process_state_f_ftt(s, msg, len);
            break;
        case T30_STATE_F_DOC_NON_ECM:
            process_state_f_doc_non_ecm(s, msg, len);
            break;
        case T30_STATE_F_POST_DOC_NON_ECM:
            process_state_f_post_doc_non_ecm(s, msg, len);
            break;
        case T30_STATE_F_DOC_ECM:
        case T30_STATE_F_POST_DOC_ECM:
            process_state_f_doc_and_post_doc_ecm(s, msg, len);
            break;
        case T30_STATE_F_POST_RCP_MCF:
            process_state_f_post_rcp_mcf(s, msg, len);
            break;
        case T30_STATE_F_POST_RCP_PPR:
            process_state_f_post_rcp_ppr(s, msg, len);
            break;
        case T30_STATE_F_POST_RCP_RNR:
            process_state_f_post_rcp_rnr(s, msg, len);
            break;
        case T30_STATE_R:
            process_state_r(s, msg, len);
            break;
        case T30_STATE_T:
            process_state_t(s, msg, len);
            break;
        case T30_STATE_I:
            process_state_i(s, msg, len);
            break;
        case T30_STATE_II:
            process_state_ii(s, msg, len);
            break;
        case T30_STATE_II_Q:
            process_state_ii_q(s, msg, len);
            break;
        case T30_STATE_III_Q_MCF:
            process_state_iii_q_mcf(s, msg, len);
            break;
        case T30_STATE_III_Q_RTP:
            process_state_iii_q_rtp(s, msg, len);
            break;
        case T30_STATE_III_Q_RTN:
            process_state_iii_q_rtn(s, msg, len);
            break;
        case T30_STATE_IV:
            process_state_iv(s, msg, len);
            break;
        case T30_STATE_IV_PPS_NULL:
            process_state_iv_pps_null(s, msg, len);
            break;
        case T30_STATE_IV_PPS_Q:
            process_state_iv_pps_q(s, msg, len);
            break;
        case T30_STATE_IV_PPS_RNR:
            process_state_iv_pps_rnr(s, msg, len);
            break;
        case T30_STATE_IV_CTC:
            process_state_iv_ctc(s, msg, len);
            break;
        case T30_STATE_IV_EOR:
            process_state_iv_eor(s, msg, len);
            break;
        case T30_STATE_IV_EOR_RNR:
            process_state_iv_eor_rnr(s, msg, len);
            break;
        case T30_STATE_CALL_FINISHED:
            process_state_call_finished(s, msg, len);
            break;
        default:
            /* We don't know what to do with this. */
            unexpected_final_frame(s, msg, len);
            break;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void queue_phase(t30_state_t *s, int phase)
{
    if (s->rx_signal_present)
    {
        /* We need to wait for that signal to go away */
        if (s->next_phase != T30_PHASE_IDLE)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Flushing queued phase %s\n", phase_names[s->next_phase]);
            /* Ensure nothing has been left in the queue that was scheduled to go out in the previous next
               phase */
            if (s->send_hdlc_handler)
                s->send_hdlc_handler(s->send_hdlc_user_data, NULL, -1);
        }
        s->next_phase = phase;
    }
    else
    {
        /* We don't need to queue the new phase. We can change to it immediately. */
        set_phase(s, phase);
    }
}
/*- End of function --------------------------------------------------------*/

static void set_phase(t30_state_t *s, int phase)
{
    if (phase != s->next_phase  &&  s->next_phase != T30_PHASE_IDLE)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Flushing queued phase %s\n", phase_names[s->next_phase]);
        /* Ensure nothing has been left in the queue that was scheduled to go out in the previous next
           phase */
        if (s->send_hdlc_handler)
            s->send_hdlc_handler(s->send_hdlc_user_data, NULL, -1);
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "Changing from phase %s to %s\n", phase_names[s->phase], phase_names[phase]);
    /* We may be killing a receiver before it has declared the end of the
       signal. Force the signal present indicator to off, because the
       receiver will never be able to. */
    if (s->phase != T30_PHASE_A_CED  &&  s->phase != T30_PHASE_A_CNG)
        s->rx_signal_present = false;
    s->rx_trained = false;
    s->rx_frame_received = false;
    s->phase = phase;
    s->next_phase = T30_PHASE_IDLE;
    switch (phase)
    {
    case T30_PHASE_A_CED:
        if (s->set_rx_type_handler)
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_V21, 300, false, true);
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, T30_MODEM_CED, 0, false, false);
        break;
    case T30_PHASE_A_CNG:
        if (s->set_rx_type_handler)
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_V21, 300, false, true);
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, T30_MODEM_CNG, 0, false, false);
        break;
    case T30_PHASE_B_RX:
    case T30_PHASE_D_RX:
        if (s->set_rx_type_handler)
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_V21, 300, false, true);
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, T30_MODEM_NONE, 0, false, false);
        break;
    case T30_PHASE_B_TX:
    case T30_PHASE_D_TX:
        if (!s->far_end_detected  &&  s->timer_t0_t1 > 0)
        {
            s->timer_t0_t1 = ms_to_samples(DEFAULT_TIMER_T1);
            s->far_end_detected = true;
        }
        if (s->set_rx_type_handler)
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_NONE, 0, false, false);
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, T30_MODEM_V21, 300, false, true);
        break;
    case T30_PHASE_C_NON_ECM_RX:
        if (s->set_rx_type_handler)
        {
            /* Momentarily stop the receive modem, so the next change is forced to happen. If we don't do this
               an HDLC message on the slow modem, which has disabled the fast modem, will prevent the same
               fast modem from restarting. */
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_NONE, 0, false, false);
            s->set_rx_type_handler(s->set_rx_type_user_data, fallback_sequence[s->current_fallback].modem_type, fallback_sequence[s->current_fallback].bit_rate, s->short_train, false);
        }
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, T30_MODEM_NONE, 0, false, false);
        break;
    case T30_PHASE_C_NON_ECM_TX:
        /* Pause before switching from anything to phase C */
        /* Always prime the training count for 1.5s of data at the current rate. Its harmless if
           we prime it and are not doing TCF. */
        s->tcf_test_bits = (3*fallback_sequence[s->current_fallback].bit_rate)/2;
        if (s->set_rx_type_handler)
        {
            /* Momentarily stop the receive modem, so the next change is forced to happen. If we don't do this
               an HDLC message on the slow modem, which has disabled the fast modem, will prevent the same
               fast modem from restarting. */
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_NONE, 0, false, false);
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_NONE, 0, false, false);
        }
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, fallback_sequence[s->current_fallback].modem_type, fallback_sequence[s->current_fallback].bit_rate, s->short_train, false);
        break;
    case T30_PHASE_C_ECM_RX:
        if (s->set_rx_type_handler)
            s->set_rx_type_handler(s->set_rx_type_user_data, fallback_sequence[s->current_fallback].modem_type, fallback_sequence[s->current_fallback].bit_rate, s->short_train, true);
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, T30_MODEM_NONE, 0, false, false);
        break;
    case T30_PHASE_C_ECM_TX:
        /* Pause before switching from anything to phase C */
        if (s->set_rx_type_handler)
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_NONE, 0, false, false);
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, fallback_sequence[s->current_fallback].modem_type, fallback_sequence[s->current_fallback].bit_rate, s->short_train, true);
        break;
    case T30_PHASE_E:
        /* Send a little silence before ending things, to ensure the
           buffers are all flushed through, and the far end has seen
           the last message we sent. */
        s->tcf_test_bits = 0;
        s->tcf_current_zeros = 0;
        s->tcf_most_zeros = 0;
        if (s->set_rx_type_handler)
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_NONE, 0, false, false);
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, T30_MODEM_PAUSE, 0, FINAL_FLUSH_TIME, false);
        break;
    case T30_PHASE_CALL_FINISHED:
        if (s->set_rx_type_handler)
            s->set_rx_type_handler(s->set_rx_type_user_data, T30_MODEM_DONE, 0, false, false);
        if (s->set_tx_type_handler)
            s->set_tx_type_handler(s->set_tx_type_user_data, T30_MODEM_DONE, 0, false, false);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void set_state(t30_state_t *s, int state)
{
    if (s->state != state)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Changing from state %s to %s\n", state_names[s->state], state_names[state]);
        s->state = state;
    }
    s->step = 0;
}
/*- End of function --------------------------------------------------------*/

static void repeat_last_command(t30_state_t *s)
{
    s->step = 0;
    if (++s->retries >= MAX_COMMAND_TRIES)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Too many retries. Giving up.\n");
        switch (s->state)
        {
        case T30_STATE_D_POST_TCF:
            /* Received no response to DCS or TCF */
            t30_set_status(s, T30_ERR_TX_PHBDEAD);
            break;
        case T30_STATE_II_Q:
        case T30_STATE_IV_PPS_NULL:
        case T30_STATE_IV_PPS_Q:
            /* No response after sending a page */
            t30_set_status(s, T30_ERR_TX_PHDDEAD);
            break;
        default:
            /* Disconnected after permitted retries */
            t30_set_status(s, T30_ERR_RETRYDCN);
            break;
        }
        send_dcn(s);
        return;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "Retry number %d\n", s->retries);
    switch (s->state)
    {
    case T30_STATE_R:
        s->dis_received = false;
        queue_phase(s, T30_PHASE_B_TX);
        send_dis_or_dtc_sequence(s, true);
        break;
    case T30_STATE_III_Q_MCF:
        queue_phase(s, T30_PHASE_D_TX);
        send_simple_frame(s, T30_MCF);
        break;
    case T30_STATE_III_Q_RTP:
        queue_phase(s, T30_PHASE_D_TX);
        send_simple_frame(s, T30_RTP);
        break;
    case T30_STATE_III_Q_RTN:
        queue_phase(s, T30_PHASE_D_TX);
        send_simple_frame(s, T30_RTN);
        break;
    case T30_STATE_II_Q:
        queue_phase(s, T30_PHASE_D_TX);
        send_simple_frame(s, s->next_tx_step);
        break;
    case T30_STATE_IV_PPS_NULL:
    case T30_STATE_IV_PPS_Q:
        queue_phase(s, T30_PHASE_D_TX);
        send_pps_frame(s);
        break;
    case T30_STATE_IV_PPS_RNR:
    case T30_STATE_IV_EOR_RNR:
        queue_phase(s, T30_PHASE_D_TX);
        send_rr(s);
        break;
    case T30_STATE_D:
        queue_phase(s, T30_PHASE_B_TX);
        send_dcs_sequence(s, true);
        break;
    case T30_STATE_F_FTT:
        queue_phase(s, T30_PHASE_B_TX);
        send_simple_frame(s, T30_FTT);
        break;
    case T30_STATE_F_CFR:
        queue_phase(s, T30_PHASE_B_TX);
        send_cfr_sequence(s, true);
        break;
    case T30_STATE_D_POST_TCF:
        /* Need to send the whole training thing again */
        s->short_train = false;
        queue_phase(s, T30_PHASE_B_TX);
        send_dcs_sequence(s, true);
        break;
    case T30_STATE_F_POST_RCP_RNR:
        /* Just ignore */
        break;
    default:
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "Repeat command called with nothing to repeat - phase %s, state %s\n",
                 phase_names[s->phase],
                 state_names[s->state]);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void timer_t2_start(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Start T2\n");
    s->timer_t2_t4 = ms_to_samples(DEFAULT_TIMER_T2);
    s->timer_t2_t4_is = TIMER_IS_T2;
}
/*- End of function --------------------------------------------------------*/

static void timer_t2a_start(t30_state_t *s)
{
    /* T.30 Annex A says timeout T1 should be used in ECM phase C to time out the
       first frame after the flags start. This seems a strange reuse of the name T1
       for a different purpose, but there it is. We distinguish it by calling it T1A. */
    if (s->phase == T30_PHASE_C_ECM_RX)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Start T1A\n");
        s->timer_t2_t4 = ms_to_samples(DEFAULT_TIMER_T1A);
        s->timer_t2_t4_is = TIMER_IS_T1A;
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Start T2A\n");
        s->timer_t2_t4 = ms_to_samples(DEFAULT_TIMER_T2A);
        s->timer_t2_t4_is = TIMER_IS_T2A;
    }
}
/*- End of function --------------------------------------------------------*/

static void timer_t2b_start(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Start T2B\n");
    s->timer_t2_t4 = ms_to_samples(DEFAULT_TIMER_T2B);
    s->timer_t2_t4_is = TIMER_IS_T2B;
}
/*- End of function --------------------------------------------------------*/

static void timer_t4_start(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Start T4\n");
    s->timer_t2_t4 = ms_to_samples(DEFAULT_TIMER_T4);
    s->timer_t2_t4_is = TIMER_IS_T4;
}
/*- End of function --------------------------------------------------------*/

static void timer_t4a_start(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Start T4A\n");
    s->timer_t2_t4 = ms_to_samples(DEFAULT_TIMER_T4A);
    s->timer_t2_t4_is = TIMER_IS_T4A;
}
/*- End of function --------------------------------------------------------*/

static void timer_t4b_start(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Start T4B\n");
    s->timer_t2_t4 = ms_to_samples(DEFAULT_TIMER_T4B);
    s->timer_t2_t4_is = TIMER_IS_T4B;
}
/*- End of function --------------------------------------------------------*/

static void timer_t2_t4_stop(t30_state_t *s)
{
    const char *tag;

    switch (s->timer_t2_t4_is)
    {
    case TIMER_IS_IDLE:
        tag = "none";
        break;
    case TIMER_IS_T1A:
        tag = "T1A";
        break;
    case TIMER_IS_T2:
        tag = "T2";
        break;
    case TIMER_IS_T2A:
        tag = "T2A";
        break;
    case TIMER_IS_T2B:
        tag = "T2B";
        break;
    case TIMER_IS_T2C:
        tag = "T2C";
        break;
    case TIMER_IS_T4:
        tag = "T4";
        break;
    case TIMER_IS_T4A:
        tag = "T4A";
        break;
    case TIMER_IS_T4B:
        tag = "T4B";
        break;
    case TIMER_IS_T4C:
        tag = "T4C";
        break;
    default:
        tag = "T2/T4";
        break;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "Stop %s (%d remaining)\n", tag, s->timer_t2_t4);
    s->timer_t2_t4 = 0;
    s->timer_t2_t4_is = TIMER_IS_IDLE;
}
/*- End of function --------------------------------------------------------*/

static void timer_t0_expired(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "T0 expired in state %s\n", state_names[s->state]);
    t30_set_status(s, T30_ERR_T0_EXPIRED);
    /* Just end the call */
    disconnect(s);
}
/*- End of function --------------------------------------------------------*/

static void timer_t1_expired(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "T1 expired in state %s\n", state_names[s->state]);
    /* The initial connection establishment has timeout out. In other words, we
       have been unable to communicate successfully with a remote machine.
       It is time to abandon the call. */
    t30_set_status(s, T30_ERR_T1_EXPIRED);
    switch (s->state)
    {
    case T30_STATE_T:
        /* Just end the call */
        disconnect(s);
        break;
    case T30_STATE_R:
        /* Send disconnect, and then end the call. Since we have not
           successfully contacted the far end, it is unclear why we should
           send a disconnect message at this point. However, it is what T.30
           says we should do. */
        send_dcn(s);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void timer_t1a_expired(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "T1A expired in phase %s, state %s. An HDLC frame lasted too long.\n", phase_names[s->phase], state_names[s->state]);
    t30_set_status(s, T30_ERR_HDLC_CARRIER);
    disconnect(s);
}
/*- End of function --------------------------------------------------------*/

static void timer_t2_expired(t30_state_t *s)
{
    if (s->timer_t2_t4_is != TIMER_IS_T2B)
        span_log(&s->logging, SPAN_LOG_FLOW, "T2 expired in phase %s, state %s\n", phase_names[s->phase], state_names[s->state]);
    switch (s->state)
    {
    case T30_STATE_III_Q_MCF:
    case T30_STATE_III_Q_RTP:
    case T30_STATE_III_Q_RTN:
    case T30_STATE_F_POST_RCP_PPR:
    case T30_STATE_F_POST_RCP_MCF:
        switch (s->next_rx_step)
        {
        case T30_PRI_EOM:
        case T30_EOM:
        case T30_EOS:
            /* We didn't receive a response to our T30_MCF after T30_EOM, so we must be OK
               to proceed to phase B, and pretty much act like its the beginning of a call. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Returning to phase B after %s\n", t30_frametype(s->next_rx_step));
            set_phase(s, T30_PHASE_B_TX);
            timer_t2_start(s);
            s->dis_received = false;
            send_dis_or_dtc_sequence(s, true);
            return;
        }
        break;
    case T30_STATE_F_TCF:
        span_log(&s->logging, SPAN_LOG_FLOW, "No TCF data received\n");
        set_phase(s, T30_PHASE_B_TX);
        set_state(s, T30_STATE_F_FTT);
        send_simple_frame(s, T30_FTT);
        return;
    case T30_STATE_F_DOC_ECM:
    case T30_STATE_F_DOC_NON_ECM:
        /* While waiting for FAX page */
        t30_set_status(s, T30_ERR_RX_T2EXPFAX);
        break;
    case T30_STATE_F_POST_DOC_ECM:
    case T30_STATE_F_POST_DOC_NON_ECM:
        /* While waiting for next FAX page */
        /* Figure 5-2b/T.30 and note 7 says we should allow 1 to 3 tries at this point.
           The way we work now is effectively hard coding a 1 try limit */
        t30_set_status(s, T30_ERR_RX_T2EXPMPS);
        break;
#if 0
    case ??????:
        /* While waiting for DCN */
        t30_set_status(s, T30_ERR_RX_T2EXPDCN);
        break;
    case ??????:
        /* While waiting for phase D */
        t30_set_status(s, T30_ERR_RX_T2EXPD);
        break;
#endif
    case T30_STATE_IV_PPS_RNR:
    case T30_STATE_IV_EOR_RNR:
        /* While waiting for RR command */
        t30_set_status(s, T30_ERR_RX_T2EXPRR);
        break;
    case T30_STATE_R:
        /* While waiting for NSS, DCS or MCF */
        t30_set_status(s, T30_ERR_RX_T2EXP);
        break;
    case T30_STATE_F_FTT:
        break;
    }
    queue_phase(s, T30_PHASE_B_TX);
    start_receiving_document(s);
}
/*- End of function --------------------------------------------------------*/

static void timer_t2a_expired(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "T2A expired in phase %s, state %s. An HDLC frame lasted too long.\n", phase_names[s->phase], state_names[s->state]);
    t30_set_status(s, T30_ERR_HDLC_CARRIER);
    disconnect(s);
}
/*- End of function --------------------------------------------------------*/

static void timer_t2b_expired(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "T2B expired in phase %s, state %s. The line is now quiet.\n", phase_names[s->phase], state_names[s->state]);
    timer_t2_expired(s);
}
/*- End of function --------------------------------------------------------*/

static void timer_t3_expired(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "T3 expired in phase %s, state %s\n", phase_names[s->phase], state_names[s->state]);
    t30_set_status(s, T30_ERR_T3_EXPIRED);
    disconnect(s);
}
/*- End of function --------------------------------------------------------*/

static void timer_t4_expired(t30_state_t *s)
{
    /* There was no response (or only a corrupt response) to a command,
       within the T4 timeout period. */
    span_log(&s->logging, SPAN_LOG_FLOW, "T4 expired in phase %s, state %s\n", phase_names[s->phase], state_names[s->state]);
    /* Of course, things might just be a little late, especially if there are T.38
       links in the path. There is no point in simply timing out, and resending,
       if we are currently receiving something from the far end - its a half-duplex
       path, so the two transmissions will conflict. Our best strategy is to wait
       until there is nothing being received, or give up after a long backstop timeout.
       In the meantime, if we get a meaningful, if somewhat delayed, response, we
       should accept it and carry on. */
    repeat_last_command(s);
}
/*- End of function --------------------------------------------------------*/

static void timer_t4a_expired(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "T4A expired in phase %s, state %s. An HDLC frame lasted too long.\n", phase_names[s->phase], state_names[s->state]);
    t30_set_status(s, T30_ERR_HDLC_CARRIER);
    disconnect(s);
}
/*- End of function --------------------------------------------------------*/

static void timer_t4b_expired(t30_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "T4B expired in phase %s, state %s. The line is now quiet.\n", phase_names[s->phase], state_names[s->state]);
    timer_t4_expired(s);
}
/*- End of function --------------------------------------------------------*/

static void timer_t5_expired(t30_state_t *s)
{
    /* Give up waiting for the receiver to become ready in error correction mode */
    span_log(&s->logging, SPAN_LOG_FLOW, "T5 expired in phase %s, state %s\n", phase_names[s->phase], state_names[s->state]);
    t30_set_status(s, T30_ERR_TX_T5EXP);
}
/*- End of function --------------------------------------------------------*/

static void decode_20digit_msg(t30_state_t *s, char *msg, const uint8_t *pkt, int len)
{
    int p;
    int k;
    char text[T30_MAX_IDENT_LEN + 1];

    if (msg == NULL)
        msg = text;
    if (len > T30_MAX_IDENT_LEN + 1)
    {
        unexpected_frame_length(s, pkt, len);
        msg[0] = '\0';
        return;
    }
    p = len;
    /* Strip trailing spaces */
    while (p > 1  &&  pkt[p - 1] == ' ')
        p--;
    /* The string is actually backwards in the message */
    k = 0;
    while (p > 1)
        msg[k++] = pkt[--p];
    msg[k] = '\0';
    span_log(&s->logging, SPAN_LOG_FLOW, "Remote gave %s as: \"%s\"\n", t30_frametype(pkt[0]), msg);
}
/*- End of function --------------------------------------------------------*/

static void decode_url_msg(t30_state_t *s, char *msg, const uint8_t *pkt, int len)
{
    char text[77 + 1];

    /* TODO: decode properly, as per T.30 5.3.6.2.12 */
    if (msg == NULL)
        msg = text;
    if (len < 3  ||  len > 77 + 3  ||  len != pkt[2] + 3)
    {
        unexpected_frame_length(s, pkt, len);
        msg[0] = '\0';
        return;
    }
    /* First octet is the sequence number of the packet.
            Bit 7 = 1 for more follows, 0 for last packet in the sequence.
            Bits 6-0 = The sequence number, 0 to 0x7F
       Second octet is the type of internet address.
            Bits 7-4 = reserved
            Bits 3-0 = type:
                    0 = reserved
                    1 = e-mail address
                    2 = URL
                    3 = TCP/IP V4
                    4 = TCP/IP V6
                    5 = international phone number, in the usual +... format
                    6-15 = reserved
       Third octet is the length of the internet address
            Bit 7 = 1 for more follows, 0 for last packet in the sequence.
            Bits 6-0 = length
     */
    memcpy(msg, &pkt[3], len - 3);
    msg[len - 3] = '\0';
    span_log(&s->logging, SPAN_LOG_FLOW, "Remote fax gave %s as: %d, %d, \"%s\"\n", t30_frametype(pkt[0]), pkt[0], pkt[1], msg);
}
/*- End of function --------------------------------------------------------*/

static int decode_nsf_nss_nsc(t30_state_t *s, uint8_t *msg[], const uint8_t *pkt, int len)
{
    uint8_t *t;

    if ((t = span_alloc(len - 1)) == NULL)
        return 0;
    memcpy(t, &pkt[1], len - 1);
    *msg = t;
    return len - 1;
}
/*- End of function --------------------------------------------------------*/

static void t30_non_ecm_rx_status(void *user_data, int status)
{
    t30_state_t *s;
    int was_trained;

    s = (t30_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM signal status is %s (%d) in state %s\n", signal_status_to_str(status), status, state_names[s->state]);
    switch (status)
    {
    case SIG_STATUS_TRAINING_IN_PROGRESS:
        break;
    case SIG_STATUS_TRAINING_FAILED:
        s->rx_trained = false;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        /* In case we are in trainability test mode... */
        s->tcf_test_bits = 0;
        s->tcf_current_zeros = 0;
        s->tcf_most_zeros = 0;
        s->rx_signal_present = true;
        s->rx_trained = true;
        timer_t2_t4_stop(s);
        break;
    case SIG_STATUS_CARRIER_UP:
        break;
    case SIG_STATUS_CARRIER_DOWN:
        was_trained = s->rx_trained;
        s->rx_signal_present = false;
        s->rx_trained = false;
        switch (s->state)
        {
        case T30_STATE_F_TCF:
            /* Only respond if we managed to actually sync up with the source. We don't
               want to respond just because we saw a click. These often occur just
               before the real signal, with many modems. Presumably this is due to switching
               within the far end modem. We also want to avoid the possibility of responding
               to the tail end of any slow modem signal. If there was a genuine data signal
               which we failed to train on it should not matter. If things are that bad, we
               do not stand much chance of good quality communications. */
            if (was_trained)
            {
                /* Although T.30 says the training test should be 1.5s of all 0's, some FAX
                   machines send a burst of all 1's before the all 0's. Tolerate this. */
                if (s->tcf_current_zeros > s->tcf_most_zeros)
                    s->tcf_most_zeros = s->tcf_current_zeros;
                span_log(&s->logging, SPAN_LOG_FLOW, "Trainability (TCF) test result - %d total bits. longest run of zeros was %d\n", s->tcf_test_bits, s->tcf_most_zeros);
                if (s->tcf_most_zeros < fallback_sequence[s->current_fallback].bit_rate)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Trainability (TCF) test failed - longest run of zeros was %d\n", s->tcf_most_zeros);
                    set_phase(s, T30_PHASE_B_TX);
                    set_state(s, T30_STATE_F_FTT);
                    send_simple_frame(s, T30_FTT);
                }
                else
                {
                    /* The training went OK */
                    s->short_train = true;
                    rx_start_page(s);
                    set_phase(s, T30_PHASE_B_TX);
                    set_state(s, T30_STATE_F_CFR);
                    send_cfr_sequence(s, true);
                }
            }
            break;
        case T30_STATE_F_POST_DOC_NON_ECM:
            /* Page ended cleanly */
            if (s->current_status == T30_ERR_RX_NOCARRIER)
                t30_set_status(s, T30_ERR_OK);
            break;
        default:
            /* We should be receiving a document right now, but it did not end cleanly. */
            if (was_trained)
            {
                span_log(&s->logging, SPAN_LOG_WARNING, "Page did not end cleanly\n");
                /* We trained OK, so we should have some kind of received page, even though
                   it did not end cleanly. */
                set_state(s, T30_STATE_F_POST_DOC_NON_ECM);
                set_phase(s, T30_PHASE_D_RX);
                timer_t2_start(s);
                if (s->current_status == T30_ERR_RX_NOCARRIER)
                    t30_set_status(s, T30_ERR_OK);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_WARNING, "Non-ECM carrier not found\n");
                t30_set_status(s, T30_ERR_RX_NOCARRIER);
            }
            break;
        }
        if (s->next_phase != T30_PHASE_IDLE)
            set_phase(s, s->next_phase);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected non-ECM rx status - %d!\n", status);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(void) t30_non_ecm_put_bit(void *user_data, int bit)
{
    t30_state_t *s;
    int res;

    if (bit < 0)
    {
        t30_non_ecm_rx_status(user_data, bit);
        return;
    }
    s = (t30_state_t *) user_data;
    switch (s->state)
    {
    case T30_STATE_F_TCF:
        /* Trainability test */
        s->tcf_test_bits++;
        if (bit)
        {
            if (s->tcf_current_zeros > s->tcf_most_zeros)
                s->tcf_most_zeros = s->tcf_current_zeros;
            s->tcf_current_zeros = 0;
        }
        else
        {
            s->tcf_current_zeros++;
        }
        break;
    case T30_STATE_F_DOC_NON_ECM:
        /* Image transfer */
        if ((res = t4_rx_put_bit(&s->t4.rx, bit)) != T4_DECODE_MORE_DATA)
        {
            /* This is the end of the image */
            if (res != T4_DECODE_OK)
                span_log(&s->logging, SPAN_LOG_FLOW, "Page ended with status %d\n", res);
            set_state(s, T30_STATE_F_POST_DOC_NON_ECM);
            queue_phase(s, T30_PHASE_D_RX);
            timer_t2_start(s);
        }
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_non_ecm_put(void *user_data, const uint8_t buf[], int len)
{
    t30_state_t *s;
    int i;
    int res;

    s = (t30_state_t *) user_data;
    switch (s->state)
    {
    case T30_STATE_F_TCF:
        /* Trainability test */
        /* This makes counting zeros fast, but approximate. That really doesn't matter */
        s->tcf_test_bits += 8*len;
        for (i = 0;  i < len;  i++)
        {
            if (buf[i])
            {
                if (s->tcf_current_zeros > s->tcf_most_zeros)
                    s->tcf_most_zeros = s->tcf_current_zeros;
                s->tcf_current_zeros = 0;
            }
            else
            {
                s->tcf_current_zeros += 8;
            }
        }
        break;
    case T30_STATE_F_DOC_NON_ECM:
        /* Image transfer */
        if ((res = t4_rx_put(&s->t4.rx, buf, len)) != T4_DECODE_MORE_DATA)
        {
            /* This is the end of the image */
            if (res != T4_DECODE_OK)
                span_log(&s->logging, SPAN_LOG_FLOW, "Page ended with status %d\n", res);
            set_state(s, T30_STATE_F_POST_DOC_NON_ECM);
            queue_phase(s, T30_PHASE_D_RX);
            timer_t2_start(s);
        }
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) t30_non_ecm_get_bit(void *user_data)
{
    int bit;
    t30_state_t *s;

    s = (t30_state_t *) user_data;
    switch (s->state)
    {
    case T30_STATE_D_TCF:
        /* Trainability test. */
        bit = 0;
        if (s->tcf_test_bits-- < 0)
        {
            /* Finished sending training test. */
            bit = SIG_STATUS_END_OF_DATA;
        }
        break;
    case T30_STATE_I:
        /* Transferring real data. */
        bit = t4_tx_get_bit(&s->t4.tx);
        break;
    case T30_STATE_D_POST_TCF:
    case T30_STATE_II_Q:
        /* We should be padding out a block of samples if we are here */
        bit = 0;
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "t30_non_ecm_get_bit in bad state %s\n", state_names[s->state]);
        bit = SIG_STATUS_END_OF_DATA;
        break;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_non_ecm_get(void *user_data, uint8_t buf[], int max_len)
{
    int len;
    t30_state_t *s;

    s = (t30_state_t *) user_data;
    switch (s->state)
    {
    case T30_STATE_D_TCF:
        /* Trainability test. */
        for (len = 0;  len < max_len;  len++)
        {
            buf[len] = 0;
            if ((s->tcf_test_bits -= 8) < 0)
                break;
        }
        break;
    case T30_STATE_I:
        /* Transferring real data. */
        len = t4_tx_get(&s->t4.tx, buf, max_len);
        break;
    case T30_STATE_D_POST_TCF:
    case T30_STATE_II_Q:
        /* We should be padding out a block of samples if we are here */
        len = 0;
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "t30_non_ecm_get in bad state %s\n", state_names[s->state]);
        len = -1;
        break;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static void t30_hdlc_rx_status(void *user_data, int status)
{
    t30_state_t *s;
    int was_trained;

    s = (t30_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "HDLC signal status is %s (%d) in state %s\n", signal_status_to_str(status), status, state_names[s->state]);
    switch (status)
    {
    case SIG_STATUS_TRAINING_IN_PROGRESS:
        break;
    case SIG_STATUS_TRAINING_FAILED:
        s->rx_trained = false;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        s->rx_signal_present = true;
        s->rx_trained = true;
        break;
    case SIG_STATUS_CARRIER_UP:
        s->rx_signal_present = true;
        switch (s->timer_t2_t4_is)
        {
        case TIMER_IS_T2B:
            timer_t2_t4_stop(s);
            s->timer_t2_t4_is = TIMER_IS_T2C;
            break;
        case TIMER_IS_T4B:
            timer_t2_t4_stop(s);
            s->timer_t2_t4_is = TIMER_IS_T4C;
            break;
        }
        break;
    case SIG_STATUS_CARRIER_DOWN:
        was_trained = s->rx_trained;
        s->rx_signal_present = false;
        s->rx_trained = false;
        /* If a phase change has been queued to occur after the receive signal drops,
           its time to change. */
        if (s->state == T30_STATE_F_DOC_ECM)
        {
            /* We should be receiving a document right now, but we haven't seen an RCP at the end of
               transmission. */
            if (was_trained)
            {
                /* We trained OK, so we should have some kind of received page, possibly with
                   zero good HDLC frames. It just did'nt end cleanly with an RCP. */
                span_log(&s->logging, SPAN_LOG_WARNING, "ECM signal did not end cleanly\n");
                /* Fake the existance of an RCP, and proceed */
                set_state(s, T30_STATE_F_POST_DOC_ECM);
                queue_phase(s, T30_PHASE_D_RX);
                timer_t2_start(s);
                /* We at least trained, so any missing carrier status is out of date */
                if (s->current_status == T30_ERR_RX_NOCARRIER)
                    t30_set_status(s, T30_ERR_OK);
            }
            else
            {
                /* Either there was no image carrier, or we failed to train to it. */
                span_log(&s->logging, SPAN_LOG_WARNING, "ECM carrier not found\n");
                t30_set_status(s, T30_ERR_RX_NOCARRIER);
            }
        }
        if (s->next_phase != T30_PHASE_IDLE)
        {
            /* The appropriate timer for the next phase should already be in progress */
            set_phase(s, s->next_phase);
        }
        else
        {
            switch (s->timer_t2_t4_is)
            {
            case TIMER_IS_T1A:
            case TIMER_IS_T2A:
            case TIMER_IS_T2C:
                timer_t2b_start(s);
                break;
            case TIMER_IS_T4A:
            case TIMER_IS_T4C:
                timer_t4b_start(s);
                break;
            }
        }
        break;
    case SIG_STATUS_FRAMING_OK:
        if (!s->far_end_detected  &&  s->timer_t0_t1 > 0)
        {
            s->timer_t0_t1 = ms_to_samples(DEFAULT_TIMER_T1);
            s->far_end_detected = true;
            if (s->phase == T30_PHASE_A_CED  ||  s->phase == T30_PHASE_A_CNG)
                set_phase(s, T30_PHASE_B_RX);
        }
        /* 5.4.3.1 Timer T2 is reset if flag is received. Timer T2A must be started. */
        /* Unstated, but implied, is that timer T4 and T4A are handled the same way. */
        if (s->timer_t2_t4 > 0)
        {
            switch(s->timer_t2_t4_is)
            {
            case TIMER_IS_T1A:
            case TIMER_IS_T2:
            case TIMER_IS_T2A:
                timer_t2a_start(s);
                break;
            case TIMER_IS_T4:
            case TIMER_IS_T4A:
                timer_t4a_start(s);
                break;
            }
        }
        break;
    case SIG_STATUS_ABORT:
        /* Just ignore these */
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected HDLC special length - %d!\n", status);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(void) t30_hdlc_accept(void *user_data, const uint8_t *msg, int len, int ok)
{
    t30_state_t *s;

    if (len < 0)
    {
        t30_hdlc_rx_status(user_data, len);
        return;
    }

    s = (t30_state_t *) user_data;
    /* The spec. says a command or response is not valid if:
        - any of the frames, optional or mandatory, have an FCS error.
        - any single frame exceeds 3s +- 15% (i.e. no frame should exceed 2.55s)
        - the final frame is not tagged as a final frame
        - the final frame is not a recognised one.
       The first point seems benign. If we accept an optional frame, and a later
       frame is bad, having accepted the optional frame should be harmless.
       The 2.55s maximum seems to limit signalling frames to no more than 95 octets,
       including FCS, and flag octets (assuming the use of V.21).
    */
    if (!ok)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Bad HDLC CRC received\n");
        if (s->phase != T30_PHASE_C_ECM_RX)
        {
            /* We either force a resend, or we wait until a resend occurs through a timeout. */
            if ((s->supported_t30_features & T30_SUPPORT_COMMAND_REPEAT))
            {
                s->step = 0;
                if (s->phase == T30_PHASE_B_RX)
                    queue_phase(s, T30_PHASE_B_TX);
                else
                    queue_phase(s, T30_PHASE_D_TX);
                send_simple_frame(s, T30_CRP);
            }
            else
            {
                /* Cancel the command or response timer (if one is running) */
                span_log(&s->logging, SPAN_LOG_FLOW, "Bad CRC and timer is %d\n", s->timer_t2_t4_is);
                if (s->timer_t2_t4_is == TIMER_IS_T2A)
                    timer_t2_t4_stop(s);
            }
        }
        return;
    }

    if (len < 3)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Bad HDLC frame length - %d\n", len);
        /* Cancel the command or response timer (if one is running) */
        timer_t2_t4_stop(s);
        return;
    }
    if (msg[0] != ADDRESS_FIELD
        ||
        !(msg[1] == CONTROL_FIELD_NON_FINAL_FRAME  ||  msg[1] == CONTROL_FIELD_FINAL_FRAME))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Bad HDLC frame header - %02x %02x\n", msg[0], msg[1]);
        /* Cancel the command or response timer (if one is running) */
        timer_t2_t4_stop(s);
        return;
    }
    s->rx_frame_received = true;
    /* Cancel the command or response timer (if one is running) */
    timer_t2_t4_stop(s);
    process_rx_control_msg(s, msg, len);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_front_end_status(void *user_data, int status)
{
    t30_state_t *s;

    s = (t30_state_t *) user_data;

    switch (status)
    {
    case T30_FRONT_END_SEND_STEP_COMPLETE:
        span_log(&s->logging, SPAN_LOG_FLOW, "Send complete in phase %s, state %s\n", phase_names[s->phase], state_names[s->state]);
        /* We have finished sending our messages, so move on to the next operation. */
        switch (s->state)
        {
        case T30_STATE_ANSWERING:
            span_log(&s->logging, SPAN_LOG_FLOW, "Starting answer mode\n");
            set_phase(s, T30_PHASE_B_TX);
            timer_t2_start(s);
            s->dis_received = false;
            send_dis_or_dtc_sequence(s, true);
            break;
        case T30_STATE_R:
            if (send_dis_or_dtc_sequence(s, false))
            {
                /* Wait for an acknowledgement. */
                set_phase(s, T30_PHASE_B_RX);
                timer_t4_start(s);
            }
            break;
        case T30_STATE_F_CFR:
            if (send_cfr_sequence(s, false))
            {
                if (s->error_correcting_mode)
                {
                    set_state(s, T30_STATE_F_DOC_ECM);
                    queue_phase(s, T30_PHASE_C_ECM_RX);
                }
                else
                {
                    set_state(s, T30_STATE_F_DOC_NON_ECM);
                    queue_phase(s, T30_PHASE_C_NON_ECM_RX);
                }
                timer_t2_start(s);
                s->next_rx_step = T30_MPS;
            }
            break;
        case T30_STATE_F_FTT:
            if (s->step == 0)
            {
                shut_down_hdlc_tx(s);
                s->step++;
            }
            else
            {
                set_phase(s, T30_PHASE_B_RX);
                timer_t2_start(s);
            }
            break;
        case T30_STATE_III_Q_MCF:
        case T30_STATE_III_Q_RTP:
        case T30_STATE_III_Q_RTN:
        case T30_STATE_F_POST_RCP_PPR:
        case T30_STATE_F_POST_RCP_MCF:
            if (s->step == 0)
            {
                shut_down_hdlc_tx(s);
                s->step++;
            }
            else
            {
                switch (s->next_rx_step)
                {
                case T30_PRI_MPS:
                case T30_MPS:
                    /* We should now start to get another page */
                    if (s->error_correcting_mode)
                    {
                        set_state(s, T30_STATE_F_DOC_ECM);
                        queue_phase(s, T30_PHASE_C_ECM_RX);
                    }
                    else
                    {
                        set_state(s, T30_STATE_F_DOC_NON_ECM);
                        queue_phase(s, T30_PHASE_C_NON_ECM_RX);
                    }
                    timer_t2_start(s);
                    break;
                case T30_PRI_EOM:
                case T30_EOM:
                case T30_EOS:
                    /* See if we get something back, before moving to phase B. */
                    timer_t2_start(s);
                    set_phase(s, T30_PHASE_D_RX);
                    break;
                case T30_PRI_EOP:
                case T30_EOP:
                    /* Wait for a DCN. */
                    set_phase(s, T30_PHASE_D_RX);
                    timer_t4_start(s);
                    break;
                default:
                    span_log(&s->logging, SPAN_LOG_FLOW, "Unknown next rx step - %d\n", s->next_rx_step);
                    disconnect(s);
                    break;
                }
            }
            break;
        case T30_STATE_II_Q:
        case T30_STATE_IV_PPS_NULL:
        case T30_STATE_IV_PPS_Q:
        case T30_STATE_IV_PPS_RNR:
        case T30_STATE_IV_EOR_RNR:
        case T30_STATE_F_POST_RCP_RNR:
        case T30_STATE_IV_EOR:
        case T30_STATE_IV_CTC:
            if (s->step == 0)
            {
                shut_down_hdlc_tx(s);
                s->step++;
            }
            else
            {
                /* We have finished sending the post image message. Wait for an
                   acknowledgement. */
                set_phase(s, T30_PHASE_D_RX);
                timer_t4_start(s);
            }
            break;
        case T30_STATE_B:
            /* We have now allowed time for the last message to flush through
               the system, so it is safe to report the end of the call. */
            if (s->phase_e_handler)
                s->phase_e_handler(s->phase_e_user_data, s->current_status);
            set_state(s, T30_STATE_CALL_FINISHED);
            set_phase(s, T30_PHASE_CALL_FINISHED);
            release_resources(s);
            break;
        case T30_STATE_C:
            if (s->step == 0)
            {
                shut_down_hdlc_tx(s);
                s->step++;
            }
            else
            {
                /* We just sent the disconnect message. Now it is time to disconnect. */
                disconnect(s);
            }
            break;
        case T30_STATE_D:
            if (send_dcs_sequence(s, false))
            {
                if ((s->iaf & T30_IAF_MODE_NO_TCF))
                {
                    /* Skip the trainability test */
                    s->retries = 0;
                    s->short_train = true;
                    if (s->error_correcting_mode)
                    {
                        set_state(s, T30_STATE_IV);
                        queue_phase(s, T30_PHASE_C_ECM_TX);
                    }
                    else
                    {
                        set_state(s, T30_STATE_I);
                        queue_phase(s, T30_PHASE_C_NON_ECM_TX);
                    }
                }
                else
                {
                    /* Do the trainability test */
                    /* TCF is always sent with long training */
                    s->short_train = false;
                    set_state(s, T30_STATE_D_TCF);
                    set_phase(s, T30_PHASE_C_NON_ECM_TX);
                }
            }
            break;
        case T30_STATE_D_TCF:
            /* Finished sending training test. Listen for the response. */
            set_phase(s, T30_PHASE_B_RX);
            timer_t4_start(s);
            set_state(s, T30_STATE_D_POST_TCF);
            break;
        case T30_STATE_I:
            /* Send the end of page message */
            set_phase(s, T30_PHASE_D_TX);
            set_state(s, T30_STATE_II_Q);
            /* We might need to resend the page we are on, but we need to check if there
               are any more pages to send, so we can send the correct signal right now. */
            send_simple_frame(s, s->next_tx_step = check_next_tx_step(s));
            break;
        case T30_STATE_IV:
            /* We have finished sending an FCD frame */
            if (s->step == 0)
            {
                if (send_next_ecm_frame(s))
                {
                    shut_down_hdlc_tx(s);
                    s->step++;
                }
            }
            else
            {
                /* Send the end of page or partial page message */
                set_phase(s, T30_PHASE_D_TX);
                if (s->ecm_at_page_end)
                    s->next_tx_step = check_next_tx_step(s);
                if (send_pps_frame(s) == T30_NULL)
                    set_state(s, T30_STATE_IV_PPS_NULL);
                else
                    set_state(s, T30_STATE_IV_PPS_Q);
            }
            break;
        case T30_STATE_F_DOC_ECM:
            /* This should be the end of a CTR being sent. */
            if (s->step == 0)
            {
                shut_down_hdlc_tx(s);
                s->step++;
            }
            else
            {
                /* We have finished sending the CTR. Wait for image data again. */
                queue_phase(s, T30_PHASE_C_ECM_RX);
                timer_t2_start(s);
            }
            break;
        case T30_STATE_CALL_FINISHED:
            /* Just ignore anything that happens now. We might get here if a premature
               disconnect from the far end overlaps something. */
            break;
        default:
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad state for send complete in t30_front_end_status - %s\n", state_names[s->state]);
            break;
        }
        break;
    case T30_FRONT_END_RECEIVE_COMPLETE:
        span_log(&s->logging, SPAN_LOG_FLOW, "Receive complete in phase %s, state %s\n", phase_names[s->phase], state_names[s->state]);
        /* Usually receive complete is notified by a carrier down signal. However,
           in cases like a T.38 packet stream dying in the middle of reception
           there needs to be a means to stop things. */
        switch (s->phase)
        {
        case T30_PHASE_C_NON_ECM_RX:
            t30_non_ecm_rx_status(s, SIG_STATUS_CARRIER_DOWN);
            break;
        default:
            t30_hdlc_rx_status(s, SIG_STATUS_CARRIER_DOWN);
            break;
        }
        break;
    case T30_FRONT_END_SIGNAL_PRESENT:
        span_log(&s->logging, SPAN_LOG_FLOW, "A signal is present\n");
        /* The front end is explicitly telling us the signal we expect is present. This might
           be a premature indication from a T.38 implementation, but we have to believe it.
           if we don't we can time out improperly. For example, we might get an image modem
           carrier signal, but the first HDLC frame might only occur several seconds later.
           Many ECM senders idle on HDLC flags while waiting for the paper or filing system
           to become ready. T.38 offers no specific indication of correct carrier training, so
           if we don't kill the timer on the initial carrier starting signal, we will surely
           time out quite often before the next thing we receive. */
        switch (s->phase)
        {
        case T30_PHASE_A_CED:
        case T30_PHASE_A_CNG:
        case T30_PHASE_B_RX:
        case T30_PHASE_D_RX:
            /* We are running a V.21 receive modem, where an explicit training indication
               will not occur. */
            t30_hdlc_rx_status(s, SIG_STATUS_CARRIER_UP);
            t30_hdlc_rx_status(s, SIG_STATUS_FRAMING_OK);
            break;
        default:
            /* Cancel any receive timeout, and declare that a receive signal is present,
               since the front end is explicitly telling us we have seen something. */
            s->rx_signal_present = true;
            timer_t2_t4_stop(s);
            break;
        }
        break;
    case T30_FRONT_END_SIGNAL_ABSENT:
        span_log(&s->logging, SPAN_LOG_FLOW, "No signal is present\n");
        /* TODO: Should we do anything here? */
        break;
    case T30_FRONT_END_CED_PRESENT:
        span_log(&s->logging, SPAN_LOG_FLOW, "CED tone is present\n");
        /* TODO: Should we do anything here? */
        break;
    case T30_FRONT_END_CNG_PRESENT:
        span_log(&s->logging, SPAN_LOG_FLOW, "CNG tone is present\n");
        /* TODO: Should we do anything here? */
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_timer_update(t30_state_t *s, int samples)
{
    int previous;

    if (s->timer_t0_t1 > 0)
    {
        if ((s->timer_t0_t1 -= samples) <= 0)
        {
            s->timer_t0_t1 = 0;
            if (s->far_end_detected)
                timer_t1_expired(s);
            else
                timer_t0_expired(s);
        }
    }
    if (s->timer_t3 > 0)
    {
        if ((s->timer_t3 -= samples) <= 0)
        {
            s->timer_t3 = 0;
            timer_t3_expired(s);
        }
    }
    if (s->timer_t2_t4 > 0)
    {
        if ((s->timer_t2_t4 -= samples) <= 0)
        {
            previous = s->timer_t2_t4_is;
            /* Don't allow the count to be left at a small negative number.
               It looks cosmetically bad in the logs. */
            s->timer_t2_t4 = 0;
            s->timer_t2_t4_is = TIMER_IS_IDLE;
            switch (previous)
            {
            case TIMER_IS_T1A:
                timer_t1a_expired(s);
                break;
            case TIMER_IS_T2:
                timer_t2_expired(s);
                break;
            case TIMER_IS_T2A:
                timer_t2a_expired(s);
                break;
            case TIMER_IS_T2B:
                timer_t2b_expired(s);
                break;
            case TIMER_IS_T4:
                timer_t4_expired(s);
                break;
            case TIMER_IS_T4A:
                timer_t4a_expired(s);
                break;
            case TIMER_IS_T4B:
                timer_t4b_expired(s);
                break;
            }
        }
    }
    if (s->timer_t5 > 0)
    {
        if ((s->timer_t5 -= samples) <= 0)
        {
            s->timer_t5 = 0;
            timer_t5_expired(s);
        }
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_terminate(t30_state_t *s)
{
    if (s->phase != T30_PHASE_CALL_FINISHED)
    {
        /* The far end disconnected early, but was it just a tiny bit too early,
           as we were just tidying up, or seriously early as in a failure? */
        switch (s->state)
        {
        case T30_STATE_C:
            /* We were sending the final disconnect, so just hussle things along. */
            disconnect(s);
            break;
        case T30_STATE_B:
            /* We were in the final wait for everything to flush through, so just
               hussle things along. */
            break;
        default:
            /* If we have seen a genuine EOP or PRI_EOP, that's good enough. */
            if (!s->end_of_procedure_detected)
            {
                /* The call terminated prematurely. */
                t30_set_status(s, T30_ERR_CALLDROPPED);
            }
            break;
        }
        if (s->phase_e_handler)
            s->phase_e_handler(s->phase_e_user_data, s->current_status);
        set_state(s, T30_STATE_CALL_FINISHED);
        set_phase(s, T30_PHASE_CALL_FINISHED);
        release_resources(s);
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_get_transfer_statistics(t30_state_t *s, t30_stats_t *t)
{
    t4_stats_t stats;

    t->bit_rate = fallback_sequence[s->current_fallback].bit_rate;
    t->error_correcting_mode = s->error_correcting_mode;
    t->error_correcting_mode_retries = s->error_correcting_mode_retries;
    switch (s->operation_in_progress)
    {
    case OPERATION_IN_PROGRESS_T4_TX:
    case OPERATION_IN_PROGRESS_POST_T4_TX:
        t4_tx_get_transfer_statistics(&s->t4.tx, &stats);
        break;
    case OPERATION_IN_PROGRESS_T4_RX:
    case OPERATION_IN_PROGRESS_POST_T4_RX:
        t4_rx_get_transfer_statistics(&s->t4.rx, &stats);
        break;
    default:
        memset(&stats, 0, sizeof(stats));
        break;
    }
    t->pages_tx = s->tx_page_number;
    t->pages_rx = s->rx_page_number;
    t->pages_in_file = stats.pages_in_file;
    t->bad_rows = stats.bad_rows;
    t->longest_bad_row_run = stats.longest_bad_row_run;

    t->image_type = stats.image_type;
    t->image_x_resolution = stats.image_x_resolution;
    t->image_y_resolution = stats.image_y_resolution;
    t->image_width = stats.image_width;
    t->image_length = stats.image_length;

    t->type = stats.type;
    t->x_resolution = stats.x_resolution;
    t->y_resolution = stats.y_resolution;
    t->width = stats.width;
    t->length = stats.length;

    t->compression = stats.compression;
    t->image_size = stats.line_image_size;
    t->current_status = s->current_status;
    t->rtn_events = s->rtn_events;
    t->rtp_events = s->rtp_events;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_local_interrupt_request(t30_state_t *s, int state)
{
    if (s->timer_t3 > 0)
    {
        /* Accept the far end's outstanding request for interrupt. */
        /* TODO: */
        send_simple_frame(s, (state)  ?  T30_PIP  :  T30_PIN);
    }
    s->local_interrupt_pending = state;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_remote_interrupts_allowed(t30_state_t *s, int state)
{
    s->remote_interrupts_allowed = state;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_restart(t30_state_t *s)
{
    s->phase = T30_PHASE_IDLE;
    s->next_phase = T30_PHASE_IDLE;
    s->current_fallback = 0;
    s->rx_signal_present = false;
    s->rx_trained = false;
    s->rx_frame_received = false;
    s->current_status = T30_ERR_OK;
    s->ppr_count = 0;
    s->ecm_progress = 0;
    s->receiver_not_ready_count = 0;
    memset(&s->far_dis_dtc_frame, 0, sizeof(s->far_dis_dtc_frame));
    t30_build_dis_or_dtc(s);
    memset(&s->rx_info, 0, sizeof(s->rx_info));
    release_resources(s);
    /* The page number is only reset at call establishment */
    s->rx_page_number = 0;
    s->tx_page_number = 0;
    s->rtn_events = 0;
    s->rtp_events = 0;
    s->local_interrupt_pending = false;
    s->far_end_detected = false;
    s->end_of_procedure_detected = false;
    s->timer_t0_t1 = ms_to_samples(DEFAULT_TIMER_T0);
    if (s->calling_party)
    {
        set_state(s, T30_STATE_T);
        set_phase(s, T30_PHASE_A_CNG);
    }
    else
    {
        set_state(s, T30_STATE_ANSWERING);
        set_phase(s, T30_PHASE_A_CED);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t30_state_t *) t30_init(t30_state_t *s,
                                     int calling_party,
                                     t30_set_handler_t set_rx_type_handler,
                                     void *set_rx_type_user_data,
                                     t30_set_handler_t set_tx_type_handler,
                                     void *set_tx_type_user_data,
                                     t30_send_hdlc_handler_t send_hdlc_handler,
                                     void *send_hdlc_user_data)
{
    if (s == NULL)
    {
        if ((s = (t30_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->calling_party = calling_party;
    s->set_rx_type_handler = set_rx_type_handler;
    s->set_rx_type_user_data = set_rx_type_user_data;
    s->set_tx_type_handler = set_tx_type_handler;
    s->set_tx_type_user_data = set_tx_type_user_data;
    s->send_hdlc_handler = send_hdlc_handler;
    s->send_hdlc_user_data = send_hdlc_user_data;

    /* Default to the basic modems. */
    s->supported_modems = T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17;
    s->supported_compressions = T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D;
    s->supported_bilevel_resolutions = T4_RESOLUTION_R8_STANDARD
                                     | T4_RESOLUTION_R8_FINE
                                     | T4_RESOLUTION_R8_SUPERFINE
                                     | T4_RESOLUTION_200_100
                                     | T4_RESOLUTION_200_200
                                     | T4_RESOLUTION_200_400;
    s->supported_image_sizes = T4_SUPPORT_WIDTH_215MM
                             | T4_SUPPORT_LENGTH_US_LETTER
                             | T4_SUPPORT_LENGTH_US_LEGAL
                             | T4_SUPPORT_LENGTH_A4
                             | T4_SUPPORT_LENGTH_B4
                             | T4_SUPPORT_LENGTH_UNLIMITED;
    /* Set the output encoding to something safe. For bi-level images most things
       get 1D and 2D encoding right. Quite a lot get other things wrong. */
    s->supported_output_compressions = T4_COMPRESSION_T4_2D | T4_COMPRESSION_JPEG;
    s->local_min_scan_time_code = T30_MIN_SCAN_0MS;
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.30");
    t30_restart(s);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_release(t30_state_t *s)
{
    /* Make sure any FAX in progress is tidied up. If the tidying up has
       already happened, repeating it here is harmless. */
    terminate_operation_in_progress(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_free(t30_state_t *s)
{
    t30_release(s);
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_call_active(t30_state_t *s)
{
    return (s->phase != T30_PHASE_CALL_FINISHED);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
