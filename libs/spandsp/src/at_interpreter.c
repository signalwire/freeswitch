/*
 * SpanDSP - a series of DSP components for telephony
 *
 * at_interpreter.c - AT command interpreter to V.251, V.252, V.253, T.31 and the 3GPP specs.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Special thanks to Lee Howard <faxguy@howardsilvan.com>
 * for his great work debugging and polishing this code.
 *
 * Copyright (C) 2004, 2005, 2006 Steve Underwood
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

#if defined(__sun)
#define __EXTENSIONS__
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/fsk.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/fax_modems.h"

#include "spandsp/at_interpreter.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/at_interpreter.h"

#define MANUFACTURER            "www.soft-switch.org"
#define SERIAL_NUMBER           "42"
#define GLOBAL_OBJECT_IDENTITY  "42"

enum
{
    ASCII_RESULT_CODES = 1,
    NUMERIC_RESULT_CODES,
    NO_RESULT_CODES
};

static at_profile_t profiles[3] =
{
    {
#if defined(_MSC_VER)  ||  defined(__sunos)  ||  defined(__solaris)  ||  defined(__sun)
        /*.echo =*/ TRUE,
        /*.verbose =*/ TRUE,
        /*.result_code_format =*/ ASCII_RESULT_CODES,
        /*.pulse_dial =*/ FALSE,
        /*.double_escape =*/ FALSE,
        /*.adaptive_receive =*/ FALSE,
        /*.s_regs[100] =*/ {0, 0, 0, '\r', '\n', '\b', 1, 60, 5, 0, 0}
#else
        .echo = TRUE,
        .verbose = TRUE,
        .result_code_format = ASCII_RESULT_CODES,
        .pulse_dial = FALSE,
        .double_escape = FALSE,
        .adaptive_receive = FALSE,
        .s_regs[0] = 0,
        .s_regs[3] = '\r',
        .s_regs[4] = '\n',
        .s_regs[5] = '\b',
        .s_regs[6] = 1,
        .s_regs[7] = 60,
        .s_regs[8] = 5,
        .s_regs[10] = 0
#endif
    }
};

typedef const char *(*at_cmd_service_t)(at_state_t *s, const char *cmd);

static const char *manufacturer = MANUFACTURER;
static const char *model = PACKAGE;
static const char *revision = VERSION;

#define ETX 0x03
#define DLE 0x10
#define SUB 0x1A

static const char *at_response_codes[] =
{
    "OK",
    "CONNECT",
    "RING",
    "NO CARRIER",
    "ERROR",
    "???",
    "NO DIALTONE",
    "BUSY",
    "NO ANSWER",
    "+FCERROR",
    "+FRH:3"
};

SPAN_DECLARE(const char *) at_call_state_to_str(int state)
{
    switch (state)
    {
    case AT_CALL_EVENT_ALERTING:
        return "Alerting";
    case AT_CALL_EVENT_CONNECTED:
        return "Connected";
    case AT_CALL_EVENT_ANSWERED:
        return "Answered";
    case AT_CALL_EVENT_BUSY:
        return "Busy";
    case AT_CALL_EVENT_NO_DIALTONE:
        return "No dialtone";
    case AT_CALL_EVENT_NO_ANSWER:
        return "No answer";
    case AT_CALL_EVENT_HANGUP:
        return "Hangup";
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) at_modem_control_to_str(int state)
{
    switch (state)
    {
    case AT_MODEM_CONTROL_CALL:
        return "Call";
    case AT_MODEM_CONTROL_ANSWER:
        return "Answer";
    case AT_MODEM_CONTROL_HANGUP:
        return "Hangup";
    case AT_MODEM_CONTROL_OFFHOOK:
        return "Off hook";
    case AT_MODEM_CONTROL_ONHOOK:
        return "On hook";
    case AT_MODEM_CONTROL_DTR:
        return "DTR";
    case AT_MODEM_CONTROL_RTS:
        return "RTS";
    case AT_MODEM_CONTROL_CTS:
        return "CTS";
    case AT_MODEM_CONTROL_CAR:
        return "CAR";
    case AT_MODEM_CONTROL_RNG:
        return "RNG";
    case AT_MODEM_CONTROL_DSR:
        return "DSR";
    case AT_MODEM_CONTROL_SETID:
        return "Set ID";
    case AT_MODEM_CONTROL_RESTART:
        return "Restart";
    case AT_MODEM_CONTROL_DTE_TIMEOUT:
        return "DTE timeout";
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_set_at_rx_mode(at_state_t *s, int new_mode)
{
    /* The use of a DTE timeout is mode dependent. Set the timeout appropriately in
       the modem. */
    switch (new_mode)
    {
    case AT_MODE_HDLC:
    case AT_MODE_STUFFED:
        at_modem_control(s, s->dte_inactivity_timeout*1000, (void *) (intptr_t) s->dte_inactivity_timeout);
        break;
    default:
        at_modem_control(s, AT_MODEM_CONTROL_DTE_TIMEOUT, NULL);
        break;
    }
    s->at_rx_mode = new_mode;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_put_response(at_state_t *s, const char *t)
{
    uint8_t buf[3];

    buf[0] = s->p.s_regs[3];
    buf[1] = s->p.s_regs[4];
    buf[2] = '\0';
    if (s->p.result_code_format == ASCII_RESULT_CODES)
        s->at_tx_handler(s, s->at_tx_user_data, buf, 2);
    s->at_tx_handler(s, s->at_tx_user_data, (uint8_t *) t, strlen(t));
    s->at_tx_handler(s, s->at_tx_user_data, buf, 2);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_put_numeric_response(at_state_t *s, int val)
{
    char buf[20];

    snprintf(buf, sizeof(buf), "%d", val);
    at_put_response(s, buf);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_put_response_code(at_state_t *s, int code)
{
    uint8_t buf[20];

    span_log(&s->logging, SPAN_LOG_FLOW, "Sending AT response code %s\n", at_response_codes[code]);
    switch (s->p.result_code_format)
    {
    case ASCII_RESULT_CODES:
        at_put_response(s, at_response_codes[code]);
        break;
    case NUMERIC_RESULT_CODES:
        snprintf((char *) buf, sizeof(buf), "%d%c", code, s->p.s_regs[3]);
        s->at_tx_handler(s, s->at_tx_user_data, buf, strlen((char *) buf));
        break;
    default:
        /* No result codes */
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static int answer_call(at_state_t *s)
{
    if (at_modem_control(s, AT_MODEM_CONTROL_ANSWER, NULL) < 0)
        return FALSE;
    /* Answering should now be in progress. No AT response should be
       issued at this point. */
    s->do_hangup = FALSE;
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_call_event(at_state_t *s, int event)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Call event %d received\n", event);
    switch (event)
    {
    case AT_CALL_EVENT_ALERTING:
        at_modem_control(s, AT_MODEM_CONTROL_RNG, (void *) 1);
        if (s->display_call_info  &&  !s->call_info_displayed)
            at_display_call_info(s);
        at_put_response_code(s, AT_RESPONSE_CODE_RING);
        if ((++s->rings_indicated) >= s->p.s_regs[0]  &&  s->p.s_regs[0])
        {
            /* The modem is set to auto-answer now */
            answer_call(s);
        }
        break;
    case AT_CALL_EVENT_ANSWERED:
        at_modem_control(s, AT_MODEM_CONTROL_RNG, (void *) 0);
        if (s->fclass_mode == 0)
        {
            /* Normal data modem connection */
            at_set_at_rx_mode(s, AT_MODE_CONNECTED);
            /* TODO: */
        }
        else
        {
            /* FAX modem connection */
            at_set_at_rx_mode(s, AT_MODE_DELIVERY);
            at_modem_control(s, AT_MODEM_CONTROL_RESTART, (void *) FAX_MODEM_CED_TONE_TX);
        }
        break;
    case AT_CALL_EVENT_CONNECTED:
        span_log(&s->logging, SPAN_LOG_FLOW, "Dial call - connected. FCLASS=%d\n", s->fclass_mode);
        at_modem_control(s, AT_MODEM_CONTROL_RNG, (void *) 0);
        if (s->fclass_mode == 0)
        {
            /* Normal data modem connection */
            at_set_at_rx_mode(s, AT_MODE_CONNECTED);
            /* TODO: */
        }
        else
        {
            if (s->command_dial)
            {
                at_put_response_code(s, AT_RESPONSE_CODE_OK);
                at_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            }
            else
            {
                /* FAX modem connection */
                at_set_at_rx_mode(s, AT_MODE_DELIVERY);
                if (s->silent_dial)
                    at_modem_control(s, AT_MODEM_CONTROL_RESTART, (void *) FAX_MODEM_NOCNG_TONE_TX);
                else
                    at_modem_control(s, AT_MODEM_CONTROL_RESTART, (void *) FAX_MODEM_CNG_TONE_TX);
                s->dte_is_waiting = TRUE;
            }
        }
        break;
    case AT_CALL_EVENT_BUSY:
        at_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
        at_put_response_code(s, AT_RESPONSE_CODE_BUSY);
        break;
    case AT_CALL_EVENT_NO_DIALTONE:
        at_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
        at_put_response_code(s, AT_RESPONSE_CODE_NO_DIALTONE);
        break;
    case AT_CALL_EVENT_NO_ANSWER:
        at_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
        at_put_response_code(s, AT_RESPONSE_CODE_NO_ANSWER);
        break;
    case AT_CALL_EVENT_HANGUP:
        span_log(&s->logging, SPAN_LOG_FLOW, "Hangup... at_rx_mode %d\n", s->at_rx_mode);
        at_modem_control(s, AT_MODEM_CONTROL_ONHOOK, NULL);
        if (s->dte_is_waiting)
        {
            if (s->ok_is_pending)
            {
                at_put_response_code(s, AT_RESPONSE_CODE_OK);
                s->ok_is_pending = FALSE;
            }
            else
            {
                at_put_response_code(s, AT_RESPONSE_CODE_NO_CARRIER);
            }
            s->dte_is_waiting = FALSE;
            at_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
        }
        else if (s->fclass_mode  &&  s->rx_signal_present)
        {
            s->rx_data[s->rx_data_bytes++] = DLE;
            s->rx_data[s->rx_data_bytes++] = ETX;
            s->at_tx_handler(s, s->at_tx_user_data, s->rx_data, s->rx_data_bytes);
            s->rx_data_bytes = 0;
        }
        if (s->at_rx_mode != AT_MODE_OFFHOOK_COMMAND  &&  s->at_rx_mode != AT_MODE_ONHOOK_COMMAND)
            at_put_response_code(s, AT_RESPONSE_CODE_NO_CARRIER);
        s->rx_signal_present = FALSE;
        at_modem_control(s, AT_MODEM_CONTROL_RNG, (void *) 0);
        at_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid call event %d received.\n", event);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_reset_call_info(at_state_t *s)
{
    at_call_id_t *call_id;
    at_call_id_t *next;

    for (call_id = s->call_id;  call_id;  call_id = next)
    {
        next = call_id->next;
        free(call_id);
    }
    s->call_id = NULL;
    s->rings_indicated = 0;
    s->call_info_displayed = FALSE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_set_call_info(at_state_t *s, char const *id, char const *value)
{
    at_call_id_t *new_call_id;
    at_call_id_t *call_id;

    /* TODO: We should really not merely ignore a failure to malloc */
    if ((new_call_id = (at_call_id_t *) malloc(sizeof(*new_call_id))) == NULL)
        return;
    call_id = s->call_id;
    /* If these strdups fail its pretty harmless. We just appear to not
       have the relevant field. */
    new_call_id->id = (id)  ?  strdup(id)  :  NULL;
    new_call_id->value = (value)  ?  strdup(value)  :  NULL;
    new_call_id->next = NULL;

    if (call_id)
    {
        while (call_id->next)
            call_id = call_id->next;
        call_id->next = new_call_id;
    }
    else
    {
        s->call_id = new_call_id;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_display_call_info(at_state_t *s)
{
    char buf[132 + 1];
    at_call_id_t *call_id = s->call_id;

    while (call_id)
    {
        snprintf(buf,
                 sizeof(buf),
                 "%s=%s",
                 (call_id->id)  ?  call_id->id  :  "NULL",
                 (call_id->value)  ?  call_id->value  :  "<NONE>");
        at_put_response(s, buf);
        call_id = call_id->next;
    }
    s->call_info_displayed = TRUE;
}
/*- End of function --------------------------------------------------------*/

static int parse_num(const char **s, int max_value)
{
    int i;

    /* The spec. says no digits is valid, and should be treated as zero. */
    i = 0;
    while (isdigit((int) **s))
    {
        i = i*10 + ((**s) - '0');
        (*s)++;
    }
    if (i > max_value)
        i = -1;
    return i;
}
/*- End of function --------------------------------------------------------*/

static int parse_hex_num(const char **s, int max_value)
{
    int i;

    /* The spec. says a hex value is always 2 digits, and the alpha digits are
       upper case. */
    i = 0;
    if (isdigit((int) **s))
        i = **s - '0';
    else if (**s >= 'A'  &&  **s <= 'F')
        i = **s - 'A';
    else
        return -1;
    (*s)++;

    if (isdigit((int) **s))
        i = (i << 4)  | (**s - '0');
    else if (**s >= 'A'  &&  **s <= 'F')
        i = (i << 4)  | (**s - 'A');
    else
        return -1;
    (*s)++;
    if (i > max_value)
        i = -1;
    return i;
}
/*- End of function --------------------------------------------------------*/

static int match_element(const char **variant, const char *variants)
{
    int i;
    size_t len;
    char const *s;
    char const *t;

    s = variants;
    for (i = 0;  *s;  i++)
    {
        if ((t = strchr(s, ',')))
            len = t - s;
        else
            len = strlen(s);
        if (len == (int) strlen(*variant)  &&  memcmp(*variant, s, len) == 0)
        {
            *variant += len;
            return i;
        }
        s += len;
        if (*s == ',')
            s++;
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int parse_out(at_state_t *s, const char **t, int *target, int max_value, const char *prefix, const char *def)
{
    char buf[100];
    int val;

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s%s", (prefix)  ?  prefix  :  "", def);
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            if ((val = parse_num(t, max_value)) < 0)
                return FALSE;
            if (target)
                *target = val;
            break;
        }
        break;
    case '?':
        /* Show current value */
        val = (target)  ?  *target  :  0;
        snprintf(buf, sizeof(buf), "%s%d", (prefix)  ?  prefix  :  "", val);
        at_put_response(s, buf);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static int parse_2_out(at_state_t *s, const char **t, int *target1, int max_value1, int *target2, int max_value2, const char *prefix, const char *def)
{
    char buf[100];
    int val1;
    int val2;

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s%s", (prefix)  ?  prefix  :  "", def);
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            if ((val1 = parse_num(t, max_value1)) < 0)
                return FALSE;
            if (target1)
                *target1 = val1;
            if (**t == ',')
            {
                (*t)++;
                if ((val2 = parse_num(t, max_value2)) < 0)
                    return FALSE;
                if (target2)
                    *target2 = val2;
            }
            break;
        }
        break;
    case '?':
        /* Show current value */
        val1 = (target1)  ?  *target1  :  0;
        val2 = (target2)  ?  *target2  :  0;
        snprintf(buf, sizeof(buf), "%s%d,%d", (prefix)  ?  prefix  :  "", val1, val2);
        at_put_response(s, buf);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static int parse_n_out(at_state_t *s,
                       const char **t,
                       int *targets[],
                       const int max_values[],
                       int entries,
                       const char *prefix,
                       const char *def)
{
    char buf[100];
    int val;
    int len;
    int i;

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s%s", (prefix)  ?  prefix  :  "", def);
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            for (i = 0;  i < entries;  i++)
            {
                if ((val = parse_num(t, max_values[i])) < 0)
                    return FALSE;
                if (targets[i])
                    *targets[i] = val;
                if (**t != ',')
                    break;
                (*t)++;
            }
            break;
        }
        break;
    case '?':
        /* Show current value */
        len = snprintf(buf, sizeof(buf), "%s", (prefix)  ?  prefix  :  "");
        for (i = 0;  i < entries;  i++)
        {
            if (i > 0)
                len += snprintf(&buf[len], sizeof(buf) - len, ",");
            val = (targets[i])  ?  *targets[i]  :  0;
            len += snprintf(&buf[len], sizeof(buf) - len, "%d", val);
        }
        at_put_response(s, buf);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static int parse_hex_out(at_state_t *s, const char **t, int *target, int max_value, const char *prefix, const char *def)
{
    char buf[100];
    int val;

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s%s", (prefix)  ?  prefix  :  "", def);
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            if ((val = parse_hex_num(t, max_value)) < 0)
                return FALSE;
            if (target)
                *target = val;
            break;
        }
        break;
    case '?':
        /* Show current value */
        val = (target)  ?  *target  :  0;
        snprintf(buf, sizeof(buf), "%s%02X", (prefix)  ?  prefix  :  "", val);
        at_put_response(s, buf);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static int parse_string_list_out(at_state_t *s, const char **t, int *target, int max_value, const char *prefix, const char *def)
{
    char buf[100];
    int val;
    size_t len;
    char *tmp;

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s%s", (prefix)  ?  prefix  :  "", def);
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            if ((val = match_element(t, def)) < 0)
                return FALSE;
            if (target)
                *target = val;
            break;
        }
        break;
    case '?':
        /* Show current index value from def */
        val = (target)  ?  *target  :  0;
        while (val--  &&  (def = strchr(def, ',')))
            def++;
        if ((tmp = strchr(def, ',')))
            len = tmp - def;
        else
            len = strlen(def);
        snprintf(buf, sizeof(buf), "%s%.*s", (prefix)  ?  prefix  :  "", (int) len, def);
        at_put_response(s, buf);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static int parse_string_out(at_state_t *s, const char **t, char **target, const char *prefix)
{
    char buf[100];

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s", (prefix)  ?  prefix  :  "");
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            if (*target)
                free(*target);
            /* If this strdup fails, it should be harmless */
            *target = strdup(*t);
            break;
        }
        break;
    case '?':
        /* Show current index value */
        at_put_response(s, (*target)  ?  *target  :  "");
        break;
    default:
        return FALSE;
    }
    while (*t)
        t++;
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static const char *s_reg_handler(at_state_t *s, const char *t, int reg)
{
    int val;
    int b;
    char buf[4];

    /* Set or get an S register */
    switch (*t++)
    {
    case '=':
        switch (*t)
        {
        case '?':
            t++;
            snprintf(buf, sizeof(buf), "%3.3d", 0);
            at_put_response(s, buf);
            break;
        default:
            if ((val = parse_num(&t, 255)) < 0)
                return NULL;
            s->p.s_regs[reg] = (uint8_t) val;
            break;
        }
        break;
    case '?':
        snprintf(buf, sizeof(buf), "%3.3d", s->p.s_regs[reg]);
        at_put_response(s, buf);
        break;
    case '.':
        if ((b = parse_num(&t, 7)) < 0)
            return NULL;
        switch (*t++)
        {
        case '=':
            switch (*t)
            {
            case '?':
                t++;
                at_put_numeric_response(s, 0);
                break;
            default:
                if ((val = parse_num(&t, 1)) < 0)
                    return NULL;
                if (val)
                    s->p.s_regs[reg] |= (1 << b);
                else
                    s->p.s_regs[reg] &= ~(1 << b);
                break;
            }
            break;
        case '?':
            at_put_numeric_response(s, (int) ((s->p.s_regs[reg] >> b) & 1));
            break;
        default:
            return NULL;
        }
        break;
    default:
        return NULL;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static int process_class1_cmd(at_state_t *s, const char **t)
{
    int val;
    int operation;
    int direction;
    int result;
    const char *allowed;

    direction = (*(*t + 2) == 'T');
    operation = *(*t + 3);
    /* Step past the "+Fxx" */
    *t += 4;
    switch (operation)
    {
    case 'S':
        allowed = "0-255";
        break;
    case 'H':
        allowed = "3";
        break;
    default:
        allowed = "24,48,72,73,74,96,97,98,121,122,145,146";
        break;
    }

    val = -1;
    if (!parse_out(s, t, &val, 255, NULL, allowed))
        return TRUE;
    if (val < 0)
    {
        /* It was just a query */
        return TRUE;
    }
    /* All class 1 FAX commands are supposed to give an ERROR response, if the phone
       is on-hook. */
    if (s->at_rx_mode == AT_MODE_ONHOOK_COMMAND)
        return FALSE;

    result = TRUE;
    if (s->class1_handler)
        result = s->class1_handler(s, s->class1_user_data, direction, operation, val);
    switch (result)
    {
    case 0:
        /* Inhibit an immediate response.  (These commands should not be part of a multi-command entry.) */
        *t = (const char *) -1;
        return TRUE;
    case -1:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_dummy(at_state_t *s, const char *t)
{
    /* Dummy routine to absorb delimiting characters from a command string */
    return t + 1;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_A(at_state_t *s, const char *t)
{
    /* V.250 6.3.5 - Answer (abortable) */
    t += 1;
    if (!answer_call(s))
        return NULL;
    return (const char *) -1;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_D(at_state_t *s, const char *t)
{
    int ok;
    char *u;
    char num[100 + 1];
    char ch;

    /* V.250 6.3.1 - Dial (abortable) */
    at_reset_call_info(s);
    s->do_hangup = FALSE;
    s->silent_dial = FALSE;
    s->command_dial = FALSE;
    t += 1;
    ok = FALSE;
    /* There are a numbers of options in a dial command string.
       Many are completely irrelevant in this application. */
    u = num;
    for (  ;  (ch = *t);  t++)
    {
        if (isdigit((int) ch))
        {
            /* V.250 6.3.1.1 Basic digit set */
            *u++ = ch;
        }
        else
        {
            switch (ch)
            {
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case '*':
            case '#':
                /* V.250 6.3.1.1 Full DTMF repertoire */
                if (!s->p.pulse_dial)
                    *u++ = ch;
                break;
            case ' ':
            case '-':
                /* Ignore spaces and dashes */
                /* This is not a standards based thing. It just improves
                   compatibility with some other modems. */
                break;
            case '+':
                /* V.250 6.3.1.1 International access code */
                /* TODO: */
                break;
            case ',':
                /* V.250 6.3.1.2 Pause */
                /* Pass these through to the application to handle. */
                *u++ = ch;
                break;
            case 'T':
                /* V.250 6.3.1.3 Tone dial */
                s->p.pulse_dial = FALSE;
                break;
            case 'P':
                /* V.250 6.3.1.4 Pulse dial */
                s->p.pulse_dial = TRUE;
                break;
            case '!':
                /* V.250 6.3.1.5 Hook flash, register recall */
                /* TODO: */
                break;
            case 'W':
                /* V.250 6.3.1.6 Wait for dial tone */
                /* TODO: */
                break;
            case '@':
                /* V.250 6.3.1.7 Wait for quiet answer */
                s->silent_dial = TRUE;
                break;
            case 'S':
                /* V.250 6.3.1.8 Invoke stored string */
                /* S=<location> */
                /* TODO: */
                break;
            case 'G':
            case 'g':
                /* GSM07.07 6.2 - Control the CUG supplementary service for this call */
                /* Uses index and info values set with command +CCUG. See +CCUG */
                /* TODO: */
                break;
            case 'I':
            case 'i':
                /* GSM07.07 6.2 - Override Calling Line Identification Restriction (CLIR) */
                /* I=invocation (restrict CLI presentation), i=suppression (allow CLI presentation). See +CLIR */
                /* TODO: */
                break;
            case ';':
                /* V.250 6.3.1 - Dial string terminator - make voice call and remain in command mode */
                s->command_dial = TRUE;
                break;
            case '>':
                /* GSM07.07 6.2 - Direct dialling from phone book supplementary service subscription
                                  default value for this call */
                /* TODO: */
                break;
            default:
                return NULL;
            }
        }
    }
    *u = '\0';
    if ((ok = at_modem_control(s, AT_MODEM_CONTROL_CALL, num)) < 0)
        return NULL;
    /* Dialing should now be in progress. No AT response should be
       issued at this point. */
    return (const char *) -1;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_E(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.4 - Command echo */
    t += 1;
    if ((val = parse_num(&t, 1)) < 0)
        return NULL;
    s->p.echo = val;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_H(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.3.6 - Hook control */
    t += 1;
    if ((val = parse_num(&t, 1)) < 0)
        return NULL;
    if (val)
    {
        /* Take the receiver off-hook, effectively busying-out the modem. */
        if (s->at_rx_mode != AT_MODE_ONHOOK_COMMAND  &&  s->at_rx_mode != AT_MODE_OFFHOOK_COMMAND)
            return NULL;
        at_modem_control(s, AT_MODEM_CONTROL_OFFHOOK, NULL);
        at_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
        return t;
    }
    at_reset_call_info(s);
    if (s->at_rx_mode != AT_MODE_ONHOOK_COMMAND  &&  s->at_rx_mode != AT_MODE_OFFHOOK_COMMAND)
    {
        /* Push out the last of the audio (probably by sending a short silence). */
        at_modem_control(s, AT_MODEM_CONTROL_RESTART, (void *) FAX_MODEM_FLUSH);
        s->do_hangup = TRUE;
        at_set_at_rx_mode(s, AT_MODE_CONNECTED);
        return (const char *) -1;
    }
    at_modem_control(s, AT_MODEM_CONTROL_HANGUP, NULL);
    at_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_I(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.1.3 - Request identification information */
    /* N.B. The information supplied in response to an ATIx command is very
       variable. It was widely used in different ways before the AT command
       set was standardised by the ITU. */
    t += 1;
    switch (val = parse_num(&t, 255))
    {
    case 0:
        at_put_response(s, model);
        break;
    case 3:
        at_put_response(s, manufacturer);
        break;
    default:
        return NULL;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_L(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.3.13 - Monitor speaker loudness */
    /* Just absorb this command, as we have no speaker */
    t += 1;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    s->speaker_volume = val;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_M(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.3.14 - Monitor speaker mode */
    /* Just absorb this command, as we have no speaker */
    t += 1;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    s->speaker_mode = val;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_O(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.3.7 - Return to online data state */
    t += 1;
    if ((val = parse_num(&t, 1)) < 0)
        return NULL;
    if (val == 0)
    {
        at_set_at_rx_mode(s, AT_MODE_CONNECTED);
        at_put_response_code(s, AT_RESPONSE_CODE_CONNECT);
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_P(at_state_t *s, const char *t)
{
    /* V.250 6.3.3 - Select pulse dialling (command) */
    t += 1;
    s->p.pulse_dial = TRUE;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_Q(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.5 - Result code suppression */
    t += 1;
    if ((val = parse_num(&t, 1)) < 0)
        return NULL;
    switch (val)
    {
    case 0:
        s->p.result_code_format = (s->p.verbose)  ?  ASCII_RESULT_CODES  :  NUMERIC_RESULT_CODES;
        break;
    case 1:
        s->p.result_code_format = NO_RESULT_CODES;
        break;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S0(at_state_t *s, const char *t)
{
    /* V.250 6.3.8 - Automatic answer */
    t += 2;
    return s_reg_handler(s, t, 0);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S10(at_state_t *s, const char *t)
{
    /* V.250 6.3.12 - Automatic disconnect delay */
    t += 3;
    return s_reg_handler(s, t, 10);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S3(at_state_t *s, const char *t)
{
    /* V.250 6.2.1 - Command line termination character */
    t += 2;
    return s_reg_handler(s, t, 3);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S4(at_state_t *s, const char *t)
{
    /* V.250 6.2.2 - Response formatting character */
    t += 2;
    return s_reg_handler(s, t, 4);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S5(at_state_t *s, const char *t)
{
    /* V.250 6.2.3 - Command line editing character */
    t += 2;
    return s_reg_handler(s, t, 5);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S6(at_state_t *s, const char *t)
{
    /* V.250 6.3.9 - Pause before blind dialling */
    t += 2;
    return s_reg_handler(s, t, 6);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S7(at_state_t *s, const char *t)
{
    /* V.250 6.3.10 - Connection completion timeout */
    t += 2;
    return s_reg_handler(s, t, 7);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S8(at_state_t *s, const char *t)
{
    /* V.250 6.3.11 - Comma dial modifier time */
    t += 2;
    return s_reg_handler(s, t, 8);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_T(at_state_t *s, const char *t)
{
    /* V.250 6.3.2 - Select tone dialling (command) */
    t += 1;
    s->p.pulse_dial = FALSE;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_V(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.6 - DCE response format */
    t += 1;
    if ((val = parse_num(&t, 1)) < 0)
        return NULL;
    s->p.verbose = val;
    if (s->p.result_code_format != NO_RESULT_CODES)
        s->p.result_code_format = (s->p.verbose)  ?  ASCII_RESULT_CODES  :  NUMERIC_RESULT_CODES;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_X(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.7 - Result code selection and call progress monitoring control */
    /* 0    CONNECT result code is given upon entering online data state.
            Dial tone and busy detection are disabled.
       1    CONNECT <text> result code is given upon entering online data state.
            Dial tone and busy detection are disabled.
       2    CONNECT <text> result code is given upon entering online data state.
            Dial tone detection is enabled, and busy detection is disabled.
       3    CONNECT <text> result code is given upon entering online data state.
            Dial tone detection is disabled, and busy detection is enabled.
       4    CONNECT <text> result code is given upon entering online data state.
            Dial tone and busy detection are both enabled. */
    t += 1;
    if ((val = parse_num(&t, 4)) < 0)
        return NULL;
    s->result_code_mode = val;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_Z(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.1.1 - Reset to default configuration */
    t += 1;
    if ((val = parse_num(&t, sizeof(profiles)/sizeof(profiles[0]) - 1)) < 0)
        return NULL;
    /* Just make sure we are on hook */
    at_modem_control(s, AT_MODEM_CONTROL_HANGUP, NULL);
    at_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
    s->p = profiles[val];
    at_reset_call_info(s);
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_amp_C(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.8 - Circuit 109 (received line signal detector) behaviour */
    /* We have no RLSD pin, so just absorb this. */
    t += 2;
    if ((val = parse_num(&t, 1)) < 0)
        return NULL;
    s->rlsd_behaviour = val;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_amp_D(at_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.9 - Circuit 108 (data terminal ready) behaviour */
    t += 2;
    if ((val = parse_num(&t, 2)) < 0)
        return NULL;
    /* TODO: We have no DTR pin, but we need this to get into online
             command state. */
    s->dtr_behaviour = val;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_amp_F(at_state_t *s, const char *t)
{
    t += 2;

    /* V.250 6.1.2 - Set to factory-defined configuration */
    /* Just make sure we are on hook */
    at_modem_control(s, AT_MODEM_CONTROL_HANGUP, NULL);
    at_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
    s->p = profiles[0];
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8A(at_state_t *s, const char *t)
{
    /* V.251 6.3 - V.8 calling tone indication */
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8C(at_state_t *s, const char *t)
{
    /* V.251 6.2 - V.8 answer signal indication */
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8E(at_state_t *s, const char *t)
{
    int val;

    /* V.251 5.1 - V.8 and V.8bis operation controls */
    /* Syntax: +A8E=<v8o>,<v8a>,<v8cf>[,<v8b>][,<cfrange>][,<protrange>] */
    /* <v8o>=0  Disable V.8 origination negotiation
       <v8o>=1  Enable DCE-controlled V.8 origination negotiation
       <v8o>=2  Enable DTE-controlled V.8 origination negotiation, send V.8 CI only
       <v8o>=3  Enable DTE-controlled V.8 origination negotiation, send 1100Hz CNG only
       <v8o>=4  Enable DTE-controlled V.8 origination negotiation, send 1300Hz CT only
       <v8o>=5  Enable DTE-controlled V.8 origination negotiation, send no tones
       <v8o>=6  Enable DCE-controlled V.8 origination negotiation, issue +A8x indications
       <v8a>=0  Disable V.8 answer negotiation
       <v8a>=1  Enable DCE-controlled V.8 answer negotiation
       <v8a>=2  Enable DTE-controlled V.8 answer negotiation, send ANSam
       <v8a>=3  Enable DTE-controlled V.8 answer negotiation, send no signal
       <v8a>=4  Disable DTE-controlled V.8 answer negotiation, send ANS
       <v8a>=5  Enable DCE-controlled V.8 answer negotiation, issue +A8x indications
       <v8cf>=X..Y Set the V.8 CI signal call function to the hexadecimal octet value X..Y
       <v8b>=0  Disable V.8bis negotiation
       <v8b>=1  Enable DCE-controlled V.8bis negotiation
       <v8b>=2  Enable DTE-controlled V.8bis negotiation
       <cfrange>="<string of values>"   Set to alternative list of call function "option bit"
                                        values that the answering DCE shall accept from the caller
       <protrange>="<string of values>" Set to alternative list of protocol "option bit" values that
                                        the answering DCE shall accept from the caller
    */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, &val, 6, "+A8E:", "(0-6),(0-5),(00-FF)"))
        return NULL;
    if (*t != ',')
        return t;
    if ((val = parse_num(&t, 5)) < 0)
        return NULL;
    if (*t != ',')
        return t;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8I(at_state_t *s, const char *t)
{
    /* V.251 6.1 - V.8 CI signal indication */
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8J(at_state_t *s, const char *t)
{
    /* V.251 6.4 - V.8 negotiation complete */
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8M(at_state_t *s, const char *t)
{
    /* V.251 5.2 - Send V.8 menu signals */
    /* Syntax: +A8M=<hexadecimal coded CM or JM octet string>  */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8R(at_state_t *s, const char *t)
{
    /* V.251 6.6 - V.8bis signal and message reporting */
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8T(at_state_t *s, const char *t)
{
    int val;

    /* V.251 5.3 - Send V.8bis signal and/or message(s) */
    /* Syntax: +A8T=<signal>[,<1st message>][,<2nd message>][,<sig_en>][,<msg_en>][,<supp_delay>] */
    /*  <signal>=0  None
        <signal>=1  Initiating Mre
        <signal>=2  Initiating MRd
        <signal>=3  Initiating CRe, low power
        <signal>=4  Initiating CRe, high power
        <signal>=5  Initiating CRd
        <signal>=6  Initiating Esi
        <signal>=7  Responding MRd, low power
        <signal>=8  Responding MRd, high power
        <signal>=9  Responding CRd
        <signal>=10 Responding Esr
    */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, &val, 10, "+A8T:", "(0-10)"))
        return NULL;
    s->v8bis_signal = val;
    if (*t != ',')
        return t;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    s->v8bis_1st_message = val;
    if (*t != ',')
        return t;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    s->v8bis_2nd_message = val;
    if (*t != ',')
        return t;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    s->v8bis_sig_en = val;
    if (*t != ',')
        return t;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    s->v8bis_msg_en = val;
    if (*t != ',')
        return t;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    s->v8bis_supp_delay = val;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ASTO(at_state_t *s, const char *t)
{
    /* V.250 6.3.15 - Store telephone number */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+ASTO:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAAP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.25 - Automatic answer for eMLPP Service */
    /* TODO: */
    t += 5;
    if (!parse_2_out(s, &t, NULL, 65535, NULL, 65535, "+CAAP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CACM(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.25 - Accumulated call meter */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CACM:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CACSP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.7 - Voice Group or Voice Broadcast Call State Attribute Presentation */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CACSP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAD(at_state_t *s, const char *t)
{
    /* IS-99 5.6.3 - Query analogue or digital service */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAEMLPP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.22 - eMLPP Priority Registration and Interrogation */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CAEMLPP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAHLD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.3 - Leave an ongoing Voice Group or Voice Broadcast Call */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CAHLD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAJOIN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.1 - Accept an incoming Voice Group or Voice Broadcast Call */
    /* TODO: */
    t += 7;
    if (!parse_out(s, &t, NULL, 1, "+CAJOIN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CALA(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.16 - Alarm */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CALA:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CALCC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.6 - List current Voice Group and Voice Broadcast Calls */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CALCC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CALD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.38 - Delete alarm */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CALD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CALM(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.20 - Alert sound mode */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CALM:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAMM(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.26 - Accumulated call meter maximum */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CAMM:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CANCHEV(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.8 - NCH Support Indication */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CANCHEV:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAOC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.16 - Advice of Charge */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CAOC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAPD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.39 - Postpone or dismiss an alarm */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CAPD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAPTT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.4 - Talker Access for Voice Group Call */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CAPTT:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAREJ(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.2 - Reject an incoming Voice Group or Voice Broadcast Call */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CAREJ:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAULEV(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.5 - Voice Group Call Uplink Status Presentation */
    /* TODO: */
    t += 7;
    if (!parse_out(s, &t, NULL, 1, "+CAULEV:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CBC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.4 - Battery charge */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, NULL, 1, "+CBC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CBCS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.3.2 - VBS subscriptions and GId status */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CBCS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CBIP(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Base station IP address */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CBST(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.7 - Select bearer service type */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CBST:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCFC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.11 - Call forwarding number and conditions */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CCFC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCLK(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.15 - Clock */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CCLK:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCS(at_state_t *s, const char *t)
{
    /* IS-135 4.1.22 - Compression status */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCUG(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.10 - Closed user group */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CCUG:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCWA(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.12 - Call waiting */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CCWA:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCWE(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.28 - Call Meter maximum event */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CCWE:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CDIP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.9 - Called line identification presentation */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CDIP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CDIS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.8 - Display control */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CDIS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CDV(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Dial command for voice call */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CEER(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.10 - Extended error report */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CEER:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CESP(at_state_t *s, const char *t)
{
    /* GSM07.05  3.2.4 - Enter SMS block mode protocol */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CFCS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.24 - Fast call setup conditions */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CFCS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CFG(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Configuration string */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CFUN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.2 - Set phone functionality */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CFUN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGACT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.10 - PDP context activate or deactivate */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CGACT:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGANS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.16 - Manual response to a network request for PDP context activation */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CGANS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGATT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.9 - PS attach or detach */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CGATT:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGAUTO(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.15 - Automatic response to a network request for PDP context activation */
    /* TODO: */
    t += 7;
    if (!parse_out(s, &t, NULL, 1, "+CGAUTO:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCAP(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Request complete capabilities list */
    /* TODO: */
    t += 6;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCLASS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.17 - GPRS mobile station class (GPRS only) */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CGCLASS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCLOSP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.13 - Configure local Octet Stream PAD parameters (Obsolete) */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CGCLOSP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCLPAD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.12 - Configure local triple-X PAD parameters (GPRS only) (Obsolete) */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CGCLPAD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCMOD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.11 - PDP Context Modify */
    /* TODO: */
    t += 7;
    if (!parse_out(s, &t, NULL, 1, "+CGCMOD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.3.1 - VGCS subscriptions and GId status */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CGCS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGDATA(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.12 - Enter data state */
    /* TODO: */
    t += 7;
    if (!parse_out(s, &t, NULL, 1, "+CGDATA:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGDCONT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.1 - Define PDP Context */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CGDCONT:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGDSCONT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.2 - Define Secondary PDP Context */
    /* TODO: */
    t += 9;
    if (!parse_out(s, &t, NULL, 1, "+CGDSCONT:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGEQMIN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.7 - 3G Quality of Service Profile (Minimum acceptable) */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CGEQMIN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGEQNEG(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.8 - 3G Quality of Service Profile (Negotiated) */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CGEQNEG:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGEQREQ(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.6 - 3G Quality of Service Profile (Requested) */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CGEQREQ:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGEREP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.18 - Packet Domain event reporting */
    /* TODO: */
    t += 7;
    if (!parse_out(s, &t, NULL, 1, "+CGEREP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGMI(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.1 - Request manufacturer identification */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CGMI:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGMM(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.2 - Request model identification */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CGMM:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGMR(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.3 - Request revision identification */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CGMR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGOI(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Request global object identification */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGPADDR(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.14 - Show PDP address */
    /* TODO: */
    t += 8;
    if (!parse_out(s, &t, NULL, 1, "+CGPADDR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGQMIN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.5 - Quality of Service Profile (Minimum acceptable) */
    /* TODO: */
    t += 7;
    if (!parse_out(s, &t, NULL, 1, "+CGQMIN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGQREQ(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.4 - Quality of Service Profile (Requested) */
    /* TODO: */
    t += 7;
    if (!parse_out(s, &t, NULL, 1, "+CGQREQ:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGREG(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.19 - GPRS network registration status */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CGREG:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGSMS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.20 - Select service for MO SMS messages */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CGSMS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGSN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.4 - Request product serial number identification */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CGSN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGTFT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.3 - Traffic Flow Template */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CGTFT:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHLD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.13 - Call related supplementary services */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CHLD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSA(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.18 - HSCSD non-transparent asymmetry configuration */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CHSA:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.15 - HSCSD current call parameters */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CHSC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.12 - HSCSD device parameters */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CHSD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.14 - HSCSD non-transparent call configuration */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CHSN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSR(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.16 - HSCSD parameters report */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CHSR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHST(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.13 - HSCSD transparent call configuration */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CHST:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSU(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.17 - HSCSD automatic user initiated upgrading */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CHSU:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHUP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.5 - Hangup call */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CHUP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHV(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Hang-up voice */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CIMI(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.6 - Request international mobile subscriber identity */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CIMI:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CIND(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.9 - Indicator control */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CIND:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CIT(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Command state inactivity timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CKPD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.7 - Keypad control */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CKPD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLAC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.37 - List all available AT commands */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CLAC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLAE(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.31 - Language Event */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CLAE:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLAN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.30 - Set Language */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CLAN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLCC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.18 - List current calls */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CLCC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLCK(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.4 - Facility lock */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CLCK:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLIP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.6 - Calling line identification presentation */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CLIP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLIR(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.7 - Calling line identification restriction */
    /* TODO: */
    /* Parameter sets the adjustment for outgoing calls:
        0 presentation indicator is used according to the subscription of the CLIR service
        1 CLIR invocation
        2 CLIR suppression */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CLIR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLVL(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.23 - Loudspeaker volume level */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CLVL:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMAR(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.36 - Master Reset */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CMAR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMEC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.6 - Mobile Termination control mode */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CMEC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMEE(at_state_t *s, const char *t)
{
    /* GSM07.07 9.1 - Report mobile equipment error */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMER(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.10 - Mobile Termination event reporting */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CMER:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMGC(at_state_t *s, const char *t)
{
    /* GSM07.05 3.5.5/4.5 - Send command */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMGD(at_state_t *s, const char *t)
{
    /* GSM07.05 3.5.4 - Delete message */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMGF(at_state_t *s, const char *t)
{
    /* GSM07.05 3.2.3 - Message Format */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMGL(at_state_t *s, const char *t)
{
    /* GSM07.05 3.4.2/4.1 -  List messages */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMGR(at_state_t *s, const char *t)
{
    /* GSM07.05 3.4.3/4.2 - Read message */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMGS(at_state_t *s, const char *t)
{
    /* GSM07.05 3.5.1/4.3 - Send message */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMGW(at_state_t *s, const char *t)
{
    /* GSM07.05 3.5.3/4.4 - Write message to memory */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMIP(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Mobile station IP address */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMM(at_state_t *s, const char *t)
{
    /* IS-135 4.1.23 - Menu map */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMMS(at_state_t *s, const char *t)
{
    /* GSM07.05 3.5.6 - More messages to send */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMOD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.4 - Call mode */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CMOD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMSS(at_state_t *s, const char *t)
{
    /* GSM07.05 3.5.2/4.7 - Send message from storage */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMUT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.24 - Mute control */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CMUT:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CNMA(at_state_t *s, const char *t)
{
    /* GSM07.05 3.4.4/4.6 - New message acknowledgement to terminal adapter */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CNMI(at_state_t *s, const char *t)
{
    /* GSM07.05 3.4.1 - New message indications to terminal equipment */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMUX(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.7 - Multiplexing mode */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CMUX:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CNUM(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.1 - Subscriber number */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CNUM:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_COLP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.8 - Connected line identification presentation */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+COLP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_COPN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.21 - Read operator names */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+COPN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_COPS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.3 - PLMN selection */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+COPS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_COS(at_state_t *s, const char *t)
{
    /* IS-135 4.1.24 - Originating service */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_COTDI(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.9 - Originator to Dispatcher Information */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+COTDI:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPAS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.1 - Phone activity status */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPAS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPBF(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.13 - Find phonebook entries */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPBF:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPBR(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.12 - Read phonebook entries */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPBR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPBS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.11 - Select phonebook memory storage */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPBS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPBW(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.14 - Write phonebook entry */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPBW:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPIN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.3 - Enter PIN */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPIN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPLS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.20 - Selection of preferred PLMN list */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPLS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPMS(at_state_t *s, const char *t)
{
    /* GSM07.05 3.2.2 - Preferred message storage */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPOL(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.19 - Preferred PLMN list */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPOL:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPPS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.23 - eMLPP subscriptions */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPPS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPROT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.42 - Enter protocol mode */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CPROT:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPUC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.27 - Price per unit and currency table */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPUC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPWC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.29 - Power class */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPWC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPWD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.5 - Change password */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CPWD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CQD(at_state_t *s, const char *t)
{
    /* IS-135 4.1.25 - Query disconnect timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CR(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.9 - Service reporting control */
    /* TODO: */
    t += 3;
    if (!parse_out(s, &t, NULL, 1, "+CR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.11 - Cellular result codes */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, NULL, 1, "+CRC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CREG(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.2 - Network registration */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CREG:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRES(at_state_t *s, const char *t)
{
    /* GSM07.05 3.3.6 - Restore Settings */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CRLP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRLP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.8 - Radio link protocol */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CRLP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRM(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Set rm interface protocol */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRMC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.34 - Ring Melody Control */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CRMC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRMP(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.35 - Ring Melody Playback */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CRMP:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRSL(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.21 - Ringer sound level */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CRSL:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRSM(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.18 - Restricted SIM access */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CRSM:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSAS(at_state_t *s, const char *t)
{
    /* GSM07.05 3.3.5 - Save settings */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSCA(at_state_t *s, const char *t)
{
    /* GSM07.05 3.3.1 - Service centre address */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSCB(at_state_t *s, const char *t)
{
    /* GSM07.05 3.3.4 - Select cell broadcast message types */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSCC(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.19 - Secure control command */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSCC:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSCS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.5 - Select TE character set */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSCS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSDF(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.22 - Settings date format */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSDF:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSDH(at_state_t *s, const char *t)
{
    /* GSM07.05 3.3.3 - Show text mode parameters */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSGT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.32 - Set Greeting Text */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSGT:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSIL(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.23 - Silence Command */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSIL:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSIM(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.17 - Generic SIM access */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSIM:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSMP(at_state_t *s, const char *t)
{
    /* GSM07.05 3.3.2 - Set text mode parameters */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSMS(at_state_t *s, const char *t)
{
    /* GSM07.05 3.2.1 - Select Message Service */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSNS(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.19 - Single numbering scheme */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSNS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSQ(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.5 - Signal quality */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, NULL, 1, "+CSQ:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSS(at_state_t *s, const char *t)
{
    /* IS-135 4.1.28 - Serving system identification */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSSN(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.17 - Supplementary service notifications */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSSN:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSTA(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.1 - Select type of address */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSTA:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSTF(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.24 - Settings time format */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSTF:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSVM(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.33 - Set Voice Mail Number */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CSVM:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CTA(at_state_t *s, const char *t)
{
    /* IS-135 4.1.29 - MT-Terminated async. Data calls */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CTF(at_state_t *s, const char *t)
{
    /* IS-135 4.1.30 - MT-Terminated FAX calls */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CTFR(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.14 - Call deflection */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CTFR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CTZR(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.41 - Time Zone Reporting */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CTZR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CTZU(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.40 - Automatic Time Zone Update */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CTZU:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CUSD(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.15 - Unstructured supplementary service data */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CUSD:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CUUS1(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.26 - User to User Signalling Service 1 */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CUUS1:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CV120(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.21 - V.120 rate adaption protocol */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+CV120:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CVHU(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.20 - Voice Hangup Control */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CVHU:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CVIB(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.22 - Vibrator mode */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+CVIB:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CXT(at_state_t *s, const char *t)
{
    /* IS-99 5.6 - Cellular extension */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_DR(at_state_t *s, const char *t)
{
    /* V.250 6.6.2 - Data compression reporting */
    /* TODO: */
    t += 3;
    if (!parse_out(s, &t, NULL, 1, "+DR:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_DS(at_state_t *s, const char *t)
{
    /* V.250 6.6.1 - Data compression */
    /* TODO: */
    t += 3;
    if (!parse_out(s, &t, NULL, 1, "+DS:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_DS44(at_state_t *s, const char *t)
{
    /* V.250 6.6.2 - V.44 data compression */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_EB(at_state_t *s, const char *t)
{
    /* V.250 6.5.2 - Break handling in error control operation */
    /* TODO: */
    t += 3;
    if (!parse_out(s, &t, NULL, 1, "+EB:", ""))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_EFCS(at_state_t *s, const char *t)
{
    /* V.250 6.5.4 - 32-bit frame check sequence */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 2, "+EFCS:", "(0-2)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_EFRAM(at_state_t *s, const char *t)
{
    /* V.250 6.5.8 - Frame length */
    /* TODO: */
    t += 6;
    if (!parse_2_out(s, &t, NULL, 65535, NULL, 65535, "+EFRAM:", "(1-65535),(1-65535)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ER(at_state_t *s, const char *t)
{
    /* V.250 6.5.5 - Error control reporting */
    /*  0   Error control reporting disabled (no +ER intermediate result code transmitted)
        1   Error control reporting enabled (+ER intermediate result code transmitted) */
    /* TODO: */
    t += 3;
    if (!parse_out(s, &t, NULL, 1, "+ER:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ES(at_state_t *s, const char *t)
{
    static const int maxes[3] =
    {
        7, 4, 9
    };
    int *locations[3];

    /* V.250 6.5.1 - Error control selection */

    /* orig_rqst
        0:  Direct mode
        1:  Initiate call with Buffered mode only
        2:  Initiate V.42 without Detection Phase. If Rec. V.8 is in use, this is a request to disable V.42 Detection Phase
        3:  Initiate V.42 with Detection Phase
        4:  Initiate Altemative Protocol
        5:  Initiate Synchronous Mode when connection is completed, immediately after the entire CONNECT result code
            is delivered. V.24 circuits 113 and 115 are activated when Data State is entered
        6:  Initiate Synchronous Access Mode when connection is completed, and Data State is entered
        7:  Initiate Frame Tunnelling Mode when connection is completed, and Data State is entered

       orig_fbk
        0:  Error control optional (either LAPM or Alternative acceptable); if error control not established, maintain
            DTE-DCE data rate and use V.14 buffered mode with flow control during non-error-control operation
        1:  Error control optional (either LAPM or Alternative acceptable); if error control not established, change
            DTE-DCE data rate to match line rate and use Direct mode
        2:  Error control required (either LAPM or Alternative acceptable); if error control not established, disconnect
        3:  Error control required (only LAPM acceptable); if error control not established, disconnect
        4:  Error control required (only Altemative protocol acceptable); if error control not established, disconnect

       ans_fbk
        0:  Direct mode
        1:  Error control disabled, use Buffered mode
        2:  Error control optional (either LAPM or Alternative acceptable); if error control not established, maintain
            DTE-DCE data rate and use local buffering and flow control during non-error-control operation
        3:  Error control optional (either LAPM or Alternative acceptable); if error control not established, change
            DTE-DCE data rate to match line rate and use Direct mode
        4:  Error control required (either LAPM or Alternative acceptable); if error control not established, disconnect
        5:  Error control required (only LAPM acceptable); if error control not established, disconnect
        6:  Error control required (only Alternative protocol acceptable); if error control not established, disconnect
        7:  Initiate Synchronous Mode when connection is completed, immediately after the entire CONNECT result code
            is delivered. V.24 cicuits 113 and 115 are activated when Data State is entered
        8:  Initiate Synchronous Access Mode when connection is completed, and Data State is entered
        9:  Initiate Frame Tunnelling Mode when connection is completed, and Data State is entered */

    /* TODO: */
    t += 3;
    locations[0] = NULL;
    locations[1] = NULL;
    locations[2] = NULL;
    if (!parse_n_out(s, &t, locations, maxes, 3, "+ES:", "(0-7),(0-4),(0-9)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ESA(at_state_t *s, const char *t)
{
    static const int maxes[8] =
    {
        2, 1, 1, 1, 2, 1, 255, 255
    };
    int *locations[8];
    int i;

    /* V.80 8.2 - Synchronous access mode configuration */
    /* TODO: */
    t += 4;
    for (i = 0;  i < 8;  i++)
        locations[i] = NULL;
    if (!parse_n_out(s, &t, locations, maxes, 8, "+ESA:", "(0-2),(0-1),(0-1),(0-1),(0-2),(0-1),(0-255),(0-255)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ESR(at_state_t *s, const char *t)
{
    /* V.250 6.5.3 - Selective repeat */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ETBM(at_state_t *s, const char *t)
{
    /* V.250 6.5.6 - Call termination buffer management */
    /* TODO: */
    t += 5;
    if (!parse_2_out(s, &t, NULL, 2, NULL, 2, "+ETBM:", "(0-2),(0-2),(0-30)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_EWIND(at_state_t *s, const char *t)
{
    /* V.250 6.5.7 - Window size */
    /* TODO: */
    t += 6;
    if (!parse_2_out(s, &t, &s->rx_window, 127, &s->tx_window, 127, "+EWIND:", "(1-127),(1-127)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_F34(at_state_t *s, const char *t)
{
    static const int maxes[5] =
    {
        14, 14, 2, 14, 14
    };
    int *locations[5];
    int i;

    /* T.31 B.6.1 - Initial V.34 rate controls for FAX */
    /* Syntax: +F34=[<maxp>][,[<minp>][,<prefc>][,<maxp2>][,<minp2]] */
    /* TODO */
    t += 4;
    for (i = 0;  i < 5;  i++)
        locations[i] = NULL;
    if (!parse_n_out(s, &t, locations, maxes, 5, "+F34:", "(0-14),(0-14),(0-2),(0-14),(0-14)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FAA(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.5 - Adaptive answer parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FAP(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.12 - Addressing and polling capabilities parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FAR(at_state_t *s, const char *t)
{
    /* T.31 8.5.1 - Adaptive reception control */
    t += 4;
    if (!parse_out(s, &t, &s->p.adaptive_receive, 1, NULL, "0,1"))
         return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FBO(at_state_t *s, const char *t)
{
    /* T.32 8.5.3.4 - Phase C data bit order */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FBS(at_state_t *s, const char *t)
{
    /* T.32 8.5.3.2 - Buffer Size, read only parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FBU(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.10 - HDLC Frame Reporting parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FCC(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.1 - DCE capabilities parameters */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FCL(at_state_t *s, const char *t)
{
    /* T.31 8.5.2 - Carrier loss timeout */
    t += 4;
    if (!parse_out(s, &t, &s->carrier_loss_timeout, 255, NULL, "(0-255)"))
         return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FCLASS(at_state_t *s, const char *t)
{
    /* T.31 8.2 - Capabilities identification and control */
    t += 7;
    /* T.31 says the reply string should be "0,1.0", however making
       it "0,1,1.0" makes things compatible with a lot more software
       that may be expecting a pre-T.31 modem. */
    if (!parse_string_list_out(s, &t, &s->fclass_mode, 1, NULL, "0,1,1.0"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FCQ(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.3 -  Copy quality checking parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FCR(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.9 - Capability to receive parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FCS(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.3 - Current Session Results parameters */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FCT(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.6 - DTE phase C timeout parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FDD(at_state_t *s, const char *t)
{
    /* T.31 8.5.3 - Double escape character replacement */
    t += 4;
    if (!parse_out(s, &t, &s->p.double_escape, 1, NULL, "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FDR(at_state_t *s, const char *t)
{
    /* T.32 8.3.4 - Data reception command */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FDT(at_state_t *s, const char *t)
{
    /* T.32 8.3.3 - Data transmission command */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FEA(at_state_t *s, const char *t)
{
    /* T.32 8.5.3.5 - Phase C received EOL alignment parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FFC(at_state_t *s, const char *t)
{
    /* T.32 8.5.3.6 - Format conversion parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FFD(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.14 - File diagnostic message parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FHS(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.7 - Call termination status parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FIE(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.1 - Procedure interrupt enable parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FIP(at_state_t *s, const char *t)
{
    /* T.32 8.3.6 - Initialize Facsimile Parameters */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FIS(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.2 -  Current session parameters */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FIT(at_state_t *s, const char *t)
{
    /* T.31 8.5.4 - DTE inactivity timeout */
    t += 4;
    if (!parse_2_out(s, &t, &s->dte_inactivity_timeout, 255, &s->dte_inactivity_action, 1, "+FIT:", "(0-255),(0-1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FKS(at_state_t *s, const char *t)
{
    /* T.32 8.3.5 - Session termination command */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FLI(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.5 - Local ID string parameter, TSI or CSI */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FLO(at_state_t *s, const char *t)
{
    /* T.31 Annex A */
    /* Implement something similar to the V.250 +IFC command */
    /*  0:  None.
        1:  XON/XOFF.
        2:  Hardware (default) */
    t += 4;
    span_log(&s->logging, SPAN_LOG_FLOW, "+FLO received\n");
    if (!parse_out(s, &t, &s->dte_dce_flow_control, 2, "+FLO:", "(0-2)"))
        return NULL;
    s->dce_dte_flow_control = s->dte_dce_flow_control;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FLP(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.7 - Indicate document to poll parameter */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FMI(at_state_t *s, const char *t)
{
    /* T.31 says to duplicate +GMI */
    t += 4;
    if (t[0] == '?')
    {
        at_put_response(s, manufacturer);
        t += 1;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FMM(at_state_t *s, const char *t)
{
    /* T.31 says to duplicate +GMM */
    t += 4;
    if (t[0] == '?')
    {
        at_put_response(s, model);
        t += 1;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FMR(at_state_t *s, const char *t)
{
    /* T.31 says to duplicate +GMR */
    t += 4;
    if (t[0] == '?')
    {
        at_put_response(s, revision);
        t += 1;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FMS(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.9 - Minimum phase C speed parameter */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FND(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.10 - Non-Standard Message Data Indication parameter */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FNR(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.11 - Negotiation message reporting control parameters */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FNS(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.6 - Non-Standard Frame FIF parameter */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FPA(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.13 - Selective polling address parameter */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FPI(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.5 - Local Polling ID String parameter */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FPP(at_state_t *s, const char *t)
{
    /* T.32 8.5.3 - Facsimile packet protocol */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FPR(at_state_t *s, const char *t)
{
    /* T.31 Annex A */
    /* Implement something similar to the V.250 +IPR command */
    t += 4;
    if (!parse_out(s, &t, &s->dte_rate, 115200, NULL, "115200"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FPS(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.2 - Page Status parameter */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FPW(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.13 - PassWord parameter (Sending or Polling) */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FRH(at_state_t *s, const char *t)
{
    /* T.31 8.3.6 - HDLC receive */
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FRM(at_state_t *s, const char *t)
{
    /* T.31 8.3.4 - Facsimile receive */
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FRQ(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.4 - Receive Quality Thresholds parameters */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FRS(at_state_t *s, const char *t)
{
    /* T.31 8.3.2 - Receive silence */
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FRY(at_state_t *s, const char *t)
{
    /* T.32 8.5.2.8 - ECM Retry Value parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FSA(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.13 - Subaddress parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FSP(at_state_t *s, const char *t)
{
    /* T.32 8.5.1.8 - Request to poll parameter */
    /* TODO */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FTH(at_state_t *s, const char *t)
{
    /* T.31 8.3.5 - HDLC transmit */
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FTM(at_state_t *s, const char *t)
{
    /* T.31 8.3.3 - Facsimile transmit */
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FTS(at_state_t *s, const char *t)
{
    /* T.31 8.3.1 - Transmit silence */
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GCAP(at_state_t *s, const char *t)
{
    /* V.250 6.1.9 - Request complete capabilities list */
    t += 5;
    /* Response elements
       +FCLASS     +F (FAX) commands
       +MS         +M (modulation control) commands +MS and +MR
       +MV18S      +M (modulation control) commands +MV18S and +MV18R
       +ES         +E (error control) commands +ES, +EB, +ER, +EFCS, and +ETBM
       +DS         +D (data compression) commands +DS and +DR */
    /* TODO: make this adapt to the configuration we really have. */
    if (t[0] == '?')
    {
        at_put_response(s, "+GCAP:+FCLASS");
        t += 1;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GCI(at_state_t *s, const char *t)
{
    /* V.250 6.1.10 - Country of installation, */
    t += 4;
    if (!parse_hex_out(s, &t, &s->country_of_installation, 255, "+GCI:", "(00-FF)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GMI(at_state_t *s, const char *t)
{
    /* V.250 6.1.4 - Request manufacturer identification */
    t += 4;
    if (t[0] == '?')
    {
        at_put_response(s, manufacturer);
        t += 1;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GMM(at_state_t *s, const char *t)
{
    /* V.250 6.1.5 - Request model identification */
    t += 4;
    if (t[0] == '?')
    {
        at_put_response(s, model);
        t += 1;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GMR(at_state_t *s, const char *t)
{
    /* V.250 6.1.6 - Request revision identification */
    t += 4;
    if (t[0] == '?')
    {
        at_put_response(s, revision);
        t += 1;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GOI(at_state_t *s, const char *t)
{
    /* V.250 6.1.8 - Request global object identification */
    /* TODO: */
    t += 4;
    if (t[0] == '?')
    {
        at_put_response(s, GLOBAL_OBJECT_IDENTITY);
        t += 1;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GSN(at_state_t *s, const char *t)
{
    /* V.250 6.1.7 - Request product serial number identification */
    /* TODO: */
    t += 4;
    if (t[0] == '?')
    {
        at_put_response(s, SERIAL_NUMBER);
        t += 1;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IBC(at_state_t *s, const char *t)
{
    static const int maxes[13] =
    {
        2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    int *locations[13];
    int i;

    /* V.80 7.9 - Control of in-band control */
    /* TODO: */
    t += 4;
    /* 0: In-band control service disabled
       1: In-band control service enabled, 7-bit codes allowed, and top bit insignificant
       2; In-band control service enabled, 7-bit codes allowed, and 8-bit codes available

       Circuits 105, 106, 107, 108, 109, 110, 125, 132, 133, 135, 142 in that order. For each one:
       0: disabled
       1: enabled

       DCE line connect status reports:
       0: disabled
       1: enabled */
    for (i = 0;  i < 13;  i++)
        locations[i] = NULL;
    if (!parse_n_out(s, &t, locations, maxes, 13, "+IBC:", "(0-2),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0.1),(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IBM(at_state_t *s, const char *t)
{
    static const int maxes[3] =
    {
        7, 255, 255
    };
    int *locations[3];

    /* V.80 7.10 - In-band MARK idle reporting control */
    /* TODO: */
    t += 4;
    /* Report control
        0: No reports
        1: Report only once when <T1 > expires
        2: Report each time <T2> expires
        3: Report once when <T1> expires, and then each time <T2> expires
        4: Report only when the Mark-ldle Period ends; T3 = the entire interval
        5: Report the first time when <T1> is exceeded, and then once more when the mark idle period ends
        6: Report each time when <T2> is exceeded, and then once more when the mark idle period ends;
           T3 = entire interval -- N*T2
        7: report the first time when <T1> is exceeded, and then each time <T2> is exceeded, and then once
           more when the mark idle period ends; T3 = entire mark idle period -- N*T2 - T1

       T1 in units of 10ms

       T2 in units of 10ms */
    locations[0] = NULL;
    locations[1] = NULL;
    locations[2] = NULL;
    if (!parse_n_out(s, &t, locations, maxes, 3, "+IBM:", "(0-7),(0-255),(0-255)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ICF(at_state_t *s, const char *t)
{
    /* V.250 6.2.11 - DTE-DCE character framing */
    t += 4;
    /* Character format
        0:  auto detect
        1:  8 data 2 stop
        2:  8 data 1 parity 1 stop
        3:  8 data 1 stop
        4:  7 data 2 stop
        5:  7 data 1 parity 1 stop
        6:  7 data 1 stop

       Parity
        0:  Odd
        1:  Even
        2:  Mark
        3:  Space */
    if (!parse_2_out(s, &t, &s->dte_char_format, 6, &s->dte_parity, 3, "+ICF:", "(0-6),(0-3)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ICLOK(at_state_t *s, const char *t)
{
    /* V.250 6.2.14 - Select sync transmit clock source */
    t += 6;
    if (!parse_out(s, &t, &s->sync_tx_clock_source, 2, "+ICLOK:", "(0-2)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IDSR(at_state_t *s, const char *t)
{
    /* V.250 6.2.16 - Select data set ready option */
    t += 5;
    if (!parse_out(s, &t, &s->dsr_option, 2, "+IDSR:", "(0-2)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IFC(at_state_t *s, const char *t)
{
    /* V.250 6.2.12 - DTE-DCE local flow control */
    /*  0:  None.
        1:  XON/XOFF.
        2:  Hardware (default) */
    span_log(&s->logging, SPAN_LOG_FLOW, "+IFC received\n");
    t += 4;
    if (!parse_2_out(s, &t, &s->dte_dce_flow_control, 2, &s->dce_dte_flow_control, 2, "+IFC:", "(0-2),(0-2)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ILRR(at_state_t *s, const char *t)
{
    /* V.250 6.2.13 - DTE-DCE local rate reporting */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ILSD(at_state_t *s, const char *t)
{
    /* V.250 6.2.15 - Select long space disconnect option */
    t += 5;
    if (!parse_out(s, &t, &s->long_space_disconnect_option, 2, "+ILSD:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IPR(at_state_t *s, const char *t)
{
    /* V.250 6.2.10 - Fixed DTE rate */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, &s->dte_rate, 115200, "+IPR:", "(115200),(115200)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IRTS(at_state_t *s, const char *t)
{
    /* V.250 6.2.17 - Select synchronous mode RTS option */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+IRTS:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ITF(at_state_t *s, const char *t)
{
    /* V.80 8.4 - Transmit flow control thresholds */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MA(at_state_t *s, const char *t)
{
    /* V.250 6.4.2 - Modulation automode control */
    /* TODO: */
    t += 3;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MR(at_state_t *s, const char *t)
{
    /* V.250 6.4.3 - Modulation reporting control */
    /*  0:  Disables reporting of modulation connection (+MCR: and +MRR: are not transmitted)
        1:  Enables reporting of modulation connection (+MCR: and +MRR: are transmitted) */
    /* TODO: */
    t += 3;
    if (!parse_out(s, &t, NULL, 1, "+MR:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MS(at_state_t *s, const char *t)
{
    /* V.250 6.4.1 - Modulation selection */
    /* TODO: */
    t += 3;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MSC(at_state_t *s, const char *t)
{
    /* V.250 6.4.8 - Seamless rate change enable */
    /*  0   Disables V.34 seamless rate change
        1   Enables V.34 seamless rate change */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, NULL, 1, "+MSC:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MV18AM(at_state_t *s, const char *t)
{
    /* V.250 6.4.6 - V.18 answering message editing */
    /* TODO: */
    t += 7;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MV18P(at_state_t *s, const char *t)
{
    /* V.250 6.4.7 - Order of probes */
    /*  2    Send probe message in 5-bit (Baudot) mode
        3    Send probe message in DTMF mode
        4    Send probe message in EDT mode
        5    Send Rec. V.21 carrier as a probe
        6    Send Rec. V.23 carrier as a probe
        7    Send Bell 103 carrier as a probe */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 7, "+MV18P:", "(2-7)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MV18R(at_state_t *s, const char *t)
{
    /* V.250 6.4.5 - V.18 reporting control */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+MV18R:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MV18S(at_state_t *s, const char *t)
{
    /* V.250 6.4.4 - V.18 selection */
    /*  mode:
        0    Disables V.18 operation
        1    V.18 operation, auto detect mode
        2    V.18 operation, connect in 5-bit (Baudot) mode
        3    V.18 operation, connect in DTMF mode
        4    V.18 operation, connect in EDT mode
        5    V.18 operation, connect in V.21 mode
        6    V.18 operation, connect in V.23 mode
        7    V.18 operation, connect in Bell 103-type mode

        dflt_ans_mode:
        0    Disables V.18 answer operation
        1    No default specified (auto detect)
        2    V.18 operation connect in 5-bit (Baudot) mode
        3    V.18 operation connect in DTMF mode
        4    V.18 operation connect in EDT mode

        fbk_time_enable:
        0    Disable
        1    Enable

        ans_msg_enable
        0    Disable
        1    Enable

        probing_en
        0    Disable probing
        1    Enable probing
        2    Initiate probing */
    /* TODO: */
    t += 6;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_PCW(at_state_t *s, const char *t)
{
    /* V.250 6.8.1 - Call waiting enable (V.92 DCE) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_PIG(at_state_t *s, const char *t)
{
    /* V.250 6.8.5 - PCM upstream ignore */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_PMH(at_state_t *s, const char *t)
{
    /* V.250 6.8.2 - Modem on hold enable */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_PMHF(at_state_t *s, const char *t)
{
    /* V.250 6.8.6 - V.92 Modem on hold hook flash */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_PMHR(at_state_t *s, const char *t)
{
    /* V.250 6.8.4 - Initiate modem on hold */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_PMHT(at_state_t *s, const char *t)
{
    /* V.250 6.8.3 - Modem on hold timer */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_PQC(at_state_t *s, const char *t)
{
    /* V.250 6.8.7 - V.92 Phase 1 and Phase 2 Control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_PSS(at_state_t *s, const char *t)
{
    /* V.250 6.8.8 - V.92 Use Short Sequence */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SAC(at_state_t *s, const char *t)
{
    /* V.252 3.4 - Audio transmit configuration */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SAM(at_state_t *s, const char *t)
{
    /* V.252 3.5 - Audio receive mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SAR(at_state_t *s, const char *t)
{
    /* V.252 5.3 - Audio receive channel indication */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SARR(at_state_t *s, const char *t)
{
    /* V.252 3.9 - Audio indication reporting */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SAT(at_state_t *s, const char *t)
{
    /* V.252 5.4 - Audio transmit channel indication */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SCRR(at_state_t *s, const char *t)
{
    /* V.252 3.11 - Capabilities indication reporting */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SDC(at_state_t *s, const char *t)
{
    /* V.252 3.3 - Data configuration */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SDI(at_state_t *s, const char *t)
{
    /* V.252 5.2 - Data channel identification */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SDR(at_state_t *s, const char *t)
{
    /* V.252 3.8 - Data indication reporting */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SRSC(at_state_t *s, const char *t)
{
    /* V.252 5.1.2 - Remote terminal simultaneous capability indication */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_STC(at_state_t *s, const char *t)
{
    /* V.252 3.1 - Terminal configuration */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_STH(at_state_t *s, const char *t)
{
    /* V.252 3.2 - Close logical channel */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SVC(at_state_t *s, const char *t)
{
    /* V.252 3.6 - Video transmit configuration */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SVM(at_state_t *s, const char *t)
{
    /* V.252 3.7 - Video receive mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SVR(at_state_t *s, const char *t)
{
    /* V.252 5.5 - Video receive channel indication */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SVRR(at_state_t *s, const char *t)
{
    /* V.252 3.10 - Video indication reporting */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_SVT(at_state_t *s, const char *t)
{
    /* V.252 5.6 - Video transmit channel indication */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TADR(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.9 - Local V.54 address */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TAL(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.15 - Local analogue loop */
    /* Action
        0   Disable analogue loop
        1   Enable analogue loop
       Band
        0   Low frequency band
        1   High frequency band */
    /* TODO: */
    t += 4;
    if (!parse_2_out(s, &t, NULL, 1, NULL, 1, "+TAL:", "(0,1),(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TALS(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.6 - Analogue loop status */
    /*  0   Inactive
        1   V.24 circuit 141 invoked
        2   Front panel invoked
        3   Network management system invoked */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 3, "+TALS:", "(0-3)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TDLS(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.7 - Local digital loop status */
    /*  0   Disabled
        1   Enabled, inactive
        2   Front panel invoked
        3   Network management system invoked
        4   Remote invoked */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 3, "+TDLS:", "(0-4)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TE140(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.1 - Enable ckt 140 */
    /*  0   Disabled
        1   Enabled */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TE140:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TE141(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.2 - Enable ckt 141 */
    /*  0   Response is disabled
        1   Response is enabled */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TE141:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TEPAL(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.5 - Enable front panel analogue loop */
    /*  0   Disabled
        1   Enabled */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TEPAL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TEPDL(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.4 - Enable front panel RDL */
    /*  0   Disabled
        1   Enabled */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TEPDL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TERDL(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.3 - Enable RDL from remote */
    /*  0   Local DCE will ignore command from remote
        1   Local DCE will obey command from remote */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TERDL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TLDL(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.13 - Local digital loop */
    /*  0   Stop test
        1   Start test */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+TLDL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TMO(at_state_t *s, const char *t)
{
    /* V.250 6.9 - V.59 command */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TMODE(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.10 - Set V.54 mode */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TMODE:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TNUM(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.12 - Errored bit and block counts */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TRDL(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.14 - Request remote digital loop */
    /*  0   Stop RDL
        1   Start RDL */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+TRDL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TRDLS(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.8 - Remote digital loop status */
    /* TODO: */
    t += 6;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TRES(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.17 - Self test result */
    /*  0   No test
        1   Pass
        2   Fail */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+TRES:", "(0-2)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TSELF(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.16 - Self test */
    /*  0   Intrusive full test
        1   Safe partial test */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TSELF:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TTER(at_state_t *s, const char *t)
{
    /* V.250 6.7.2.11 - Test error rate */
    /* TODO: */
    t += 5;
    if (!parse_2_out(s, &t, NULL, 65535, NULL, 65535, "+TTER:", "(0-65535),(0-65535)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VAC(at_state_t *s, const char *t)
{
    /* V.252 4.1 - Set audio code */
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VACR(at_state_t *s, const char *t)
{
    /* V.252 6.1 - Audio code report */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VBT(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 C.2.2 - Buffer threshold setting */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VCID(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 C.2.3 - Calling number ID presentation */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, &s->display_call_info, 1, NULL, "0,1"))
         return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VCIDR(at_state_t *s, const char *t)
{
    /* V.252 6.2 - Caller ID report */
    /* TODO: */
    t += 6;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VDID(at_state_t *s, const char *t)
{
    /* V.253 9.2.4 - DID service */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VDIDR(at_state_t *s, const char *t)
{
    /* V.252 6.2 - DID report */
    /* TODO: */
    t += 6;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VDR(at_state_t *s, const char *t)
{
    /* V.253 10.3.1 - Distinctive ring (ring cadence reporting) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VDT(at_state_t *s, const char *t)
{
    /* V.253 10.3.2 - Control tone cadence reporting */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VDX(at_state_t *s, const char *t)
{
    /* V.253 10.5.6 - Speakerphone duplex mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VEM(at_state_t *s, const char *t)
{
    /* V.253 10.5.7 - Deliver event reports */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VGM(at_state_t *s, const char *t)
{
    /* V.253 10.5.2 - Microphone gain */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VGR(at_state_t *s, const char *t)
{
    /* V.253 10.2.1 - Receive gain selection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VGS(at_state_t *s, const char *t)
{
    /* V.253 10.5.3 - Speaker gain */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VGT(at_state_t *s, const char *t)
{
    /* V.253 10.2.2 - Volume selection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VHC(at_state_t *s, const char *t)
{
    /* V.252 4.12 - Telephony port hook control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VIP(at_state_t *s, const char *t)
{
    /* V.253 10.1.1 - Initialize voice parameters */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VIT(at_state_t *s, const char *t)
{
    /* V.253 10.2.3 - DTE/DCE inactivity timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VLS(at_state_t *s, const char *t)
{
    /* V.253 10.2.4 - Analogue source/destination selection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VNH(at_state_t *s, const char *t)
{
    /* V.253 9.2.5 - Automatic hangup control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VPH(at_state_t *s, const char *t)
{
    /* V.252 4.11 - Phone hookswitch status */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VPP(at_state_t *s, const char *t)
{
    /* V.253 10.4.2 - Voice packet protocol */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VPR(at_state_t *s, const char *t)
{
    /* IS-101 10.4.3 - Select DTE/DCE interface rate */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VRA(at_state_t *s, const char *t)
{
    /* V.253 10.2.5 - Ringing tone goes away timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VRID(at_state_t *s, const char *t)
{
    int val;

    /* Extension of V.253 +VCID, Calling number ID report/repeat */
    t += 5;
    val = 0;
    if (!parse_out(s, &t, &val, 1, NULL, "0,1"))
         return NULL;
    if (val == 1)
        at_display_call_info(s);
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VRL(at_state_t *s, const char *t)
{
    /* V.253 10.1.2 - Ring local phone */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VRN(at_state_t *s, const char *t)
{
    /* V.253 10.2.6 - Ringing tone never appeared timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VRX(at_state_t *s, const char *t)
{
    /* V.253 10.1.3 - Voice receive state */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VSD(at_state_t *s, const char *t)
{
    /* V.253 10.2.7 - Silence detection (QUIET and SILENCE) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VSID(at_state_t *s, const char *t)
{
    /* Extension of V.253 +VCID, Set calling number ID */
    t += 5;
    if (!parse_string_out(s, &t, &s->local_id, NULL))
        return NULL;
    if (at_modem_control(s, AT_MODEM_CONTROL_SETID, s->local_id) < 0)
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VSM(at_state_t *s, const char *t)
{
    /* V.253 10.2.8 - Compression method selection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VSP(at_state_t *s, const char *t)
{
    /* V.253 10.5.1 - Voice speakerphone state */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTA(at_state_t *s, const char *t)
{
    /* V.253 10.5.4 - Train acoustic echo-canceller */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTD(at_state_t *s, const char *t)
{
    /* V.253 10.2.9 - Beep tone duration timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTER(at_state_t *s, const char *t)
{
    /* V.252 6.4 - Simple telephony event report */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTH(at_state_t *s, const char *t)
{
    /* V.253 10.5.5 - Train line echo-canceller */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTR(at_state_t *s, const char *t)
{
    /* V.253 10.1.4 - Voice duplex state */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTS(at_state_t *s, const char *t)
{
    /* V.253 10.1.5 - DTMF and tone generation in voice */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTX(at_state_t *s, const char *t)
{
    /* V.253 10.1.6 - Transmit data state */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VXT(at_state_t *s, const char *t)
{
    /* IS-101 10.1.5 - Translate voice data */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_W(at_state_t *s, const char *t)
{
    /* TIA-678 5.2.4.1 - Compliance indication */
    /* TODO: */
    t += 2;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WBAG(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.6 Bias Modem Audio Gain */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WCDA(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.2.5 Display Data Link Address */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WCHG(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.2.4 Display Battery Charging Status */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WCID(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.2.1 Display System ID (operator) */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WCLK(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.2.3 Lock/Unlock DCE */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WCPN(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.2.2 Set Personal Identification Number */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WCXF(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.2.6 Display Supported Annex B commands */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WDAC(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.1 Data over Analogue Cellular Command Query */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WDIR(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.8 Phone Number Directory Selection */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WECR(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.3 Enable Cellular Result Codes */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WFON(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.5 Phone Specification */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WKPD(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.7 Keypad Emulation */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WPBA(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.9 Phone Battery Query */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WPTH(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.10 Call Path */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WRLK(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.4 Roam Lockout */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS45(at_state_t *s, const char *t)
{
    /* TIA-678 5.2.4.2 DTE-side stack selection */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS46(at_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.9 - PCCA STD-101 [17] select wireless network */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS50(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.1.1 Normalized Signal Strength */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS51(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.1.2 Carrier Detect Signal Threshold */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS52(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.1.3 Normalized Battery Level */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS53(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.1.4 Normalized Channel Quality */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS54(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.1.5 Carrier Detect Channel Quality Threshold */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS57(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.1.7 Antenna Preference */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS58(at_state_t *s, const char *t)
{
    /* TIA-678 B.3.1.8 Idle Time-out Value */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WSTL(at_state_t *s, const char *t)
{
    /* TIA-678 C.5.2 Call Session Time Limit */
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

/*
    AT command group prefixes:

    +A    Call control (network addressing) issues, common, PSTN, ISDN, Rec. X.25, switched digital
    +C    Digital cellular extensions
    +D    Data compression, Rec. V.42bis
    +E    Error control, Rec. V.42
    +F    Facsimile, Rec. T.30, etc.
    +G    Generic issues such as identity and capabilities
    +I    DTE-DCE interface issues, Rec. V.24, etc.
    +M    Modulation, Rec. V.32bis, etc.
    +S    Switched or simultaneous data types
    +T    Test issues
    +V    Voice extensions
    +W    Wireless extensions
*/

#include "at_interpreter_dictionary.h"

static int command_search(const char *u, int *matched)
{
    int i;
    int index;
    int first;
    int last;
    int entry;
    int ptr;

    entry = 0;
    /* Loop over the length of the string to search the trie... */
    for (i = 0, ptr = 0;  ptr < COMMAND_TRIE_LEN;  i++)
    {
        /* The character in u we are processing... */
        /* V.250 5.4.1 says upper and lower case are equivalent in commands */
        index = toupper((int) u[i]);
        /* Is there a child node for this character? */
        /* Note: First and last could have been packed into one uint16_t,
           but space is not that critical, so the other packing is good
           enough to make the table reasonable. */
        first = command_trie[ptr++];
        last = command_trie[ptr++];
        entry = command_trie[ptr++];
        if (index < first  ||  index > last)
            break;
        if ((ptr = command_trie[ptr + index - first]) == 0)
            break;
        ptr--;
    }
    *matched = i;
    return entry;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) at_modem_control(at_state_t *s, int op, const char *num)
{
    switch (op)
    {
    case AT_MODEM_CONTROL_ANSWER:
        break;
    case AT_MODEM_CONTROL_CALL:
        break;
    case AT_MODEM_CONTROL_HANGUP:
        break;
    case AT_MODEM_CONTROL_OFFHOOK:
        break;
    case AT_MODEM_CONTROL_DTR:
        break;
    case AT_MODEM_CONTROL_RTS:
        break;
    case AT_MODEM_CONTROL_CTS:
        break;
    case AT_MODEM_CONTROL_CAR:
        break;
    case AT_MODEM_CONTROL_RNG:
        break;
    case AT_MODEM_CONTROL_DSR:
        break;
    case AT_MODEM_CONTROL_RESTART:
        break;
    default:
        break;
    }
    /*endswitch*/
    return s->modem_control_handler(s, s->modem_control_user_data, op, num);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_interpreter(at_state_t *s, const char *cmd, int len)
{
    int i;
    int c;
    int entry;
    int matched;
    const char *t;

    if (s->p.echo)
        s->at_tx_handler(s, s->at_tx_user_data, (uint8_t *) cmd, len);

    for (i = 0;  i < len;  i++)
    {
        /* The spec says the top bit should be ignored */
        c = *cmd++ & 0x7F;
        /* Handle incoming character */
        if (s->line_ptr < 2)
        {
            /* Look for the initial "at", "AT", "a/" or "A/", and ignore anything before it */
            /* V.250 5.2.1 only shows "at" and "AT" as command prefixes. "At" and "aT" are
               not specified, despite 5.4.1 saying upper and lower case are equivalent in
               commands. Let's be tolerant and accept them. */
            if (tolower(c) == 'a')
            {
                s->line_ptr = 0;
                s->line[s->line_ptr++] = (char) toupper(c);
            }
            else if (s->line_ptr == 1)
            {
                if (tolower(c) == 't')
                {
                    /* We have an "AT" command */
                    s->line[s->line_ptr++] = (char) toupper(c);
                }
                else if (c == '/')
                {
                    /* We have an "A/" command */
                    /* TODO: implement "A/" command repeat */
                    s->line[s->line_ptr++] = (char) c;
                }
                else
                {
                    s->line_ptr = 0;
                }
            }
        }
        else
        {
            /* We are beyond the initial AT */
            if (c >= 0x20  &&  c <= 0x7E)
            {
                /* Add a new char */
                if (s->line_ptr < (int) (sizeof(s->line) - 1))
                    s->line[s->line_ptr++] = (char) toupper(c);
            }
            else if (c == s->p.s_regs[3])
            {
                /* End of command line. Do line validation */
                s->line[s->line_ptr] = '\0';
                if (s->line_ptr > 2)
                {
                    /* The spec says the commands within a command line are executed in order, until
                       an error is found, or the end of the command line is reached. */
                    t = s->line + 2;
                    while (t  &&  *t)
                    {
                        if ((entry = command_search(t, &matched)) <= 0)
                            break;
                        /* The following test shouldn't be needed, but let's keep it here for completeness. */
                        if (entry > sizeof(at_commands)/sizeof(at_commands[0]))
                            break;
                        if ((t = at_commands[entry - 1](s, t)) == NULL)
                            break;
                        if (t == (const char *) -1)
                            break;
                    }
                    if (t != (const char *) -1)
                    {
                        if (t == NULL)
                            at_put_response_code(s, AT_RESPONSE_CODE_ERROR);
                        else
                            at_put_response_code(s, AT_RESPONSE_CODE_OK);
                    }
                }
                else if (s->line_ptr == 2)
                {
                    /* It's just an empty "AT" command, return OK. */
                    at_put_response_code(s, AT_RESPONSE_CODE_OK);
                }
                s->line_ptr = 0;
            }
            else if (c == s->p.s_regs[5])
            {
                /* Command line editing character (backspace) */
                if (s->line_ptr > 0)
                    s->line_ptr--;
            }
            else
            {
                /* The spec says control characters, other than those
                   explicitly handled, should be ignored, and so this
                   invalid character causes everything buffered
                   before it to also be ignored. */
                s->line_ptr = 0;
            }
        }
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) at_set_class1_handler(at_state_t *s, at_class1_handler_t handler, void *user_data)
{
    s->class1_handler = handler;
    s->class1_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) at_get_logging_state(at_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(at_state_t *) at_init(at_state_t *s,
                                   at_tx_handler_t at_tx_handler,
                                   void *at_tx_user_data,
                                   at_modem_control_handler_t modem_control_handler,
                                   void *modem_control_user_data)
{
    if (s == NULL)
    {
        if ((s = (at_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, '\0', sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "AT");
    s->modem_control_handler = modem_control_handler;
    s->modem_control_user_data = modem_control_user_data;
    s->at_tx_handler = at_tx_handler;
    s->at_tx_user_data = at_tx_user_data;
    s->call_id = NULL;
    s->local_id = NULL;
    s->display_call_info = 0;
    s->dte_dce_flow_control = 2;
    s->dce_dte_flow_control = 2;
    at_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
    s->p = profiles[0];
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) at_release(at_state_t *s)
{
    at_reset_call_info(s);
    if (s->local_id)
        free(s->local_id);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) at_free(at_state_t *s)
{
    int ret;

    ret = at_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
