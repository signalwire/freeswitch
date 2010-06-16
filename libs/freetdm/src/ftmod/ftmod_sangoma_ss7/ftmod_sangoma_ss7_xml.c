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
    int     channel;
    int     siglink;
    int     gap;
    int     hole;
}sng_timeslot_t;
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
int ftmod_ss7_parse_xml(ftdm_conf_parameter_t *ftdm_parameters, ftdm_span_t *span);

static int ftmod_ss7_parse_sng_isup(ftdm_conf_node_t *sng_isup);

static int ftmod_ss7_parse_mtp_linksets(ftdm_conf_node_t *mtp_linksets);
static int ftmod_ss7_parse_mtp_linkset(ftdm_conf_node_t *mtp_linkset);
static int ftmod_ss7_parse_mtp_link(ftdm_conf_node_t *mtp_link, sng_mtp1Link_t *mtp1_link,
                                     sng_mtp2Link_t *mtp2_link, sng_mtp3Link_t *mtp3_link);

static int ftmod_ss7_parse_mtp_routes(ftdm_conf_node_t *mtp_routes);
static int ftmod_ss7_parse_mtp_route(ftdm_conf_node_t *mtp_route);

static int ftmod_ss7_parse_isup_interfaces(ftdm_conf_node_t *isup_interfaces);
static int ftmod_ss7_parse_isup_interface(ftdm_conf_node_t *isup_interface);

static int ftmod_ss7_fill_in_mtp1_link(sng_mtp1Link_t *mtp1_link);
static int ftmod_ss7_fill_in_mtp2_link(sng_mtp2Link_t *mtp2_link);
static int ftmod_ss7_fill_in_mtp3_link(sng_mtp3Link_t *mtp3_link);
static int ftmod_ss7_fill_in_mtp3_linkset(sng_mtp3LinkSet_t *mtp3_linkset);
static int ftmod_ss7_fill_in_mtp3_route(sng_mtp3Route_t *mtp3_route);
static int ftmod_ss7_fill_in_mtp3_isup_interface(sng_mtp3Route_t *mtp3_route);
static int ftmod_ss7_fill_in_isup_interface(sng_isupInterface_t *sng_isup);
static int ftmod_ss7_fill_in_isup_cc_interface(sng_isup_cc_t *sng_cc);

static int ftmod_ss7_fill_in_self_route(int spc, int linkType, int switchType, int ssf);

static int ftmod_ss7_fill_in_circuits(char *ch_map, int cicbase, int typeCntrl, int isup_id, ftdm_span_t *span);
static int ftmod_ss7_next_timeslot(char *ch_map, sng_timeslot_t *timeslot);

/******************************************************************************/

/* FUNCTIONS ******************************************************************/
int ftmod_ss7_parse_xml(ftdm_conf_parameter_t *ftdm_parameters, ftdm_span_t *span)
{
    int              i = 0;
    int              x = 0;
    const char       *var = NULL;
    const char       *val = NULL;
    ftdm_conf_node_t *ptr = NULL;
    sng_mtp3Route_t  self_route;
    char             ch_map[MAX_CIC_MAP_LENGTH];
    int              typeCntrl = 0;
    int              cicbase = 0;

    /* clean out the self route */
    memset(&self_route, 0x0, sizeof(sng_mtp3Route_t));

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
            strcpy(ch_map, val);
            SS7_DEBUG("\tFound channel map \"%s\"\n", ch_map);
        /**********************************************************************/
        } else if (!strcasecmp(var, "typeCntrl")) {
            if (!strcasecmp(val, "bothway")) {
                typeCntrl = SNG_BOTHWAY;
                SS7_DEBUG("\tFound control type \"bothway\"\n");
            } else if (!strcasecmp(val, "incoming")) {
                typeCntrl = SNG_INCOMING;
                SS7_DEBUG("\tFound control type \"incoming\"\n");
            } else if (!strcasecmp(val, "outgoing")) {
                typeCntrl = SNG_OUTGOING;
                SS7_DEBUG("\tFound control type \"outgoing\"\n");
            } else if (!strcasecmp(val, "controlled")) {
                typeCntrl = SNG_CONTROLLED;
                SS7_DEBUG("\tFound control type \"controlled\"\n");
            } else if (!strcasecmp(val, "controlling")) {
                typeCntrl = SNG_CONTROLLING;
                SS7_DEBUG("\tFound control type \"controlling\"\n");
            } else {
                SS7_ERROR("Found invalid circuit control type \"%s\"!", val);
                goto ftmod_ss7_parse_xml_error;
            }
        /**********************************************************************/
        } else if (!strcasecmp(var, "cicbase")) {
            cicbase = atoi(val);
            SS7_DEBUG("\tFound cicbase = %d\n", cicbase);
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
            while (g_ftdm_sngss7_data.cfg.isupInterface[x].id != 0) {
                if (!strcasecmp(g_ftdm_sngss7_data.cfg.isupInterface[x].name, val)) {
                    /* we have a match so break out of this loop */
                    break;
                }
                /* move on to the next one */
                x++;
            }
            SS7_DEBUG("\tFound isup_interface = %s\n",g_ftdm_sngss7_data.cfg.isupInterface[x].name );
        /**********************************************************************/
        } else {
            SS7_ERROR("Unknown parameter found =\"%s\"...ignoring it!\n", var);
        /**********************************************************************/
        }

        i++;
    } /* while (ftdm_parameters[i].var != NULL) */

    /* setup the self mtp3 route */
    i = g_ftdm_sngss7_data.cfg.isupInterface[x].mtp3RouteId;

    if(ftmod_ss7_fill_in_self_route(atoi(g_ftdm_sngss7_data.cfg.spc),
                                    g_ftdm_sngss7_data.cfg.mtp3Route[i].linkType,
                                    g_ftdm_sngss7_data.cfg.isupInterface[x].switchType,
                                    g_ftdm_sngss7_data.cfg.mtp3Route[i].ssf)) {
        SS7_ERROR("Failed to fill in self route structure!\n");
        goto ftmod_ss7_parse_xml_error;
    }


    /* setup the circuits structure */
    if(ftmod_ss7_fill_in_circuits(ch_map, 
                                  cicbase, 
                                  typeCntrl, 
                                  g_ftdm_sngss7_data.cfg.isupInterface[x].id,
                                  span)) {
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
    ftdm_conf_node_t    *mtp_linksets = NULL;
    ftdm_conf_node_t    *mtp_routes = NULL;
    ftdm_conf_node_t    *isup_interfaces = NULL;
    ftdm_conf_node_t    *tmp_node = NULL;

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
    ftdm_conf_node_t    *mtp_linkset = NULL;

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
    ftdm_conf_parameter_t   *parm = mtp_linkset->parameters;
    int                     num_parms = mtp_linkset->n_parameters;
    ftdm_conf_node_t        *mtp_link = NULL;
    sng_mtp1Link_t          mtp1_link[MAX_MTP_LINKS];
    sng_mtp2Link_t          mtp2_link[MAX_MTP_LINKS];
    sng_mtp3Link_t          mtp3_link[MAX_MTP_LINKS];
    sng_mtp3LinkSet_t       mtp3_linkset;
    int                     count;
    int                     i;

    /* initialize the mtp_link structures */
    for (i = 0; i < MAX_MTP_LINKS; i++) {
        memset(&mtp1_link[i], 0x0, sizeof(mtp1_link[i]));
        memset(&mtp2_link[i], 0x0, sizeof(mtp2_link[i]));
        memset(&mtp3_link[i], 0x0, sizeof(mtp3_link[i]));
    }
    memset(&mtp3_linkset, 0x0, sizeof(mtp3_linkset));

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
            strcpy((char *)mtp3_linkset.name, parm->val);
            SS7_DEBUG("\tFound an \"mtp_linkset\" named = %s\n", mtp3_linkset.name);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "apc")) {
            mtp3_linkset.apc = atoi(parm->val);
            SS7_DEBUG("\tFound mtp3_linkSet->apc = %d\n", mtp3_linkset.apc);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "minActive")) {
            mtp3_linkset.minActive = atoi(parm->val);
            SS7_DEBUG("\tFound mtp3_linkSet->minActive = %d\n", mtp3_linkset.minActive);
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
        if (ftmod_ss7_parse_mtp_link(mtp_link, &mtp1_link[count], &mtp2_link[count], &mtp3_link[count] )) {
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
    } else {
        mtp3_linkset.numLinks = count;
    }

    /* now we need to see if this linkset exists already or not and grab an Id */
    i = 1;
    while (g_ftdm_sngss7_data.cfg.mtp3LinkSet[i].id != 0) {
        if (!strcasecmp((const char *)g_ftdm_sngss7_data.cfg.mtp3LinkSet[i].name, (const char *)mtp3_linkset.name)) {
            /* we've found the linkset...so it has already been configured */
            break;
        }
        i++;
        /* add in error check to make sure we don't go out-of-bounds */
    }

    /* if the id value is 0 that means we didn't find the linkset */
    if (g_ftdm_sngss7_data.cfg.mtp3LinkSet[i].id  == 0) {
        mtp3_linkset.id = i;
        SS7_DEBUG("found new mtp3_linkset, id is = %d\n", mtp3_linkset.id);
    } else {
        mtp3_linkset.id = i;
        SS7_DEBUG("found existing mtp3_linkset, id is = %d\n", mtp3_linkset.id);
    }

    /* we now have all the information to fill in the Libsng_ss7 structures */
    i = 0;
    count = 0;
    while (mtp1_link[i].span != 0 ){
        /* fill in the mtp1 link structure */
        mtp2_link[i].spId = ftmod_ss7_fill_in_mtp1_link(&mtp1_link[i]);
        /* fill in the mtp2 link structure */
        mtp3_link[i].mtp2LinkId = ftmod_ss7_fill_in_mtp2_link(&mtp2_link[i]);
        /* have to grab a couple of values from the linkset */
        mtp3_link[i].apc = mtp3_linkset.apc;
        mtp3_link[i].linkSetId = mtp3_linkset.id;
        /* fill in the mtp3 link structure */
        mtp3_linkset.links[count] = ftmod_ss7_fill_in_mtp3_link(&mtp3_link[i]);
        /* increment the links counter */
        count++;
        /* increment the index value */
        i++;
    }

    ftmod_ss7_fill_in_mtp3_linkset(&mtp3_linkset);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_mtp_link(ftdm_conf_node_t *mtp_link, sng_mtp1Link_t *mtp1_link,
            sng_mtp2Link_t *mtp2_link, sng_mtp3Link_t *mtp3_link)
{
    ftdm_conf_parameter_t   *parm = mtp_link->parameters;
    int                     num_parms = mtp_link->n_parameters;
    int                     i;

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
            strcpy((char *)mtp1_link->name, parm->val);
            strcpy((char *)mtp2_link->name, parm->val);
            strcpy((char *)mtp3_link->name, parm->val);
            SS7_DEBUG("\tFound an \"mtp_link\" named = %s\n", mtp1_link->name);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "span")) {
            mtp1_link->span = atoi(parm->val);
            SS7_DEBUG("\tFound mtp1_link->span = %d\n", mtp1_link->span);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "chan")) {
            mtp1_link->chan = atoi(parm->val);
            SS7_DEBUG("\tFound mtp1_link->chan = %d\n", mtp1_link->chan);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "span")) {
            mtp1_link->span = atoi(parm->val);
            SS7_DEBUG("\tFound mtp1_link->span = %d\n", mtp1_link->span);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "errorType")) {
            if (!strcasecmp(parm->val, "basic")) {
                mtp2_link->errorType = SNG_BASIC_ERR;
            } else if (!strcasecmp(parm->val, "pcr")) {
                mtp2_link->errorType = SNG_PCR_ERR;
            } else {
                SS7_ERROR("\tFound an invalid \"errorType\" = %s\n", parm->var);
                return FTDM_FAIL;
            }
            SS7_DEBUG("\tFound mtp2_link->errorType=%s\n", parm->val);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "lssuLength")) {
            mtp2_link->lssuLength = atoi(parm->val);
            if ((mtp2_link->lssuLength != 1) && (mtp2_link->lssuLength != 2)) {
                SS7_ERROR("\tFound an invalid \"lssuLength\" = %d\n", mtp2_link->lssuLength);
                return FTDM_FAIL;
            } else {
                SS7_DEBUG("\tFound mtp2_link->lssuLength=%d\n", mtp2_link->lssuLength);
            }
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "priority")) {
            mtp3_link->priority = atoi(parm->val);
            if ((mtp3_link->priority == 0) || (mtp3_link->priority == 1) || 
                (mtp3_link->priority == 2) || (mtp3_link->priority == 3)) {
                SS7_DEBUG("\tFound mtp3_link->priority = %d\n",mtp3_link->priority);
            } else {
                SS7_ERROR("\tFound an invalid \"priority\"=%d\n", mtp3_link->priority);
                return FTDM_FAIL;
            } 
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "linkType")) {
            if (!strcasecmp(parm->val, "itu92")) {
                mtp2_link->linkType = SNG_MTP2_ITU92;
                mtp3_link->linkType = SNG_MTP3_ITU92;
                SS7_DEBUG("\tFound mtp3_link->linkType = \"ITU92\"\n");
            } else if (!strcasecmp(parm->val, "itu88")) {
                mtp2_link->linkType = SNG_MTP2_ITU88;
                mtp3_link->linkType = SNG_MTP3_ITU88;
                SS7_DEBUG("\tFound mtp3_link->linkType = \"ITU88\"\n");
            } else {
                SS7_ERROR("\tFound an invalid linktype of \"%s\"!\n", parm->val);
                return FTDM_FAIL;
            }
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "switchType")) {
            if (!strcasecmp(parm->val, "itu97")) {
                mtp3_link->switchType = SNG_ISUP_ITU97;
                SS7_DEBUG("\tFound mtp3_link->switchType = \"ITU97\"\n");
            } else if (!strcasecmp(parm->val, "itu88")) {
                mtp3_link->switchType = SNG_ISUP_ITU88;
                SS7_DEBUG("\tFound mtp3_link->switchType = \"ITU88\"\n");
            } else if (!strcasecmp(parm->val, "itu92")) {
                mtp3_link->switchType = SNG_ISUP_ITU92;
                SS7_DEBUG("\tFound mtp3_link->switchType = \"ITU92\"\n");
            } else if (!strcasecmp(parm->val, "itu00")) {
                mtp3_link->switchType = SNG_ISUP_ITU00;
                SS7_DEBUG("\tFound mtp3_link->switchType = \"ITU00\"\n");
            } else if (!strcasecmp(parm->val, "ETSIV2")) {
                mtp3_link->switchType = SNG_ISUP_ETSIV2;
                SS7_DEBUG("\tFound mtp3_link->switchType = \"ETSIV2\"\n");
            } else if (!strcasecmp(parm->val, "ETSIV3")) {
                mtp3_link->switchType = SNG_ISUP_ETSIV3;
                SS7_DEBUG("\tFound mtp3_link->switchType = \"ETSIV3\"\n");
            } else if (!strcasecmp(parm->val, "UK")) {
                mtp3_link->switchType = SNG_ISUP_UK;
                SS7_DEBUG("\tFound mtp3_link->switchType = \"UK\"\n");
            } else if (!strcasecmp(parm->val, "RUSSIA")) {
                mtp3_link->switchType = SNG_ISUP_RUSSIA;
                SS7_DEBUG("\tFound mtp3_link->switchType = \"RUSSIA\"\n");
            } else if (!strcasecmp(parm->val, "INDIA")) {
                mtp3_link->switchType = SNG_ISUP_INDIA;
                SS7_DEBUG("\tFound mtp3_link->switchType = \"INDIA\"\n");
            } else {
                SS7_ERROR("\tFound an invalid linktype of \"%s\"!\n", parm->val);
                return FTDM_FAIL;
            }
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "ssf")) {
            if (!strcasecmp(parm->val, "nat")) {
                mtp3_link->ssf = SNG_SSF_NAT;
            } else if (!strcasecmp(parm->val, "int")) {
                mtp3_link->ssf = SNG_SSF_INTER;
            } else {
                SS7_ERROR("\tFound an invalid ssf of \"%s\"!\n", parm->val);
                return FTDM_FAIL;
            }
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "slc")) {
            mtp3_link->lnkTstSLC = atoi(parm->val);
            SS7_DEBUG("\tFound mtp3_link->slc = \"%d\"\n",mtp3_link->lnkTstSLC);
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
    ftdm_conf_node_t    *mtp_route = NULL;

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
    sng_mtp3Route_t         mtp3_route;
    ftdm_conf_parameter_t   *parm = mtp_route->parameters;
    int                     num_parms = mtp_route->n_parameters;
    int                     i;

    memset(&mtp3_route, 0x0, sizeof(sng_mtp3Route_t));

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
            strcpy((char *)mtp3_route.name, parm->val);
            SS7_DEBUG("\tFound an \"mtp_route\" named = %s\n", mtp3_route.name);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "dpc")) {
            mtp3_route.dpc = atoi(parm->val);
            SS7_DEBUG("\tFound mtp3_route->dpc = %d\n", mtp3_route.dpc);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "mtp_linkset")) {
            /* find the linkset by it's name */
            int x = 1;
            while (g_ftdm_sngss7_data.cfg.mtp3LinkSet[x].id != 0) {
                /* check if the name matches */
                if (!strcasecmp((char *)g_ftdm_sngss7_data.cfg.mtp3LinkSet[x].name, parm->val)) {
                    /* grab the mtp3_link id value first*/
                    int id = g_ftdm_sngss7_data.cfg.mtp3LinkSet[x].links[0];
                    /* now, harvest the required infomormation from the global structure */
                    mtp3_route.linkType     = g_ftdm_sngss7_data.cfg.mtp3Link[id].linkType;
                    mtp3_route.switchType   = g_ftdm_sngss7_data.cfg.mtp3Link[id].switchType;
                    mtp3_route.ssf          = g_ftdm_sngss7_data.cfg.mtp3Link[id].ssf;
                    mtp3_route.cmbLinkSetId = g_ftdm_sngss7_data.cfg.mtp3LinkSet[x].id;
                    break;
                }
                x++;
            }

            /* check why we exited the wile loop ... and react accordingly */
            if (mtp3_route.cmbLinkSetId == 0) {
                SS7_ERROR("\tFailed to find the linkset = \"%s\"!\n", parm->val);
                return FTDM_FAIL;
            } else {
                SS7_DEBUG("\tFound mtp3_route->linkset = %s\n", parm->val);
            }
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "isSTP")) {
            if (!strcasecmp(parm->val, "no")) {
                mtp3_route.isSTP = 0;
                SS7_DEBUG("\tFound mtp3_route->isSTP = no\n");
            } else if (!strcasecmp(parm->val, "yes")) {
                mtp3_route.isSTP = 1;
                SS7_DEBUG("\tFound mtp3_route->isSTP = yes\n");
            } else {
                SS7_ERROR("\tFound an invalid parameter for isSTP \"%s\"!\n", parm->val);
               return FTDM_FAIL;
            }
        /**********************************************************************/
        } else {
            SS7_ERROR("\tFound an invalid parameter \"%s\"!\n", parm->val);
            return FTDM_FAIL;

        }

        /* move to the next parameter */
        parm = parm + 1;
    }

    ftmod_ss7_fill_in_mtp3_route(&mtp3_route);

    ftmod_ss7_fill_in_mtp3_isup_interface(&mtp3_route);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_isup_interfaces(ftdm_conf_node_t *isup_interfaces)
{
    ftdm_conf_node_t    *isup_interface = NULL;

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
    sng_isupInterface_t     sng_isup;
    sng_isup_cc_t           sng_cc;
    ftdm_conf_parameter_t   *parm = isup_interface->parameters;
    int                     num_parms = isup_interface->n_parameters;
    int                     i;

    memset(&sng_isup, 0x0, sizeof(sng_isupInterface_t));
    memset(&sng_cc, 0x0, sizeof(sng_isup_cc_t));

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
			strcpy(g_ftdm_sngss7_data.cfg.spc, parm->val);
            SS7_DEBUG("\tFound SPC = %s\n", g_ftdm_sngss7_data.cfg.spc);
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "mtp_route")) {
            /* find the route by it's name */
            int x = 1;
            while (g_ftdm_sngss7_data.cfg.mtp3Route[x].id != 0) {
                /* check if the name matches */
                if (!strcasecmp((char *)g_ftdm_sngss7_data.cfg.mtp3Route[x].name, parm->val)) {
                    /* now, harvest the required information from the global structure */
                    sng_isup.mtp3RouteId    = g_ftdm_sngss7_data.cfg.mtp3Route[x].id;
                    sng_isup.dpc            = g_ftdm_sngss7_data.cfg.mtp3Route[x].dpc;
                    sng_isup.switchType     = g_ftdm_sngss7_data.cfg.mtp3Route[x].switchType;
                    sng_cc.switchType       = g_ftdm_sngss7_data.cfg.mtp3Route[x].switchType;

                    /* find the nwID from the mtp3_isup_interface */
                    int y = 1;
                    while (g_ftdm_sngss7_data.cfg.mtp3_isup[y].id != 0) {
                        if (g_ftdm_sngss7_data.cfg.mtp3_isup[y].linkType == g_ftdm_sngss7_data.cfg.mtp3Route[x].linkType &&
                            g_ftdm_sngss7_data.cfg.mtp3_isup[y].switchType == g_ftdm_sngss7_data.cfg.mtp3Route[x].switchType &&
                            g_ftdm_sngss7_data.cfg.mtp3_isup[y].ssf == g_ftdm_sngss7_data.cfg.mtp3Route[x].ssf) {

                            /* we have a match so break out of this loop */
                            break;
                        }
                        /* move on to the next one */
                        y++;
                    } /* while (g_ftdm_sngss7_data.cfg.mtp3_isup[y].id != 0) */

                    /* check how we exited the last while loop */
                    if (g_ftdm_sngss7_data.cfg.mtp3_isup[y].id == 0) {
                        SS7_ERROR("\tFailed to find the nwID for = \"%s\"!\n", parm->val);
                        return FTDM_FAIL;
                    } else {
                        sng_isup.nwId = g_ftdm_sngss7_data.cfg.mtp3_isup[y].nwId;
                    }

                    break;
                }
                x++;
            } /* while (g_ftdm_sngss7_data.cfg.mtp3Route[x].id != 0) */

            /* check why we exited the while loop ... and react accordingly */
            if (sng_isup.mtp3RouteId == 0) {
                SS7_ERROR("\tFailed to find the MTP3 Route = \"%s\"!\n", parm->val);
                return FTDM_FAIL;
            } else {
                SS7_DEBUG("\tFound MTP3 Route = %s\n", parm->val);
            }
        /**********************************************************************/
        } else if (!strcasecmp(parm->var, "ssf")) {
            if (!strcasecmp(parm->val, "nat")) {
                sng_isup.ssf = SNG_SSF_NAT;
                sng_cc.ssf   = SNG_SSF_NAT;
            } else if (!strcasecmp(parm->val, "int")) {
                sng_isup.ssf = SNG_SSF_INTER;
                sng_cc.ssf      = SNG_SSF_INTER;
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
        } else {
            SS7_ERROR("\tFound an invalid parameter \"%s\"!\n", parm->val);
            return FTDM_FAIL;

        }

        /* move to the next parameter */
        parm = parm + 1;
    }

    ftmod_ss7_fill_in_isup_interface(&sng_isup);

    ftmod_ss7_fill_in_isup_cc_interface(&sng_cc);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_mtp1_link(sng_mtp1Link_t *mtp1_link)
{
    int i;

    /* go through all the existing links and see if we find a match */
    i = 1;
    while (g_ftdm_sngss7_data.cfg.mtp1Link[i].id != 0) {
        if (g_ftdm_sngss7_data.cfg.mtp1Link[i].span == mtp1_link->span &&
            g_ftdm_sngss7_data.cfg.mtp1Link[i].chan == mtp1_link->chan) {

            /* we have a match so break out of this loop */
            break;
        }
        /* move on to the next one */
        i++;
    }

    /* if the id value is 0 that means we didn't find the link */
    if (g_ftdm_sngss7_data.cfg.mtp1Link[i].id  == 0) {
        mtp1_link->id = i;
        SS7_DEBUG("found new mtp1_link on span=%d, chan=%d, id = %d\n", 
                    mtp1_link->span, 
                    mtp1_link->chan, 
                    mtp1_link->id);
    } else {
        mtp1_link->id = i;
        SS7_DEBUG("found existing mtp1_link on span=%d, chan=%d, id = %d\n", 
                    mtp1_link->span, 
                    mtp1_link->chan, 
                    mtp1_link->id);
    }

    /* fill in the information */

    strcpy((char *)g_ftdm_sngss7_data.cfg.mtp1Link[i].name, (char *)mtp1_link->name);

    g_ftdm_sngss7_data.cfg.mtp1Link[i].id = mtp1_link->id;
    g_ftdm_sngss7_data.cfg.mtp1Link[i].span = mtp1_link->span;
    g_ftdm_sngss7_data.cfg.mtp1Link[i].chan = mtp1_link->chan;

    return (mtp1_link->id);
}

/******************************************************************************/
static int ftmod_ss7_fill_in_mtp2_link(sng_mtp2Link_t *mtp2_link)
{
    /* the mtp2link->spId is also the index value */

    int i = mtp2_link->spId;

    mtp2_link->id = i;

    if (g_ftdm_sngss7_data.cfg.mtp2Link[i].id == 0) {
        SS7_DEBUG("found new mtp2_link, id is = %d\n", mtp2_link->id);
    } else {
        SS7_DEBUG("found existing mtp2_link, id is = %d\n", mtp2_link->id);
    } 

    strcpy((char *)g_ftdm_sngss7_data.cfg.mtp2Link[i].name, (char *)mtp2_link->name);

    g_ftdm_sngss7_data.cfg.mtp2Link[i].id           = mtp2_link->id;
    g_ftdm_sngss7_data.cfg.mtp2Link[i].spId         = mtp2_link->spId;
    g_ftdm_sngss7_data.cfg.mtp2Link[i].linkType     = mtp2_link->linkType;
    g_ftdm_sngss7_data.cfg.mtp2Link[i].errorType    = mtp2_link->errorType;
    g_ftdm_sngss7_data.cfg.mtp2Link[i].lssuLength   = mtp2_link->lssuLength;

    if ( mtp2_link->t1 != 0 ) {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t1       = mtp2_link->t1;
    }else {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t1       = 500;
    }
    if ( mtp2_link->t2 != 0 ) {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t2       = mtp2_link->t2;
    }else {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t2       = 250;
    }
    if ( mtp2_link->t3 != 0 ) {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t3       = mtp2_link->t3;
    }else {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t3       = 20;
    }
    if ( mtp2_link->t4n != 0 ) {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t4n      = mtp2_link->t4n;
    }else {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t4n      = 80;
    }
    if ( mtp2_link->t4e != 0 ) {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t4e      = mtp2_link->t4e;
    }else {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t4e      = 5;
    }
    if ( mtp2_link->t5 != 0 ) {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t5       = mtp2_link->t5;
    }else {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t5       = 1;
    }
    if ( mtp2_link->t6 != 0 ) {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t6       = mtp2_link->t6;
    }else {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t6       = 60;
    }
    if ( mtp2_link->t7 != 0 ) {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t7       = mtp2_link->t7;
    }else {
        g_ftdm_sngss7_data.cfg.mtp2Link[i].t7       = 20;
    }

    return(mtp2_link->id);
}

/******************************************************************************/
static int ftmod_ss7_fill_in_mtp3_link(sng_mtp3Link_t *mtp3_link)
{
    int i = mtp3_link->mtp2LinkId;

    mtp3_link->id = i;

    if (g_ftdm_sngss7_data.cfg.mtp3Link[i].id == 0) {
        SS7_DEBUG("found new mtp3_link, id is = %d\n", mtp3_link->id);
    } else {
        SS7_DEBUG("found existing mtp3_link, id is = %d\n", mtp3_link->id);
    }

    strcpy((char *)g_ftdm_sngss7_data.cfg.mtp3Link[i].name, (char *)mtp3_link->name);

    g_ftdm_sngss7_data.cfg.mtp3Link[i].id           = mtp3_link->id;
    g_ftdm_sngss7_data.cfg.mtp3Link[i].mtp2LinkId   = mtp3_link->mtp2LinkId;
    g_ftdm_sngss7_data.cfg.mtp3Link[i].linkSetId    = mtp3_link->linkSetId;
    g_ftdm_sngss7_data.cfg.mtp3Link[i].priority     = mtp3_link->priority;
    g_ftdm_sngss7_data.cfg.mtp3Link[i].linkType     = mtp3_link->linkType;
    g_ftdm_sngss7_data.cfg.mtp3Link[i].switchType   = mtp3_link->switchType;
    g_ftdm_sngss7_data.cfg.mtp3Link[i].apc          = mtp3_link->apc;
    g_ftdm_sngss7_data.cfg.mtp3Link[i].ssf          = mtp3_link->ssf;
    g_ftdm_sngss7_data.cfg.mtp3Link[i].lnkTstSLC    = mtp3_link->lnkTstSLC;
    if (mtp3_link->t1 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t1       = mtp3_link->t1;
    } else { 
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t1       = 8;
    }
    if (mtp3_link->t2 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t2       = mtp3_link->t2;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t2       = 14;
    }
    if (mtp3_link->t3 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t3       = mtp3_link->t3;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t3       = 8;
    }
    if (mtp3_link->t4 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t4       = mtp3_link->t4;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t4       = 8;
    }
    if (mtp3_link->t5 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t5       = mtp3_link->t5;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t5       = 8;
    }
    if (mtp3_link->t7 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t7       = mtp3_link->t7;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t7       = 15;
    }
    if (mtp3_link->t12 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t12      = mtp3_link->t12;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t12      = 15;
    }
    if (mtp3_link->t13 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t13      = mtp3_link->t13;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t13      = 15;
    }
    if (mtp3_link->t14 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t14      = mtp3_link->t14;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t14      = 30;
    }
    if (mtp3_link->t17 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t17      = mtp3_link->t17;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t17      = 15;
    }
    if (mtp3_link->t22 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t22      = mtp3_link->t22;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t22      = 1800;
    }
    if (mtp3_link->t23 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t23      = mtp3_link->t23;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t23      = 1800;
    }
    if (mtp3_link->t24 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t24      = mtp3_link->t24;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t24      = 5;
    }
    if (mtp3_link->t31 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t31       = mtp3_link->t31;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t31      = 50;
    }
    if (mtp3_link->t32 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t32      = mtp3_link->t32;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t32      = 120;
    }
    if (mtp3_link->t33 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t33       = mtp3_link->t33;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t33      = 3000;
    }
    if (mtp3_link->t34 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t34       = mtp3_link->t34;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].t34      = 600;
    }
    if (mtp3_link->tflc != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].tflc       = mtp3_link->tflc;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Link[i].tflc     = 300;
    }

    return(mtp3_link->id);
}

/******************************************************************************/
static int ftmod_ss7_fill_in_mtp3_linkset(sng_mtp3LinkSet_t *mtp3_linkset)
{
    int count;
    int i = mtp3_linkset->id;

    strcpy((char *)g_ftdm_sngss7_data.cfg.mtp3LinkSet[i].name, (char *)mtp3_linkset->name);

    g_ftdm_sngss7_data.cfg.mtp3LinkSet[i].id          = mtp3_linkset->id;
    g_ftdm_sngss7_data.cfg.mtp3LinkSet[i].apc         = mtp3_linkset->apc;
    g_ftdm_sngss7_data.cfg.mtp3LinkSet[i].minActive   = mtp3_linkset->minActive;
    g_ftdm_sngss7_data.cfg.mtp3LinkSet[i].numLinks    = mtp3_linkset->numLinks;

    for (count = 0; count < mtp3_linkset->numLinks; count++) {
        g_ftdm_sngss7_data.cfg.mtp3LinkSet[i].links[count]    = mtp3_linkset->links[count];
    }

    return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_mtp3_route(sng_mtp3Route_t *mtp3_route)
{
    int i;

    /* go through all the existing routes and see if we find a match */
    i = 1;
    while (g_ftdm_sngss7_data.cfg.mtp3Route[i].id != 0) {
            if (g_ftdm_sngss7_data.cfg.mtp3Route[i].dpc == mtp3_route->dpc) {

            /* we have a match so break out of this loop */
            break;
        }
        /* move on to the next one */
        i++;
    }

    if (g_ftdm_sngss7_data.cfg.mtp3Route[i].id == 0) {
        mtp3_route->id = i;
        SS7_DEBUG("found new mtp3_route, id is = %d\n", mtp3_route->id);
    } else {
        mtp3_route->id = i;
        SS7_DEBUG("found existing mtp3_route, id is = %d\n", mtp3_route->id);
    }

    strcpy((char *)g_ftdm_sngss7_data.cfg.mtp3Route[i].name, (char *)mtp3_route->name);

    g_ftdm_sngss7_data.cfg.mtp3Route[i].id            = mtp3_route->id;
    g_ftdm_sngss7_data.cfg.mtp3Route[i].dpc           = mtp3_route->dpc;
    g_ftdm_sngss7_data.cfg.mtp3Route[i].linkType      = mtp3_route->linkType;
    g_ftdm_sngss7_data.cfg.mtp3Route[i].switchType    = mtp3_route->switchType;
    g_ftdm_sngss7_data.cfg.mtp3Route[i].cmbLinkSetId  = 1; /* mtp3_route->cmbLinkSetId;*/
    g_ftdm_sngss7_data.cfg.mtp3Route[i].isSTP         = mtp3_route->isSTP;
    g_ftdm_sngss7_data.cfg.mtp3Route[i].ssf           = mtp3_route->ssf;
    if (mtp3_route->t6 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t6        = mtp3_route->t6;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t6        = 8;
    }
    if (mtp3_route->t8 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t8        = mtp3_route->t8;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t8        = 12;
    }
    if (mtp3_route->t10 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t10        = mtp3_route->t10;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t10       = 300;
    }
    if (mtp3_route->t11 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t11        = mtp3_route->t11;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t11       = 300;
    }
    if (mtp3_route->t15 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t15        = mtp3_route->t15;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t15       = 30;
    }
    if (mtp3_route->t16 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t16        = mtp3_route->t16;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t16       = 20;
    }
    if (mtp3_route->t18 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t18        = mtp3_route->t18;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t18       = 200;
    }
    if (mtp3_route->t19 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t19        = mtp3_route->t19;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t19       = 690;
    }
    if (mtp3_route->t21 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t21        = mtp3_route->t21;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t21       = 650; 
    }
    if (mtp3_route->t25 != 0) {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t25        = mtp3_route->t25;
    } else {
        g_ftdm_sngss7_data.cfg.mtp3Route[i].t25       = 100;
    }

    return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_mtp3_isup_interface(sng_mtp3Route_t *mtp3_route)
{

    int i;

    /* go through all the existing interfaces and see if we find a match */
    i = 1;
    while (g_ftdm_sngss7_data.cfg.mtp3_isup[i].id != 0) {
        if (g_ftdm_sngss7_data.cfg.mtp3_isup[i].linkType == mtp3_route->linkType &&
            g_ftdm_sngss7_data.cfg.mtp3_isup[i].switchType == mtp3_route->switchType &&
            g_ftdm_sngss7_data.cfg.mtp3_isup[i].ssf == mtp3_route->ssf) {

            /* we have a match so break out of this loop */
            break;
        }
        /* move on to the next one */
        i++;
    }

    if (g_ftdm_sngss7_data.cfg.mtp3_isup[i].id == 0) {
        g_ftdm_sngss7_data.cfg.mtp3_isup[i].id = i;
        SS7_DEBUG("found new mtp3_isup interface, id is = %d\n", g_ftdm_sngss7_data.cfg.mtp3_isup[i].id);
    } else {
        g_ftdm_sngss7_data.cfg.mtp3_isup[i].id = i;
        SS7_DEBUG("found existing mtp3_isup interface, id is = %d\n", g_ftdm_sngss7_data.cfg.mtp3_isup[i].id);
    }

    g_ftdm_sngss7_data.cfg.mtp3_isup[i].nwId          = g_ftdm_sngss7_data.cfg.mtp3_isup[i].id;
    g_ftdm_sngss7_data.cfg.mtp3_isup[i].linkType      = mtp3_route->linkType;
    g_ftdm_sngss7_data.cfg.mtp3_isup[i].switchType    = mtp3_route->switchType;
    g_ftdm_sngss7_data.cfg.mtp3_isup[i].ssf           = mtp3_route->ssf;

    return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_isup_interface(sng_isupInterface_t *sng_isup)
{

    int i;

    /* go through all the existing interfaces and see if we find a match */
    i = 1;
    while (g_ftdm_sngss7_data.cfg.isupInterface[i].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isupInterface[i].nwId == sng_isup->nwId) {

            /* we have a match so break out of this loop */
            break;
        }
        /* move on to the next one */
        i++;
    }

    if (g_ftdm_sngss7_data.cfg.isupInterface[i].id == 0) {
        sng_isup->id = i;
        SS7_DEBUG("found new isup interface, id is = %d\n", sng_isup->id);
    } else {
        sng_isup->id = i;
        SS7_DEBUG("found existing isup interface, id is = %d\n", sng_isup->id);
    }

    strcpy((char *)g_ftdm_sngss7_data.cfg.isupInterface[i].name, (char *)sng_isup->name);

    g_ftdm_sngss7_data.cfg.isupInterface[i].id             = sng_isup->id;
    g_ftdm_sngss7_data.cfg.isupInterface[i].mtp3RouteId    = sng_isup->mtp3RouteId;
    g_ftdm_sngss7_data.cfg.isupInterface[i].nwId           = sng_isup->nwId;
    g_ftdm_sngss7_data.cfg.isupInterface[i].dpc            = sng_isup->dpc;
    g_ftdm_sngss7_data.cfg.isupInterface[i].switchType     = sng_isup->switchType;
    g_ftdm_sngss7_data.cfg.isupInterface[i].ssf            = sng_isup->ssf;
    if (sng_isup->t4 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t4         = sng_isup->t4;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t4         = 3000;
    }
    if (sng_isup->t10 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t10        = sng_isup->t10;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t10        = 50;
    }
    if (sng_isup->t11 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t11        = sng_isup->t11;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t11        = 170;
    }
    if (sng_isup->t18 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t18        = sng_isup->t18;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t18        = 300;
    }
    if (sng_isup->t19 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t19        = sng_isup->t19;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t19        = 3000;
    }
    if (sng_isup->t20 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t20        = sng_isup->t20;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t20        = 300;
    }
    if (sng_isup->t21 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t21        = sng_isup->t21;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t21        = 3000;
    }
    if (sng_isup->t22 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t22        = sng_isup->t22;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t22        = 300;
    }
    if (sng_isup->t23 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t23        = sng_isup->t23;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t23        = 3000;
    }
    if (sng_isup->t24 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t24        = sng_isup->t24;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t24        = 10;
    }
    if (sng_isup->t25 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t25        = sng_isup->t25;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t25        = 20;
    }
    if (sng_isup->t26 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t26        = sng_isup->t26;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t26        = 600;
    }
    if (sng_isup->t28 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t28        = sng_isup->t28;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t28        = 100;
    }
    if (sng_isup->t29 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t29        = sng_isup->t29;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t29        = 6;
    }
    if (sng_isup->t30 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t30        = sng_isup->t30;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t30        = 50;
    }
    if (sng_isup->t32 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t32        = sng_isup->t32;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t32        = 30;
    }
    if (sng_isup->t35 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t35        = sng_isup->t35;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t35        = 170;
    }
    if (sng_isup->t37 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t37        = sng_isup->t37;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t37        = 20;
    }
    if (sng_isup->t38 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t38        = sng_isup->t38;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t38        = 1200;
    }
    if (sng_isup->t39 != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t39        = sng_isup->t39;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].t39        = 300;
    }
    if (sng_isup->tfgr != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].tfgr       = sng_isup->tfgr;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].tfgr       = 50;
    }
    if (sng_isup->tpause != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].tpause     = sng_isup->tpause;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].tpause     = 150;
    }
    if (sng_isup->tstaenq != 0) {
        g_ftdm_sngss7_data.cfg.isupInterface[i].tstaenq    = sng_isup->tstaenq;
    } else {
        g_ftdm_sngss7_data.cfg.isupInterface[i].tstaenq    = 5000;
    }

    return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_isup_cc_interface(sng_isup_cc_t *sng_cc)
{

    int i;

    /* go through all the existing interfaces and see if we find a match */
    i = 1;
    while (g_ftdm_sngss7_data.cfg.isup_cc[i].id != 0) {
        if (g_ftdm_sngss7_data.cfg.isup_cc[i].switchType == sng_cc->switchType) {

            /* we have a match so break out of this loop */
            break;
        }
        /* move on to the next one */
        i++;
    }

    if (g_ftdm_sngss7_data.cfg.isup_cc[i].id == 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].id = i;
        SS7_DEBUG("found new isup to cc interface, id is = %d\n", g_ftdm_sngss7_data.cfg.isup_cc[i].id);
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].id = i;
        SS7_DEBUG("found existing isup to cc interface, id is = %d\n", g_ftdm_sngss7_data.cfg.isup_cc[i].id);
    }

    SS7_ERROR("KONRAD -> you're hard coding the CC id....fix me!!!!\n");
    g_ftdm_sngss7_data.cfg.isup_cc[i].ccId          = 1; /*KONRAD FIX ME */
    g_ftdm_sngss7_data.cfg.isup_cc[i].switchType    = sng_cc->switchType;
    g_ftdm_sngss7_data.cfg.isup_cc[i].ssf           = sng_cc->ssf;
    if (sng_cc->t1 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t1        = sng_cc->t1;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t1        = 200;
    }
    if (sng_cc->t2 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t2        = sng_cc->t2;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t2        = 1800;
    }
    if (sng_cc->t5 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t5        = sng_cc->t5;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t5        = 3000;
    }
    if (sng_cc->t6 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t6        = sng_cc->t6;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t6        = 200;
    }
    if (sng_cc->t7 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t7        = sng_cc->t7;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t7        = 250;
    }
    if (sng_cc->t8 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t8        = sng_cc->t8;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t8        = 120;
    }
    if (sng_cc->t9 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t9        = sng_cc->t9;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t9        = 1800;
    }
    if (sng_cc->t27 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t27       = sng_cc->t27;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t27       = 2400;
    }
    if (sng_cc->t31 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t31       = sng_cc->t31;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t31       = 3650;
    }
    if (sng_cc->t33 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t33       = sng_cc->t33;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t33       = 120;
    }
    if (sng_cc->t34 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t34       = sng_cc->t34;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t34       = 40;
    }
    if (sng_cc->t36 != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t36       = sng_cc->t36;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].t36       = 120;
    }
    if (sng_cc->tccr != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tccr      = sng_cc->tccr;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tccr      = 200;
    }
    if (sng_cc->tccrt != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tccrt     = sng_cc->tccrt;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tccrt     = 20;
    }
    if (sng_cc->tex != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tex       = sng_cc->tex;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tex       = 10;
    }
    if (sng_cc->tcrm != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tcrm      = sng_cc->tcrm;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tcrm      = 30;
    }
    if (sng_cc->tcra != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tcra      = sng_cc->tcra;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tcra      = 100;
    }
    if (sng_cc->tect != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tect      = sng_cc->tect;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tect      = 10;
    }
    if (sng_cc->trelrsp != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].trelrsp   = sng_cc->trelrsp;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].trelrsp   = 10;
    }
    if (sng_cc->tfnlrelrsp != 0) {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tfnlrelrsp= sng_cc->tfnlrelrsp;
    } else {
        g_ftdm_sngss7_data.cfg.isup_cc[i].tfnlrelrsp= 10;
    }

    return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_self_route(int spc, int linkType, int switchType, int ssf)
{

    if (g_ftdm_sngss7_data.cfg.mtp3Route[0].dpc == 0){
        SS7_DEBUG("found new mtp3 self route\n");
    } else if (g_ftdm_sngss7_data.cfg.mtp3Route[0].dpc == spc) {
        SS7_DEBUG("found existing mtp3 self route\n");
        return FTDM_SUCCESS;
    } else {
        SS7_ERROR("found new mtp3 self route but it does not much the route already configured\n");
        return FTDM_FAIL;
    }

    g_ftdm_sngss7_data.cfg.mtp3Route[0].id            = 0;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].dpc           = spc;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].linkType      = linkType;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].switchType    = switchType;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].cmbLinkSetId  = 0;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].isSTP         = 0;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].ssf           = 0;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t6        = 8;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t8        = 12;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t10       = 300;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t11       = 300;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t15       = 30;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t16       = 20;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t18       = 200;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t19       = 690;
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t21       = 650; 
    g_ftdm_sngss7_data.cfg.mtp3Route[0].t25        = 100;


    return 0;
}

/******************************************************************************/
static int ftmod_ss7_fill_in_circuits(char *ch_map, int cicbase, int typeCntrl, int isup_id, ftdm_span_t *span)
{
    sngss7_chan_data_t  *ss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;
    sng_timeslot_t      timeslot;
    int                 count;
    int                 i;
    int                 x;

    count = 1;

    while (ch_map[0] != '\0') {

         /* pull out the next timeslot */
        if (ftmod_ss7_next_timeslot(ch_map, &timeslot)) {
            SS7_ERROR("Failed to parse the channel map!\n");
            return FTDM_FAIL;
        }

        if ((timeslot.siglink) || (timeslot.gap)) {
            /* try to find the channel in the circuits structure*/
            x = 1;
            while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
                if ((g_ftdm_sngss7_data.cfg.isupCircuit[x].chan == count) &&
                    (g_ftdm_sngss7_data.cfg.isupCircuit[x].span == span->channels[1]->physical_span_id)) {

                    SS7_DEVEL_DEBUG("Circuit for span=%d, chan=%d is already exists...id=%d\n",
                                span->channels[1]->physical_span_id,
                                count,
                                x);

                    /* we have a match so this circuit already exists in the structure */
                    break;
                }
                /* move to the next circuit */
                x++;
            } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) */

                       /* check why we exited the while loop */
            if (g_ftdm_sngss7_data.cfg.isupCircuit[x].id == 0) {
                SS7_DEVEL_DEBUG("Circuit for span=%d, chan=%d is new...id=%d\n",
                span->channels[1]->physical_span_id,
                count,
                x);

                /* prepare the global info sturcture */
                ss7_info = ftdm_calloc(1, sizeof(sngss7_chan_data_t));
                ss7_info->ftdmchan = NULL;
                ss7_info->circuit = &g_ftdm_sngss7_data.cfg.isupCircuit[x];

                /* circuit is new so fill in the needed information */
                g_ftdm_sngss7_data.cfg.isupCircuit[x].id          = x;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].span        = span->channels[1]->physical_span_id;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].chan        = count;
                if (timeslot.siglink) {
                    g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink     = 1;
                } else {
                    g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink     = 0;
                }
                if (timeslot.channel) {
                    g_ftdm_sngss7_data.cfg.isupCircuit[x].hole        = 0;
                    g_ftdm_sngss7_data.cfg.isupCircuit[x].cic         = cicbase;
                    cicbase++;
                } else {
                    g_ftdm_sngss7_data.cfg.isupCircuit[x].hole        = 1;
                    g_ftdm_sngss7_data.cfg.isupCircuit[x].cic         = 0;
                }
                g_ftdm_sngss7_data.cfg.isupCircuit[x].infId       = isup_id;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].typeCntrl   = typeCntrl;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t3          = 1200;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t12         = 300;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t13         = 3000;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t14         = 300;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t15         = 3000;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t16         = 300;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t17         = 3000;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].tval        = 10;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].obj         = ss7_info;

            } /* if (g_ftdm_sngss7_data.cfg.isupCircuit[x].id == 0) */
        } else { /* if ((timeslot.siglink) || (timeslot.gap)) */
            /* find the ftdm the channel structure for this channel*/
            i = 1;
            while (span->channels[i] != NULL) {
                if (span->channels[i]->physical_chan_id == timeslot.channel) {
                    break;
                }
                i++;
            } /* while (span->channels[i] != NULL) */

            if (span->channels[i] == NULL) {
                /* we weren't able to find the channel in the ftdm channels */
                SS7_ERROR("Unable to find the requested channel %d in the FreeTDM channels!\n", timeslot.channel);
                return FTDM_FAIL;
            } else {
                ftdmchan = span->channels[i];
            }

            /* try to find a match for the physical span and chan */
            x = 1;
            while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) {
                if ((g_ftdm_sngss7_data.cfg.isupCircuit[x].chan == ftdmchan->physical_chan_id) 
                    && (g_ftdm_sngss7_data.cfg.isupCircuit[x].span == ftdmchan->physical_span_id)) {

                    /* we have a match so this circuit already exists in the structure */
                    break;
                }
                /* move to the next circuit */
                x++;
            } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[x].id != 0) */

            /* check why we exited the while loop */
            if (g_ftdm_sngss7_data.cfg.isupCircuit[x].id == 0) {
                SS7_DEVEL_DEBUG("Circuit for span=%d, chan=%d is new...id=%d\n",
                ftdmchan->physical_span_id,
                ftdmchan->physical_chan_id,
                x);

                /* prepare the global info sturcture */
                ss7_info = ftdm_calloc(1, sizeof(sngss7_chan_data_t));
                ss7_info->ftdmchan = ftdmchan;
                ss7_info->circuit = &g_ftdm_sngss7_data.cfg.isupCircuit[x];
                ftdmchan->call_data = ss7_info;

                /* prepare the timer structures */
                ss7_info->t35.sched         = sngss7_sched;
                ss7_info->t35.counter       = 1;
                ss7_info->t35.beat          = g_ftdm_sngss7_data.cfg.isupInterface[isup_id].t35*100; /* beat is in ms, t35 is in 100ms */
                ss7_info->t35.callback      = handle_isup_t35;
                ss7_info->t35.sngss7_info   = ss7_info;

                /* circuit is new so fill in the needed information */
                g_ftdm_sngss7_data.cfg.isupCircuit[x].id          = x;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].span        = ftdmchan->physical_span_id;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].chan        = ftdmchan->physical_chan_id;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].siglink     = 0;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].hole        = 0;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].cic         = cicbase;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].infId       = isup_id;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].typeCntrl   = typeCntrl;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t3          = 1200;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t12         = 300;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t13         = 3000;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t14         = 300;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t15         = 3000;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t16         = 300;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].t17         = 3000;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].tval        = 10;
                g_ftdm_sngss7_data.cfg.isupCircuit[x].obj         = ss7_info;

                /* increment the cicbase */
                cicbase++;
            } else { /* if (g_ftdm_sngss7_data.cfg.isupCircuit[x].id == 0) */
                SS7_DEBUG("Circuit for span=%d, chan=%d is new...id=%d\n",
                                    ftdmchan->physical_span_id,
                                    ftdmchan->physical_chan_id,
                                    x);

                /* for now make sure ss7_info is set to null */
                ss7_info = NULL;

                /* KONRAD FIX ME -> confirm that it is the same circuit */
            }  /* if (g_ftdm_sngss7_data.cfg.isupCircuit[x].id == 0) */

            /* increment the span channel count */
            count++;
        } /* if ((timeslot.siglink) || (timeslot.gap)) */

        if (ss7_info == NULL) {
            SS7_ERROR("KONRAD -> circuit was not configured !\n");
            return FTDM_FAIL;
        }

        if (ss7_info->ftdmchan == NULL) {
            SS7_INFO("Added span = %d, chan = %d, cic = %d, FTDM chan = %d, ISUP cirId = %d\n",
                        g_ftdm_sngss7_data.cfg.isupCircuit[x].span,
                        g_ftdm_sngss7_data.cfg.isupCircuit[x].chan,
                        g_ftdm_sngss7_data.cfg.isupCircuit[x].cic,
                        0,
                        g_ftdm_sngss7_data.cfg.isupCircuit[x].id);
        } else {
            SS7_INFO("Added span = %d, chan = %d, cic = %d, FTDM chan = %d, ISUP cirId = %d\n",
                        g_ftdm_sngss7_data.cfg.isupCircuit[x].span,
                        g_ftdm_sngss7_data.cfg.isupCircuit[x].chan,
                        g_ftdm_sngss7_data.cfg.isupCircuit[x].cic,
                        ss7_info->ftdmchan->chan_id,
                        g_ftdm_sngss7_data.cfg.isupCircuit[x].id);
        }

    } /* while (ch_map[0] != '\0') */

    return 0;
}

/******************************************************************************/
static int ftmod_ss7_next_timeslot(char *ch_map, sng_timeslot_t *timeslot)
{
    int             i;
    int             x;
    int             lower;
    int             upper;
    char            tmp[5]; /*KONRAD FIX ME*/
    char            new_ch_map[MAX_CIC_LENGTH];

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
