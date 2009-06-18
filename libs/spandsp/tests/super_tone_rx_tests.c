/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_detect_tests.c
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
 *
 * $Id: super_tone_rx_tests.c,v 1.33 2009/06/02 14:55:36 steveu Exp $
 */

/*! \file */

/*! \page super_tone_rx_tests_page Supervisory tone detection tests
\section super_tone_rx_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <sndfile.h>

#if defined(HAVE_LIBXML_XMLMEMORY_H)
#include <libxml/xmlmemory.h>
#endif
#if defined(HAVE_LIBXML_PARSER_H)
#include <libxml/parser.h>
#endif
#if defined(HAVE_LIBXML_XINCLUDE_H)
#include <libxml/xinclude.h>
#endif

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#define IN_FILE_NAME    "super_tone.wav"

#define MITEL_DIR       "../test-data/mitel/"
#define BELLCORE_DIR	"../test-data/bellcore/"

const char *bellcore_files[] =
{
    MITEL_DIR    "mitel-cm7291-talkoff.wav",
    BELLCORE_DIR "tr-tsy-00763-1.wav",
    BELLCORE_DIR "tr-tsy-00763-2.wav",
    BELLCORE_DIR "tr-tsy-00763-3.wav",
    BELLCORE_DIR "tr-tsy-00763-4.wav",
    BELLCORE_DIR "tr-tsy-00763-5.wav",
    BELLCORE_DIR "tr-tsy-00763-6.wav",
    ""
};

const char *tone_names[20] = {NULL};

SNDFILE *inhandle;

super_tone_rx_segment_t tone_segments[20][10];

super_tone_tx_step_t *dialtone_tree = NULL;
super_tone_tx_step_t *ringback_tree = NULL;
super_tone_tx_step_t *busytone_tree = NULL;
super_tone_tx_step_t *nutone_tree = NULL;
super_tone_tx_step_t *congestiontone_tree = NULL;
super_tone_tx_step_t *waitingtone_tree = NULL;

#if defined(HAVE_LIBXML2)
static int parse_tone(super_tone_rx_descriptor_t *desc, int tone_id, super_tone_tx_step_t **tree, xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur)
{
    xmlChar *x;
    float f1;
    float f2;
    float f_tol;
    float l1;
    float l2;
    float length;
    float length_tol;
    float recognition_length;
    float recognition_length_tol;
    int cycles;
    super_tone_tx_step_t *treep;
    int min_duration;
    int max_duration;

    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "step") == 0)
        {
            printf("Step - ");
            /* Set some defaults */
            f1 = 0.0;
            f2 = 0.0;
            f_tol = 1.0;
            l1 = -11.0;
            l2 = -11.0;
            length = 0.0;
            length_tol = 10.0;
            recognition_length = 0.0;
            recognition_length_tol = 10.0;
            cycles = 1;
            if ((x = xmlGetProp(cur, (const xmlChar *) "freq")))
            {
                sscanf((char *) x, "%f [%f%%]", &f1, &f_tol);
                sscanf((char *) x, "%f+%f [%f%%]", &f1, &f2, &f_tol);
                printf(" Frequency=%.2f+%.2f [%.2f%%]", f1, f2, f_tol);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "level")))
            {
                if (sscanf((char *) x, "%f+%f", &l1, &l2) < 2)
                    l2 = l1;
                printf(" Level=%.2f+%.2f", l1, l2);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "length")))
            {
                sscanf((char *) x, "%f [%f%%]", &length, &length_tol);
                printf(" Length=%.2f [%.2f%%]", length, length_tol);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "recognition-length")))
            {
                sscanf((char *) x, "%f [%f%%]", &recognition_length, &recognition_length_tol);
                printf(" Recognition length=%.2f [%.2f%%]", recognition_length, recognition_length_tol);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "cycles")))
            {
                if (strcasecmp((char *) x, "endless") == 0)
                    cycles = 0;
                else
                    cycles = atoi((char *) x);
                printf(" Cycles='%d' ", cycles);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "recorded-announcement")))
                printf(" Recorded announcement='%s'", x);
            printf("\n");
            if (f1  ||  f2  ||  length)
            {
                /* TODO: This cannot handle cycling patterns */
                if (length == 0.0)
                {
                    if (recognition_length)
                        min_duration = recognition_length*1000.0 + 0.5;
                    else
                        min_duration = 700;
                    max_duration = 0;
                }
                else
                {
                    if (recognition_length)
                        min_duration = recognition_length*1000.0 + 0.5;
                    else
                        min_duration = (length*1000.0 + 0.5)*(1.0 - length_tol/100.0) - 30;
                    max_duration = (length*1000.0 + 0.5)*(1.0 + length_tol/100.0) + 30;
                }
                printf(">>>Detector element %10d %10d %10d %10d\n", (int) (f1 + 0.5), (int) (f2 + 0.5), min_duration, max_duration);
                super_tone_rx_add_element(desc, tone_id, f1 + 0.5, f2 + 0.5, min_duration, max_duration);
            }
            treep = super_tone_tx_make_step(NULL,
                                            f1,
                                            l1,
                                            f2,
                                            l2,
                                            length*1000.0 + 0.5,
                                            cycles);
            *tree = treep;
            tree = &(treep->next);
            parse_tone(desc, tone_id, &(treep->nest), doc, ns, cur);
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    return  0;
}
/*- End of function --------------------------------------------------------*/

static void parse_tone_set(super_tone_rx_descriptor_t *desc, xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur)
{
    int tone_id;

    printf("Parsing tone set\n");
    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (strcmp((char *) cur->name, "dial-tone") == 0)
        {
            printf("Hit %s\n", cur->name);
            tone_id = super_tone_rx_add_tone(desc);
            dialtone_tree = NULL;
            parse_tone(desc, tone_id, &dialtone_tree, doc, ns, cur);
            tone_names[tone_id] = "Dial tone";
        }
        else if (strcmp((char *) cur->name, "ringback-tone") == 0)
        {
            printf("Hit %s\n", cur->name);
            tone_id = super_tone_rx_add_tone(desc);
            ringback_tree = NULL;
            parse_tone(desc, tone_id, &ringback_tree, doc, ns, cur);
            tone_names[tone_id] = "Ringback tone";
        }
        else if (strcmp((char *) cur->name, "busy-tone") == 0)
        {
            printf("Hit %s\n", cur->name);
            tone_id = super_tone_rx_add_tone(desc);
            busytone_tree = NULL;
            parse_tone(desc, tone_id, &busytone_tree, doc, ns, cur);
            tone_names[tone_id] = "Busy tone";
        }
        else if (strcmp((char *) cur->name, "number-unobtainable-tone") == 0)
        {
            printf("Hit %s\n", cur->name);
            tone_id = super_tone_rx_add_tone(desc);
            nutone_tree = NULL;
            parse_tone(desc, tone_id, &nutone_tree, doc, ns, cur);
            tone_names[tone_id] = "NU tone";
        }
        else if (strcmp((char *) cur->name, "congestion-tone") == 0)
        {
            printf("Hit %s\n", cur->name);
            tone_id = super_tone_rx_add_tone(desc);
            congestiontone_tree = NULL;
            parse_tone(desc, tone_id, &congestiontone_tree, doc, ns, cur);
            tone_names[tone_id] = "Congestion tone";
        }
        else if (strcmp((char *) cur->name, "waiting-tone") == 0)
        {
            printf("Hit %s\n", cur->name);
            tone_id = super_tone_rx_add_tone(desc);
            waitingtone_tree = NULL;
            parse_tone(desc, tone_id, &waitingtone_tree, doc, ns, cur);
            tone_names[tone_id] = "Waiting tone";
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
}
/*- End of function --------------------------------------------------------*/

static void get_tone_set(super_tone_rx_descriptor_t *desc, const char *tone_file, const char *set_id)
{
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;
#if 1
    xmlValidCtxt valid;
#endif
    xmlChar *x;
    
    ns = NULL;
    xmlKeepBlanksDefault(0);
    xmlCleanupParser();
    if ((doc = xmlParseFile(tone_file)) == NULL)
    {
        fprintf(stderr, "No document\n");
        exit(2);
    }
    /*endif*/
    xmlXIncludeProcess(doc);
#if 1
    if (!xmlValidateDocument(&valid, doc))
    {
        fprintf(stderr, "Invalid document\n");
        exit(2);
    }
    /*endif*/
#endif
    /* Check the document is of the right kind */
    if ((cur = xmlDocGetRootElement(doc)) == NULL)
    {
        fprintf(stderr, "Empty document\n");
        xmlFreeDoc(doc);
        exit(2);
    }
    /*endif*/
    if (xmlStrcmp(cur->name, (const xmlChar *) "global-tones"))
    {
        fprintf(stderr, "Document of the wrong type, root node != global-tones");
        xmlFreeDoc(doc);
        exit(2);
    }
    /*endif*/
    cur = cur->xmlChildrenNode;
    while (cur  &&  xmlIsBlankNode (cur))
        cur = cur->next;
    /*endwhile*/
    if (cur == NULL)
        exit(2);
    /*endif*/
    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "tone-set") == 0)
        {
            if ((x = xmlGetProp(cur, (const xmlChar *) "uncode")))
            {
                if (strcmp((char *) x, set_id) == 0)
                    parse_tone_set(desc, doc, ns, cur);
            }
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    xmlFreeDoc(doc);
}
/*- End of function --------------------------------------------------------*/
#endif

static void super_tone_rx_fill_descriptor(super_tone_rx_descriptor_t *desc)
{
    int tone_id;
    
    tone_id = super_tone_rx_add_tone(desc);
    super_tone_rx_add_element(desc, tone_id, 400, 0, 700, 0);
    tone_names[tone_id] = "XXX";

    tone_id = super_tone_rx_add_tone(desc);
    super_tone_rx_add_element(desc, tone_id, 1100, 0, 400, 600);
    super_tone_rx_add_element(desc, tone_id, 0, 0, 2800, 3200);
    tone_names[tone_id] = "FAX tone";
}
/*- End of function --------------------------------------------------------*/

static void wakeup(void *data, int code, int level, int delay)
{
    if (code >= 0)
        printf("Current tone is %d '%s' '%s'\n", code, (tone_names[code])  ?  tone_names[code]  :  "???", (char *) data);
    else
        printf("Tone off '%s'\n", (char *) data);
}
/*- End of function --------------------------------------------------------*/

static void tone_segment(void *data, int f1, int f2, int duration)
{
    if (f1 < 0)
        printf("Result %5d silence\n", duration);
    else if (f2 < 0)
        printf("Result %5d %4d\n", duration, f1);
    else
        printf("Result %5d %4d + %4d\n", duration, f1, f2);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int x;
    int16_t amp[8000];
    int sample;
    int frames;
    awgn_state_t noise_source;
    super_tone_rx_state_t *super;
    super_tone_rx_descriptor_t desc;

    if ((inhandle = sf_open_telephony_read(IN_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", IN_FILE_NAME);
        exit(2);
    }
    super_tone_rx_make_descriptor(&desc);
#if defined(HAVE_LIBXML2)
    get_tone_set(&desc, "../spandsp/global-tones.xml", (argc > 1)  ?  argv[1]  :  "hk");
#endif
    super_tone_rx_fill_descriptor(&desc);
    if ((super = super_tone_rx_init(NULL, &desc, wakeup, (void *) "test")) == NULL)
    {
        printf("    Failed to create detector.\n");
        exit(2);
    }
    super_tone_rx_segment_callback(super, tone_segment);
    awgn_init_dbm0(&noise_source, 1234567, -30.0f);
    printf("Processing file\n");
    while ((frames = sf_readf_short(inhandle, amp, 8000)))
    {
        /* Add some noise to the signal for a more meaningful test. */
        //for (sample = 0;  sample < frames;  sample++)
        //    amp[sample] += saturate(amp[sample] + awgn (&noise_source));
        for (sample = 0;  sample < frames;  )
        {
            x = super_tone_rx(super, amp + sample, frames - sample);
            sample += x;
        }
    }
    if (sf_close(inhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", IN_FILE_NAME);
        exit(2);
    }
#if 0
    /* Test for voice immunity */
    for (j = 0;  bellcore_files[j][0];  j++)
    {
        if ((inhandle = sf_open_telephony_read(bellcore_files[j], 1)) == NULL)
        {
            printf("    Cannot open audio file '%s'\n", bellcore_files[j]);
            exit(2);
        }
        while ((frames = sf_readf_short(inhandle, amp, 8000)))
        {
            for (sample = 0;  sample < frames;  )
            {
                x = super_tone_rx(super, amp + sample, frames - sample);
                sample += x;
            }
    	}
        if (sf_close(inhandle) != 0)
    	{
    	    printf("    Cannot close speech file '%s'\n", bellcore_files[j]);
            exit(2);
    	}
    }
#endif
    super_tone_rx_free(super);
    printf("Done\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
