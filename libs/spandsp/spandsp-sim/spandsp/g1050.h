/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g1050.h - IP network modeling, as per G.1050/TIA-921.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
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

/*! \page g1050_ip_network_model_page G.1050/TIA-921 IP network path model
\section g1050_ip_network_model_page_sec_1 What does it do?
The ITU G.1050 specification defines a model of an IP network, appropriate
for the testing of how streaming media woud behave across the internet. The
model is based on a path having 5 segments:
 - a local LAN (wired or wireless)
 - an access link to the internet
 - an internet of arbitrary complexity
 - an access link from the internet
 - a distant LAN (wired or wireless)
The impairments typical of these segments at various service levels are modelled.
8 standard service level behaviours are defined, covering lightly loaded to heavily
congested levels. 168 standard sets of link speeds are defined, covering typical
wired and wireless LAN, broadband access link, and backbone characteristics.

The G.1050 model is suitable for testing the behaviour of RTP, UDPTL and other streaming
protocols for packet loss and jitter behaviour.
*/

#if !defined(_G1050_H_)
#define _G1050_H_

/* This is the time slice at which delays, packet loss, etc. are calculated. */
#define G1050_TICKS_PER_SEC         1000

/* Search back 200 ms to preserve order of legitimately out of sequence packets. */
#define SEARCHBACK_PERIOD           200

#define G1050_LOW_LOSS      0
#define G1050_HIGH_LOSS     1

#define G1050_LAN_LINK      1
#define G1050_ACCESS_LINK   2

/*! Segment constants, as defined in G.1050. */
typedef struct
{
    /*! Probability of changing from low to high and high to low loss states */
    double prob_loss_rate_change[2];
    /*! Probability of an impulse in the low and high loss states */
    double prob_impulse[2][2];

    /*! Impulse height, based on MTU and bit rate */
    double impulse_height;
    /*! Impulse decay coefficient for the single pole IIR filter. */
    double impulse_coeff;

    /*! Probability of packet loss due to occupancy. */
    double prob_packet_loss;
    /*! Probability of packet loss due to a multiple access collision. */
    double prob_packet_collision_loss;
} g1050_segment_constants_t;

/*! End-to-end constants, as defined in G.1050. */
typedef struct
{
    g1050_segment_constants_t segment[4];
} g1050_constants_t;

/*! The model definition for a LAN or access link segment */
typedef struct
{
    /*! Percentage occupancy of the media */
    double percentage_occupancy;
    /*! MTU of the media */
    int mtu;
    /*! Maximum jitter in the segment. */
    double max_jitter;
} g1050_segment_model_t;

/*! The model definition for the core network (backbone) segment */
typedef struct
{
    /*! Basic delay of the backbone for regional paths */
    double base_regional_delay;
    /*! Basic delay of the backbone for intercontinental paths */
    double base_intercontinental_delay;
    /*! Percentage packet loss of the backbone */
    /*! Percentage packet loss of the backbone. */
    double percentage_packet_loss;
    /*! Maximum jitter in the backbone. */
    double max_jitter;
    /*! Interval between the backbone route flapping between two paths, in seconds. */
    double route_flap_interval;
    /*! The difference in backbone delay between the two routes we flap between, in seconds. */
    double route_flap_delay;
    /*! The interval between link failures. */
    double link_failure_interval;
    /*! The duration of link failures. */
    double link_failure_duration;
    /*! Probability of packet loss in the backbone. */
    double prob_packet_loss;
    /*! Probability of a packet going out of sequence in the backbone. */
    double prob_oos;
} g1050_core_model_t;

/*! The model definition for a complete end-to-end path */
typedef struct
{
    /*! The likelyhood of occurance probabilities for the A, B and C scenarios defined in G.1050 */
    int loo[3];
    g1050_segment_model_t sidea_lan;
    g1050_segment_model_t sidea_access_link;
    g1050_core_model_t core;
    g1050_segment_model_t sideb_access_link;
    g1050_segment_model_t sideb_lan;
} g1050_model_t;

/*! The speed model for a complete end-to-end path */
typedef struct
{
    int sidea_lan_bit_rate;
    int sidea_lan_multiple_access;
    int sidea_access_link_bit_rate_ab;
    int sidea_access_link_bit_rate_ba;
    int sidea_access_link_qos_enabled;
    int sideb_lan_bit_rate;
    int sideb_lan_multiple_access;
    int sideb_access_link_bit_rate_ab;
    int sideb_access_link_bit_rate_ba;
    int sideb_access_link_qos_enabled;
    double loo;
} g1050_channel_speeds_t;

/*! The model state for a LAN or access link segment */
typedef struct
{
    /*! The type of link, G1050_LAN_LINK or G_1050_ACCESS_LINK */
    int link_type;
    /*! 1 if in the high loss state, or 0 if in the low loss state. */
    int high_loss;

    /*! The probability of a loss rate change, for both loss rate states. */
    double prob_loss_rate_change[2];
    /*! The probability of a impulse occuring, for both loss rate states. */
    double prob_impulse[2];

    /*! The maximum permitted height of impulses. */
    double impulse_height;
    /*! The impulse decay coefficient. */
    double impulse_coeff;

    /*! The basic serial delay due to the link. */
    double serial_delay;
    /*! Peak jitter in the segment. */
    double max_jitter;
    /*! The probability of packet loss. */
    double prob_packet_loss;
    /*! The probability of packet loss due to collision. */
    double prob_packet_collision_loss;
    /*! The maximum addition delay due to congestion. */
    double congestion_delay;

    /*! TRUE if QoS is enabled on the link. */
    int qos_enabled;
    /*! TRUE if the link is a multiple access type (e.g. an ethernet hub). */
    int multiple_access;

    /*! The latest packet arrival time seen on the link. */
    double last_arrival_time;

    /*! 3 seconds of predicted delays for the link */
    double delays[3*G1050_TICKS_PER_SEC];

    /*! A count of packets lost on the link. */
    uint32_t lost_packets;
    /*! An extra debug count of packets lost on the link. */
    uint32_t lost_packets_2;
} g1050_segment_state_t;

/*! The model state for the core network (backbone) segment */
typedef struct
{
    /* Router model. */
    int32_t route_flap_counter;
    int32_t route_flap_interval;
    double route_flap_delta;

    /* Link failure model. */
    int32_t link_failure_counter;
    int32_t link_recovery_counter;

    int32_t link_failure_interval_ticks;
    int32_t link_failure_duration_ticks;

    /*! Basic backbone delay */
    double base_delay;
    /*! Peak jitter in the backbone delay */
    double max_jitter;
    /*! Probability of packet loss in the backbone, in percent */
    double prob_packet_loss;
    /*! Probability of a packet going out of sequence in the backbone. */
    double prob_oos;

    /*! The latest packet arrival time seen on the link. */
    double last_arrival_time;
    double delay_delta;

    /*! 3 seconds of predicted delays for the link */
    double delays[3*G1050_TICKS_PER_SEC];

    /*! A count of packets lost on the link. */
    uint32_t lost_packets;
    /*! An extra debug count of packets lost on the link. */
    uint32_t lost_packets_2;
} g1050_core_state_t;

/*! The definition of an element in the packet queue */
typedef struct g1050_queue_element_s
{
    struct g1050_queue_element_s *next;
    struct g1050_queue_element_s *prev;
    int seq_no;
    double departure_time;
    double arrival_time;
    int len;
    uint8_t pkt[];
} g1050_queue_element_t;

/*! The model definition for a complete end-to-end path */
typedef struct
{
    int packet_rate;
    int packet_size;
    float base_time;
    g1050_segment_state_t segment[4];
    g1050_core_state_t core;
    double arrival_times_1[3*G1050_TICKS_PER_SEC];
    double arrival_times_2[3*G1050_TICKS_PER_SEC];
    g1050_queue_element_t *first;
    g1050_queue_element_t *last;
} g1050_state_t;

extern g1050_constants_t g1050_constants[1];
extern g1050_channel_speeds_t g1050_speed_patterns[168];
extern g1050_model_t g1050_standard_models[9];

#ifdef  __cplusplus
extern "C"
{
#endif

SPAN_DECLARE(g1050_state_t *) g1050_init(int model,
                                         int speed_pattern,
                                         int packet_size,
                                         int packet_rate);

SPAN_DECLARE(int) g1050_free(g1050_state_t *s);

SPAN_DECLARE(void) g1050_dump_parms(int model, int speed_pattern);

SPAN_DECLARE(int) g1050_put(g1050_state_t *s,
                            const uint8_t buf[],
                            int len,
                            int seq_no,
                            double departure_time);

SPAN_DECLARE(int) g1050_get(g1050_state_t *s,
                            uint8_t buf[],
                            int max_len,
                            double current_time,
                            int *seq_no,
                            double *departure_time,
                            double *arrival_time);

SPAN_DECLARE(void) g1050_queue_dump(g1050_state_t *s);

#ifdef  __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
