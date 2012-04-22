/*
 * SpanDSP - a series of DSP components for telephony
 *
 * testadsi.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>

#if defined(HAVE_LIBUNICALL)

#include <linux/zaptel.h>
#include <pthread.h>
#include <sndfile.h>
#include <tiffio.h>

#include "unicall.h"
//#include "../libmfcr2/libmfcr2.h"
//#include "../libpri/libpri.h"

#include "spandsp.h"

#define FALSE 0
#define TRUE (!FALSE)

int caller_mode = FALSE;
static SNDFILE *rxhandle;
static SNDFILE *txhandle;

#if 0
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;

    len = adsi_add_field(s, msg, -1, CLASS_SDMF_CALLERID, 0);
    len = adsi_add_field(s, msg, len, 0, "10011750", 8);
    len = adsi_add_field(s, msg, len, 0, "6095551212", 10);
    return len;
}
#endif
#if 0
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;

    len = adsi_add_field(s, msg, -1, CLIP_MDMF_CALLERID, 0);
    len = adsi_add_field(s, msg, len, CLIP_CALLTYPE, "\x81", 1);
    len = adsi_add_field(s, msg, len, CLIP_DATETIME, "10011750", 8);
    len = adsi_add_field(s, msg, len, CLIP_CALLER_NUMBER, "12345678", 8);
    len = adsi_add_field(s, msg, len, CLIP_DIALED_NUMBER, "87654321", 8);
    len = adsi_add_field(s, msg, len, CLIP_CALLER_NAME, "Steve Underwood", 15);
    return len;
}
#endif
#if 0
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;

    len = adsi_add_field(s, msg, -1, CLIP_MDMF_CALLERID, 0);
    len = adsi_add_field(s, msg, len, CLIP_NUM_MSG, "\x03", 1);
    return len;
}
#endif
#if 0
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;

    len = adsi_add_field(s, msg, -1, CLIP_MDMF_MSG_WAITING, NULL, 0);
    len = adsi_add_field(s, msg, len, CLIP_VISUAL_INDICATOR, "\x00", 1);
    return len;
}
#endif
#if 0
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;

    len = adsi_add_field(s, msg, -1, CLIP_MDMF_MSG_WAITING, NULL, 0);
    len = adsi_add_field(s, msg, len, CLIP_VISUAL_INDICATOR, "\xFF", 1);
    len = adsi_add_field(s, msg, len, CLIP_NUM_MSG, "\x05", 1);
    return len;
}
#endif
#if 1
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;

    len = adsi_add_field(s, msg, -1, CLASS_SDMF_MSG_WAITING, NULL, 0);
    len = adsi_add_field(s, msg, len, 0, "\x6F", 1);
    len = adsi_add_field(s, msg, len, 0, "\x6F", 1);
    len = adsi_add_field(s, msg, len, 0, "\x6F", 1);
    return len;
}
#endif
#if 0
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;

    len = adsi_add_field(s, msg, -1, CLASS_SDMF_MSG_WAITING, NULL, 0);
    len = adsi_add_field(s, msg, len, 0, "\x42", 1);
    len = adsi_add_field(s, msg, len, 0, "\x42", 1);
    len = adsi_add_field(s, msg, len, 0, "\x42", 1);
    return len;
}
#endif
#if 0
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;

    len = adsi_add_field(s, msg, -1, CLIP_MDMF_SMS, NULL, 0);
    len = adsi_add_field(s, msg, len, CLIP_DISPLAY_INFO, "\x00ABC", 4);
    return len;
}
#endif

struct
{
    pthread_t thread;
    int chan;
    int sig_fd;
    int fd;
    uc_call_t *call;
    uc_crn_t crn;
    int xxx;
    int cause;

    dtmf_rx_state_t dtmf_state;
    char dtmf[101];
    int dtmf_ptr;
    
    char *tag;
    
    char originating_number[32];
    char destination_number[32];
    
    adsi_rx_state_t adsi_rx;
    adsi_tx_state_t adsi_tx;
} chan_stuff[30];

tone_gen_descriptor_t tone_desc;
tone_gen_state_t gen;

void channel_read_adsi_channel(uc_t *uc, int chan, void *user_data, uint8_t *buf, int len);
int channel_write_adsi_channel(uc_t *uc, int chan, void *user_data, uint8_t *buf, int max_len);
int channel_error(uc_t *uc, int chan, void *user_data, int cause);
int signaling_error(uc_t *uc, void *user_data, int cause);

void channel_read_adsi_channel(uc_t *uc, int chan, void *user_data, uint8_t *buf, int len)
{
    int i;
    int xlen;
    int16_t pcm_buf[1024];
    char *s;
    int outframes;
    
    for (i = 0;  i < len;  i++)
        pcm_buf[i] = alaw_to_linear(buf[i]);
    /*endfor*/
    outframes = sf_writef_short(rxhandle,
                              AF_DEFAULT_TRACK,
                              pcm_buf,
                              len);
    if (outframes != len)
    {
        printf("Failed to write %d samples\n", len);
        exit(2);
    }
    
    dtmf_rx(&chan_stuff[chan].dtmf_state, pcm_buf, len);
    xlen = dtmf_rx_get(&chan_stuff[chan].dtmf_state,
                       chan_stuff[chan].dtmf + chan_stuff[chan].dtmf_ptr,
                       100 - chan_stuff[chan].dtmf_ptr);
    if (xlen > 0)
    {
        s = chan_stuff[chan].dtmf + chan_stuff[chan].dtmf_ptr;
        while (*s)
        {
            if (*s == '#')
            {
                uc_set_channel_read_callback(uc, 0, NULL, 0);
                uc_set_channel_write_callback(uc, 0, NULL, 0);
                if (uc_call_control(uc, UC_OP_DROPCALL, chan_stuff[chan].crn, (void *) UC_CAUSE_NORMAL_CLEARING))
                    printf ("A Drop Call failed\n");
                /*endif*/
                break;
            }
            /*endif*/
            s++;
        }
        /*endwhile*/
        printf("Got '%s'\n", chan_stuff[chan].dtmf);
        chan_stuff[chan].dtmf_ptr += xlen;
    }
    /*endif*/
    adsi_rx(&(chan_stuff[chan].adsi_rx), pcm_buf, len);
}
/*- End of function --------------------------------------------------------*/

int channel_write_adsi_channel(uc_t *uc, int chan, void *user_data, uint8_t *buf, int max_len)
{
    int16_t pcm_buf[1024];
    int len;
    int i;
    static int block = 0;
    uint8_t adsi_msg[256];
    int adsi_msg_len;

    if (++block == 100)
    {
        adsi_send_alert_tone(&(chan_stuff[chan].adsi_tx));
        printf("Alert tone sent\n");
        adsi_msg_len = adsi_create_message(&(chan_stuff[chan].adsi_tx), adsi_msg);
        if ((len = adsi_put_message(&(chan_stuff[chan].adsi_tx), adsi_msg, adsi_msg_len)) > 0)
            printf("Message put - %d bytes\n", len);
    }
    len = adsi_tx(&(chan_stuff[chan].adsi_tx), pcm_buf, max_len);
    sf_writef_short(txhandle,
                  AF_DEFAULT_TRACK,
                  pcm_buf,
                  len);
    for (i = 0;  i < len;  i++)
        buf[i] = linear_to_alaw(pcm_buf[i]);
    /*endfor*/
    return len;
}
/*- End of function --------------------------------------------------------*/

int channel_error(uc_t *uc, int chan, void *user_data, int cause)
{
    printf("Error %d\n", cause);
    return  0;
}
/*- End of function --------------------------------------------------------*/

int signaling_error(uc_t *uc, void *user_data, int cause)
{
    printf("Error %d\n", cause);
    return  0;
}
/*- End of function --------------------------------------------------------*/

static void initiate_call(uc_t *uc, int chan, uc_event_t *e)
{
    uc_makecall_t makecall;
    uc_callparms_t *callparms;
    int ret;

    printf ("Initiating call\n");
    if ((callparms = uc_new_callparms(NULL)) == NULL)
        return;
    /*endif*/
    uc_callparm_originating_number(callparms, chan_stuff[chan].originating_number);
    uc_callparm_destination_number(callparms, chan_stuff[chan].destination_number);
    makecall.callparms = callparms;
    makecall.crn = 0;
    if (ret = uc_call_control(uc, UC_OP_MAKECALL, 0, (void *) &makecall) != UC_RET_OK)
        fprintf(stderr, "Make Call failed - %d\n", ret);
    /*endif*/
    chan_stuff[chan].crn = makecall.crn;
    free(callparms);
}
/*- End of function --------------------------------------------------------*/

static void handle_uc_event(uc_t *uc, void *user_data, uc_event_t *e)
{
    int chan;
    
    chan = (int) user_data;
    printf ("-- %s (%d)\n", uc_event2str(e->e), chan);
    switch (e->e)
    {
    case UC_EVENT_DEVICEFAIL:
        break;
    case UC_EVENT_PROTOCOLFAIL:
        printf("-- Protocol failure on channel %d, cause %d\n", e->gen.channel, e->gen.data);
        break;
    case UC_EVENT_SIGCHANSTATUS:
        printf("-- Signalling channel status - %s\n", e->sigchanstatus.ok  ?  "Up"  :  "Down");
        break;
    case UC_EVENT_ALARM:
        printf("-- Alarm - 0x%X 0x%X\n", e->alarm.raised, e->alarm.cleared);
        break;
    case UC_EVENT_FARBLOCKED:
        printf("-- Channel far end blocked! :-(\n");
        chan_stuff[chan].xxx &= ~1;
        break;
    case UC_EVENT_FARUNBLOCKED:
        printf("-- Channel far end unblocked! :-)\n");
        chan_stuff[chan].xxx |= 1;
        if (chan_stuff[chan].xxx == 3)
        {
            if (caller_mode)
                initiate_call(uc, chan, e);
            /*endif*/
        }
        /*endif*/
        break;
    case UC_EVENT_LOCALBLOCKED:
        printf("-- Channel local end blocked! :-(\n");
        chan_stuff[chan].xxx &= ~2;
        break;
    case UC_EVENT_LOCALUNBLOCKED:
        printf("-- Channel local end unblocked! :-)\n");
        chan_stuff[chan].xxx |= 2;
        if (chan_stuff[chan].xxx == 3)
        {
            if (caller_mode)
                initiate_call(uc, chan, e);
            /*endif*/
        }
        /*endif*/
        break;
    case UC_EVENT_DIALING:
        printf("-- Dialing on channel %d\n", e->gen.channel);
        break;
    case UC_EVENT_ACCEPTED:
        printf("-- Accepted on channel %d\n", e->gen.channel);
        if (uc_call_control(uc, UC_OP_ANSWERCALL, e->gen.crn, (void *) -1))
            fprintf(stderr, "Answer Call failed\n");
        /*endif*/
        break;
    case UC_EVENT_DETECTED:
        printf("-- Detected on channel %d\n", e->gen.channel);
        break;
    case UC_EVENT_ALERTING:
        printf("-- Alerting on channel %d\n", e->gen.channel);
        /* This is just a notification of call progress. We need take no action at this point. */
        break;
    case UC_EVENT_FARDISCONNECTED:
        printf("-- Far end disconnected on channel %d\n", e->fardisconnected.channel);
        /* Kill any outstanding audio processing */
        uc_set_channel_read_callback(uc, 0, NULL, 0);
        uc_set_channel_write_callback(uc, 0, NULL, 0);
        if (uc_call_control(uc, UC_OP_DROPCALL, e->fardisconnected.crn, (void *) UC_CAUSE_NORMAL_CLEARING))
            fprintf(stderr, "C Drop Call failed\n");
        /*endif*/
        break;
    case UC_EVENT_DROPCALL:
        printf("-- Drop call on channel %d\n", e->gen.channel);
        if (uc_call_control(uc, UC_OP_RELEASECALL, e->gen.crn, NULL))
            fprintf(stderr, "uc_ReleaseCall failed\n");
        /*endif*/
        break;
    case UC_EVENT_RELEASECALL:
        printf("-- Released on channel %d\n", e->gen.channel);
        if (caller_mode)
            initiate_call(uc, chan, e);
        /*endif*/
        break;
    case UC_EVENT_OFFERED:
        printf("-- Offered on channel %d, CRN %d (ANI: %s, DNIS: %s)\n", e->offered.channel, e->offered.crn, e->offered.parms.originating_number, e->offered.parms.destination_number);
        if (!caller_mode)
        {
            switch (chan_stuff[chan].cause)
            {
            case 0:
                if (uc_call_control(uc, UC_OP_ACCEPTCALL, e->offered.crn, (void *) -1))
                    fprintf(stderr, "uc_AcceptCall failed\n");
                /*endif*/
                break;
            case 1:
                if (uc_call_control(uc, UC_OP_ANSWERCALL, e->offered.crn, (void *) -1))
                    fprintf(stderr, "uc_AnswerCall failed\n");
                /*endif*/
                break;
            case 2:
                if (uc_call_control(uc, UC_OP_DROPCALL, e->offered.crn, (void *) UC_CAUSE_USER_BUSY))
                    fprintf(stderr, "E Drop Call failed\n");
                /*endif*/
                break;
            case 3:
                if (uc_call_control(uc, UC_OP_DROPCALL, e->offered.crn, (void *) UC_CAUSE_UNASSIGNED_NUMBER))
                    fprintf(stderr, "F Drop Call failed\n");
                /*endif*/
                break;
            case 4:
                if (uc_call_control(uc, UC_OP_DROPCALL, e->offered.crn, (void *) UC_CAUSE_NETWORK_CONGESTION))
                    fprintf(stderr, "G Drop Call failed\n");
                /*endif*/
                break;
            case 5:
                if (uc_call_control(uc, UC_OP_DROPCALL, e->offered.crn, (void *) UC_CAUSE_DEST_OUT_OF_ORDER))
                    fprintf(stderr, "H Drop Call failed\n");
                /*endif*/
                break;
            }
            /*endswitch*/
            if (++chan_stuff[chan].cause > 5)
                chan_stuff[chan].cause = 0;
            /*endif*/
        }
        /*endif*/
        break;
    case UC_EVENT_ANSWERED:
        printf("-- Answered on channel %d\n", e->gen.channel);
        uc_set_channel_read_callback(uc, 0, channel_read_adsi_channel, (void *) chan);
printf("XXX read callback set\n");
        uc_set_channel_write_callback(uc, 0, channel_write_adsi_channel, (void *) chan);
printf("XXX write callback set\n");
        adsi_tx_init(&(chan_stuff[chan].adsi_tx), ADSI_STANDARD_CLASS);
printf("XXX ADSI inited\n");
        dtmf_rx_init(&chan_stuff[chan].dtmf_state, NULL, NULL);
printf("XXX DTMF inited\n");
        break;
    case UC_EVENT_CONNECTED:
        printf("-- Connected on channel %d\n", e->gen.channel);
        if (uc_call_control(uc, UC_OP_DROPCALL, e->offered.crn, (void *) UC_CAUSE_NORMAL_CLEARING))
            printf ("I Drop Call failed\n");
        /*endif*/
        break;
    default:
        fprintf(stderr, "--!! Unknown signaling event %d\n", e->e);
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void *run_uc(void *arg)
{
    uc_t *uc;
    uc_event_t *e;
    struct timeval tv = {0,0};
    struct timeval *next;
    fd_set rfds;
    fd_set wfds;
    fd_set efds;
    int res;
    int dfd;
    int chan;
    
    chan = *((int *) arg);

    dfd = chan_stuff[chan].fd;
    uc = uc_new(dfd, dfd, "mfcr2", "cn", UC_MODE_CPE, 1);
    //uc = uc_new(dfd, dfd, "pri", "ctr4", UC_MODE_CPE, 1);
    if (uc == NULL)
    {
        fprintf(stderr, "Unable to create instance\n");
        return NULL;
    }
    /*endif*/
    uc_set_signaling_callback(uc, handle_uc_event, (void *) chan);
    uc_set_signaling_error_callback(uc, signaling_error, (void *) chan);
    uc_set_channel_error_callback(uc, 0, channel_error, (void *) chan);
    uc_set_logging(uc, 0x7FFFFFFF, 0, chan_stuff[chan].tag);
    uc_call_control(uc, UC_OP_UNBLOCK, 0, (void *) -1);
    for (;;)
    {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        FD_SET(dfd, &rfds);
        FD_SET(dfd, &wfds);
        FD_SET(dfd, &efds);

        if ((next = uc_schedule_next(uc)))
        {
            gettimeofday(&tv, NULL);
            tv.tv_sec = next->tv_sec - tv.tv_sec;
            tv.tv_usec = next->tv_usec - tv.tv_usec;
            if (tv.tv_usec < 0)
            {
                tv.tv_usec += 1000000;
                tv.tv_sec -= 1;
            }
            /*endif*/
            if (tv.tv_sec < 0)
            {
                tv.tv_sec = 0;
                tv.tv_usec = 0;
            }
            /*endif*/
        }
        /*endif*/
        res = select(dfd + 1, &rfds, NULL, &efds, next  ?  &tv  :  NULL);
        e = NULL;
        if (res == 0)
        {
            uc_schedule_run(uc);
        }
        else if (res > 0)
        {
            e = uc_check_event(uc);
        }
        else if (errno != EINTR)
        {
            fprintf(stderr, "Error (%d) on select: %s\n", errno, strerror(errno));
        }
        /*endif*/

        if (e)
        {
            printf("Non-callback signaling event\n");
            handle_uc_event(uc, (void *) chan, e);
        }
        /*endif*/
    }
    /*endfor*/
    return NULL;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[]) 
{
    pthread_attr_t attr;
    struct zt_bufferinfo b;
    struct zt_gains g;
    int chan;
    int chanx;
    char dev_name[20];
    AFfilesetup filesetup;
    int j;

    filesetup = afNewFileSetup();
    if (filesetup == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);
    rxhandle = afOpenFile("rxadsi.wav", "w", filesetup);
    if (rxhandle == NULL)
    {
        fprintf(stderr, "    Failed to open adsi audio file\n");
        exit(2);
    }
    txhandle = afOpenFile("txadsi.wav", "w", filesetup);
    if (txhandle == NULL)
    {
        fprintf(stderr, "    Failed to open adsi audio file\n");
        exit(2);
    }

    uc_start();    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (argc < 1)
    {
        fprintf(stderr, "Usage: testcall [call]\n");
        exit(1);
    }
    /*endif*/
    for (chan = 0;  chan < 1/*30*/;  chan++)
    {
        chan_stuff[chan].sig_fd = open("/dev/zap/channel", O_RDWR | O_NONBLOCK);
        if (chan_stuff[chan].sig_fd < 0)
        {
            fprintf(stderr, "Failed to open channel: %s\n", strerror(errno));
            exit(1);
        }
        /*endif*/
        chan_stuff[chan].fd = chan_stuff[chan].sig_fd;
        
        /* Allow for the missing channel at TS16 */
        if (chan < 15)
            chanx = chan + 1;
        else
            chanx = chan + 2;
        /*endif*/
        if (ioctl(chan_stuff[chan].fd, ZT_SPECIFY, &chanx))
        {
            fprintf(stderr, "Failed to specify channel %d: %s\n", chanx, strerror(errno));
            exit(1);
        }
        /*endif*/
        if (ioctl(chan_stuff[chan].fd, ZT_GET_BUFINFO, &b))
        {
            fprintf(stderr, "Unable to get buffer info on channel %d: %s\n", chanx, strerror(errno));
            exit(1);
        }
        /*endif*/
        printf ("%d %d %d %d %d %d\n",
                b.rxbufpolicy,
                b.txbufpolicy,
                b.numbufs,
                b.bufsize,
                b.readbufs,
                b.writebufs);
        b.rxbufpolicy = ZT_POLICY_IMMEDIATE;
        b.txbufpolicy = ZT_POLICY_IMMEDIATE;
        b.numbufs = 4;
        b.bufsize = 160;
        if (ioctl(chan_stuff[chan].fd, ZT_SET_BUFINFO, &b))
        {
            fprintf(stderr, "Unable to set buffer info on channel %d: %s\n", chanx, strerror(errno));
            exit(1);
        }
        /*endif*/
        if (ioctl(chan_stuff[chan].fd, ZT_GET_BUFINFO, &b))
        {
            fprintf(stderr, "Unable to get buffer info on channel %d: %s\n", chanx, strerror(errno));
            exit(1);
        }
        /*endif*/
        for (j = 0;  j < 256;  j++)
        {
            g.rxgain[j] = j;
            g.txgain[j] = j;
        }
        ioctl(chan_stuff[chan].fd, ZT_SETGAINS, &g);
        printf("%d %d %d %d %d %d\n",
               b.rxbufpolicy,
               b.txbufpolicy,
               b.numbufs,
               b.bufsize,
               b.readbufs,
               b.writebufs);

        if (argc > 1)
            caller_mode = TRUE;
        /*endif*/
        chan_stuff[chan].chan = chan;
        sprintf(dev_name, "Chan %2d:", chanx);
        chan_stuff[chan].tag = strdup(dev_name);
        sprintf(chan_stuff[chan].originating_number, "%d", 987654321 + chan);
        sprintf(chan_stuff[chan].destination_number, "%d", 1234 + chan);
        
        printf("Thread for channel %d\n", chan);
        if (pthread_create(&chan_stuff[chan].thread, &attr, run_uc, &chan_stuff[chan].chan))
            exit(2);
        /*endif*/
    }
    /*endfor*/
    for (;;)
    {
        sleep(5);
        printf("Main thread\n");
    }
    /*endfor*/
    exit(0);

    return 0;
}
/*- End of function --------------------------------------------------------*/
#else
int main(int argc, char *argv[]) 
{
    printf("This program was not built with Unicall available\n"); 
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
