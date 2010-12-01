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

void print_hex_dump(char* str, uint32_t *str_len, uint8_t* data, uint32_t index_start, uint32_t index_end);
void sngisdn_trace_q921(char* str, uint8_t* data, uint32_t data_len);
void sngisdn_trace_q931(char* str, uint8_t* data, uint32_t data_len);
uint32_t sngisdn_decode_ie(char *str, uint32_t *str_len, uint8_t current_codeset, uint8_t *data, uint16_t index_start);

uint8_t get_bits(uint8_t octet, uint8_t bitLo, uint8_t bitHi);
char* get_code_2_str(int code, struct code2str *pCodeTable);

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

void sngisdn_trace_q921(char* str, uint8_t* data, uint32_t data_len)
{
	int str_len;
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
	return;
}

void sngisdn_trace_q931(char* str, uint8_t* data, uint32_t data_len)
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
				uint8_t codingStandard, infTransferCap, transferMode, infTransferRate, usrL1Prot;
				
				codingStandard = get_bits(OCTET(3),6,7);
				infTransferCap = get_bits(OCTET(3),1,5);
				transferMode = get_bits(OCTET(4),6,7);
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
				uint8_t ifaceIdentifier = 0; /* octet_3_1 */
				uint8_t chanType=0, numberMap=0, codingStandard=0;
				uint8_t channelNo = 0;
				
				infoChannelSelection = get_bits(OCTET(3),1,2);
				prefExclusive = get_bits(OCTET(3),4,4);
				ifaceIdPresent = get_bits(OCTET(3),7,7);
	
				if (ifaceIdPresent) {
					ifaceIdentifier= get_bits(OCTET(4),1,7);
					chanType = get_bits(OCTET(5),1,4);
					numberMap = get_bits(OCTET(5),5,5);
					codingStandard = get_bits(OCTET(5),6,7);
					channelNo = get_bits(OCTET(6),1,7);
				} else {
					chanType = get_bits(OCTET(4),1,4);
					numberMap = get_bits(OCTET(4),5,5);
					codingStandard = get_bits(OCTET(4),6,7);
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
				uint8_t displayStrOct=2, j;
				char displayStr[82];
				memset(displayStr, 0, sizeof(displayStr));
				
				if(get_bits(OCTET(3),8,8)) {
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
				*str_len+= sprintf(&str[*str_len], "%s(l:%d)\n",
														displayStr, len);
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
		case PROT_Q931_IE_REDIRECTION_NUMBER:
		case PROT_Q931_IE_NOTIFICATION_IND:
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
				print_hex_dump((char*)str, str_len, data, index_start, index_end);
			}
			break;
	}

	return len+1;
}

void print_hex_dump(char* str, uint32_t *str_len, uint8_t* data, uint32_t index_start, uint32_t index_end)
{
	uint32_t k;
	*str_len += sprintf(&str[*str_len], "  [ ");
	for(k=index_start; k <= index_end; k++) {
		if (k && !(k%32)) {
			*str_len += sprintf(&str[*str_len], "\n    ");
		}
		*str_len += sprintf(&str[*str_len], "%02x ", data[k]);
	}
	*str_len += sprintf(&str[*str_len], "]\n");
	return;
}


