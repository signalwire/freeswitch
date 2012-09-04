/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_generate_tests.c
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

/*! \file */

/*! \page super_tone_tx_tests_page Supervisory tone generation tests
\section super_tone_tx_tests_page_sec_1 What does it do?
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
#include <inttypes.h>
#include <sys/socket.h>
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

#define OUT_FILE_NAME   "super_tone.wav"

SNDFILE *outhandle;

super_tone_tx_step_t *tone_tree = NULL;

#if defined(HAVE_LIBXML2)
static void play_tones(super_tone_tx_state_t *tone, int max_samples)
{
    int16_t amp[8000];
    int len;
    int outframes;
    int i;
    int total_length;

    i = 500;
    total_length = 0;
    do
    {
        len = super_tone_tx(tone, amp, 160);
        outframes = sf_writef_short(outhandle, amp, len);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing audio file\n");
            exit(2);
        }
        total_length += len;
    }
    while (len > 0  &&  --i > 0);
    printf("Tone length = %d samples (%dms)\n", total_length, total_length/8);
}
/*- End of function --------------------------------------------------------*/

static int parse_tone(super_tone_tx_step_t **tree, xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur)
{
    xmlChar *x;
    float f1;
    float f2;
    float f_tol;
    float l1;
    float l2;
    float length;
    float length_tol;
    int cycles;
    super_tone_tx_step_t *treep;

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
            cycles = 1;
            if ((x = xmlGetProp(cur, (const xmlChar *) "freq")))
            {
                sscanf((char *) x, "%f [%f%%]", &f1, &f_tol);
                sscanf((char *) x, "%f+%f [%f%%]", &f1, &f2, &f_tol);
                printf("Frequency=%.2f+%.2f [%.2f%%]", f1, f2, f_tol);
                xmlFree(x);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "level")))
            {
                if (sscanf((char *) x, "%f+%f", &l1, &l2) < 2)
                    l2 = l1;
                printf("Level=%.2f+%.2f", l1, l2);
                xmlFree(x);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "length")))
            {
                sscanf((char *) x, "%f [%f%%]", &length, &length_tol);
                printf("Length=%.2f [%.2f%%]", length, length_tol);
                xmlFree(x);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "recognition-length")))
            {
                printf("Recognition length='%s'", x);
                xmlFree(x);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "cycles")))
            {
                if (strcasecmp((char *) x, "endless") == 0)
                    cycles = 0;
                else
                    cycles = atoi((char *) x);
                printf("Cycles=%d ", cycles);
                xmlFree(x);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "recorded-announcement")))
            {
                printf("Recorded announcement='%s'", x);
                xmlFree(x);
            }
            printf("\n");
            treep = super_tone_tx_make_step(NULL,
                                            f1,
                                            l1,
                                            f2,
                                            l2,
                                            length*1000.0 + 0.5,
                                            cycles);
            *tree = treep;
            tree = &(treep->next);
            parse_tone(&(treep->nest), doc, ns, cur);
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void parse_tone_set(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur)
{
    super_tone_tx_state_t tone;

    printf("Parsing tone set\n");
    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (strcmp((char *) cur->name + strlen((char *) cur->name) - 5, "-tone") == 0)
        {
            printf("Hit %s\n", cur->name);
            tone_tree = NULL;
            parse_tone(&tone_tree, doc, ns, cur);
            super_tone_tx_init(&tone, tone_tree);
//printf("Len %p %p %d %d\n", (void *) tone.levels[0], (void *) tone_tree, tone_tree->length, tone_tree->tone);
            play_tones(&tone, 99999999);
            super_tone_tx_free_tone(tone_tree);
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
}
/*- End of function --------------------------------------------------------*/

static void get_tone_set(const char *tone_file, const char *set_id)
{
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;
#if 0
    xmlValidCtxt valid;
#endif
    xmlChar *x;

    ns = NULL;    
    xmlKeepBlanksDefault(0);
    xmlCleanupParser();
    doc = xmlParseFile(tone_file);
    if (doc == NULL)
    {
        fprintf(stderr, "No document\n");
        exit(2);
    }
    /*endif*/
    xmlXIncludeProcess(doc);
#if 0
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
    while (cur  &&  xmlIsBlankNode(cur))
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
                    parse_tone_set(doc, ns, cur);
                /*endif*/
                xmlFree(x);
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

int main(int argc, char *argv[])
{
    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
#if defined(HAVE_LIBXML2)
    get_tone_set("../spandsp/global-tones.xml", (argc > 1)  ?  argv[1]  :  "hk");
#endif
    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    printf("Done\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
