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
 *
 * Contributors:
 * James Zhang <jzhang@sangoma.com>
 *
 */

#if 0
#define SMG_RELAY_DBG
#endif

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
ftdm_status_t ftdm_sngss7_handle_cli_cmd(ftdm_stream_handle_t *stream, const char *data);

static ftdm_status_t handle_print_usage(ftdm_stream_handle_t *stream);

static ftdm_status_t handle_show_procId(ftdm_stream_handle_t *stream);

static ftdm_status_t handle_set_function_trace(ftdm_stream_handle_t *stream, int on, int level);
static ftdm_status_t handle_set_message_trace(ftdm_stream_handle_t *stream, int on, int level);
static ftdm_status_t handle_set_inhibit(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_set_uninhibit(ftdm_stream_handle_t *stream, char *name);

static ftdm_status_t handle_show_free(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_inuse(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_inreset(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_flags(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_blocks(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_show_status(ftdm_stream_handle_t *stream, int span, int chan, int verbose);

static ftdm_status_t handle_tx_rsc(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_tx_grs(ftdm_stream_handle_t *stream, int span, int chan, int range, int verbose);

static ftdm_status_t handle_tx_blo(ftdm_stream_handle_t *stream, int span, int chan, int verbose);
static ftdm_status_t handle_tx_ubl(ftdm_stream_handle_t *stream, int span, int chan, int verbose);

static ftdm_status_t handle_tx_cgb(ftdm_stream_handle_t *stream, int span, int chan, int range, int verbose);
static ftdm_status_t handle_tx_cgu(ftdm_stream_handle_t *stream, int span, int chan, int range, int verbose);

static ftdm_status_t handle_bind_link(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_unbind_link(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_activate_link(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_deactivate_link(ftdm_stream_handle_t *stream, char *name);

static ftdm_status_t handle_activate_linkset(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_deactivate_linkset(ftdm_stream_handle_t *stream, char *name);

static ftdm_status_t handle_tx_lpo(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_tx_lpr(ftdm_stream_handle_t *stream, char *name);

static ftdm_status_t handle_status_mtp3link(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_status_mtp2link(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_status_linkset(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_status_relay(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_status_isup_ckt(ftdm_stream_handle_t *stream, char *id_name);

static ftdm_status_t extract_span_chan(char *argv[10], int pos, int *span, int *chan);
static ftdm_status_t check_arg_count(int args, int min);



static ftdm_status_t cli_ss7_show_general(ftdm_stream_handle_t *stream);

static ftdm_status_t cli_ss7_show_mtp2link_by_id(ftdm_stream_handle_t *stream, int rcId);
static ftdm_status_t cli_ss7_show_mtp2link_by_name(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t cli_ss7_show_all_mtp2link(ftdm_stream_handle_t *stream);

static ftdm_status_t cli_ss7_show_mtp3link_by_id(ftdm_stream_handle_t *stream, int rcId);
static ftdm_status_t cli_ss7_show_mtp3link_by_name(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t cli_ss7_show_all_mtp3link(ftdm_stream_handle_t *stream);

static ftdm_status_t cli_ss7_show_relay_by_id(ftdm_stream_handle_t *stream, int rcId);
static ftdm_status_t cli_ss7_show_relay_by_name(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t cli_ss7_show_all_relay(ftdm_stream_handle_t *stream);

static ftdm_status_t cli_ss7_show_channel_detail_of_span(ftdm_stream_handle_t *stream, char *span_id, char *chan_id);
static ftdm_status_t cli_ss7_show_all_channels_of_span(ftdm_stream_handle_t *stream, char *span_id);

static ftdm_status_t cli_ss7_show_span_by_id(ftdm_stream_handle_t *stream, char *span_id);
static ftdm_status_t cli_ss7_show_all_spans_general(ftdm_stream_handle_t *stream);
static ftdm_status_t cli_ss7_show_all_spans_detail(ftdm_stream_handle_t *stream); 
static ftdm_status_t handle_show_sctp_profiles(ftdm_stream_handle_t *stream);
static ftdm_status_t handle_show_sctp_profile(ftdm_stream_handle_t *stream, char* sctp_profile_name);
static ftdm_status_t handle_show_m2ua_profiles(ftdm_stream_handle_t *stream);
static ftdm_status_t handle_show_m2ua_profile(ftdm_stream_handle_t *stream, char* m2ua_profile_name);
static ftdm_status_t handle_show_m2ua_peer_status(ftdm_stream_handle_t *stream, char* m2ua_profile_name);
static ftdm_status_t handle_show_m2ua_cluster_status(ftdm_stream_handle_t *stream, char* m2ua_profile_name);
static ftdm_status_t handle_show_nif_profiles(ftdm_stream_handle_t *stream);
static ftdm_status_t handle_show_nif_profile(ftdm_stream_handle_t *stream, char* profile_name);
int get_assoc_resp_buf(char* buf,SbMgmt* cfm);

/******************************************************************************/
/* FUNCTIONS ******************************************************************/
ftdm_status_t ftdm_sngss7_handle_cli_cmd(ftdm_stream_handle_t *stream, const char *data)
{
	char	*mycmd = NULL;
	char	*argv[10] = { 0 };
	int		argc = 0;
	int		span = 0;
	int		chan = 0;
	int		range = 0;
	int		trace = 0;
	int		trace_level = 7;
	int		verbose = 1;
	int		c = 0;

	if (data) {
		mycmd = ftdm_strdup(data);
		argc = ftdm_separate_string(mycmd, ' ', argv, ftdm_array_len(argv));
	}

	if (check_arg_count(argc, 1)) {
		goto handle_cli_error_argc;
	}

	if (!strcasecmp(argv[c], "show")) {
	/**************************************************************************/   
		if (check_arg_count(argc, 2)) {
			cli_ss7_show_general(stream); 
			return FTDM_SUCCESS;
		}
		c++;

		if (!strcasecmp(argv[c], "relay")) {
		/**********************************************************************/
			c++;
			handle_status_relay(stream, argv[c]);
			
		} else  if (!strcasecmp(argv[c], "span")) {
		/**********************************************************************/
			switch (argc) {
			case 2:
				{
					/* > ftdm ss7 show span */
					cli_ss7_show_all_spans_general(stream);
				}
				break;

			case 3:
				{
					if (!strcasecmp(argv[2], "all")) {
						/* > ftdm ss7 show span all */
						cli_ss7_show_all_spans_detail(stream);
					} else {
						/* > ftdm ss7 show span 1 */
						cli_ss7_show_span_by_id(stream, argv[2]);
					}
				}
				break;

			case 4:
				{
					/* > ftdm ss7 show span 1 chan */
					cli_ss7_show_all_channels_of_span(stream, argv[2]);
				}
				break;

			case 5:
			default:
				{
					if (!strcasecmp(argv[3], "chan")) {
						/* > ftdm ss7 show span 1 chan 2 */
						cli_ss7_show_channel_detail_of_span(stream, argv[2], argv[4]);
					} else {
						/* > ftdm ss7 show span 1 bla bla  bla*/
						cli_ss7_show_all_channels_of_span(stream, argv[2]);
					}
				}
				break;
			}
		} else  if (!strcasecmp(argv[c], "status")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) {
				cli_ss7_show_general(stream); 
				return FTDM_SUCCESS;
			}

			c++;

			if (!strcasecmp(argv[c], "mtp3")) {
			/******************************************************************/
				c++;
				handle_status_mtp3link(stream, argv[c]);
			/******************************************************************/
			} else if (!strcasecmp(argv[c], "mtp2")) {
			/******************************************************************/
				c++;
				handle_status_mtp2link(stream, argv[c]);
			/******************************************************************/
			} else if (!strcasecmp(argv[c], "linkset")) {
			/******************************************************************/
				c++;
				handle_status_linkset(stream, argv[c]);
			/******************************************************************/
			} else if (!strcasecmp(argv[c], "relay")) {
			/******************************************************************/
				c++;
				handle_status_relay(stream, argv[c]);
			/******************************************************************/
			} else if (!strcasecmp(argv[c], "span")) {
			/******************************************************************/
				if (check_arg_count(argc, 6)) goto handle_cli_error_argc;

				if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

				handle_show_status(stream, span, chan, verbose);
			/******************************************************************/
			} else if (!strcasecmp(argv[c], "isup")) {
			/******************************************************************/
				if (check_arg_count(argc, 4)) goto handle_cli_error_argc;
				c++;

				if (!strcasecmp(argv[c], "ckt")) {
				/**************************************************************/
					if (check_arg_count(argc, 5)) goto handle_cli_error_argc;
					c++;

					handle_status_isup_ckt(stream, argv[c]);
				/**************************************************************/
				} else {
				/**************************************************************/
					stream->write_function(stream, "Unknown \"status isup\" command\n");
					goto handle_cli_error;
				/**************************************************************/
				}
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
		} else if (!strcasecmp(argv[c], "mem")) {
		/**********************************************************************/
			sng_isup_reg_info_show();
		/**********************************************************************/
		} else if (!strcasecmp(argv[c], "stats")) {
		/**********************************************************************/
/*			sng_mtp1_sts_t sts;

			memset(&sts, 0x0, sizeof(sng_mtp1_sts_t));

			sng_mtp1_sts(1, &sts);

			stream->write_function(stream,"MTP1 tx stats:|tx_frm=%d|tx_err=%d|tx_fisu=%d|tx_lssu=%d|tx_msu=%d|\n",
											sts.tx_frm, sts.tx_err, sts.tx_fisu, sts.tx_lssu, sts.tx_msu);
			stream->write_function(stream,"MTP1 rx stats:|rx_frm=%d|rx_err=%d|rx_fisu=%d|rx_lssu=%d|rx_msu=%d|\n",
											sts.rx_frm, sts.rx_err, sts.rx_fisu, sts.rx_lssu, sts.rx_msu);
*/
		/**********************************************************************/
		} else if (!strcasecmp(argv[c], "procid")) {
		/**********************************************************************/
			handle_show_procId(stream);

		/**********************************************************************/
		} else{ 
	    /**********************************************************************/
			stream->write_function(stream, "Unknown \"show\" command\n");
			goto handle_cli_error;
		}
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "xmlshow")) {
	/**************************************************************************/

		if (check_arg_count(argc, 2)) {
			stream->write_function(stream, "Unknown \"xmlshow\" command\n");
			goto handle_cli_error;
		}
		c++;
	/**************************************************************************/
		if (!strcasecmp(argv[c], "m2ua")) {
	/**************************************************************************/
			switch(argc)
			{
				case 2: /* show m2ua */
					{
						handle_show_m2ua_profiles(stream);
						break;
					}
				case 3: /* show m2ua <profile-name> */
					{
						c++;
						handle_show_m2ua_profile(stream, argv[c]);
						break;
					}
				case 4:
					{
						char* profile_name = argv[++c];
						c++;
						/***************************************************************/
						if(!strcasecmp(argv[c],"peerstatus")){
						/***************************************************************/
							handle_show_m2ua_peer_status(stream, profile_name);
						/***************************************************************/
						}else if(!strcasecmp(argv[c],"clusterstatus")){
						/***************************************************************/
							handle_show_m2ua_cluster_status(stream, profile_name);
						/***************************************************************/
						} else{
						/***************************************************************/
							stream->write_function(stream, "Unknown \"show m2ua \" command..\n");
							goto handle_cli_error_argc;
						}
						break;
					}
				default:
					goto handle_cli_error_argc;
			}

	   /**********************************************************************/
		} else if (!strcasecmp(argv[c], "nif")) {
	   /**********************************************************************/
			if (check_arg_count(argc, 3)){
				handle_show_nif_profiles(stream);
			}else{	
				c++;
				handle_show_nif_profile(stream, argv[c]);
			}
	    /**********************************************************************/
		} else if (!strcasecmp(argv[c], "sctp")) {
	    /**********************************************************************/
			if (check_arg_count(argc, 3)){
				handle_show_sctp_profiles(stream);
			}else{	
				c++;
				handle_show_sctp_profile(stream, argv[c]);
			}
	    /**********************************************************************/
		} else {
	    /**********************************************************************/
			stream->write_function(stream, "Unknown \"xmlshow\" command\n");
			goto handle_cli_error;
		}
	    /**********************************************************************/
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
	} else if (!strcasecmp(argv[c], "inhibit")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "link")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_set_inhibit(stream, argv[c]);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"block\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		}   
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "uninhibit")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "link")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_set_uninhibit(stream, argv[c]);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"unblock\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		} 
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "blo")) {
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "span")) {
		/**********************************************************************/
			if (check_arg_count(argc, 5)) goto handle_cli_error_argc;

			if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

			handle_tx_blo(stream, span, chan, verbose);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"block\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		}
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "ubl")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "span")) {
		/**********************************************************************/
			if (check_arg_count(argc, 5)) goto handle_cli_error_argc;

			if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

			handle_tx_ubl(stream, span, chan, verbose);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"ubl\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		} 
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "cgb")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "span")) {
		/**********************************************************************/
			if (check_arg_count(argc, 5)) goto handle_cli_error_argc;

			if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;
			c = c + 4;

			if (check_arg_count(argc, 7)) goto handle_cli_error_argc;

			if (!strcasecmp(argv[c], "range")) {
			/******************************************************************/
				c++;
				range =  atoi(argv[c]);
			/******************************************************************/
			} else {
			/******************************************************************/
				stream->write_function(stream, "Unknown \"cgb range\" command\n");
				goto handle_cli_error;
			/******************************************************************/
			}

			handle_tx_cgb(stream, span, chan, range, verbose);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"cgb\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		} 
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "cgu")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "span")) {
		/**********************************************************************/
			if (check_arg_count(argc, 5)) goto handle_cli_error_argc;

			if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;
			c = c + 4;

			if (check_arg_count(argc, 7)) goto handle_cli_error_argc;

			if (!strcasecmp(argv[c], "range")) {
			/******************************************************************/
				c++;
				range =  atoi(argv[c]);
			/******************************************************************/
			} else {
			/******************************************************************/
				stream->write_function(stream, "Unknown \"cgu range\" command\n");
				goto handle_cli_error;
			/******************************************************************/
			}

			handle_tx_cgu(stream, span, chan, range, verbose);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"cgu\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		} 
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "rsc")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "span")) {
		/**********************************************************************/
			if (check_arg_count(argc, 5)) goto handle_cli_error_argc;

			if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;

			handle_tx_rsc(stream, span, chan, verbose);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"rsc\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		} 
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "grs")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "span")) {
		/**********************************************************************/
			if (check_arg_count(argc, 5)) goto handle_cli_error_argc;

			if (extract_span_chan(argv, c, &span, &chan)) goto handle_cli_error_span_chan;
			c = c + 4;

			if (check_arg_count(argc, 7)) goto handle_cli_error_argc;

			if (!strcasecmp(argv[c], "range")) {
			/******************************************************************/
				c++;
				range =  atoi(argv[c]);
			/******************************************************************/
			} else {
			/******************************************************************/
				stream->write_function(stream, "Unknown \"grs range\" command\n");
				goto handle_cli_error;
			/******************************************************************/
			}

			handle_tx_grs(stream, span, chan, range, verbose);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"grs\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		}
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "lpo")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "link")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_tx_lpo(stream, argv[c]);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"lpo\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		}
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "lpr")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "link")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_tx_lpr(stream, argv[c]);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"lpr\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		}
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "bind")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "link")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_bind_link(stream, argv[c]);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"bind\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		}
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "unbind")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "link")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_unbind_link(stream, argv[c]);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"bind\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		}
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "activate")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "link")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_activate_link(stream, argv[c]);
		/**********************************************************************/
		}else if (!strcasecmp(argv[c], "linkset")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_activate_linkset(stream, argv[c]);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"activate\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		}
	/**************************************************************************/
	} else if (!strcasecmp(argv[c], "deactivate")) {
	/**************************************************************************/
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
		c++;

		if (!strcasecmp(argv[c], "link")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_deactivate_link(stream, argv[c]);
		/**********************************************************************/
		}else if (!strcasecmp(argv[c], "linkset")) {
		/**********************************************************************/
			if (check_arg_count(argc, 3)) goto handle_cli_error_argc;
			c++;
			
			handle_deactivate_linkset(stream, argv[c]);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			stream->write_function(stream, "Unknown \"deactivate\" command\n");
			goto handle_cli_error;
		/**********************************************************************/
		}
	/**************************************************************************/	
	} else if (!strcasecmp(argv[c], "m2ua")) {
	/**************************************************************************/	
		if (check_arg_count(argc, 3)) {
			stream->write_function(stream, "Invalid \"m2ua  option\", please use \"m2ua logging [enable|disable] \n");
			goto handle_cli_error_argc;
		}
		c++;
		if(!strcasecmp(argv[c],"logging")){
			c++;
			if(!strcasecmp(argv[c],"enable")){
				ftmod_ss7_enable_m2ua_sg_logging();
			}else if(!strcasecmp(argv[c],"disable")){
				ftmod_ss7_disable_m2ua_sg_logging();
			} else{
				stream->write_function(stream, "Unknown \"m2ua logging %s option\", supported values enable/disable\n",argv[c]);
				goto handle_cli_error_argc;
			}
		}else{
			stream->write_function(stream, "Unknown \"m2ua  %s option\", supported values \"logging\"\n",argv[c]);
			goto handle_cli_error_argc;
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
	handle_print_usage(stream);
	return FTDM_SUCCESS;

handle_cli_error_span_chan:
	stream->write_function(stream, "Unknown \"span\\chan\" command\n");
	handle_print_usage(stream);
	return FTDM_SUCCESS;

handle_cli_error:
	stream->write_function(stream, "Unknown command requested\n");
	handle_print_usage(stream);
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_print_usage(ftdm_stream_handle_t *stream)
{
	stream->write_function(stream, "Sangoma SS7 CLI usage:\n\n");

	stream->write_function(stream, "ftmod_sangoma_ss7 general control:\n");
	stream->write_function(stream, "ftdm ss7 set ftrace X Y\n");
	stream->write_function(stream, "ftdm ss7 set mtrace X Y\n");
	stream->write_function(stream, "\n");
    
	stream->write_function(stream, "ftmod_sangoma_ss7 signaling information:\n");
	stream->write_function(stream, "ftdm ss7 show \n");
	stream->write_function(stream, "ftdm ss7 show status mtp2 X\n");
	stream->write_function(stream, "ftdm ss7 show status mtp3 X\n");
	stream->write_function(stream, "ftdm ss7 show status linkset X\n");
	stream->write_function(stream, "\n");
    
	stream->write_function(stream, "ftmod_sangoma_ss7 circuit information:\n");
	stream->write_function(stream, "ftdm ss7 show span all\n");
	stream->write_function(stream, "ftdm ss7 show span X\n");
	stream->write_function(stream, "ftdm ss7 show status span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 show free span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 show blocks span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 show inuse span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 show inreset span X chan Y\n");
	stream->write_function(stream, "\n");
    
	stream->write_function(stream, "ftmod_sangoma_ss7 circuit control:\n");
	stream->write_function(stream, "ftdm ss7 blo span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 ubl span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 rsc span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 grs span X chan Y range Z\n");
	stream->write_function(stream, "ftdm ss7 cgb span X chan Y range Z\n");
	stream->write_function(stream, "ftdm ss7 cgu span X chan Y range Z\n");
	stream->write_function(stream, "\n");
    
	stream->write_function(stream, "ftmod_sangoma_ss7 link control:\n");
	/*
	stream->write_function(stream, "ftdm ss7 inhibit link X\n");
	stream->write_function(stream, "ftdm ss7 uninhibit link X\n");
	*/
	stream->write_function(stream, "ftdm ss7 activate link X\n");
	stream->write_function(stream, "ftdm ss7 deactivate link X\n");
	stream->write_function(stream, "ftdm ss7 activate linkset X\n");
	stream->write_function(stream, "ftdm ss7 deactivate linkset X\n");
	stream->write_function(stream, "ftdm ss7 lpo link X\n");
	stream->write_function(stream, "ftdm ss7 lpr link X\n");
	stream->write_function(stream, "\n");

	
	stream->write_function(stream, "ftmod_sangoma_ss7 Relay status:\n");
	stream->write_function(stream, "ftdm ss7 show status relay X\n");
	stream->write_function(stream, "ftdm ss7 show relay X\n");
	stream->write_function(stream, "ftdm ss7 show relay\n");
	stream->write_function(stream, "\n");

	stream->write_function(stream, "ftmod_sangoma_ss7 M2UA :\n");
	stream->write_function(stream, "ftdm ss7 xmlshow sctp \n");
	stream->write_function(stream, "ftdm ss7 xmlshow sctp <sctp_interface_name>\n");
	stream->write_function(stream, "ftdm ss7 xmlshow m2ua \n");
	stream->write_function(stream, "ftdm ss7 xmlshow m2ua <m2ua_interface_name>\n");
	stream->write_function(stream, "ftdm ss7 xmlshow m2ua <m2ua_interface_name> peerstatus\n");
	stream->write_function(stream, "ftdm ss7 xmlshow m2ua <m2ua_interface_name> clusterstatus\n");
	stream->write_function(stream, "ftdm ss7 xmlshow nif \n");
	stream->write_function(stream, "ftdm ss7 xmlshow nif <nif_interface_name>\n");
	stream->write_function(stream, "\n");


	stream->write_function(stream, "ftmod_sangoma_ss7 M2UA logging:\n");
	stream->write_function(stream, "ftdm ss7 m2ua logging [enable|disable] \n");

	stream->write_function(stream, "\n");

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_procId(ftdm_stream_handle_t *stream)
{
	int	procId = sng_get_procId();

	stream->write_function(stream, "Local ProcId = %d\n", procId);

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
	int				 x;
	int				 free;
	sngss7_chan_data_t  *ss7_info;
	ftdm_channel_t	  *ftdmchan;
	int				 lspan;
	int				 lchan;

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	free = 0;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
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
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	stream->write_function(stream, "\nTotal # of CICs free = %d\n",free);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_inuse(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 x;
	int				 in_use;
	sngss7_chan_data_t  *ss7_info;
	ftdm_channel_t	  *ftdmchan;
	int				 lspan;
	int				 lchan;

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	in_use = 0;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
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
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	stream->write_function(stream, "\nTotal # of CICs in use = %d\n",in_use);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_inreset(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 x;
	int				 in_reset;
	sngss7_chan_data_t  *ss7_info;
	ftdm_channel_t	  *ftdmchan;
	int				 lspan;
	int				 lchan;

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	in_reset = 0;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
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
				if ((sngss7_test_ckt_flag(ss7_info, FLAG_RESET_RX)) ||
					(sngss7_test_ckt_flag(ss7_info, FLAG_RESET_TX)) ||
					(sngss7_test_ckt_flag(ss7_info, FLAG_GRP_RESET_RX)) ||
					(sngss7_test_ckt_flag(ss7_info, FLAG_GRP_RESET_TX))) {
					
					if (verbose) {
						stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|in_reset=Y\n",
									ftdmchan->physical_span_id,
									ftdmchan->physical_chan_id,
									ss7_info->circuit->cic);
					} /* if (verbose) */
		
					/*increment the count of circuits in reset */
					in_reset++;
				} /* if ((sngss7_test_ckt_flag(ss7_info, FLAG_RESET_RX) ... */
			} /* if ( span and chan) */
		} /* if ( cic != 0) */

		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	stream->write_function(stream, "\nTotal # of CICs in reset = %d\n",in_reset);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_flags(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	sngss7_chan_data_t	*ss7_info;
	ftdm_channel_t		*ftdmchan;
	int					x;
	int					bit;
	int					lspan;
	int					lchan;
	const char			*text;
	int					flag;

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
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
					if (ss7_info->ckt_flags & ( 0x1 << bit)) {
						stream->write_function(stream, "|");
						flag = bit;
						text = ftmod_ss7_ckt_flag2str(flag);
						stream->write_function(stream, "%s",text);
					}
				}

				for (bit = 0; bit < 33; bit++) {
					if (ss7_info->blk_flags & ( 0x1 << bit)) {
						stream->write_function(stream, "|");
						flag = bit;
						text = ftmod_ss7_blk_flag2str(flag);
						stream->write_function(stream, "%s",text);
					}
				}

				stream->write_function(stream, "\n");
			} /* if ( span and chan) */

		} /* if ( cic != 0) */

		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_blocks(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 x;
	sngss7_chan_data_t  *ss7_info;
	ftdm_channel_t	  *ftdmchan;
	int				 lspan;
	int				 lchan;

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
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

				if((sngss7_test_ckt_blk_flag(ss7_info, FLAG_CKT_MN_BLOCK_TX)) || (sngss7_test_ckt_blk_flag(ss7_info, FLAG_GRP_MN_BLOCK_TX))) {
					stream->write_function(stream, "l_mn=Y|");
				}else {
					stream->write_function(stream, "l_mn=N|");
				}

				if((sngss7_test_ckt_blk_flag(ss7_info, FLAG_CKT_MN_BLOCK_RX)) || (sngss7_test_ckt_blk_flag(ss7_info, FLAG_GRP_MN_BLOCK_RX))) {
					stream->write_function(stream, "r_mn=Y|");
				}else {
					stream->write_function(stream, "r_mn=N|");
				}

				if(sngss7_test_ckt_blk_flag(ss7_info, FLAG_GRP_HW_BLOCK_TX)) {
					stream->write_function(stream, "l_hw=Y|");
				}else {
					stream->write_function(stream, "l_hw=N|");
				}

				if(sngss7_test_ckt_blk_flag(ss7_info, FLAG_GRP_HW_BLOCK_RX)) {
					stream->write_function(stream, "r_hw=Y|");
				}else {
					stream->write_function(stream, "r_hw=N|");
				}

				if(sngss7_test_ckt_blk_flag(ss7_info, FLAG_CKT_LC_BLOCK_RX)) {
					stream->write_function(stream, "l_mngmt=Y|");
				}else {
					stream->write_function(stream, "l_mngmt=N|");
				}

				if(sngss7_test_ckt_blk_flag(ss7_info, FLAG_CKT_UCIC_BLOCK)) {
					stream->write_function(stream, "l_ucic=Y|");
				}else {
					stream->write_function(stream, "l_ucic=N|");
				} 

#ifdef SMG_RELAY_DBG
				stream->write_function(stream," blk_flag=%x | ckt_flag=%x | chan_flag=%x", ss7_info->blk_flags, ss7_info->ckt_flags, ftdmchan->flags);
#endif
				stream->write_function(stream, "\n");				
			} /* if ( span and chan) */

		} /* if ( cic != 0) */

		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_status(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 			x;
	sngss7_chan_data_t  		*ss7_info;
	ftdm_channel_t	  			*ftdmchan;
	int				 			lspan;
	int				 			lchan;
	ftdm_signaling_status_t		sigstatus = FTDM_SIG_STATE_DOWN;
	sng_isup_ckt_t				*ckt;

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
			/* extract the circuit to make it easier to work with */
			ckt = &g_ftdm_sngss7_data.cfg.isupCkt[x];

			/* if span == 0 then all spans should be printed */
			if (span == 0) {
				lspan = ckt->span;
			} else {
				lspan = span;
			}

			/* if chan == 0 then all chans should be printed */
			if (chan == 0) {
				lchan = ckt->chan;
			} else {
				lchan = chan;
			}

			/* check if this circuit is one of the circuits we're interested in */
			if ((ckt->span == lspan) && (ckt->chan == lchan)) {
				if (ckt->type == SNG_CKT_HOLE) {
					stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|NOT USED\n",
							ckt->span,
							ckt->chan,
							ckt->cic);
				} else if (ckt->type == SNG_CKT_SIG) {
					stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|SIGNALING LINK\n",
							ckt->span,
							ckt->chan,
							ckt->cic);
				} else {
					ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
					ftdmchan = ss7_info->ftdmchan;

					if (ftdmchan == NULL) {
						/* this should never happen!!! */
						stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|FTDMCHAN DOES NOT EXISTS",
														ckt->span,
														ckt->chan,
														ckt->cic);
						
					} else {
						/* grab the signaling_status */
						ftdm_channel_get_sig_status(ftdmchan, &sigstatus);
		
						stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|ckt=%4d|sig_status=%4s|state=%s|",
														ckt->span,
														ckt->chan,
														ckt->cic,
														ckt->id,
														ftdm_signaling_status2str(sigstatus),
														ftdm_channel_state2str(ftdmchan->state));
		
						if ((sngss7_test_ckt_blk_flag(ss7_info, FLAG_CKT_MN_BLOCK_TX)) || 
							(sngss7_test_ckt_blk_flag(ss7_info, FLAG_GRP_MN_BLOCK_TX)) ||
							(sngss7_test_ckt_blk_flag(ss7_info, FLAG_CKT_LC_BLOCK_RX))) {
							stream->write_function(stream, "l_mn=Y|");
						}else {
							stream->write_function(stream, "l_mn=N|");
						}
		
						if ((sngss7_test_ckt_blk_flag(ss7_info, FLAG_CKT_MN_BLOCK_RX)) || 
							(sngss7_test_ckt_blk_flag(ss7_info, FLAG_GRP_MN_BLOCK_RX))) {
							stream->write_function(stream, "r_mn=Y|");
						}else {
							stream->write_function(stream, "r_mn=N|");
						}
		
						if (sngss7_test_ckt_blk_flag(ss7_info, FLAG_GRP_HW_BLOCK_TX)) {
							stream->write_function(stream, "l_hw=Y|");
						}else {
							stream->write_function(stream, "l_hw=N|");
						}
		
						if (sngss7_test_ckt_blk_flag(ss7_info, FLAG_GRP_HW_BLOCK_RX)) {
							stream->write_function(stream, "r_hw=Y|");
						}else {
							stream->write_function(stream, "r_hw=N|");
						}
	

						if (g_ftdm_sngss7_data.cfg.procId != 1) {
						/* if (sngss7_test_ckt_blk_flag(ss7_info, FLAG_RELAY_DOWN)) { */
							stream->write_function(stream, "relay=Y|");
						}else {
							stream->write_function(stream, "relay=N");
						}

#ifdef SMG_RELAY_DBG
						stream->write_function(stream, "| flag=0x%llx", ftdmchan->flags);
#endif
					}
		
#ifdef SMG_RELAY_DBG
					stream->write_function(stream," | blk_flag=%x | ckt_flag=%x", ss7_info->blk_flags, ss7_info->ckt_flags);
#endif
					stream->write_function(stream, "\n");
				} /* if ( hole, sig, voice) */
			} /* if ( span and chan) */
		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	/* Look spans that are being used by M2UA SG links */
	for (x = 1; x < ftdm_array_len(g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif); x++) {
		if (g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].id) {
			if (g_ftdm_sngss7_data.cfg.mtp2Link[g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].mtp2LnkNmb].id) {
				uint32_t mtp1_id = g_ftdm_sngss7_data.cfg.mtp2Link[g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].mtp2LnkNmb].id;
				if (g_ftdm_sngss7_data.cfg.mtp1Link[mtp1_id].id) {
					if (g_ftdm_sngss7_data.cfg.mtp1Link[mtp1_id].span == span) {
						if (chan) {
							if (chan == g_ftdm_sngss7_data.cfg.mtp1Link[mtp1_id].chan) {
							stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|SIGNALING LINK\n",
													g_ftdm_sngss7_data.cfg.mtp1Link[mtp1_id].span,
													g_ftdm_sngss7_data.cfg.mtp1Link[mtp1_id].chan,
													0);
							}
						} else {
							stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|SIGNALING LINK\n",
													g_ftdm_sngss7_data.cfg.mtp1Link[mtp1_id].span,
													g_ftdm_sngss7_data.cfg.mtp1Link[mtp1_id].chan,
													0);
						}
					}
				}

			}
		}
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_tx_blo(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 x;
	sngss7_chan_data_t  *ss7_info;
	ftdm_channel_t	  *ftdmchan;
	int				 lspan;
	int				 lchan;

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
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
				ftdm_mutex_lock(ftdmchan->mutex);

				/* check if there is a pending state change|give it a bit to clear */
				if (check_for_state_change(ftdmchan)) {
					SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", ss7_info->circuit->cic);
					ftdm_assert(0, "State change not completed\n");
					ftdm_mutex_unlock(ftdmchan->mutex);
					continue;
				} else {
					sngss7_set_ckt_blk_flag(ss7_info, FLAG_CKT_MN_BLOCK_TX);
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
				}
				
				ftdm_mutex_unlock(ftdmchan->mutex);
			}

		}

		x++;
	}

	handle_show_blocks(stream, span, chan, verbose);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_tx_ubl(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 x;
	sngss7_chan_data_t  *ss7_info;
	ftdm_channel_t	  *ftdmchan;
	int				 lspan;
	int				 lchan;

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
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
				ftdm_mutex_lock(ftdmchan->mutex);

				/* check if there is a pending state change|give it a bit to clear */
				if (check_for_state_change(ftdmchan)) {
					SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", ss7_info->circuit->cic);
					ftdm_assert(0, "State change not completed\n");
					ftdm_mutex_unlock(ftdmchan->mutex);
					continue;
				} else {
					sngss7_set_ckt_blk_flag(ss7_info, FLAG_CKT_MN_UNBLK_TX);
					sngss7_clear_ckt_blk_flag(ss7_info, FLAG_CKT_MN_BLOCK_TX);
					sngss7_clear_ckt_blk_flag(ss7_info, FLAG_GRP_MN_BLOCK_TX); 
					
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
				}

				ftdm_mutex_unlock(ftdmchan->mutex);

			}

		}

		/* go the next circuit */
		x++;
	}

	handle_show_blocks(stream, span, chan, verbose);

	return FTDM_SUCCESS;
}


/******************************************************************************/
static ftdm_status_t handle_status_mtp3link(ftdm_stream_handle_t *stream, char *name)
{
	SS7_RELAY_DBG_FUN(handle_status_mtp3link);
	
	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Invalid stream\n");

	if (!name) {
		return cli_ss7_show_all_mtp3link(stream);
	}

	return cli_ss7_show_mtp3link_by_name(stream, name);
}


/******************************************************************************/
static ftdm_status_t handle_status_mtp2link(ftdm_stream_handle_t *stream, char *name)
{
	SS7_RELAY_DBG_FUN(handle_status_mtp2link);
	
	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Invalid stream\n");

	if (!name) {
		return cli_ss7_show_all_mtp2link(stream);
	}

	return cli_ss7_show_mtp2link_by_name(stream, name);
}

/******************************************************************************/
static ftdm_status_t handle_status_linkset(ftdm_stream_handle_t *stream, char *name)
{
	int 		x = 0;
	SnMngmt		sta;

	/* find the linkset request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKSETS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLinkSet[x].name, name)) {

			/* send the status request */
			if (ftmod_ss7_mtplinkSet_sta(x, &sta)) {
				stream->write_function(stream, "Failed to read linkset=%s status\n", name);
				return FTDM_FAIL;
			}

			/* print the results */
			stream->write_function(stream, "%s|state=%s|nmbActLnk=%d\n",
						name,
						DECODE_LSN_LINKSET_STATUS(sta.t.ssta.s.snLnkSet.state),
						sta.t.ssta.s.snLnkSet.nmbActLnks);
			
			goto success;
		}
 
		/* move to the next linkset */
		x++;
	} /* while (id != 0) */

	stream->write_function(stream, "Failed to find link=\"%s\"\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_set_inhibit(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {

			/* send the inhibit request */
			if (ftmod_ss7_inhibit_mtp3link(x)) {
				stream->write_function(stream, "Failed to inhibit link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_mtp3link(stream, &name[0]);

			goto success;
		}
 
		/* move to the next linkset */
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	stream->write_function(stream, "Failed to find link=\"%s\"\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_set_uninhibit(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_uninhibit_mtp3link(x)) {
				stream->write_function(stream, "Failed to uninhibit link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_mtp3link(stream, &name[0]);

			goto success;
		}
 
		/* move to the next linkset */
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	stream->write_function(stream, "Failed to find link=\"%s\"\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_tx_rsc(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 	x;
	sngss7_chan_data_t  *sngss7_info;
	ftdm_channel_t	  	*ftdmchan;
	int				 	lspan;
	int				 	lchan;

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;

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
				/* lock the channel */
				ftdm_mutex_lock(ftdmchan->mutex);

				/* throw the reset flag */
				sngss7_set_ckt_flag (sngss7_info, FLAG_LOCAL_REL);
				sngss7_clear_ckt_flag (sngss7_info, FLAG_REMOTE_REL);
				sngss7_tx_reset_restart(sngss7_info);

				switch (ftdmchan->state) {
				/**************************************************************************/
				case FTDM_CHANNEL_STATE_RESTART:
					/* go to idle so that we can redo the restart state*/
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_IDLE);
					break;
				/**************************************************************************/
				default:
					/* set the state of the channel to restart...the rest is done by the chan monitor */
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
					break;
				/**************************************************************************/
				}
			
				/* unlock the channel again before we exit */
				ftdm_mutex_unlock(ftdmchan->mutex);
			} /* if ( span and chan) */

		} /* if ( cic == voice) */

		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	/* print the status of channels */
	handle_show_status(stream, span, chan, verbose);
	
	

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_tx_grs(ftdm_stream_handle_t *stream, int span, int chan, int range, int verbose)
{
	sngss7_chan_data_t *sngss7_info = NULL;
	ftdm_channel_t *ftdmchan = NULL;
	sngss7_span_data_t *sngss7_span = NULL;
	int x = 0;
	int basefound = 0;

	if (range > 31) {
		stream->write_function(stream, "Range value %d is too big for a GRS", range);
		return FTDM_SUCCESS;
	}

	if (range < 2) {
		stream->write_function(stream, "Range value %d is too small for a GRS", range);
		return FTDM_SUCCESS;
	}

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {

			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->signal_data;

			if ((ftdmchan->physical_span_id == span) && 
				((ftdmchan->physical_chan_id >= chan) && (ftdmchan->physical_chan_id < (chan+range)))) {

				/* now that we have the right channel...put a lock on it so no-one else can use it */
				ftdm_channel_lock(ftdmchan);

				/* if another reset is still in progress, skip this channel */
				if (sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_TX)) {
					ftdm_channel_unlock(ftdmchan);
					continue;
				}

				/* check if there is a pending state change|give it a bit to clear */
				if (check_for_state_change(ftdmchan)) {
					SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
					ftdm_channel_unlock(ftdmchan);
					continue;
				}

				/* throw the grp reset flag */
				sngss7_set_ckt_flag(sngss7_info, FLAG_GRP_RESET_TX);
				if (!basefound) {
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Setting channel as GRS base\n");
					sngss7_set_ckt_flag(sngss7_info, FLAG_GRP_RESET_BASE);
					sngss7_info->tx_grs.circuit = sngss7_info->circuit->id;
					sngss7_info->tx_grs.range = range - 1;
					basefound = 1;
				}

				/* set the channel to restart state */
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);

				ftdm_channel_unlock(ftdmchan);

			}

		}

		/* go the next circuit */
		x++;
	}
	
	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {

			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->signal_data;

			if ((ftdmchan->physical_span_id == span) && 
				((ftdmchan->physical_chan_id >= chan) && (ftdmchan->physical_chan_id < (chan+range)))) {

				handle_show_status(stream, span, chan, verbose);
			}
		} /* if ( cic == voice) */

		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_tx_cgb(ftdm_stream_handle_t *stream, int span, int chan, int range, int verbose)
{
	int					x;
	sngss7_chan_data_t	*sngss7_info;
	ftdm_channel_t		*ftdmchan;
	ftdm_channel_t		*main_chan = NULL;
	sngss7_span_data_t	*sngss7_span;
	int					byte = 0;
	int					bit = 0;
	ftdm_sigmsg_t 		sigev;

	memset (&sigev, 0, sizeof (sigev));


	if (range <= 1 || range > 31) {
		stream->write_function(stream, "Invalid range value %d. Range value must be greater than 1 and less than 31. \n", range);
		return FTDM_SUCCESS;
	}

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {

			/* extract the channel and span info for this circuit */
			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->signal_data;

			/* check if this circuit is part of the block */
			if ((ftdmchan->physical_span_id == span) && 
				((ftdmchan->physical_chan_id >= chan) && (ftdmchan->physical_chan_id < (chan+range)))) {

				/* now that we have the right channel...put a lock on it so no-one else can use it */
				ftdm_channel_lock(ftdmchan);

				/* throw the grp maint. block flag */
				sngss7_set_ckt_blk_flag(sngss7_info, FLAG_GRP_MN_BLOCK_TX);

				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

				/* bring the sig status down */
				sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_DOWN);

				/* if this is the first channel in the range */
				if (!main_chan) {
					/* attach the cgb information */
					main_chan = ftdmchan;
					sngss7_span->tx_cgb.circuit = sngss7_info->circuit->id;
					sngss7_span->tx_cgb.range = 0;
					sngss7_span->tx_cgb.type = 0; /* maintenace block */
				} else {
					((sngss7_span_data_t*)(main_chan->span->signal_data))->tx_cgb.range++;
				}
				
				/* update the status field */
				sngss7_span->tx_cgb.status[byte] = (sngss7_span->tx_cgb.status[byte] | (1 << bit));

				/* update the bit and byte counter*/
				bit ++;
				if (bit == 8) {
					byte++;
					bit = 0;
				}

				/* unlock the channel again before we exit */
				ftdm_channel_unlock(ftdmchan);
			}
		}
		/* go the next circuit */
		x++;
	}

	if (!main_chan) {
		stream->write_function(stream, "Failed to find a voice cic in span %d chan %d range %d", span, chan, range);
		return FTDM_SUCCESS;
	}

	/* send the circuit group block */
	ft_to_sngss7_cgb(main_chan);

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {

			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->signal_data;

			if ((ftdmchan->physical_span_id == span) && 
				((ftdmchan->physical_chan_id >= chan) && (ftdmchan->physical_chan_id < (chan+range)))) {

				handle_show_status(stream, ftdmchan->physical_span_id, ftdmchan->physical_chan_id, verbose);
			}
		} /* if ( cic == voice) */

		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */
	

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_tx_cgu(ftdm_stream_handle_t *stream, int span, int chan, int range, int verbose)
{
	sngss7_chan_data_t *sngss7_info = NULL;
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_channel_t *main_chan = NULL;
	sngss7_span_data_t *sngss7_span = NULL;
	sngss7_chan_data_t *ubl_sng_info[MAX_CIC_MAP_LENGTH+1];
	int x = 0;
	int byte = 0;
	int bit = 0;
	int ubl_sng_info_idx = 1;
	ftdm_sigmsg_t sigev;

	memset(ubl_sng_info, 0, sizeof(ubl_sng_info));
	memset (&sigev, 0, sizeof (sigev));

	if (range <= 1 || range > 31) {
		stream->write_function(stream, "Invalid range value %d. Range value must be greater than 1 and less than 31.\n", range);
		return FTDM_SUCCESS;
	}

	/* verify that there is not hardware block in the range. 
	 * if there is any channel within the group unblock range, do not execute the group unblock */
	x = (g_ftdm_sngss7_data.cfg.procId * MAX_CIC_MAP_LENGTH) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->signal_data;

			if ( (ftdmchan->physical_span_id == span) 
			  && (ftdmchan->physical_chan_id >= chan) 
			  && (ftdmchan->physical_chan_id < (chan+range))
			  && sngss7_test_ckt_blk_flag(sngss7_info, (FLAG_GRP_HW_BLOCK_TX | FLAG_GRP_HW_BLOCK_TX_DN))
			  ) {
				stream->write_function(stream, "There is at least one channel with hardware block. Group unblock operation not allowed at this time.\n");
				return FTDM_SUCCESS;
			}
		}
		x++;
	}


	x = (g_ftdm_sngss7_data.cfg.procId * MAX_CIC_MAP_LENGTH) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {

			/* extract the channel and span info for this circuit */
			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->signal_data;

			/* check if this circuit is part of the block */
			if ((ftdmchan->physical_span_id == span) && 
				((ftdmchan->physical_chan_id >= chan) && (ftdmchan->physical_chan_id < (chan+range)))) {

				/* now that we have the right channel...put a lock on it so no-one else can use it */
				ftdm_channel_lock(ftdmchan);

				/* throw the grp maint. block flag */
				sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_GRP_MN_BLOCK_TX);
				
				if (sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX_DN)) {
					ubl_sng_info[ubl_sng_info_idx] = sngss7_info;
					ubl_sng_info_idx++;
				}

				/* bring the sig status up */
				sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_UP);
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

				/* if this is the first channel in the range */
				if (!main_chan) {
					/* attach the cgb information */
					main_chan = ftdmchan;
					sngss7_span->tx_cgu.circuit = sngss7_info->circuit->id;
					sngss7_span->tx_cgu.range = 0;
					sngss7_span->tx_cgu.type = 0; /* maintenace block */
				} else {
					((sngss7_span_data_t*)(main_chan->span->signal_data))->tx_cgu.range++;
				}
				
				/* update the status field */
				sngss7_span->tx_cgu.status[byte] = (sngss7_span->tx_cgu.status[byte] | (1 << bit));

				/* update the bit and byte counter*/
				bit ++;
				if (bit == 8) {
					byte++;
					bit = 0;
				}

				/* unlock the channel again before we exit */
				ftdm_channel_unlock(ftdmchan);
			}
		}
		/* go the next circuit */
		x++;
	}

	if (!main_chan) {
		stream->write_function(stream, "Failed to find a voice cic in span %d chan %d range %d", span, chan, range);
		return FTDM_SUCCESS;
	}

	/* send the circuit group block */
	ft_to_sngss7_cgu(main_chan);

	/* clearing blocking flags */
	for (x = 1; ubl_sng_info[x]; x++) {
		sngss7_info = ubl_sng_info[x];
		sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX);
		sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX_DN);
	}

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {

			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->signal_data;

			if ((ftdmchan->physical_span_id == span) && 
				((ftdmchan->physical_chan_id >= chan) && (ftdmchan->physical_chan_id < (chan+range)))) {

				handle_show_status(stream, ftdmchan->physical_span_id, ftdmchan->physical_chan_id, verbose);

			}
		} /* if ( cic == voice) */

		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */
	

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_bind_link(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_bind_mtp3link(g_ftdm_sngss7_data.cfg.mtp3Link[x].mtp2Id)) {
				stream->write_function(stream, "Failed to bind link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_mtp3link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	stream->write_function(stream, "Could not find link=%s\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_unbind_link(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_unbind_mtp3link(g_ftdm_sngss7_data.cfg.mtp3Link[x].mtp2Id)) {
				stream->write_function(stream, "Failed to bind link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_mtp3link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	stream->write_function(stream, "Could not find link=%s\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_activate_link(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_activate_mtp3link(x)) {
				stream->write_function(stream, "Failed to activate link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_mtp3link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	stream->write_function(stream, "Could not find link=%s\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_deactivate_link(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {

			/* send the deactivate request */
			if (ftmod_ss7_deactivate2_mtp3link(x)) {
				stream->write_function(stream, "Failed to deactivate link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_mtp3link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	stream->write_function(stream, "Could not find link=%s\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_activate_linkset(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the linkset request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKSETS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLinkSet[x].name, name)) {

			/* send the activate request */
			if (ftmod_ss7_activate_mtplinkSet(x)) {
				stream->write_function(stream, "Failed to activate linkset=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the linkset */
			handle_status_linkset(stream, &name[0]);
			goto success;
		}
 
		/* move to the next linkset */
		x++;
	} /* while (id != 0) */

	stream->write_function(stream, "Could not find linkset=%s\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_deactivate_linkset(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the linkset request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKSETS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLinkSet[x].name, name)) {

			/* send the deactivate request */
			if (ftmod_ss7_deactivate2_mtplinkSet(x)) {
				stream->write_function(stream, "Failed to deactivate linkset=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the linkset */
			handle_status_linkset(stream, &name[0]);
			goto success;
		}
 
		/* move to the next linkset */
		x++;
	} /* while (id != 0) */

	stream->write_function(stream, "Could not find linkset=%s\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/

static ftdm_status_t handle_tx_lpo(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_lpo_mtp3link(x)) {
				stream->write_function(stream, "Failed set LPO link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_mtp3link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	stream->write_function(stream, "Could not find link=%s\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_tx_lpr(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(x < (MAX_MTP_LINKS+1)) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_lpr_mtp3link(x)) {
				stream->write_function(stream, "Failed set LPR link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_mtp3link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	stream->write_function(stream, "Could not find link=%s\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_status_relay(ftdm_stream_handle_t *stream, char *name)
{
	SS7_RELAY_DBG_FUN(handle_status_relay);
	if (!name) {
		return cli_ss7_show_all_relay(stream);
	}
	return cli_ss7_show_relay_by_name(stream, name);
}

/******************************************************************************/
static ftdm_status_t handle_status_isup_ckt(ftdm_stream_handle_t *stream, char *id_name)
{
	sng_isup_ckt_t				*ckt;
	sngss7_chan_data_t  		*ss7_info;
	ftdm_channel_t	  			*ftdmchan;
	uint32_t					id;
	uint8_t						state = 0;
	uint8_t						bits_ab = 0;
	uint8_t						bits_cd = 0;	
	uint8_t						bits_ef = 0;

	/* extract the integer version of the id (ckt) */
	id = atoi(id_name);

	/* extract the global config circuit structure */
	ckt = &g_ftdm_sngss7_data.cfg.isupCkt[id];

	/* confirm the ckt exists */
	if (ckt->id == 0) {
		stream->write_function(stream, "Requested ckt does not exist (%d)\n", id);
		return FTDM_FAIL;
	}

	/* confirm the ckt is a voice channel */
	if (ckt->type != SNG_CKT_VOICE) {
		stream->write_function(stream, "Requested ckt is a sig link/hole and can not be queried (%d)\n", id);
		return FTDM_FAIL;
	}

	/* extract the global structure */
	ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[id].obj;
	ftdmchan = ss7_info->ftdmchan;

	/* query the isup stack for the state of the ckt */
	if (ftmod_ss7_isup_ckt_sta(ckt->id, &state)) {
		stream->write_function(stream, "Failed to read isup ckt =%d status\n", id);
		return FTDM_FAIL;
	}


	stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|ckt=%4d|state=0x%02X|",
									ckt->span,
									ckt->chan,
									ckt->cic,
									ckt->id,
									state);

	/* extract the bit sections */
	bits_ab = (state & (SNG_BIT_A + SNG_BIT_B)) >> 0;

	bits_cd = (state & (SNG_BIT_C + SNG_BIT_D)) >> 2;

	bits_ef = (state & (SNG_BIT_E + SNG_BIT_F)) >> 4;

	/* check bits C and D */
	switch (bits_cd) {
	/**************************************************************************/
	case (0):
		/* ckt is either un-equipped or transient, check bits A and B */
		switch (bits_ab) {
		/**********************************************************************/
		case (0):
			/* bit a and bit are cleared, transient */
			stream->write_function(stream, "transient\n");
			goto success;
			break;
		/**********************************************************************/
		case (1):
		case (2):
			/* bit a or bit b are set, spare ... shouldn't happen */
			stream->write_function(stream, "spare\n");
			goto success;
			break;
		/**********************************************************************/
		case (3):
			/* bit a and bit b are set, unequipped */
			stream->write_function(stream, "unequipped\n");
			goto success;
			break;
		/**********************************************************************/
		default:
			stream->write_function(stream, "invalid values for bits A and B (%d)\n",
											bits_ab);
			goto success;
			break;
		/**********************************************************************/
		} /* switch (bits_ab) */

		/* shouldn't get here but have a break for properness */
		break;
	/**************************************************************************/
	case (1):
		/* circuit incoming busy */
		stream->write_function(stream, "incoming busy");
		break;
	/**************************************************************************/
	case (2):
		/* circuit outgoing busy */
		stream->write_function(stream, "outgoing busy");
		break;
	/**************************************************************************/
	case (3):
		/* circuit idle */
		stream->write_function(stream, "idle");
		break;
	/**************************************************************************/
	default:
		/* invalid */
		stream->write_function(stream, "bits C and D are invalid (%d)!\n",
										bits_cd);
		goto success;
	/**************************************************************************/
	} /* switch (bits_cd) */

	/* check the maintenance block status in bits A and B */
	switch (bits_ab) {
	/**************************************************************************/
	case (0):
		/* no maintenace block...do nothing */
		break;
	/**************************************************************************/
	case (1):
		/* locally blocked */
		stream->write_function(stream, "|l_mn");
		break;
	/**************************************************************************/
	case (2):
		/* remotely blocked */
		stream->write_function(stream, "|r_mn");
		break;
	/**************************************************************************/
	case (3):
		/* both locally and remotely blocked */
		stream->write_function(stream, "|l_mn|r_mn");
		break;
	/**************************************************************************/
	default:
		stream->write_function(stream, "bits A and B are invlaid (%d)!\n",
										bits_ab);
		goto success;
	/**************************************************************************/
	} /* switch (bits_ab) */

	/* check the hardware block status in bits e and f */
	switch (bits_ef) {
	/**************************************************************************/
	case (0):
		/* no maintenace block...do nothing */
		break;
	/**************************************************************************/
	case (1):
		/* locally blocked */
		stream->write_function(stream, "|l_hw");
		break;
	/**************************************************************************/
	case (2):
		/* remotely blocked */
		stream->write_function(stream, "|r_hw");
		break;
	/**************************************************************************/
	case (3):
		/* both locally and remotely blocked */
		stream->write_function(stream, "|l_hw|r_hw");
		break;
	/**************************************************************************/
	default:
		stream->write_function(stream, "bits E and F are invlaid (%d)!\n",
										bits_ef);
		goto success;
	/**************************************************************************/
	} /* switch (bits_ef) */

success:
	stream->write_function(stream, "\n");
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


/******************************************************************************
* Fun:  cli_ss7_show_mtp2link_by_id()
* Desc: display mtp3 link information with id
* Param: 
*          stream : output stream object
*          rcId     : mtp2 link's id
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_mtp2link_by_id(ftdm_stream_handle_t *stream, int rcId)
{
	SdMngmt sta;

	SS7_RELAY_DBG_FUN(cli_ss7_show_mtp2link_by_id);

	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");

	if (ftmod_ss7_mtp2link_sta(rcId, &sta)) {
		stream->write_function(stream, "Failed to read status of MTP2 link, id=%d \n", rcId);
		return FTDM_FAIL;
	}
    
	stream->write_function(stream, "name=%s|span=%d|chan=%d|sap=%d|state=%s|outsFrm=%d|drpdFrm=%d|lclStatus=%s|rmtStatus=%s|fsn=%d|bsn=%d\n",
						g_ftdm_sngss7_data.cfg.mtp2Link[rcId].name,
						g_ftdm_sngss7_data.cfg.mtp1Link[rcId].span,
						g_ftdm_sngss7_data.cfg.mtp1Link[rcId].chan,
						g_ftdm_sngss7_data.cfg.mtp2Link[rcId].id,
						DECODE_LSD_LINK_STATUS(sta.t.ssta.s.sdDLSAP.hlSt),
						sta.t.ssta.s.sdDLSAP.psOutsFrm,
						sta.t.ssta.s.sdDLSAP.cntMaDrop,
						(sta.t.ssta.s.sdDLSAP.lclBsy) ? "Y":"N",
						(sta.t.ssta.s.sdDLSAP.remBsy) ? "Y":"N",
						sta.t.ssta.s.sdDLSAP.fsn,
						sta.t.ssta.s.sdDLSAP.bsn
					     );

	return FTDM_SUCCESS;
}

/******************************************************************************
* Fun:  cli_ss7_show_mtp2link_by_name()
* Desc: display all relay channels information
* Param: 
*          stream : output stream object
*          rcName: mtp2 link's name
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_mtp2link_by_name(ftdm_stream_handle_t *stream, char *name)
{
	int x = 0;
	SS7_RELAY_DBG_FUN(cli_ss7_show_mtp2link_by_name);

	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	ftdm_assert_return(!ftdm_strlen_zero(name), FTDM_FAIL, "Null MTP2 link name\n");
	
	for (x = 0; x < (MAX_MTP_LINKS + 1); x++) { 
		if (0 == strcasecmp(g_ftdm_sngss7_data.cfg.mtp2Link[x].name, name)) {
			return cli_ss7_show_mtp2link_by_id( stream, x );
		}
	}

	stream->write_function (stream, "The MTP2 link with name \'%s\' is not found. \n", name);
	return FTDM_FAIL;
}

/******************************************************************************
* Fun:  cli_ss7_show_all_mtp2link()
* Desc: display all mtp2 links information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_all_mtp2link(ftdm_stream_handle_t *stream)
{
	int x = 0;

	SS7_RELAY_DBG_FUN(cli_ss7_show_all_mtp2link);

	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	
	for (x = 0; x < (MAX_MTP_LINKS + 1); x++) {
		if (!ftdm_strlen_zero( g_ftdm_sngss7_data.cfg.mtp2Link[x].name)) {
			cli_ss7_show_mtp2link_by_id(stream, x );
		}
	}

	return FTDM_SUCCESS;
}

/******************************************************************************
* Fun:  cli_ss7_show_mtp3link_by_id()
* Desc: display mtp3 link information with id
* Param: 
*          stream : output stream object
*          rcId     : mtp3 link's id
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_mtp3link_by_id(ftdm_stream_handle_t *stream, int rcId)
{
	SnMngmt sta;

	SS7_RELAY_DBG_FUN(cli_ss7_show_mtp3link_by_id);

	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
    
	memset(&sta, 0, sizeof(sta));
	if (ftmod_ss7_mtp3link_sta(rcId, &sta)) {
		stream->write_function(stream, "Failed to read status of MTP3 link, id=%d \n", rcId);
		return FTDM_FAIL;
	}

	stream->write_function(stream, "name=%s|span=%d|chan=%d|sap=%d|state=%s|l_blk=%s|r_blk=%s|l_inhbt=%s|r_inhbt=%s\n",
					g_ftdm_sngss7_data.cfg.mtp3Link[rcId].name,
					g_ftdm_sngss7_data.cfg.mtp1Link[rcId].span,
					g_ftdm_sngss7_data.cfg.mtp1Link[rcId].chan,
					g_ftdm_sngss7_data.cfg.mtp3Link[rcId].id,
					DECODE_LSN_LINK_STATUS(sta.t.ssta.s.snDLSAP.state),
					(sta.t.ssta.s.snDLSAP.locBlkd) ? "Y":"N",
					(sta.t.ssta.s.snDLSAP.remBlkd) ? "Y":"N",
					(sta.t.ssta.s.snDLSAP.locInhbt) ? "Y":"N",
					(sta.t.ssta.s.snDLSAP.rmtInhbt) ? "Y":"N"
			      );

	return FTDM_SUCCESS;    			
}

/******************************************************************************
* Fun:  cli_ss7_show_mtp3link_by_name()
* Desc: display all relay channels information
* Param: 
*          stream : output stream object
*          rcName: mtp3 link's name
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_mtp3link_by_name(ftdm_stream_handle_t *stream, char *name)
{
	int x=0;
	SS7_RELAY_DBG_FUN(cli_ss7_show_mtp3link_by_name);

	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	ftdm_assert_return(!ftdm_strlen_zero(name), FTDM_FAIL, "Null MTP3 link name\n");

	for (x = 0; x < (MAX_MTP_LINKS + 1); x++) { 
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtp3Link[x].name, name)) {
			return cli_ss7_show_mtp3link_by_id(stream, x );
		}
	}

	stream->write_function(stream, "The MTP3 link with name \'%s\' is not found. \n", name);
	return FTDM_FAIL;
}
/******************************************************************************
* Fun:  cli_ss7_show_all_mtp3link()
* Desc: display all mtp3 links information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_all_mtp3link(ftdm_stream_handle_t *stream)
{
	int x = 0;

	SS7_RELAY_DBG_FUN(cli_ss7_show_all_mtp3link);
	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	
	for (x = 0; x < (MAX_MTP_LINKS + 1); x++) {
		if (!ftdm_strlen_zero(g_ftdm_sngss7_data.cfg.mtp3Link[x].name)) {
			cli_ss7_show_mtp3link_by_id(stream, x);
		}
	}

	return FTDM_SUCCESS;
}


/******************************************************************************
* Fun:  cli_ss7_show_all_linkset()
* Desc: display all mtp3 linksets information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_all_linkset(ftdm_stream_handle_t *stream)
{
	int x = 0;
	SnMngmt	sta;

	SS7_RELAY_DBG_FUN(cli_ss7_show_all_linkset);
	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");

	x = 1;
	while(x < (MAX_MTP_LINKSETS+1)) {
		if (!ftdm_strlen_zero(g_ftdm_sngss7_data.cfg.mtpLinkSet[x].name)) {
			if (ftmod_ss7_mtplinkSet_sta(x, &sta)) {
				stream->write_function(stream, "Failed to read linkset=%s status\n", g_ftdm_sngss7_data.cfg.mtpLinkSet[x].name);
			} else {
				stream->write_function(stream, "name=%s|state=%s|nmbActLnk=%d\n", 
								g_ftdm_sngss7_data.cfg.mtpLinkSet[x].name, 
								DECODE_LSN_LINKSET_STATUS(sta.t.ssta.s.snLnkSet.state), sta.t.ssta.s.snLnkSet.nmbActLnks
				   		      );
			}
		}
		x++;
	}
	return FTDM_SUCCESS;
}


/******************************************************************************
* Fun:  cli_ss7_show_general()
* Desc: display all general information about ss7
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_general(ftdm_stream_handle_t *stream)
{
	SS7_RELAY_DBG_FUN(cli_ss7_show_general);

	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");

	stream->write_function(stream, "MTP2 status: \n");
	cli_ss7_show_all_mtp2link(stream);

	if(SNG_SS7_OPR_MODE_M2UA_SG != g_ftdm_operating_mode){
		stream->write_function(stream, "\nMTP3 status: \n");
		cli_ss7_show_all_mtp3link(stream);

		stream->write_function(stream, "\nMTP3 linkset status: \n");
		cli_ss7_show_all_linkset(stream);

#if 0
		stream->write_function(stream, "\nMTP3 link route status: \n");

		stream->write_function(stream, "\nISUP status: \n");
#endif

		stream->write_function(stream, "\nRelay status: \n");
		cli_ss7_show_all_relay(stream);
	}
	
	return FTDM_SUCCESS;
}

/******************************************************************************
* Fun:  cli_ss7_show_relay_by_id()
* Desc: display all relay channels information
* Param: 
*          stream : output stream object
*          rcId     : channel's id
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_relay_by_id(ftdm_stream_handle_t *stream, int rcId)
{	
	RyMngmt	sta;

	SS7_RELAY_DBG_FUN(cli_ss7_show_relay_by_id);
	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
		
	memset(&sta, 0x0, sizeof(sta));
	if (ftmod_ss7_relay_status(g_ftdm_sngss7_data.cfg.relay[rcId].id, &sta)) {
		stream->write_function(stream, "Failed to read relay =%s status\n", g_ftdm_sngss7_data.cfg.relay[rcId].name);
		return FTDM_FAIL;
	}	

	stream->write_function(stream, "name=%s|sap=%d|type=%d|port=%d|hostname=%s|procId=%d|status=%s\n",
							g_ftdm_sngss7_data.cfg.relay[rcId].name,
							g_ftdm_sngss7_data.cfg.relay[rcId].id,	
							g_ftdm_sngss7_data.cfg.relay[rcId].type,
							g_ftdm_sngss7_data.cfg.relay[rcId].port,
							g_ftdm_sngss7_data.cfg.relay[rcId].hostname,
							g_ftdm_sngss7_data.cfg.relay[rcId].procId,
							DECODE_LRY_CHAN_STATUS(sta.t.ssta.rySta.cStatus)
						);
		
	return FTDM_SUCCESS;
}
/******************************************************************************
* Fun:  cli_ss7_show_relay_by_name()
* Desc: display all relay channels information
* Param: 
*          stream : output stream object
*          rcName: channel's name
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_relay_by_name(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;
	
	SS7_RELAY_DBG_FUN(cli_ss7_show_relay_by_name);
	 
	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	ftdm_assert_return(!ftdm_strlen_zero(name), FTDM_FAIL, "Null relay link name\n");

	for (x = 1; x < MAX_RELAY_CHANNELS; x++) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.relay[x].name, name)) {
			return cli_ss7_show_relay_by_id(stream, x);
		}
	}

	stream->write_function( stream, "The relay channel with name \'%s\' is not found. \n", name);
	return FTDM_FAIL;
	
}
/******************************************************************************
* Fun:  cli_ss7_show_all_relay()
* Desc: display all relay channels information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_all_relay(ftdm_stream_handle_t *stream)
{
	int x = 0;
	SS7_RELAY_DBG_FUN(cli_ss7_show_relay_by_name);

	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	
	for (x = 1; x < MAX_RELAY_CHANNELS; x++) {
		if (!ftdm_strlen_zero(g_ftdm_sngss7_data.cfg.relay[x].name)) {
			cli_ss7_show_relay_by_id (stream, x);
		}
	}
		
	return FTDM_SUCCESS;
}


/******************************************************************************
* Fun:  cli_ss7_show_channel_detail_of_span()
* Desc: display span information of a given id
* Param: 
*          stream : output stream object
*          span_id : span id string received from cli
*          chan_id : channel id string received from cli
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_channel_detail_of_span(ftdm_stream_handle_t *stream, char *span_id, char *chan_id)
{	
	int x, y;

	SS7_RELAY_DBG_FUN(cli_ss7_show_channel_detail_of_span);
	
	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	ftdm_assert_return(span_id != 0, FTDM_FAIL, "Invalid span id\n");
	ftdm_assert_return(chan_id != 0, FTDM_FAIL, "Invalid chan id\n");

	x = atoi(span_id); 
	y = atoi(chan_id);
	if (!x) {
		stream->write_function( stream, "Span \'%s\' does not exist. \n", span_id);
		return FTDM_FAIL;
	}

	return handle_show_status(stream, x, y, 1);
}

/******************************************************************************
* Fun:  cli_ss7_show_all_channels_of_span()
* Desc: display span information of a given id
* Param: 
*          stream : output stream object
*          span_id : span id string received from cli
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_all_channels_of_span(ftdm_stream_handle_t *stream, char *span_id)
{	
	int x=-1; 
	SS7_RELAY_DBG_FUN(cli_ss7_show_all_channels_of_span);

	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	ftdm_assert_return(span_id != 0, FTDM_FAIL, "Invalid span id\n");

	x = atoi(span_id);
	if (!x) {
		stream->write_function( stream, "Span \'%s\' does not exist. \n", span_id);
		return FTDM_FAIL;
	}
	return handle_show_status(stream, x, 0, 1);
}

/******************************************************************************
* Fun:  cli_ss7_show_span_by_id()
* Desc: display span information of a given id
* Param: 
*          stream : output stream object
*          span_id : span id string received from cli
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_span_by_id(ftdm_stream_handle_t *stream, char *span_id)
{	
	int x = -1; 

	SS7_RELAY_DBG_FUN(cli_ss7_show_span_by_id);

	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	ftdm_assert_return(span_id != 0, FTDM_FAIL, "Invalid span id\n");

	x = atoi(span_id);
	if (!x) {
		stream->write_function(stream, "Span \'%s\' does not exist. \n", span_id);
		return FTDM_FAIL;
	}

#if 0
	stream->write_function( stream, "JZ: we should display span details here \n" );
#endif

	cli_ss7_show_all_channels_of_span(stream, span_id);
	
	return FTDM_FAIL;
}


/******************************************************************************
* Fun:  cli_ss7_show_all_spans_detail()
* Desc: display all spans information in detail
* Param: 
*          stream : output stream object
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_all_spans_detail(ftdm_stream_handle_t *stream)
{
	SS7_RELAY_DBG_FUN(cli_ss7_show_all_spans_detail);
	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	return handle_show_status(stream, 0, 0, 1);
}

/******************************************************************************
* Fun:  cli_ss7_show_all_spans_general()
* Desc: display all spans information in general
* Param: 
*          stream : output stream object
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: James Zhang
*******************************************************************************/
static ftdm_status_t cli_ss7_show_all_spans_general(ftdm_stream_handle_t *stream)
{
	SS7_RELAY_DBG_FUN(cli_ss7_show_all_spans_general);
	ftdm_assert_return(stream != NULL, FTDM_FAIL, "Null stream\n");
	return FTDM_FAIL;
}


/******************************************************************************
* Fun:  handle_show_m2ua_profiles()
* Desc: display all m2ua profile information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: Kapil Gupta
*******************************************************************************/

static ftdm_status_t handle_show_m2ua_profiles(ftdm_stream_handle_t *stream)
{
	MwMgmt cfm;
	MwMgmt rsp;
	char  buf[2048];
	char*  xmlhdr = (char*)"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	int x = 0x00;
	int idx = 0x00;
	int len = 0x00;

	memset((U8 *)&cfm, 0, sizeof(MwMgmt));
	memset((U8 *)&rsp, 0, sizeof(MwMgmt));
	memset(&buf[0], 0, sizeof(buf));

	len = len + sprintf(buf + len, "%s\n", xmlhdr);
	len = len + sprintf(buf + len, "<m2ua_profiles>\n");

	if(ftmod_m2ua_ssta_req(STMWGEN, 0x00, &cfm)) {
		stream->write_function(stream," Request to  layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<m2ua_gen>\n");
#ifdef BIT_64		
		len = len + sprintf(buf + len, "<mem_size> %d </mem_size>\n", cfm.t.ssta.s.genSta.memSize);
		len = len + sprintf(buf + len, " <allocated_mem_size> %d </allocated_mem_size>\n", cfm.t.ssta.s.genSta.memAlloc);
#else
		len = len + sprintf(buf + len, "<mem_size> %ld </mem_size>\n", cfm.t.ssta.s.genSta.memSize);
		len = len + sprintf(buf + len, " <allocated_mem_size> %ld </allocated_mem_size>\n", cfm.t.ssta.s.genSta.memAlloc);
#endif
		len = len + sprintf(buf + len, " <num_of_cluster> %d </num_of_cluster>\n", cfm.t.ssta.s.genSta.nmbClusters);
		len = len + sprintf(buf + len, " <num_of_peers> %d </num_of_peers>\n", cfm.t.ssta.s.genSta.nmbPeers);
		len = len + sprintf(buf + len, " <num_of_interfaces> %d </num_of_interfaces>\n", cfm.t.ssta.s.genSta.nmbIntf);
		len = len + sprintf(buf + len, "</m2ua_gen>\n");
	}

	/*iterate through all the m2ua links and prints all information */
	 x = 1;
	 while(x<MW_MAX_NUM_OF_INTF){
		 if((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].id !=0) &&
				 ((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].flags & SNGSS7_ACTIVE))) {

			 memset((U8 *)&cfm, 0, sizeof(MwMgmt));

			 len = len + sprintf(buf + len, "<m2ua_profile>\n");
			 len = len + sprintf(buf + len, "<name> %s </name>\n", g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].name);

			 if(ftmod_m2ua_ssta_req(STMWDLSAP,x,&cfm)) {
				 stream->write_function(stream," Request to M2UA  layer failed \n");
				 return FTDM_FAIL;
			 } else {
				 len = len + sprintf(buf + len, "<m2ua_dlsap>\n");
				 len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_SAP_STATE(cfm.t.ssta.s.dlSapSta.state));
				 len = len + sprintf(buf + len," <link_state> %s </link_state>\n", PRNT_M2UA_LINK_STATE(cfm.t.ssta.s.dlSapSta.lnkState));
				 len = len + sprintf(buf + len," <rpo_enable> %d </rpo_enable>\n", cfm.t.ssta.s.dlSapSta.rpoEnable);
				 len = len + sprintf(buf + len," <lpo_enable> %d </lpo_enable>\n", cfm.t.ssta.s.dlSapSta.lpoEnable);
				 len = len + sprintf(buf + len," <congestion_level> %d </congestion_level>\n", cfm.t.ssta.s.dlSapSta.congLevel);
				 len = len + sprintf(buf + len, "</m2ua_dlsap>\n");
			 }

			 memset((U8 *)&cfm, 0, sizeof(MwMgmt));
			 if(ftmod_m2ua_ssta_req(STMWCLUSTER,g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].clusterId,&cfm)) {
				 stream->write_function(stream," Request to  M2UA layer failed \n");
				 return FTDM_FAIL;
			 } else {
				 len = len + sprintf(buf + len, "<m2ua_cluster>\n");
				 len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_CLUSTER_STATE(cfm.t.ssta.s.clusterSta.state));
				 len = len + sprintf(buf + len, " <num_of_peers> %d </num_of_peers>\n",cfm.t.ssta.s.clusterSta.nmbPeer);
				 len = len + sprintf(buf + len, "<m2ua_cluster_peer>\n");
				 for(idx = 0; idx < cfm.t.ssta.s.clusterSta.nmbPeer; idx++)
				 {
					 len = len + sprintf(buf + len, " <peer_id> %d </peer_id>\n", cfm.t.ssta.s.clusterSta.peerSt[idx].peerId);
					 len = len + sprintf(buf + len, " <peer_state> %s </peer_state>\n",  PRNT_M2UA_PEER_STATE(cfm.t.ssta.s.clusterSta.peerSt[idx].peerState));
				 }
				 len = len + sprintf(buf + len, "</m2ua_cluster_peer>\n");
				 len = len + sprintf(buf + len, "<num_active_peer> %d </num_active_peer>\n",cfm.t.ssta.s.clusterSta.nmbActPeer);

				 len = len + sprintf(buf + len, "</m2ua_cluster>\n");

				 memset((U8 *)&rsp, 0, sizeof(MwMgmt));
				 memcpy(&rsp, &cfm, sizeof(MwMgmt));


				 /* loop through configured peers */
				 for(idx = 0; idx < rsp.t.ssta.s.clusterSta.nmbPeer; idx++)
				 {
					 int peer_id = rsp.t.ssta.s.clusterSta.peerSt[idx].peerId;
					 
					 memset(&cfm, 0, sizeof(MwMgmt));

					 if(LMW_PEER_DOWN != rsp.t.ssta.s.clusterSta.peerSt[idx].peerState){

						 if(ftmod_m2ua_ssta_req(STMWPEER, peer_id, &cfm)) {
							 stream->write_function(stream," Request to M2UA  layer failed \n");
							 return FTDM_FAIL;
						 } else {
							 len = len + sprintf(buf + len, "<m2ua_peer>\n");
							 len = len + sprintf(buf + len, "<name> %s </name>\n",g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[peer_id].name);
							 len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_PEER_STATE(cfm.t.ssta.s.peerSta.state));
							 len = len + sprintf(buf + len, " <retry_count> %d </retry_count>\n",cfm.t.ssta.s.peerSta.retryCount);
							 len = len + sprintf(buf + len, " <assoc_id> %d </assoc_id>\n", (int)cfm.t.ssta.s.peerSta.assocSta.spAssocId);
							 len = len + sprintf(buf + len, " <connected_status> %s </connected_status>\n",(cfm.t.ssta.s.peerSta.assocSta.connected)?"CONNECTED":"NOT CONNECTED");
							 len = len + sprintf(buf + len, " <flow_cntrl_progress> %d </flow_cntrl_progress>\n",cfm.t.ssta.s.peerSta.assocSta.flcInProg);
							 len = len + sprintf(buf + len, " <flow_cntrl_level> %d </flow_cntrl_level>\n",cfm.t.ssta.s.peerSta.assocSta.flcLevel);
							 len = len + sprintf(buf + len, " <hearbeat_status> %d </hearbeat_status>\n",cfm.t.ssta.s.peerSta.assocSta.sctpHBeatEnb);
							 len = len + sprintf(buf + len, " <nmb_of_stream> %d </nmb_of_stream>\n",cfm.t.ssta.s.peerSta.assocSta.locOutStrms);

							 len = len + sprintf(buf + len, "</m2ua_peer>\n");
						 }
					 } else {
							 len = len + sprintf(buf + len, "<m2ua_peer>\n");
							 len = len + sprintf(buf + len, "<name> %s </name>\n",g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[peer_id].name);
							 len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_PEER_STATE(rsp.t.ssta.s.clusterSta.peerSt[idx].peerState));
							 len = len + sprintf(buf + len, "</m2ua_peer>\n");
					 }
				 }
			 }

			 memset((U8 *)&cfm, 0, sizeof(MwMgmt));
			 if(ftmod_m2ua_ssta_req(STMWSCTSAP,x,&cfm)) {
				 stream->write_function(stream," Request to M2UA layer failed \n");
				 return FTDM_FAIL;
			 } else {
				 len = len + sprintf(buf + len, "<m2ua_sctp_sap>\n");
				 len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_SAP_STATE(cfm.t.ssta.s.sctSapSta.state));
				 len = len + sprintf(buf + len," <end_point_open_state> %s </end_point_open_state>\n", (cfm.t.ssta.s.sctSapSta.endpOpen)?"END_POINT_OPENED_SUCCESSFULLY":"END_POINT_NOT_OPEN");
				 len = len + sprintf(buf + len," <end_point_id> %d </end_point_id>\n", (int) cfm.t.ssta.s.sctSapSta.spEndpId);
				 len = len + sprintf(buf + len," <nmb_of_retry_attemp> %d </nmb_of_retry_attemp>\n", cfm.t.ssta.s.sctSapSta.nmbPrimRetry);
				 len = len + sprintf(buf + len, "</m2ua_sctp_sap>\n");
			 }

			 len = len + sprintf(buf + len, "</m2ua_profile>\n");
		 }
		 x++;
	 }

	len = len + sprintf(buf + len, "</m2ua_profiles>\n");
	stream->write_function(stream,"\n%s\n",buf); 

	return FTDM_SUCCESS;

}

/******************************************************************************
* Fun:  handle_show_m2ua_profile()
* Desc: display requested m2ua profile information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: Kapil Gupta
*******************************************************************************/

static ftdm_status_t handle_show_m2ua_profile(ftdm_stream_handle_t *stream, char* m2ua_profile_name) 
{
	char*  xmlhdr = (char*)"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	char  buf[4096];
	int x = 0x00;
	int idx = 0x00;
	int found = 0x00;
	int len = 0x00;
	MwMgmt cfm;
	MwMgmt rsp;

	memset((U8 *)&cfm, 0, sizeof(MwMgmt));
	memset((U8 *)&rsp, 0, sizeof(MwMgmt));
	memset(&buf[0], 0, sizeof(buf));

	len = len + sprintf(buf + len, "%s\n", xmlhdr);

	/*iterate through all the m2ua links and get required profile */
	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].id !=0) &&
				((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].flags & SNGSS7_ACTIVE))) {

			if(!strcasecmp(m2ua_profile_name, g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].name)){
				found = 0x01;
				break;
			}
		}
		x++;
	}

	if(!found){
		stream->write_function(stream,"Requested M2UA profile[%s] not configured\n", m2ua_profile_name);
		return FTDM_FAIL;
	}


	len = len + sprintf(buf + len, "<m2ua_profile>\n");
	len = len + sprintf(buf + len, "<name> %s </name>\n", m2ua_profile_name);

	if(ftmod_m2ua_ssta_req(STMWDLSAP,x,&cfm)) {
		stream->write_function(stream," Request to M2UA layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<m2ua_dlsap>\n");
		len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_SAP_STATE(cfm.t.ssta.s.dlSapSta.state));
		len = len + sprintf(buf + len," <link_state> %s </link_state>\n", PRNT_M2UA_LINK_STATE(cfm.t.ssta.s.dlSapSta.lnkState));
		len = len + sprintf(buf + len," <rpo_enable> %d </rpo_enable>\n", cfm.t.ssta.s.dlSapSta.rpoEnable);
		len = len + sprintf(buf + len," <lpo_enable> %d </lpo_enable>\n", cfm.t.ssta.s.dlSapSta.lpoEnable);
		len = len + sprintf(buf + len," <congestion_level> %d </congestion_level>\n", cfm.t.ssta.s.dlSapSta.congLevel);
		len = len + sprintf(buf + len, "</m2ua_dlsap>\n");
	}

	if(ftmod_m2ua_ssta_req(STMWCLUSTER, g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].clusterId, &cfm)) {
		stream->write_function(stream," Request to M2UA layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<m2ua_cluster>\n");
		len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_CLUSTER_STATE(cfm.t.ssta.s.clusterSta.state));
		len = len + sprintf(buf + len, " <num_of_peers> %d </num_of_peers>\n",cfm.t.ssta.s.clusterSta.nmbPeer);
		len = len + sprintf(buf + len, "<m2ua_cluster_peer>\n");
		for(idx = 0; idx < cfm.t.ssta.s.clusterSta.nmbPeer; idx++)
		{
			len = len + sprintf(buf + len, " <peer_id> %d </peer_id>\n", cfm.t.ssta.s.clusterSta.peerSt[idx].peerId);
			len = len + sprintf(buf + len, " <peer_state> %s </peer_state>\n",  PRNT_M2UA_PEER_STATE(cfm.t.ssta.s.clusterSta.peerSt[idx].peerState));
		}
		len = len + sprintf(buf + len, "</m2ua_cluster_peer>\n");
		len = len + sprintf(buf + len, "<num_active_peer> %d </num_active_peer>\n",cfm.t.ssta.s.clusterSta.nmbActPeer);

		len = len + sprintf(buf + len, "</m2ua_cluster>\n");
	}

	memcpy((U8 *)&rsp, &cfm, sizeof(MwMgmt));

	/* loop through configured peers */
	for(idx = 0; idx < rsp.t.ssta.s.clusterSta.nmbPeer; idx++)
	{
		memset((U8 *)&cfm, 0, sizeof(MwMgmt));

		if(ftmod_m2ua_ssta_req(STMWPEER, rsp.t.ssta.s.clusterSta.peerSt[idx].peerId, &cfm)) {
			stream->write_function(stream," Request to M2UA layer failed \n");
			return FTDM_FAIL;
		} else {
			len = len + sprintf(buf + len, "<m2ua_peer>\n");
			len = len + sprintf(buf + len, "<name> %s </name>\n",g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[rsp.t.ssta.s.clusterSta.peerSt[idx].peerId].name);
			len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_PEER_STATE(cfm.t.ssta.s.peerSta.state));
			len = len + sprintf(buf + len, " <retry_count> %d </retry_count>\n",cfm.t.ssta.s.peerSta.retryCount);
#ifdef BIT_64
			len = len + sprintf(buf + len, " <assoc_id> %d </assoc_id>\n", cfm.t.ssta.s.peerSta.assocSta.spAssocId);
#else
			len = len + sprintf(buf + len, " <assoc_id> %ld </assoc_id>\n", cfm.t.ssta.s.peerSta.assocSta.spAssocId);
#endif
			len = len + sprintf(buf + len, " <connected_status> %s </connected_status>\n",(cfm.t.ssta.s.peerSta.assocSta.connected)?"CONNECTED":"NOT CONNECTED");
			len = len + sprintf(buf + len, " <flow_cntrl_progress> %d </flow_cntrl_progress>\n",cfm.t.ssta.s.peerSta.assocSta.flcInProg);
			len = len + sprintf(buf + len, " <flow_cntrl_level> %d </flow_cntrl_level>\n",cfm.t.ssta.s.peerSta.assocSta.flcLevel);
			len = len + sprintf(buf + len, " <hearbeat_status> %d </hearbeat_status>\n",cfm.t.ssta.s.peerSta.assocSta.sctpHBeatEnb);
			len = len + sprintf(buf + len, " <nmb_of_stream> %d </nmb_of_stream>\n",cfm.t.ssta.s.peerSta.assocSta.locOutStrms);

			len = len + sprintf(buf + len, "</m2ua_peer>\n");
		}
	}

	if(ftmod_m2ua_ssta_req(STMWSCTSAP,x,&cfm)) {
		stream->write_function(stream," Request to M2UA layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<m2ua_sctp_sap>\n");
		len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_SAP_STATE(cfm.t.ssta.s.sctSapSta.state));
		len = len + sprintf(buf + len," <end_point_open_state> %s </end_point_open_state>\n", (cfm.t.ssta.s.sctSapSta.endpOpen)?"END_POINT_OPENED_SUCCESSFULLY":"END_POINT_NOT_OPEN");
#ifdef BIT_64
		len = len + sprintf(buf + len," <end_point_id> %d </end_point_id>\n", cfm.t.ssta.s.sctSapSta.spEndpId);
#else
		len = len + sprintf(buf + len," <end_point_id> %ld </end_point_id>\n", cfm.t.ssta.s.sctSapSta.spEndpId);
#endif
		len = len + sprintf(buf + len," <nmb_of_retry_attemp> %d </nmb_of_retry_attemp>\n", cfm.t.ssta.s.sctSapSta.nmbPrimRetry);
		len = len + sprintf(buf + len, "</m2ua_sctp_sap>\n");
	}

	len = len + sprintf(buf + len, "</m2ua_profile>\n");

	stream->write_function(stream,"\n%s\n",buf); 

	return FTDM_SUCCESS;

}

/******************************************************************************
* Fun:  handle_show_sctp_profiles()
* Desc: display all sctp profile information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: Kapil Gupta
*******************************************************************************/
static ftdm_status_t handle_show_sctp_profiles(ftdm_stream_handle_t *stream)
{
	char*  xmlhdr = (char*)"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	char  buf[4096];
	int x = 0x00;
	int len = 0x00;
	SbMgmt cfm;

	memset((U8 *)&cfm, 0, sizeof(SbMgmt));
	memset(&buf[0], 0, sizeof(buf));

	len = len + sprintf(buf + len, "%s\n", xmlhdr);
	len = len + sprintf(buf + len, "<sctp_profiles>\n");

	if(ftmod_sctp_ssta_req(STSBGEN, 0x00, &cfm)) {
		stream->write_function(stream," Request to  SCTP layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<sctp_gen>\n");
#ifdef BIT_64
		len = len + sprintf(buf + len, "<mem_size> %d </mem_size>\n",cfm.t.ssta.s.genSta.memSize);
		len = len + sprintf(buf + len, " <allocated_mem_size> %d </allocated_mem_size>\n",cfm.t.ssta.s.genSta.memAlloc);
#else
		len = len + sprintf(buf + len, "<mem_size> %ld </mem_size>\n",cfm.t.ssta.s.genSta.memSize);
		len = len + sprintf(buf + len, " <allocated_mem_size> %ld </allocated_mem_size>\n",cfm.t.ssta.s.genSta.memAlloc);
#endif
		len = len + sprintf(buf + len, " <num_of_open_assoc> %d </num_of_open_assoc>\n",cfm.t.ssta.s.genSta.nmbAssoc);
		len = len + sprintf(buf + len, " <num_of_open_end_points> %d </num_of_open_end_points>\n",cfm.t.ssta.s.genSta.nmbEndp);
		len = len + sprintf(buf + len, " <num_of_lcl_addr_in_use> %d </num_of_lcl_addr_in_use>\n",cfm.t.ssta.s.genSta.nmbLocalAddr);
		len = len + sprintf(buf + len, " <num_of_rmt_addr_in_use> %d </num_of_rmt_addr_in_use>\n",cfm.t.ssta.s.genSta.nmbPeerAddr);
		len = len + sprintf(buf + len, "</sctp_gen>\n");
	}

#ifdef LSB12
	if(ftmod_sctp_ssta_req(STSBTMR, 0x00, &cfm)) {
		stream->write_function(stream," Request to  SCTP layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<sctp_timers>\n");

		len = len + sprintf(buf + len, "<life_time_timer_val> %d </life_time_timer_val>\n", cfm.t.ssta.s.tmrSta.lifetimeTmr);
		len = len + sprintf(buf + len, "<ack_delay_timer_val> %d </ack_delay_timer_val>\n", cfm.t.ssta.s.tmrSta.ackDelayTmr);
		len = len + sprintf(buf + len, "<cookie_timer_val> %d </cookie_timer_val>\n", cfm.t.ssta.s.tmrSta.cookieTmr);
		len = len + sprintf(buf + len, "<key_timer_val> %d </key_timer_val>\n", cfm.t.ssta.s.tmrSta.keyTmr);
		len = len + sprintf(buf + len, "<freeze_timer_val> %d </freeze_timer_val> \n", cfm.t.ssta.s.tmrSta.freezeTmr);
#ifdef LSB4
		len = len + sprintf(buf + len, "<bundle_timer_val> %d </bundle_timer_val> \n", cfm.t.ssta.s.tmrSta.bundleTmr);
#endif
		len = len + sprintf(buf + len, "<t1_init_timer_val> %d </t1_init_timer_val> \n", cfm.t.ssta.s.tmrSta.t1InitTmr);
		len = len + sprintf(buf + len, "<t2_shutdown_timer_val> %d </t2_shutdown_timer_val> \n", cfm.t.ssta.s.tmrSta.t2ShutdownTmr);
		len = len + sprintf(buf + len, "<round_trip_timer_val> %d </round_trip_timer_val> \n", cfm.t.ssta.s.tmrSta.hbeat);
		len = len + sprintf(buf + len, "<t3_rtx_timer_val> %d </t3_rtx_timer_val> \n", cfm.t.ssta.s.tmrSta.t3rtx);
		len = len + sprintf(buf + len, "<bind_retry_timer_val> %d </bind_retry_timer_val> \n", cfm.t.ssta.s.tmrSta.tIntTmr);
	}

#endif


	/*iterate through all the sctp links and prints all information */
	x = 1;
	while(x<MAX_SCTP_LINK){
		if((g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].id !=0) &&
				((g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].flags & SNGSS7_ACTIVE))) {

			len = len + sprintf(buf + len, "<sctp_profile>\n");

			if(ftmod_sctp_ssta_req(STSBSCTSAP,x,&cfm)) {
				stream->write_function(stream," Request to  SCTP layer failed \n");
				return FTDM_FAIL;
			} else {
				len = len + sprintf(buf + len, "<sctp_sap>\n");
				len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_SCTP_SAP_STATE(cfm.t.ssta.s.sapSta.hlSt));
				len = len + sprintf(buf + len," <switch> %s </switch>\n", PRNT_SCTP_PROTO_SWITCH(cfm.t.ssta.s.sapSta.swtch));
				len = len + sprintf(buf + len, "</sctp_sap>\n");
			}

			if(ftmod_sctp_ssta_req(STSBTSAP,x,&cfm)) {
				stream->write_function(stream," Request to  SCTP layer failed \n");
				return FTDM_FAIL;
			} else {
				len = len + sprintf(buf + len, "<sctp_transport_sap>\n");
				len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_SCTP_SAP_STATE(cfm.t.ssta.s.sapSta.hlSt));
				len = len + sprintf(buf + len," <switch> %s </switch>\n", PRNT_SCTP_PROTO_SWITCH(cfm.t.ssta.s.sapSta.swtch));
				len = len + sprintf(buf + len, "</sctp_transport_sap>\n");
			}

			if(ftmod_sctp_ssta_req(STSBASSOC,x,&cfm)) {
				if(LCM_REASON_INVALID_PAR_VAL == cfm.cfm.reason){
					len = len + sprintf(buf + len, "<sctp_association>\n");
					len = len + sprintf(buf + len, " <status> SCT_ASSOC_STATE_CLOSED </status>\n");
					len = len + sprintf(buf + len, "</sctp_association>\n");
				}else{
					stream->write_function(stream," Request to  SCTP layer failed \n");
					return FTDM_FAIL;
				}
			} else {
				len = len + sprintf(buf + len, "<sctp_association>\n");
				len = len + get_assoc_resp_buf(buf + len, &cfm);
				len = len + sprintf(buf + len, "</sctp_association>\n");
			}

			/* TODO - STSBDTA */

			len = len + sprintf(buf + len, "</sctp_profile>\n");
		}
		x++;
	}

	len = len + sprintf(buf + len, "</sctp_profiles>\n");
	stream->write_function(stream,"\n%s\n",buf); 

	return FTDM_SUCCESS;
}

int get_assoc_resp_buf(char* buf,SbMgmt* cfm)
{
	int len = 0x00;
	int idx = 0x00;
	char *asciiAddr;
	CmInetIpAddr ip;

#ifdef BIT_64
	len = len + sprintf(buf + len, " <assoc_id> %d </assoc_id>\n", cfm->t.ssta.s.assocSta.assocId);
#else
	len = len + sprintf(buf + len, " <assoc_id> %ld </assoc_id>\n", cfm->t.ssta.s.assocSta.assocId);
#endif
	len = len + sprintf(buf + len, " <assoc_status> %s </assoc_status>\n", PRNT_SCTP_ASSOC_STATE(cfm->t.ssta.s.assocSta.assocState));
	len = len + sprintf(buf + len, " <assoc_dst_port> %d </assoc_dst_port>\n", cfm->t.ssta.s.assocSta.dstPort);
	len = len + sprintf(buf + len, " <assoc_src_port> %d </assoc_src_port>\n", cfm->t.ssta.s.assocSta.srcPort);
	len = len + sprintf(buf + len, " <nmb_dst_addr> %d </nmb_dst_addr>\n", cfm->t.ssta.s.assocSta.dstNAddrLst.nmb);
	for(idx =0; idx < cfm->t.ssta.s.assocSta.dstNAddrLst.nmb; idx++)
	{
		len = len + sprintf(buf + len, " <dst_addr_list> \n");
		len = len + sprintf(buf + len, " <dst_addr_type> %s </dst_addr_type>\n", PRNT_CM_ADDR_TYPE(cfm->t.ssta.s.assocSta.dstNAddrLst.nAddr[idx].type));
		if(cfm->t.ssta.s.assocSta.dstNAddrLst.nAddr[idx].type == CM_IPV4ADDR_TYPE)
		{
			ip = ntohl(cfm->t.ssta.s.assocSta.dstNAddrLst.nAddr[idx].u.ipv4NetAddr);
			cmInetNtoa(ip, &asciiAddr);
			len = len + sprintf(buf + len, " <dst_addr> %s </dst_addr>\n",asciiAddr); 
		}
		else
		{
			len = len + sprintf(buf + len, " <dst_addr> %s </dst_addr> \n", cfm->t.ssta.s.assocSta.dstNAddrLst.nAddr[idx].u.ipv6NetAddr);
		}
		len = len + sprintf(buf + len, " </dst_addr_list> \n");
	}

	len = len + sprintf(buf + len, " <nmb_src_addr> %d </nmb_src_addr> \n", cfm->t.ssta.s.assocSta.srcNAddrLst.nmb);
	for(idx =0; idx < cfm->t.ssta.s.assocSta.srcNAddrLst.nmb; idx++)
	{
		len = len + sprintf(buf + len, " <src_addr_list> \n");
		len = len + sprintf(buf + len, " <src_addr_type> %s </src_addr_type>\n", PRNT_CM_ADDR_TYPE(cfm->t.ssta.s.assocSta.srcNAddrLst.nAddr[idx].type));
		if(cfm->t.ssta.s.assocSta.srcNAddrLst.nAddr[idx].type == CM_IPV4ADDR_TYPE)
		{
			ip = ntohl(cfm->t.ssta.s.assocSta.srcNAddrLst.nAddr[idx].u.ipv4NetAddr);
			cmInetNtoa(ip, &asciiAddr);
			len = len + sprintf(buf + len, " <src_addr> %s </src_addr>\n", asciiAddr); 
		}
		else
		{
			len = len + sprintf(buf + len, " <src_addr> %s </src_addr>\n", cfm->t.ssta.s.assocSta.srcNAddrLst.nAddr[idx].u.ipv6NetAddr);
		}
		len = len + sprintf(buf + len, " </src_addr_list> \n");
	}

	len = len + sprintf(buf + len, "\n <primary_addr_type> %s </primary_addr_type>\n", PRNT_CM_ADDR_TYPE(cfm->t.ssta.s.assocSta.priNAddr.type));

	if(cfm->t.ssta.s.assocSta.priNAddr.type == CM_IPV4ADDR_TYPE)
	{
		ip = ntohl(cfm->t.ssta.s.assocSta.priNAddr.u.ipv4NetAddr);
		cmInetNtoa(ip, &asciiAddr);
		len = len + sprintf(buf + len, " <primary_addr> %s </primary_addr>\n",asciiAddr); 
	}
	else
	{
		len = len + sprintf(buf + len, " <primary_addr> %s </primary_addr>\n", cfm->t.ssta.s.assocSta.priNAddr.u.ipv6NetAddr);
	}

#ifdef LSB11
	/* TODO - this flag is not enable as of now.. so later on will convert below prints to XML tags */
	len = len + sprintf(buf + len, " The number of unsent datagrams : %d\n", cfm->t.ssta.s.assocSta.nmbUnsentDgms);
	len = len + sprintf(buf + len, " The number of unack datagrams : %d\n", cfm->t.ssta.s.assocSta.nmbUnackDgms);
	len = len + sprintf(buf + len, " The number of undelivered datagrams : %d\n", cfm->t.ssta.s.assocSta.nmbUndelDgms);
	len = len + sprintf(buf + len, " The number of retransmissions count : %d\n", cfm->t.ssta.s.assocSta.rtxCnt);
	len = len + sprintf(buf + len, " The receive window size is: %d\n\n", cfm->t.ssta.s.assocSta.SctWinSize);
	for(idx =0; idx < LSB_MAX_TMRS ; idx++)
	{
		len = len + sprintf(buf + len, " %d) Timer state is %d\n", idx, cfm->t.ssta.s.assocSta.tmr[idx].state);
		len = len + sprintf(buf + len, " %d) Timer value is %d\n", idx, cfm->t.ssta.s.assocSta.tmr[idx].tmrVal);
		len = len + sprintf(buf + len, " %d) No of paths is %d\n", idx, cfm->t.ssta.s.assocSta.tmr[idx].numPaths);
		for(idx1 =0; idx1 < cfm->t.ssta.s.assocSta.tmr[idx].numPaths; idx1++)
		{
			if( cfm->t.ssta.s.assocSta.tmr[idx].path[idx1].localAddr.type == CM_IPV4ADDR_TYPE)
			{
				len = len + sprintf(buf + len, "     %d) the local Addr is %d\n", idx1,
						cfm->t.ssta.s.assocSta.tmr[idx].path[idx1].localAddr.u.ipv4NetAddr);
			}
			else
			{
				len = len + sprintf(buf + len, "     %d) the local Addr is %s\n", idx1,
						cfm->t.ssta.s.assocSta.tmr[idx].path[idx1].localAddr.u.ipv6NetAddr);
			}

			if( cfm->t.ssta.s.assocSta.tmr[idx].path[idx1].peerAddr.type == CM_IPV4ADDR_TYPE)
			{
				len = len + sprintf(buf + len, "     %d) the peer Addr is %d\n", idx1,
						cfm->t.ssta.s.assocSta.tmr[idx].path[idx1].peerAddr.u.ipv4NetAddr);
			}
			else
			{
				len = len + sprintf(buf + len, "     %d) the peer Addr is %s\n", idx1,
						cfm->t.ssta.s.assocSta.tmr[idx].path[idx1].peerAddr.u.ipv6NetAddr);
			}
		} /* Loop for paths */
	} /* Loop for timers */
#endif

	return len;
}

/******************************************************************************
* Fun:  handle_show_sctp_profile()
* Desc: display requested sctp profile information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: Kapil Gupta
*******************************************************************************/
static ftdm_status_t handle_show_sctp_profile(ftdm_stream_handle_t *stream, char* sctp_profile_name)
{
	char*  xmlhdr = (char*)"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	char  buf[4096];
	int x = 0x00;
	int len = 0x00;
	SbMgmt cfm;
	int found = 0x00;

	memset((U8 *)&cfm, 0, sizeof(SbMgmt));
	memset(&buf[0], 0, sizeof(buf));

	len = len + sprintf(buf + len, "%s\n", xmlhdr);

	/*iterate through all the sctp links and prints all information */
	x = 1;
	while(x<MAX_SCTP_LINK){
		if((g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].id !=0) &&
				((g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].flags & SNGSS7_ACTIVE))) {
			if(!strcasecmp(sctp_profile_name, g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].name)){
				found = 0x01;
				break;
			}
		}
		x++;
	}
	if(!found){
		stream->write_function(stream,"Requested SCTP profile[%s] not configured\n", sctp_profile_name);
		return FTDM_FAIL;
	}

	len = len + sprintf(buf + len, "<sctp_profile>\n");

	if(ftmod_sctp_ssta_req(STSBSCTSAP,x,&cfm)) {
		stream->write_function(stream," Request to  SCTP layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<sctp_sap>\n");
		len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_SCTP_SAP_STATE(cfm.t.ssta.s.sapSta.hlSt));
		len = len + sprintf(buf + len," <switch> %s </switch>\n", PRNT_SCTP_PROTO_SWITCH(cfm.t.ssta.s.sapSta.swtch));
		len = len + sprintf(buf + len, "</sctp_sap>\n");
	}

	if(ftmod_sctp_ssta_req(STSBTSAP,x,&cfm)) {
		stream->write_function(stream," Request to  SCTP layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<sctp_transport_sap>\n");
		len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_SCTP_SAP_STATE(cfm.t.ssta.s.sapSta.hlSt));
		len = len + sprintf(buf + len," <switch> %s </switch>\n", PRNT_SCTP_PROTO_SWITCH(cfm.t.ssta.s.sapSta.swtch));
		len = len + sprintf(buf + len, "</sctp_transport_sap>\n");
	}

	if(ftmod_sctp_ssta_req(STSBASSOC,x,&cfm)) {
		/* it means assoc id not yet allocated */
		if(LCM_REASON_INVALID_PAR_VAL == cfm.cfm.reason){
			len = len + sprintf(buf + len, "<sctp_association>\n");
			len = len + sprintf(buf + len, " <status> SCT_ASSOC_STATE_CLOSED </status>\n");
			len = len + sprintf(buf + len, "</sctp_association>\n");
		}else{
			stream->write_function(stream," Request to  SCTP layer failed \n");
			return FTDM_FAIL;
		}
	} else {
		len = len + sprintf(buf + len, "<sctp_association>\n");
		len = len + get_assoc_resp_buf(buf + len, &cfm);
		len = len + sprintf(buf + len, "</sctp_association>\n");
	}

	/* TODO - STSBDTA */

	len = len + sprintf(buf + len, "</sctp_profile>\n");

	stream->write_function(stream,"\n%s\n",buf); 

	return FTDM_SUCCESS;
}

/******************************************************************************
* Fun:  handle_show_nif_profiles()
* Desc: display all nif profile information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: Kapil Gupta
*******************************************************************************/
static ftdm_status_t handle_show_nif_profiles(ftdm_stream_handle_t *stream)
{
	char*  xmlhdr = (char*)"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	char  buf[4096];
	int x = 0x00;
	int len = 0x00;
	NwMgmt cfm;

	memset((U8 *)&cfm, 0, sizeof(NwMgmt));
	memset(&buf[0], 0, sizeof(buf));

	len = len + sprintf(buf + len, "%s\n", xmlhdr);
	len = len + sprintf(buf + len, "<nif_profiles>\n");

	if(ftmod_nif_ssta_req(STNWGEN, 0x00, &cfm)) {
		stream->write_function(stream," Request to  NIF layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<nif_gen>\n");
#ifdef BIT_64
		len = len + sprintf(buf + len, "<mem_size> %d </mem_size>\n",cfm.t.ssta.s.genSta.memSize);
		len = len + sprintf(buf + len, " <allocated_mem_size> %d </allocated_mem_size>\n",cfm.t.ssta.s.genSta.memAlloc);
#else
		len = len + sprintf(buf + len, "<mem_size> %ld </mem_size>\n",cfm.t.ssta.s.genSta.memSize);
		len = len + sprintf(buf + len, " <allocated_mem_size> %ld </allocated_mem_size>\n",cfm.t.ssta.s.genSta.memAlloc);
#endif
		len = len + sprintf(buf + len, "</nif_gen>\n");
	}

	/*iterate through all the NIF links and prints all information */
	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].id !=0) &&
				((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].flags & SNGSS7_ACTIVE))) {

			len = len + sprintf(buf + len, "<nif_profile>\n");

			if(ftmod_nif_ssta_req(STNWDLSAP,x,&cfm)) {
				stream->write_function(stream," Request to NIF layer failed \n");
				return FTDM_FAIL;
			} else {
				len = len + sprintf(buf + len, "<nif_dlsap>\n");
				len = len + sprintf(buf + len," <m2ua_sap_state> %s </m2ua_sap_state>\n", PRNT_NIF_SAP_STATE(cfm.t.ssta.s.dlSapSta.m2uaState));
				len = len + sprintf(buf + len," <mtp2_sap_state> %s </mtp2_sap_state>\n", PRNT_NIF_SAP_STATE(cfm.t.ssta.s.dlSapSta.mtp2State));
#ifdef BIT_64
				len = len + sprintf(buf + len," <nmb_of_retry> %d </nmb_of_retry>\n", cfm.t.ssta.s.dlSapSta.nmbRetry);
#else
				len = len + sprintf(buf + len," <nmb_of_retry> %ld </nmb_of_retry>\n", cfm.t.ssta.s.dlSapSta.nmbRetry);
#endif
				len = len + sprintf(buf + len, "</nif_dlsap>\n");
			}

			len = len + sprintf(buf + len, "</nif_profile>\n");
		}
		x++;
	}

	len = len + sprintf(buf + len, "</nif_profiles>\n");
	stream->write_function(stream,"\n%s\n",buf); 

	return FTDM_SUCCESS;
}

/******************************************************************************
* Fun:  handle_show_nif_profile()
* Desc: display requested nif profile information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: Kapil Gupta
*******************************************************************************/
static ftdm_status_t handle_show_nif_profile(ftdm_stream_handle_t *stream, char* nif_profile_name) 
{
	char*  xmlhdr = (char*)"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	char  buf[4096];
	int x = 0x00;
	int found = 0x00;
	int len = 0x00;
	NwMgmt cfm;

	memset((U8 *)&cfm, 0, sizeof(NwMgmt));
	memset(&buf[0], 0, sizeof(buf));

	len = len + sprintf(buf + len, "%s\n", xmlhdr);

	/*iterate through all the m2ua links and get required profile */
	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].id !=0) &&
				((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].flags & SNGSS7_ACTIVE))) {

			if(!strcasecmp(nif_profile_name, g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].name)){
				found = 0x01;
				break;
			}
		}
		x++;
	}

	if(!found){
		stream->write_function(stream,"Requested NIF profile[%s] not configured\n", nif_profile_name);
		return FTDM_FAIL;
	}


	len = len + sprintf(buf + len, "<nif_profile>\n");

	if(ftmod_nif_ssta_req(STNWDLSAP,x,&cfm)) {
		stream->write_function(stream," Request to NIF layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<nif_dlsap>\n");
		len = len + sprintf(buf + len," <m2ua_sap_state> %s </m2ua_sap_state>\n", PRNT_NIF_SAP_STATE(cfm.t.ssta.s.dlSapSta.m2uaState));
		len = len + sprintf(buf + len," <mtp2_sap_state> %s </mtp2_sap_state>\n", PRNT_NIF_SAP_STATE(cfm.t.ssta.s.dlSapSta.mtp2State));
#ifdef BIT_64
		len = len + sprintf(buf + len," <nmb_of_retry> %d </nmb_of_retry>\n", cfm.t.ssta.s.dlSapSta.nmbRetry);
#else
		len = len + sprintf(buf + len," <nmb_of_retry> %ld </nmb_of_retry>\n", cfm.t.ssta.s.dlSapSta.nmbRetry);
#endif
		len = len + sprintf(buf + len, "</nif_dlsap>\n");
	}

	len = len + sprintf(buf + len, "</nif_profile>\n");

	stream->write_function(stream,"\n%s\n",buf); 

	return FTDM_SUCCESS;
}

/******************************************************************************/
/******************************************************************************
* Fun:  handle_show_m2ua_peer_status()
* Desc: display requested m2ua profile peer information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: Kapil Gupta
*******************************************************************************/

static ftdm_status_t handle_show_m2ua_peer_status(ftdm_stream_handle_t *stream, char* m2ua_profile_name) 
{
	char*  xmlhdr = (char*)"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	char  buf[4096];
	int x = 0x00;
	int found = 0x00;
	int len = 0x00;
	MwMgmt cfm;
	SbMgmt sctp_cfm;
	sng_m2ua_cluster_cfg_t*     clust = NULL; 
	sng_m2ua_cfg_t*             m2ua  = NULL;
        sng_m2ua_peer_cfg_t*        peer  = NULL;
	int peer_id = 0;	
	int sctp_id = 0;	

	memset((U8 *)&cfm, 0, sizeof(MwMgmt));
	memset((U8 *)&sctp_cfm, 0, sizeof(SbMgmt));
	memset(&buf[0], 0, sizeof(buf));

	len = len + sprintf(buf + len, "%s\n", xmlhdr);

	/*iterate through all the m2ua links and get required profile */
	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].id !=0) &&
				((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].flags & SNGSS7_ACTIVE))) {

			if(!strcasecmp(m2ua_profile_name, g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].name)){
				found = 0x01;
				break;
			}
		}
		x++;
	}

	if(!found){
		stream->write_function(stream,"Requested M2UA profile[%s] not configured\n", m2ua_profile_name);
		return FTDM_FAIL;
	}

	m2ua  = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x];
	clust = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[m2ua->clusterId];

	for(x = 0; x < clust->numOfPeers;x++){
		peer_id = clust->peerIdLst[x];
		peer = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[peer_id];

		if(ftmod_m2ua_ssta_req(STMWPEER, peer_id, &cfm)) {
			stream->write_function(stream," Request to  M2UA layer failed \n");
			return FTDM_FAIL;
		} else {
			len = len + sprintf(buf + len, "<m2ua_peer>\n");
			len = len + sprintf(buf + len, "<name> %s </name>\n",peer->name);
			len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_PEER_STATE(cfm.t.ssta.s.peerSta.state));
			/*len = len + sprintf(buf + len, " <connected_status> %s </connected_status>\n",(cfm.t.ssta.s.peerSta.assocSta.connected)?"CONNECTED":"NOT CONNECTED");*/
			len = len + sprintf(buf + len, "</m2ua_peer>\n");
		}

		sctp_id = peer->sctpId;

		if(ftmod_sctp_ssta_req(STSBASSOC, sctp_id, &sctp_cfm)) {
			if(LMW_PEER_DOWN == cfm.t.ssta.s.peerSta.state){
				/* If there is no association established so far, it will return fail..*/
				len = len + sprintf(buf + len, "<sctp_association>\n");
				len = len + sprintf(buf + len, " <status> SCT_ASSOC_STATE_CLOSED </status>\n");
				len = len + sprintf(buf + len, "</sctp_association>\n");
			}else{
				stream->write_function(stream," Request to SCTP layer failed \n");
				return FTDM_FAIL;
			}
		} else {
			len = len + sprintf(buf + len, "<sctp_association>\n");
			len = len + sprintf(buf + len, " <status> %s </status>\n", PRNT_SCTP_ASSOC_STATE(sctp_cfm.t.ssta.s.assocSta.assocState));
			len = len + sprintf(buf + len, "</sctp_association>\n");
		}
	}

	stream->write_function(stream,"\n%s\n",buf); 

	return FTDM_SUCCESS;
}

/******************************************************************************
* Fun:  handle_show_m2ua_cluster_status()
* Desc: display requested m2ua profile cluster information
* Ret:  FTDM_SUCCESS | FTDM_FAIL
* Note: 
* author: Kapil Gupta
*******************************************************************************/

static ftdm_status_t handle_show_m2ua_cluster_status(ftdm_stream_handle_t *stream, char* m2ua_profile_name) 
{
	char*  xmlhdr = (char*)"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	char  buf[4096];
	int x = 0x00;
	int found = 0x00;
	int len = 0x00;
	int idx = 0x00;
	MwMgmt cfm;
	SbMgmt sctp_cfm;
	sng_m2ua_cluster_cfg_t*     clust = NULL; 
	sng_m2ua_cfg_t*             m2ua  = NULL;

	memset((U8 *)&cfm, 0, sizeof(MwMgmt));
	memset((U8 *)&sctp_cfm, 0, sizeof(SbMgmt));
	memset(&buf[0], 0, sizeof(buf));

	len = len + sprintf(buf + len, "%s\n", xmlhdr);

	/*iterate through all the m2ua links and get required profile */
	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].id !=0) &&
				((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].flags & SNGSS7_ACTIVE))) {

			if(!strcasecmp(m2ua_profile_name, g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].name)){
				found = 0x01;
				break;
			}
		}
		x++;
	}

	if(!found){
		stream->write_function(stream,"Requested M2UA profile[%s] not configured\n", m2ua_profile_name);
		return FTDM_FAIL;
	}

	m2ua  = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x];
	clust = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[m2ua->clusterId];

	if(ftmod_m2ua_ssta_req(STMWCLUSTER,g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].clusterId,&cfm)) {
		stream->write_function(stream," Request to M2UA layer failed \n");
		return FTDM_FAIL;
	} else {
		len = len + sprintf(buf + len, "<m2ua_cluster>\n");
		len = len + sprintf(buf + len, "<name> %s </name>\n",clust->name);
		len = len + sprintf(buf + len," <state> %s </state>\n", PRNT_M2UA_CLUSTER_STATE(cfm.t.ssta.s.clusterSta.state));
		len = len + sprintf(buf + len, "<num_of_peers> %d </num_of_peers>\n",cfm.t.ssta.s.clusterSta.nmbPeer);
		for(idx = 0; idx < cfm.t.ssta.s.clusterSta.nmbPeer; idx++)
		{
			len = len + sprintf(buf + len, "<m2ua_cluster_peer>\n");
			len = len + sprintf(buf + len, " <peer_name> %s </peer_name>\n", g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[cfm.t.ssta.s.clusterSta.peerSt[idx].peerId].name);
			len = len + sprintf(buf + len, " <peer_id> %d </peer_id>\n", cfm.t.ssta.s.clusterSta.peerSt[idx].peerId);
			len = len + sprintf(buf + len, " <peer_state> %s </peer_state>\n",  PRNT_M2UA_PEER_STATE(cfm.t.ssta.s.clusterSta.peerSt[idx].peerState));
			len = len + sprintf(buf + len, "</m2ua_cluster_peer>\n");
		}
		len = len + sprintf(buf + len, "<num_active_peer> %d </num_active_peer>\n",cfm.t.ssta.s.clusterSta.nmbActPeer);

		len = len + sprintf(buf + len, "</m2ua_cluster>\n");
	}

	stream->write_function(stream,"\n%s\n",buf); 

	return FTDM_SUCCESS;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
/******************************************************************************/
