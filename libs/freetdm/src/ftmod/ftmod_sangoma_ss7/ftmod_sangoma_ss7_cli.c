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

static ftdm_status_t handle_activate_link(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_deactivate_link(ftdm_stream_handle_t *stream, char *name);

static ftdm_status_t handle_activate_linkset(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_deactivate_linkset(ftdm_stream_handle_t *stream, char *name);

static ftdm_status_t handle_tx_lpo(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_tx_lpr(ftdm_stream_handle_t *stream, char *name);

static ftdm_status_t handle_status_link(ftdm_stream_handle_t *stream, char *name);
static ftdm_status_t handle_status_linkset(ftdm_stream_handle_t *stream, char *name);

static ftdm_status_t extract_span_chan(char *argv[10], int pos, int *span, int *chan);
static ftdm_status_t check_arg_count(int args, int min);
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
		argc = ftdm_separate_string(mycmd,' ',argv,(sizeof(argv) / sizeof(argv[0])));
	}

	if (check_arg_count(argc, 1)) goto handle_cli_error_argc;
	
	if (!strcasecmp(argv[c], "show")) {
	/**************************************************************************/   
		if (check_arg_count(argc, 2)) goto handle_cli_error_argc;
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
	/**************************************************************************/
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
	stream->write_function(stream, "ftdm ss7 set ftrace X Y\n");
	stream->write_function(stream, "ftdm ss7 set mtrace X Y\n");
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
	stream->write_function(stream, "ftdm ss7 blo span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 ubl span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 rsc span X chan Y\n");
	stream->write_function(stream, "ftdm ss7 grs span X chan Y range Z\n");
	stream->write_function(stream, "ftdm ss7 cgb span X chan Y range Z\n");
	stream->write_function(stream, "ftdm ss7 cgu span X chan Y range Z\n");
	stream->write_function(stream, "\n");
	stream->write_function(stream, "Ftmod_sangoma_ss7 link control:\n");
	stream->write_function(stream, "ftdm ss7 inhibit link X\n");
	stream->write_function(stream, "ftdm ss7 uninhibit link X\n");
	stream->write_function(stream, "ftdm ss7 activate link X\n");
	stream->write_function(stream, "ftdm ss7 deactivate link X\n");
	stream->write_function(stream, "ftdm ss7 activate linkset X\n");
	stream->write_function(stream, "ftdm ss7 deactivate linkset X\n");
	stream->write_function(stream, "ftdm ss7 lpo link X\n");
	stream->write_function(stream, "ftdm ss7 lpr link X\n");
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
	int				 x;
	int				 free;
	sngss7_chan_data_t  *ss7_info;
	ftdm_channel_t	  *ftdmchan;
	int				 lspan;
	int				 lchan;

	x=1;
	free = 0;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {
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

	x=1;
	in_use = 0;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {
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

	x=1;
	in_reset = 0;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {
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
				if ((sngss7_test_flag(ss7_info, FLAG_RESET_RX)) ||
					(sngss7_test_flag(ss7_info, FLAG_RESET_TX)) ||
					(sngss7_test_flag(ss7_info, FLAG_GRP_RESET_RX)) ||
					(sngss7_test_flag(ss7_info, FLAG_GRP_RESET_TX))) {
					
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
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	stream->write_function(stream, "\nTotal # of CICs in reset = %d\n",in_reset);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_show_flags(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 x;
	int				 bit;
	sngss7_chan_data_t  *ss7_info;
	ftdm_channel_t	  *ftdmchan;
	int				 lspan;
	int				 lchan;

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {
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

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {
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
					stream->write_function(stream, "r_hw=Y|");
				}else {
					stream->write_function(stream, "r_hw=N|");
				}

				if(sngss7_test_flag(ss7_info, FLAG_CKT_LC_BLOCK_RX)) {
					stream->write_function(stream, "l_mngmt=Y|");
				}else {
					stream->write_function(stream, "l_mngmt=N|");
				}

				if(sngss7_test_flag(ss7_info, FLAG_CKT_UCIC_BLOCK)) {
					stream->write_function(stream, "l_ucic=Y|");
				}else {
					stream->write_function(stream, "l_ucic=N|");
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
static ftdm_status_t handle_show_status(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 			x;
	sngss7_chan_data_t  		*ss7_info;
	ftdm_channel_t	  			*ftdmchan;
	int				 			lspan;
	int				 			lchan;
	ftdm_signaling_status_t		sigstatus = FTDM_SIG_STATE_DOWN;
	sng_isup_ckt_t				*ckt;

	x=1;
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
				if (ckt->type == HOLE) {
					stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|NOT USED\n",
							ckt->span,
							ckt->chan,
							ckt->cic);
				} else if (ckt->type == SIG) {
					stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|SIGNALING LINK\n",
							ckt->span,
							ckt->chan,
							ckt->cic);
				} else {
					ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
					ftdmchan = ss7_info->ftdmchan;

					/* grab the signaling_status */
					ftdm_channel_get_sig_status(ftdmchan, &sigstatus);
	
					stream->write_function(stream, "span=%2d|chan=%2d|cic=%4d|sig_status=%4s|state=%s|",
													ckt->span,
													ckt->chan,
													ckt->cic,
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
						stream->write_function(stream, "r_hw=Y|");
					}else {
						stream->write_function(stream, "r_hw=N|");
					}
	
					if(sngss7_test_flag(ss7_info, FLAG_CKT_LC_BLOCK_RX)) {
						stream->write_function(stream, "l_mngmt=Y|");
					}else {
						stream->write_function(stream, "l_mngmt=N|");
					}
	
					if(sngss7_test_flag(ss7_info, FLAG_CKT_UCIC_BLOCK)) {
						stream->write_function(stream, "l_ucic=Y|");
					}else {
						stream->write_function(stream, "l_ucic=N|");
					}				
	
					stream->write_function(stream, "flags=0x%X",ss7_info->flags);
	
					stream->write_function(stream, "\n");
				} /* if ( hole, sig, voice) */
			} /* if ( span and chan) */
		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

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

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {
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
				/* now that we have the right channel...put a lock on it so no-one else can use it */
				ftdm_mutex_lock(ftdmchan->mutex);

				/* check if there is a pending state change|give it a bit to clear */
				if (check_for_state_change(ftdmchan)) {
					SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", ss7_info->circuit->cic);
					/* check if we need to die */
					SS7_ASSERT;
					/* unlock the channel again before we exit */
					ftdm_mutex_unlock(ftdmchan->mutex);
					/* move to the next channel */
					continue;
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
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

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

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {
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
				/* now that we have the right channel...put a lock on it so no-one else can use it */
				ftdm_mutex_lock(ftdmchan->mutex);

				/* check if there is a pending state change|give it a bit to clear */
				if (check_for_state_change(ftdmchan)) {
					SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", ss7_info->circuit->cic);
					/* check if we need to die */
					SS7_ASSERT;
					/* unlock the channel again before we exit */
					ftdm_mutex_unlock(ftdmchan->mutex);
					/* move to the next channel */
					continue;
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
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	handle_show_blocks(stream, span, chan, verbose);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_status_link(ftdm_stream_handle_t *stream, char *name)
{
	int 		x = 0;
	SnMngmt		sta;
	
	/* find the link request by it's name */
	x = 1;
	while(g_ftdm_sngss7_data.cfg.mtpLink[x].id != 0) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLink[x].name, name)) {

			/* send the status request */
			if (ftmod_ss7_mtplink_sta(x, &sta)) {
				stream->write_function(stream, "Failed to read link=%s status\n", name);
				return FTDM_FAIL;
			}

			/* print the results */
			stream->write_function(stream, "%s|span=%d|chan=%d|sap=%d|state=%s|l_blk=%s|r_blk=%s|l_inhbt=%s|r_inhbt=%s\n",
						name,
						g_ftdm_sngss7_data.cfg.mtpLink[x].mtp1.span,
						g_ftdm_sngss7_data.cfg.mtpLink[x].mtp1.chan,
						g_ftdm_sngss7_data.cfg.mtpLink[x].id,
						DECODE_LSN_LINK_STATUS(sta.t.ssta.s.snDLSAP.state),
						(sta.t.ssta.s.snDLSAP.locBlkd) ? "Y":"N",
						(sta.t.ssta.s.snDLSAP.remBlkd) ? "Y":"N",
						(sta.t.ssta.s.snDLSAP.locInhbt) ? "Y":"N",
						(sta.t.ssta.s.snDLSAP.rmtInhbt) ? "Y":"N");

			goto success;
		}
		
		/* move to the next link */
		x++;
	} /* while (id != 0) */

	stream->write_function(stream, "Failed to find link=\"%s\"\n", name);

success:
	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_status_linkset(ftdm_stream_handle_t *stream, char *name)
{
	int 		x = 0;
	SnMngmt		sta;

	/* find the linkset request by it's name */
	x = 1;
	while(g_ftdm_sngss7_data.cfg.mtpLinkSet[x].id != 0) {
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
	while(g_ftdm_sngss7_data.cfg.mtpLink[x].id != 0) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLink[x].name, name)) {

			/* send the inhibit request */
			if (ftmod_ss7_inhibit_mtplink(x)) {
				stream->write_function(stream, "Failed to inhibit link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_link(stream, &name[0]);

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
static ftdm_status_t handle_set_uninhibit(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(g_ftdm_sngss7_data.cfg.mtpLink[x].id != 0) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLink[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_uninhibit_mtplink(x)) {
				stream->write_function(stream, "Failed to uninhibit link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_link(stream, &name[0]);

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
static ftdm_status_t handle_tx_rsc(ftdm_stream_handle_t *stream, int span, int chan, int verbose)
{
	int				 	x;
	sngss7_chan_data_t  *sngss7_info;
	ftdm_channel_t	  	*ftdmchan;
	int				 	lspan;
	int				 	lchan;

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {
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
				sngss7_set_flag(sngss7_info, FLAG_RESET_TX);

				switch (ftdmchan->state) {
				/**************************************************************************/
				case FTDM_CHANNEL_STATE_RESTART:
					/* go to idle so that we can redo the restart state*/
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_IDLE);
					break;
				/**************************************************************************/
				default:
					/* set the state of the channel to restart...the rest is done by the chan monitor */
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
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
	int					x;
	sngss7_chan_data_t	*sngss7_info;
	ftdm_channel_t		*ftdmchan;
	sngss7_span_data_t	*sngss7_span;

	if (range > 31) {
		stream->write_function(stream, "Invalid range value %d", range);
		return FTDM_SUCCESS;
	}

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {

			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->mod_data;

			if ((ftdmchan->physical_span_id == span) && 
				((ftdmchan->physical_chan_id >= chan) && (ftdmchan->physical_chan_id < (chan+range)))) {
				/* now that we have the right channel...put a lock on it so no-one else can use it */
				ftdm_mutex_lock(ftdmchan->mutex);

				/* check if there is a pending state change|give it a bit to clear */
				if (check_for_state_change(ftdmchan)) {
					SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
					/* check if we need to die */
					SS7_ASSERT;
					/* unlock the channel again before we exit */
					ftdm_mutex_unlock(ftdmchan->mutex);
					/* move to the next channel */
					continue;
				} else {
					/* throw the grp reset flag */
					sngss7_set_flag(sngss7_info, FLAG_GRP_RESET_TX);
					if (ftdmchan->physical_chan_id == chan) {
						sngss7_set_flag(sngss7_info, FLAG_GRP_RESET_BASE);
						sngss7_span->tx_grs.circuit = sngss7_info->circuit->id;
						sngss7_span->tx_grs.range = range-1;
					}

					/* set the channel to suspended state */
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);

				}

				/* unlock the channel again before we exit */
				ftdm_mutex_unlock(ftdmchan->mutex);

			} /* if ( span and chan) */

		} /* if ( cic != 0) */

		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */
	
	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {

			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->mod_data;

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


	if (range > 31) {
		stream->write_function(stream, "Invalid range value %d", range);
		return FTDM_SUCCESS;
	}

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {

			/* extract the channel and span info for this circuit */
			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->mod_data;

			/* check if this circuit is part of the block */
			if ((ftdmchan->physical_span_id == span) && 
				((ftdmchan->physical_chan_id >= chan) && (ftdmchan->physical_chan_id < (chan+range)))) {

				/* now that we have the right channel...put a lock on it so no-one else can use it */
				ftdm_mutex_lock(ftdmchan->mutex);

				/* throw the grp maint. block flag */
				sngss7_set_flag(sngss7_info, FLAG_GRP_MN_BLOCK_TX);

				/* bring the sig status down */
				sigev.chan_id = ftdmchan->chan_id;
				sigev.span_id = ftdmchan->span_id;
				sigev.channel = ftdmchan;
				sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
				sigev.sigstatus = FTDM_SIG_STATE_DOWN;
				ftdm_span_send_signal(ftdmchan->span, &sigev); 

				/* if this is the first channel in the range */
				if (ftdmchan->physical_chan_id == chan) {
					/* attach the cgb information */
					main_chan = ftdmchan;
					sngss7_span->tx_cgb.circuit = sngss7_info->circuit->id;
					sngss7_span->tx_cgb.range = range-1;
					sngss7_span->tx_cgb.type = 0; /* maintenace block */
				} /* if (ftdmchan->physical_chan_id == chan) */
				
				/* update the status field */
				sngss7_span->tx_cgb.status[byte] = (sngss7_span->tx_cgb.status[byte] | (1 << bit));

				/* update the bit and byte counter*/
				bit ++;
				if (bit == 8) {
					byte++;
					bit = 0;
				}

				/* unlock the channel again before we exit */
				ftdm_mutex_unlock(ftdmchan->mutex);
			} /* if ( span and chan) */
		} /* if ( cic == voice) */
		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	/* send the circuit group block */
	ft_to_sngss7_cgb(main_chan);

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {

			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->mod_data;

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
static ftdm_status_t handle_tx_cgu(ftdm_stream_handle_t *stream, int span, int chan, int range, int verbose)
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


	if (range > 31) {
		stream->write_function(stream, "Invalid range value %d", range);
		return FTDM_SUCCESS;
	}

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {

			/* extract the channel and span info for this circuit */
			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->mod_data;

			/* check if this circuit is part of the block */
			if ((ftdmchan->physical_span_id == span) && 
				((ftdmchan->physical_chan_id >= chan) && (ftdmchan->physical_chan_id < (chan+range)))) {

				/* now that we have the right channel...put a lock on it so no-one else can use it */
				ftdm_mutex_lock(ftdmchan->mutex);

				/* throw the grp maint. block flag */
				sngss7_clear_flag(sngss7_info, FLAG_GRP_MN_BLOCK_TX);

				/* bring the sig status up */
				sigev.chan_id = ftdmchan->chan_id;
				sigev.span_id = ftdmchan->span_id;
				sigev.channel = ftdmchan;
				sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
				sigev.sigstatus = FTDM_SIG_STATE_UP;
				ftdm_span_send_signal(ftdmchan->span, &sigev); 

				/* if this is the first channel in the range */
				if (ftdmchan->physical_chan_id == chan) {
					/* attach the cgb information */
					main_chan = ftdmchan;
					sngss7_span->tx_cgu.circuit = sngss7_info->circuit->id;
					sngss7_span->tx_cgu.range = range-1;
					sngss7_span->tx_cgu.type = 0; /* maintenace block */
				} /* if (ftdmchan->physical_chan_id == chan) */
				
				/* update the status field */
				sngss7_span->tx_cgu.status[byte] = (sngss7_span->tx_cgu.status[byte] | (1 << bit));

				/* update the bit and byte counter*/
				bit ++;
				if (bit == 8) {
					byte++;
					bit = 0;
				}

				/* unlock the channel again before we exit */
				ftdm_mutex_unlock(ftdmchan->mutex);
			} /* if ( span and chan) */
		} /* if ( cic == voice) */
		/* go the next circuit */
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x]id != 0) */

	/* send the circuit group block */
	ft_to_sngss7_cgu(main_chan);

	x=1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == VOICE) {

			sngss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = sngss7_info->ftdmchan;
			sngss7_span = ftdmchan->span->mod_data;

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
static ftdm_status_t handle_activate_link(ftdm_stream_handle_t *stream, char *name)
{
	int		x = 0;

	/* find the link request by it's name */
	x = 1;
	while(g_ftdm_sngss7_data.cfg.mtpLink[x].id != 0) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLink[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_activate_mtplink(x)) {
				stream->write_function(stream, "Failed to activate link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (id != 0) */

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
	while(g_ftdm_sngss7_data.cfg.mtpLink[x].id != 0) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLink[x].name, name)) {

			/* send the deactivate request */
			if (ftmod_ss7_deactivate2_mtplink(x)) {
				stream->write_function(stream, "Failed to deactivate link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (id != 0) */

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
	while(g_ftdm_sngss7_data.cfg.mtpLinkSet[x].id != 0) {
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
	while(g_ftdm_sngss7_data.cfg.mtpLinkSet[x].id != 0) {
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
	while(g_ftdm_sngss7_data.cfg.mtpLink[x].id != 0) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLink[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_lpo_mtplink(x)) {
				stream->write_function(stream, "Failed set LPO link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (id != 0) */

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
	while(g_ftdm_sngss7_data.cfg.mtpLink[x].id != 0) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpLink[x].name, name)) {

			/* send the uninhibit request */
			if (ftmod_ss7_lpr_mtplink(x)) {
				stream->write_function(stream, "Failed set LPR link=%s\n", name);
				return FTDM_FAIL;
			}

			/* print the new status of the link */
			handle_status_link(stream, &name[0]);
			goto success;
		}
 
		/* move to the next link */
		x++;
	} /* while (id != 0) */

	stream->write_function(stream, "Could not find link=%s\n", name);

success:
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
