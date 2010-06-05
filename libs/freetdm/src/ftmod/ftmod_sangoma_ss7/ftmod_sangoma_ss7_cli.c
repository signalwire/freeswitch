/*
 * Copyright (c) 2009|Konrad Hammel <konrad@sangoma.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms|with or without
 * modification|are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice|this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice|this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES|INCLUDING|BUT NOT
 * LIMITED TO|THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT|INDIRECT|INCIDENTAL|SPECIAL,
 * EXEMPLARY|OR CONSEQUENTIAL DAMAGES (INCLUDING|BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE|DATA|OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY|WHETHER IN CONTRACT|STRICT LIABILITY|OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE|EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
ftdm_status_t ftdm_sngss7_handle_cli_cmd(ftdm_stream_handle_t *stream, const char *data);

static ftdm_status_t handle_print_usuage(ftdm_stream_handle_t *stream);

static ftdm_status_t handle_set_function_trace(ftdm_stream_handle_t *stream, int on, int level);
static ftdm_status_t handle_set_message_trace(ftdm_stream_handle_t *stream, int on, int level);
static ftdm_status_t handle_set_blocks(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_set_unblks(ftdm_stream_handle_t *stream, int span, int chan, int verbose);

static ftdm_status_t handle_show_free(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_inuse(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_inreset(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_flags(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_blocks(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_status(ftdm_stream_handle_t *stream, int span, int chan, int verbose);

static ftdm_status_t handle_status_link(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_status_linkset(ftdm_stream_handle_t *stream, char *name);

static ftdm_status_t extract_span_chan(char *argv[10], int pos, int *span, int *chan);
static ftdm_status_t check_arg_count(int args, int min);
/******************************************************************************/

/* FUNCTIONS ******************************************************************/
ftdm_status_t ftdm_sngss7_handle_cli_cmd(ftdm_stream_handle_t *stream, const char *data)
{
    char    *mycmd = NULL;
    char    *argv[10] = { 0 };
    int     argc = 0;
    int     span = 0;
    int     chan = 0;
    int     trace = 0;
    int     trace_level = 7;
    int     verbose = 1;
    int     c = 0;

    if (data) {
        mycmd = ftdm_strdup(data);
        argc = ftdm_separate_string(mycmd,' ',argv,(sizeof(argv) / sizeof(argv[0])));
    }

    if (check_arg_count(argc, 1)) goto handle_cli_error_argc;
    
    if (!strcasecmp(argv[c], "show")) {
    /**************************************************************************/   
        if (check_arg_count(argc, 4)) goto handle_cli_error_argc;
        c++;

        if (!strcasecmp(argv[c], "status")) {
        /**********************************************************************/
            c++;

            if (!strcasecmp(argv[c], "link")) {
            /******************************************************************/
                c++;
                handle_status_link(stream, argv[c]);
            /******************************************************************/
            } else if (!strcasecmp(argv[c], "linkset")) {
            /******************************************************************/
                c++;
                handle_status_linkset(stream, argv[c]);
            /******************************************************************/
            } else if (!strcasecmp(argv[c], "span")) {
            /******************************************************************/
                if (check_arg_count(argc, 6)) goto handle_cli_error_argc;

                if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

                handle_show_status(stream, span, chan, verbose);
            /******************************************************************/
            } else {
            /******************************************************************/
                stream->write_function(stream, "Unknown \"status\" command\n");
                goto handle_cli_error;
            /******************************************************************/
            }
        /**********************************************************************/
        } else if (!strcasecmp(argv[c], "inuse")) {
        /**********************************************************************/
            if (check_arg_count(argc, 6)) goto handle_cli_error_argc;
            c++;

            if (!strcasecmp(argv[c], "span")) {
            /******************************************************************/
                if (check_arg_count(argc, 6)) goto handle_cli_error_argc;

                if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

                handle_show_inuse(stream, span, chan, verbose);
            /******************************************************************/
            } else {
            /******************************************************************/
                stream->write_function(stream, "Unknown \"inuse\" command\n");
                goto handle_cli_error;
            /******************************************************************/
            }           
        /**********************************************************************/
        } else if (!strcasecmp(argv[c], "inreset")) {
        /**********************************************************************/
            if (check_arg_count(argc, 6)) goto handle_cli_error_argc;
            c++;

            if (!strcasecmp(argv[c], "span")) {
            /******************************************************************/
                if (check_arg_count(argc, 6)) goto handle_cli_error_argc;

                if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

                handle_show_inreset(stream, span, chan, verbose);
            /******************************************************************/
            } else {
            /******************************************************************/
                stream->write_function(stream, "Unknown \"inreset\" command\n");
                goto handle_cli_error;
            /******************************************************************/
            }   
        /**********************************************************************/
        } else if (!strcasecmp(argv[c], "free")) {
        /**********************************************************************/
            if (check_arg_count(argc, 6)) goto handle_cli_error_argc;
            c++;

            if (!strcasecmp(argv[c], "span")) {
            /******************************************************************/
                if (check_arg_count(argc, 6)) goto handle_cli_error_argc;

                if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

                handle_show_free(stream, span, chan, verbose);
            /******************************************************************/
            } else {
            /******************************************************************/
                stream->write_function(stream, "Unknown \"free\" command\n");
                goto handle_cli_error;
            /******************************************************************/
            }   
        /**********************************************************************/
        } else if (!strcasecmp(argv[c], "blocks")) {
        /**********************************************************************/
            if (check_arg_count(argc, 6)) goto handle_cli_error_argc;
            c++;

            if (!strcasecmp(argv[c], "span")) {
            /******************************************************************/
                if (check_arg_count(argc, 6)) goto handle_cli_error_argc;

                if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

                handle_show_blocks(stream, span, chan, verbose);
            /******************************************************************/
            } else {
            /******************************************************************/
                stream->write_function(stream, "Unknown \"blocks\" command\n");
                goto handle_cli_error;
            /******************************************************************/
            }   
        /**********************************************************************/
        } else if (!strcasecmp(argv[c], "flags")) {
        /**********************************************************************/
            if (check_arg_count(argc, 6)) goto handle_cli_error_argc;
            c++;

            if (!strcasecmp(argv[c], "span")) {
            /******************************************************************/
                if (check_arg_count(argc, 6)) goto handle_cli_error_argc;

                if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

                handle_show_flags(stream, span, chan, verbose);
            /******************************************************************/
            } else {
            /******************************************************************/
                stream->write_function(stream, "Unknown \"flags\" command\n");
                goto handle_cli_error;
            /******************************************************************/
            }   
        /**********************************************************************/
        } else {
        /**********************************************************************/
            stream->write_function(stream, "Unknown \"show\" command\n");
            goto handle_cli_error;
        /**********************************************************************/
        }
    /**************************************************************************/
    } else if (!strcasecmp(argv[c], "set")) {
    /**************************************************************************/
        if (check_arg_count(argc, 4)) goto handle_cli_error_argc;
        c++;

        if (!strcasecmp(argv[c], "ftrace")) {
        /**********************************************************************/
            c++;
            trace = atoi(argv[c]);
            c++;
            trace_level = atoi(argv[c]);
            c++;
            handle_set_function_trace(stream, trace, trace_level);
        /**********************************************************************/
        } else if (!strcasecmp(argv[c], "mtrace")) {
        /**********************************************************************/
            c++;
            trace = atoi(argv[c]);
            c++;
            trace_level = atoi(argv[c]);
            c++;
            handle_set_message_trace(stream, trace, trace_level);
        /**********************************************************************/
        } else {
        /**********************************************************************/
            stream->write_function(stream, "Unknown \"set\" command\n");
            goto handle_cli_error;
        /**********************************************************************/
        }  
    /**************************************************************************/
    } else if (!strcasecmp(argv[c], "block")) {
    /**************************************************************************/
        if (check_arg_count(argc, 6)) goto handle_cli_error_argc;
        c++;

        if (!strcasecmp(argv[c], "span")) {
        /**********************************************************************/
            if (check_arg_count(argc, 6)) goto handle_cli_error_argc;

            if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

            handle_set_blocks(stream, span, chan, verbose);
        /**********************************************************************/
        } else {
        /**********************************************************************/
            stream->write_function(stream, "Unknown \"block\" command\n");
            goto handle_cli_error;
        /**********************************************************************/
        }   
    /**************************************************************************/
    } else if (!strcasecmp(argv[c], "unblock")) {
    /**************************************************************************/
        if (check_arg_count(argc, 6)) goto handle_cli_error_argc;
        c++;

        if (!strcasecmp(argv[c], "span")) {
        /**********************************************************************/
            if (check_arg_count(argc, 6)) goto handle_cli_error_argc;

            if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

            handle_set_unblks(stream, span, chan, verbose);
        /**********************************************************************/
        } else {
        /**********************************************************************/
            stream->write_function(stream, "Unknown \"unblock\" command\n");
            goto handle_cli_error;
        /**********************************************************************/
        } 
    /**************************************************************************/
    } else {
    /**************************************************************************/
        goto handle_cli_error;
    /**************************************************************************/
    }

    return FTDM_SUCCESS;

handle_cli_error_argc:
    stream->write_function(stream, "Invalid # of arguments in command\n");
    handle_print_usuage(stream);
    return FTDM_SUCCESS;

handle_cli_error_span_chan:
    stream->write_function(stream, "Unknown \"span\\chan\" command\n");
    handle_print_usuage(stream);
    return FTDM_SUCCESS;

handle_cli_error:
    stream->write_function(stream, "Unknown command requested\n");
    handle_print_usuage(stream);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_print_usuage(ftdm_stream_handle_t *stream)
{
    stream->write_function(stream, "Sangoma SS7 CLI usuage:\n\n");

    stream->write_function(stream, "Ftmod_sangoma_ss7 general control:\n");
    stream->write_function(stream, "ftdm ss7 set ftace X Y\n");
    stream->write_function(stream, "ftdm ss7 set mtace X Y\n");
    stream->write_function(stream, "\n");
    stream->write_function(stream, "Ftmod_sangoma_ss7 information:\n");
    stream->write_function(stream, "ftdm ss7 show status link X\n");
    stream->write_function(stream, "ftdm ss7 show status linkset X\n");
    stream->write_function(stream, "ftdm ss7 show status span X chan Y\n");
    stream->write_function(stream, "ftdm ss7 show free span X chan Y\n");
    stream->write_function(stream, "ftdm ss7 show inuse span X chan Y\n");
    stream->write_function(stream, "ftdm ss7 show inreset span X chan Y\n");
    stream->write_function(stream, "\n");
    stream->write_function(stream, "Ftmod_sangoma_ss7 circuit control:\n");
    stream->write_function(stream, "ftdm ss7 set block span X chan Y\n");
    stream->write_function(stream, "ftdm ss7 set unblk span X chan Y\n");
    stream->write_function(stream, "\n");

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_set_function_trace(ftdm_stream_handle_t *stream, int on, int level)
{
    stream->write_function(stream, "ftmod_sangoma_ss7 Function Trace was %s, level = %d\n",
                            (g_ftdm_sngss7_data.function_trace == 1) ? "ON" : "OFF",
                            g_ftdm_sngss7_data.function_trace_level);

    g_ftdm_sngss7_data.function_trace = on;
    g_ftdm_sngss7_data.function_trace_level = level;

    stream->write_function(stream, "ftmod_sangoma_ss7 Function Trace now is %s, level = %d\n",
                            (g_ftdm_sngss7_data.function_trace == 1) ? "ON" : "OFF",
                            g_ftdm_sngss7_data.function_trace_level);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_set_message_trace(ftdm_stream_handle_t *stream, int on, int level)
{
    stream->write_function(stream, "ftmod_sangoma_ss7 Message Trace was %s, level = %d\n",
                            (g_ftdm_sngss7_data.message_trace == 1) ? "ON" : "OFF",
                            g_ftdm_sngss7_data.message_trace_level);

    g_ftdm_sngss7_data.message_trace = on;
    g_ftdm_sngss7_data.message_trace_level = level;

    stream->write_function(stream, "ftmod_sangoma_ss7 Message Trace now is %s, level = %d\n",
                            (g_ftdm_sngss7_data.message_trace == 1) ? "ON" : "OFF",
                            g_ftdm_sngss7_data.message_trace_level);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_free(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
    int                 x;
    int                 free;
    sngss7_chan_data_t  *ss7_info;
    ftdm_channel_t      *ftdmchan;
    int                 lspan;
    int                 lchan;

    x=1;
    free = 0;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink != 1) {
            ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCircuit[x].obj;
            ftdmchan = ss7_info->ftdmchan;

            /* if span == 0 then all spans should be printed */
            if (span == 0) {
                lspan = ftdmchan->physical_span_id;
            } else {
                lspan = span;
            }

            /* if chan == 0 then all chans should be printed */
            if (chan == 0) {
                lchan = ftdmchan->physical_chan_id;
            } else {
                lchan = chan;
            }

            if ((ftdmchan->physical_span_id == lspan) && (ftdmchan->physical_chan_id == lchan)) {
                switch (ftdmchan->state) {
                /******************************************************************/
                case (FTDM_CHANNEL_STATE_DOWN):
                    if (verbose) {
                        stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|state=%s\n",
                                    ftdmchan->physical_span_id,
                                    ftdmchan->physical_chan_id,
                                    ss7_info->circuit->cic,
                                    ftdm_channel_state2str(ftdmchan->state));
                    } /* if (verbose) */

                    /*increment the count of circuits in reset */
                    free++;
                    break;
                /******************************************************************/
                default:
                    break;
                /******************************************************************/
                } /* switch (ftdmchan->state) */
            } /* if ( span and chan) */
        } /* if ( cic != 0) */

        /* go the next circuit */
        x++;
    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x]id != 0) */

    stream->write_function(stream, "\nTotal # of CICs free = %d\n",free);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_inuse(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
    int                 x;
    int                 in_use;
    sngss7_chan_data_t  *ss7_info;
    ftdm_channel_t      *ftdmchan;
    int                 lspan;
    int                 lchan;

    x=1;
    in_use = 0;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink != 1) {
            ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCircuit[x].obj;
            ftdmchan = ss7_info->ftdmchan;

            /* if span == 0 then all spans should be printed */
            if (span == 0) {
                lspan = ftdmchan->physical_span_id;
            } else {
                lspan = span;
            }

            /* if chan == 0 then all chans should be printed */
            if (chan == 0) {
                lchan = ftdmchan->physical_chan_id;
            } else {
                lchan = chan;
            }

            if ((ftdmchan->physical_span_id == lspan) && (ftdmchan->physical_chan_id == lchan)) {
                switch (ftdmchan->state) {
                /******************************************************************/
                case (FTDM_CHANNEL_STATE_COLLECT):
                case (FTDM_CHANNEL_STATE_RING):
                case (FTDM_CHANNEL_STATE_DIALING):
                case (FTDM_CHANNEL_STATE_PROGRESS):
                case (FTDM_CHANNEL_STATE_PROGRESS_MEDIA):
                case (FTDM_CHANNEL_STATE_UP):
                case (FTDM_CHANNEL_STATE_TERMINATING):
                case (FTDM_CHANNEL_STATE_HANGUP):
                    if (verbose) {
                        stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|state=%s\n",
                                    ftdmchan->physical_span_id,
                                    ftdmchan->physical_chan_id,
                                    ss7_info->circuit->cic,
                                    ftdm_channel_state2str(ftdmchan->state));
                    } /* if (verbose) */

                    /*increment the count of circuits in reset */
                    in_use++;
                    break;
                /******************************************************************/
                default:
                    break;
                /******************************************************************/
                } /* switch (ftdmchan->state) */
            } /* if ( span and chan) */
        } /* if ( cic != 0) */

        /* go the next circuit */
        x++;
    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x]id != 0) */

    stream->write_function(stream, "\nTotal # of CICs in use = %d\n",in_use);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_inreset(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
    int                 x;
    int                 in_reset;
    sngss7_chan_data_t  *ss7_info;
    ftdm_channel_t      *ftdmchan;
    int                 lspan;
    int                 lchan;

    x=1;
    in_reset = 0;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink != 1) {
            ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCircuit[x].obj;
            ftdmchan = ss7_info->ftdmchan;

            /* if span == 0 then all spans should be printed */
            if (span == 0) {
                lspan = ftdmchan->physical_span_id;
            } else {
                lspan = span;
            }

            /* if chan == 0 then all chans should be printed */
            if (chan == 0) {
                lchan = ftdmchan->physical_chan_id;
            } else {
                lchan = chan;
            }

            if ((ftdmchan->physical_span_id == lspan) && (ftdmchan->physical_chan_id == lchan)) {
                if ((sngss7_test_flag(ss7_info, FLAG_RESET_RX)) || (sngss7_test_flag(ss7_info, FLAG_RESET_TX))) {
                    if (verbose) {
                        stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|in_reset=Y\n",
                                    ftdmchan->physical_span_id,
                                    ftdmchan->physical_chan_id,
                                    ss7_info->circuit->cic);
                    } /* if (verbose) */
        
                    /*increment the count of circuits in reset */
                    in_reset++;
                } /* if ((sngss7_test_flag(ss7_info, FLAG_RESET_RX) ... */
            } /* if ( span and chan) */
        } /* if ( cic != 0) */

        /* go the next circuit */
        x++;
    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x]id != 0) */

    stream->write_function(stream, "\nTotal # of CICs in reset = %d\n",in_reset);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_flags(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
    int                 x;
    int                 bit;
    sngss7_chan_data_t  *ss7_info;
    ftdm_channel_t      *ftdmchan;
    int                 lspan;
    int                 lchan;

    x=1;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink != 1) {
            ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCircuit[x].obj;
            ftdmchan = ss7_info->ftdmchan;

            /* if span == 0 then all spans should be printed */
            if (span == 0) {
                lspan = ftdmchan->physical_span_id;
            } else {
                lspan = span;
            }

            /* if chan == 0 then all chans should be printed */
            if (chan == 0) {
                lchan = ftdmchan->physical_chan_id;
            } else {
                lchan = chan;
            }

            if ((ftdmchan->physical_span_id == lspan) && (ftdmchan->physical_chan_id == lchan)) {
                stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d",
                            ftdmchan->physical_span_id,
                            ftdmchan->physical_chan_id,
                            ss7_info->circuit->cic);
    
                for (bit = 0; bit < 33; bit++) {
                    stream->write_function(stream, "|");
                    if (ss7_info->flags & ( 0x1 << bit)) {
                        stream->write_function(stream, "%2d=1", bit);
                    } else {
                        stream->write_function(stream, "%2d=0", bit);
                    }
                }

                stream->write_function(stream, "\n");
            } /* if ( span and chan) */

        } /* if ( cic != 0) */

        /* go the next circuit */
        x++;
    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x]id != 0) */

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_blocks(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
    int                 x;
    sngss7_chan_data_t  *ss7_info;
    ftdm_channel_t      *ftdmchan;
    int                 lspan;
    int                 lchan;

    x=1;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink != 1) {
            ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCircuit[x].obj;
            ftdmchan = ss7_info->ftdmchan;

            /* if span == 0 then all spans should be printed */
            if (span == 0) {
                lspan = ftdmchan->physical_span_id;
            } else {
                lspan = span;
            }

            /* if chan == 0 then all chans should be printed */
            if (chan == 0) {
                lchan = ftdmchan->physical_chan_id;
            } else {
                lchan = chan;
            }

            if ((ftdmchan->physical_span_id == lspan) && (ftdmchan->physical_chan_id == lchan)) {
                stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|",
                            ftdmchan->physical_span_id,
                            ftdmchan->physical_chan_id,
                            ss7_info->circuit->cic);

                if((sngss7_test_flag(ss7_info, FLAG_CKT_MN_BLOCK_TX)) || (sngss7_test_flag(ss7_info, FLAG_GRP_MN_BLOCK_TX))) {
                    stream->write_function(stream, "l_mn=Y|");
                }else {
                    stream->write_function(stream, "l_mn=N|");
                }

                if((sngss7_test_flag(ss7_info, FLAG_CKT_MN_BLOCK_RX)) || (sngss7_test_flag(ss7_info, FLAG_GRP_MN_BLOCK_RX))) {
                    stream->write_function(stream, "r_mn=Y|");
                }else {
                    stream->write_function(stream, "r_mn=N|");
                }

                if(sngss7_test_flag(ss7_info, FLAG_GRP_HW_BLOCK_TX)) {
                    stream->write_function(stream, "l_hw=Y|");
                }else {
                    stream->write_function(stream, "l_hw=N|");
                }

                if(sngss7_test_flag(ss7_info, FLAG_GRP_HW_BLOCK_RX)) {
                    stream->write_function(stream, "r_hw=Y\n");
                }else {
                    stream->write_function(stream, "r_hw=N\n");
                }
            } /* if ( span and chan) */

        } /* if ( cic != 0) */

        /* go the next circuit */
        x++;
    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x]id != 0) */

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_status(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
    int                 x;
    sngss7_chan_data_t  *ss7_info;
    ftdm_channel_t      *ftdmchan;
    int                 lspan;
    int                 lchan;
    ftdm_signaling_status_t sigstatus = FTDM_SIG_STATE_DOWN;

    x=1;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink != 1) {
            ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCircuit[x].obj;
            ftdmchan = ss7_info->ftdmchan;

            /* if span == 0 then all spans should be printed */
            if (span == 0) {
                lspan = ftdmchan->physical_span_id;
            } else {
                lspan = span;
            }

            /* if chan == 0 then all chans should be printed */
            if (chan == 0) {
                lchan = ftdmchan->physical_chan_id;
            } else {
                lchan = chan;
            }

            if ((ftdmchan->physical_span_id == lspan) && (ftdmchan->physical_chan_id == lchan)) {
                /* grab the signaling_status */
                ftdm_channel_get_sig_status(ftdmchan, &sigstatus);

                stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|sig_status=%s|state=%s|",
                            ftdmchan->physical_span_id,
                            ftdmchan->physical_chan_id,
                            ss7_info->circuit->cic,
                            ftdm_signaling_status2str(sigstatus),
                            ftdm_channel_state2str(ftdmchan->state));

                if((sngss7_test_flag(ss7_info, FLAG_CKT_MN_BLOCK_TX)) || (sngss7_test_flag(ss7_info, FLAG_GRP_MN_BLOCK_TX))) {
                    stream->write_function(stream, "l_mn=Y|");
                }else {
                    stream->write_function(stream, "l_mn=N|");
                }

                if((sngss7_test_flag(ss7_info, FLAG_CKT_MN_BLOCK_RX)) || (sngss7_test_flag(ss7_info, FLAG_GRP_MN_BLOCK_RX))) {
                    stream->write_function(stream, "r_mn=Y|");
                }else {
                    stream->write_function(stream, "r_mn=N|");
                }

                if(sngss7_test_flag(ss7_info, FLAG_GRP_HW_BLOCK_TX)) {
                    stream->write_function(stream, "l_hw=Y|");
                }else {
                    stream->write_function(stream, "l_hw=N|");
                }

                if(sngss7_test_flag(ss7_info, FLAG_GRP_HW_BLOCK_RX)) {
                    stream->write_function(stream, "r_hw=Y\n");
                }else {
                    stream->write_function(stream, "r_hw=N\n");
                }
            } /* if ( span and chan) */

        } /* if ( cic != 0) */

        /* go the next circuit */
        x++;
    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x]id != 0) */

    return FTDM_SUCCESS;

    return FTDM_SUCCESS;
}
/******************************************************************************/
static ftdm_status_t handle_set_blocks(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
    int                 x;
    sngss7_chan_data_t  *ss7_info;
    ftdm_channel_t      *ftdmchan;
    int                 lspan;
    int                 lchan;

    x=1;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink != 1) {
            ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCircuit[x].obj;
            ftdmchan = ss7_info->ftdmchan;

            /* if span == 0 then all spans should be printed */
            if (span == 0) {
                lspan = ftdmchan->physical_span_id;
            } else {
                lspan = span;
            }

            /* if chan == 0 then all chans should be printed */
            if (chan == 0) {
                lchan = ftdmchan->physical_chan_id;
            } else {
                lchan = chan;
            }

            if ((ftdmchan->physical_span_id == lspan) && (ftdmchan->physical_chan_id == lchan)) {
                /* now that we have the right channel...put a lock on it so no-one else can use it */
                ftdm_mutex_lock(ftdmchan->mutex);

                /* check if there is a pending state change|give it a bit to clear */
                if (check_for_state_change(ftdmchan)) {
                    SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", ss7_info->circuit->cic);
                } else {
                    /* throw the ckt block flag */
                    sngss7_set_flag(ss7_info, FLAG_CKT_MN_BLOCK_TX);

                    /* set the channel to suspended state */
                    ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
                }

                /* unlock the channel again before we exit */
                ftdm_mutex_unlock(ftdmchan->mutex);

            } /* if ( span and chan) */

        } /* if ( cic != 0) */

        /* go the next circuit */
        x++;
    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x]id != 0) */

    handle_show_blocks(stream, span, chan, verbose);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_set_unblks(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
    int                 x;
    sngss7_chan_data_t  *ss7_info;
    ftdm_channel_t      *ftdmchan;
    int                 lspan;
    int                 lchan;

    x=1;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink != 1) {
            ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCircuit[x].obj;
            ftdmchan = ss7_info->ftdmchan;

            /* if span == 0 then all spans should be printed */
            if (span == 0) {
                lspan = ftdmchan->physical_span_id;
            } else {
                lspan = span;
            }

            /* if chan == 0 then all chans should be printed */
            if (chan == 0) {
                lchan = ftdmchan->physical_chan_id;
            } else {
                lchan = chan;
            }

            if ((ftdmchan->physical_span_id == lspan) && (ftdmchan->physical_chan_id == lchan)) {
                /* now that we have the right channel...put a lock on it so no-one else can use it */
                ftdm_mutex_lock(ftdmchan->mutex);

                /* check if there is a pending state change|give it a bit to clear */
                if (check_for_state_change(ftdmchan)) {
                    SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", ss7_info->circuit->cic);
                } else {
                    /* throw the ckt block flag */
                    sngss7_set_flag(ss7_info, FLAG_CKT_MN_UNBLK_TX);

                    /* clear the block flag */
                    sngss7_clear_flag(ss7_info, FLAG_CKT_MN_BLOCK_TX);

                    /* set the channel to suspended state */
                    ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
                }

                /* unlock the channel again before we exit */
                ftdm_mutex_unlock(ftdmchan->mutex);

            } /* if ( span and chan) */

        } /* if ( cic != 0) */

        /* go the next circuit */
        x++;
    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x]id != 0) */

    handle_show_blocks(stream, span, chan, verbose);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_status_link(ftdm_stream_handle_t *stream, char *name)
{
    int x;
    sng_mtp3Link_sta_t  sta;

    /* find the link request by it's name */
    x = 1;
    while(g_ftdm_sngss7_data.cfg.mtp3Link[x].id != 0) {
        if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {
            /* send the status request */
            if (sng_sta_mtp3_link(&g_ftdm_sngss7_data.cfg, x, &sta)) {
                stream->write_function(stream, "Failed to read link=%s status\n", name);
                return FTDM_FAIL;
            }

            /* print the results */
            stream->write_function(stream, "%s|state=%s|l_blk=%s|r_blk=%s|l_inhbt=%s|r_inhbt=%s\n",
                        name,
                        DECODE_LSN_LINK_STATUS(sta.state),
                        (sta.lblkd) ? "Y":"N",
                        (sta.rblkd) ? "Y":"N",
                        (sta.linhbt) ? "Y":"N",
                        (sta.rinhbt) ? "Y":"N");
            break;
        }
        
        /* move to the next link */
        x++;
    } /* while (id != 0) */

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_status_linkset(ftdm_stream_handle_t *stream, char *name)
{
    int x;
    sng_mtp3LinkSet_sta_t  sta;

    /* find the linkset request by it's name */
    x = 1;
    while(g_ftdm_sngss7_data.cfg.mtp3LinkSet[x].id != 0) {
        if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3LinkSet[x].name, name)) {
            /* send the status request */
            if (sng_sta_mtp3_linkset(&g_ftdm_sngss7_data.cfg, x, 0, &sta)) {
                stream->write_function(stream, "Failed to read linkset=%s status\n", name);
                return FTDM_FAIL;
            }

            /* print the results */
            stream->write_function(stream, "%s|state=%s|nmbActLnk=%d\n",
                        name,
                        DECODE_LSN_LINKSET_STATUS(sta.state),
                        sta.nmbActLnks);
            break;
        }
 
        /* move to the next linkset */
        x++;
    } /* while (id != 0) */

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t extract_span_chan(char *argv[10], int pos, int *span, int *chan)
{

    if (!strcasecmp(argv[pos], "span")) {
    /**************************************************************************/
        pos++;
        *span = atoi(argv[pos]);

        pos++;
        if (!strcasecmp(argv[pos], "chan")) {
        /**********************************************************************/
            pos++;
            *chan = atoi(argv[pos]);
        /**********************************************************************/
        } else {
        /**********************************************************************/
            return FTDM_FAIL;
        /**********************************************************************/
        }
    /**************************************************************************/
    } else {
    /**************************************************************************/
        *span = atoi(argv[pos]);

        pos++;
        if (!strcasecmp(argv[pos], "chan")) {
        /**********************************************************************/
            pos++;
            *chan = atoi(argv[pos]);
        /**********************************************************************/
        } else {
        /**********************************************************************/
            return FTDM_FAIL;
        /**********************************************************************/
        }
    /**************************************************************************/
    }

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t check_arg_count(int args, int min)
{
    if ( args < min ) {
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}        

/******************************************************************************/
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
/******************************************************************************/
