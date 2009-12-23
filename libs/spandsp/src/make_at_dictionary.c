/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_at_dictionary.c - Generate a trie based dictionary for the AT
 *                        commands supported by the AT interpreter.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: make_at_dictionary.c,v 1.6 2009/10/09 14:53:57 steveu Exp $
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

const char *wordlist[] =
{
    " ",        /* Dummy to absorb spaces in commands */
    "&C",       /* V.250 6.2.8 - Circuit 109 (received line signal detector), behaviour */ 
    "&D",       /* V.250 6.2.9 - Circuit 108 (data terminal ready) behaviour */ 
    "&F",       /* V.250 6.1.2 - Set to factory-defined configuration */ 
    "+A8A",     /* V.251 6.3 - V.8 calling tone indication */
    "+A8C",     /* V.251 6.2 - V.8 answer signal indication */
    "+A8E",     /* V.251 5.1 - V.8 and V.8bis operation controls */
    "+A8I",     /* V.251 6.1 - V.8 CI signal indication */
    "+A8J",     /* V.251 6.4 - V.8 negotiation complete */
    "+A8M",     /* V.251 5.2 - Send V.8 menu signals */
    "+A8R",     /* V.251 6.6 - V.8bis signal and message reporting */
    "+A8T",     /* V.251 5.3 - Send V.8bis signal and/or message(s) */
    "+ASTO",    /* V.250 6.3.15 - Store telephone number */ 
    "+CAAP",    /* 3GPP TS 27.007 7.25 - Automatic answer for eMLPP Service */
    "+CACM",    /* 3GPP TS 27.007 8.25 - Accumulated call meter */
    "+CACSP",   /* 3GPP TS 27.007 11.1.7 - Voice Group or Voice Broadcast Call State Attribute Presentation */
    "+CAD",     /* IS-99 5.6.3 - Query analogue or digital service */
    "+CAEMLPP", /* 3GPP TS 27.007 7.22 - eMLPP Priority Registration and Interrogation */
    "+CAHLD",   /* 3GPP TS 27.007 11.1.3 - Leave an ongoing Voice Group or Voice Broadcast Call */
    "+CAJOIN",  /* 3GPP TS 27.007 11.1.1 - Accept an incoming Voice Group or Voice Broadcast Call */
    "+CALA",    /* 3GPP TS 27.007 8.16 - Alarm */
    "+CALCC",   /* 3GPP TS 27.007 11.1.6 - List current Voice Group and Voice Broadcast Calls */
    "+CALD",    /* 3GPP TS 27.007 8.38 - Delete alar m */
    "+CALM",    /* 3GPP TS 27.007 8.20 - Alert sound mode */
    "+CAMM",    /* 3GPP TS 27.007 8.26 - Accumulated call meter maximum */
    "+CANCHEV", /* 3GPP TS 27.007 11.1.8 - NCH Support Indication */
    "+CAOC",    /* 3GPP TS 27.007 7.16 - Advice of Charge */
    "+CAPD",    /* 3GPP TS 27.007 8.39 - Postpone or dismiss an alarm */
    "+CAPTT",   /* 3GPP TS 27.007 11.1.4 - Talker Access for Voice Group Call */
    "+CAREJ",   /* 3GPP TS 27.007 11.1.2 - Reject an incoming Voice Group or Voice Broadcast Call */
    "+CAULEV",  /* 3GPP TS 27.007 11.1.5 - Voice Group Call Uplink Status Presentation */
    "+CBC",     /* 3GPP TS 27.007 8.4 - Battery charge */
    "+CBCS",    /* 3GPP TS 27.007 11.3.2 - VBS subscriptions and GId status */
    "+CBIP",    /* IS-99 5.6 - Base station IP address */
    "+CBST",    /* 3GPP TS 27.007 6.7 - Select bearer service type */
    "+CCFC",    /* 3GPP TS 27.007 7.11 - Call forwarding number and conditions */
    "+CCLK",    /* 3GPP TS 27.007 8.15 - Clock */
    "+CCS",     /* IS-135 4.1.22 - Compression status */
    "+CCUG",    /* 3GPP TS 27.007 7.10 - Closed user group */
    "+CCWA",    /* 3GPP TS 27.007 7.12 - Call waiting */
    "+CCWE",    /* 3GPP TS 27.007 8.28 - Call Meter maximum event */
    "+CDIP",    /* 3GPP TS 27.007 7.9 - Called line identification presentation */
    "+CDIS",    /* 3GPP TS 27.007 8.8 - Display control */
    "+CDV",     /* IS-99 5.6 - Dial command for voice call */
    "+CEER",    /* 3GPP TS 27.007 6.10 - Extended error report */
    "+CESP",    /* GSM07.05  3.2.4 - Enter SMS block mode protocol */
    "+CFCS",    /* 3GPP TS 27.007 7.24 - Fast call setup conditions */
    "+CFG",     /* IS-99 5.6 - Configuration string */
    "+CFUN",    /* 3GPP TS 27.007 8.2 - Set phone functionality */
    "+CGACT",   /* 3GPP TS 27.007 10.1.10 - PDP context activate or deactivate */
    "+CGANS",   /* 3GPP TS 27.007 10.1.16 - Manual response to a network request for PDP context activation */
    "+CGATT",   /* 3GPP TS 27.007 10.1.9 - PS attach or detach */
    "+CGAUTO",  /* 3GPP TS 27.007 10.1.15 - Automatic response to a network request for PDP context activation */
    "+CGCAP",   /* IS-99 5.6 - Request complete capabilities list */
    "+CGCLASS", /* 3GPP TS 27.007 10.1.17 - GPRS mobile station class (GPRS only) */
    "+CGCLOSP", /* 3GPP TS 27.007 10.1.13 - Configure local octet stream PAD parameters (Obsolete) */
    "+CGCLPAD", /* 3GPP TS 27.007 10.1.12 - Configure local triple-X PAD parameters (GPRS only) (Obsolete) */
    "+CGCMOD",  /* 3GPP TS 27.007 10.1.11 - PDP Context Modify */
    "+CGCS",    /* 3GPP TS 27.007 11.3.1 - VGCS subscriptions and GId status */
    "+CGDATA",  /* 3GPP TS 27.007 10.1.12 - Enter data state */
    "+CGDCONT", /* 3GPP TS 27.007 10.1.1 - Define PDP Context */
    "+CGDSCONT",/* 3GPP TS 27.007 10.1.2 - Define Secondary PDP Context */
    "+CGEQMIN", /* 3GPP TS 27.007 10.1.7 - 3G Quality of Service Profile (Minimum acceptable) */
    "+CGEQNEG", /* 3GPP TS 27.007 10.1.8 - 3G Quality of Service Profile (Negotiated) */
    "+CGEQREQ", /* 3GPP TS 27.007 10.1.6 - 3G Quality of Service Profile (Requested) */
    "+CGEREP",  /* 3GPP TS 27.007 10.1.18 - Packet Domain event reporting */
    "+CGMI",    /* 3GPP TS 27.007 5.1 - Request manufacturer identification */
    "+CGMM",    /* 3GPP TS 27.007 5.2 - Request model identification */
    "+CGMR",    /* 3GPP TS 27.007 5.3 - Request revision identification */
    "+CGOI",    /* IS-99 5.6 - Request global object identification */
    "+CGPADDR", /* 3GPP TS 27.007 10.1.14 - Show PDP address */
    "+CGQMIN",  /* 3GPP TS 27.007 10.1.5 - Quality of Service Profile (Minimum acceptable) */
    "+CGQREQ",  /* 3GPP TS 27.007 10.1.4 - Quality of Service Profile (Requested) */
    "+CGREG",   /* 3GPP TS 27.007 10.1.19 - GPRS network registration status */
    "+CGSMS",   /* 3GPP TS 27.007 10.1.20 - Select service for MO SMS messages */
    "+CGSN",    /* 3GPP TS 27.007 5.4 - Request product serial number identification */
    "+CGTFT",   /* 3GPP TS 27.007 10.1.3 - Traffic Flow Template */
    "+CHLD",    /* 3GPP TS 27.007 7.13 - Call related supplementary services */
    "+CHSA",    /* 3GPP TS 27.007 6.18 - HSCSD non-transparent asymmetry configuration */
    "+CHSC",    /* 3GPP TS 27.007 6.15 - HSCSD current call parameters */
    "+CHSD",    /* 3GPP TS 27.007 6.12 - HSCSD device parameters */
    "+CHSN",    /* 3GPP TS 27.007 6.14 - HSCSD non-transparent call configuration */
    "+CHSR",    /* 3GPP TS 27.007 6.16 - HSCSD parameters report */
    "+CHST",    /* 3GPP TS 27.007 6.13 - HSCSD transparent call configuration */
    "+CHSU",    /* 3GPP TS 27.007 6.17 - HSCSD automatic user initiated upgrading */
    "+CHUP",    /* 3GPP TS 27.007 6.5 - Hangup call */
    "+CHV",     /* IS-99 5.6 - Hang-up voice */
    "+CIMI",    /* 3GPP TS 27.007 5.6 - Request international mobile subscriber identity */
    "+CIND",    /* 3GPP TS 27.007 8.9 - Indicator control */
    "+CIT",     /* IS-99 5.6 - Command state inactivity timer */
    "+CKPD",    /* 3GPP TS 27.007 8.7 - Keypad control */
    "+CLAC",    /* 3GPP TS 27.007 8.37 - List all available AT commands */
    "+CLAE",    /* 3GPP TS 27.007 8.31 - Language Event */
    "+CLAN",    /* 3GPP TS 27.007 8.30 - Set Language */
    "+CLCC",    /* 3GPP TS 27.007 7.18 - List current calls */
    "+CLCK",    /* 3GPP TS 27.007 7.4 - Facility lock */
    "+CLIP",    /* 3GPP TS 27.007 7.6 - Calling line identification presentation */
    "+CLIR",    /* 3GPP TS 27.007 7.7 - Calling line identification restriction */
    "+CLVL",    /* 3GPP TS 27.007 8.23 - Loudspeaker volume level */
    "+CMAR",    /* 3GPP TS 27.007 8.36 - Master reset */
    "+CMEC",    /* 3GPP TS 27.007 8.6 - Mobile termination control mode */
    "+CMEE",    /* GSM07.07 9.1 - Report mobile equipment error */
    "+CMER",    /* 3GPP TS 27.007 8.10 - Mobile termination event reporting */
    "+CMGC",    /* GSM07.05 3.5.5/4.5 - Send command */
    "+CMGD",    /* GSM07.05 3.5.4 - Delete message */
    "+CMGF",    /* GSM07.05 3.2.3 - Message Format */
    "+CMGL",    /* GSM07.05 3.4.2/4.1 -  List messages */
    "+CMGR",    /* GSM07.05 3.4.3/4.2 - Read message */
    "+CMGS",    /* GSM07.05 3.5.1/4.3 - Send message */
    "+CMGW",    /* GSM07.05 3.5.3/4.4 - Write message to memory */
    "+CMIP",    /* IS-99 5.6 - Mobile station IP address */
    "+CMM",     /* IS-135 4.1.23 - Menu map */
    "+CMMS",    /* GSM07.05 3.5.6 - More messages to send */
    "+CMOD",    /* 3GPP TS 27.007 6.4 - Call mode */
    "+CMSS",    /* GSM07.05 3.5.2/4.7 - Send message from storage */
    "+CMUT",    /* 3GPP TS 27.007 8.24 - Mute control */
    "+CMUX",    /* 3GPP TS 27.007 5.7 - Multiplexing mode */
    "+CNMA",    /* GSM07.05 3.4.4/4.6 - New message acknowledgement to terminal adapter */
    "+CNMI",    /* GSM07.05 3.4.1 - New message indications to terminal equipment */
    "+CNUM",    /* 3GPP TS 27.007 7.1 - Subscriber number */
    "+COLP",    /* 3GPP TS 27.007 7.8 - Connected line identification presentation */
    "+COPN",    /* 3GPP TS 27.007 7.21 - Read operator names */
    "+COPS",    /* 3GPP TS 27.007 7.3 - PLMN selection */
    "+COS",     /* IS-135 4.1.24 - Originating service */
    "+COTDI",   /* 3GPP TS 27.007 11.1.9 - Originator to Dispatcher Information */
    "+CPAS",    /* 3GPP TS 27.007 8.1 - Phone activity status */
    "+CPBF",    /* 3GPP TS 27.007 8.13 - Find phonebook entries */
    "+CPBR",    /* 3GPP TS 27.007 8.12 - Read phonebook entries */
    "+CPBS",    /* 3GPP TS 27.007 8.11 - Select phonebook memory storage */
    "+CPBW",    /* 3GPP TS 27.007 8.14 - Write phonebook entry */
    "+CPIN",    /* 3GPP TS 27.007 8.3 - Enter PIN */
    "+CPLS",    /* 3GPP TS 27.007 7.20 - Selection of preferred PLMN list */
    "+CPMS",    /* GSM07.05 3.2.2 - Preferred message storage */
    "+CPOL",    /* 3GPP TS 27.007 7.19 - Preferred PLMN list */
    "+CPPS",    /* 3GPP TS 27.007 7.23 - eMLPP subscriptions */
    "+CPROT",   /* 3GPP TS 27.007 8.42 - Enter protocol mode */
    "+CPUC",    /* 3GPP TS 27.007 8.27 - Price per unit and currency table */
    "+CPWC",    /* 3GPP TS 27.007 8.29 - Power class */
    "+CPWD",    /* 3GPP TS 27.007 7.5 - Change password */
    "+CQD",     /* IS-135 4.1.25 - Query disconnect timer */
    "+CR",      /* 3GPP TS 27.007 6.9 - Service reporting control */
    "+CRC",     /* 3GPP TS 27.007 6.11 - Cellular result codes */
    "+CREG",    /* 3GPP TS 27.007 7.2 - Network registration */
    "+CRES",    /* GSM07.05 3.3.6 - Restore Settings */
    "+CRLP",    /* 3GPP TS 27.007 6.8 - Radio link protocol */
    "+CRM",     /* IS-99 5.6 - Set rm interface protocol */
    "+CRMC",    /* 3GPP TS 27.007 8.34 - Ring Melody Control */
    "+CRMP",    /* 3GPP TS 27.007 8.35 - Ring Melody Playback */
    "+CRSL",    /* 3GPP TS 27.007 8.21 - Ringer sound level */
    "+CRSM",    /* 3GPP TS 27.007 8.18 - Restricted SIM access */
    "+CSAS",    /* GSM07.05 3.3.5 - Save settings */
    "+CSCA",    /* GSM07.05 3.3.1 - Service centre address */
    "+CSCB",    /* GSM07.05 3.3.4 - Select cell broadcast message types */
    "+CSCC",    /* 3GPP TS 27.007 8.19 - Secure control command */
    "+CSCS",    /* 3GPP TS 27.007 5.5 - Select TE character set */
    "+CSDF",    /* 3GPP TS 27.007 6.22 - Settings date format */
    "+CSDH",    /* GSM07.05 3.3.3 - Show text mode parameters */
    "+CSGT",    /* 3GPP TS 27.007 8.32 - Set Greeting Text */
    "+CSIL",    /* 3GPP TS 27.007 6.23 - Silence Command */
    "+CSIM",    /* 3GPP TS 27.007 8.17 - Generic SIM access */
    "+CSMP",    /* GSM07.05 3.3.2 - Set text mode parameters */
    "+CSMS",    /* GSM07.05 3.2.1 - Select Message Service */
    "+CSNS",    /* 3GPP TS 27.007 6.19 - Single numbering scheme */
    "+CSQ",     /* 3GPP TS 27.007 8.5 - Signal quality */
    "+CSS",     /* IS-135 4.1.28 - Serving system identification */
    "+CSSN",    /* 3GPP TS 27.007 7.17 - Supplementary service notifications */
    "+CSTA",    /* 3GPP TS 27.007 6.1 - Select type of address */
    "+CSTF",    /* 3GPP TS 27.007 6.24 - Settings time format */
    "+CSVM",    /* 3GPP TS 27.007 8.33 - Set Voice Mail Number */
    "+CTA",     /* IS-135 4.1.29 - MT-Terminated async. Data calls */
    "+CTF",     /* IS-135 4.1.30 - MT-Terminated FAX calls */
    "+CTFR",    /* 3GPP TS 27.007 7.14 - Call deflection */
    "+CTZR",    /* 3GPP TS 27.007 8.41 - Time Zone Reporting */
    "+CTZU",    /* 3GPP TS 27.007 8.40 - Automatic Time Zone Update */
    "+CUSD",    /* 3GPP TS 27.007 7.15 - Unstructured supplementary service data */
    "+CUUS1",   /* 3GPP TS 27.007 7.26 - User to User Signalling Service 1 */
    "+CV120",   /* 3GPP TS 27.007 6.21 - V.120 rate adaption protocol */
    "+CVHU",    /* 3GPP TS 27.007 6.20 - Voice Hangup Control */
    "+CVIB",    /* 3GPP TS 27.007 8.22 - Vibrator mode */
    "+CXT",     /* IS-99 5.6 - Cellular extension */
    "+DR",      /* V.250 6.6.2 - Data compression reporting */ 
    "+DS",      /* V.250 6.6.1 - Data compression */ 
    "+DS44",    /* V.250 6.6.2 - V.44 data compression */
    "+EB",      /* V.250 6.5.2 - Break handling in error control operation */ 
    "+EFCS",    /* V.250 6.5.4 - 32-bit frame check sequence */ 
    "+EFRAM",   /* V.250 6.5.8 - Frame length */ 
    "+ER",      /* V.250 6.5.5 - Error control reporting */ 
    "+ES",      /* V.250 6.5.1 - Error control selection */ 
    "+ESA",     /* V.80 8.2 - Synchronous access mode configuration */ 
    "+ESR",     /* V.250 6.5.3 - Selective repeat */ 
    "+ETBM",    /* V.250 6.5.6 - Call termination buffer management */ 
    "+EWIND",   /* V.250 6.5.7 - Window size */ 
    "+F34",     /* T.31 B.6.1 - Initial V.34 rate controls for FAX */
    "+FAA",     /* T.32 8.5.2.5 - Adaptive Answer parameter */
    "+FAP",     /* T.32 8.5.1.12 - Addressing and polling capabilities parameter */
    "+FAR",     /* T.31 8.5.1 - Adaptive reception control */ 
    "+FBO",     /* T.32 8.5.3.4 - Phase C data bit order */
    "+FBS",     /* T.32 8.5.3.2 - Buffer Size, read only parameter */
    "+FBU",     /* T.32 8.5.1.10 - HDLC Frame Reporting parameter */
    "+FCC",     /* T.32 8.5.1.1 - DCE capabilities parameters */
    "+FCL",     /* T.31 8.5.2 - Carrier loss timeout */ 
    "+FCLASS",  /* T.31 8.2 - Capabilities identification and control */ 
    "+FCQ",     /* T.32 8.5.2.3 -  Copy quality checking parameter */
    "+FCR",     /* T.32 8.5.1.9 - Capability to receive parameter */
    "+FCS",     /* T.32 8.5.1.3 - Current Session Results parameters */
    "+FCT",     /* T.32 8.5.2.6 - DTE phase C timeout parameter */
    "+FDD",     /* T.31 8.5.3 - Double escape character replacement */ 
    "+FDR",     /* T.32 8.3.4 - Data reception command */
    "+FDT",     /* T.32 8.3.3 - Data transmission command */
    "+FEA",     /* T.32 8.5.3.5 - Phase C received EOL alignment parameter */
    "+FFC",     /* T.32 8.5.3.6 - Format conversion parameter */
    "+FFD",     /* T.32 8.5.1.14 - File diagnostic message parameter */
    "+FHS",     /* T.32 8.5.2.7 - Call termination status parameter */
    "+FIE",     /* T.32 8.5.2.1 - Procedure interrupt enable parameter */
    "+FIP",     /* T.32 8.3.6 - Initialize facsimile parameters */
    "+FIS",     /* T.32 8.5.1.2 -  Current session parameters */
    "+FIT",     /* T.31 8.5.4 - DTE inactivity timeout */ 
    "+FKS",     /* T.32 8.3.5 - Session termination command */
    "+FLI",     /* T.32 8.5.1.5 - Local ID string parameter, TSI or CSI */
    "+FLO",     /* T.31 says to implement something similar to +IFC */ 
    "+FLP",     /* T.32 8.5.1.7 - Indicate document to poll parameter */
    "+FMI",     /* T.31 says to duplicate +GMI */ 
    "+FMM",     /* T.31 says to duplicate +GMM */ 
    "+FMR",     /* T.31 says to duplicate +GMR */ 
    "+FMS",     /* T.32 8.5.2.9 - Minimum phase C speed parameter */
    "+FND",     /* T.32 8.5.2.10 - Non-Standard Message Data Indication parameter */
    "+FNR",     /* T.32 8.5.1.11 - Negotiation message reporting control parameters */
    "+FNS",     /* T.32 8.5.1.6 - Non-Standard Frame FIF parameter */
    "+FPA",     /* T.32 8.5.1.13 - Selective polling address parameter */
    "+FPI",     /* T.32 8.5.1.5 - Local Polling ID String parameter */
    "+FPP",     /* T.32 8.5.3 - Facsimile packet protocol */
    "+FPR",     /* T.31 says to implement something similar to +IPR */ 
    "+FPS",     /* T.32 8.5.2.2 - Page Status parameter */
    "+FPW",     /* T.32 8.5.1.13 - PassWord parameter (Sending or Polling) */
    "+FRH",     /* T.31 8.3.6 - HDLC receive */ 
    "+FRM",     /* T.31 8.3.4 - Facsimile receive */ 
    "+FRQ",     /* T.32 8.5.2.4 - Receive Quality Thresholds parameters */
    "+FRS",     /* T.31 8.3.2 - Receive silence */ 
    "+FRY",     /* T.32 8.5.2.8 - ECM Retry Value parameter */
    "+FSA",     /* T.32 8.5.1.13 - Subaddress parameter */
    "+FSP",     /* T.32 8.5.1.8 - Request to poll parameter */
    "+FTH",     /* T.31 8.3.5 - HDLC transmit */ 
    "+FTM",     /* T.31 8.3.3 - Facsimile transmit */ 
    "+FTS",     /* T.31 8.3.1 - Transmit silence */ 
    "+GCAP",    /* V.250 6.1.9 - Request complete capabilities list */ 
    "+GCI",     /* V.250 6.1.10 - Country of installation, */ 
    "+GMI",     /* V.250 6.1.4 - Request manufacturer identification */ 
    "+GMM",     /* V.250 6.1.5 - Request model identification */ 
    "+GMR",     /* V.250 6.1.6 - Request revision identification */ 
    "+GOI",     /* V.250 6.1.8 - Request global object identification */ 
    "+GSN",     /* V.250 6.1.7 - Request product serial number identification */ 
    "+IBC",     /* V.80 7.9 - Control of in-band control */
    "+IBM",     /* V.80 7.10 - In-band MARK idle reporting control */
    "+ICF",     /* V.250 6.2.11 - DTE-DCE character framing */ 
    "+ICLOK",   /* V.250 6.2.14 - Select sync transmit clock source */ 
    "+IDSR",    /* V.250 6.2.16 - Select data set ready option */ 
    "+IFC",     /* V.250 6.2.12 - DTE-DCE local flow control */ 
    "+ILRR",    /* V.250 6.2.13 - DTE-DCE local rate reporting */ 
    "+ILSD",    /* V.250 6.2.15 - Select long space disconnect option */ 
    "+IPR",     /* V.250 6.2.10 - Fixed DTE rate */ 
    "+IRTS",    /* V.250 6.2.17 - Select synchronous mode RTS option */ 
    "+ITF",     /* V.80 8.4 - Transmit flow control thresholds */
    "+MA",      /* V.250 6.4.2 - Modulation automode control */ 
    "+MR",      /* V.250 6.4.3 - Modulation reporting control */ 
    "+MS",      /* V.250 6.4.1 - Modulation selection */ 
    "+MSC",     /* V.250 6.4.8 - Seamless rate change enable */ 
    "+MV18AM",  /* V.250 6.4.6 - V.18 answering message editing */ 
    "+MV18P",   /* V.250 6.4.7 - Order of probes */ 
    "+MV18R",   /* V.250 6.4.5 - V.18 reporting control */ 
    "+MV18S",   /* V.250 6.4.4 - V.18 selection */
    "+PCW",     /* V.250 6.8.1 - Call waiting enable (V.92 DCE) */
    "+PIG",     /* V.250 6.8.5 - PCM upstream ignore */
    "+PMH",     /* V.250 6.8.2 - Modem on hold enable */
    "+PMHF",    /* V.250 6.8.6 - V.92 Modem on hold hook flash */
    "+PMHR",    /* V.250 6.8.4 - Initiate modem on hold */
    "+PMHT",    /* V.250 6.8.3 - Modem on hold timer */
    "+PQC",     /* V.250 6.8.7 - V.92 Phase 1 and Phase 2 Control */
    "+PSS",     /* V.250 6.8.8 - V.92 Use Short Sequence */
    "+SAC",     /* V.252 3.4 - Audio transmit configuration */
    "+SAM",     /* V.252 3.5 - Audio receive mode */
    "+SAR",     /* V.252 5.3 - Audio receive channel indication */
    "+SARR",    /* V.252 3.9 - Audio indication reporting */
    "+SAT",     /* V.252 5.4 - Audio transmit channel indication */
    "+SCRR",    /* V.252 3.11 - Capabilities indication reporting */
    "+SDC",     /* V.252 3.3 - Data configuration */
    "+SDI",     /* V.252 5.2 - Data channel identification */
    "+SDR",     /* V.252 3.8 - Data indication reporting */
    "+SRSC",    /* V.252 5.1.2 - Remote terminal simultaneous capability indication */
    "+STC",     /* V.252 3.1 - Terminal configuration */
    "+STH",     /* V.252 3.2 - Close logical channel */
    "+SVC",     /* V.252 3.6 - Video transmit configuration */
    "+SVM",     /* V.252 3.7 - Video receive mode */
    "+SVR",     /* V.252 5.5 - Video receive channel indication */
    "+SVRR",    /* V.252 3.10 - Video indication reporting */
    "+SVT",     /* V.252 5.6 - Video transmit channel indication */
    "+TADR",    /* V.250 6.7.2.9 - Local V.54 address */ 
    "+TAL",     /* V.250 6.7.2.15 - Local analogue loop */ 
    "+TALS",    /* V.250 6.7.2.6 - Analogue loop status */ 
    "+TDLS",    /* V.250 6.7.2.7 - Local digital loop status */ 
    "+TE140",   /* V.250 6.7.2.1 - Enable ckt 140 */ 
    "+TE141",   /* V.250 6.7.2.2 - Enable ckt 141 */ 
    "+TEPAL",   /* V.250 6.7.2.5 - Enable front panel analogue loop */ 
    "+TEPDL",   /* V.250 6.7.2.4 - Enable front panel RDL */ 
    "+TERDL",   /* V.250 6.7.2.3 - Enable RDL from remote */ 
    "+TLDL",    /* V.250 6.7.2.13 - Local digital loop */ 
    "+TMO",     /* V.250 6.9 - V.59 command */
    "+TMODE",   /* V.250 6.7.2.10 - Set V.54 mode */ 
    "+TNUM",    /* V.250 6.7.2.12 - Errored bit and block counts */ 
    "+TRDL",    /* V.250 6.7.2.14 - Request remote digital loop */ 
    "+TRDLS",   /* V.250 6.7.2.8 - Remote digital loop status */ 
    "+TRES",    /* V.250 6.7.2.17 - Self test result */ 
    "+TSELF",   /* V.250 6.7.2.16 - Self test */ 
    "+TTER",    /* V.250 6.7.2.11 - Test error rate */ 
    "+VAC",     /* V.252 4.1 - Set audio code */
    "+VACR",    /* V.252 6.1 - Audio code report */
    "+VBT",     /* 3GPP TS 27.007 C.2.2 - Buffer threshold setting */
    "+VCID",    /* V.253 9.2.3 - Caller ID service */
    "+VCIDR",   /* V.252 6.2 - Caller ID report */
    "+VDID",    /* V.253 9.2.4 - DID service */
    "+VDIDR",   /* V.252 6.2 - DID report */
    "+VDR",     /* V.253 10.3.1 - Distinctive ring (ring cadence reporting) */
    "+VDT",     /* V.253 10.3.2 - Control tone cadence reporting */
    "+VDX",     /* V.253 10.5.6 - Speakerphone duplex mode */
    "+VEM",     /* V.253 10.5.7 - Deliver event reports */
    "+VGM",     /* V.253 10.5.2 - Microphone gain */
    "+VGR",     /* V.253 10.2.1 - Receive gain selection */
    "+VGS",     /* V.253 10.5.3 - Speaker gain */
    "+VGT",     /* V.253 10.2.2 - Volume selection */
    "+VHC",     /* V.252 4.12 - Telephony port hook control */
    "+VIP",     /* V.253 10.1.1 - Initialize voice parameters */
    "+VIT",     /* V.253 10.2.3 - DTE/DCE inactivity timer */
    "+VLS",     /* V.253 10.2.4 - Analogue source/destination selection */
    "+VNH",     /* V.253 9.2.5 - Automatic hangup control */
    "+VPH",     /* V.252 4.11 - Phone hookswitch status */
    "+VPP",     /* V.253 10.4.2 - Voice packet protocol */
    "+VPR",     /* IS-101 10.4.3 - Select DTE/DCE interface rate */
    "+VRA",     /* V.253 10.2.5 - Ringing tone goes away timer */
    "+VRID",    /* Extension - Find the originating and destination numbers */
    "+VRL",     /* V.253 10.1.2 - Ring local phone */
    "+VRN",     /* V.253 10.2.6 - Ringing tone never appeared timer */
    "+VRX",     /* V.253 10.1.3 - Voice receive state */
    "+VSD",     /* V.253 10.2.7 - Silence detection (QUIET and SILENCE) */
    "+VSID",    /* Extension - Set the originating number */
    "+VSM",     /* V.253 10.2.8 - Compression method selection */
    "+VSP",     /* V.253 10.5.1 - Voice speakerphone state */
    "+VTA",     /* V.253 10.5.4 - Train acoustic echo-canceller */ 
    "+VTD",     /* V.253 10.2.9 - Beep tone duration timer */
    "+VTER",    /* V.252 6.4 - Simple telephony event report */
    "+VTH",     /* V.253 10.5.5 - Train line echo-canceller */ 
    "+VTR",     /* V.253 10.1.4 - Voice duplex state */
    "+VTS",     /* V.253 10.1.5 - DTMF and tone generation in voice */
    "+VTX",     /* V.253 10.1.6 - Transmit data state */
    "+VXT",     /* IS-101 10.1.5 - Translate voice data */
    "+W",       /* TIA-678 5.2.4.1 - Compliance indication */
    "+WBAG",    /* TIA-678 C.5.6 Bias modem audio gain */
    "+WCDA",    /* TIA-678 B.3.2.5 Display data link address */
    "+WCHG",    /* TIA-678 B.3.2.4 Display battery charging status */
    "+WCID",    /* TIA-678 B.3.2.1 Display system ID (operator) */
    "+WCLK",    /* TIA-678 B.3.2.3 Lock/unlock DCE */
    "+WCPN",    /* TIA-678 B.3.2.2 Set personal identification number */
    "+WCXF",    /* TIA-678 B.3.2.6 Display supported annex B commands */
    "+WDAC",    /* TIA-678 C.5.1 Data over analogue cellular command query */
    "+WDIR",    /* TIA-678 C.5.8 Phone number directory selection */
    "+WECR",    /* TIA-678 C.5.3 Enable cellular result codes */
    "+WFON",    /* TIA-678 C.5.5 Phone specification */
    "+WKPD",    /* TIA-678 C.5.7 Keypad emulation */
    "+WPBA",    /* TIA-678 C.5.9 Phone battery query */
    "+WPTH",    /* TIA-678 C.5.10 Call path */
    "+WRLK",    /* TIA-678 C.5.4 Roam lockout */
    "+WS45",    /* TIA-678 5.2.4.2 DTE-side stack selection */
    "+WS46",    /* 3GPP TS 27.007 5.9 - PCCA STD-101 [17] select wireless network */
    "+WS50",    /* TIA-678 B.3.1.1 Normalized signal strength */
    "+WS51",    /* TIA-678 B.3.1.2 Carrier detect signal threshold */
    "+WS52",    /* TIA-678 B.3.1.3 Normalized battery level */
    "+WS53",    /* TIA-678 B.3.1.4 Normalized channel quality */
    "+WS54",    /* TIA-678 B.3.1.5 Carrier detect channel quality threshold */
    "+WS57",    /* TIA-678 B.3.1.7 Antenna preference */
    "+WS58",    /* TIA-678 B.3.1.8 Idle time-out value */
    "+WSTL",    /* TIA-678 C.5.2 Call session time limit */
    ";",        /* Dummy to absorb semi-colon delimiters in commands */
    "A",        /* V.250 6.3.5 - Answer */ 
    "D",        /* V.250 6.3.1 - Dial */ 
    "E",        /* V.250 6.2.4 - Command echo */ 
    "H",        /* V.250 6.3.6 - Hook control */ 
    "I",        /* V.250 6.1.3 - Request identification information */ 
    "L",        /* V.250 6.3.13 - Monitor speaker loudness */ 
    "M",        /* V.250 6.3.14 - Monitor speaker mode */ 
    "O",        /* V.250 6.3.7 - Return to online data state */ 
    "P",        /* V.250 6.3.3 - Select pulse dialling (command) */ 
    "Q",        /* V.250 6.2.5 - Result code suppression */ 
    "S0",       /* V.250 6.3.8 - Automatic answer */ 
    "S10",      /* V.250 6.3.12 - Automatic disconnect delay */ 
    "S3",       /* V.250 6.2.1 - Command line termination character */ 
    "S4",       /* V.250 6.2.2 - Response formatting character */ 
    "S5",       /* V.250 6.2.3 - Command line editing character */ 
    "S6",       /* V.250 6.3.9 - Pause before blind dialling */ 
    "S7",       /* V.250 6.3.10 - Connection completion timeout */ 
    "S8",       /* V.250 6.3.11 - Comma dial modifier time */ 
    "T",        /* V.250 6.3.2 - Select tone dialling (command) */ 
    "V",        /* V.250 6.2.6 - DCE response format */ 
    "X",        /* V.250 6.2.7 - Result code selection and call progress monitoring control */ 
    "Z",        /* V.250 6.1.1 - Reset to default configuration */
    NULL
};

int packed_ptr = 0;

short int packed_trie[30000];

#define ALPHABET_SIZE       128

typedef struct trie_node_s
{
    int first;
    int last;
    int node_no;
    int entry;
    /* Array of pointers to children */
    struct trie_node_s *child_list[ALPHABET_SIZE];
} trie_node_t;

typedef struct
{
    int entries;
    /* The root of the trie */
    trie_node_t *root;
} trie_t;

static trie_node_t *trie_node_create(void)
{
    trie_node_t *s;
    
    if ((s = (trie_node_t *) malloc(sizeof(*s))))
    {
        memset(s, 0, sizeof(*s));
        s->first = ALPHABET_SIZE - 1;
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

static trie_t *trie_create(void)
{
    trie_t *s;
    
    if ((s = (trie_t *) malloc(sizeof(*s))))
    {
        memset(s, 0, sizeof(*s));
        s->root = trie_node_create();
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

static void trie_recursive_add_node_numbers(trie_node_t *t)
{
    int index;
 
    if (t)
    {
        if (t->first <= t->last)
        {
            t->node_no = packed_ptr + 1;
            packed_ptr += (t->last - t->first + 1 + 3);
            for (index = 0;  index < ALPHABET_SIZE;  index++)
                trie_recursive_add_node_numbers(t->child_list[index]);
        }
        else
        {
            t->node_no = packed_ptr + 1;
            packed_ptr += 3;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void trie_recursive_build_packed_trie(trie_node_t *t)
{
    int i;
 
    if (t)
    {
        if (t->first <= t->last)
        {
            packed_trie[packed_ptr++] = t->first;
            packed_trie[packed_ptr++] = t->last;
            packed_trie[packed_ptr++] = t->entry;
            for (i = t->first;  i <= t->last;  i++)
                packed_trie[packed_ptr++] = (t->child_list[i])  ?  t->child_list[i]->node_no  :  0;
            for (i = t->first;  i <= t->last;  i++)
                trie_recursive_build_packed_trie(t->child_list[i]);
        }
        else
        {
            packed_trie[packed_ptr++] = 1;
            packed_trie[packed_ptr++] = 0;
            packed_trie[packed_ptr++] = t->entry;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void trie_add(trie_t *s, const char *u, size_t len)
{
    size_t i;
    int index;
    trie_node_t *t;

    s->entries++;
    /* Loop over the length of the string to add and traverse the trie... */
    for (t = s->root, i = 0;  i < len;  i++)
    {
        /* The character in u we are processing... */
        index = (unsigned char) u[i];
 
        /* Is there a child node for this character? */
        if (t->child_list[index] == NULL)
        {
            t->child_list[index] = trie_node_create();
            if (index < t->first)
                t->first = index;
            if (index > t->last)
                t->last = index;
        }
 
        /* Move to the new node... and loop */
        t = t->child_list[index];
    }
    t->entry = s->entries;
}
/*- End of function --------------------------------------------------------*/

static void dump_trie(void)
{
    int i;

    printf("\nstatic const at_cmd_service_t at_commands[] =\n{\n");
    for (i = 0;  wordlist[i];  i++)
    {
        switch (wordlist[i][0])
        {
        case ' ':
        case ';':
            printf("    at_cmd_dummy,\n");
            break;
        case '+':
            printf("    at_cmd_plus_%s,\n", wordlist[i] + 1);
            break;
        case '&':
            printf("    at_cmd_amp_%s,\n", wordlist[i] + 1);
            break;
        default:
            printf("    at_cmd_%s,\n", wordlist[i]);
            break;
        }
    }
    printf("};\n");

    printf("\nstatic const uint16_t command_trie[] =\n{");
    for (i = 0;  i < packed_ptr;  i++)
    {
        if ((i & 7) == 0)
            printf("\n    ");
        printf("0x%04X, ", packed_trie[i]);
    }
    printf("\n};\n");
    printf("\n#define COMMAND_TRIE_LEN %d\n", packed_ptr);

}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    trie_t *s;
    int i;
    
    s = trie_create();

    for (i = 0;  wordlist[i];  i++)
        trie_add(s, wordlist[i], strlen(wordlist[i]));
    printf("// The trie contains %d entries\n", i);

    packed_ptr = 0;
    trie_recursive_add_node_numbers(s->root);
    packed_ptr = 0;
    trie_recursive_build_packed_trie(s->root);

    dump_trie();

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
