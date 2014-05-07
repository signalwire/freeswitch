/*
 * SpanDSP - a series of DSP components for telephony
 *
 * socket_harness.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
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


/*! \page bitstream_tests_page Bitstream tests
\section bitstream_tests_page_sec_1 What does it do?

\section bitstream_tests_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#include "pseudo_terminals.h"
#include "socket_harness.h"

//#define SIMULATE_RING 1

#define CLOSE_COUNT_MAX 100

/* static data */
static int16_t inbuf[4096];
static int16_t outbuf[4096];

static volatile sig_atomic_t keep_running = true;

static void log_signal(int signum)
{
    fprintf(stderr, "Signal %d: mark termination.\n", signum);
    keep_running = false;
    exit(2);
}
/*- End of function --------------------------------------------------------*/

int socket_harness_run(socket_harness_state_t *s)
{
    struct timeval tmo;
    fd_set rset;
    fd_set eset;
    struct termios termios;
    int max_fd;
    int count;
    int samples;
    int tx_samples;
    int ret;

    while (keep_running)
    {
        //if (s->modem->event)
        //    modem_event(s->modem);
#ifdef SIMULATE_RING
        tmo.tv_sec = 0;
        tmo.tv_usec= 1000000/RING_HZ;
#else
        tmo.tv_sec = 1;
        tmo.tv_usec= 0;
#endif
        max_fd = 0;
        FD_ZERO(&rset);
        FD_ZERO(&eset);
        FD_SET(s->audio_fd, &rset);
        FD_SET(s->audio_fd, &eset);
        FD_SET(s->pty_fd, &rset);
        FD_SET(s->pty_fd, &eset);
        if (s->audio_fd > max_fd)
            max_fd = s->audio_fd;
        if (s->pty_fd > max_fd)
            max_fd = s->pty_fd;
        if (s->pty_closed  &&  s->close_count)
        {
            if (!s->started  ||  s->close_count++ > CLOSE_COUNT_MAX)
                s->close_count = 0;
        }
        else if (s->terminal_free_space_callback(s->user_data))
        {
            FD_SET(s->pty_fd, &rset);
            if (s->pty_fd > max_fd)
                max_fd = s->pty_fd;
        }
        if ((ret = select(max_fd + 1, &rset, NULL, &eset, &tmo)) < 0)
        {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "Error: select: %s\n", strerror(errno));
            return ret;
        }

        if (ret == 0)
        {
            /* Timeout */
#ifdef SIMULATE_RING
            if (!modem->modem->started)
            {
                rcount++;
                if (rcount <= RING_ON)
                    modem_ring(modem->modem);
                else if (rcount > RING_OFF)
                    rcount = 0;
            }
#endif
            continue;
        }

        if (FD_ISSET(s->audio_fd, &rset))
        {
            if ((count = read(s->audio_fd, inbuf, sizeof(inbuf)/2)) < 0)
            {
                if (errno != EAGAIN)
                {
                    fprintf(stderr, "Error: audio read: %s\n", strerror(errno));
                    return -1;
                }
                count = 0;
            }
            if (count == 0)
            {
                fprintf(stderr, "Audio socket closed\n");
                return 0;
            }
            samples = count/2;
            usleep(125*samples);

            s->rx_callback(s->user_data, inbuf, samples);
            tx_samples = s->tx_callback(s->user_data, outbuf, samples);
            if (tx_samples < samples)
                memset(&outbuf[tx_samples], 0, (samples - tx_samples)*2);

            if ((count = write(s->audio_fd, outbuf, samples*2)) < 0)
            {
                if (errno != EAGAIN)
                {
                    fprintf(stderr, "Error: audio write: %s\n", strerror(errno));
                    return -1;
                }
                /* TODO: */
            }
            if (count != samples*2)
                fprintf(stderr, "audio write = %d\n", count);
        }

        if (FD_ISSET(s->pty_fd, &rset))
        {
            /* Check termios */
            tcgetattr(s->pty_fd, &termios);
            if (memcmp(&termios, &s->termios, sizeof(termios)))
                s->termios_callback(s->user_data, &termios);
            /* Read data */
            if ((count = s->terminal_free_space_callback(s->user_data)))
            {
                if (count > sizeof(inbuf))
                    count = sizeof(inbuf);
                if ((count = read(s->pty_fd, inbuf, count)) < 0)
                {
                    if (errno == EAGAIN)
                    {
                        fprintf(stderr, "pty read, errno = EAGAIN\n");
                    }
                    else
                    {
                        if (errno == EIO)
                        {
                            if (!s->pty_closed)
                            {
                                fprintf(stderr, "pty closed.\n");
                                s->pty_closed = 1;
                                if ((termios.c_cflag & HUPCL))
                                    s->hangup_callback(s->user_data, 0);
                            }
                            s->close_count = 1;
                        }
                        else
                        {
                            fprintf(stderr, "Error: pty read: %s\n", strerror(errno));
                            return -1;
                        }
                    }
                }
                else
                {
                    if (count == 0)
                        fprintf(stderr, "pty read = 0\n");
                    s->pty_closed = false;
                    s->terminal_callback(s->user_data, (uint8_t *) inbuf, count);
                }
            }
        }
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/

socket_harness_state_t *socket_harness_init(socket_harness_state_t *s,
                                            const char *socket_name,
                                            const char *tag,
                                            int caller,
                                            put_msg_func_t terminal_callback,
                                            termio_update_func_t termios_callback,
                                            modem_status_func_t hangup_callback,
                                            put_msg_free_space_func_t terminal_free_space_callback,
                                            span_rx_handler_t rx_callback,
                                            span_rx_fillin_handler_t rx_fillin_callback,
                                            span_tx_handler_t tx_callback,
                                            void *user_data)
{
    int sockfd;
    int listensockfd;
    struct sockaddr_un serv_addr;
    struct sockaddr_un cli_addr;
    socklen_t servlen;
    socklen_t clilen;

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Socket failed - errno = %d\n", errno);
        return NULL;
    }

    if (s == NULL)
    {
        if ((s = (socket_harness_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    signal(SIGINT, log_signal);
    signal(SIGTERM, log_signal);

    s->terminal_callback = terminal_callback;
    s->termios_callback = termios_callback;
    s->hangup_callback = hangup_callback;
    s->terminal_free_space_callback = terminal_free_space_callback;

    s->rx_callback = rx_callback;
    s->rx_fillin_callback = rx_fillin_callback;
    s->tx_callback = tx_callback;

    s->user_data = user_data;

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    /* This is a generic Unix domain socket. */
    strcpy(serv_addr.sun_path, socket_name);
    printf("Creating socket '%s'\n", serv_addr.sun_path);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family) + 1;
    if (caller)
    {
        fprintf(stderr, "Connecting to '%s'\n", serv_addr.sun_path);
        if (connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        {
            fprintf(stderr, "Connect failed - errno = %d\n", errno);
            exit(2);
        }
        fprintf(stderr, "Connected to '%s'\n", serv_addr.sun_path);
    }
    else
    {
        fprintf(stderr, "Listening to '%s'\n", serv_addr.sun_path);
        listensockfd = sockfd;
        /* The file may or may not exist. Just try to delete it anyway. */
        unlink(serv_addr.sun_path);
        if (bind(listensockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        {
            fprintf(stderr, "Bind failed - errno = %d\n", errno);
            exit(2);
        }
        listen(listensockfd, 5);
        clilen = sizeof(cli_addr);
        if ((sockfd = accept(listensockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0)
        {
            fprintf(stderr, "Accept failed - errno = %d", errno);
            exit(2);
        }
        fprintf(stderr, "Accepted on '%s'\n", serv_addr.sun_path);
    }
    if (pseudo_terminal_create(&s->modem))
    {
        fprintf(stderr, "Failed to create pseudo TTY\n");
        exit(2);
    }
    s->audio_fd = sockfd;
    s->pty_fd = s->modem.master;
    return s;
}
/*- End of function --------------------------------------------------------*/

int socket_harness_release(socket_harness_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

int socket_harness_free(socket_harness_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
