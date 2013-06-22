/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2010, Mathieu Parent <math.parent@gmail.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Mathieu Parent <math.parent@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Parent <math.parent@gmail.com>
 *
 * Based on chan-sccp-b (file src/sccp_labels.h)
 *
 * skinny_labels.h -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */
#ifndef _SKINNY_LABELS_H
#define _SKINNY_LABELS_H

#define SKINNY_DISP_EMPTY                                		""
#define SKINNY_DISP_REDIAL                                		"\200\1"
#define SKINNY_DISP_NEWCALL                               		"\200\2"
#define SKINNY_DISP_HOLD                                  		"\200\3"
#define SKINNY_DISP_TRANSFER                              		"\200\4"
#define SKINNY_DISP_CFWDALL                               		"\200\5"
#define SKINNY_DISP_CFWDBUSY                              		"\200\6"
#define SKINNY_DISP_CFWDNOANSWER                          		"\200\7"
#define SKINNY_DISP_BACKSPACE                             		"\200\10"
#define SKINNY_DISP_ENDCALL                               		"\200\11"
#define SKINNY_DISP_RESUME                                		"\200\12"
#define SKINNY_DISP_ANSWER                                		"\200\13"
#define SKINNY_DISP_INFO                                  		"\200\14"
#define SKINNY_DISP_CONF                                		"\200\15"
#define SKINNY_DISP_PARK                                  		"\200\16"
#define SKINNY_DISP_JOIN                                  		"\200\17"
#define SKINNY_DISP_MEETME                                		"\200\20"
#define SKINNY_DISP_CALLPICKUP                                		"\200\21"
#define SKINNY_DISP_GRPCALLPICKUP                             		"\200\22"
#define SKINNY_DISP_YOUR_CURRENT_OPTIONS                  		"\200\23"
#define SKINNY_DISP_OFF_HOOK                              		"\200\24"
#define SKINNY_DISP_ON_HOOK                               		"\200\25"
#define SKINNY_DISP_RING_OUT                              		"\200\26"
#define SKINNY_DISP_FROM                                  		"\200\27"
#define SKINNY_DISP_CONNECTED                             		"\200\30"
#define SKINNY_DISP_BUSY                                  		"\200\31"
#define SKINNY_DISP_LINE_IN_USE                           		"\200\32"
#define SKINNY_DISP_CALL_WAITING                          		"\200\33"
#define SKINNY_DISP_CALL_TRANSFER                         		"\200\34"
#define SKINNY_DISP_CALL_PARK                             		"\200\35"
#define SKINNY_DISP_CALL_PROCEED                          		"\200\36"
#define SKINNY_DISP_IN_USE_REMOTE                         		"\200\37"
#define SKINNY_DISP_ENTER_NUMBER                          		"\200\40"
#define SKINNY_DISP_CALL_PARK_AT                          		"\200\41"
#define SKINNY_DISP_PRIMARY_ONLY                          		"\200\42"
#define SKINNY_DISP_TEMP_FAIL                             		"\200\43"
#define SKINNY_DISP_YOU_HAVE_VOICEMAIL                    		"\200\44"
#define SKINNY_DISP_FORWARDED_TO                          		"\200\45"
#define SKINNY_DISP_CAN_NOT_COMPLETE_CONFERENCE           		"\200\46"
#define SKINNY_DISP_NO_CONFERENCE_BRIDGE                  		"\200\47"
#define SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL          		"\200\50"
#define SKINNY_DISP_INVALID_CONFERENCE_PARTICIPANT        		"\200\51"
#define SKINNY_DISP_IN_CONFERENCE_ALREADY                 		"\200\52"
#define SKINNY_DISP_NO_PARTICIPANT_INFO                   		"\200\53"
#define SKINNY_DISP_EXCEED_MAXIMUM_PARTIES                		"\200\54"
#define SKINNY_DISP_KEY_IS_NOT_ACTIVE                     		"\200\55"
#define SKINNY_DISP_ERROR_NO_LICENSE                      		"\200\56"
#define SKINNY_DISP_ERROR_DBCONFIG                        		"\200\57"
#define SKINNY_DISP_ERROR_DATABASE                        		"\200\60"
#define SKINNY_DISP_ERROR_PASS_LIMIT                      		"\200\61"
#define SKINNY_DISP_ERROR_UNKNOWN                         		"\200\62"
#define SKINNY_DISP_ERROR_MISMATCH                        		"\200\63"
#define SKINNY_DISP_CONFERENCE                            		"\200\64"
#define SKINNY_DISP_PARK_NUMBER                           		"\200\65"
#define SKINNY_DISP_PRIVATE                               		"\200\66"
#define SKINNY_DISP_NOT_ENOUGH_BANDWIDTH                  		"\200\67"
#define SKINNY_DISP_UNKNOWN_NUMBER                        		"\200\70"
#define SKINNY_DISP_RMLSTC                                		"\200\71"
#define SKINNY_DISP_VOICEMAIL                             		"\200\72"
#define SKINNY_DISP_IMMDIV                                		"\200\73"
#define SKINNY_DISP_INTRCPT                               		"\200\74"
#define SKINNY_DISP_SETWTCH                               		"\200\75"
#define SKINNY_DISP_TRNSFVM                               		"\200\76"
#define SKINNY_DISP_DND                                   		"\200\77"
#define SKINNY_DISP_DIVALL                                		"\200\100"
#define SKINNY_DISP_CALLBACK                              		"\200\101"
#define SKINNY_DISP_NETWORK_CONGESTION_REROUTING          		"\200\102"
#define SKINNY_DISP_BARGE                                 		"\200\103"
#define SKINNY_DISP_FAILED_TO_SETUP_BARGE                 		"\200\104"
#define SKINNY_DISP_ANOTHER_BARGE_EXISTS                  		"\200\105"
#define SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE              		"\200\106"
#define SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE              		"\200\107"
#define SKINNY_DISP_CALLPARK_REVERSION                    		"\200\110"
#define SKINNY_DISP_SERVICE_IS_NOT_ACTIVE                 		"\200\111"
#define SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER          		"\200\112"
#define SKINNY_DISP_QRT                                   		"\200\113"
#define SKINNY_DISP_MCID                                  		"\200\114"
#define SKINNY_DISP_DIRTRFR                               		"\200\115"
#define SKINNY_DISP_SELECT                                		"\200\116"
#define SKINNY_DISP_CONFLIST                              		"\200\117"
#define SKINNY_DISP_IDIVERT                               		"\200\120"
#define SKINNY_DISP_CBARGE                                		"\200\121"
#define SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER             		"\200\122"
#define SKINNY_DISP_CAN_NOT_JOIN_CALLS                    		"\200\123"
#define SKINNY_DISP_MCID_SUCCESSFUL                       		"\200\124"
#define SKINNY_DISP_NUMBER_NOT_CONFIGURED                 		"\200\125"
#define SKINNY_DISP_SECURITY_ERROR                        		"\200\126"
#define SKINNY_DISP_VIDEO_BANDWIDTH_UNAVAILABLE           		"\200\127"
#define SKINNY_DISP_VIDMODE						"\200\130"
#define SKINNY_DISP_MAX_CALL_DURATION_TIMEOUT				"\200\131"
#define SKINNY_DISP_MAX_HOLD_DURATION_TIMEOUT				"\200\132"
#define SKINNY_DISP_OPICKUP						"\200\133"
#define SKINNY_DISP_EXTERNAL_TRANSFER_RESTRICTED			"\200\141"
#define SKINNY_DISP_MAC_ADDRESS						"\200\145"
#define SKINNY_DISP_HOST_NAME						"\200\146"
#define SKINNY_DISP_DOMAIN_NAME						"\200\147"
#define SKINNY_DISP_IP_ADDRESS						"\200\150"
#define SKINNY_DISP_SUBNET_MASK						"\200\151"
#define SKINNY_DISP_TFTP_SERVER_1					"\200\152"
#define SKINNY_DISP_DEFAULT_ROUTER_1					"\200\153"
#define SKINNY_DISP_DEFAULT_ROUTER_2					"\200\154"
#define SKINNY_DISP_DEFAULT_ROUTER_3					"\200\155"
#define SKINNY_DISP_DEFAULT_ROUTER_4					"\200\156"
#define SKINNY_DISP_DEFAULT_ROUTER_5					"\200\157"
#define SKINNY_DISP_DNS_SERVER_1					"\200\160"
#define SKINNY_DISP_DNS_SERVER_2					"\200\161"
#define SKINNY_DISP_DNS_SERVER_3					"\200\162"
#define SKINNY_DISP_DNS_SERVER_4					"\200\163"
#define SKINNY_DISP_DNS_SERVER_5					"\200\164"
#define SKINNY_DISP_OPERATIONAL_VLAN_ID					"\200\165"
#define SKINNY_DISP_ADMIN_VLAN_ID					"\200\166"
#define SKINNY_DISP_CALL_MANAGER_1					"\200\167"
#define SKINNY_DISP_CALL_MANAGER_2					"\200\170"
#define SKINNY_DISP_CALL_MANAGER_3					"\200\171"
#define SKINNY_DISP_CALL_MANAGER_4					"\200\172"
#define SKINNY_DISP_CALL_MANAGER_5					"\200\173"
#define SKINNY_DISP_INFORMATION_URL					"\200\174"
#define SKINNY_DISP_DIRECTORIES_URL					"\200\175"
#define SKINNY_DISP_MESSAGES_URL					"\200\176"
#define SKINNY_DISP_SERVICES_URL					"\200\177"

#endif /* _SKINNY_LABELS_H */

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
