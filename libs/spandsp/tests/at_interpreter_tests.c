/*
 * SpanDSP - a series of DSP components for telephony
 *
 * at_interpreter_tests.c - Tests for the AT interpreter.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004, 2005, 2006 Steve Underwood
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

/*! \page at_interpreter_tests_page AT interpreter tests
\section at_interpreter_tests_page_sec_1 What does it do?
These tests exercise all the commands which should be understood by the AT interpreter.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sndfile.h>

#include "spandsp.h"

#define DLE 0x10
#define ETX 0x03
#define SUB 0x1A

#define MANUFACTURER            "www.soft-switch.org"

struct command_response_s
{
    const char *command;
    const char *response;
};

static const struct command_response_s general_test_seq[] =
{
    /* Try the various cases for "AT" */
    {"atE1\r", "atE1\r\r\nOK\r\n"},
    {"AtE1\r", "AtE1\r\r\nOK\r\n"},
    {"aTE1\r", "aTE1\r\r\nOK\r\n"},
    {"ATE1\r", "ATE1\r\r\nOK\r\n"},
    {"ATE0\r", "ATE0\r\r\nOK\r\n"},

    /* Try the various command formats */
    {"ATS8?\r", "\r\n005\r\n\r\nOK\r\n"},
    {"ATS8=1\r", "\r\nOK\r\n"},
    {"ATS8.5?\r", "\r\n0\r\n\r\nOK\r\n"},
    {"ATS8.5=1\r", "\r\nOK\r\n"},
    {"ATS8.5?\r", "\r\n1\r\n\r\nOK\r\n"},
    {"ATS8?\r", "\r\n033\r\n\r\nOK\r\n"},
    {"AT+FCLASS=1\r", "\r\nOK\r\n"},
    {"AT+FCLASS?\r", "\r\n1\r\n\r\nOK\r\n"},
    {"AT+FCLASS=?\r", "\r\n0,1,1.0\r\n\r\nOK\r\n"},

    /* Try all the commands */
    {"AT&C\r", "\r\nOK\r\n"},                                       /* V.250 6.2.8 - Circuit 109 (received line signal detector), behaviour */
    {"AT&D\r", "\r\nOK\r\n"},                                       /* V.250 6.2.9 - Circuit 108 (data terminal ready) behaviour */
    {"AT&F\r", "\r\nOK\r\n"},                                       /* V.250 6.1.2 - Set to factory-defined configuration */
    {"ATE0\r", "ATE0\r\r\nOK\r\n"},                                 /* Counteract the effects of the above */
    {"AT+A8E=?\r", "\r\n+A8E:(0-6),(0-5),(00-FF)\r\n\r\nOK\r\n"},   /* V.251 5.1 - V.8 and V.8bis operation controls */
    {"AT+A8M\r", "\r\nOK\r\n"},                                     /* V.251 5.2 - Send V.8 menu signals */
    {"AT+A8T=?\r", "\r\n+A8T:(0-10)\r\n\r\nOK\r\n"},                /* V.251 5.3 - Send V.8bis signal and/or message(s) */
    {"AT+ASTO=?\r", "\r\n+ASTO:\r\n\r\nOK\r\n"},                    /* V.250 6.3.15 - Store telephone number */
    {"AT+CAAP=?\r", "\r\n+CAAP:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.25 - Automatic answer for eMLPP Service */
    {"AT+CACM=?\r", "\r\n+CACM:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.25 - Accumulated call meter */
    {"AT+CACSP=?\r", "\r\n+CACSP:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 11.1.7 - Voice Group or Voice Broadcast Call State Attribute Presentation */
    {"AT+CAEMLPP=?\r", "\r\n+CAEMLPP:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 7.22 - eMLPP Priority Registration and Interrogation */
    {"AT+CAHLD=?\r", "\r\n+CAHLD:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 11.1.3 - Leave an ongoing Voice Group or Voice Broadcast Call */
    {"AT+CAJOIN=?\r", "\r\n+CAJOIN:\r\n\r\nOK\r\n"},                /* 3GPP TS 27.007 11.1.1 - Accept an incoming Voice Group or Voice Broadcast Call */
    {"AT+CALA=?\r", "\r\n+CALA:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.16 - Alarm */
    {"AT+CALCC=?\r", "\r\n+CALCC:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 11.1.6 - List current Voice Group and Voice Broadcast Calls */
    {"AT+CALD=?\r", "\r\n+CALD:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.38 - Delete alar m */
    {"AT+CALM=?\r", "\r\n+CALM:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.20 - Alert sound mode */
    {"AT+CAMM=?\r", "\r\n+CAMM:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.26 - Accumulated call meter maximum */
    {"AT+CANCHEV=?\r", "\r\n+CANCHEV:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 11.1.8 - NCH Support Indication */
    {"AT+CAOC=?\r", "\r\n+CAOC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.16 - Advice of Charge */
    {"AT+CAPD=?\r", "\r\n+CAPD:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.39 - Postpone or dismiss an alarm */
    {"AT+CAPTT=?\r", "\r\n+CAPTT:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 11.1.4 - Talker Access for Voice Group Call */
    {"AT+CAREJ=?\r", "\r\n+CAREJ:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 11.1.2 - Reject an incoming Voice Group or Voice Broadcast Call */
    {"AT+CAULEV=?\r", "\r\n+CAULEV:\r\n\r\nOK\r\n"},                /* 3GPP TS 27.007 11.1.5 - Voice Group Call Uplink Status Presentation */
    {"AT+CBC=?\r", "\r\n+CBC:\r\n\r\nOK\r\n"},                      /* 3GPP TS 27.007 8.4 - Battery charge */
    {"AT+CBCS=?\r", "\r\n+CBCS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 11.3.2 - VBS subscriptions and GId status */
    {"AT+CBST=?\r", "\r\n+CBST:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.7 - Select bearer service type */
    {"AT+CCFC=?\r", "\r\n+CCFC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.11 - Call forwarding number and conditions */
    {"AT+CCLK=?\r", "\r\n+CCLK:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.15 - Clock */
    {"AT+CCUG=?\r", "\r\n+CCUG:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.10 - Closed user group */
    {"AT+CCWA=?\r", "\r\n+CCWA:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.12 - Call waiting */
    {"AT+CCWE=?\r", "\r\n+CCWE:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.28 - Call Meter maximum event */
    {"AT+CDIP=?\r", "\r\n+CDIP:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.9 - Called line identification presentation */
    {"AT+CDIS=?\r", "\r\n+CDIS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.8 - Display control */
    {"AT+CEER=?\r", "\r\n+CEER:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.10 - Extended error report */
    {"AT+CFCS=?\r", "\r\n+CFCS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.24 - Fast call setup conditions */
    {"AT+CFUN=?\r", "\r\n+CFUN:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.2 - Set phone functionality */
    {"AT+CGACT=?\r", "\r\n+CGACT:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 10.1.10 - PDP context activate or deactivate */
    {"AT+CGANS=?\r", "\r\n+CGANS:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 10.1.16 - Manual response to a network request for PDP context activation */
    {"AT+CGATT=?\r", "\r\n+CGATT:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 10.1.9 - PS attach or detach */
    {"AT+CGAUTO=?\r", "\r\n+CGAUTO:\r\n\r\nOK\r\n"},                /* 3GPP TS 27.007 10.1.15 - Automatic response to a network request for PDP context activation */
    {"AT+CGCLASS=?\r", "\r\n+CGCLASS:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 10.1.17 - GPRS mobile station class (GPRS only) */
    {"AT+CGCLOSP=?\r", "\r\n+CGCLOSP:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 10.1.13 - Configure local octet stream PAD parameters (Obsolete) */
    {"AT+CGCLPAD=?\r", "\r\n+CGCLPAD:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 10.1.12 - Configure local triple-X PAD parameters (GPRS only) (Obsolete) */
    {"AT+CGCMOD=?\r", "\r\n+CGCMOD:\r\n\r\nOK\r\n"},                /* 3GPP TS 27.007 10.1.11 - PDP Context Modify */
    {"AT+CGCS=?\r", "\r\n+CGCS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 11.3.1 - VGCS subscriptions and GId status */
    {"AT+CGDATA=?\r", "\r\n+CGDATA:\r\n\r\nOK\r\n"},                /* 3GPP TS 27.007 10.1.12 - Enter data state */
    {"AT+CGDCONT=?\r", "\r\n+CGDCONT:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 10.1.1 - Define PDP Context */
    {"AT+CGDSCONT=?\r", "\r\n+CGDSCONT:\r\n\r\nOK\r\n"},            /* 3GPP TS 27.007 10.1.2 - Define Secondary PDP Context */
    {"AT+CGEQMIN=?\r", "\r\n+CGEQMIN:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 10.1.7 - 3G Quality of Service Profile (Minimum acceptable) */
    {"AT+CGEQNEG=?\r", "\r\n+CGEQNEG:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 10.1.8 - 3G Quality of Service Profile (Negotiated) */
    {"AT+CGEQREQ=?\r", "\r\n+CGEQREQ:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 10.1.6 - 3G Quality of Service Profile (Requested) */
    {"AT+CGEREP=?\r", "\r\n+CGEREP:\r\n\r\nOK\r\n"},                /* 3GPP TS 27.007 10.1.18 - Packet Domain event reporting */
    {"AT+CGMI=?\r", "\r\n+CGMI:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 5.1 - Request manufacturer identification */
    {"AT+CGMM=?\r", "\r\n+CGMM:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 5.2 - Request model identification */
    {"AT+CGMR=?\r", "\r\n+CGMR:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 5.3 - Request revision identification */
    {"AT+CGPADDR=?\r", "\r\n+CGPADDR:\r\n\r\nOK\r\n"},              /* 3GPP TS 27.007 10.1.14 - Show PDP address */
    {"AT+CGQMIN=?\r", "\r\n+CGQMIN:\r\n\r\nOK\r\n"},                /* 3GPP TS 27.007 10.1.5 - Quality of Service Profile (Minimum acceptable) */
    {"AT+CGQREQ=?\r", "\r\n+CGQREQ:\r\n\r\nOK\r\n"},                /* 3GPP TS 27.007 10.1.4 - Quality of Service Profile (Requested) */
    {"AT+CGREG=?\r", "\r\n+CGREG:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 10.1.19 - GPRS network registration status */
    {"AT+CGSMS=?\r", "\r\n+CGSMS:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 10.1.20 - Select service for MO SMS messages */
    {"AT+CGSN=?\r", "\r\n+CGSN:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 5.4 - Request product serial number identification */
    {"AT+CGTFT=?\r", "\r\n+CGTFT:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 10.1.3 - Traffic Flow Template */
    {"AT+CHLD=?\r", "\r\n+CHLD:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.13 - Call related supplementary services */
    {"AT+CHSA=?\r", "\r\n+CHSA:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.18 - HSCSD non-transparent asymmetry configuration */
    {"AT+CHSC=?\r", "\r\n+CHSC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.15 - HSCSD current call parameters */
    {"AT+CHSD=?\r", "\r\n+CHSD:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.12 - HSCSD device parameters */
    {"AT+CHSN=?\r", "\r\n+CHSN:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.14 - HSCSD non-transparent call configuration */
    {"AT+CHSR=?\r", "\r\n+CHSR:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.16 - HSCSD parameters report */
    {"AT+CHST=?\r", "\r\n+CHST:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.13 - HSCSD transparent call configuration */
    {"AT+CHSU=?\r", "\r\n+CHSU:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.17 - HSCSD automatic user initiated upgrading */
    {"AT+CHUP=?\r", "\r\n+CHUP:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.5 - Hangup call */
    {"AT+CIMI=?\r", "\r\n+CIMI:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 5.6 - Request international mobile subscriber identity */
    {"AT+CIND=?\r", "\r\n+CIND:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.9 - Indicator control */
    {"AT+CKPD=?\r", "\r\n+CKPD:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.7 - Keypad control */
    {"AT+CLAC=?\r", "\r\n+CLAC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.37 - List all available AT commands */
    {"AT+CLAE=?\r", "\r\n+CLAE:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.31 - Language Event */
    {"AT+CLAN=?\r", "\r\n+CLAN:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.30 - Set Language */
    {"AT+CLCC=?\r", "\r\n+CLCC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.18 - List current calls */
    {"AT+CLCK=?\r", "\r\n+CLCK:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.4 - Facility lock */
    {"AT+CLIP=?\r", "\r\n+CLIP:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.6 - Calling line identification presentation */
    {"AT+CLIR=?\r", "\r\n+CLIR:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.7 - Calling line identification restriction */
    {"AT+CLVL=?\r", "\r\n+CLVL:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.23 - Loudspeaker volume level */
    {"AT+CMAR=?\r", "\r\n+CMAR:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.36 - Master Reset */
    {"AT+CMEC=?\r", "\r\n+CMEC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.6 - Mobile Termination control mode */
    {"AT+CMER=?\r", "\r\n+CMER:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.10 - Mobile Termination event reporting */
    {"AT+CMOD=?\r", "\r\n+CMOD:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.4 - Call mode */
    {"AT+CMUT=?\r", "\r\n+CMUT:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.24 - Mute control */
    {"AT+CMUX=?\r", "\r\n+CMUX:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 5.7 - Multiplexing mode */
    {"AT+CNUM=?\r", "\r\n+CNUM:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.1 - Subscriber number */
    {"AT+COLP=?\r", "\r\n+COLP:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.8 - Connected line identification presentation */
    {"AT+COPN=?\r", "\r\n+COPN:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.21 - Read operator names */
    {"AT+COPS=?\r", "\r\n+COPS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.3 - PLMN selection */
    {"AT+COTDI=?\r", "\r\n+COTDI:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 11.1.9 - Originator to Dispatcher Information */
    {"AT+CPAS=?\r", "\r\n+CPAS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.1 - Phone activity status */
    {"AT+CPBF=?\r", "\r\n+CPBF:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.13 - Find phonebook entries */
    {"AT+CPBR=?\r", "\r\n+CPBR:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.12 - Read phonebook entries */
    {"AT+CPBS=?\r", "\r\n+CPBS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.11 - Select phonebook memory storage */
    {"AT+CPBW=?\r", "\r\n+CPBW:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.14 - Write phonebook entry */
    {"AT+CPIN=?\r", "\r\n+CPIN:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.3 - Enter PIN */
    {"AT+CPLS=?\r", "\r\n+CPLS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.20 - Selection of preferred PLMN list */
    {"AT+CPOL=?\r", "\r\n+CPOL:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.19 - Preferred PLMN list */
    {"AT+CPPS=?\r", "\r\n+CPPS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.23 - eMLPP subscriptions */
    {"AT+CPROT=?\r", "\r\n+CPROT:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 8.42 - Enter protocol mode */
    {"AT+CPUC=?\r", "\r\n+CPUC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.27 - Price per unit and currency table */
    {"AT+CPWC=?\r", "\r\n+CPWC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.29 - Power class */
    {"AT+CPWD=?\r", "\r\n+CPWD:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.5 - Change password */
    {"AT+CR=?\r", "\r\n+CR:\r\n\r\nOK\r\n"},                        /* 3GPP TS 27.007 6.9 - Service reporting control */
    {"AT+CRC=?\r", "\r\n+CRC:\r\n\r\nOK\r\n"},                      /* 3GPP TS 27.007 6.11 - Cellular result codes */
    {"AT+CREG=?\r", "\r\n+CREG:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.2 - Network registration */
    {"AT+CRLP=?\r", "\r\n+CRLP:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.8 - Radio link protocol */
    {"AT+CRMC=?\r", "\r\n+CRMC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.34 - Ring Melody Control */
    {"AT+CRMP=?\r", "\r\n+CRMP:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.35 - Ring Melody Playback */
    {"AT+CRSL=?\r", "\r\n+CRSL:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.21 - Ringer sound level */
    {"AT+CRSM=?\r", "\r\n+CRSM:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.18 - Restricted SIM access */
    {"AT+CSCC=?\r", "\r\n+CSCC:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.19 - Secure control command */
    {"AT+CSCS=?\r", "\r\n+CSCS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 5.5 - Select TE character set */
    {"AT+CSDF=?\r", "\r\n+CSDF:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.22 - Settings date format */
    {"AT+CSGT=?\r", "\r\n+CSGT:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.32 - Set Greeting Text */
    {"AT+CSIL=?\r", "\r\n+CSIL:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.23 - Silence Command */
    {"AT+CSIM=?\r", "\r\n+CSIM:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.17 - Generic SIM access */
    {"AT+CSNS=?\r", "\r\n+CSNS:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.19 - Single numbering scheme */
    {"AT+CSQ=?\r", "\r\n+CSQ:\r\n\r\nOK\r\n"},                      /* 3GPP TS 27.007 8.5 - Signal quality */
    {"AT+CSSN=?\r", "\r\n+CSSN:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.17 - Supplementary service notifications */
    {"AT+CSTA=?\r", "\r\n+CSTA:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.1 - Select type of address */
    {"AT+CSTF=?\r", "\r\n+CSTF:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.24 - Settings time format */
    {"AT+CSVM=?\r", "\r\n+CSVM:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.33 - Set Voice Mail Number */
    {"AT+CTFR=?\r", "\r\n+CTFR:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.14 - Call deflection */
    {"AT+CTZR=?\r", "\r\n+CTZR:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.41 - Time Zone Reporting */
    {"AT+CTZU=?\r", "\r\n+CTZU:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.40 - Automatic Time Zone Update */
    {"AT+CUSD=?\r", "\r\n+CUSD:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 7.15 - Unstructured supplementary service data */
    {"AT+CUUS1=?\r", "\r\n+CUUS1:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 7.26 - User to User Signalling Service 1 */
    {"AT+CV120=?\r", "\r\n+CV120:\r\n\r\nOK\r\n"},                  /* 3GPP TS 27.007 6.21 - V.120 rate adaption protocol */
    {"AT+CVHU=?\r", "\r\n+CVHU:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 6.20 - Voice Hangup Control */
    {"AT+CVIB=?\r", "\r\n+CVIB:\r\n\r\nOK\r\n"},                    /* 3GPP TS 27.007 8.22 - Vibrator mode */
    {"AT+DR=?\r", "\r\n+DR:\r\n\r\nOK\r\n"},                        /* V.250 6.6.2 - Data compression reporting */
    {"AT+DS=?\r", "\r\n+DS:\r\n\r\nOK\r\n"},                        /* V.250 6.6.1 - Data compression */
    {"AT+EB=?\r", "\r\n+EB:\r\n\r\nOK\r\n"},                        /* V.250 6.5.2 - Break handling in error control operation */
    {"AT+EFCS=?\r", "\r\n+EFCS:(0-2)\r\n\r\nOK\r\n"},               /* V.250 6.5.4 - 32-bit frame check sequence */
    {"AT+EFCS?\r", "\r\n+EFCS:0\r\n\r\nOK\r\n"},
    {"AT+EFRAM=?\r", "\r\n+EFRAM:(1-65535),(1-65535)\r\n\r\nOK\r\n"},
                                                                    /* V.250 6.5.8 - Frame length */
    {"AT+ER=?\r", "\r\n+ER:(0,1)\r\n\r\nOK\r\n"},                   /* V.250 6.5.5 - Error control reporting */
    {"AT+ES=?\r", "\r\n+ES:(0-7),(0-4),(0-9)\r\n\r\nOK\r\n"},       /* V.250 6.5.1 - Error control selection */
    {"AT+ES?\r", "\r\n+ES:0,0,0\r\n\r\nOK\r\n"},
    {"AT+ESA=?\r", "\r\n+ESA:(0-2),(0-1),(0-1),(0-1),(0-2),(0-1),(0-255),(0-255)\r\n\r\nOK\r\n"},
                                                                    /* V.80 8.2 - Synchronous access mode configuration */
    {"AT+ESA?\r", "\r\n+ESA:0,0,0,0,0,0,0,0\r\n\r\nOK\r\n"},
    {"AT+ESR\r", "\r\nOK\r\n"},                                     /* V.250 6.5.3 - Selective repeat */
    {"AT+ETBM=?\r", "\r\n+ETBM:(0-2),(0-2),(0-30)\r\n\r\nOK\r\n"},  /* T.31 8.5.1 - Adaptive reception control */
    {"AT+ETBM?\r", "\r\n+ETBM:0,0\r\n\r\nOK\r\n"},
    {"AT+EWIND=?\r", "\r\n+EWIND:(1-127),(1-127)\r\n\r\nOK\r\n"},   /* V.250 6.5.7 - Window size */
    {"AT+EWIND?\r", "\r\n+EWIND:0,0\r\n\r\nOK\r\n"},
    {"AT+F34=?\r",  "\r\n+F34:(0-14),(0-14),(0-2),(0-14),(0-14)\r\n\r\nOK\r\n"},
                                                                    /* T.31 B.6.1 - Initial V.34 rate controls for FAX */
    {"AT+F34?\r", "\r\n+F34:0,0,0,0,0\r\n\r\nOK\r\n"},
    {"AT+FAR=?\r", "\r\n0,1\r\n\r\nOK\r\n"},                        /* T.31 8.5.1 - Adaptive reception control */
    {"AT+FAR?\r", "\r\n0\r\n\r\nOK\r\n"},
    {"AT+FCL=?\r", "\r\n(0-255)\r\n\r\nOK\r\n"},                    /* T.31 8.5.2 - Carrier loss timeout */
    {"AT+FCLASS=?\r", "\r\n0,1,1.0\r\n\r\nOK\r\n"},                 /* T.31 8.2 - Capabilities identification and control */
    {"AT+FCLASS?\r", "\r\n1\r\n\r\nOK\r\n"},
    {"AT+FDD=?\r", "\r\n(0,1)\r\n\r\nOK\r\n"},                      /* T.31 8.5.3 - Double escape character replacement */
    {"AT+FDD?\r", "\r\n0\r\n\r\nOK\r\n"},
    {"AT+FIT=?\r", "\r\n+FIT:(0-255),(0-1)\r\n\r\nOK\r\n"},         /* T.31 8.5.4 - DTE inactivity timeout */
    {"AT+FIT?\r", "\r\n+FIT:0,0\r\n\r\nOK\r\n"},
    {"AT+FLO=?\r", "\r\n+FLO:(0-2)\r\n\r\nOK\r\n"},                 /* T.31 says to implement something similar to +IFC */
    {"AT+FLO?\r", "\r\n+FLO:2\r\n\r\nOK\r\n"},
    {"AT+FMI?\r", "\r\n" MANUFACTURER "\r\n\r\nOK\r\n"},            /* T.31 says to duplicate +GMI */
    {"AT+FMM?\r", "\r\n" PACKAGE "\r\n\r\nOK\r\n"},                 /* T.31 says to duplicate +GMM */
    {"AT+FMR?\r", "\r\n" VERSION "\r\n\r\nOK\r\n"},                 /* T.31 says to duplicate +GMR */
    {"AT+FPR=?\r", "\r\n115200\r\n\r\nOK\r\n"},                     /* T.31 says to implement something similar to +IPR */
    {"AT+FPR?\r", "\r\n0\r\n\r\nOK\r\n"},
    {"AT+FRH=?\r", "\r\n3\r\n\r\nOK\r\n"},                          /* T.31 8.3.6 - HDLC receive */
    {"AT+FRH?\r", "\r\n-1\r\n\r\nOK\r\n"},
    {"AT+FRM=?\r", "\r\n24,48,72,73,74,96,97,98,121,122,145,146\r\n\r\nOK\r\n"}, /* T.31 8.3.4 - Facsimile receive */
    {"AT+FRM?\r", "\r\n-1\r\n\r\nOK\r\n"},
    {"AT+FRS=?\r", "\r\n0-255\r\n\r\nOK\r\n"},                      /* T.31 8.3.2 - Receive silence */
    {"AT+FRS?\r", "\r\n-1\r\n\r\nOK\r\n"},
    {"AT+FTH=?\r", "\r\n3\r\n\r\nOK\r\n"},                          /* T.31 8.3.5 - HDLC transmit */
    {"AT+FTH?\r", "\r\n-1\r\n\r\nOK\r\n"},
    {"AT+FTM=?\r", "\r\n24,48,72,73,74,96,97,98,121,122,145,146\r\n\r\nOK\r\n"}, /* T.31 8.3.3 - Facsimile transmit */
    {"AT+FTM?\r", "\r\n-1\r\n\r\nOK\r\n"},
    {"AT+FTS=?\r", "\r\n0-255\r\n\r\nOK\r\n"},                      /* T.31 8.3.1 - Transmit silence */
    {"AT+FTS?\r", "\r\n-1\r\n\r\nOK\r\n"},
    {"AT+GCAP\r", "\r\nOK\r\n"},                                    /* V.250 6.1.9 - Request complete capabilities list */
    {"AT+GCI=?\r", "\r\n+GCI:(00-FF)\r\n\r\nOK\r\n"},               /* V.250 6.1.10 - Country of installation, */
    {"AT+GCI?\r", "\r\n+GCI:00\r\n\r\nOK\r\n"},
    {"AT+GMI?\r", "\r\n" MANUFACTURER "\r\n\r\nOK\r\n"},            /* V.250 6.1.4 - Request manufacturer identification */
    {"AT+GMM?\r", "\r\n" PACKAGE "\r\n\r\nOK\r\n"},                 /* V.250 6.1.5 - Request model identification */
    {"AT+GMR?\r", "\r\n" VERSION "\r\n\r\nOK\r\n"},                 /* V.250 6.1.6 - Request revision identification */
    {"AT+GOI\r", "\r\nOK\r\n"},                                     /* V.250 6.1.8 - Request global object identification */
    {"AT+GSN?\r", "\r\n42\r\n\r\nOK\r\n"},                          /* V.250 6.1.7 - Request product serial number identification */
    {"AT+IBC=?\r", "\r\n+IBC:(0-2),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0.1),(0,1)\r\n\r\nOK\r\n"},
                                                                    /* V.80 7.9 - Control of in-band control */
    {"AT+IBC?\r", "\r\n+IBC:0,0,0,0,0,0,0,0,0,0,0,0,0\r\n\r\nOK\r\n"},
    {"AT+IBM=?\r", "\r\n+IBM:(0-7),(0-255),(0-255)\r\n\r\nOK\r\n"}, /* V.80 7.10 - In-band MARK idle reporting control */
    {"AT+IBM?\r", "\r\n+IBM:0,0,0\r\n\r\nOK\r\n"},
    {"AT+ICF?\r", "\r\n+ICF:0,0\r\n\r\nOK\r\n"},                    /* V.250 6.2.11 - DTE-DCE character framing */
    {"AT+ICLOK?\r", "\r\n+ICLOK:0\r\n\r\nOK\r\n"},                  /* V.250 6.2.14 - Select sync transmit clock source */
    {"AT+IDSR?\r", "\r\n+IDSR:0\r\n\r\nOK\r\n"},                    /* V.250 6.2.16 - Select data set ready option */
    {"AT+IFC=?\r", "\r\n+IFC:(0-2),(0-2)\r\n\r\nOK\r\n"},           /* V.250 6.2.12 - DTE-DCE local flow control */
    {"AT+IFC?\r", "\r\n+IFC:2,2\r\n\r\nOK\r\n"},
    {"AT+ILRR\r", "\r\nOK\r\n"},                                    /* V.250 6.2.13 - DTE-DCE local rate reporting */
    {"AT+ILSD=?\r", "\r\n+ILSD:(0,1)\r\n\r\nOK\r\n"},               /* V.250 6.2.15 - Select long space disconnect option */
    {"AT+ILSD?\r", "\r\n+ILSD:0\r\n\r\nOK\r\n"},
    {"AT+IPR=?\r", "\r\n+IPR:(115200),(115200)\r\n\r\nOK\r\n"},     /* V.250 6.2.10 - Fixed DTE rate */
    {"AT+IPR?\r", "\r\n+IPR:0\r\n\r\nOK\r\n"},
    {"AT+IRTS=?\r", "\r\n+IRTS:(0,1)\r\n\r\nOK\r\n"},               /* V.250 6.2.17 - Select synchronous mode RTS option */
    {"AT+IRTS?\r", "\r\n+IRTS:0\r\n\r\nOK\r\n"},
    {"AT+MA\r", "\r\nOK\r\n"},                                      /* V.250 6.4.2 - Modulation automode control */
    {"AT+MR=?\r", "\r\n+MR:(0,1)\r\n\r\nOK\r\n"},                   /* V.250 6.4.3 - Modulation reporting control */
    {"AT+MR?\r", "\r\n+MR:0\r\n\r\nOK\r\n"},
    {"AT+MS\r", "\r\nOK\r\n"},                                      /* V.250 6.4.1 - Modulation selection */
    {"AT+MSC=?\r", "\r\n+MSC:(0,1)\r\n\r\nOK\r\n"},                 /* V.250 6.4.8 - Seamless rate change enable */
    {"AT+MSC?\r", "\r\n+MSC:0\r\n\r\nOK\r\n"},
    {"AT+MV18AM\r", "\r\nOK\r\n"},                                  /* V.250 6.4.6 - V.18 answering message editing */
    {"AT+MV18P=?\r", "\r\n+MV18P:(2-7)\r\n\r\nOK\r\n"},             /* V.250 6.4.7 - Order of probes */
    {"AT+MV18P?\r", "\r\n+MV18P:0\r\n\r\nOK\r\n"},
    {"AT+MV18R=?\r", "\r\n+MV18R:(0,1)\r\n\r\nOK\r\n"},             /* V.250 6.4.5 - V.18 reporting control */
    {"AT+MV18R?\r", "\r\n+MV18R:0\r\n\r\nOK\r\n"},
    {"AT+MV18S\r", "\r\nOK\r\n"},                                   /* V.250 6.4.4 - V.18 selection */
    {"AT+TADR\r", "\r\nOK\r\n"},                                    /* V.250 6.7.2.9 - Local V.54 address */
    {"AT+TAL=?\r", "\r\n+TAL:(0,1),(0,1)\r\n\r\nOK\r\n"},           /* V.250 6.7.2.15 - Local analogue loop */
    {"AT+TAL?\r", "\r\n+TAL:0,0\r\n\r\nOK\r\n"},
    {"AT+TALS=?\r", "\r\n+TALS:(0-3)\r\n\r\nOK\r\n"},               /* V.250 6.7.2.6 - Analogue loop status */
    {"AT+TALS?\r", "\r\n+TALS:0\r\n\r\nOK\r\n"},
    {"AT+TDLS=?\r", "\r\n+TDLS:(0-4)\r\n\r\nOK\r\n"},               /* V.250 6.7.2.7 - Local digital loop status */
    {"AT+TDLS?\r", "\r\n+TDLS:0\r\n\r\nOK\r\n"},
    {"AT+TE140=?\r", "\r\n+TE140:(0,1)\r\n\r\nOK\r\n"},             /* V.250 6.7.2.1 - Enable ckt 140 */
    {"AT+TE140?\r", "\r\n+TE140:0\r\n\r\nOK\r\n"},
    {"AT+TE141=?\r", "\r\n+TE141:(0,1)\r\n\r\nOK\r\n"},             /* V.250 6.7.2.2 - Enable ckt 141 */
    {"AT+TE141?\r", "\r\n+TE141:0\r\n\r\nOK\r\n"},
    {"AT+TEPAL=?\r", "\r\n+TEPAL:(0,1)\r\n\r\nOK\r\n"},             /* V.250 6.7.2.5 - Enable front panel analogue loop */
    {"AT+TEPAL?\r", "\r\n+TEPAL:0\r\n\r\nOK\r\n"},
    {"AT+TEPDL=?\r", "\r\n+TEPDL:(0,1)\r\n\r\nOK\r\n"},             /* V.250 6.7.2.4 - Enable front panel RDL */
    {"AT+TEPDL?\r", "\r\n+TEPDL:0\r\n\r\nOK\r\n"},
    {"AT+TERDL=?\r", "\r\n+TERDL:(0,1)\r\n\r\nOK\r\n"},             /* V.250 6.7.2.3 - Enable RDL from remote */
    {"AT+TERDL?\r", "\r\n+TERDL:0\r\n\r\nOK\r\n"},
    {"AT+TLDL=?\r", "\r\n+TLDL:(0,1)\r\n\r\nOK\r\n"},               /* V.250 6.7.2.13 - Local digital loop */
    {"AT+TLDL?\r", "\r\n+TLDL:0\r\n\r\nOK\r\n"},
    {"AT+TMODE=?\r", "\r\n+TMODE:(0,1)\r\n\r\nOK\r\n"},             /* V.250 6.7.2.10 - Set V.54 mode */
    {"AT+TMODE?\r", "\r\n+TMODE:0\r\n\r\nOK\r\n"},
    {"AT+TNUM\r", "\r\nOK\r\n"},                                    /* V.250 6.7.2.12 - Errored bit and block counts */
    {"AT+TRDL=?\r", "\r\n+TRDL:(0,1)\r\n\r\nOK\r\n"},               /* V.250 6.7.2.14 - Request remote digital loop */
    {"AT+TRDL?\r", "\r\n+TRDL:0\r\n\r\nOK\r\n"},
    {"AT+TRDLS\r", "\r\nOK\r\n"},                                   /* V.250 6.7.2.8 - Remote digital loop status */
    {"AT+TRES=?\r", "\r\n+TRES:(0-2)\r\n\r\nOK\r\n"},               /* V.250 6.7.2.17 - Self test result */
    {"AT+TRES?\r", "\r\n+TRES:0\r\n\r\nOK\r\n"},
    {"AT+TSELF=?\r", "\r\n+TSELF:(0,1)\r\n\r\nOK\r\n"},             /* V.250 6.7.2.16 - Self test */
    {"AT+TSELF?\r", "\r\n+TSELF:0\r\n\r\nOK\r\n"},
    {"AT+TTER=?\r", "\r\n+TTER:(0-65535),(0-65535)\r\n\r\nOK\r\n"}, /* V.250 6.7.2.11 - Test error rate */
    {"AT+TTER?\r", "\r\n+TTER:0,0\r\n\r\nOK\r\n"},
    {"AT+VBT\r", "\r\nOK\r\n"},                                     /* 3GPP TS 27.007 C.2.2 - Buffer threshold setting */
    {"AT+VCID=?\r", "\r\n0,1\r\n\r\nOK\r\n"},                       /* 3GPP TS 27.007 C.2.3 - Calling number ID presentation */
    {"AT+VCID?\r", "\r\n0\r\n\r\nOK\r\n"},
    {"AT+VDR\r", "\r\nOK\r\n"},                                     /* V.253 10.3.1 - Distinctive ring (ring cadence reporting) */
    {"AT+VDT\r", "\r\nOK\r\n"},                                     /* V.253 10.3.2 - Control tone cadence reporting */
    {"AT+VDX\r", "\r\nOK\r\n"},                                     /* V.253 10.5.6 - Speakerphone duplex mode */
    {"AT+VEM\r", "\r\nOK\r\n"},                                     /* V.253 10.5.7 - Deliver event reports */
    {"AT+VGM\r", "\r\nOK\r\n"},                                     /* V.253 10.5.2 - Microphone gain */
    {"AT+VGR\r", "\r\nOK\r\n"},                                     /* V.253 10.2.1 - Receive gain selection */
    {"AT+VGS\r", "\r\nOK\r\n"},                                     /* V.253 10.5.3 - Speaker gain */
    {"AT+VGT\r", "\r\nOK\r\n"},                                     /* V.253 10.2.2 - Volume selection */
    {"AT+VIP\r", "\r\nOK\r\n"},                                     /* V.253 10.1.1 - Initialize voice parameters */
    {"AT+VIT\r", "\r\nOK\r\n"},                                     /* V.253 10.2.3 - DTE/DCE inactivity timer */
    {"AT+VLS\r", "\r\nOK\r\n"},                                     /* V.253 10.2.4 - Analogue source/destination selection */
    {"AT+VPP\r", "\r\nOK\r\n"},                                     /* V.253 10.4.2 - Voice packet protocol */
    {"AT+VRA\r", "\r\nOK\r\n"},                                     /* V.253 10.2.5 - Ringing tone goes away timer */
    {"AT+VRID?\r", "\r\n0\r\n\r\nOK\r\n"},                          /* Extension - Find the originating and destination numbers */
    {"AT+VRL\r", "\r\nOK\r\n"},                                     /* V.253 10.1.2 - Ring local phone */
    {"AT+VRN\r", "\r\nOK\r\n"},                                     /* V.253 10.2.6 - Ringing tone never appeared timer */
    {"AT+VRX\r", "\r\nOK\r\n"},                                     /* V.253 10.1.3 - Voice receive state */
    {"AT+VSD\r", "\r\nOK\r\n"},                                     /* V.253 10.2.7 - Silence detection (QUIET and SILENCE) */
    {"AT+VSID=12345\r", "\r\nOK\r\n"},                              /* Extension - Set the originating number */
    {"AT+VSID?\r", "\r\n12345\r\n\r\nOK\r\n"},
    {"AT+VSM\r", "\r\nOK\r\n"},                                     /* V.253 10.2.8 - Compression method selection */
    {"AT+VSP\r", "\r\nOK\r\n"},                                     /* V.253 10.5.1 - Voice speakerphone state */
    {"AT+VTA\r", "\r\nOK\r\n"},                                     /* V.253 10.5.4 - Train acoustic echo-canceller */
    {"AT+VTD\r", "\r\nOK\r\n"},                                     /* V.253 10.2.9 - Beep tone duration timer */
    {"AT+VTH\r", "\r\nOK\r\n"},                                     /* V.253 10.5.5 - Train line echo-canceller */
    {"AT+VTR\r", "\r\nOK\r\n"},                                     /* V.253 10.1.4 - Voice duplex state */
    {"AT+VTS\r", "\r\nOK\r\n"},                                     /* V.253 10.1.5 - DTMF and tone generation in voice */
    {"AT+VTX\r", "\r\nOK\r\n"},                                     /* V.253 10.1.6 - Transmit data state */
    {"AT+WS46\r", "\r\nOK\r\n"},                                    /* 3GPP TS 27.007 5.9 - PCCA STD-101 [17] select wireless network */
    {"ATA\r", "\r\nERROR\r\n"},                                     /* V.250 6.3.5 - Answer */
    {"ATDT -1234567890ABCDPSTW*#+,!@\r;", ""},                      /* V.250 6.3.1 - Dial */
    {"ATE1\r", "\r\nOK\r\n"},                                       /* V.250 6.2.4 - Command echo */
    {"ATE0\r", "ATE0\r\r\nOK\r\n"},                                 /* V.250 6.2.4 - Command echo */
    {"ATH\r", "\r\nOK\r\n"},                                        /* V.250 6.3.6 - Hook control */
    {"ATI\r", "\r\n" PACKAGE "\r\n\r\nOK\r\n"},                     /* V.250 6.1.3 - Request identification information */
    {"ATL\r", "\r\nOK\r\n"},                                        /* V.250 6.3.13 - Monitor speaker loudness */
    {"ATM\r", "\r\nOK\r\n"},                                        /* V.250 6.3.14 - Monitor speaker mode */
    {"ATO\r", "\r\nCONNECT\r\n\r\nOK\r\n"},                         /* V.250 6.3.7 - Return to online data state */
    {"ATP\r", "\r\nOK\r\n"},                                        /* V.250 6.3.3 - Select pulse dialling (command) */
    {"ATQ\r", "\r\nOK\r\n"},                                        /* V.250 6.2.5 - Result code suppression */
    {"ATS0=?\r", "\r\n000\r\n\r\nOK\r\n"},                          /* V.250 6.3.8 - Automatic answer */
    {"ATS0?\r", "\r\n000\r\n\r\nOK\r\n"},
    {"ATS10=?\r", "\r\n000\r\n\r\nOK\r\n"},                         /* V.250 6.3.12 - Automatic disconnect delay */
    {"ATS10?\r", "\r\n000\r\n\r\nOK\r\n"},
    {"ATS3=?\r", "\r\n000\r\n\r\nOK\r\n"},                          /* V.250 6.2.1 - Command line termination character */
    {"ATS3?\r", "\r\n013\r\n\r\nOK\r\n"},
    {"ATS4=?\r", "\r\n000\r\n\r\nOK\r\n"},                          /* V.250 6.2.2 - Response formatting character */
    {"ATS4?\r", "\r\n010\r\n\r\nOK\r\n"},
    {"ATS5=?\r", "\r\n000\r\n\r\nOK\r\n"},                          /* V.250 6.2.3 - Command line editing character */
    {"ATS5?\r", "\r\n008\r\n\r\nOK\r\n"},
    {"ATS6=?\r", "\r\n000\r\n\r\nOK\r\n"},                          /* V.250 6.3.9 - Pause before blind dialling */
    {"ATS6?\r", "\r\n001\r\n\r\nOK\r\n"},
    {"ATS7=?\r", "\r\n000\r\n\r\nOK\r\n"},                          /* V.250 6.3.10 - Connection completion timeout */
    {"ATS7?\r", "\r\n060\r\n\r\nOK\r\n"},
    {"ATS8=?\r", "\r\n000\r\n\r\nOK\r\n"},                          /* V.250 6.3.11 - Comma dial modifier time */
    {"ATS8?\r", "\r\n005\r\n\r\nOK\r\n"},
    {"ATT\r", "\r\nOK\r\n"},                                        /* V.250 6.3.2 - Select tone dialling (command) */
    {"ATV0\r", "0\r"},                                              /* V.250 6.2.6 - DCE response format */
    {"ATV1\r", "\r\nOK\r\n"},
    {"ATX4\r", "\r\nOK\r\n"},                                       /* V.250 6.2.7 - Result code selection and call progress monitoring control */
    {"ATZ\r", "\r\nOK\r\n"},                                        /* V.250 6.1.1 - Reset to default configuration */
    {"", ""}
};

char *decode_test_file = NULL;
int countdown = 0;
int command_response_test_step = -1;
char response_buf[1000];
int response_buf_ptr = 0;

static int at_rx(at_state_t *s, const char *t, int len)
{
    at_interpreter(s, t, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int at_send(at_state_t *s, const char *t)
{
    printf("%s", t);
    at_rx(s, t, strlen(t));
    return 0;
}
/*- End of function --------------------------------------------------------*/

#if 0
static int at_send_at(at_state_t *s, const char *t)
{
    uint8_t buf[500];

    sprintf((char *) buf, "AT%s\r", t);
    printf("%s", t);
    at_rx(s, t, strlen(t));
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int at_expect(at_state_t *s, const char *t)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int at_send_hdlc(at_state_t *s, uint8_t *t, int len)
{
    uint8_t buf[100];
    int i;
    int j;

    for (i = 0, j = 0;  i < len;  i++)
    {
        if (*t == DLE)
            buf[j++] = DLE;
        buf[j++] = *t++;
    }
    buf[j++] = DLE;
    buf[j++] = ETX;
    at_rx(s, (char *) buf, j);
    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

static int general_test(at_state_t *s)
{
    int i;

    for (i = 0;  general_test_seq[i].command[0];  i++)
    {
        response_buf_ptr = 0;
        response_buf[0] = '\0';
        command_response_test_step = i;
        at_send(s, general_test_seq[i].command);
        if (strcmp(general_test_seq[command_response_test_step].response, response_buf) != 0)
        {
            printf("Incorrect response\n");
            printf("Expected: '%s'\n", general_test_seq[command_response_test_step].response);
            printf("Received: '%s'\n", response_buf);
            return -1;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int modem_call_control(void *user_data, int op, const char *num)
{
    switch (op)
    {
    case AT_MODEM_CONTROL_CALL:
        printf("\nModem control - Dialing '%s'\n", num);
        break;
    case AT_MODEM_CONTROL_ANSWER:
        printf("\nModem control - Answering\n");
        /* Force an error response, so we get something well defined for the test. */
        return -1;
    case AT_MODEM_CONTROL_HANGUP:
        printf("\nModem control - Hanging up\n");
        break;
    case AT_MODEM_CONTROL_OFFHOOK:
        printf("\nModem control - Going off hook\n");
        break;
    case AT_MODEM_CONTROL_ONHOOK:
        printf("\nModem control - Going on hook\n");
        break;
    case AT_MODEM_CONTROL_DTR:
        printf("\nModem control - DTR %d\n", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_RTS:
        printf("\nModem control - RTS %d\n", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_CTS:
        printf("\nModem control - CTS %d\n", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_CAR:
        printf("\nModem control - CAR %d\n", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_RNG:
        printf("\nModem control - RNG %d\n", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_DSR:
        printf("\nModem control - DSR %d\n", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_SETID:
        printf("\nModem control - Set ID '%s'\n", num);
        break;
    case AT_MODEM_CONTROL_RESTART:
        printf("\nModem control - Restart %d\n", (int) (intptr_t) num);
        break;
    case AT_MODEM_CONTROL_DTE_TIMEOUT:
        printf("\nModem control - Set DTE timeout %d\n", (int) (intptr_t) num);
        break;
    default:
        printf("\nModem control - operation %d\n", op);
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int at_tx_handler(void *user_data, const uint8_t *buf, size_t len)
{
    int i;

    for (i = 0;  i < len;  i++)
    {
        response_buf[response_buf_ptr++] = buf[i];
        putchar(buf[i]);
    }
    response_buf[response_buf_ptr] = '\0';

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    at_state_t *at_state;

    if ((at_state = at_init(NULL, at_tx_handler, NULL, modem_call_control, NULL)) == NULL)
    {
        fprintf(stderr, "Cannot start the AT interpreter\n");
        exit(2);
    }
    if (general_test(at_state))
    {
        printf("Tests failed.\n");
        exit(2);
    }
    printf("Tests passed.\n");
    at_free(at_state);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
