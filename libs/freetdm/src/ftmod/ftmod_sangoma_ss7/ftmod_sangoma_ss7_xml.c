/*
 * Copyright (c) 2009, Konrad Hammel <konrad@sangoma.com>
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

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
typedef struct sng_timeslot
{
	int	 channel;
	int	 siglink;
	int	 gap;
	int	 hole;
}sng_timeslot_t;

typedef struct sng_isupCkt
{
	ftdm_span_t		*span;
	uint32_t		cicbase;
	uint32_t		typeCntrl;
	char			ch_map[MAX_CIC_MAP_LENGTH];
	uint32_t		isupInf;
	uint32_t		t3;
	uint32_t		t12;
	uint32_t		t13;
	uint32_t		t14;
	uint32_t		t15;
	uint32_t		t16;
	uint32_t		t17;
	uint32_t		tval;
} sng_isupCkt_t;

int cmbLinkSetId;
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
int ftmod_ss7_parse_xml(ftdm_conf_parameter_t *ftdm_parameters, ftdm_span_t *span);

static int ftmod_ss7_parse_sng_isup(ftdm_conf_node_t *sng_isup);

static int ftmod_ss7_parse_mtp_linksets(ftdm_conf_node_t *mtp_linksets);
static int ftmod_ss7_parse_mtp_linkset(ftdm_conf_node_t *mtp_linkset);
static int ftmod_ss7_parse_mtp_link(ftdm_conf_node_t *mtp_link, sng_mtp_link_t *mtpLink);

static int ftmod_ss7_parse_mtp_routes(ftdm_conf_node_t *mtp_routes);
static int ftmod_ss7_parse_mtp_route(ftdm_conf_node_t *mtp_route);

static int ftmod_ss7_parse_isup_interfaces(ftdm_conf_node_t *isup_interfaces);
static int ftmod_ss7_parse_isup_interface(ftdm_conf_node_t *isup_interface);

static int ftmod_ss7_fill_in_mtpLink(sng_mtp_link_t *mtpLink);

static int ftmod_ss7_fill_in_mtpLinkSet(sng_link_set_t *mtpLinkSet);

static int ftmod_ss7_fill_in_mtp3_route(sng_route_t *mtp3_route);
static int ftmod_ss7_fill_in_nsap(sng_route_t *mtp3_route);

static int ftmod_ss7_fill_in_isup_interface(sng_isup_inf_t *sng_isup);
static int ftmod_ss7_fill_in_isap(sng_isap_t *sng_isap);

static int ftmod_ss7_fill_in_self_route(int spc, int linkType, int switchType, int ssf);

static int ftmod_ss7_fill_in_circuits(sng_isupCkt_t *isupCkt);
static int ftmod_ss7_next_timeslot(char *ch_map, sng_timeslot_t *timeslot);

/******************************************************************************/

/* FUNCTIONS ******************************************************************/
int ftmod_ss7_parse_xml(ftdm_conf_parameter_t *ftdm_parameters, ftdm_span_t *span)
{
	int					i = 0;
	int					x = 0;
	const char			*var = NULL;
	const char			*val = NULL;
	ftdm_conf_node_t	*ptr = NULL;
	sng_route_t			self_route;
	sng_isupCkt_t		isupCkt;

	/* clean out the isup ckt */
	memset(&isupCkt, 0x0, sizeof(sng_isupCkt_t));

	/* clean out the self route */
	memset(&self_route, 0x0, sizeof(sng_route_t));

	var = ftdm_parameters[i].var;
	val = ftdm_parameters[i].val;
	ptr = (ftdm_conf_node_t *)ftdm_parameters[i].ptr;

	/* confirm that the first parameter is the "confnode" */
	if (!strcasecmp(var, "confnode")) {
		/* parse the confnode and fill in the global libsng_ss7 config structure */
		if (ftmod_ss7_parse_sng_isup(ptr)) {
			SS7_ERROR("Failed to parse the \"confnode\"!\n");
			goto ftmod_ss7_parse_xml_error;
		}
	} else {
		/* ERROR...exit */
		SS7_ERROR("The \"confnode\" configuration was not the first parameter!\n");
		SS7_ERROR("\tFound \"%s\" in the first slot\n", var);
		goto ftmod_ss7_parse_xml_error;
	}

	i++;
	while (ftdm_parameters[i].var != NULL) {

		var = ftdm_parameters[i].var;
		val = ftdm_parameters[i].val;

		if (!strcasecmp(var, "ch_map")) {
		/**********************************************************************/
			strcpy(isupCkt.ch_map, val);
			SS7_DEBUG("\tFound channel map \"%s\"\n", isupCkt.ch_map);
		/**********************************************************************/
		} else if (!strcasecmp(var, "typeCntrl")) {
			if (!strcasecmp(val, "bothway")) {
				isupCkt.typeCntrl = BOTHWAY;
				SS7_DEBUG("\tFound control type \"bothway\"\n");
			} else if (!strcasecmp(val, "incoming")) {
				isupCkt.typeCntrl = INCOMING;
				SS7_DEBUG("\tFound control type \"incoming\"\n");
			} else if (!strcasecmp(val, "outgoing")) {
				isupCkt.typeCntrl = OUTGOING;
				SS7_DEBUG("\tFound control type \"outgoing\"\n");
			} else if (!strcasecmp(val, "controlled")) {
				isupCkt.typeCntrl = CONTROLLED;
				SS7_DEBUG("\tFound control type \"controlled\"\n");
			} else if (!strcasecmp(val, "controlling")) {
				isupCkt.typeCntrl = CONTROLLING;
				SS7_DEBUG("\tFound control type \"controlling\"\n");
			} else {
				SS7_ERROR("Found invalid circuit control type \"%s\"!", val);
				goto ftmod_ss7_parse_xml_error;
			}
		/**********************************************************************/
		} else if (!strcasecmp(var, "cicbase")) {
			isupCkt.cicbase = atoi(val);
			SS7_DEBUG("\tFound cicbase = %d\n", isupCkt.cicbase);
		/**********************************************************************/
		} else if (!strcasecmp(var, "dialplan")) {
			/* do i give a shit about this??? */
		/**********************************************************************/
		} else if (!strcasecmp(var, "context")) {
			/* do i give a shit about this??? */
		/**********************************************************************/
		} else if (!strcasecmp(var, "isup_interface")) {
			/* go through all the existing interfaces and see if we find a match */
			x = 1;
			while (g_ftdm_sngss7_data.cfg.isupIntf[x].id != 0) {
				if (!strcasecmp(g_ftdm_sngss7_data.cfg.isupIntf[x].name, val)) {
					/* we have a match so break out of this loop */
					break;
				}
				/* move on to the next one */
				x++;
			}

			isupCkt.isupInf = x;
			SS7_DEBUG("\tFound isup_interface = %s\n",g_ftdm_sngss7_data.cfg.isupIntf[x].name);
		/**********************************************************************/
		} else if (!strcasecmp(var, "isup.t3")) {
			isupCkt.t3 = atoi(val);
			SS7_DEBUG("\tFound isup t3 = \"%d\"\n", isupCkt.t3);
		/**********************************************************************/
		} else if (!strcasecmp(var, "isup.t12")) {
			isupCkt.t12 = atoi(val);
			SS7_DEBUG("\tFound isup t12 = \"%d\"\n", isupCkt.t12);
		/**********************************************************************/
		} else if (!strcasecmp(var, "isup.t13")) {
			isupCkt.t13 = atoi(val);
			SS7_DEBUG("\tFound isup t13 = \"%d\"\n", isupCkt.t13);
		/**********************************************************************/
		} else if (!strcasecmp(var, "isup.t14")) {
			isupCkt.t14 = atoi(val);
			SS7_DEBUG("\tFound isup t14 = \"%d\"\n", isupCkt.t14);
		/**********************************************************************/
		} else if (!strcasecmp(var, "isup.t15")) {
			isupCkt.t15 = atoi(val);
			SS7_DEBUG("\tFound isup t15 = \"%d\"\n", isupCkt.t15);
		/**********************************************************************/
		} else if (!strcasecmp(var, "isup.t16")) {
			isupCkt.t16 = atoi(val);
			SS7_DEBUG("\tFound isup t16 = \"%d\"\n", isupCkt.t16);
		/**********************************************************************/
		} else if (!strcasecmp(var, "isup.t17")) {
			isupCkt.t17 = atoi(val);
			SS7_DEBUG("\tFound isup t17 = \"%d\"\n", isupCkt.t17);
		/**********************************************************************/
		} else if (!strcasecmp(var, "isup.tval")) {
			isupCkt.tval = atoi(val);
			SS7_DEBUG("\tFound isup tval = \"%d\"\n", isupCkt.tval);
		/**********************************************************************/
		} else {
			SS7_ERROR("Unknown parameter found =\"%s\"...ignoring it!\n", var);
		/**********************************************************************/
		}

		i++;
	} /* while (ftdm_parameters[i].var != NULL) */

	/* setup the self mtp3 route */
	i = g_ftdm_sngss7_data.cfg.isupIntf[x].mtpRouteId;

	if(ftmod_ss7_fill_in_self_route(g_ftdm_sngss7_data.cfg.isupIntf[x].spc,
									g_ftdm_sngss7_data.cfg.mtpRoute[i].linkType,
									g_ftdm_sngss7_data.cfg.mtpRoute[i].switchType,
									g_ftdm_sngss7_data.cfg.mtpRoute[i].ssf)) {

		SS7_ERROR("Failed to fill in self route structure!\n");
		goto ftmod_ss7_parse_xml_error;

	}

	/* fill the pointer to span into isupCkt */
	isupCkt.span = span;

	/* setup the circuits structure */
	if(ftmod_ss7_fill_in_circuits(&isupCkt)) {
		SS7_ERROR("Failed to fill in circuits structure!\n");
		goto ftmod_ss7_parse_xml_error;
	}

	return FTDM_SUCCESS;

ftmod_ss7_parse_xml_error:
	return FTDM_FAIL;
}

/******************************************************************************/
static int ftmod_ss7_parse_sng_isup(ftdm_conf_node_t *sng_isup)
{
	ftdm_conf_node_t	*mtp_linksets = NULL;
	ftdm_conf_node_t	*mtp_routes = NULL;
	ftdm_conf_node_t	*isup_interfaces = NULL;
	ftdm_conf_node_t	*tmp_node = NULL;

	/* confirm that we are looking at sng_isup */
	if (strcasecmp(sng_isup->name, "sng_isup")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"sng_isup\"!\n",sng_isup->name);
		return FTDM_FAIL;
	}  else {
		SS7_DEBUG("Parsing \"sng_isup\"...\n");
	}

	/* extract the 3 main sections of the sng_isup block */
	tmp_node = sng_isup->child;
	while (tmp_node != NULL) {

		if (!strcasecmp(tmp_node->name, "mtp_linksets")) {
			if (mtp_linksets == NULL) {
				mtp_linksets = tmp_node;
				SS7_DEBUG("\tFound a \"mtp_linksets section!\n");
			} else {
				SS7_ERROR("\tFound a second \"mtp_linksets\" section!\n");
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(tmp_node->name, "mtp_routes")) {
			if (mtp_routes == NULL) {
				mtp_routes = tmp_node;
				SS7_DEBUG("\tFound a \"mtp_routes\" section!\n");
			} else {
				SS7_ERROR("\tFound a second \"mtp_routes\" section!\n");
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(tmp_node->name, "isup_interfaces")) {
			if (isup_interfaces == NULL) {
				isup_interfaces = tmp_node;
				SS7_DEBUG("\tFound a \"isup_interfaces\" section!\n");
			} else {
				SS7_ERROR("\tFound a second \"isup_interfaces\" section\n!");
				return FTDM_FAIL;
			}
		} else {
			SS7_ERROR("\tFound an unknown section \"%s\"!\n", tmp_node->name);
			return FTDM_FAIL;
		}

		/* go to the next sibling */
		tmp_node = tmp_node->next;

	} /* while (tmp_node != NULL) */

	/* now try to parse the sections */
	if (ftmod_ss7_parse_mtp_linksets(mtp_linksets)) {
		SS7_ERROR("Failed to parse \"mtp_linksets\"!\n");
		return FTDM_FAIL;
	}

	if (ftmod_ss7_parse_mtp_routes(mtp_routes)) {
		SS7_ERROR("Failed to parse \"mtp_routes\"!\n");
		return FTDM_FAIL;
	}

	if (ftmod_ss7_parse_isup_interfaces(isup_interfaces)) {
		SS7_ERROR("Failed to parse \"isup_interfaces\"!\n");
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_mtp_linksets(ftdm_conf_node_t *mtp_linksets)
{
	ftdm_conf_node_t	*mtp_linkset = NULL;

	/* confirm that we are looking at mtp_linksets */
	if (strcasecmp(mtp_linksets->name, "mtp_linksets")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"mtp_linksets\"!\n",mtp_linksets->name);
		return FTDM_FAIL;
	}  else {
		SS7_DEBUG("Parsing \"mtp_linksets\"...\n");
	}

	/* extract the mtp_links */
	mtp_linkset = mtp_linksets->child;

	/* run through all of the mtp_linksets found  */
	while (mtp_linkset != NULL) {
		/* try to the parse mtp_linkset */
		if (ftmod_ss7_parse_mtp_linkset(mtp_linkset)) {
			SS7_ERROR("Failed to parse \"mtp_linkset\"!\n");
			return FTDM_FAIL;
		}

		/* move on to the next linkset */
		mtp_linkset = mtp_linkset->next;

	} /* while (mtp_linkset != NULL) */

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_mtp_linkset(ftdm_conf_node_t *mtp_linkset)
{
	ftdm_conf_parameter_t	*parm = mtp_linkset->parameters;
	int						num_parms = mtp_linkset->n_parameters;
	ftdm_conf_node_t		*mtp_link = NULL;
	sng_mtp_link_t			mtpLink[MAX_MTP_LINKS+1];
	sng_link_set_t			mtpLinkSet;
	int						count;
	int						i;

	/* initialize the mtp_link structures */
	for (i = 0; i < (MAX_MTP_LINKS  + 1); i++) {
		memset(&mtpLink[i], 0x0, sizeof(mtpLink[i]));
	}
	memset(&mtpLinkSet, 0x0, sizeof(mtpLinkSet));

	/* confirm that we are looking at mtp_linkset */
	if (strcasecmp(mtp_linkset->name, "mtp_linkset")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"mtp_linkset\"!\n",mtp_linkset->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"mtp_linkset\"...\n");
	}

	/* extract all the information from the parameters */
	for (i = 0; i < num_parms; i++) {
		/**********************************************************************/
		if (!strcasecmp(parm->var, "name")) {
			strcpy((char *)mtpLinkSet.name, parm->val);
			SS7_DEBUG("\tFound an \"mtp_linkset\" named = %s\n", mtpLinkSet.name);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "apc")) {
			mtpLinkSet.apc = atoi(parm->val);
			SS7_DEBUG("\tFound mtpLinkSet->apc = %d\n", mtpLinkSet.apc);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "minActive")) {
			mtpLinkSet.minActive = atoi(parm->val);
			SS7_DEBUG("\tFound mtpLinkSet->minActive = %d\n", mtpLinkSet.minActive);
		/**********************************************************************/
		} else {
			SS7_ERROR("\tFound an invalid parameter \"%s\"!\n", parm->val);
			return FTDM_FAIL;
		}

		/* move to the next parmeter */
		parm = parm + 1;

	} /* for (i = 0; i < num_parms; i++) */

	/* grab the first mtp-link (which sits below the mtp_links section) */
	mtp_link = mtp_linkset->child->child;

	/* initalize the link counter */
	count = 0;

	/* run through all of the mtp_links found  */
	while (mtp_link != NULL) {
		/* try to the parse mtp_linkset */
		if (ftmod_ss7_parse_mtp_link(mtp_link, &mtpLink[count] )) {
			SS7_ERROR("Failed to parse \"mtp_link\"!\n");
			return FTDM_FAIL;
		}

		/* incremenet the link counter */
		count++;

		/* move on to the next link */
		mtp_link = mtp_link->next;

	} /* while (mtp_link != NULL) */

	/* confirm we have the right number of links */
	if (count < 1 || count > 15 ) {
		SS7_ERROR("Invalid number of mtp_links found (%d)\n", count);
		return FTDM_FAIL;
	}

	/* now we need to see if this linkset exists already or not and grab an Id */
	i = 1;
	while (g_ftdm_sngss7_data.cfg.mtpLinkSet[i].id != 0) {
		if (!strcasecmp((const char *)g_ftdm_sngss7_data.cfg.mtpLinkSet[i].name, (const char *)mtpLinkSet.name)) {
			/* we've found the linkset...so it has already been configured */
			break;
		}
		i++;
		/* add in error check to make sure we don't go out-of-bounds */
	}

	/* if the id value is 0 that means we didn't find the linkset */
	if (g_ftdm_sngss7_data.cfg.mtpLinkSet[i].id  == 0) {
		mtpLinkSet.id = i;
		SS7_DEBUG("found new mtpLinkSet, id is = %d\n", mtpLinkSet.id);
	} else {
		mtpLinkSet.id = i;
		SS7_DEBUG("found existing mtpLinkSet, id is = %d\n", mtpLinkSet.id);
	}

	/* we now have all the information to fill in the Libsng_ss7 structures */
	i = 0;
	count = 0;
	while (mtpLink[i].mtp1.span != 0 ){

		/* have to grab a couple of values from the linkset */
		mtpLink[i].mtp3.apc			= mtpLinkSet.apc;
		mtpLink[i].mtp3.linkSetId	= mtpLinkSet.id;

		ftmod_ss7_fill_in_mtpLink(&mtpLink[i]);

		/* increment the links counter */
		count++;

		/* increment the index value */
		i++;
	}

	mtpLinkSet.linkType		= mtpLink[0].mtp3.linkType;
	mtpLinkSet.switchType	= mtpLink[0].mtp3.switchType;
	mtpLinkSet.ssf			= mtpLink[0].mtp3.ssf;

	ftmod_ss7_fill_in_mtpLinkSet(&mtpLinkSet);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_mtp_link(ftdm_conf_node_t *mtp_link, sng_mtp_link_t *mtpLink)
{
	ftdm_conf_parameter_t	*parm = mtp_link->parameters;
	int					 	num_parms = mtp_link->n_parameters;
	int					 	i;
	
	/* confirm that we are looking at an mtp_link */
	if (strcasecmp(mtp_link->name, "mtp_link")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"mtp_link\"!\n",mtp_link->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"mtp_link\"...\n");
	}
	
	for (i = 0; i < num_parms; i++) {
		/* try to match the parameter to what we expect */
		/**********************************************************************/
		if (!strcasecmp(parm->var, "name")) {
			strcpy((char *)mtpLink->name, parm->val);
			SS7_DEBUG("\tFound an \"mtp_link\" named = %s\n", mtpLink->name);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "span")) {
			mtpLink->mtp1.span = atoi(parm->val);
			SS7_DEBUG("\tFound mtpLink->span = %d\n", mtpLink->mtp1.span);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "chan")) {
			mtpLink->mtp1.chan = atoi(parm->val);
			SS7_DEBUG("\tFound mtpLink->chan = %d\n", mtpLink->mtp1.chan);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "errorType")) {
			if (!strcasecmp(parm->val, "basic")) {
				mtpLink->mtp2.errorType = SD_ERR_NRM;
			} else if (!strcasecmp(parm->val, "pcr")) {
				mtpLink->mtp2.errorType = SD_ERR_CYC;
			} else {
				SS7_ERROR("\tFound an invalid \"errorType\" = %s\n", parm->var);
				return FTDM_FAIL;
			}
			SS7_DEBUG("\tFound mtpLink->errorType=%s\n", parm->val);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "lssuLength")) {
			mtpLink->mtp2.lssuLength = atoi(parm->val);
			if ((mtpLink->mtp2.lssuLength != 1) && (mtpLink->mtp2.lssuLength != 2)) {
				SS7_ERROR("\tFound an invalid \"lssuLength\" = %d\n", mtpLink->mtp2.lssuLength);
				return FTDM_FAIL;
			} else {
				SS7_DEBUG("\tFound mtpLink->lssuLength=%d\n", mtpLink->mtp2.lssuLength);
			}
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "priority")) {
			mtpLink->mtp3.priority = atoi(parm->val);
			if ((mtpLink->mtp3.priority == 0) || (mtpLink->mtp3.priority == 1) || 
				(mtpLink->mtp3.priority == 2) || (mtpLink->mtp3.priority == 3)) {
				SS7_DEBUG("\tFound mtpLink->priority = %d\n",mtpLink->mtp3.priority);
			} else {
				SS7_ERROR("\tFound an invalid \"priority\"=%d\n",mtpLink->mtp3.priority);
				return FTDM_FAIL;
			} 
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "linkType")) {
			if (!strcasecmp(parm->val, "itu92")) {
				mtpLink->mtp2.linkType = LSD_SW_ITU92;
				mtpLink->mtp3.linkType = LSN_SW_ITU;
				SS7_DEBUG("\tFound mtpLink->linkType = \"ITU92\"\n");
			} else if (!strcasecmp(parm->val, "itu88")) {
				mtpLink->mtp2.linkType = LSD_SW_ITU88;
				mtpLink->mtp3.linkType = LSN_SW_ITU;
				SS7_DEBUG("\tFound mtpLink->linkType = \"ITU88\"\n");
			} else if (!strcasecmp(parm->val, "ansi96")) {
				mtpLink->mtp2.linkType = LSD_SW_ANSI92;
				mtpLink->mtp3.linkType = LSN_SW_ANS96;
				SS7_DEBUG("\tFound mtpLink->linkType = \"ANSI96\"\n");
			} else if (!strcasecmp(parm->val, "ansi92")) {
				mtpLink->mtp2.linkType = LSD_SW_ANSI92;
				mtpLink->mtp3.linkType = LSN_SW_ANS;
				SS7_DEBUG("\tFound mtpLink->linkType = \"ANSI92\"\n");
			} else if (!strcasecmp(parm->val, "ansi88")) {
				mtpLink->mtp2.linkType = LSD_SW_ANSI88;
				mtpLink->mtp3.linkType = LSN_SW_ANS;
				SS7_DEBUG("\tFound mtpLink->linkType = \"ANSI88\"\n");
			} else if (!strcasecmp(parm->val, "etsi")) {
				mtpLink->mtp2.linkType = LSD_SW_ITU92;
				mtpLink->mtp3.linkType = LSN_SW_ITU;
				SS7_DEBUG("\tFound mtpLink->linkType = \"ETSI\"\n");
			} else {
				SS7_ERROR("\tFound an invalid linktype of \"%s\"!\n", parm->val);
				return FTDM_FAIL;
			}
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "switchType")) {
			if (!strcasecmp(parm->val, "itu97")) {
				mtpLink->mtp3.switchType = LSI_SW_ITU97;
				SS7_DEBUG("\tFound mtpLink->switchType = \"ITU97\"\n");
			} else if (!strcasecmp(parm->val, "itu88")) {
				mtpLink->mtp3.switchType = LSI_SW_ITU;
				SS7_DEBUG("\tFound mtpLink->switchType = \"ITU88\"\n");
			} else if (!strcasecmp(parm->val, "itu92")) {
				mtpLink->mtp3.switchType = LSI_SW_ITU;
				SS7_DEBUG("\tFound mtpLink->switchType = \"ITU92\"\n");
			} else if (!strcasecmp(parm->val, "itu00")) {
				mtpLink->mtp3.switchType = LSI_SW_ITU2000;
				SS7_DEBUG("\tFound mtpLink->switchType = \"ITU00\"\n");
			} else if (!strcasecmp(parm->val, "ETSIV2")) {
				mtpLink->mtp3.switchType = LSI_SW_ETSI;
				SS7_DEBUG("\tFound mtpLink->switchType = \"ETSIV2\"\n");
			} else if (!strcasecmp(parm->val, "ETSIV3")) {
				mtpLink->mtp3.switchType = LSI_SW_ETSIV3;
				SS7_DEBUG("\tFound mtpLink->switchType = \"ETSIV3\"\n");
			} else if (!strcasecmp(parm->val, "UK")) {
				mtpLink->mtp3.switchType = LSI_SW_UK;
				SS7_DEBUG("\tFound mtpLink->switchType = \"UK\"\n");
			} else if (!strcasecmp(parm->val, "RUSSIA")) {
				mtpLink->mtp3.switchType = LSI_SW_RUSSIA;
				SS7_DEBUG("\tFound mtpLink->switchType = \"RUSSIA\"\n");
			} else if (!strcasecmp(parm->val, "INDIA")) {
				mtpLink->mtp3.switchType = LSI_SW_INDIA;
				SS7_DEBUG("\tFound mtpLink->switchType = \"INDIA\"\n");
			} else if (!strcasecmp(parm->val, "ansi88")) {
				mtpLink->mtp3.switchType = LSI_SW_ANS88;
				SS7_DEBUG("\tFound mtpLink->switchType = \"ANSI88\"\n");
			} else if (!strcasecmp(parm->val, "ansi92")) {
				mtpLink->mtp3.switchType = LSI_SW_ANS92;
				SS7_DEBUG("\tFound mtpLink->switchType = \"ANSI92\"\n");
			} else if (!strcasecmp(parm->val, "ansi95")) {
				mtpLink->mtp3.switchType = LSI_SW_ANS95;
				SS7_DEBUG("\tFound mtpLink->switchType = \"ANSI95\"\n");
			} else {
				SS7_ERROR("\tFound an invalid linktype of \"%s\"!\n", parm->val);
				return FTDM_FAIL;
			}
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "ssf")) {
			if (!strcasecmp(parm->val, "nat")) {
				mtpLink->mtp3.ssf = SSF_NAT;
			} else if (!strcasecmp(parm->val, "int")) {
				mtpLink->mtp3.ssf = SSF_INTL;
			} else {
				SS7_ERROR("\tFound an invalid ssf of \"%s\"!\n", parm->val);
				return FTDM_FAIL;
			}
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "slc")) {
			mtpLink->mtp3.slc = atoi(parm->val);
			SS7_DEBUG("\tFound mtpLink->slc = \"%d\"\n",mtpLink->mtp3.slc);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp2.t1")) {
			mtpLink->mtp2.t1 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp2 t1 = \"%d\"\n",mtpLink->mtp2.t1);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp2.t2")) {
			mtpLink->mtp2.t2 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp2 t2 = \"%d\"\n",mtpLink->mtp2.t2);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp2.t3")) {
			mtpLink->mtp2.t3 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp2 t3 = \"%d\"\n",mtpLink->mtp2.t3);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp2.t4n")) {
			mtpLink->mtp2.t4n = atoi(parm->val);
			SS7_DEBUG("\tFound mtp2 t4n = \"%d\"\n",mtpLink->mtp2.t4n);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp2.t4e")) {
			mtpLink->mtp2.t4e = atoi(parm->val);
			SS7_DEBUG("\tFound mtp2 t4e = \"%d\"\n",mtpLink->mtp2.t4e);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp2.t5")) {
			mtpLink->mtp2.t5 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp2 t5 = \"%d\"\n",mtpLink->mtp2.t5);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp2.t6")) {
			mtpLink->mtp2.t6 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp2 t6 = \"%d\"\n",mtpLink->mtp2.t6);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp2.t7")) {
			mtpLink->mtp2.t7 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp2 t7 = \"%d\"\n",mtpLink->mtp2.t7);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t1")) {
			mtpLink->mtp3.t1 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t1 = \"%d\"\n",mtpLink->mtp3.t1);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t2")) {
			mtpLink->mtp3.t2 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t2 = \"%d\"\n",mtpLink->mtp3.t2);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t3")) {
			mtpLink->mtp3.t3 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t3 = \"%d\"\n",mtpLink->mtp3.t3);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t4")) {
			mtpLink->mtp3.t4 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t4 = \"%d\"\n",mtpLink->mtp3.t4);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t5")) {
			mtpLink->mtp3.t5 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t5 = \"%d\"\n",mtpLink->mtp3.t5);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t7")) {
			mtpLink->mtp3.t7 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t7 = \"%d\"\n",mtpLink->mtp3.t7);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t12")) {
			mtpLink->mtp3.t12 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t12 = \"%d\"\n",mtpLink->mtp3.t12);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t13")) {
			mtpLink->mtp3.t13 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t13 = \"%d\"\n",mtpLink->mtp3.t13);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t14")) {
			mtpLink->mtp3.t14 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t14 = \"%d\"\n",mtpLink->mtp3.t14);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t17")) {
			mtpLink->mtp3.t17 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t17 = \"%d\"\n",mtpLink->mtp3.t17);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t22")) {
			mtpLink->mtp3.t22 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t22 = \"%d\"\n",mtpLink->mtp3.t22);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t23")) {
			mtpLink->mtp3.t23 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t23 = \"%d\"\n",mtpLink->mtp3.t23);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t24")) {
			mtpLink->mtp3.t24 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t24 = \"%d\"\n",mtpLink->mtp3.t24);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t31")) {
			mtpLink->mtp3.t31 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t31 = \"%d\"\n",mtpLink->mtp3.t31);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t32")) {
			mtpLink->mtp3.t32 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t32 = \"%d\"\n",mtpLink->mtp3.t32);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t33")) {
			mtpLink->mtp3.t33 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t33 = \"%d\"\n",mtpLink->mtp3.t33);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t34")) {
			mtpLink->mtp3.t34 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t34 = \"%d\"\n",mtpLink->mtp3.t34);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t35")) {
			mtpLink->mtp3.t35 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t35 = \"%d\"\n",mtpLink->mtp3.t35);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t36")) {
			mtpLink->mtp3.t36 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t36 = \"%d\"\n",mtpLink->mtp3.t36);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t37")) {
			mtpLink->mtp3.t37 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t37 = \"%d\"\n",mtpLink->mtp3.t37);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.tcraft")) {
			mtpLink->mtp3.tcraft = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 tcraft = \"%d\"\n",mtpLink->mtp3.tcraft);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.tflc")) {
			mtpLink->mtp3.tflc = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 tflc = \"%d\"\n",mtpLink->mtp3.tflc);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.tbnd")) {
			mtpLink->mtp3.tbnd = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 tbnd = \"%d\"\n",mtpLink->mtp3.tbnd);
		/**********************************************************************/
		} else {
			SS7_ERROR("\tFound an invalid parameter \"%s\"!\n", parm->val);
			return FTDM_FAIL;
		}
	
		/* move to the next parameter */
		parm = parm + 1;
	}
	
	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_mtp_routes(ftdm_conf_node_t *mtp_routes)
{
	ftdm_conf_node_t	*mtp_route = NULL;

	/* confirm that we are looking at an mtp_routes */
	if (strcasecmp(mtp_routes->name, "mtp_routes")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"mtp_routes\"!\n",mtp_routes->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"mtp_routes\"...\n");
	}

	/* extract the mtp_routes */
	mtp_route = mtp_routes->child;

	while (mtp_route != NULL) {
		/* parse the found mtp_route */
		if (ftmod_ss7_parse_mtp_route(mtp_route)) {
			SS7_ERROR("Failed to parse \"mtp_route\"\n");
			return FTDM_FAIL;
		}

		/* go to the next mtp_route */
		mtp_route = mtp_route->next;
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_mtp_route(ftdm_conf_node_t *mtp_route)
{
	sng_route_t		 		mtpRoute;
	ftdm_conf_parameter_t	*parm = mtp_route->parameters;
	int					 	num_parms = mtp_route->n_parameters;
	int					 	i;

	memset(&mtpRoute, 0x0, sizeof(mtpRoute));

	/* confirm that we are looking at an mtp_link */
	if (strcasecmp(mtp_route->name, "mtp_route")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"mtp_route\"!\n",mtp_route->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"mtp_route\"...\n");
	}

	for (i = 0; i < num_parms; i++) {
		/* try to match the parameter to what we expect */
		/**********************************************************************/
		if (!strcasecmp(parm->var, "name")) {
			strcpy((char *)mtpRoute.name, parm->val);
			SS7_DEBUG("\tFound an \"mtp_route\" named = %s\n", mtpRoute.name);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "dpc")) {
			mtpRoute.dpc = atoi(parm->val);
			SS7_DEBUG("\tFound mtpRoute->dpc = %d\n", mtpRoute.dpc);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp_linkset")) {

			/* find the linkset by it's name */
			int x = 1;
			while (g_ftdm_sngss7_data.cfg.mtpLinkSet[x].id != 0) {
				/* check if the name matches */
				if (!strcasecmp((char *)g_ftdm_sngss7_data.cfg.mtpLinkSet[x].name, parm->val)) {

					/* now, harvest the required infomormation from the global structure */
					mtpRoute.linkType		= g_ftdm_sngss7_data.cfg.mtpLinkSet[x].linkType;
					mtpRoute.switchType		= g_ftdm_sngss7_data.cfg.mtpLinkSet[x].switchType;
					mtpRoute.ssf			= g_ftdm_sngss7_data.cfg.mtpLinkSet[x].ssf;
					mtpRoute.linkSetId		= g_ftdm_sngss7_data.cfg.mtpLinkSet[x].id;
					cmbLinkSetId++;
					mtpRoute.cmbLinkSetId	= cmbLinkSetId;

					/* update the linkset with the new cmbLinkSet value */
					g_ftdm_sngss7_data.cfg.mtpLinkSet[x].numLinks++;
					g_ftdm_sngss7_data.cfg.mtpLinkSet[x].links[g_ftdm_sngss7_data.cfg.mtpLinkSet[x].numLinks-1] = mtpRoute.cmbLinkSetId;
					break;
				}
				x++;
			}

			/* check why we exited the wile loop ... and react accordingly */
			if (mtpRoute.cmbLinkSetId == 0) {
				SS7_ERROR("\tFailed to find the linkset = \"%s\"!\n", parm->val);
				return FTDM_FAIL;
			} else {
				SS7_DEBUG("\tFound mtp3_route->linkset = %s\n", parm->val);
			}
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isSTP")) {
			if (!strcasecmp(parm->val, "no")) {
				mtpRoute.isSTP = 0;
				SS7_DEBUG("\tFound mtpRoute->isSTP = no\n");
			} else if (!strcasecmp(parm->val, "yes")) {
				mtpRoute.isSTP = 1;
				SS7_DEBUG("\tFound mtpRoute->isSTP = yes\n");
			} else {
				SS7_ERROR("\tFound an invalid parameter for isSTP \"%s\"!\n", parm->val);
			   return FTDM_FAIL;
			}
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t6")) {
			mtpRoute.t6 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t6 = \"%d\"\n",mtpRoute.t6);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t8")) {
			mtpRoute.t8 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t8 = \"%d\"\n",mtpRoute.t8);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t10")) {
			mtpRoute.t10 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t10 = \"%d\"\n",mtpRoute.t10);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t11")) {
			mtpRoute.t11 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t11 = \"%d\"\n",mtpRoute.t11);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t15")) {
			mtpRoute.t15 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t15 = \"%d\"\n",mtpRoute.t15);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t16")) {
			mtpRoute.t16 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t16 = \"%d\"\n",mtpRoute.t16);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t18")) {
			mtpRoute.t18 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t18 = \"%d\"\n",mtpRoute.t18);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t19")) {
			mtpRoute.t19 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t19 = \"%d\"\n",mtpRoute.t19);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t21")) {
			mtpRoute.t21 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t21 = \"%d\"\n",mtpRoute.t21);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t25")) {
			mtpRoute.t25 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t25 = \"%d\"\n",mtpRoute.t25);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp3.t26")) {
			mtpRoute.t26 = atoi(parm->val);
			SS7_DEBUG("\tFound mtp3 t26 = \"%d\"\n",mtpRoute.t26);
		/**********************************************************************/
		} else {
			SS7_ERROR("\tFound an invalid parameter \"%s\"!\n", parm->val);
			return FTDM_FAIL;

		}

		/* move to the next parameter */
		parm = parm + 1;
	}

	ftmod_ss7_fill_in_nsap(&mtpRoute);

	ftmod_ss7_fill_in_mtp3_route(&mtpRoute);



	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_isup_interfaces(ftdm_conf_node_t *isup_interfaces)
{
	ftdm_conf_node_t	*isup_interface = NULL;

	/* confirm that we are looking at isup_interfaces */
	if (strcasecmp(isup_interfaces->name, "isup_interfaces")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"isup_interfaces\"!\n",isup_interfaces->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"isup_interfaces\"...\n");
	}

	/* extract the isup_interfaces */
	isup_interface = isup_interfaces->child;

	while (isup_interface != NULL) {
		/* parse the found mtp_route */
		if (ftmod_ss7_parse_isup_interface(isup_interface)) {
			SS7_ERROR("Failed to parse \"isup_interface\"\n");
			return FTDM_FAIL;
		}

		/* go to the next mtp_route */
		isup_interface = isup_interface->next;
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_isup_interface(ftdm_conf_node_t *isup_interface)
{
	sng_isup_inf_t			sng_isup;
	sng_isap_t				sng_isap;
	ftdm_conf_parameter_t	*parm = isup_interface->parameters;
	int						num_parms = isup_interface->n_parameters;
	int						i;
	int						linkSetId;
	int						flag_cld_nadi = 0;
	int						flag_clg_nadi = 0;

	memset(&sng_isup, 0x0, sizeof(sng_isup));
	memset(&sng_isap, 0x0, sizeof(sng_isap));

	/* confirm that we are looking at an mtp_link */
	if (strcasecmp(isup_interface->name, "isup_interface")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"isup_interface\"!\n",isup_interface->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"isup_interface\"...\n");
	}


	for (i = 0; i < num_parms; i++) {
		/* try to match the parameter to what we expect */
		/**********************************************************************/
		if (!strcasecmp(parm->var, "name")) {
			strcpy((char *)sng_isup.name, parm->val);
			SS7_DEBUG("\tFound an \"isup_interface\" named = %s\n", sng_isup.name);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "spc")) {
			sng_isup.spc = atoi(parm->val);
			g_ftdm_sngss7_data.cfg.spc = sng_isup.spc;
			SS7_DEBUG("\tFound SPC = %d\n", sng_isup.spc);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp_route")) {
			/* find the route by it's name */
			int x = 1;

			while (g_ftdm_sngss7_data.cfg.mtpRoute[x].id != 0) {

				/* check if the name matches */
				if (!strcasecmp((char *)g_ftdm_sngss7_data.cfg.mtpRoute[x].name, parm->val)) {
					/* now, harvest the required information from the global structure */
					sng_isup.mtpRouteId		= g_ftdm_sngss7_data.cfg.mtpRoute[x].id;
					sng_isup.dpc			= g_ftdm_sngss7_data.cfg.mtpRoute[x].dpc;
					sng_isup.switchType	 	= g_ftdm_sngss7_data.cfg.mtpRoute[x].switchType;
					sng_isap.switchType		= g_ftdm_sngss7_data.cfg.mtpRoute[x].switchType;

					/* find the NSAP corresponding to this switchType and SSF */
					int z = 1;
					while (g_ftdm_sngss7_data.cfg.nsap[z].id != 0) {
						if ((g_ftdm_sngss7_data.cfg.nsap[z].linkType == g_ftdm_sngss7_data.cfg.mtpRoute[x].linkType) &&
							(g_ftdm_sngss7_data.cfg.nsap[z].switchType == g_ftdm_sngss7_data.cfg.mtpRoute[x].switchType) &&
							(g_ftdm_sngss7_data.cfg.nsap[z].ssf == g_ftdm_sngss7_data.cfg.mtpRoute[x].ssf)) {
								sng_isup.nwId 	= g_ftdm_sngss7_data.cfg.nsap[z].nwId;
							/* we have a match so break out of this loop */
							break;
						}
						/* move on to the next one */
						z++;
					}
					break;
				}
				x++;
			} /* while (g_ftdm_sngss7_data.cfg.mtpRoute[x].id != 0) */

			/* check why we exited the while loop ... and react accordingly */
			if (sng_isup.mtpRouteId == 0) {
				SS7_ERROR("\tFailed to find the MTP3 Route = \"%s\"!\n", parm->val);
				return FTDM_FAIL;
			} else {
				SS7_DEBUG("\tFound MTP3 Route = %s\n", parm->val);
			}
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "ssf")) {
			if (!strcasecmp(parm->val, "nat")) {
				sng_isup.ssf = SSF_NAT;
				sng_isap.ssf = SSF_NAT;
			} else if (!strcasecmp(parm->val, "int")) {
				sng_isup.ssf = SSF_INTL;
				sng_isap.ssf = SSF_INTL;
			} else {
				SS7_ERROR("\tFound an invalid ssf of \"%s\"!\n", parm->val);
				return FTDM_FAIL;
			}
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "license")) {
		/**********************************************************************/
			strcpy(g_ftdm_sngss7_data.cfg.license, parm->val);
			strcpy(g_ftdm_sngss7_data.cfg.signature, parm->val);
			strcat(g_ftdm_sngss7_data.cfg.signature, ".sig");
			SS7_DEBUG("\tFound license file = %s\n", g_ftdm_sngss7_data.cfg.license);
			SS7_DEBUG("\tFound signature file = %s\n", g_ftdm_sngss7_data.cfg.signature);	
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t1")) {
			sng_isap.t1 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t1 = \"%d\"\n",sng_isap.t1);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t2")) {
			sng_isap.t2 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t2 = \"%d\"\n",sng_isap.t2);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t4")) {
			sng_isup.t4 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t4 = \"%d\"\n",sng_isup.t4);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t5")) {
			sng_isap.t5 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t5 = \"%d\"\n",sng_isap.t5);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t6")) {
			sng_isap.t6 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t6 = \"%d\"\n",sng_isap.t6);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t7")) {
			sng_isap.t7 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t7 = \"%d\"\n",sng_isap.t7);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t8")) {
			sng_isap.t8 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t8 = \"%d\"\n",sng_isap.t8);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t9")) {
			sng_isap.t9 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t9 = \"%d\"\n",sng_isap.t9);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t10")) {
			sng_isup.t10 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t10 = \"%d\"\n",sng_isup.t10);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t11")) {
			sng_isup.t11 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t11 = \"%d\"\n",sng_isup.t11);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t18")) {
			sng_isup.t18 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t18 = \"%d\"\n",sng_isup.t18);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t19")) {
			sng_isup.t19 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t19 = \"%d\"\n",sng_isup.t19);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t20")) {
			sng_isup.t20 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t20 = \"%d\"\n",sng_isup.t20);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t21")) {
			sng_isup.t21 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t21 = \"%d\"\n",sng_isup.t21);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t22")) {
			sng_isup.t22 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t22 = \"%d\"\n",sng_isup.t22);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t23")) {
			sng_isup.t23 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t23 = \"%d\"\n",sng_isup.t23);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t24")) {
			sng_isup.t24 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t24 = \"%d\"\n",sng_isup.t24);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t25")) {
			sng_isup.t25 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t25 = \"%d\"\n",sng_isup.t25);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t26")) {
			sng_isup.t26 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t26 = \"%d\"\n",sng_isup.t26);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t28")) {
			sng_isup.t28 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t28 = \"%d\"\n",sng_isup.t28);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t29")) {
			sng_isup.t29 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t29 = \"%d\"\n",sng_isup.t29);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t30")) {
			sng_isup.t30 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t30 = \"%d\"\n",sng_isup.t30);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t31")) {
			sng_isap.t31 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t31 = \"%d\"\n",sng_isap.t31);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t32")) {
			sng_isup.t32 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t32 = \"%d\"\n",sng_isup.t32);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t33")) {
			sng_isap.t33 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t33 = \"%d\"\n",sng_isap.t33);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t34")) {
			sng_isap.t34 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t34 = \"%d\"\n",sng_isap.t34);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t35")) {
			sng_isup.t35 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t35 = \"%d\"\n",sng_isup.t35);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t36")) {
			sng_isap.t36 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t36 = \"%d\"\n",sng_isap.t36);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t37")) {
			sng_isup.t37 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t37 = \"%d\"\n",sng_isup.t37);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t38")) {
			sng_isup.t38 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t38 = \"%d\"\n",sng_isup.t38);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.t39")) {
			sng_isup.t39 = atoi(parm->val);
			SS7_DEBUG("\tFound isup t39 = \"%d\"\n",sng_isup.t39);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tccr")) {
			sng_isap.tccr = atoi(parm->val);
			SS7_DEBUG("\tFound isup tccr = \"%d\"\n",sng_isap.tccr);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tccrt")) {
			sng_isap.tccrt = atoi(parm->val);
			SS7_DEBUG("\tFound isup tccrt = \"%d\"\n",sng_isap.tccrt);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tex")) {
			sng_isap.tex = atoi(parm->val);
			SS7_DEBUG("\tFound isup tex = \"%d\"\n",sng_isap.tex);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tect")) {
			sng_isap.tect = atoi(parm->val);
			SS7_DEBUG("\tFound isup tect = \"%d\"\n",sng_isap.tect);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tcrm")) {
			sng_isap.tcrm = atoi(parm->val);
			SS7_DEBUG("\tFound isup tcrm = \"%d\"\n",sng_isap.tcrm);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tcra")) {
			sng_isap.tcra = atoi(parm->val);
			SS7_DEBUG("\tFound isup tcra = \"%d\"\n",sng_isap.tcra);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tfgr")) {
			sng_isup.tfgr = atoi(parm->val);
			SS7_DEBUG("\tFound isup tfgr = \"%d\"\n",sng_isup.tfgr);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.trelrsp")) {
			sng_isap.trelrsp = atoi(parm->val);
			SS7_DEBUG("\tFound isup trelrsp = \"%d\"\n",sng_isap.trelrsp);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tfnlrelrsp")) {
			sng_isap.tfnlrelrsp = atoi(parm->val);
			SS7_DEBUG("\tFound isup tfnlrelrsp = \"%d\"\n",sng_isap.tfnlrelrsp);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tfnlrelrsp")) {
			sng_isap.tfnlrelrsp = atoi(parm->val);
			SS7_DEBUG("\tFound isup tfnlrelrsp = \"%d\"\n",sng_isap.tfnlrelrsp);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tpause")) {
			sng_isup.tpause = atoi(parm->val);
			SS7_DEBUG("\tFound isup tpause = \"%d\"\n",sng_isup.tpause);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "isup.tstaenq")) {
			sng_isup.tstaenq = atoi(parm->val);
			SS7_DEBUG("\tFound isup tstaenq = \"%d\"\n",sng_isup.tstaenq);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "clg_nadi")) {
		/**********************************************************************/
			/* throw the flag so that we know we got this optional parameter */
			flag_clg_nadi = 1;
			sng_isup.clg_nadi = atoi(parm->val);
			SS7_DEBUG("\tFound default CLG_NADI value = %d\n", sng_isup.clg_nadi);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "cld_nadi")) {
		/**********************************************************************/
			/* throw the flag so that we know we got this optional parameter */
			flag_cld_nadi = 1;
			sng_isup.cld_nadi = atoi(parm->val);
			SS7_DEBUG("\tFound default CLD_NADI value = %d\n", sng_isup.cld_nadi);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "obci_bita")) {
		/**********************************************************************/
			if (*parm->val == '1') {
				sngss7_set_options(&sng_isup, SNGSS7_ACM_OBCI_BITA);
				SS7_DEBUG("\tFound Optional Backwards Indicator: Bit A (early media) enable option\n");
			} else if (*parm->val == '0') {
				sngss7_clear_options(&sng_isup, SNGSS7_ACM_OBCI_BITA);
				SS7_DEBUG("\tFound Optional Backwards Indicator: Bit A (early media) disable option\n");
			} else {
				SS7_DEBUG("\tInvalid value for \"obci_bita\" option\n");
			}
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "lpa_on_cot")) {
		/**********************************************************************/
			if (*parm->val == '1') {
				sngss7_set_options(&sng_isup, SNGSS7_LPA_FOR_COT);
				SS7_DEBUG("\tFound Tx LPA on COT enable option\n");
			} else if (*parm->val == '0') {
				sngss7_clear_options(&sng_isup, SNGSS7_LPA_FOR_COT);
				SS7_DEBUG("\tFound Tx LPA on COT disable option\n");
			} else {
				SS7_DEBUG("\tInvalid value for \"lpa_on_cot\" option\n");
			}
		/**********************************************************************/
		} else {
			SS7_ERROR("\tFound an invalid parameter \"%s\"!\n", parm->val);
			return FTDM_FAIL;

		}

		/* move to the next parameter */
		parm = parm + 1;
	}

	/* check if the user filled in a nadi value by looking at flag */
	if (!flag_cld_nadi) {
		/* default the nadi value to national */
		sng_isup.cld_nadi = 0x03;
	}

	if (!flag_clg_nadi) {
		/* default the nadi value to national */
		sng_isup.clg_nadi = 0x03;
	}

	/* trickle down the SPC to all sub entities */
	linkSetId = g_ftdm_sngss7_data.cfg.mtpRoute[sng_isup.mtpRouteId].linkSetId;

	i = 1;
	while (g_ftdm_sngss7_data.cfg.mtpLink[i].id != 0) {
		if (g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.linkSetId == linkSetId) {
			g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.spc = g_ftdm_sngss7_data.cfg.spc;
		}

		i++;
	}

	ftmod_ss7_fill_in_isap(&sng_isap);

	sng_isup.isap = sng_isap.id;

	ftmod_ss7_fill_in_isup_interface(&sng_isup);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_mtpLink(sng_mtp_link_t *mtpLink)
{
	int i;

	/* go through all the existing links and see if we find a match */
	i = 1;
	while (g_ftdm_sngss7_data.cfg.mtpLink[i].id != 0) {
		if ((g_ftdm_sngss7_data.cfg.mtpLink[i].mtp1.span == mtpLink->mtp1.span) &&
			(g_ftdm_sngss7_data.cfg.mtpLink[i].mtp1.chan == mtpLink->mtp1.chan)) {

			/* we have a match so break out of this loop */
			break;
		}
		/* move on to the next one */
		i++;
	}

	/* if the id value is 0 that means we didn't find the link */
	if (g_ftdm_sngss7_data.cfg.mtpLink[i].id  == 0) {
		mtpLink->id = i;
		SS7_DEBUG("found new mtpLink on span=%d, chan=%d, id = %d\n", 
					mtpLink->mtp1.span, 
					mtpLink->mtp1.chan, 
					mtpLink->id);
	} else {
		mtpLink->id = i;
		SS7_DEBUG("found existing mtpLink on span=%d, chan=%d, id = %d\n", 
					mtpLink->mtp1.span, 
					mtpLink->mtp1.chan, 
					mtpLink->id);
	}

	/* fill in the information */
	strcpy((char *)g_ftdm_sngss7_data.cfg.mtpLink[i].name, (char *)mtpLink->name);

	g_ftdm_sngss7_data.cfg.mtpLink[i].id				= mtpLink->id;

	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp1.span 		= mtpLink->mtp1.span;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp1.chan 		= mtpLink->mtp1.chan;

	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.linkType		= mtpLink->mtp2.linkType;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.errorType	= mtpLink->mtp2.errorType;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.lssuLength	= mtpLink->mtp2.lssuLength;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.mtp1Id		= mtpLink->id;

	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.priority		= mtpLink->mtp3.priority;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.linkType		= mtpLink->mtp3.linkType;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.switchType	= mtpLink->mtp3.switchType;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.apc			= mtpLink->mtp3.apc;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.spc			= g_ftdm_sngss7_data.cfg.spc;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.ssf			= mtpLink->mtp3.ssf;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.slc			= mtpLink->mtp3.slc;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.linkSetId	= mtpLink->mtp3.linkSetId;
	g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.mtp2Id		= mtpLink->id;

	if ( mtpLink->mtp2.t1 != 0 ) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t1		= mtpLink->mtp2.t1;
	}else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t1		= 500;
	}
	if ( mtpLink->mtp2.t2 != 0 ) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t2		= mtpLink->mtp2.t2;
	}else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t2		= 250;
	}
	if ( mtpLink->mtp2.t3 != 0 ) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t3		= mtpLink->mtp2.t3;
	}else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t3		= 20;
	}
	if ( mtpLink->mtp2.t4n != 0 ) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t4n		= mtpLink->mtp2.t4n;
	}else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t4n		= 80;
	}
	if ( mtpLink->mtp2.t4e != 0 ) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t4e		= mtpLink->mtp2.t4e;
	}else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t4e		= 5;
	}
	if ( mtpLink->mtp2.t5 != 0 ) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t5		= mtpLink->mtp2.t5;
	}else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t5		= 1;
	}
	if ( mtpLink->mtp2.t6 != 0 ) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t6		= mtpLink->mtp2.t6;
	}else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t6		= 60;
	}
	if ( mtpLink->mtp2.t7 != 0 ) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t7		= mtpLink->mtp2.t7;
	}else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp2.t7		= 40;
	}

	if (mtpLink->mtp3.t1 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t1		= mtpLink->mtp3.t1;
	} else { 
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t1		= 8;
	}
	if (mtpLink->mtp3.t2 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t2		= mtpLink->mtp3.t2;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t2		= 14;
	}
	if (mtpLink->mtp3.t3 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t3		= mtpLink->mtp3.t3;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t3		= 8;
	}
	if (mtpLink->mtp3.t4 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t4		= mtpLink->mtp3.t4;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t4		= 8;
	}
	if (mtpLink->mtp3.t5 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t5		= mtpLink->mtp3.t5;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t5		= 8;
	}
	if (mtpLink->mtp3.t7 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t7		= mtpLink->mtp3.t7;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t7		= 15;
	}
	if (mtpLink->mtp3.t12 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t12		= mtpLink->mtp3.t12;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t12		= 15;
	}
	if (mtpLink->mtp3.t13 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t13		= mtpLink->mtp3.t13;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t13		= 15;
	}
	if (mtpLink->mtp3.t14 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t14		= mtpLink->mtp3.t14;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t14		= 30;
	}
	if (mtpLink->mtp3.t17 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t17		= mtpLink->mtp3.t17;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t17		= 15;
	}
	if (mtpLink->mtp3.t22 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t22		= mtpLink->mtp3.t22;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t22		= 1800;
	}
	if (mtpLink->mtp3.t23 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t23		= mtpLink->mtp3.t23;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t23		= 1800;
	}
	if (mtpLink->mtp3.t24 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t24		= mtpLink->mtp3.t24;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t24		= 5;
	}
	if (mtpLink->mtp3.t31 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t31		= mtpLink->mtp3.t31;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t31		= 50;
	}
	if (mtpLink->mtp3.t32 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t32		= mtpLink->mtp3.t32;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t32		= 120;
	}
	if (mtpLink->mtp3.t33 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t33		= mtpLink->mtp3.t33;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t33		= 3000;
	}
	if (mtpLink->mtp3.t34 != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t34		= mtpLink->mtp3.t34;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.t34		= 600;
	}
	if (mtpLink->mtp3.tflc != 0) {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.tflc		= mtpLink->mtp3.tflc;
	} else {
		g_ftdm_sngss7_data.cfg.mtpLink[i].mtp3.tflc		= 300;
	}
	return (mtpLink->id);
}


/******************************************************************************/
static int ftmod_ss7_fill_in_mtpLinkSet(sng_link_set_t *mtpLinkSet)
{
	int	i = mtpLinkSet->id;

	strcpy((char *)g_ftdm_sngss7_data.cfg.mtpLinkSet[i].name, (char *)mtpLinkSet->name);

	g_ftdm_sngss7_data.cfg.mtpLinkSet[i].id			= mtpLinkSet->id;
	g_ftdm_sngss7_data.cfg.mtpLinkSet[i].apc		= mtpLinkSet->apc;
	g_ftdm_sngss7_data.cfg.mtpLinkSet[i].linkType	= mtpLinkSet->linkType;
	g_ftdm_sngss7_data.cfg.mtpLinkSet[i].switchType	= mtpLinkSet->switchType;
	g_ftdm_sngss7_data.cfg.mtpLinkSet[i].ssf		= mtpLinkSet->ssf;

	/* these values are filled in as we find routes and start allocating cmbLinkSetIds */
	g_ftdm_sngss7_data.cfg.mtpLinkSet[i].minActive	= mtpLinkSet->minActive;
	g_ftdm_sngss7_data.cfg.mtpLinkSet[i].numLinks	= 0;
	return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_mtp3_route(sng_route_t *mtp3_route)
{
	int i;

	/* go through all the existing routes and see if we find a match */
	i = 1;
	while (g_ftdm_sngss7_data.cfg.mtpRoute[i].id != 0) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.mtpRoute[i].name, mtp3_route->name)) {
			/* we have a match so break out of this loop */
			break;
		}
		/* move on to the next one */
		i++;
	}

	if (g_ftdm_sngss7_data.cfg.mtpRoute[i].id == 0) {
		mtp3_route->id = i;
		SS7_DEBUG("found new mtp3_route, id is = %d\n", mtp3_route->id);
	} else {
		mtp3_route->id = i;
		SS7_DEBUG("found existing mtp3_route, id is = %d\n", mtp3_route->id);
	}

	strcpy((char *)g_ftdm_sngss7_data.cfg.mtpRoute[i].name, (char *)mtp3_route->name);

	g_ftdm_sngss7_data.cfg.mtpRoute[i].id			= mtp3_route->id;
	g_ftdm_sngss7_data.cfg.mtpRoute[i].dpc			= mtp3_route->dpc;
	g_ftdm_sngss7_data.cfg.mtpRoute[i].linkType		= mtp3_route->linkType;
	g_ftdm_sngss7_data.cfg.mtpRoute[i].switchType	= mtp3_route->switchType;
	g_ftdm_sngss7_data.cfg.mtpRoute[i].cmbLinkSetId	= mtp3_route->cmbLinkSetId;
	g_ftdm_sngss7_data.cfg.mtpRoute[i].isSTP		= mtp3_route->isSTP;
	g_ftdm_sngss7_data.cfg.mtpRoute[i].nwId			= mtp3_route->nwId;
	g_ftdm_sngss7_data.cfg.mtpRoute[i].linkSetId	= mtp3_route->linkSetId;
	g_ftdm_sngss7_data.cfg.mtpRoute[i].ssf			= mtp3_route->ssf;
	if (mtp3_route->t6 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t6		= mtp3_route->t6;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t6		= 8;
	}
	if (mtp3_route->t8 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t8		= mtp3_route->t8;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t8		= 12;
	}
	if (mtp3_route->t10 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t10		= mtp3_route->t10;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t10	   = 300;
	}
	if (mtp3_route->t11 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t11		= mtp3_route->t11;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t11	   = 300;
	}
	if (mtp3_route->t15 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t15		= mtp3_route->t15;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t15	   = 30;
	}
	if (mtp3_route->t16 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t16		= mtp3_route->t16;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t16	   = 20;
	}
	if (mtp3_route->t18 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t18		= mtp3_route->t18;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t18	   = 200;
	}
	if (mtp3_route->t19 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t19		= mtp3_route->t19;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t19	   = 690;
	}
	if (mtp3_route->t21 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t21		= mtp3_route->t21;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t21	   = 650; 
	}
	if (mtp3_route->t25 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t25		= mtp3_route->t25;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t25	   = 100;
	}
	if (mtp3_route->t26 != 0) {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t26		= mtp3_route->t26;
	} else {
		g_ftdm_sngss7_data.cfg.mtpRoute[i].t26	   = 100;
	}

	return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_nsap(sng_route_t *mtp3_route)
{
	int i;

	/* go through all the existing interfaces and see if we find a match */
	i = 1;
	while (g_ftdm_sngss7_data.cfg.nsap[i].id != 0) {
		if ((g_ftdm_sngss7_data.cfg.nsap[i].linkType == mtp3_route->linkType) &&
			(g_ftdm_sngss7_data.cfg.nsap[i].switchType == mtp3_route->switchType) &&
			(g_ftdm_sngss7_data.cfg.nsap[i].ssf == mtp3_route->ssf)) {

			/* we have a match so break out of this loop */
			break;
		}
		/* move on to the next one */
		i++;
	}

	if (g_ftdm_sngss7_data.cfg.nsap[i].id == 0) {
		g_ftdm_sngss7_data.cfg.nsap[i].id = i;
		 mtp3_route->nwId = i;
		SS7_DEBUG("found new mtp3_isup interface, id is = %d\n", g_ftdm_sngss7_data.cfg.nsap[i].id);
	} else {
		g_ftdm_sngss7_data.cfg.nsap[i].id = i;
		 mtp3_route->nwId = i;
		SS7_DEBUG("found existing mtp3_isup interface, id is = %d\n", g_ftdm_sngss7_data.cfg.nsap[i].id);
	}
	
	g_ftdm_sngss7_data.cfg.nsap[i].spId			= g_ftdm_sngss7_data.cfg.nsap[i].id;
	g_ftdm_sngss7_data.cfg.nsap[i].suId			= g_ftdm_sngss7_data.cfg.nsap[i].id;
	g_ftdm_sngss7_data.cfg.nsap[i].nwId			= mtp3_route->nwId;
	g_ftdm_sngss7_data.cfg.nsap[i].linkType		= mtp3_route->linkType;
	g_ftdm_sngss7_data.cfg.nsap[i].switchType	= mtp3_route->switchType;
	g_ftdm_sngss7_data.cfg.nsap[i].ssf			= mtp3_route->ssf;

	return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_isup_interface(sng_isup_inf_t *sng_isup)
{
	int i;

	/* go through all the existing interfaces and see if we find a match */
	i = 1;
	while (g_ftdm_sngss7_data.cfg.isupIntf[i].id != 0) {
		if (!strcasecmp(g_ftdm_sngss7_data.cfg.isupIntf[i].name, sng_isup->name)) {

			/* we have a match so break out of this loop */
			break;
		}
		/* move on to the next one */
		i++;
	}

	if (g_ftdm_sngss7_data.cfg.isupIntf[i].id == 0) {
		sng_isup->id = i;
		SS7_DEBUG("found new isup interface, id is = %d\n", sng_isup->id);
	} else {
		sng_isup->id = i;
		SS7_DEBUG("found existing isup interface, id is = %d\n", sng_isup->id);
	}

	strcpy((char *)g_ftdm_sngss7_data.cfg.isupIntf[i].name, (char *)sng_isup->name);

	g_ftdm_sngss7_data.cfg.isupIntf[i].id			= sng_isup->id;
	g_ftdm_sngss7_data.cfg.isupIntf[i].mtpRouteId	= sng_isup->mtpRouteId;
	g_ftdm_sngss7_data.cfg.isupIntf[i].nwId			= sng_isup->nwId;
	g_ftdm_sngss7_data.cfg.isupIntf[i].dpc			= sng_isup->dpc;
	g_ftdm_sngss7_data.cfg.isupIntf[i].spc			= sng_isup->spc;
	g_ftdm_sngss7_data.cfg.isupIntf[i].switchType	= sng_isup->switchType;
	g_ftdm_sngss7_data.cfg.isupIntf[i].ssf			= sng_isup->ssf;
	g_ftdm_sngss7_data.cfg.isupIntf[i].isap			= sng_isup->isap;
	g_ftdm_sngss7_data.cfg.isupIntf[i].cld_nadi		= sng_isup->cld_nadi;
	g_ftdm_sngss7_data.cfg.isupIntf[i].clg_nadi		= sng_isup->clg_nadi;
	g_ftdm_sngss7_data.cfg.isupIntf[i].options		= sng_isup->options;
	if (sng_isup->t4 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t4		= sng_isup->t4;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t4		= 3000;
	}
	if (sng_isup->t10 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t10		= sng_isup->t10;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t10		= 50;
	}
	if (sng_isup->t11 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t11		= sng_isup->t11;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t11		= 170;
	}
	if (sng_isup->t18 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t18		= sng_isup->t18;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t18		= 300;
	}
	if (sng_isup->t19 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t19		= sng_isup->t19;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t19		= 3000;
	}
	if (sng_isup->t20 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t20		= sng_isup->t20;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t20		= 300;
	}
	if (sng_isup->t21 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t21		= sng_isup->t21;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t21		= 3000;
	}
	if (sng_isup->t22 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t22		= sng_isup->t22;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t22		= 300;
	}
	if (sng_isup->t23 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t23		= sng_isup->t23;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t23		= 3000;
	}
	if (sng_isup->t24 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t24		= sng_isup->t24;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t24		= 10;
	}
	if (sng_isup->t25 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t25		= sng_isup->t25;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t25		= 20;
	}
	if (sng_isup->t26 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t26		= sng_isup->t26;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t26		= 600;
	}
	if (sng_isup->t28 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t28		= sng_isup->t28;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t28		= 100;
	}
	if (sng_isup->t29 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t29		= sng_isup->t29;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t29		= 6;
	}
	if (sng_isup->t30 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t30		= sng_isup->t30;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t30		= 50;
	}
	if (sng_isup->t32 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t32		= sng_isup->t32;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t32		= 30;
	}
	if (sng_isup->t35 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t35		= sng_isup->t35;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t35		= 170;
	}
	if (sng_isup->t37 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t37		= sng_isup->t37;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t37		= 20;
	}
	if (sng_isup->t38 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t38		= sng_isup->t38;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t38		= 1200;
	}
	if (sng_isup->t39 != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t39		= sng_isup->t39;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].t39		= 300;
	}
	if (sng_isup->tfgr != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].tfgr		= sng_isup->tfgr;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].tfgr		= 50;
	}
	if (sng_isup->tpause != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].tpause	= sng_isup->tpause;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].tpause	= 150;
	}
	if (sng_isup->tstaenq != 0) {
		g_ftdm_sngss7_data.cfg.isupIntf[i].tstaenq	= sng_isup->tstaenq;
	} else {
		g_ftdm_sngss7_data.cfg.isupIntf[i].tstaenq	= 5000;
	}

	return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_isap(sng_isap_t *sng_isap)
{
	int i;

	/* go through all the existing interfaces and see if we find a match */
	i = 1;
	while (g_ftdm_sngss7_data.cfg.isap[i].id != 0) {
		if (g_ftdm_sngss7_data.cfg.isap[i].switchType == sng_isap->switchType) {

			/* we have a match so break out of this loop */
			break;
		}
		/* move on to the next one */
		i++;
	}

	if (g_ftdm_sngss7_data.cfg.isap[i].id == 0) {
		sng_isap->id = i;
		SS7_DEBUG("found new isup to cc interface, id is = %d\n", g_ftdm_sngss7_data.cfg.isap[i].id);
	} else {
		sng_isap->id = i;
		SS7_DEBUG("found existing isup to cc interface, id is = %d\n", g_ftdm_sngss7_data.cfg.isap[i].id);
	}

	g_ftdm_sngss7_data.cfg.isap[i].id 			= sng_isap->id;
	g_ftdm_sngss7_data.cfg.isap[i].suId			= sng_isap->id;
	g_ftdm_sngss7_data.cfg.isap[i].spId			= sng_isap->id;
	g_ftdm_sngss7_data.cfg.isap[i].switchType	= sng_isap->switchType;
	g_ftdm_sngss7_data.cfg.isap[i].ssf			= sng_isap->ssf;

	if (sng_isap->t1 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t1		= sng_isap->t1;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t1		= 200;
	}
	if (sng_isap->t2 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t2		= sng_isap->t2;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t2		= 1800;
	}
	if (sng_isap->t5 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t5		= sng_isap->t5;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t5		= 3000;
	}
	if (sng_isap->t6 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t6		= sng_isap->t6;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t6		= 200;
	}
	if (sng_isap->t7 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t7		= sng_isap->t7;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t7		= 250;
	}
	if (sng_isap->t8 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t8		= sng_isap->t8;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t8		= 120;
	}
	if (sng_isap->t9 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t9		= sng_isap->t9;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t9		= 1800;
	}
	if (sng_isap->t27 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t27		= sng_isap->t27;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t27		= 2400;
	}
	if (sng_isap->t31 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t31		= sng_isap->t31;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t31		= 3650;
	}
	if (sng_isap->t33 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t33		= sng_isap->t33;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t33		= 120;
	}
	if (sng_isap->t34 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t34		= sng_isap->t34;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t34		= 40;
	}
	if (sng_isap->t36 != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].t36		= sng_isap->t36;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].t36		= 120;
	}
	if (sng_isap->tccr != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].tccr		= sng_isap->tccr;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].tccr		= 200;
	}
	if (sng_isap->tccrt != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].tccrt	= sng_isap->tccrt;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].tccrt	= 20;
	}
	if (sng_isap->tex != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].tex		= sng_isap->tex;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].tex		= 1000;
	}
	if (sng_isap->tcrm != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].tcrm		= sng_isap->tcrm;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].tcrm		= 30;
	}
	if (sng_isap->tcra != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].tcra		= sng_isap->tcra;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].tcra		= 100;
	}
	if (sng_isap->tect != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].tect		= sng_isap->tect;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].tect		= 10;
	}
	if (sng_isap->trelrsp != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].trelrsp	= sng_isap->trelrsp;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].trelrsp	= 10;
	}
	if (sng_isap->tfnlrelrsp != 0) {
		g_ftdm_sngss7_data.cfg.isap[i].tfnlrelrsp	= sng_isap->tfnlrelrsp;
	} else {
		g_ftdm_sngss7_data.cfg.isap[i].tfnlrelrsp	= 10;
	}

	return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_self_route(int spc, int linkType, int switchType, int ssf)
{

	if (g_ftdm_sngss7_data.cfg.mtpRoute[0].dpc == 0){
		SS7_DEBUG("found new mtp3 self route\n");
	} else if (g_ftdm_sngss7_data.cfg.mtpRoute[0].dpc == spc) {
		SS7_DEBUG("found existing mtp3 self route\n");
		return FTDM_SUCCESS;
	} else {
		SS7_ERROR("found new mtp3 self route but it does not match the route already configured (dpc=%d:spc=%d)\n",
					g_ftdm_sngss7_data.cfg.mtpRoute[0].dpc,
					spc);
		return FTDM_FAIL;
	}

	strcpy((char *)g_ftdm_sngss7_data.cfg.mtpRoute[0].name, "self-route");

	g_ftdm_sngss7_data.cfg.mtpRoute[0].id			= 0;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].dpc			= spc;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].linkType		= linkType;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].switchType	= switchType;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].cmbLinkSetId	= 0;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].isSTP		= 0;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].ssf			= ssf;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t6			= 8;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t8			= 12;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t10			= 300;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t11			= 300;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t15			= 30;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t16			= 20;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t18			= 200;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t19			= 690;
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t21			= 650; 
	g_ftdm_sngss7_data.cfg.mtpRoute[0].t25			= 100;

	return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_circuits(sng_isupCkt_t *isupCkt)
{
	sngss7_chan_data_t	*ss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sng_timeslot_t		timeslot;
	int					count;
	int					i;
	int					x;

	count = 1;

	while (isupCkt->ch_map[0] != '\0') {

		 /* pull out the next timeslot */
		if (ftmod_ss7_next_timeslot(isupCkt->ch_map, &timeslot)) {
			SS7_ERROR("Failed to parse the channel map!\n");
			return FTDM_FAIL;
		}

		if ((timeslot.siglink) || (timeslot.gap)) {
			/* try to find the channel in the circuits structure*/
			x = 1;
			while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
				if ((g_ftdm_sngss7_data.cfg.isupCkt[x].chan == count) &&
					(g_ftdm_sngss7_data.cfg.isupCkt[x].span == isupCkt->span->channels[1]->physical_span_id)) {

					SS7_DEVEL_DEBUG("Circuit for span=%d, chan=%d is already exists...id=%d\n",
								isupCkt->span->channels[1]->physical_span_id,
								count,
								x);

					/* we have a match so this circuit already exists in the structure */
					break;
				}
				/* move to the next circuit */
				x++;
			} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) */

			/* check why we exited the while loop */
			if (g_ftdm_sngss7_data.cfg.isupCkt[x].id == 0) {
				SS7_DEVEL_DEBUG("Circuit for span=%d, chan=%d is new...id=%d\n",
				isupCkt->span->channels[1]->physical_span_id,
				count,
				x);

				/* prepare the global info sturcture */
				ss7_info = ftdm_calloc(1, sizeof(sngss7_chan_data_t));
				ss7_info->ftdmchan = NULL;
				ss7_info->circuit = &g_ftdm_sngss7_data.cfg.isupCkt[x];

				/* circuit is new so fill in the needed information */
				g_ftdm_sngss7_data.cfg.isupCkt[x].id		  	= x;
				g_ftdm_sngss7_data.cfg.isupCkt[x].span			= isupCkt->span->channels[1]->physical_span_id;
				g_ftdm_sngss7_data.cfg.isupCkt[x].chan			= count;
				if (timeslot.siglink) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].type		= SIG;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].type		= HOLE;
				}

				if (timeslot.channel) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].cic		= isupCkt->cicbase;
					isupCkt->cicbase++;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].cic		= 0;
				}
				g_ftdm_sngss7_data.cfg.isupCkt[x].infId	   		= isupCkt->isupInf;
				g_ftdm_sngss7_data.cfg.isupCkt[x].typeCntrl   	= isupCkt->typeCntrl;
				g_ftdm_sngss7_data.cfg.isupCkt[x].ssf			= g_ftdm_sngss7_data.cfg.isupIntf[isupCkt->isupInf].ssf;
				g_ftdm_sngss7_data.cfg.isupCkt[x].obj			= ss7_info;

			} /* if (g_ftdm_sngss7_data.cfg.isupCkt[x].id == 0) */

			/* increment the span channel count */
			count++;

		} else { /* if ((timeslot.siglink) || (timeslot.gap)) */
			/* find the ftdm the channel structure for this channel*/
			i = 1;
			while (isupCkt->span->channels[i] != NULL) {
				if (isupCkt->span->channels[i]->physical_chan_id == timeslot.channel) {
					break;
				}
				i++;
			} /* while (span->channels[i] != NULL) */

			if (isupCkt->span->channels[i] == NULL) {
				/* we weren't able to find the channel in the ftdm channels */
				SS7_ERROR("Unable to find the requested channel %d in the FreeTDM channels!\n", timeslot.channel);
				return FTDM_FAIL;
			} else {
				ftdmchan = isupCkt->span->channels[i];
			}

			/* try to find a match for the physical span and chan */
			x = 1;
			while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
				if ((g_ftdm_sngss7_data.cfg.isupCkt[x].chan == ftdmchan->physical_chan_id) && 
					(g_ftdm_sngss7_data.cfg.isupCkt[x].span == ftdmchan->physical_span_id)) {

					/* we have a match so this circuit already exists in the structure */
					break;
				}
				/* move to the next circuit */
				x++;
			} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) */

			/* check why we exited the while loop */
			if (g_ftdm_sngss7_data.cfg.isupCkt[x].id == 0) {
				SS7_DEVEL_DEBUG("Circuit for span=%d, chan=%d is new...id=%d\n",
				ftdmchan->physical_span_id,
				ftdmchan->physical_chan_id,
				x);

				/* prepare the global info sturcture */
				ss7_info = ftdm_calloc(1, sizeof(sngss7_chan_data_t));
				ss7_info->ftdmchan = ftdmchan;
				ss7_info->circuit = &g_ftdm_sngss7_data.cfg.isupCkt[x];
				ftdmchan->call_data = ss7_info;

				/* prepare the timer structures */
				ss7_info->t35.sched			= ((sngss7_span_data_t *)isupCkt->span->mod_data)->sched;
				ss7_info->t35.counter		= 1;
				ss7_info->t35.beat			= g_ftdm_sngss7_data.cfg.isupIntf[isupCkt->isupInf].t35*100; /* beat is in ms, t35 is in 100ms */
				ss7_info->t35.callback		= handle_isup_t35;
				ss7_info->t35.sngss7_info	= ss7_info;

				/* circuit is new so fill in the needed information */
				g_ftdm_sngss7_data.cfg.isupCkt[x].id		= x;
				g_ftdm_sngss7_data.cfg.isupCkt[x].span		= ftdmchan->physical_span_id;
				g_ftdm_sngss7_data.cfg.isupCkt[x].chan		= ftdmchan->physical_chan_id;
				g_ftdm_sngss7_data.cfg.isupCkt[x].type		= VOICE;
				g_ftdm_sngss7_data.cfg.isupCkt[x].cic		= isupCkt->cicbase;
				g_ftdm_sngss7_data.cfg.isupCkt[x].infId		= isupCkt->isupInf;
				g_ftdm_sngss7_data.cfg.isupCkt[x].typeCntrl	= isupCkt->typeCntrl;
				if (isupCkt->t3 == 0) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t3	= 1200;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t3	= isupCkt->t3;
				}
				if (isupCkt->t12 == 0) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t12	= 300;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t12	= isupCkt->t12;
				}
				if (isupCkt->t13 == 0) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t13	= 3000;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t13	= isupCkt->t13;
				}
				if (isupCkt->t14 == 0) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t14	= 300;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t14	= isupCkt->t14;
				}
				if (isupCkt->t15 == 0) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t15	= 3000;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t15	= isupCkt->t15;
				}
				if (isupCkt->t16 == 0) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t16	= 300;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t16	= isupCkt->t16;
				}
				if (isupCkt->t17 == 0) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t17	= 3000;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].t17	= isupCkt->t17;
				}
				if (isupCkt->tval == 0) {
					g_ftdm_sngss7_data.cfg.isupCkt[x].tval	= 10;
				} else {
					g_ftdm_sngss7_data.cfg.isupCkt[x].tval	= isupCkt->tval;
				}
				g_ftdm_sngss7_data.cfg.isupCkt[x].obj		= ss7_info;
				g_ftdm_sngss7_data.cfg.isupCkt[x].ssf		= g_ftdm_sngss7_data.cfg.isupIntf[isupCkt->isupInf].ssf;

				/* increment the cicbase */
				isupCkt->cicbase++;
			} else { /* if (g_ftdm_sngss7_data.cfg.isupCkt[x].id == 0) */
				SS7_DEBUG("Circuit for span=%d, chan=%d is new...id=%d\n",
									ftdmchan->physical_span_id,
									ftdmchan->physical_chan_id,
									x);

				/* for now make sure ss7_info is set to null */
				ss7_info = NULL;

				/* KONRAD FIX ME -> confirm that it is the same circuit */
			}  /* if (g_ftdm_sngss7_data.cfg.isupCkt[x].id == 0) */

			/* increment the span channel count */
			count++;
		} /* if ((timeslot.siglink) || (timeslot.gap)) */

		if (ss7_info == NULL) {
			SS7_ERROR("KONRAD -> circuit was not configured !\n");
			return FTDM_FAIL;
		}

		if (ss7_info->ftdmchan == NULL) {
			SS7_INFO("Added span = %d, chan = %d, cic = %d, FTDM chan = %d, ISUP cirId = %d\n",
						g_ftdm_sngss7_data.cfg.isupCkt[x].span,
						g_ftdm_sngss7_data.cfg.isupCkt[x].chan,
						g_ftdm_sngss7_data.cfg.isupCkt[x].cic,
						0,
						g_ftdm_sngss7_data.cfg.isupCkt[x].id);
		} else {
			SS7_INFO("Added span = %d, chan = %d, cic = %d, FTDM chan = %d, ISUP cirId = %d\n",
						g_ftdm_sngss7_data.cfg.isupCkt[x].span,
						g_ftdm_sngss7_data.cfg.isupCkt[x].chan,
						g_ftdm_sngss7_data.cfg.isupCkt[x].cic,
						ss7_info->ftdmchan->chan_id,
						g_ftdm_sngss7_data.cfg.isupCkt[x].id);
		}

	} /* while (ch_map[0] != '\0') */

	return 0;
}

/******************************************************************************/
static int ftmod_ss7_next_timeslot(char *ch_map, sng_timeslot_t *timeslot)
{
	int			i;
	int			x;
	int			lower;
	int			upper;
	char		tmp[5]; /*KONRAD FIX ME*/
	char		new_ch_map[MAX_CIC_LENGTH];

	memset(&tmp[0], '\0', sizeof(tmp));
	memset(&new_ch_map[0], '\0', sizeof(new_ch_map));
	memset(timeslot, 0x0, sizeof(sng_timeslot_t));

	SS7_DEVEL_DEBUG("Old channel map = \"%s\"\n", ch_map);

	/* start at the beginning of the ch_map */
	x = 0;

	switch (ch_map[x]) {
	/**************************************************************************/
	case 'S':
	case 's':   /* we have a sig link */
		timeslot->siglink = 1;

		/* check what comes next either a comma or a number */
		x++;
		if (ch_map[x] == ',') {
			timeslot->hole = 1;
			SS7_DEVEL_DEBUG(" Found a siglink in the channel map with a hole in the cic map\n");
		} else if (isdigit(ch_map[x])) {
			/* consume all digits until a comma as this is the channel */
			SS7_DEVEL_DEBUG(" Found a siglink in the channel map with out a hole in the cic map\n");
		} else {
			SS7_ERROR("Found an illegal channel map character after signal link flag = \"%c\"!\n", ch_map[x]);
			return FTDM_FAIL;
		}
		break;
	/**************************************************************************/
	case 'G':
	case 'g':   /* we have a channel gap */
		timeslot->gap = 1;

		/* check what comes next either a comma or a number */
		x++;
		if (ch_map[x] == ',') {
			timeslot->hole = 1;
			SS7_DEVEL_DEBUG(" Found a gap in the channel map with a hole in the cic map\n");
		} else if (isdigit(ch_map[x])) {
			SS7_DEVEL_DEBUG(" Found a gap in the channel map with out a hole in the cic map\n");
			/* consume all digits until a comma as this is the channel */
		} else {
			SS7_ERROR("Found an illegal channel map character after signal link flag = \"%c\"!\n", ch_map[x]);
			return FTDM_FAIL;
		}
		break;
	/**************************************************************************/
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':   /* we have a channel */
		/* consume all digits until a comma or a dash */
		SS7_DEVEL_DEBUG("Found a starting channel in the channel map\n");
		break;
	/**************************************************************************/
	default:
		SS7_ERROR("Found an illegal channel map character = \"%c\"!\n", ch_map[x]);
		return FTDM_FAIL;
	/**************************************************************************/
	} /* switch (ch_map[x]) */

	/* grab the first number in the string */
	i = 0;
	while ((ch_map[x] != '\0') && (ch_map[x] != '-') && (ch_map[x] != ',')) {
		tmp[i] = ch_map[x];
		i++;
		x++;
	}
	tmp[i] = '\0';
	timeslot->channel = atoi(tmp);
	lower = timeslot->channel + 1;

	/* check the next value in the list */
	if (ch_map[x] == '-') {
		/* consume the number after the dash */
		x++;
		i = 0;
		while ((ch_map[x] != '\0') && (ch_map[x] != '-') && (ch_map[x] != ',')) {
			tmp[i] = ch_map[x];
			i++;
			x++;
		}
		tmp[i] = '\0';
		upper = atoi(tmp);

		/* check if the upper end of the range is the same as the lower end of the range */
		if (upper == lower) {
			/* the range is completed, eat the next comma or \0  and write it */
			sprintf(new_ch_map, "%d", lower);
		} else if ( upper > lower) {
			/* the list continues, add 1 from the channel map value and re-insert it to the list */
			sprintf(new_ch_map, "%d-%d", lower, upper);
		} else {
			SS7_ERROR("The upper is less then the lower end of the range...should not happen!\n");
			return FTDM_FAIL;
		}

		/* the the rest of ch_map to new_ch_map */
		strncat(new_ch_map, &ch_map[x], strlen(&ch_map[x]));

		/* set the new cic map to ch_map*/
		strcpy(ch_map, new_ch_map);

	} else if (ch_map[x] == ',') {
		/* move past the comma */
		x++;

		/* copy the rest of the list to new_ch_map */
		strcpy(new_ch_map, &ch_map[x]);

		/* copy the new_ch_map over the old one */
		strcpy(ch_map, new_ch_map);

	} else if (ch_map[x] == '\0') {
		/* we're at the end of the string...copy the rest of the list to new_ch_map */
		strcpy(new_ch_map, &ch_map[x]);

		/* set the new cic map to ch_map*/
		strcpy(ch_map, new_ch_map);
	} else { 
		/* nothing to do */
	}

	SS7_DEVEL_DEBUG("New channel map = \"%s\"\n", ch_map);

	return FTDM_SUCCESS;
}

/******************************************************************************/

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
