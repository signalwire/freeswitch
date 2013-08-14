/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t31_pseudo_terminal_tests.c -
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2012 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <inttypes.h>
#include <stdlib.h>

#if defined(WIN32)
#include <windows.h>
#else
#if defined(__APPLE__)
#include <util.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#include <termios.h>
#include <sys/socket.h>
#else
#include <pty.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#endif

#include "spandsp.h"

#include "spandsp/t30_fcf.h"

#include "spandsp-sim.h"

#undef SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "pseudo_terminals.h"

#if defined(ENABLE_GUI)
#include "media_monitor.h"
#endif
#include "fax_utils.h"

#define INPUT_FILE_NAME         "../test-data/itu/fax/itutests.tif"
#define OUTPUT_FILE_NAME        "t31_pseudo_terminal.tif"
#define OUTPUT_WAVE_FILE_NAME   "t31_tests.wav"

#define MANUFACTURER            "www.soft-switch.org"

#define SAMPLES_PER_CHUNK 160

typedef enum
{
    MODEM_POLL_READ = (1 << 0),
    MODEM_POLL_WRITE = (1 << 1),
    MODEM_POLL_ERROR = (1 << 2)
} modem_poll_t;

g1050_state_t *path_a_to_b;
g1050_state_t *path_b_to_a;

double when = 0.0;

int t38_mode = false;

struct modem_s modem[10];

char *decode_test_file = NULL;
int countdown = 0;
int answered = 0;
int done = false;

int test_seq_ptr = 0;

t31_state_t *t31_state;

static int phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    char tag[20];

    i = (int) (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase B", i);
    printf("%c: Phase B handler on channel %c - (0x%X) %s\n", i, i, result, t30_frametype(result));
    fax_log_rx_parameters(s, tag);
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static int phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    char tag[20];

    i = (int) (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase D", i);
    printf("%c: Phase D handler on channel %c - (0x%X) %s\n", i, i, result, t30_frametype(result));
    fax_log_page_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    char tag[20];

    i = (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase E", i);
    printf("Phase E handler on channel %c\n", i);
    fax_log_final_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);
    //exit(0);
}
/*- End of function --------------------------------------------------------*/

static int at_tx_handler(at_state_t *s, void *user_data, const uint8_t *buf, size_t len)
{
#if defined(WIN32)
    DWORD res;
    OVERLAPPED o;
#else
    int res;
#endif
    modem_t *modem;

int i;

printf("YYZ %d - ", (int) len);
for (i = 0;  i < len;  i++)
    printf(" 0x%02x", buf[i]);
printf("\n");

    modem = (modem_t *) user_data;
#if defined(WIN32)
    o.hEvent = CreateEvent(NULL, true, false, NULL);
    /* Initialize the rest of the OVERLAPPED structure to zero. */
    o.Internal = 0;
    o.InternalHigh = 0;
    o.Offset = 0;
    o.OffsetHigh = 0;
    assert(o.hEvent);
    if (!WriteFile(modem->master, buf, (DWORD) len, &res, &o))
        GetOverlappedResult(modem->master, &o, &res, true);
    CloseHandle(o.hEvent);
#else
    res = write(modem->master, buf, len);
#endif
    if (res != len)
    {
        printf("Failed to write the whole buffer to the device. %d bytes of %d written: %s\n", res, (int) len, strerror(errno));

        if (res == -1)
            res = 0;
#if !defined(WIN32)
        if (tcflush(modem->master, TCOFLUSH))
            printf("Unable to flush pty master buffer: %s\n", strerror(errno));
        else if (tcflush(modem->slave, TCOFLUSH))
            printf("Unable to flush pty slave buffer: %s\n", strerror(errno));
        else
            printf("Successfully flushed pty buffer\n");
#endif
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int t31_call_control(t31_state_t *s, void *user_data, int op, const char *num)
{
    uint8_t x[2];
    modem_t *modem;

    printf("Modem control - %s", at_modem_control_to_str(op));
    modem = (modem_t *) user_data;
    switch (op)
    {
    case AT_MODEM_CONTROL_CALL:
        printf(" %s", num);
        t31_call_event(t31_state, AT_CALL_EVENT_CONNECTED);
        answered = 2;
        break;
    case AT_MODEM_CONTROL_ANSWER:
        answered = 1;
        break;
    case AT_MODEM_CONTROL_HANGUP:
        //done = true;
        break;
    case AT_MODEM_CONTROL_OFFHOOK:
        break;
    case AT_MODEM_CONTROL_DTR:
        printf(" %d", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_RTS:
        printf(" %d", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_CTS:
        printf(" %d", (int) (intptr_t) num);
        /* Use XON/XOFF characters for flow control */
        switch (t31_state->at_state.dte_dce_flow_control)
        {
        case 1:
            x[0] = (num)  ?  0x11  :  0x13;
            at_tx_handler(&t31_state->at_state, user_data, x, 1);
            break;
        case 2:
            break;
        }
        /*endswitch*/
        modem->block_read = (num == NULL);
        break;
    case AT_MODEM_CONTROL_CAR:
        printf(" %d", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_RNG:
        printf(" %d", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_DSR:
        printf(" %d", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_SETID:
        printf(" %d", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_RESTART:
        printf(" %d", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_DTE_TIMEOUT:
        printf(" %d", (int) (intptr_t) num);
        break;
    }
    /*endswitch*/
    printf("\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int t38_tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    int i;

    /* This routine queues messages between two instances of T.38 processing, from the T.38 terminal side. */
    span_log(t38_core_get_logging_state(s), SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);

    for (i = 0;  i < count;  i++)
    {
        if (g1050_put(path_a_to_b, buf, len, s->tx_seq_no, when) < 0)
            printf("Lost packet %d\n", s->tx_seq_no);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int t31_tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    int i;

    /* This routine queues messages between two instances of T.38 processing, from the T.31 modem side. */
    span_log(t38_core_get_logging_state(s), SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);

    for (i = 0;  i < count;  i++)
    {
        if (g1050_put(path_b_to_a, buf, len, s->tx_seq_no, when) < 0)
            printf("Lost packet %d\n", s->tx_seq_no);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

#if defined(WIN32)
static int modem_wait_sock(modem_t *modem, int ms, modem_poll_t flags)
{
    /* This method ignores ms and waits infinitely */
    DWORD dwEvtMask;
    DWORD dwWait;
    DWORD comerrors;
    OVERLAPPED o;
    BOOL result;
    int ret;
    HANDLE arHandles[2];

    ret = MODEM_POLL_ERROR;
    arHandles[0] = modem->threadAbort;

    o.hEvent = CreateEvent(NULL, true, false, NULL);
    arHandles[1] = o.hEvent;

    /* Initialize the rest of the OVERLAPPED structure to zero. */
    o.Internal = 0;
    o.InternalHigh = 0;
    o.Offset = 0;
    o.OffsetHigh = 0;
    assert(o.hEvent);

    if ((result = WaitCommEvent(modem->master, &dwEvtMask, &o)) == 0)
    {
        if (GetLastError() != ERROR_IO_PENDING)
        {
            /* Something went horribly wrong with WaitCommEvent(), so
               clear all errors and try again */
            ClearCommError(modem->master, &comerrors, 0);
        }
        else
        {
            /* IO is pending, wait for it to finish */
            dwWait = WaitForMultipleObjects(2, arHandles, false, INFINITE);
            if (dwWait == WAIT_OBJECT_0 + 1  &&  !modem->block_read)
                ret = MODEM_POLL_READ;
        }
    }
    else
    {
        if (!modem->block_read)
            ret = MODEM_POLL_READ;
    }

    CloseHandle (o.hEvent);
    return ret;
}
/*- End of function --------------------------------------------------------*/
#else
static int modem_wait_sock(int sock, uint32_t ms, modem_poll_t flags)
{
    struct pollfd pfds[2] = {{0}};
    int s;
    int ret;

    pfds[0].fd = sock;

    if ((flags & MODEM_POLL_READ))
        pfds[0].events |= POLLIN;
    if ((flags & MODEM_POLL_WRITE))
        pfds[0].events |= POLLOUT;
    if ((flags & MODEM_POLL_ERROR))
        pfds[0].events |= POLLERR;

    s = poll(pfds, (modem->block_read)  ?  0  :  1, ms);

    ret = 0;
    if (s < 0)
    {
        ret = s;
    }
    else if (s > 0)
    {
        if ((pfds[0].revents & POLLIN))
            ret |= MODEM_POLL_READ;
        if ((pfds[0].revents & POLLOUT))
            ret |= MODEM_POLL_WRITE;
        if ((pfds[0].revents & POLLERR))
            ret |= MODEM_POLL_ERROR;
    }

    return ret;

}
/*- End of function --------------------------------------------------------*/
#endif

static int t30_tests(int t38_mode, int use_ecm, int use_gui, int log_audio, int test_sending, int g1050_model_no, int g1050_speed_pattern_no)
{
    t38_terminal_state_t *t38_state;
    fax_state_t *fax_state;
    uint8_t msg[1024];
    char buf[1024];
    int len;
    int msg_len;
    int t30_len;
    int t31_len;
    int t38_version;
    int without_pacing;
    int use_tep;
    int seq_no;
    double tx_when;
    double rx_when;
    t30_state_t *t30;
    t38_core_state_t *t38_core;
    logging_state_t *logging;
    int k;
    int outframes;
    int ret;
    int16_t t30_amp[SAMPLES_PER_CHUNK];
    int16_t t31_amp[SAMPLES_PER_CHUNK];
    int16_t silence[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    SNDFILE *wave_handle;
    SNDFILE *in_handle;
    at_state_t *at_state;
#if defined(WIN32)
    DWORD read_bytes;
    OVERLAPPED o;
#endif

    /* Test the T.31 modem against the full FAX machine in spandsp */

    /* Set up the test environment */
    t38_version = 1;
    without_pacing = false;
    use_tep = false;

    wave_handle = NULL;
    if (log_audio)
    {
        if ((wave_handle = sf_open_telephony_write(OUTPUT_WAVE_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            exit(2);
        }
    }

    in_handle = NULL;
    if (decode_test_file)
    {
        if ((in_handle = sf_open_telephony_read(decode_test_file, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", decode_test_file);
            exit(2);
        }
    }

    srand48(0x1234567);
    if ((path_a_to_b = g1050_init(g1050_model_no, g1050_speed_pattern_no, 100, 33)) == NULL)
    {
        fprintf(stderr, "Failed to start IP network path model\n");
        exit(2);
    }
    if ((path_b_to_a = g1050_init(g1050_model_no, g1050_speed_pattern_no, 100, 33)) == NULL)
    {
        fprintf(stderr, "Failed to start IP network path model\n");
        exit(2);
    }

    t38_state = NULL;
    fax_state = NULL;
    if (test_sending)
    {
        if (t38_mode)
        {
            if ((t38_state = t38_terminal_init(NULL, false, t38_tx_packet_handler, t31_state)) == NULL)
            {
                fprintf(stderr, "Cannot start the T.38 channel\n");
                exit(2);
            }
            t30 = t38_terminal_get_t30_state(t38_state);
        }
        else
        {
            fax_state = fax_init(NULL, false);
            t30 = fax_get_t30_state(fax_state);
        }
        t30_set_rx_file(t30, OUTPUT_FILE_NAME, -1);
        countdown = 0;
    }
    else
    {
        if (t38_mode)
        {
            if ((t38_state = t38_terminal_init(NULL, true, t38_tx_packet_handler, t31_state)) == NULL)
            {
                fprintf(stderr, "Cannot start the T.38 channel\n");
                exit(2);
            }
            t30 = t38_terminal_get_t30_state(t38_state);
        }
        else
        {
            fax_state = fax_init(NULL, true);
            t30 = fax_get_t30_state(fax_state);
        }
        t30_set_tx_file(t30, INPUT_FILE_NAME, -1, -1);
        countdown = 250;
    }

    t30_set_ecm_capability(t30, use_ecm);

    if (t38_mode)
    {
        t38_core = t38_terminal_get_t38_core_state(t38_state);
        t38_set_t38_version(t38_core, t38_version);
        t38_terminal_set_config(t38_state, without_pacing);
        t38_terminal_set_tep_mode(t38_state, use_tep);
    }

    t30_set_tx_ident(t30, "11111111");
    t30_set_supported_modems(t30, T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17);
    //t30_set_tx_nsf(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
    t30_set_phase_b_handler(t30, phase_b_handler, (void *) 'A');
    t30_set_phase_d_handler(t30, phase_d_handler, (void *) 'A');
    t30_set_phase_e_handler(t30, phase_e_handler, (void *) 'A');

    if (t38_mode)
        logging = t38_terminal_get_logging_state(t38_state);
    else
        logging = t30_get_logging_state(t30);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, (t38_mode)  ?  "T.38"  :  "FAX");

    if (t38_mode)
    {
        t38_core = t38_terminal_get_t38_core_state(t38_state);
        logging = t38_core_get_logging_state(t38_core);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
        span_log_set_tag(logging, "T.38");

        logging = t30_get_logging_state(t30);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
        span_log_set_tag(logging, "T.38");
    }
    else
    {
        logging = fax_get_logging_state(fax_state);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
        span_log_set_tag(logging, "FAX");
    }

    memset(silence, 0, sizeof(silence));
    memset(t30_amp, 0, sizeof(t30_amp));

    /* Now set up and run the T.31 modem */
    if ((t31_state = t31_init(NULL, at_tx_handler, &modem[0], t31_call_control, &modem[0], t31_tx_packet_handler, NULL)) == NULL)
    {
        fprintf(stderr, "    Cannot start the T.31 modem\n");
        exit(2);
    }
    at_state = t31_get_at_state(t31_state);

    logging = t31_get_logging_state(t31_state);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.31");

    logging = at_get_logging_state(at_state);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.31");

    if (t38_mode)
    {
        t38_core = t31_get_t38_core_state(t31_state);
        logging = t38_core_get_logging_state(t38_core);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
        span_log_set_tag(logging, "T.31");

        t31_set_mode(t31_state, true);
        t38_set_t38_version(t38_core, t38_version);
    }

    at_reset_call_info(at_state);
    at_set_call_info(at_state, "DATE", "1231");
    at_set_call_info(at_state, "TIME", "1200");
    at_set_call_info(at_state, "NAME", "Name");
    at_set_call_info(at_state, "NMBR", "123456789");
    at_set_call_info(at_state, "ANID", "987654321");
    at_set_call_info(at_state, "USER", "User");
    at_set_call_info(at_state, "CDID", "234567890");
    at_set_call_info(at_state, "NDID", "345678901");

#if defined(ENABLE_GUI)
    if (use_gui)
        start_media_monitor();
#endif

    while (!done)
    {
        /* Deal with call setup, through the AT interface. */
        if (test_sending)
        {
        }
        else
        {
            if (answered == 0)
            {
                if (--countdown == 0)
                {
                    t31_call_event(t31_state, AT_CALL_EVENT_ALERTING);
                    countdown = 250;
                }
            }
            else if (answered == 1)
            {
printf("ZZZ\n");
                answered = 2;
                t31_call_event(t31_state, AT_CALL_EVENT_ANSWERED);
            }
        }

        ret = modem_wait_sock(modem[0].master, 20, MODEM_POLL_READ);
        if ((ret & MODEM_POLL_READ))
        {
#if defined(WIN32)
            o.hEvent = CreateEvent(NULL, true, false, NULL);

            /* Initialize the rest of the OVERLAPPED structure to zero. */
            o.Internal = 0;
            o.InternalHigh = 0;
            o.Offset = 0;
            o.OffsetHigh = 0;
            assert(o.hEvent);
            if (!ReadFile(modem->master, buf, avail, &read_bytes, &o))
                GetOverlappedResult(modem->master, &o, &read_bytes, true);
            CloseHandle (o.hEvent);
            if ((len = read_bytes))
#else
            if ((len = read(modem[0].master, buf, 1024)))
#endif
{
int i;

printf("YYY %d - ", len);
for (i = 0;  i < len;  i++)
    printf(" 0x%02x", buf[i] & 0xFF);
printf("\n");
                t31_at_rx(t31_state, buf, len);
}
        }

        if (answered == 2)
        {
            if (t38_mode)
            {
                while ((msg_len = g1050_get(path_a_to_b, msg, 1024, when, &seq_no, &tx_when, &rx_when)) >= 0)
                {
#if defined(ENABLE_GUI)
                    if (use_gui)
                        media_monitor_rx(seq_no, tx_when, rx_when);
#endif
                    t38_core = t31_get_t38_core_state(t31_state);
                    t38_core_rx_ifp_packet(t38_core, msg, msg_len, seq_no);
                }
                while ((msg_len = g1050_get(path_b_to_a, msg, 1024, when, &seq_no, &tx_when, &rx_when)) >= 0)
                {
#if defined(ENABLE_GUI)
                    if (use_gui)
                        media_monitor_rx(seq_no, tx_when, rx_when);
#endif
                    t38_core = t38_terminal_get_t38_core_state(t38_state);
                    t38_core_rx_ifp_packet(t38_core, msg, msg_len, seq_no);
                }
#if defined(ENABLE_GUI)
                if (use_gui)
                    media_monitor_update_display();
#endif
                /* Bump the G.1050 models along */
                when += (float) SAMPLES_PER_CHUNK/(float) SAMPLE_RATE;

                /* Bump things along on the t38_terminal side */
                span_log_bump_samples(t38_terminal_get_logging_state(t38_state), SAMPLES_PER_CHUNK);
                t38_core = t38_terminal_get_t38_core_state(t38_state);
                span_log_bump_samples(t38_core_get_logging_state(t38_core), SAMPLES_PER_CHUNK);

                t38_terminal_send_timeout(t38_state, SAMPLES_PER_CHUNK);
                t31_t38_send_timeout(t31_state, SAMPLES_PER_CHUNK);
            }
            else
            {
                t30_len = fax_tx(fax_state, t30_amp, SAMPLES_PER_CHUNK);
                /* The receive side always expects a full block of samples, but the
                   transmit side may not be sending any when it doesn't need to. We
                   may need to pad with some silence. */
                if (t30_len < SAMPLES_PER_CHUNK)
                {
                    memset(t30_amp + t30_len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - t30_len));
                    t30_len = SAMPLES_PER_CHUNK;
                }
                if (log_audio)
                {
                    for (k = 0;  k < t30_len;  k++)
                        out_amp[2*k] = t30_amp[k];
                }
                if (t31_rx(t31_state, t30_amp, t30_len))
                    break;
                t31_len = t31_tx(t31_state, t31_amp, SAMPLES_PER_CHUNK);
                if (t31_len < SAMPLES_PER_CHUNK)
                {
                    memset(t31_amp + t31_len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - t31_len));
                    t31_len = SAMPLES_PER_CHUNK;
                }
                if (log_audio)
                {
                    for (k = 0;  k < t31_len;  k++)
                        out_amp[2*k + 1] = t31_amp[k];
                }
                if (fax_rx(fax_state, t31_amp, SAMPLES_PER_CHUNK))
                    break;

                if (log_audio)
                {
                    outframes = sf_writef_short(wave_handle, out_amp, SAMPLES_PER_CHUNK);
                    if (outframes != SAMPLES_PER_CHUNK)
                        break;
                }

                /* Bump things along on the FAX machine side */
                span_log_bump_samples(fax_get_logging_state(fax_state), SAMPLES_PER_CHUNK);
            }
            /* Bump things along on the FAX machine side */
            span_log_bump_samples(t30_get_logging_state(t30), SAMPLES_PER_CHUNK);

            /* Bump things along on the T.31 modem side */
            t38_core = t31_get_t38_core_state(t31_state);
            span_log_bump_samples(t38_core_get_logging_state(t38_core), SAMPLES_PER_CHUNK);
            span_log_bump_samples(t31_get_logging_state(t31_state), SAMPLES_PER_CHUNK);
            span_log_bump_samples(at_get_logging_state(t31_get_at_state(t31_state)), SAMPLES_PER_CHUNK);
        }
    }

    if (t38_mode)
        t38_terminal_release(t38_state);

    if (decode_test_file)
    {
        if (sf_close_telephony(in_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", decode_test_file);
            exit(2);
        }
    }
    if (log_audio)
    {
        if (sf_close_telephony(wave_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            exit(2);
        }
    }

    if (!done)
    {
        printf("Tests failed\n");
        return 2;
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int log_audio;
    int t38_mode;
    int test_sending;
    int use_ecm;
    int use_gui;
    int g1050_model_no;
    int g1050_speed_pattern_no;
    int opt;
#if !defined(WIN32)
    int tioflags;
#endif

    decode_test_file = NULL;
    log_audio = false;
    test_sending = false;
    t38_mode = false;
    use_ecm = false;
    use_gui = false;
    g1050_model_no = 0;
    g1050_speed_pattern_no = 1;
    while ((opt = getopt(argc, argv, "d:eglM:rS:st")) != -1)
    {
        switch (opt)
        {
        case 'd':
            decode_test_file = optarg;
            break;
        case 'e':
            use_ecm = true;
            break;
        case 'g':
#if defined(ENABLE_GUI)
            use_gui = true;
#else
            fprintf(stderr, "Graphical monitoring not available\n");
            exit(2);
#endif
            break;
        case 'l':
            log_audio = true;
            break;
        case 'M':
            g1050_model_no = optarg[0] - 'A' + 1;
            break;
        case 'r':
            test_sending = false;
            break;
        case 'S':
            g1050_speed_pattern_no = atoi(optarg);
            break;
        case 's':
            test_sending = true;
            break;
        case 't':
            t38_mode = true;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    if (psuedo_terminal_create(&modem[0]))
        printf("Failure\n");

#if !defined(WIN32)
    ioctl(modem[0].slave, TIOCMGET, &tioflags);
    tioflags |= TIOCM_RI;
    ioctl(modem[0].slave, TIOCMSET, &tioflags);
#endif

    t30_tests(t38_mode, use_ecm, use_gui, log_audio, test_sending, g1050_model_no, g1050_speed_pattern_no);
    if (psuedo_terminal_close(&modem[0]))
        printf("Failure\n");
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
