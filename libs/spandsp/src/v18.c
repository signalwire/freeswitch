/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v18.c - V.18 text telephony for the deaf.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004-2009 Steve Underwood
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
 * $Id: v18.c,v 1.12 2009/11/04 15:52:06 steveu Exp $
 */
 
/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/async.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/power_meter.h"
#include "spandsp/fsk.h"
#include "spandsp/dtmf.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/v8.h"
#include "spandsp/v18.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/queue.h"
#include "spandsp/private/tone_generate.h"
#include "spandsp/private/async.h"
#include "spandsp/private/fsk.h"
#include "spandsp/private/dtmf.h"
#include "spandsp/private/modem_connect_tones.h"
#include "spandsp/private/v18.h"

#include <stdlib.h>

/*! The baudot code to shift from alpha to digits and symbols */
#define BAUDOT_FIGURE_SHIFT     0x1B
/*! The baudot code to shift from digits and symbols to alpha */
#define BAUDOT_LETTER_SHIFT     0x1F

struct dtmf_to_ascii_s
{
    const char *dtmf;
    char ascii;
};

static const struct dtmf_to_ascii_s dtmf_to_ascii[] =
{
    {"###1", 'C'},
    {"###2", 'F'},
    {"###3", 'I'},
    {"###4", 'L'},
    {"###5", 'O'},
    {"###6", 'R'},
    {"###7", 'U'},
    {"###8", 'X'},
    {"###9", ';'},
    {"###0", '!'},
    {"##*1", 'A'},
    {"##*2", 'D'},
    {"##*3", 'G'},
    {"##*4", 'J'},
    {"##*5", 'M'},
    {"##*6", 'P'},
    {"##*7", 'S'},
    {"##*8", 'V'},
    {"##*9", 'Y'},
    {"##1", 'B'},
    {"##2", 'E'},
    {"##3", 'H'},
    {"##4", 'K'},
    {"##5", 'N'},
    {"##6", 'Q'},
    {"##7", 'T'},
    {"##8", 'W'},
    {"##9", 'Z'},
    {"##0", ' '},
#if defined(WIN32)
    {"#*1", 'X'}, // (Note 1) 111 1011
    {"#*2", 'X'}, // (Note 1) 111 1100
    {"#*3", 'X'}, // (Note 1) 111 1101
    {"#*4", 'X'}, // (Note 1) 101 1011
    {"#*5", 'X'}, // (Note 1) 101 1100
    {"#*6", 'X'}, // (Note 1) 101 1101
#else
    {"#*1", 'æ'}, // (Note 1) 111 1011
    {"#*2", 'ø'}, // (Note 1) 111 1100
    {"#*3", 'å'}, // (Note 1) 111 1101
    {"#*4", 'Æ'}, // (Note 1) 101 1011
    {"#*5", 'Ø'}, // (Note 1) 101 1100
    {"#*6", 'Å'}, // (Note 1) 101 1101
#endif
    {"#0", '?'},
    {"#1", 'c'},
    {"#2", 'f'},
    {"#3", 'i'},
    {"#4", 'l'},
    {"#5", 'o'},
    {"#6", 'r'},
    {"#7", 'u'},
    {"#8", 'x'},
    {"#9", '.'},
    {"*#0", '0'},
    {"*#1", '1'},
    {"*#2", '2'},
    {"*#3", '3'},
    {"*#4", '4'},
    {"*#5", '5'},
    {"*#6", '6'},
    {"*#7", '7'},
    {"*#8", '8'},
    {"*#9", '9'},
    {"**1", '+'},
    {"**2", '-'},
    {"**3", '='},
    {"**4", ':'},
    {"**5", '%'},
    {"**6", '('},
    {"**7", ')'},
    {"**8", ','},
    {"**9", '\n'},
    {"*0", '\b'},
    {"*1", 'a'},
    {"*2", 'd'},
    {"*3", 'g'},
    {"*4", 'j'},
    {"*5", 'm'},
    {"*6", 'p'},
    {"*7", 's'},
    {"*8", 'v'},
    {"*9", 'y'},
    {"0", ' '},
    {"1", 'b'},
    {"2", 'e'},
    {"3", 'h'},
    {"4", 'k'},
    {"5", 'n'},
    {"6", 'q'},
    {"7", 't'},
    {"8", 'w'},
    {"9", 'z'},
    {"", '\0'}
};

static const char *ascii_to_dtmf[128] =
{
    "",         /* NULL */
    "",         /* SOH */
    "",         /* STX */
    "",         /* ETX */
    "",         /* EOT */
    "",         /* ENQ */
    "",         /* ACK */
    "",         /* BEL */
    "*0",       /* BACK SPACE */
    "0",        /* HT >> SPACE */
    "**9",      /* LF */
    "**9",      /* VT >> LF */
    "**9",      /* FF >> LF */
    "",         /* CR */
    "",         /* SO */
    "",         /* SI */
    "",         /* DLE */
    "",         /* DC1 */
    "",         /* DC2 */
    "",         /* DC3 */
    "",         /* DC4 */
    "",         /* NAK */
    "",         /* SYN */
    "",         /* ETB */
    "",         /* CAN */
    "",         /* EM */
    "#0",       /* SUB >> ? */
    "",         /* ESC */
    "**9",      /* IS4 >> LF */
    "**9",      /* IS3 >> LF */
    "**9",      /* IS2 >> LF */
    "0",        /* IS1 >> SPACE */
    "0",        /* SPACE */
    "###0",     /* ! */
    "",         /* " */
    "",         /* # */
    "",         /* $ */
    "**5",      /* % */
    "**1",      /* & >> + */
    "",         /* ’ */
    "**6",      /* ( */
    "**7",      /* ) */
    "#9",       /* _ >> . */
    "**1",      /* + */
    "**8",      /* , */
    "**2",      /* - */
    "#9",       /* . */
    "",         /* / */
    "*#0",      /* 0 */
    "*#1",      /* 1 */
    "*#2",      /* 2 */
    "*#3",      /* 3 */
    "*#4",      /* 4 */
    "*#5",      /* 5 */
    "*#6",      /* 6 */
    "*#7",      /* 7 */
    "*#8",      /* 8 */
    "*#9",      /* 9 */
    "**4",      /* : */
    "###9",     /* ; */
    "**6",      /* < >> ( */
    "**3",      /* = */
    "**7",      /* > >> ) */
    "#0",       /* ? */
    "###8",     /* @ >> X */
    "##*1",     /* A */
    "##1",      /* B */
    "###1",     /* C */
    "##*2",     /* D */
    "##2",      /* E */
    "###2",     /* F */
    "##*3",     /* G */
    "##3",      /* H */
    "###3",     /* I */
    "##*4",     /* J */
    "##4",      /* K */
    "###4",     /* L */
    "##*5",     /* M */
    "##5",      /* N */
    "###5",     /* O */
    "##*6",     /* P */
    "##6",      /* Q */
    "###6",     /* R */
    "##*7",     /* S */
    "##7",      /* T */
    "###7",     /* U */
    "##*8",     /* V */
    "##8",      /* W */
    "###8",     /* X */
    "##*9",     /* Y */
    "##9",      /* Z */
    "#*4",      /* Æ (National code) */
    "#*5",      /* Ø (National code) */
    "#*6",      /* Å (National code) */
    "",         /* ^ */
    "0",        /* _ >> SPACE */
    "",         /* ’ */
    "*1",       /* a */
    "1",        /* b */
    "#1",       /* c */
    "*2",       /* d */
    "2",        /* e */
    "#2",       /* f */
    "*3",       /* g */
    "3",        /* h */
    "#3",       /* i */
    "*4",       /* j */
    "4",        /* k */
    "#4",       /* l */
    "*5",       /* m */
    "5",        /* n */
    "#5",       /* o */
    "*6",       /* p */
    "6",        /* q */
    "#6",       /* r */
    "*7",       /* s */
    "7",        /* t */
    "#7",       /* u */
    "*8",       /* v */
    "8",        /* w */
    "#8",       /* x */
    "*9",       /* y */
    "9",        /* z */
    "#*1",      /* æ (National code) */
    "#*2",      /* ø (National code) */
    "#*3",      /* å (National code) */
    "0",        /* ~ >> SPACE */
    "*0"        /* DEL >> BACK SPACE */
};

static int cmp(const void *s, const void *t)
{
    const char *ss;
    struct dtmf_to_ascii_s *tt;

    ss = (const char *) s;
    tt = (struct dtmf_to_ascii_s *) t;
    return strncmp(ss, tt->dtmf, strlen(tt->dtmf));
}

SPAN_DECLARE(int) v18_encode_dtmf(v18_state_t *s, char dtmf[], const char msg[])
{
    const char *t;
    char *u;
    const char *v;
    
    t = msg;
    u = dtmf;
    while (*t)
    {
        v = ascii_to_dtmf[*t & 0x7F];
        while (*v)
            *u++ = *v++;
        t++;
    }
    *u = '\0';

    return u - dtmf;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_decode_dtmf(v18_state_t *s, char msg[], const char dtmf[])
{
    int entries;
    const char *t;
    char *u;
    struct dtmf_to_ascii_s *ss;

    entries = sizeof(dtmf_to_ascii)/sizeof(dtmf_to_ascii[0]) - 1;
    t = dtmf;
    u = msg;
    while (*t)
    {
        ss = bsearch(t, dtmf_to_ascii, entries, sizeof(dtmf_to_ascii[0]), cmp);
        if (ss)
        {
            t += strlen(ss->dtmf);
            *u++ = ss->ascii;
        }
        else
        {
            /* Can't match the code. Let's assume this is a code we just don't know, and skip over it */
            while (*t == '#'  ||  *t == '*')
                t++;
            if (*t)
                t++;
        }
    }
    *u = '\0';
    return u - msg;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint16_t) v18_encode_baudot(v18_state_t *s, uint8_t ch)
{
    static const uint8_t conv[128] =
    {
        0xFF, /* NUL */
        0xFF, /* SOH */
        0xFF, /* STX */
        0xFF, /* ETX */
        0xFF, /* EOT */
        0xFF, /* ENQ */
        0xFF, /* ACK */
        0xFF, /* BEL */
        0x00, /* BS */
        0x44, /* HT >> SPACE */
        0x42, /* LF */
        0x42, /* VT >> LF */
        0x42, /* FF >> LF */
        0x48, /* CR */
        0xFF, /* SO */
        0xFF, /* SI */
        0xFF, /* DLE */
        0xFF, /* DC1 */
        0xFF, /* DC2 */
        0xFF, /* DC3 */
        0xFF, /* DC4 */
        0xFF, /* NAK */
        0xFF, /* SYN */
        0xFF, /* ETB */
        0xFF, /* CAN */
        0xFF, /* EM */
        0x99, /* SUB >> ? */
        0xFF, /* ESC */
        0x42, /* IS4 >> LF */
        0x42, /* IS3 >> LF */
        0x42, /* IS2 >> LF */
        0x44, /* IS1 >> SPACE */
        0x44, /*   */
        0x8D, /* ! */
        0x91, /* " */
        0x89, /* # >> $ */
        0x89, /* $ */
        0x9D, /* % >> / */
        0x9A, /* & >> + */
        0x8B, /* ' */
        0x8F, /* ( */
        0x92, /* ) */
        0x9C, /* * >> . */
        0x9A, /* + */
        0x8C, /* , */
        0x83, /* - */
        0x9C, /* . */
        0x9D, /* / */
        0x96, /* 0 */
        0x97, /* 1 */
        0x93, /* 2 */
        0x81, /* 3 */
        0x8A, /* 4 */
        0x90, /* 5 */
        0x95, /* 6 */
        0x87, /* 7 */
        0x86, /* 8 */
        0x98, /* 9 */
        0x8E, /* : */
        0x9E, /* ; */
        0x8F, /* < >> )*/
        0x94, /* = */
        0x92, /* > >> ( */
        0x99, /* ? */
        0x1D, /* @ >> X */
        0x03, /* A */
        0x19, /* B */
        0x0E, /* C */
        0x09, /* D */
        0x01, /* E */
        0x0D, /* F */
        0x1A, /* G */
        0x14, /* H */
        0x06, /* I */
        0x0B, /* J */
        0x0F, /* K */
        0x12, /* L */
        0x1C, /* M */
        0x0C, /* N */
        0x18, /* O */
        0x16, /* P */
        0x17, /* Q */
        0x0A, /* R */
        0x05, /* S */
        0x10, /* T */
        0x07, /* U */
        0x1E, /* V */
        0x13, /* W */
        0x1D, /* X */
        0x15, /* Y */
        0x11, /* Z */
        0x8F, /* [ >> (*/
        0x9D, /* \ >> / */
        0x92, /* ] >> ) */
        0x8B, /* ^ >> ' */
        0x44, /* _ >> Space */
        0x8B, /* ` >> ' */
        0x03, /* a */
        0x19, /* b */
        0x0E, /* c */
        0x09, /* d */
        0x01, /* e */
        0x0D, /* f */
        0x1A, /* g */
        0x14, /* h */
        0x06, /* i */
        0x0B, /* j */
        0x0F, /* k */
        0x12, /* l */
        0x1C, /* m */
        0x0C, /* n */
        0x18, /* o */
        0x16, /* p */
        0x17, /* q */
        0x0A, /* r */
        0x05, /* s */
        0x10, /* t */
        0x07, /* u */
        0x1E, /* v */
        0x13, /* w */
        0x1D, /* x */
        0x15, /* y */
        0x11, /* z */
        0x8F, /* { >> ( */
        0x8D, /* | >> ! */
        0x92, /* } >> ) */
        0x44, /* ~ >> Space */
        0xFF, /* DEL */
    };
    uint16_t shift;

    if (ch == 0x7F)
    {
        /* DLE is a special character meaning "force a
           change to the letter character set */
        shift = BAUDOT_LETTER_SHIFT;
        return 0;
    }
    ch = conv[ch];
    /* Is it a non-existant code? */
    if (ch == 0xFF)
        return 0;
    /* Is it a code present in both character sets? */
    if ((ch & 0x40))
        return 0x8000 | (ch & 0x1F);
    /* Need to allow for a possible character set change. */
    if ((ch & 0x80))
    {
        if (s->baudot_tx_shift == 1)
            return ch & 0x1F;
        s->baudot_tx_shift = 1;
        shift = BAUDOT_FIGURE_SHIFT;
    }
    else
    {
        if (s->baudot_tx_shift == 0)
            return ch & 0x1F;
        s->baudot_tx_shift = 0;
        shift = BAUDOT_LETTER_SHIFT;
    }
    return 0x8000 | (shift << 5) | (ch & 0x1F);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint8_t) v18_decode_baudot(v18_state_t *s, uint8_t ch)
{
    static const uint8_t conv[2][32] =
    {
        {"\bE\nA SIU\rDRJNFCKTZLWHYPQOBG^MXV^"},
        {"\b3\n- -87\r$4',!:(5\")2=6019?+^./;^"}
    };

    switch (ch)
    {
    case BAUDOT_FIGURE_SHIFT:
        s->baudot_rx_shift = 1;
        break;
    case BAUDOT_LETTER_SHIFT:
        s->baudot_rx_shift = 0;
        break;
    default:
        return conv[s->baudot_rx_shift][ch];
    }
    /* return 0 if we did not produce a character */
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void v18_rx_dtmf(void *user_data, const char digits[], int len)
{
    v18_state_t *s;

    s = (v18_state_t *) user_data;
}
/*- End of function --------------------------------------------------------*/

static int v18_tdd_get_async_byte(void *user_data)
{
    v18_state_t *s;
    int ch;
    
    s = (v18_state_t *) user_data;
    if ((ch = queue_read_byte(&s->queue.queue)) >= 0)
    {
        int space;
        int cont;
        space = queue_free_space(&s->queue.queue);
        cont = queue_contents(&s->queue.queue);
        return ch;
    }
    if (s->tx_signal_on)
    {
        /* The FSK should now be switched off. */
        s->tx_signal_on = FALSE;
    }
    return 0x1F;
}
/*- End of function --------------------------------------------------------*/

static void v18_tdd_put_async_byte(void *user_data, int byte)
{
    v18_state_t *s;
    uint8_t octet;
    
    s = (v18_state_t *) user_data;
    //printf("Rx byte %x\n", byte);
    if (byte < 0)
    {
        /* Special conditions */
        span_log(&s->logging, SPAN_LOG_FLOW, "V.18 signal status is %s (%d)\n", signal_status_to_str(byte), byte);
        switch (byte)
        {
        case SIG_STATUS_CARRIER_UP:
            s->consecutive_ones = 0;
            s->bit_pos = 0;
            s->in_progress = 0;
            s->rx_msg_len = 0;
            break;
        case SIG_STATUS_CARRIER_DOWN:
            if (s->rx_msg_len > 0)
            {
                /* Whatever we have to date constitutes the message */
                s->rx_msg[s->rx_msg_len] = '\0';
                s->put_msg(s->user_data, s->rx_msg, s->rx_msg_len);
                s->rx_msg_len = 0;
            }
            break;
        default:
            span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected special put byte value - %d!\n", byte);
            break;
        }
        return;
    }
    if ((octet = v18_decode_baudot(s, (uint8_t) (byte & 0x1F))))
        s->rx_msg[s->rx_msg_len++] = octet;
    if (s->rx_msg_len >= 256)
    {
        s->rx_msg[s->rx_msg_len] = '\0';
        s->put_msg(s->user_data, s->rx_msg, s->rx_msg_len);
        s->rx_msg_len = 0;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) v18_tx(v18_state_t *s, int16_t *amp, int max_len)
{
    int len;
    int lenx;

    len = tone_gen(&(s->alert_tone_gen), amp, max_len);
    if (s->tx_signal_on)
    {
        switch (s->mode)
        {
        case V18_MODE_DTMF:
            if (len < max_len)
                len += dtmf_tx(&(s->dtmftx), amp, max_len - len);
            break;
        default:
            if (len < max_len)
            {
                if ((lenx = fsk_tx(&(s->fsktx), amp + len, max_len - len)) <= 0)
                    s->tx_signal_on = FALSE;
                len += lenx;
            }
            break;
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) v18_rx(v18_state_t *s, const int16_t amp[], int len)
{
    switch (s->mode)
    {
    case V18_MODE_DTMF:
        /* Apply a message timeout. */
        s->in_progress -= len;
        if (s->in_progress <= 0)
            s->rx_msg_len = 0;
        dtmf_rx(&(s->dtmfrx), amp, len);
        break;
    default:
        fsk_rx(&(s->fskrx), amp, len);
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_put(v18_state_t *s, const char msg[], int len)
{
    char buf[256 + 1];
    int x;
    int n;
    int i;

    /* This returns the number of characters that would not fit in the buffer.
       The buffer will only be loaded if the whole string of digits will fit,
       in which case zero is returned. */
    if (len < 0)
    {
        if ((len = strlen(msg)) == 0)
            return 0;
    }
    switch (s->mode)
    {
    case V18_MODE_5BIT_45:
    case V18_MODE_5BIT_50:
        for (i = 0;  i < len;  i++)
        {
            n = 0;
            if ((x = v18_encode_baudot(s, msg[i])))
            {
                if ((x & 0x3E0))
                    buf[n++] = (uint8_t) ((x >> 5) & 0x1F);
                buf[n++] = (uint8_t) (x & 0x1F);
                /* TODO: Deal with out of space condition */
                if (queue_write(&s->queue.queue, (const uint8_t *) buf, n) < 0)
                    return i;
                s->tx_signal_on = TRUE;
            }
        }
        return len;
    case V18_MODE_DTMF:
        break;
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v18_get_logging_state(v18_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v18_state_t *) v18_init(v18_state_t *s,
                                     int calling_party,
                                     int mode,
                                     put_msg_func_t put_msg,
                                     void *user_data)
{
    if (s == NULL)
    {
        if ((s = (v18_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->calling_party = calling_party;
    s->mode = mode;
    s->put_msg = put_msg;
    s->user_data = user_data;

    switch (s->mode)
    {
    case V18_MODE_5BIT_45:
        fsk_tx_init(&(s->fsktx), &preset_fsk_specs[FSK_WEITBRECHT], async_tx_get_bit, &(s->asynctx));
        async_tx_init(&(s->asynctx), 5, ASYNC_PARITY_NONE, 2, FALSE, v18_tdd_get_async_byte, s);
        /* Schedule an explicit shift at the start of baudot transmission */
        s->baudot_tx_shift = 2;
        /* TDD uses 5 bit data, no parity and 1.5 stop bits. We scan for the first stop bit, and
           ride over the fraction. */
        fsk_rx_init(&(s->fskrx), &preset_fsk_specs[FSK_WEITBRECHT], FSK_FRAME_MODE_5N1_FRAMES, v18_tdd_put_async_byte, s);
        s->baudot_rx_shift = 0;
        break;
    case V18_MODE_5BIT_50:
        fsk_tx_init(&(s->fsktx), &preset_fsk_specs[FSK_WEITBRECHT50], async_tx_get_bit, &(s->asynctx));
        async_tx_init(&(s->asynctx), 5, ASYNC_PARITY_NONE, 2, FALSE, v18_tdd_get_async_byte, s);
        /* Schedule an explicit shift at the start of baudot transmission */
        s->baudot_tx_shift = 2;
        /* TDD uses 5 bit data, no parity and 1.5 stop bits. We scan for the first stop bit, and
           ride over the fraction. */
        fsk_rx_init(&(s->fskrx), &preset_fsk_specs[FSK_WEITBRECHT50], FSK_FRAME_MODE_5N1_FRAMES, v18_tdd_put_async_byte, s);
        s->baudot_rx_shift = 0;
        break;
    case V18_MODE_DTMF:
        dtmf_tx_init(&(s->dtmftx));
        dtmf_rx_init(&(s->dtmfrx), v18_rx_dtmf, s);
        break;
    case V18_MODE_EDT:
        break;
    case V18_MODE_BELL103:
        break;
    case V18_MODE_V23VIDEOTEX:
        break;
    case V18_MODE_V21TEXTPHONE:
        break;
    case V18_MODE_V18TEXTPHONE:
        break;
    }
    queue_init(&s->queue.queue, 128, QUEUE_READ_ATOMIC | QUEUE_WRITE_ATOMIC);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_release(v18_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_free(v18_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v18_mode_to_str(int mode)
{
    switch (mode)
    {
    case V18_MODE_NONE:
        return "None";
    case V18_MODE_5BIT_45:
        return "Weitbrecht TDD (45.45bps)";
    case V18_MODE_5BIT_50:
        return "Weitbrecht TDD (50bps)";
    case V18_MODE_DTMF:
        return "DTMF";
    case V18_MODE_EDT:
        return "EDT";
    case V18_MODE_BELL103:
        return "Bell 103";
    case V18_MODE_V23VIDEOTEX:
        return "Videotex";
    case V18_MODE_V21TEXTPHONE:
        return "V.21";
    case V18_MODE_V18TEXTPHONE:
        return "V.18 text telephone";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
