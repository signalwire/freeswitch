/*
 * Copyright (c) 2012, Kapil Gupta <kgupta@sangoma.com>
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
 *
 *
 * Contributors: 
 *
 *
 */

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

static int ftmod_ss7_parse_nif_interface(ftdm_conf_node_t *nif_interface);
static int ftmod_ss7_parse_m2ua_interface(ftdm_conf_node_t *m2ua_interface);
static int ftmod_ss7_parse_m2ua_peer_interface(ftdm_conf_node_t *m2ua_peer_interface);
static int ftmod_ss7_parse_m2ua_clust_interface(ftdm_conf_node_t *m2ua_clust_interface);
static int ftmod_ss7_fill_in_nif_interface(sng_nif_cfg_t *nif_iface);
static int ftmod_ss7_fill_in_m2ua_interface(sng_m2ua_cfg_t *m2ua_iface);
static int ftmod_ss7_fill_in_m2ua_peer_interface(sng_m2ua_peer_cfg_t *m2ua_peer_face);
static int ftmod_ss7_fill_in_m2ua_clust_interface(sng_m2ua_cluster_cfg_t *m2ua_cluster_face);

static int ftmod_ss7_parse_sctp_link(ftdm_conf_node_t *node);

/******************************************************************************/
int ftmod_ss7_parse_nif_interfaces(ftdm_conf_node_t *nif_interfaces)
{
	ftdm_conf_node_t	*nif_interface = NULL;

	/* confirm that we are looking at sng_nif_interfaces */
	if (strcasecmp(nif_interfaces->name, "sng_nif_interfaces")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"sng_nif_interfaces\"!\n",nif_interfaces->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"nif_interfaces\"...\n");
	}

	/* extract the isup_interfaces */
	nif_interface = nif_interfaces->child;

	while (nif_interface != NULL) {
		/* parse the found mtp_route */
		if (ftmod_ss7_parse_nif_interface(nif_interface)) {
			SS7_ERROR("Failed to parse \"nif_interface\"\n");
			return FTDM_FAIL;
		}

		/* go to the next nif_interface */
		nif_interface = nif_interface->next;
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_nif_interface(ftdm_conf_node_t *nif_interface)
{
	sng_nif_cfg_t			sng_nif;
	ftdm_conf_parameter_t		*parm = nif_interface->parameters;
	int				num_parms = nif_interface->n_parameters;
	int				i;

	/* initalize the nif intf and isap structure */
	memset(&sng_nif, 0x0, sizeof(sng_nif));

	if(!nif_interface){
		SS7_ERROR("ftmod_ss7_parse_nif_interface: Null XML Node pointer \n");
		return FTDM_FAIL;
	}

	/* confirm that we are looking at an nif_interface */
	if (strcasecmp(nif_interface->name, "sng_nif_interface")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"nif_interface\"!\n",nif_interface->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"nif_interface\"...\n");
	}


	for (i = 0; i < num_parms; i++) {
	/**************************************************************************/

		/* try to match the parameter to what we expect */
		if (!strcasecmp(parm->var, "name")) {
		/**********************************************************************/
			strcpy((char *)sng_nif.name, parm->val);
			SS7_DEBUG("Found an nif_interface named = %s\n", sng_nif.name);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "id")) {
		/**********************************************************************/
			sng_nif.id = atoi(parm->val);
			SS7_DEBUG("Found an nif id = %d\n", sng_nif.id);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "m2ua-interface-id")) {
		/**********************************************************************/
			sng_nif.m2uaLnkNmb = atoi(parm->val);
			SS7_DEBUG("Found an nif m2ua-interface-id = %d\n", sng_nif.m2uaLnkNmb);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "mtp2-interface-id")) {
		/**********************************************************************/
			sng_nif.mtp2LnkNmb=atoi(parm->val);

			SS7_DEBUG("Found an nif mtp2-interface-id = %d\n", sng_nif.mtp2LnkNmb);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			SS7_ERROR("Found an invalid parameter %s!\n", parm->var);
			return FTDM_FAIL;
		/**********************************************************************/
		}

		/* move to the next parameter */
		parm = parm + 1;
	/**************************************************************************/
	} /* for (i = 0; i < num_parms; i++) */

	/* fill in the nif interface */
	ftmod_ss7_fill_in_nif_interface(&sng_nif);

	return FTDM_SUCCESS;
}
/******************************************************************************/
static int ftmod_ss7_fill_in_nif_interface(sng_nif_cfg_t *nif_iface)
{
	int	i = nif_iface->id;

	strncpy((char *)g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[i].name, (char *)nif_iface->name, MAX_NAME_LEN-1);

	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[i].id		= nif_iface->id;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[i].m2uaLnkNmb 	= nif_iface->m2uaLnkNmb;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[i].mtp2LnkNmb 	= nif_iface->mtp2LnkNmb;

	sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_NIF_PRESENT);

	return 0;
}

/******************************************************************************/
int ftmod_ss7_parse_m2ua_interfaces(ftdm_conf_node_t *m2ua_interfaces)
{
	ftdm_conf_node_t	*m2ua_interface = NULL;

	/* confirm that we are looking at sng_m2ua_interfaces */
	if (strcasecmp(m2ua_interfaces->name, "sng_m2ua_interfaces")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"m2ua_nif_interfaces\"!\n",m2ua_interfaces->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"m2ua_interfaces\"...\n");
	}

	/* extract the isup_interfaces */
	m2ua_interface = m2ua_interfaces->child;

	while (m2ua_interface != NULL) {
		/* parse the found mtp_route */
		if (ftmod_ss7_parse_m2ua_interface(m2ua_interface)) {
			SS7_ERROR("Failed to parse \"m2ua_interface\"\n");
			return FTDM_FAIL;
		}

		/* go to the next m2ua_interface */
		m2ua_interface = m2ua_interface->next;
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_m2ua_interface(ftdm_conf_node_t *m2ua_interface)
{
	sng_m2ua_cfg_t			sng_m2ua;
	ftdm_conf_parameter_t		*parm = m2ua_interface->parameters;
	int				num_parms = m2ua_interface->n_parameters;
	int				i;

	/* initalize the m2ua intf */
	memset(&sng_m2ua, 0x0, sizeof(sng_m2ua));

	if(!m2ua_interface){
		SS7_ERROR("ftmod_ss7_parse_m2ua_interface: Null XML Node pointer \n");
		return FTDM_FAIL;
	}

	/* confirm that we are looking at an nif_interface */
	if (strcasecmp(m2ua_interface->name, "sng_m2ua_interface")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"m2ua_interface\"!\n",m2ua_interface->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"m2ua_interface\"...\n");
	}


	for (i = 0; i < num_parms; i++) {
	/**************************************************************************/

		/* try to match the parameter to what we expect */
		if (!strcasecmp(parm->var, "name")) {
		/**********************************************************************/
			strcpy((char *)sng_m2ua.name, parm->val);
			SS7_DEBUG("Found an m2ua_interface named = %s\n", sng_m2ua.name);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "id")) {
		/**********************************************************************/
			sng_m2ua.id = atoi(parm->val);
			SS7_DEBUG("Found an m2ua id = %d\n", sng_m2ua.id);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "m2ua-cluster-interface-id")) {
		/**********************************************************************/
			sng_m2ua.clusterId=atoi(parm->val);

			SS7_DEBUG("Found an m2ua cluster_id = %d\n", sng_m2ua.clusterId);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "interface-identifier")) {
		/**********************************************************************/
			sng_m2ua.iid=atoi(parm->val);

			SS7_DEBUG("Found an m2ua interface-identifier = %d\n", sng_m2ua.iid);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			SS7_ERROR("Found an invalid parameter %s!\n", parm->var);
			return FTDM_FAIL;
		/**********************************************************************/
		}

		/* move to the next parameter */
		parm = parm + 1;
	/**************************************************************************/
	} /* for (i = 0; i < num_parms; i++) */

	sng_m2ua.nodeType = SNG_M2UA_NODE_TYPE_SGP;

	/* fill in the nif interface */
	ftmod_ss7_fill_in_m2ua_interface(&sng_m2ua);

	return FTDM_SUCCESS;
}
/******************************************************************************/
static int ftmod_ss7_fill_in_m2ua_interface(sng_m2ua_cfg_t *m2ua_iface)
{
	int	i = m2ua_iface->id;

	strncpy((char *)g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[i].name, (char *)m2ua_iface->name, MAX_NAME_LEN-1);

	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[i].id		= m2ua_iface->id;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[i].nodeType 	= m2ua_iface->nodeType;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[i].clusterId 	= m2ua_iface->clusterId;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[i].iid 		= m2ua_iface->iid;
	sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_M2UA_PRESENT);

	return 0;
}

/******************************************************************************/
int ftmod_ss7_parse_m2ua_peer_interfaces(ftdm_conf_node_t *m2ua_peer_interfaces)
{
	ftdm_conf_node_t	*m2ua_peer_interface = NULL;

	/* confirm that we are looking at m2ua_peer_interfaces */
	if (strcasecmp(m2ua_peer_interfaces->name, "sng_m2ua_peer_interfaces")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"m2ua_peer_interfaces\"!\n",m2ua_peer_interfaces->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"m2ua_peer_interfaces\"...\n");
	}

	/* extract the m2ua_peer_interfaces */
	m2ua_peer_interface = m2ua_peer_interfaces->child;

	while (m2ua_peer_interface != NULL) {
		/* parse the found mtp_route */
		if (ftmod_ss7_parse_m2ua_peer_interface(m2ua_peer_interface)) {
			SS7_ERROR("Failed to parse \"m2ua_peer_interface\"\n");
			return FTDM_FAIL;
		}

		/* go to the next m2ua_peer_interface */
		m2ua_peer_interface = m2ua_peer_interface->next;
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_m2ua_peer_interface(ftdm_conf_node_t *m2ua_peer_interface)
{
	sng_m2ua_peer_cfg_t		sng_m2ua_peer;
	ftdm_conf_parameter_t		*parm = m2ua_peer_interface->parameters;
	int				num_parms = m2ua_peer_interface->n_parameters;
	int				i;

	/* initalize the m2ua intf */
	memset(&sng_m2ua_peer, 0x0, sizeof(sng_m2ua_peer));

	if(!m2ua_peer_interface){
		SS7_ERROR("ftmod_ss7_parse_m2ua_peer_interface: Null XML Node pointer \n");
		return FTDM_FAIL;
	}

	/* confirm that we are looking at an m2ua_peer_interface */
	if (strcasecmp(m2ua_peer_interface->name, "sng_m2ua_peer_interface")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"m2ua_peer_interface\"!\n",m2ua_peer_interface->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"m2ua_peer_interface\"...\n");
	}

	for (i = 0; i < num_parms; i++) {
	/**************************************************************************/

		/* try to match the parameter to what we expect */
		if (!strcasecmp(parm->var, "name")) {
		/**********************************************************************/
			strcpy((char *)sng_m2ua_peer.name, parm->val);
			SS7_DEBUG("Found an sng_m2ua_peer named = %s\n", sng_m2ua_peer.name);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "id")) {
		/**********************************************************************/
			sng_m2ua_peer.id = atoi(parm->val);
			SS7_DEBUG("Found an sng_m2ua_peer id = %d\n", sng_m2ua_peer.id);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "include-asp-identifier")) {
			/**********************************************************************/
			if(!strcasecmp(parm->val, "TRUE")){
				sng_m2ua_peer.aspIdFlag = 0x01;
			} else if(!strcasecmp(parm->val, "FALSE")){
				sng_m2ua_peer.aspIdFlag = 0x00;
			} else {
				SS7_ERROR("Found an invalid aspIdFlag Parameter Value[%s]\n", parm->val);
				return FTDM_FAIL;
			}
			SS7_DEBUG("Found an sng_m2ua_peer aspIdFlag = %d\n", sng_m2ua_peer.aspIdFlag);
			/**********************************************************************/
		} else if (!strcasecmp(parm->var, "asp-identifier")) {
		/**********************************************************************/
			sng_m2ua_peer.selfAspId=atoi(parm->val);

			SS7_DEBUG("Found an sng_m2ua_peer self_asp_id = %d\n", sng_m2ua_peer.selfAspId);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "sctp-interface-id")) {
		/**********************************************************************/
			sng_m2ua_peer.sctpId = atoi(parm->val);

			SS7_DEBUG("Found an sng_m2ua_peer sctp_id = %d\n", sng_m2ua_peer.sctpId);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "destination-port")) {
		/**********************************************************************/
			sng_m2ua_peer.port = atoi(parm->val);

			SS7_DEBUG("Found an sng_m2ua_peer port = %d\n", sng_m2ua_peer.port);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "address")) {
		/**********************************************************************/
			if (sng_m2ua_peer.numDestAddr < SCT_MAX_NET_ADDRS) {
                                sng_m2ua_peer.destAddrList[sng_m2ua_peer.numDestAddr] = iptoul (parm->val);
                                sng_m2ua_peer.numDestAddr++;
                                SS7_DEBUG("sng_m2ua_peer - Parsing  with dest IP Address = %s \n", parm->val);
                        } else {
                                SS7_ERROR("sng_m2ua_peer - too many dest address configured. dropping %s \n", parm->val);
                        }
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "number-of-outgoing-streams")) {
		/**********************************************************************/
			sng_m2ua_peer.locOutStrms=atoi(parm->val);

			SS7_DEBUG("Found an sng_m2ua_peer number-of-outgoing-streams = %d\n", sng_m2ua_peer.locOutStrms);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "init-sctp-association")) {
		/**********************************************************************/
			if(!strcasecmp(parm->val, "TRUE")){
				sng_m2ua_peer.init_sctp_assoc = 0x01;
			} else if(!strcasecmp(parm->val, "FALSE")){
				sng_m2ua_peer.init_sctp_assoc = 0x00;
			} else {
				SS7_ERROR("Found an invalid init_sctp_assoc Parameter Value[%s]\n", parm->val);
				return FTDM_FAIL;
			}

			SS7_DEBUG("Found an sng_m2ua_peer init_sctp_assoc = %d\n", sng_m2ua_peer.init_sctp_assoc);
		/**********************************************************************/
		} else {
		/**********************************************************************/
			SS7_ERROR("Found an invalid parameter %s!\n", parm->var);
			return FTDM_FAIL;
		/**********************************************************************/
		}

		/* move to the next parameter */
		parm = parm + 1;
	/**************************************************************************/
	} /* for (i = 0; i < num_parms; i++) */

	/* fill in the sng_m2ua_peer interface */
	ftmod_ss7_fill_in_m2ua_peer_interface(&sng_m2ua_peer);

	return FTDM_SUCCESS;
}
/******************************************************************************/
static int ftmod_ss7_fill_in_m2ua_peer_interface(sng_m2ua_peer_cfg_t *m2ua_peer_iface)
{
	int     k = 0x00;
	int	i = m2ua_peer_iface->id;

	strncpy((char *)g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].name, (char *)m2ua_peer_iface->name, MAX_NAME_LEN-1);

	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].id		= m2ua_peer_iface->id;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].aspIdFlag 	= m2ua_peer_iface->aspIdFlag;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].selfAspId 	= m2ua_peer_iface->selfAspId;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].locOutStrms 	= m2ua_peer_iface->locOutStrms;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].numDestAddr 	= m2ua_peer_iface->numDestAddr;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].sctpId 		= m2ua_peer_iface->sctpId;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].port 		= m2ua_peer_iface->port;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].init_sctp_assoc 	= m2ua_peer_iface->init_sctp_assoc;
	for (k=0; k<m2ua_peer_iface->numDestAddr; k++) {
		g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[i].destAddrList[k] = m2ua_peer_iface->destAddrList[k];	
	}


	return 0;
}

/******************************************************************************/
int ftmod_ss7_parse_m2ua_clust_interfaces(ftdm_conf_node_t *m2ua_cluster_interfaces)
{
	ftdm_conf_node_t	*m2ua_cluster_interface = NULL;

	/* confirm that we are looking at m2ua_cluster_interfaces */
	if (strcasecmp(m2ua_cluster_interfaces->name, "sng_m2ua_cluster_interfaces")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"m2ua_cluster_interfaces\"!\n",m2ua_cluster_interfaces->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"m2ua_cluster_interfaces\"...\n");
	}

	/* extract the m2ua_cluster_interfaces */
	m2ua_cluster_interface = m2ua_cluster_interfaces->child;

	while (m2ua_cluster_interface != NULL) {
		/* parse the found m2ua_cluster_interface */
		if (ftmod_ss7_parse_m2ua_clust_interface(m2ua_cluster_interface)) {
			SS7_ERROR("Failed to parse \"m2ua_cluster_interface\"\n");
			return FTDM_FAIL;
		}

		/* go to the next m2ua_cluster_interface */
		m2ua_cluster_interface = m2ua_cluster_interface->next;
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_parse_m2ua_clust_interface(ftdm_conf_node_t *m2ua_cluster_interface)
{
	sng_m2ua_cluster_cfg_t		sng_m2ua_cluster;
	ftdm_conf_parameter_t		*parm = m2ua_cluster_interface->parameters;
	int				num_parms = m2ua_cluster_interface->n_parameters;
	int				i;

	/* initalize the m2ua_cluster_interface */
	memset(&sng_m2ua_cluster, 0x0, sizeof(sng_m2ua_cluster));

	if (!m2ua_cluster_interface){
		SS7_ERROR("ftmod_ss7_parse_m2ua_clust_interface - NULL XML Node pointer \n");
		return FTDM_FAIL;
	}

	/* confirm that we are looking at an m2ua_cluster_interface */
	if (strcasecmp(m2ua_cluster_interface->name, "sng_m2ua_cluster_interface")) {
		SS7_ERROR("We're looking at \"%s\"...but we're supposed to be looking at \"m2ua_cluster_interface\"!\n",m2ua_cluster_interface->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("Parsing \"m2ua_cluster_interface\"...\n");
	}


	for (i = 0; i < num_parms; i++) {
	/**************************************************************************/

		/* try to match the parameter to what we expect */
		if (!strcasecmp(parm->var, "name")) {
		/**********************************************************************/
			strcpy((char *)sng_m2ua_cluster.name, parm->val);
			SS7_DEBUG("Found an sng_m2ua_cluster named = %s\n", sng_m2ua_cluster.name);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "id")) {
		/**********************************************************************/
			sng_m2ua_cluster.id = atoi(parm->val);
			SS7_DEBUG("Found an sng_m2ua_cluster id = %d\n", sng_m2ua_cluster.id);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "traffic-mode")) {
			/**********************************************************************/
			if(!strcasecmp(parm->val, "loadshare")){
				sng_m2ua_cluster.trfMode = SNG_M2UA_TRF_MODE_LOADSHARE;
			} else if(!strcasecmp(parm->val, "override")){
				sng_m2ua_cluster.trfMode = SNG_M2UA_TRF_MODE_OVERRIDE;
			} else if(!strcasecmp(parm->val, "broadcast")){
				sng_m2ua_cluster.trfMode = SNG_M2UA_TRF_MODE_BROADCAST;
			} else {
				SS7_ERROR("Found an invalid trfMode Parameter Value[%s]..adding default one[ANY]\n", parm->val);
				sng_m2ua_cluster.trfMode = SNG_M2UA_TRF_MODE_ANY;
			}
			SS7_DEBUG("Found an sng_m2ua_cluster.trfMode  = %d\n", sng_m2ua_cluster.trfMode);
			/**********************************************************************/
		} else if (!strcasecmp(parm->var, "load-share-algorithm")) {
		/**********************************************************************/
			if(!strcasecmp(parm->val, "roundrobin")){
				sng_m2ua_cluster.loadShareAlgo = SNG_M2UA_LOAD_SHARE_ALGO_RR;
			} else if(!strcasecmp(parm->val, "linkspecified")){
				sng_m2ua_cluster.loadShareAlgo = SNG_M2UA_LOAD_SHARE_ALGO_LS;
			} else if(!strcasecmp(parm->val, "customerspecified")){
				sng_m2ua_cluster.loadShareAlgo = SNG_M2UA_LOAD_SHARE_ALGO_CS;
			} else {
				SS7_ERROR("Found an invalid loadShareAlgo Parameter Value[%s]\n", parm->val);
				return FTDM_FAIL;
			}

			SS7_DEBUG("Found an sng_m2ua_cluster.loadShareAlgo = %d\n", sng_m2ua_cluster.loadShareAlgo);
		/**********************************************************************/
		} else if (!strcasecmp(parm->var, "m2ua-peer-interface-id")) {
			/**********************************************************************/
			if(sng_m2ua_cluster.numOfPeers < MW_MAX_NUM_OF_PEER) {
				sng_m2ua_cluster.peerIdLst[sng_m2ua_cluster.numOfPeers] = atoi(parm->val);
				SS7_DEBUG("Found an sng_m2ua_cluster peerId[%d], Total numOfPeers[%d] \n", 
						sng_m2ua_cluster.peerIdLst[sng_m2ua_cluster.numOfPeers],
						sng_m2ua_cluster.numOfPeers+1);
				sng_m2ua_cluster.numOfPeers++;
			}else{
				SS7_ERROR("Peer List excedding max[%d] limit \n", MW_MAX_NUM_OF_PEER);
				return FTDM_FAIL;
			}
			/**********************************************************************/
		} else {
		/**********************************************************************/
			SS7_ERROR("Found an invalid parameter %s!\n", parm->var);
			return FTDM_FAIL;
		/**********************************************************************/
		}

		/* move to the next parameter */
		parm = parm + 1;
	/**************************************************************************/
	} /* for (i = 0; i < num_parms; i++) */

	/* fill in the sng_m2ua_peer interface */
	ftmod_ss7_fill_in_m2ua_clust_interface(&sng_m2ua_cluster);

	return FTDM_SUCCESS;
}
/******************************************************************************/
static int ftmod_ss7_fill_in_m2ua_clust_interface(sng_m2ua_cluster_cfg_t *m2ua_cluster_iface)
{
	int	k = 0x00;
	int	i = m2ua_cluster_iface->id;

	strncpy((char *)g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[i].name, (char *)m2ua_cluster_iface->name, MAX_NAME_LEN-1);

	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[i].id		= m2ua_cluster_iface->id;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[i].trfMode 		= m2ua_cluster_iface->trfMode;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[i].loadShareAlgo 	= m2ua_cluster_iface->loadShareAlgo;
	g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[i].numOfPeers 	= m2ua_cluster_iface->numOfPeers;
	for(k=0;k<m2ua_cluster_iface->numOfPeers;k++){
		g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[i].peerIdLst[k] 	= m2ua_cluster_iface->peerIdLst[k];
	}

	return 0;
}

/******************************************************************************/
int ftmod_ss7_parse_sctp_links(ftdm_conf_node_t *node)
{
	ftdm_conf_node_t	*node_sctp_link = NULL;

	if (!node)
		return FTDM_FAIL;

	if (strcasecmp(node->name, "sng_sctp_interfaces")) {
		SS7_ERROR("SCTP - We're looking at <%s>...but we're supposed to be looking at <sng_sctp_interfaces>!\n", node->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("SCTP - Parsing <sng_sctp_interface> configurations\n");
	}

	for (node_sctp_link = node->child; node_sctp_link != NULL; node_sctp_link = node_sctp_link->next) {
		if (ftmod_ss7_parse_sctp_link(node_sctp_link) != FTDM_SUCCESS) {
			SS7_ERROR("SCTP - Failed to parse <node_sctp_link>. \n");
			return FTDM_FAIL;
		}
	}

	return FTDM_SUCCESS; 
}

/******************************************************************************/
static int ftmod_ss7_parse_sctp_link(ftdm_conf_node_t *node)
{
	ftdm_conf_parameter_t	*param = NULL;
	int					num_params = 0;
	int 					i=0;

	if (!node){
		SS7_ERROR("SCTP - NULL XML Node pointer \n");
		return FTDM_FAIL;
	}

	param = node->parameters;
	num_params = node->n_parameters;
	
	sng_sctp_link_t		t_link;
	memset (&t_link, 0, sizeof(sng_sctp_link_t));
	
	if (strcasecmp(node->name, "sng_sctp_interface")) {
		SS7_ERROR("SCTP - We're looking at <%s>...but we're supposed to be looking at <sng_sctp_interface>!\n", node->name);
		return FTDM_FAIL;
	} else {
		SS7_DEBUG("SCTP - Parsing <sng_sctp_interface> configurations\n");
	}

	for (i=0; i<num_params; i++, param++) {
		if (!strcasecmp(param->var, "name")) {
			int n_strlen = strlen(param->val);
			strncpy((char*)t_link.name, param->val, (n_strlen>MAX_NAME_LEN)?MAX_NAME_LEN:n_strlen);
			SS7_DEBUG("SCTP - Parsing <sng_sctp_interface> with name = %s\n", param->val);
		}
		else if (!strcasecmp(param->var, "id")) {
			t_link.id = atoi(param->val);
			SS7_DEBUG("SCTP - Parsing <sng_sctp_interface> with id = %s\n", param->val);
		}
		else if (!strcasecmp(param->var, "address")) {
			if (t_link.numSrcAddr < SCT_MAX_NET_ADDRS) {
				t_link.srcAddrList[t_link.numSrcAddr+1] = iptoul (param->val);
				t_link.numSrcAddr++;
				SS7_DEBUG("SCTP - Parsing <sng_sctp_interface> with source IP Address = %s\n", param->val);
			} else {
				SS7_ERROR("SCTP - too many source address configured. dropping %s \n", param->val);
			}
		} else if (!strcasecmp(param->var, "source-port")) {
			t_link.port = atoi(param->val);
			SS7_DEBUG("SCTP - Parsing <sng_sctp_interface> with port = %s\n", param->val);
		}
		else {
			SS7_ERROR("SCTP - Found an unknown parameter <%s>. Skipping it.\n", param->var);
		}
	}
	
	g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[t_link.id].id 		= t_link.id;
	g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[t_link.id].port   	= t_link.port;
	strncpy((char*)g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[t_link.id].name, t_link.name, strlen(t_link.name) );
	g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[t_link.id].numSrcAddr = t_link.numSrcAddr;
	for (i=1; i<=t_link.numSrcAddr; i++) {
		g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[t_link.id].srcAddrList[i] = t_link.srcAddrList[i];
	}

	sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_SCTP_PRESENT);
			
	return FTDM_SUCCESS;
}
/******************************************************************************/
