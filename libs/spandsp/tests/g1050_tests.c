/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g1050_tests.c - Tests for the G.1050/TIA-921 model.
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)  &&  defined(HAVE_FL_FL_AUDIO_METER_H)
#define ENABLE_GUI
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sndfile.h>
#if defined(HAVE_MATH_H)
#define GEN_CONST
#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "media_monitor.h"
#endif

#define PACKET_SIZE                 256
#define PACKET_INTERVAL             20
#define SIMULATION_TIME             300

#define MODEL_NO                    8
#define SPEED_PATTERN_NO            133

int main(int argc, char *argv[])
{
    g1050_state_t *s;
    double *packet_arrival_times;
    int packets_per_sec;
    int num_packets;
    int model_no;
    int speed_pattern_no;
    int simulation_time;
    int i;
    int len;
    uint8_t put_pkt[256];
    uint8_t get_pkt[256];
    int put_pkt_len;
    int get_pkt_len;
    int get_seq_no;
    double get_departure_time;
    double get_arrival_time;
    int packets_put;
    int packets_really_put;
    int packets_got;
    int oos_packets_got;
    int missing_packets_got;
    int highest_seq_no_got;
    int opt;
    FILE *out_file;
#if defined(ENABLE_GUI)
    int use_gui;
#endif

#if defined(ENABLE_GUI)
    use_gui = false;
#endif
    model_no = MODEL_NO;
    speed_pattern_no = SPEED_PATTERN_NO;
    simulation_time = SIMULATION_TIME;
    while ((opt = getopt(argc, argv, "gm:s:t:")) != -1)
    {
        switch (opt)
        {
        case 'g':
#if defined(ENABLE_GUI)
            use_gui = true;
#else
            fprintf(stderr, "Graphical monitoring not available\n");
            exit(2);
#endif
            break;
        case 'm':
            model_no = optarg[0] - 'A' + 1;
            if (model_no < 0  ||  model_no > 8)
            {
                fprintf(stderr, "Bad model ID '%s'\n", optarg);
                exit(2);
            }
            break;
        case 's':
            speed_pattern_no = atoi(optarg);
            if (speed_pattern_no < 1  ||  speed_pattern_no > 133)
            {
                fprintf(stderr, "Bad link speed pattern %s\n", optarg);
                exit(2);
            }
            break;
        case 't':
            simulation_time = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    argc -= optind;
    argv += optind;

    if ((out_file = fopen("g1050_tests.txt", "w")) == NULL)
    {
        fprintf(stderr, "Can't open %s\n", "g1050_tests.txt");
        return 2;
    }
    packets_per_sec = 1000/PACKET_INTERVAL;
    num_packets = packets_per_sec*simulation_time;

    if ((packet_arrival_times = calloc(num_packets, sizeof(double))) == NULL)
    {
        fprintf(stderr, "Can't allocate the data buffers\n");
        return 2;
    }
    for (i = 0;  i < num_packets;  i++)
        packet_arrival_times[i] = 0.0;

    /* If we don't initialise this random number generator it gives endless zeros on some systems. */
    /* Use a fixed seed to produce identical results in successive runs of the simulation, for debug purposes. */
    srand48(0x1234567);

    if ((s = g1050_init(model_no, speed_pattern_no, PACKET_SIZE, packets_per_sec)) == NULL)
    {
        fprintf(stderr, "Failed to start the G.1050 model\n");
        exit(2);
    }
    g1050_dump_parms(model_no, speed_pattern_no);

#if defined(ENABLE_GUI)
    if (use_gui)
        start_media_monitor();
#endif

    for (i = 0;  i < 256;  i++)
        put_pkt[i] = i;
    put_pkt_len = 256;
    get_pkt_len = -1;
    get_seq_no = -1;
    get_arrival_time = -1;
    packets_put = 0;
    packets_really_put = 0;
    packets_got = 0;
    oos_packets_got = 0;
    missing_packets_got = 0;
    highest_seq_no_got = -1;
    for (i = 0;  i < num_packets;  i++)
    {
        if ((len = g1050_put(s, put_pkt, put_pkt_len, i, (double) i*0.001*PACKET_INTERVAL)) > 0)
            packets_really_put++;
        packets_put++;
        if (i == 5)
            g1050_queue_dump(s);
        if (i >= 5)
        {
            do
            {
                get_pkt_len = g1050_get(s, get_pkt, 256, (double) i*0.001*PACKET_INTERVAL, &get_seq_no, &get_departure_time, &get_arrival_time);
                if (get_pkt_len >= 0)
                {
#if defined(ENABLE_GUI)
                    if (use_gui)
                        media_monitor_rx(get_seq_no, get_departure_time, get_arrival_time);
#endif
                    packets_got++;
                    if (get_seq_no < highest_seq_no_got)
                        oos_packets_got++;
                    else if (get_seq_no > highest_seq_no_got + 1)
                        missing_packets_got += (get_seq_no - highest_seq_no_got - 1);
                    if (get_seq_no > highest_seq_no_got)
                        highest_seq_no_got = get_seq_no;
                    fprintf(out_file, "%d, %.3f, %.8f\n", get_seq_no, get_seq_no*0.001*PACKET_INTERVAL, get_arrival_time);
                }
            }
            while (get_pkt_len >= 0);
        }
#if defined(ENABLE_GUI)
        if (use_gui)
            media_monitor_update_display();
#endif
    }
    /* Clear out anything remaining in the queue, by jumping forwards in time */
    do
    {
        get_pkt_len = g1050_get(s, get_pkt, 256, (double) i*0.001*PACKET_INTERVAL + 5.0, &get_seq_no, &get_departure_time, &get_arrival_time);
        if (get_pkt_len >= 0)
        {
            packets_got++;
            fprintf(out_file, "%d, %.3f, %.8f\n", get_seq_no, get_seq_no*0.001*PACKET_INTERVAL, get_arrival_time);
        }
    }
    while (get_pkt_len >= 0);

    fclose(out_file);

    printf("Put %d packets. Really put %d packets. Got %d packets.\n", packets_put, packets_really_put, packets_got);
    printf("%d OOS packets, %d missing packets\n", oos_packets_got, missing_packets_got - oos_packets_got);
    if (packets_really_put != packets_got)
    {
        printf("%d packets queued, but only %d received\n", packets_really_put, packets_got);
        exit(2);
    }
    printf("%.3f%% of packets lost\n", 100.0*(packets_put - packets_really_put)/packets_put);
    g1050_free(s);
    free(packet_arrival_times);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
