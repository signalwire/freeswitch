/*
 * Copyright (c) 2010, Sangoma Technologies
 * David Yat Sin <davidy@sangoma.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ftmod_sangoma_isdn.h"
#include "ftmod_sangoma_isdn_trace.h"

#define OCTET(x) (ieData[x-1] & 0xFF)
#define MAX_DECODE_STR_LEN 2000

typedef struct sngisdn_trace_info
{
	uint8_t call_ref_flag;
	uint16_t call_ref;
	uint8_t msgtype;
	uint8_t bchan_no;
	ftdm_trace_dir_t dir;
} sngisdn_frame_info_t;

void print_hex_dump(char* str, uint32_t *str_len, uint8_t* data, uint32_t index_start, uint32_t index_end);
uint32_t sngisdn_decode_ie(char *str, uint32_t *str_len, uint8_t current_codeset, uint8_t *data, uint16_t index_start);
static ftdm_status_t sngisdn_map_call(sngisdn_span_data_t *signal_data, sngisdn_frame_info_t frame_info, ftdm_channel_t **found);
static ftdm_status_t sngisdn_get_frame_info(uint8_t *data, uint32_t data_len, ftdm_trace_dir_t dir, sngisdn_frame_info_t *frame_info);

uint8_t get_bits(uint8_t octet, uint8_t bitLo, uint8_t bitHi);
char* get_code_2_str(int code, struct code2str *pCodeTable);
void sngisdn_decode_q921(char* str, uint8_t* data, uint32_t data_len);
void sngisdn_decode_q931(char* str, uint8_t* data, uint32_t data_len);


char* get_code_2_str(int code, struct code2str *pCodeTable)
{
	struct code2str* pCode2txt;
	pCode2txt = pCodeTable;	
	while(pCode2txt) {
		if(pCode2txt->code >= 0) {
			if (pCode2txt->code == code) {
				return pCode2txt->text;
			}
			pCode2txt++;
		} else {
			/* This is the default value from the table */
			return pCode2txt->text;
		}
	}
	return (char*)"unknown";
}


uint8_t get_bits(uint8_t octet, uint8_t bitLo, uint8_t bitHi)
{
	if (!bitLo || !bitHi) {
		return 0;
	}
	if (bitLo > bitHi) {
		return 0;
	}

	bitLo--;
	bitHi--;

	switch(bitHi - bitLo) {
		case 0:
			return (octet >> bitLo) & 0x01;
		case 1:
			return (octet >> bitLo) & 0x03;
		case 2:
			return (octet >> bitLo) & 0x07;
		case 3:
			return (octet >> bitLo) & 0x0F;
		case 4:
			return (octet >> bitLo) & 0x1F;
		case 5:
			return (octet >> bitLo) & 0x3F;
		case 6:
			return (octet >> bitLo) & 0x7F;
		case 7:
			return (octet >> bitLo) & 0xFF;
	}
	return 0;
}

void sngisdn_trace_interpreted_q921(sngisdn_span_data_t *signal_data, ftdm_trace_dir_t dir, uint8_t *data, uint32_t data_len)
{
	char *data_str = ftdm_calloc(1,500); /* TODO Find a proper size */
 	sngisdn_decode_q921(data_str, data, data_len);
	ftdm_log(FTDM_LOG_DEBUG, "[SNGISDN Q921] %s FRAME %s:\n%s\n", signal_data->ftdm_span->name, ftdm_trace_dir2str(dir), data_str);
	ftdm_safe_free(data_str);
}

void sngisdn_trace_raw_q921(sngisdn_span_data_t *signal_data, ftdm_trace_dir_t dir, uint8_t *data, uint32_t data_len)
{
	uint8_t 			*raw_data;
	ftdm_sigmsg_t		sigev;
	
	memset(&sigev, 0, sizeof(sigev));

	sigev.span_id = signal_data->ftdm_span->span_id;
	sigev.chan_id = signal_data->dchan->chan_id;
	sigev.channel = signal_data->dchan;
	sigev.event_id = FTDM_SIGEVENT_TRACE_RAW;
	
	sigev.ev_data.trace.dir = dir;
	sigev.ev_data.trace.type = FTDM_TRACE_TYPE_Q921;
	
	raw_data = ftdm_malloc(data_len);
	ftdm_assert(raw_data, "Failed to malloc");
	
	memcpy(raw_data, data, data_len);
	sigev.raw.data = raw_data;
	sigev.raw.len = data_len;
	ftdm_span_send_signal(signal_data->ftdm_span, &sigev);
}

void sngisdn_decode_q921(char* str, uint8_t* data, uint32_t data_len)
{
	uint32_t str_len;
	uint32_t i;
	uint8_t sapi, cr, ea, tei, ns, nr, pf, p, cmd;
	uint8_t frame_format = 0;

	str_len = 0;

	if(data_len >= 2) {
		switch ((int)data[2] & 0x03) {
			case 0: case 2:
				frame_format = I_FRAME;
				break;
			case 1: 
				frame_format = S_FRAME;
				break;
			case 3:
				frame_format = U_FRAME;
				break;
		}
	}

	str_len+= sprintf(&str[str_len], "  format: %s\n",
										get_code_2_str(frame_format, dcodQ921FrameFormatTable));
										
	for(i=0; i < data_len; i++) {
		switch(i) {
			case 0: // Octet 2
				sapi = (uint8_t)((data[i]>>2) & 0x3F);
				cr = (uint8_t)((data[i]>>1) & 0x1);
				ea = (uint8_t)(data[i] & 0x1);
				str_len+= sprintf(&str[str_len], "  sapi: %03d  c/r: %01d  ea: %01d\n", sapi, cr, ea);
				break;
			case 1:
				tei = (uint8_t)((data[i]>>1) & 0x7F);
				ea = (uint8_t)(data[i] & 0x1);
				str_len+= sprintf(&str[str_len], "   tei: %03d          ea: %01d\n", tei, ea);
				break;
			case 2:
				switch(frame_format) {
					case I_FRAME:
						ns = (uint8_t)((data[i]>>1) & 0x7F);
						nr = (uint8_t)((data[i+1]>>1) & 0x7F);
						p = (uint8_t)(data[i+1] & 0x01);
						str_len+= sprintf(&str[str_len], "  n(s): %03d\n  n(r): %03d  p: %01d\n", ns, nr, p);
						break;
					case S_FRAME:
						nr = (uint8_t)((data[i+1]>>1) & 0x7F);
						pf = (uint8_t)(data[i+1] & 0x01);
						str_len+= sprintf(&str[str_len], "  n(r): %03d  p/f: %01d\n", nr, pf);

						cmd = (uint8_t)((data[i]>>2) & 0x03);
						str_len+= sprintf(&str[str_len], "   cmd: %s\n", get_code_2_str(cmd, dcodQ921SupervisoryCmdTable));
						
						break;
					case U_FRAME:
						pf = (uint8_t)((data[i]>>4) & 0x01);
						str_len+= sprintf(&str[str_len], "   p/f: %01d\n", pf);

						cmd = (uint8_t)((data[i]>>2) & 0x03);
						cmd |= (uint8_t)((data[i]>>5) & 0x07);
						
						str_len+= sprintf(&str[str_len], "   cmd: %s\n", get_code_2_str(cmd, dcodQ921UnnumberedCmdTable));
						break;
				}
				break;
		}
	}

	print_hex_dump(str, &str_len, (uint8_t*) data, 0, data_len);
	return;
}


void sngisdn_trace_interpreted_q931(sngisdn_span_data_t *signal_data, ftdm_trace_dir_t dir, uint8_t *data, uint32_t data_len)
{
	char *data_str = ftdm_calloc(1,MAX_DECODE_STR_LEN); /* TODO Find a proper size */
	sngisdn_decode_q931(data_str, data, data_len);
	ftdm_log(FTDM_LOG_DEBUG, "[SNGISDN Q931] %s FRAME %s:\n%s\n", signal_data->ftdm_span->name, ftdm_trace_dir2str(dir), data_str);
	ftdm_safe_free(data_str);
}

void sngisdn_trace_raw_q931(sngisdn_span_data_t *signal_data, ftdm_trace_dir_t dir, uint8_t *data, uint32_t data_len)
{
	uint8_t 			*raw_data;
	ftdm_sigmsg_t		sigev;
	ftdm_channel_t *ftdmchan = NULL;
	sngisdn_frame_info_t  frame_info;

	memset(&sigev, 0, sizeof(sigev));

	/* Note: Mapped raw trace assume only exclusive b-channel selection is used. i.e the b-channel selected on outgoing SETUP is always used for the call */
	
	if (sngisdn_get_frame_info(data, data_len, dir, &frame_info) == FTDM_SUCCESS) {
		if (sngisdn_map_call(signal_data, frame_info, &ftdmchan) == FTDM_SUCCESS) {
			sigev.call_id = ftdmchan->caller_data.call_id;
			sigev.span_id = ftdmchan->physical_span_id;
			sigev.chan_id = ftdmchan->physical_chan_id;
			sigev.channel = ftdmchan;
		} else {
			/* We could not map the channel, but at least set the span */
			if (signal_data->ftdm_span->channels[1]) {
				sigev.span_id = signal_data->ftdm_span->channels[1]->physical_span_id;
			}
		}
		sigev.event_id = FTDM_SIGEVENT_TRACE_RAW;

		sigev.ev_data.trace.dir = dir;
		sigev.ev_data.trace.type = FTDM_TRACE_TYPE_Q931;

		raw_data = ftdm_malloc(data_len);
		ftdm_assert(raw_data, "Failed to malloc");

		memcpy(raw_data, data, data_len);
		sigev.raw.data = raw_data;
		sigev.raw.len = data_len;
		ftdm_span_send_signal(signal_data->ftdm_span, &sigev);
	}
}

void sngisdn_decode_q931(char* str, uint8_t* data, uint32_t data_len)
{
	uint32_t str_len;
	uint8_t	 prot_disc, callRefFlag;
	uint16_t lenCallRef, c, i;
	uint8_t current_codeset = 0;

	str_len = 0;

	/* Decode Protocol Discrimator */
	prot_disc = (uint8_t)data[0];
	str_len += sprintf(&str[str_len], "  Prot Disc:%s (0x%02x)\n", get_code_2_str(prot_disc, dcodQ931ProtDiscTable), prot_disc);
	
	/* Decode Call Reference */
	lenCallRef = (uint8_t) (data[1] & 0x0F);

	str_len += sprintf(&str[str_len], "  Call Ref:");
	c=2;
	callRefFlag = get_bits(data[c], 8,8);
	for(i=0; i<(2*lenCallRef);i++) {
		if(i==0) {
			str_len += sprintf(&str[str_len], "%s%s",
						get_code_2_str((uint8_t)(data[c] & 0x70), dcodQ931CallRefHiTable),
						get_code_2_str((uint8_t)(data[c] & 0x0F), dcodQ931CallRefLoTable));
		} else {
			str_len += sprintf(&str[str_len], "%s%s",
						get_code_2_str((uint8_t)(data[c] & 0xF0), dcodQ931CallRefHiTable),
						get_code_2_str((uint8_t)(data[c] & 0x0F), dcodQ931CallRefLoTable));
		}

		i=i+1;
		c=c+1;
	}
	str_len += sprintf(&str[str_len], " (%s side)\n", callRefFlag?"Destination":"Originating");

	/* Decode message type */
	str_len+= sprintf(&str[str_len], "  Type:%s (0x%x)\n", get_code_2_str((int)(data[2+lenCallRef] & 0xFF), dcodQ931MsgTypeTable), (int)(data[2+lenCallRef] & 0xFF));

	/* go through rest of data and look for important info */
	for(i=3+lenCallRef; i < data_len; i++) {
		switch (data[i] & 0xF8) {
			case Q931_LOCKING_SHIFT:
				current_codeset = (data[i] & 0x7);
				str_len+= sprintf(&str[str_len], "Codeset shift to %d (locking)\n", current_codeset);
				continue;
			case Q931_NON_LOCKING_SHIFT:
				current_codeset = (data[i] & 0x7);
				str_len+= sprintf(&str[str_len], "Codeset shift to %d (non-locking)\n", current_codeset);
				continue;
		}
		i+= sngisdn_decode_ie(str, &str_len, current_codeset, data, i);
	}
	print_hex_dump(str, &str_len, (uint8_t*) data, 0, data_len);
	return;
}

uint32_t sngisdn_decode_ie(char *str, uint32_t *str_len, uint8_t current_codeset, uint8_t *data, uint16_t index_start)
{
	unsigned char* ieData;
	uint8_t ieId;
	uint32_t len = 0;
	int index_end;

	ieData = (unsigned char*) &data[index_start];

	ieId = OCTET(1);
	len = OCTET(2);	
	index_end = index_start+len+1;

	*str_len += sprintf(&str[*str_len], "  %s:", get_code_2_str(data[index_start], dcodQ931IEIDTable));
	switch(ieId) {
		case PROT_Q931_IE_BEARER_CAP:
			{
				uint8_t codingStandard, infTransferCap, infTransferRate, usrL1Prot;
				/*uint8_t transferMode;*/
				
				codingStandard = get_bits(OCTET(3),6,7);
				infTransferCap = get_bits(OCTET(3),1,5);
				/*transferMode = get_bits(OCTET(4),6,7);*/
				infTransferRate = get_bits(OCTET(4),1,5);
				usrL1Prot = get_bits(OCTET(5),1,5);
				
				*str_len+= sprintf(&str[*str_len], "Coding:%s(%d) TransferCap:%s(%d) TransferRate:%s(%d) L1Prot:%s(%d)\n",
															get_code_2_str(codingStandard, dcodQ931BcCodingStandardTable), codingStandard,
															get_code_2_str(infTransferCap, dcodQ931BcInfTransferCapTable), infTransferCap,
															get_code_2_str(infTransferRate, dcodQ931BcInfTransferRateTable), infTransferRate,
															get_code_2_str(usrL1Prot, dcodQ931BcusrL1ProtTable), usrL1Prot);
			}
			break;
		case PROT_Q931_IE_CAUSE:
			{
				uint8_t codingStandard, location, cause,diagOct = 5;
				codingStandard = get_bits(OCTET(3),6,7);
				location = get_bits(OCTET(3),1,4);
				
				cause = get_bits(OCTET(4),1,7);

				*str_len+= sprintf(&str[*str_len], "coding:%s(%d) location:%s(%d) val:%s(%d)\n",
											get_code_2_str(codingStandard, dcodQ931BcCodingStandardTable), codingStandard,
											get_code_2_str(location,dcodQ931IelocationTable), location,
											get_code_2_str(cause, dcodQ931CauseCodeTable),
											cause);
				switch(cause) {
					case PROT_Q931_RELEASE_CAUSE_IE_NOT_EXIST:
						while(diagOct++ < len) {
							*str_len+= sprintf(&str[*str_len], "  %d:IE %s(0x%02x)\n",
															diagOct,
															get_code_2_str(OCTET(diagOct), dcodQ931IEIDTable),
															OCTET(diagOct));
						}
						break;
					case PROT_Q931_RELEASE_CAUSE_WRONG_CALL_STATE:
						while(diagOct++ < len) {
							*str_len+= sprintf(&str[*str_len], "  %d:Message %s(0x%02x)\n",
															diagOct,
															get_code_2_str(OCTET(diagOct), dcodQ931MsgTypeTable),
															OCTET(diagOct));
						}
						break;
					case PROT_Q931_RECOVERY_ON_TIMER_EXPIRE:
						*str_len+= sprintf(&str[*str_len], "  Timer T\n");
						while(diagOct++ < len) {
							if(OCTET(diagOct) >= ' ' && OCTET(diagOct) < 0x7f) {
								*str_len+= sprintf(&str[*str_len], "%c", OCTET(diagOct));
							} else {
								*str_len+= sprintf(&str[*str_len], ".");
							}
						}
						break;
					default:
						while(diagOct++ < len) {
							*str_len+= sprintf(&str[*str_len], " %d: 0x%02x\n",
																	diagOct,
																	OCTET(diagOct));
						}
						break;
				}
			}		
			break;		
		case PROT_Q931_IE_CHANNEL_ID:
			{
				uint8_t infoChannelSelection=0;
				uint8_t prefExclusive=0;
				uint8_t ifaceIdPresent=0;
				/* uint8_t ifaceIdentifier = 0; */ /* octet_3_1 */
				uint8_t chanType=0, numberMap=0;
				/* uint8_t codingStandard=0; */
				uint8_t channelNo = 0;
				
				infoChannelSelection = get_bits(OCTET(3),1,2);
				prefExclusive = get_bits(OCTET(3),4,4);
				ifaceIdPresent = get_bits(OCTET(3),7,7);
	
				if (ifaceIdPresent) {
					/*ifaceIdentifier= get_bits(OCTET(4),1,7);*/
					chanType = get_bits(OCTET(5),1,4);
					numberMap = get_bits(OCTET(5),5,5);
					/*codingStandard = get_bits(OCTET(5),6,7);*/
					channelNo = get_bits(OCTET(6),1,7);
				} else {
					chanType = get_bits(OCTET(4),1,4);
					numberMap = get_bits(OCTET(4),5,5);
					/*codingStandard = get_bits(OCTET(4),6,7);*/
					channelNo = get_bits(OCTET(5),1,7);
				}
				
				if (numberMap) {
					*str_len+= sprintf(&str[*str_len], " MAP:%s ", get_code_2_str(infoChannelSelection, dcodQ931InfoChannelSelTable));
				} else {
					*str_len+= sprintf(&str[*str_len], "No:%d ", channelNo);
				}
	
				*str_len+= sprintf(&str[*str_len], "Type:%s(%d) %s ", get_code_2_str(chanType,dcodQ931ChanTypeTable), chanType, (numberMap)? "Map":"");
				*str_len+= sprintf(&str[*str_len], "%s/%s \n",
									(prefExclusive)? "Exclusive":"Preferred", 
									(ifaceIdPresent)? "Explicit":"Implicit");
			}
			break;
		case PROT_Q931_IE_CALLING_PARTY_NUMBER:
			{
				uint8_t plan, type, screening = 0, presentation = 0, callingNumOct, j;
				uint8_t screeningEnabled = 0, presentationEnabled = 0;
				char callingNumDigits[32];
				memset(callingNumDigits, 0, sizeof(callingNumDigits));
				
				plan = get_bits(OCTET(3),1,4);
				type = get_bits(OCTET(3),5,7);

				if(!get_bits(OCTET(3),8,8)) {
					screening = get_bits(OCTET(4),1,2);
					presentation = get_bits(OCTET(4),6,7);
					screeningEnabled = 1;
					presentationEnabled = 1;
					callingNumOct = 4;
				} else {
					callingNumOct = 3;
				}
				if(len >= sizeof(callingNumDigits)) {	
					len = sizeof(callingNumDigits)-1;
				}
				j = 0;
				while(callingNumOct++ <= len+1) {
					callingNumDigits[j++]=ia5[get_bits(OCTET(callingNumOct),1,4)][get_bits(OCTET(callingNumOct),5,8)];
				}
				callingNumDigits[j]='\0';
				*str_len+= sprintf(&str[*str_len], "%s(l:%d) plan:%s(%d) type:%s(%d)",
															 
															callingNumDigits, j,
															get_code_2_str(plan, dcodQ931NumberingPlanTable), plan,
															get_code_2_str(type, dcodQ931TypeofNumberTable), type);
															
				if (presentationEnabled||screeningEnabled) {
					*str_len+= sprintf(&str[*str_len], "scr:%s(%d) pres:%s(%d)\n",
														get_code_2_str(screening, dcodQ931ScreeningTable),	screening,
														get_code_2_str(presentation, dcodQ931PresentationTable), presentation);
				} else {
					*str_len+= sprintf(&str[*str_len], "\n");
				}
			}
			break;
		
		case PROT_Q931_IE_CALLED_PARTY_NUMBER:
			{
				uint8_t plan, type, calledNumOct,j;
				char calledNumDigits[32];
				memset(calledNumDigits, 0, sizeof(calledNumDigits));
				plan = get_bits(OCTET(3),1,4);
				type = get_bits(OCTET(3),5,7);

				if(len >= sizeof(calledNumDigits)) {	
					len = sizeof(calledNumDigits)-1;
				}
				calledNumOct = 3;
				j = 0;
				while(calledNumOct++ <= len+1) {
					calledNumDigits[j++]=ia5[get_bits(OCTET(calledNumOct),1,4)][get_bits(OCTET(calledNumOct),5,8)];
				}
				calledNumDigits[j]='\0';
				*str_len+= sprintf(&str[*str_len], "%s(l:%d) plan:%s(%d) type:%s(%d)\n",
														calledNumDigits, j,
														get_code_2_str(plan, dcodQ931NumberingPlanTable), plan,
														get_code_2_str(type, dcodQ931TypeofNumberTable), type);
			}
			break;
		case PROT_Q931_IE_REDIRECTING_NUMBER: //rdnis
			{
				uint8_t plan, type, screening = 0, presentation = 0, reason = 0, rdnisOct,j;
				uint8_t screeningEnabled = 0, presentationEnabled = 0, reasonEnabled = 0;
				char rdnis_string[32];
				memset(rdnis_string, 0, sizeof(rdnis_string));
				rdnisOct = 5;
				plan = get_bits(OCTET(3),1,4);
				type = get_bits(OCTET(3),5,7);
			
				if(!get_bits(OCTET(3),8,8)) { //Oct 3a exists
					rdnisOct++;
					screening = get_bits(OCTET(4),1,2);
					presentation = get_bits(OCTET(4),6,7);
					screeningEnabled = 1;
					presentationEnabled = 1;
					if (!get_bits(OCTET(4),8,8)) { //Oct 3b exists
						rdnisOct++;
						reason = get_bits(OCTET(5),1,4);
						reasonEnabled = 1;
					}
				} 
	
				if(len >= sizeof(rdnis_string)) {	
					len = sizeof(rdnis_string)-1;
				}
				
				j = 0;
				while(rdnisOct++ <= len+1) {
					rdnis_string[j++]=ia5[get_bits(OCTET(rdnisOct),1,4)][get_bits(OCTET(rdnisOct),5,8)];
				}
	
				rdnis_string[j]='\0';	
				*str_len+= sprintf(&str[*str_len], "%s(l:%d) plan:%s(%d) type:%s(%d)",
															rdnis_string, j,
															get_code_2_str(plan, dcodQ931NumberingPlanTable), plan,
															get_code_2_str(type, dcodQ931TypeofNumberTable), type);
															
				if(presentationEnabled || screeningEnabled) {
					*str_len+= sprintf(&str[*str_len], "scr:%s(%d) pres:%s(%d)",
														get_code_2_str(screening, dcodQ931ScreeningTable),	screening,
														get_code_2_str(presentation, dcodQ931PresentationTable), presentation);
				}
	
				if(reasonEnabled) {
					*str_len+= sprintf(&str[*str_len], "reason:%s(%d)",
														get_code_2_str(reason, dcodQ931ReasonTable), reason);
				}
				*str_len+= sprintf(&str[*str_len], "\n");
			}
			break;
		case PROT_Q931_IE_USER_USER:
			{
				uint8_t protDiscr = 0x00, j, uui_stringOct;
				char uui_string[32];
				memset(uui_string, 0, sizeof(uui_string));
				protDiscr = OCTET(3);
				uui_stringOct = 3;
				if (protDiscr != 0x04) { /* Non-IA5 */
					*str_len+= sprintf(&str[*str_len], "%s (0x%02x)\n",
															get_code_2_str(protDiscr, dcodQ931UuiProtDiscrTable), protDiscr);
				} else {
					j = 0;
					
					if(len >= sizeof(uui_string)) {	
						len = sizeof(uui_string)-1;
					}
					while(uui_stringOct++ <= len+1) {
						uui_string[j++]=ia5[get_bits(OCTET(uui_stringOct),1,4)][get_bits(OCTET(uui_stringOct),5,8)];
					}
					uui_string[j]='\0';	
					*str_len+= sprintf(&str[*str_len], "  %s (0x%02x) <%s>\n",
															get_code_2_str(protDiscr, dcodQ931UuiProtDiscrTable), protDiscr,
															uui_string);
				}
			}
			break;
		case PROT_Q931_IE_DISPLAY:
			{
				uint8_t j;
				char displayStr[82];
				uint8_t displayNtEnabled = 0;
				uint8_t displayStrOct = 2;
				uint8_t displayType = 0;
				uint8_t assocInfo = 0;
				
				memset(displayStr, 0, sizeof(displayStr));
				
				if(get_bits(OCTET(3),8,8)) {
					displayType = get_bits(OCTET(3),1,4);
					assocInfo = get_bits(OCTET(3),5,7);

					displayNtEnabled = 1;
					displayStrOct++;
				}
				j = 0;	
				if(len >= sizeof(displayStr)) {	
					len = sizeof(displayStr)-1;
				}
				while(displayStrOct++ <= len+1) {
					displayStr[j++]=ia5[get_bits(OCTET(displayStrOct),1,4)][get_bits(OCTET(displayStrOct),5,8)];
				}
				displayStr[j]='\0';
				if (displayNtEnabled) {
					*str_len+= sprintf(&str[*str_len], "%s(l:%d) type:%s(%d) info:%s(%d)\n",
												displayStr, len,
												get_code_2_str(displayType, dcodQ931DisplayTypeTable), displayType,
												get_code_2_str(assocInfo, dcodQ931AssocInfoTable), assocInfo);
				} else {
					*str_len+= sprintf(&str[*str_len], "%s(l:%d)\n",
															displayStr, len);
				}
			}
			break;
		case PROT_Q931_IE_RESTART_IND:
			{
				uint8_t indClass;
				indClass = get_bits(OCTET(3),1,3);
				*str_len+= sprintf(&str[*str_len], "class:%s(%d)\n",
													get_code_2_str(indClass,dcodQ931RestartIndClassTable), indClass);
			}
			break;
		case PROT_Q931_IE_PROGRESS_IND:
			{
				uint8_t codingStandard, location, progressDescr;
				codingStandard = get_bits(OCTET(3),6,7);
				location = get_bits(OCTET(3),1,4);
				progressDescr = get_bits(OCTET(4),1,7);
				*str_len+= sprintf(&str[*str_len], "coding:%s(%d) location:%s(%d) descr:%s(%d)\n",
													get_code_2_str(codingStandard,dcodQ931BcCodingStandardTable), codingStandard,
													get_code_2_str(location,dcodQ931IelocationTable), location,
													get_code_2_str(progressDescr,dcodQ931IeprogressDescrTable), progressDescr);
			}
			break;
		case PROT_Q931_IE_KEYPAD_FACILITY:
			{
				uint8_t keypadFacilityStrOct = 3, j;
				char keypadFacilityStr[82];
				memset(keypadFacilityStr, 0, sizeof(keypadFacilityStr));
				
				j = 0;	
				if(len >= sizeof(keypadFacilityStr)) {	
					len = sizeof(keypadFacilityStr)-1;
				}
				while(keypadFacilityStrOct++ < len+1) {
					keypadFacilityStr[j++]=ia5[get_bits(OCTET(keypadFacilityStrOct),1,4)][get_bits(OCTET(keypadFacilityStrOct),5,8)];
				}
				keypadFacilityStr[j]='\0';
				*str_len+= sprintf(&str[*str_len], "  digits:%s(l:%d)\n",
														keypadFacilityStr, len);
			}
			break;
		case PROT_Q931_IE_FACILITY:
			{
				uint8_t protProfile;
				protProfile = get_bits(OCTET(3),1,5);
				*str_len+= sprintf(&str[*str_len], "Prot profile:%s(%d)\n",
													get_code_2_str(protProfile,dcodQ931IeFacilityProtProfileTable), protProfile);
			}
			break;
		case PROT_Q931_IE_GENERIC_DIGITS:
			{
				uint8_t encoding,type;
				int value = 0;

				encoding = get_bits(OCTET(3),6,8);
				type = get_bits(OCTET(3),1,5);

				*str_len+= sprintf(&str[*str_len], "encoding:%s(%d) type:%s(%d) ",
													get_code_2_str(encoding,dcodQ931GenDigitsEncodingTable), encoding,
													get_code_2_str(encoding,dcodQ931GenDigitsTypeTable), type);

				if (len > 1) {
					uint32_t j=0;
					
					while(++j < len) {
						switch(encoding) {
							case 0: /* BCD even */
							case 1: /* BCD odd */
								{
									uint8_t byte = OCTET(j+3);
									value = (get_bits(byte,1,4)*10) + get_bits(byte,5,8) + (value*10);
								}
								break;
							case 2:	/* IA 5 */							
								value = value*10 + OCTET(j+3)-'0';
								*str_len+= sprintf(&str[*str_len], "%c", OCTET(j+3));
								break;
							case 3: 
								/* Don't know how to decode binary encoding yet */
								*str_len+= sprintf(&str[*str_len], "Binary encoded");
								break;
						}
					}
					*str_len+= sprintf(&str[*str_len], " ");
					switch(type) {
						case 4: /* info digits */
							*str_len+= sprintf(&str[*str_len], "ani2:%s(%d)", get_code_2_str(value,dcodQ931LineInfoTable), value);
							break;
						case 5: /* Callid */
							*str_len+= sprintf(&str[*str_len], "Caller ID not implemented\n");
							break;
					}
				}
				*str_len+= sprintf(&str[*str_len], "\n");
				print_hex_dump(str, str_len, (uint8_t*) data, index_start, index_end);
			}
			break;
		case PROT_Q931_IE_SENDING_COMPLETE:
			/* No need to decode sending complete IE, as no additional info is available except that sending is done */
			/* This is a single octet IE */
			*str_len+= sprintf(&str[*str_len], "\n");
			return 0;
			break;
		case PROT_Q931_IE_CALLED_PARTY_SUBADDRESS:
			{
				uint8_t type;
				uint8_t currentOct, j=0;
				char calling_subaddr_string[82];
				memset(calling_subaddr_string, 0, sizeof(calling_subaddr_string));
				type = get_bits(OCTET(3),5,7);
				currentOct = 3;
				while(currentOct++ <= len+1) {
						calling_subaddr_string[j++]=ia5[get_bits(OCTET(currentOct),1,4)][get_bits(OCTET(currentOct),5,8)];
				}
				calling_subaddr_string[j++]='\0';
				*str_len += sprintf(&str[*str_len], "%s (l:%d) type:%s(%d) \n",
														calling_subaddr_string, (j-1), get_code_2_str(type, dcodQ931TypeOfSubaddressTable), type);
			}
			break;
		case PROT_Q931_IE_NOTIFICATION_IND:
			{
				uint8_t desc;

				desc = get_bits(OCTET(3),1,7);
				*str_len += sprintf(&str[*str_len], "%s (%d)\n",
														get_code_2_str(desc, dcodQ931NotificationIndTable), desc);
			}
			break;
		case PROT_Q931_IE_REDIRECTION_NUMBER:		
		case PROT_Q931_IE_DATE_TIME:
		case PROT_Q931_IE_INFORMATION_REQUEST:
		case PROT_Q931_IE_SIGNAL:
		case PROT_Q931_IE_SWITCHOOK:
		case PROT_Q931_IE_FEATURE_ACT:
		case PROT_Q931_IE_FEATURE_IND:
		case PROT_Q931_IE_INFORMATION_RATE:
		case PROT_Q931_IE_END_TO_END_TRANSIT_DELAY:
		case PROT_Q931_IE_TRANSIT_DELAY_SELECT_IND:
		case PROT_Q931_IE_PACKET_LAYER_BINARY_PARAMS:
		case PROT_Q931_IE_PACKET_LAYER_WINDOW_SIZE:
		case PROT_Q931_IE_PACKET_LAYER_SIZE:
		case PROT_Q931_IE_TRANSIT_NETWORK_SELECTION:
		case PROT_Q931_IE_LOW_LAYER_COMPAT:
		case PROT_Q931_IE_HIGH_LAYER_COMPAT:
		case PROT_Q931_IE_ESCAPE_FOR_EXTENSION:
		case PROT_Q931_IE_CALL_IDENTITY:
		case PROT_Q931_IE_CALL_STATE:
		case PROT_Q931_IE_SEGMENTED_MESSAGE:
		case PROT_Q931_IE_NETWORK_SPF_FACILITY:
		case PROT_Q931_IE_CALLING_PARTY_SUBADDRESS:
		default:
			{
				*str_len += sprintf(&str[*str_len], "Undecoded");
				print_hex_dump((char*)str, str_len, data, index_start, index_end + 1);
			}
			break;
	}

	return len+1;
}

void print_hex_dump(char* str, uint32_t *str_len, uint8_t* data, uint32_t index_start, uint32_t index_end)
{
	uint32_t k;
	*str_len += sprintf(&str[*str_len], "  [ ");
	for(k=index_start; k < index_end; k++) {
		if (k && !(k%32)) {
			*str_len += sprintf(&str[*str_len], "\n    ");
		}
		*str_len += sprintf(&str[*str_len], "%02x ", data[k]);
	}
	*str_len += sprintf(&str[*str_len], "]\n");
	return;
}

static ftdm_status_t sngisdn_get_frame_info(uint8_t *data, uint32_t data_len, ftdm_trace_dir_t dir, sngisdn_frame_info_t *target)
{
	uint8_t pos = 0;
	uint8_t flag;
	uint16_t ref = 0;
	uint8_t ref_len = 0;
	uint8_t bchan_no = 0;
	uint8_t msgtype;

	/* First octet is protocol discriminator */
	pos++;
	/* Second octet contains length of call reference */
	ref_len = data[pos++] & 0x0F;

	/* third octet is call reference */
	flag = (data[pos] & 0x80) >> 7;
	if (ref_len == 2) {
		ref = (data[pos++] & 0x7F) << 8;
		ref |= (data[pos++] & 0xFF) ;
	} else {
		ref = (data[pos++] & 0x7F);
	}

	/* Next octet is the message type */
	msgtype = data[pos++] & 0x7F;
	
	/*
		ftdm_log(FTDM_LOG_DEBUG, "Raw frame:call_ref:0x%04x flag:%d msgtype:%d\n", ref, flag, msgtype);
	*/
	if (!ref) {
		/* This is not a call specific message (RESTART for example and we do not care about it) */
		return FTDM_FAIL;
	}

	/* Look for the b-channel */
	if (msgtype == PROT_Q931_MSGTYPE_SETUP) {
		/* Try to find the b-channel no*/

		for(; pos < data_len; pos++) {
			uint8_t ie_id = data[pos];
			uint8_t ie_len = data[pos+1];

			switch(ie_id) {
				case PROT_Q931_IE_SENDING_COMPLETE:
					/* Single octet ie's do not have a length */
					ie_len = 0;
					break;
				case PROT_Q931_IE_CHANNEL_ID:
					{
						/* Try to obtain the b-channel */
						uint8_t ie_pos = pos+2;					
						//ifaceIdPresent = get_bits(OCTET(3),7,7);
						if (data[ie_pos] & 0x20) {
							/* Interface type is Primary Rate */
							ie_pos+=2;
							bchan_no = data[ie_pos] & 0x7F;
						} else {
							/* Interface type is Basic Interface */
							/* Get the channel number from info channel selection */
							bchan_no = data[ie_pos] & 0x03;
						}
						ftdm_log(FTDM_LOG_DEBUG, "Found b-channel:%d\n", bchan_no);
						goto parse_ies_done;
					}
					break;
				default:
					pos = pos+ie_len+1;
			}
			//ftdm_log(FTDM_LOG_DEBUG, "Decoded IE:%s\n", get_code_2_str(ie_id, dcodQ931IEIDTable));
		}
		if (!bchan_no) {
			uint32_t tmp_len = 0;
			char tmp[1000];
			print_hex_dump(tmp, &tmp_len, data, 0, data_len);			
			ftdm_log(FTDM_LOG_DEBUG, "Failed to determine b-channel on SETUP message\n%s\n", tmp);
		}
	}

parse_ies_done:

	target->call_ref = ref;
	target->call_ref_flag = flag;
	target->msgtype = msgtype;
	target->bchan_no = bchan_no;
	target->dir = dir;

	return FTDM_SUCCESS;
}

static ftdm_status_t sngisdn_map_call(sngisdn_span_data_t *signal_data, sngisdn_frame_info_t frame_info, ftdm_channel_t **found)
{
	sngisdn_chan_data_t *sngisdn_info;
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *curr = NULL;
	ftdm_status_t status = FTDM_FAIL;
	uint8_t outbound_call = 0;
	
	if ((!frame_info.call_ref_flag && frame_info.dir == FTDM_TRACE_DIR_OUTGOING) ||
		(frame_info.call_ref_flag && frame_info.dir == FTDM_TRACE_DIR_INCOMING)) {

		/* If this is an outgoing frame and this frame was sent by the originating side
			of the call (frame_info.call_ref_flag == 0), then this is an outbound call */
		outbound_call = 1;
	} else {
		outbound_call = 0;
	}

	switch (frame_info.msgtype) {
		case PROT_Q931_MSGTYPE_SETUP:
			/* We initiated this outgoing call try to match the call reference with our internal call-id*/
			if (!frame_info.bchan_no) {
				/* We were not able to determine the bchannel on this call, so we will not be able to match it anyway */
				status = FTDM_FAIL;
			}
			
			chaniter = ftdm_span_get_chan_iterator(signal_data->ftdm_span, NULL);
			for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
				ftdmchan = (ftdm_channel_t*)(ftdm_iterator_current(curr));
				ftdm_channel_lock(ftdmchan);
				
				if (outbound_call) {
					sngisdn_info = (sngisdn_chan_data_t*)ftdmchan->call_data;
					if (sngisdn_info && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
						if (ftdmchan->caller_data.call_id && ftdmchan->physical_chan_id == frame_info.bchan_no) {

							sngisdn_info->call_ref = frame_info.call_ref;
							*found = ftdmchan;
							status = FTDM_SUCCESS;
						}
					}
				} else {
					if (ftdmchan->physical_chan_id == frame_info.bchan_no) {
						*found = ftdmchan;
						status = FTDM_SUCCESS;
					}
				}
				ftdm_channel_unlock(ftdmchan);
			}
			ftdm_iterator_free(chaniter);
			break;
		case PROT_Q931_MSGTYPE_ALERTING:
		case PROT_Q931_MSGTYPE_PROCEEDING:
		case PROT_Q931_MSGTYPE_PROGRESS:
		case PROT_Q931_MSGTYPE_CONNECT:
		case PROT_Q931_MSGTYPE_SETUP_ACK:
		case PROT_Q931_MSGTYPE_CONNECT_ACK:
		case PROT_Q931_MSGTYPE_USER_INFO:
		case PROT_Q931_MSGTYPE_DISCONNECT:
		case PROT_Q931_MSGTYPE_RELEASE:
		case PROT_Q931_MSGTYPE_RESTART_ACK:
		case PROT_Q931_MSGTYPE_RELEASE_COMPLETE:
		case PROT_Q931_MSGTYPE_FACILITY:
		case PROT_Q931_MSGTYPE_NOTIFY:
		case PROT_Q931_MSGTYPE_STATUS_ENQUIRY:
		case PROT_Q931_MSGTYPE_INFORMATION:
		case PROT_Q931_MSGTYPE_STATUS:
			/* Look for an outbound call on that span and and try to match the call-id */
			chaniter = ftdm_span_get_chan_iterator(signal_data->ftdm_span, NULL);
			for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
				ftdmchan = (ftdm_channel_t*)(ftdm_iterator_current(curr));
				ftdm_channel_lock(ftdmchan);
				sngisdn_info = (sngisdn_chan_data_t*)ftdmchan->call_data;
				if (outbound_call) {
					if (sngisdn_info && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
						if (sngisdn_info->call_ref == frame_info.call_ref) {

							*found = ftdmchan;
							status = FTDM_SUCCESS;
						}
					}
				} else {
					if (sngisdn_info && !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
						if (sngisdn_info->call_ref && sngisdn_info->call_ref == frame_info.call_ref) {

							*found = ftdmchan;
							status = FTDM_SUCCESS;
						}
					}
				}
				ftdm_channel_unlock(ftdmchan);
			}
			ftdm_iterator_free(chaniter);
			break;
		default:
			/* This frame is not call specific, ignore */
			break;
	}
	if (status == FTDM_SUCCESS) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Mapped %s with Call Ref:%04x to call-id:%d\n", get_code_2_str(frame_info.msgtype, dcodQ931MsgTypeTable), frame_info.call_ref, (*found)->caller_data.call_id);
	} else {
		/* We could not map this frame to a call-id */
		ftdm_log(FTDM_LOG_DEBUG, "Failed to map %s with Call Ref:%04x to local call\n",
				 get_code_2_str(frame_info.msgtype, dcodQ931MsgTypeTable), frame_info.call_ref);
	}

	return status;
}



