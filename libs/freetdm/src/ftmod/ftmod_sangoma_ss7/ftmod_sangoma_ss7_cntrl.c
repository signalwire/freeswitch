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
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
void handle_sng_log(uint8_t level, char *fmt,...);
void handle_sng_alarm(sng_alrm_t t_alarm);

static void handle_entsi_alarm(sng_alrm_t t_alarm);

int ft_to_sngss7_cfg(void);
int ft_to_sngss7_activate_all(void);

static int ftmod_ss7_general_configuration(void);

static int ftmod_ss7_configure_mtp1_link(int id);

static int ftmod_ss7_configure_mtp2_link(int id);

static int ftmod_ss7_configure_mtp3_link(int id);
static int ftmod_ss7_configure_mtp3_linkset(int id);
static int ftmod_ss7_configure_mtp3_route(int id);
static int ftmod_ss7_configure_mtp3_isup(int id);

static int ftmod_ss7_configure_isup_mtp3(int id);
static int ftmod_ss7_configure_isup_interface(int id);
static int ftmod_ss7_configure_isup_circuit(int id);
static int ftmod_ss7_configure_isup_cc(int id);

static int ftmod_ss7_configure_cc_isup(int id);

/******************************************************************************/

/* FUNCTIONS ******************************************************************/

/* LOGGIGING ******************************************************************/
void handle_sng_log(uint8_t level, char *fmt,...)
{
    char    *data;
    int     ret;
    va_list ap;

    va_start(ap, fmt);
    ret = vasprintf(&data, fmt, ap);
    if (ret == -1) {
        return;
    }

    switch (level) {
    /**************************************************************************/
    case SNG_LOGLEVEL_DEBUG:
        ftdm_log(FTDM_LOG_DEBUG, "sng_ss7->%s", data);
        break;
    /**************************************************************************/
    case SNG_LOGLEVEL_WARN:
        ftdm_log(FTDM_LOG_INFO, "sng_ss7->%s", data);
        break;
    /**************************************************************************/
    case SNG_LOGLEVEL_INFO:
        ftdm_log(FTDM_LOG_INFO, "sng_ss7->%s", data);
        break;
    /**************************************************************************/
    case SNG_LOGLEVEL_STATS:
        ftdm_log(FTDM_LOG_INFO, "sng_ss7->%s", data);
        break;
    /**************************************************************************/
    case SNG_LOGLEVEL_ERROR:
        ftdm_log(FTDM_LOG_ERROR, "sng_ss7->%s", data);
        break;
    /**************************************************************************/
    case SNG_LOGLEVEL_CRIT:
        printf("%s",data);
        /*ftdm_log(FTDM_LOG_CRIT, "sng_ss7->%s", data);*/
        break;
    /**************************************************************************/
    default:
        ftdm_log(FTDM_LOG_INFO, "sng_ss7->%s", data);
        break;
    /**************************************************************************/
    }

    return;
}

/******************************************************************************/
void handle_sng_alarm(sng_alrm_t t_alarm)
{

    switch (t_alarm.entity) {
    /**************************************************************************/
    case (ENTL1):
        ftdm_log(FTDM_LOG_WARNING,"[SNG-MTP1] %s : %s : %s \n",
                                    DECODE_LL1_EVENT(t_alarm.event),
                                    DECODE_LL1_CAUSE(t_alarm.cause),
                                    DECODE_LL1_PARM(t_alarm.eventParm[0]));
        break;
    /**************************************************************************/
    case (ENTSD):
        ftdm_log(FTDM_LOG_WARNING,"[SNG-MTP2] %s : %s \n",
                                    DECODE_LSD_EVENT(t_alarm.event),
                                    DECODE_LSD_CAUSE(t_alarm.cause));
        break;
    /**************************************************************************/
    case (ENTSN):
        ftdm_log(FTDM_LOG_WARNING,"[SNG-MTP3] %s on %d: %s \n",
                                    DECODE_LSN_EVENT(t_alarm.event),
                                    t_alarm.id,
                                    DECODE_LSN_CAUSE(t_alarm.cause));
        break;
    /**************************************************************************/
    case (ENTSI):
        handle_entsi_alarm(t_alarm);
        break;
    /**************************************************************************/
    case (ENTCC):
        ftdm_log(FTDM_LOG_DEBUG,"[SNG-CC] %s : %s \n",
                                    DECODE_LCC_EVENT(t_alarm.event),
                                    DECODE_LCC_CAUSE(t_alarm.cause));
        break;
    /**************************************************************************/
    default:
        ftdm_log(FTDM_LOG_WARNING,"Received alarm from unknown entity");
        break;
    /**************************************************************************/
    } /* switch (t_alarm.entity) */

    return;
}

/******************************************************************************/
static void handle_entsi_alarm(sng_alrm_t alarm)
{

    switch (alarm.event) {
    /**************************************************************************/
    case (LCM_EVENT_TIMEOUT):
        /* this event always has the circuit value embedded */
        SS7_WARN("[ISUP] Timer %d expired on CIC %d\n",
                    (alarm.eventParm[1] > 0 ) ? alarm.eventParm[1] : alarm.eventParm[8],
                    g_ftdm_sngss7_data.cfg.isupCircuit[alarm.eventParm[0]].cic);
        break;
    /**************************************************************************/
    case (LSI_EVENT_REMOTE):
        SS7_WARN("[ISUP] %s received on CIC %d\n",
                    DECODE_LSI_CAUSE(alarm.cause),
                    g_ftdm_sngss7_data.cfg.isupCircuit[alarm.eventParm[0]].cic);
        break;
    /**************************************************************************/
    case (LSI_EVENT_LOCAL):
        SS7_WARN("[ISUP] %s transmitted on CIC %d\n",
                    DECODE_LSI_CAUSE(alarm.cause),
                    g_ftdm_sngss7_data.cfg.isupCircuit[alarm.eventParm[0]].cic);
        break;
    /**************************************************************************/
    case (LSI_EVENT_MTP):
        SS7_WARN("[ISUP] Received %s on %d\n",
                    DECODE_LSI_CAUSE(alarm.cause),
                    g_ftdm_sngss7_data.cfg.mtp3_isup[alarm.eventParm[2]].id);
        break;
    /**************************************************************************/
    case (LCM_EVENT_UI_INV_EVT):
        switch (alarm.cause) {
        /**********************************************************************/
        case (LSI_CAUSE_INV_CIRCUIT):
            SS7_WARN("[ISUP] Invalid circuit = %d (CIC = %d)\n",
                        alarm.eventParm[0],
                        g_ftdm_sngss7_data.cfg.isupCircuit[alarm.eventParm[0]].cic);
            break;
        /**********************************************************************/
        }
        break;
    /**************************************************************************/
    case (LCM_EVENT_LI_INV_EVT):
        switch (alarm.cause) {
        /**********************************************************************/
        case (LCM_CAUSE_INV_SAP):
            SS7_WARN("[ISUP] Invalid spId = %d\n",
                        alarm.eventParm[3]);
            break;
        /**********************************************************************/
        }
        break;
    /**************************************************************************/
    default:
        SS7_WARN("[ISUP] %s : %s \n", DECODE_LSI_EVENT(alarm.event), DECODE_LSI_CAUSE(alarm.cause));
        break;
    /**************************************************************************/
    } /* switch (alarm.event) */
    return;
}

/* ACTIVATION *****************************************************************/
int ft_to_sngss7_activate_all(void)
{
    sng_isup_cc_t       *cc_isup = NULL;
    sng_mtp3_isup_t     *isup_mtp3 = NULL;
    sng_mtp3LinkSet_t   *mtp3_linkset = NULL;
    int                 x;

    /* CC to ISUP *************************************************************/
    x = 1;
    cc_isup = &g_ftdm_sngss7_data.cfg.isup_cc[x];
    while (cc_isup->id != 0) {
        if (sngss7_test_flag(cc_isup, SNGSS7_FLAG_ACTIVE)) {
            SS7_DEBUG("CC-ISUP interface already active = %d\n", cc_isup->id);
        } else {
            if (sng_activate_cc_isup_inf(cc_isup->ccId)) {
                SS7_ERROR("Failed to activate CC-ISUP = %d\n",cc_isup->id);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Started CC-ISUP interface = %d\n", cc_isup->id);
                sngss7_set_flag(cc_isup, SNGSS7_FLAG_ACTIVE);
            }
        } /* if (sngss7_test_flag(cc_isup, SNGSS7_FLAG_ACTIVE) */

        x++;
        cc_isup = &g_ftdm_sngss7_data.cfg.isup_cc[x];
    } /* while (cc_isup->id != 0) */

    /* ISUP - MTP3 ************************************************************/
    x = 1;
    isup_mtp3 = &g_ftdm_sngss7_data.cfg.mtp3_isup[x];
    while (isup_mtp3->id != 0) {
        if (sngss7_test_flag(isup_mtp3, SNGSS7_FLAG_ACTIVE)) {
            SS7_DEBUG("ISUP-MTP3 interface already active = %d\n", isup_mtp3->id);
        } else {
            if (sng_activate_isup_mtp3_inf(isup_mtp3->id)) {
                SS7_ERROR("Failed to activate ISUP-MTP3 = %d\n",isup_mtp3->id);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Started ISUP-MTP3interface = %d\n", isup_mtp3->id);
                sngss7_set_flag(isup_mtp3, SNGSS7_FLAG_ACTIVE);
            }
        } /* if (sngss7_test_flag(isup_mtp3, SNGSS7_FLAG_ACTIVE) */

        x++;
        isup_mtp3 = &g_ftdm_sngss7_data.cfg.mtp3_isup[x];
    } /* while (isup_mtp3->id != 0) */

    /* MTP3 Linkset (MTP3 - MTP2 - MTP1) **************************************/
    x = 1;
    mtp3_linkset = &g_ftdm_sngss7_data.cfg.mtp3LinkSet[x];
    while (mtp3_linkset->id != 0) {
        if (sngss7_test_flag(mtp3_linkset, SNGSS7_FLAG_ACTIVE)) {
            SS7_DEBUG("MTP3 Linkset already active = %s\n", mtp3_linkset->name);
        } else {
            if (sng_activate_mtp3_linkset(mtp3_linkset->id)) {
                SS7_ERROR("Failed to activate MTP3 Linkset = %s\n",mtp3_linkset->name);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Started MTP3 Linkset = %s\n", mtp3_linkset->name);
                sngss7_set_flag(mtp3_linkset, SNGSS7_FLAG_ACTIVE);
            }
        } /* if (sngss7_test_flag(mtp3_linkset, SNGSS7_FLAG_ACTIVE) */

        x++;
        mtp3_linkset = &g_ftdm_sngss7_data.cfg.mtp3LinkSet[x];
    } /* while (mtp3_linkset->id != 0) */

    return FTDM_SUCCESS;
}

/* CONFIGURATION **************************************************************/
int  ft_to_sngss7_cfg(void)
{
    sng_mtp1Link_t      *mtp1_link = NULL;
    sng_mtp2Link_t      *mtp2_link = NULL;
    sng_mtp3Link_t      *mtp3_link = NULL;
    sng_mtp3LinkSet_t   *mtp3_linkset = NULL;
    sng_mtp3Route_t     *mtp3_route = NULL;
    sng_mtp3_isup_t     *mtp3_isup = NULL;
    sng_mtp3_isup_t     *isup_mtp3 = NULL;
    sng_isupInterface_t *isup_interface = NULL;
    sng_isupCircuit_t   *isup_circuit = NULL;
    sng_isup_cc_t       *isup_cc = NULL;
    sng_isup_cc_t       *cc_isup = NULL;
    int x;

    SS7_DEBUG("Starting LibSngSS7 configuration...\n");

    if (g_ftdm_sngss7_data.gen_config_done == 0) {
        /* perform general configuration */
        if(ftmod_ss7_general_configuration()) {
            SS7_ERROR("Failed to run general configuration!\n");
            return FTDM_FAIL;
        } else {
            SS7_INFO("General Configuration was successful\n");
            g_ftdm_sngss7_data.gen_config_done = 1;
        }
    } else {
        SS7_DEBUG("General configuration already done.\n");
    }

    /* MTP1 *******************************************************************/
    x=1;
    mtp1_link = &g_ftdm_sngss7_data.cfg.mtp1Link[x];
    while (mtp1_link->id != 0) {

        if (sngss7_test_flag(mtp1_link, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("MTP1 Link already configured = %s\n",mtp1_link->name);
        } else {
            if (ftmod_ss7_configure_mtp1_link(x)) {
                SS7_ERROR("Failed to configure MTP1 link = %s\n!", mtp1_link->name);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured MTP1 link = %s\n", mtp1_link->name);
                sngss7_set_flag(mtp1_link, SNGSS7_FLAG_CONFIGURED);
            }
        }

        /* next link */
        x++;
        mtp1_link = &g_ftdm_sngss7_data.cfg.mtp1Link[x];
    } /* while (g_ftdm_sngss7_data.cfg.mtp1Link[x]->id != 0) */

    /* MTP2 *******************************************************************/
    x=1;
    mtp2_link = &g_ftdm_sngss7_data.cfg.mtp2Link[x];
    while (mtp2_link->id != 0) {
        if (sngss7_test_flag(mtp2_link, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("MTP2 Link already configured = %s\n",mtp2_link->name);
        } else {
            if (ftmod_ss7_configure_mtp2_link(x)) {
                SS7_ERROR("Failed to configure MTP2 link = %s\n!", mtp2_link->name);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured MTP2 link = %s\n", mtp2_link->name);
                sngss7_set_flag(mtp2_link, SNGSS7_FLAG_CONFIGURED);
            }
        }
        /* next link */
        x++;
        mtp2_link = &g_ftdm_sngss7_data.cfg.mtp2Link[x];
    } /* while (g_ftdm_sngss7_data.cfg.mtp2Link[x]->id != 0) */

    /* MTP3 *******************************************************************/
    x=1;
    mtp3_link = &g_ftdm_sngss7_data.cfg.mtp3Link[x];
    while (mtp3_link->id != 0) {
        if (sngss7_test_flag(mtp3_link, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("MTP3 Link already configured = %s\n", mtp3_link->name);
        } else {
            if (ftmod_ss7_configure_mtp3_link(x)) {
                SS7_ERROR("Failed to configure MTP3 link = %s\n!", mtp3_link->name);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured MTP3 link = %s\n", mtp3_link->name);
                sngss7_set_flag(mtp3_link, SNGSS7_FLAG_CONFIGURED);
            }
        }
        /* next link */
        x++;
        mtp3_link = &g_ftdm_sngss7_data.cfg.mtp3Link[x];
    } /* while (g_ftdm_sngss7_data.cfg.mtp3Link[x]->id != 0) */

    x=1;
    mtp3_linkset = &g_ftdm_sngss7_data.cfg.mtp3LinkSet[x];
    while (mtp3_linkset->id != 0) {
        if (sngss7_test_flag(mtp3_linkset, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("MTP3 LinkSet already configured = %s\n", mtp3_linkset->name);
        } else {
            if (ftmod_ss7_configure_mtp3_linkset(x)) {
                SS7_ERROR("Failed to configure MTP3 link = %s\n!", mtp3_linkset->name);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured MTP3 link = %s\n", mtp3_linkset->name);
                sngss7_set_flag(mtp3_linkset, SNGSS7_FLAG_CONFIGURED);
            }
        }
        /* next link */
        x++;
        mtp3_linkset = &g_ftdm_sngss7_data.cfg.mtp3LinkSet[x];
    } /* while (g_ftdm_sngss7_data.cfg.mtp1Link[x]->id != 0) */

    x=1;
    mtp3_route = &g_ftdm_sngss7_data.cfg.mtp3Route[x];
    while (mtp3_route->id != 0) {
        if (sngss7_test_flag(mtp3_route, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("MTP3 Route already configured = %s\n", mtp3_route->name);
        } else {
            if (ftmod_ss7_configure_mtp3_route(x)) {
                SS7_ERROR("Failed to configure MTP3 route = %s\n!", mtp3_route->name);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured MTP3 route = %s\n", mtp3_route->name);
                sngss7_set_flag(mtp3_route, SNGSS7_FLAG_CONFIGURED);
            }
        }
        /* next link */
        x++;
        mtp3_route = &g_ftdm_sngss7_data.cfg.mtp3Route[x];
    } /* while (g_ftdm_sngss7_data.cfg.mtp3Route[x]->id != 0) */

    mtp3_route = &g_ftdm_sngss7_data.cfg.mtp3Route[0];
    if (sngss7_test_flag(mtp3_route, SNGSS7_FLAG_CONFIGURED)) {
        SS7_DEBUG("MTP3 Self Route already configured\n");
    } else {
        if (ftmod_ss7_configure_mtp3_route(0)) {
            SS7_ERROR("Failed to configure MTP3 Route = SelfRoute\n!");
            return FTDM_FAIL;
        } else {
            SS7_INFO("Successfully configured MTP3 Route = SelfRoute\n");
            sngss7_set_flag(mtp3_route, SNGSS7_FLAG_CONFIGURED);
        }
    }

    x=1;
    mtp3_isup = &g_ftdm_sngss7_data.cfg.mtp3_isup[x];
    while (mtp3_isup->id != 0) {
        if (sngss7_test_flag(mtp3_isup, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("MTP3-ISUP interface already configured = %d\n", mtp3_isup->id);
        } else {
            if (ftmod_ss7_configure_mtp3_isup(x)) {
                SS7_ERROR("Failed to configure MTP3-ISUP interface = %d\n!", mtp3_isup->id);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured MTP3-ISUP interface = %d\n", mtp3_isup->id);
            }
        }
        /* next link */
        x++;
        mtp3_isup = &g_ftdm_sngss7_data.cfg.mtp3_isup[x];
    } /* while (g_ftdm_sngss7_data.cfg.mtp3_isup[x]->id != 0) */

    /* ISUP *******************************************************************/
    x=1;
    isup_mtp3 = &g_ftdm_sngss7_data.cfg.mtp3_isup[x];
    while (isup_mtp3->id != 0) {
        if (sngss7_test_flag(isup_mtp3, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("ISUP-MTP3 interface already configured = %d\n", isup_mtp3->id);
        } else {
            if (ftmod_ss7_configure_isup_mtp3(x)) {
                SS7_ERROR("Failed to configure ISUP-MTP3 interface = %d\n!", isup_mtp3->id);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured ISUP-MTP3 interface = %d\n", isup_mtp3->id);
                sngss7_set_flag(isup_mtp3, SNGSS7_FLAG_CONFIGURED);
            }
        }
        /* next link */
        x++;
        isup_mtp3 = &g_ftdm_sngss7_data.cfg.mtp3_isup[x];
    } /* while (g_ftdm_sngss7_data.cfg.isup_mtp3[x]->id != 0) */

    x=1;
    isup_cc = &g_ftdm_sngss7_data.cfg.isup_cc[x];
    while (isup_cc->id != 0) {
        if (sngss7_test_flag(isup_cc, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("ISUP-CC interface already configured = %d\n", isup_cc->id);
        } else {
            if (ftmod_ss7_configure_isup_cc(x)) {
                SS7_ERROR("Failed to configure ISUP-CC interface = %d\n!", isup_cc->id);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured ISUP-CC interface = %d\n", isup_cc->id);
            }
        }
        /* next link */
        x++;
        isup_cc = &g_ftdm_sngss7_data.cfg.isup_cc[x];
    } /* while (g_ftdm_sngss7_data.cfg.isup_cc[x]->id != 0) */

    x=1;
    isup_interface = &g_ftdm_sngss7_data.cfg.isupInterface[x];
    while (isup_interface->id != 0) {
        if (sngss7_test_flag(isup_interface, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("ISUP interface already configured = %s\n", isup_interface->name);
        } else {
            if (ftmod_ss7_configure_isup_interface(x)) {
                SS7_ERROR("Failed to configure ISUP interface = %s\n!", isup_interface->name);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured ISUP interface = %s\n", isup_interface->name);
                sngss7_set_flag(isup_interface, SNGSS7_FLAG_CONFIGURED);
            }
        }
        /* next link */
        x++;
        isup_interface = &g_ftdm_sngss7_data.cfg.isupInterface[x];
    } /* while (g_ftdm_sngss7_data.cfg.isup_interface[x]->id != 0) */

    x=1;
    isup_circuit = &g_ftdm_sngss7_data.cfg.isupCircuit[x];
    while (isup_circuit->id != 0) {
        if (isup_circuit->cic != 0) {
            if (sngss7_test_flag(isup_circuit, SNGSS7_FLAG_CONFIGURED)) {
                SS7_DEBUG("ISUP Circuit already configured = %d\n", isup_circuit->id);
            } else {
                if (ftmod_ss7_configure_isup_circuit(x)) {
                    SS7_ERROR("Failed to configure ISUP circuit = %d\n!", isup_circuit->id);
                    return FTDM_FAIL;
                } else {
                    SS7_INFO("Successfully configured ISUP circuit = %d\n", isup_circuit->id);
                    sngss7_set_flag(isup_circuit, SNGSS7_FLAG_CONFIGURED);
                }
            }
        }
        /* next link */
        x++;
        isup_circuit = &g_ftdm_sngss7_data.cfg.isupCircuit[x];
    } /* while (g_ftdm_sngss7_data.cfg.isup_circuit[x]->id != 0) */

    /* CC *********************************************************************/

    x=1;
    cc_isup = &g_ftdm_sngss7_data.cfg.isup_cc[x];
    while (cc_isup->id != 0) {
        if (sngss7_test_flag(cc_isup, SNGSS7_FLAG_CONFIGURED)) {
            SS7_DEBUG("CC-ISUP interface already configured = %d\n", cc_isup->id);
        } else {
            if (ftmod_ss7_configure_cc_isup(x)) {
                SS7_ERROR("Failed to configure CC-ISUP interface = %d\n!", cc_isup->id);
                return FTDM_FAIL;
            } else {
                SS7_INFO("Successfully configured CC-ISUP interface = %d\n", cc_isup->id);
                sngss7_set_flag(cc_isup, SNGSS7_FLAG_CONFIGURED);
            }
        }
        /* next link */
        x++;
        cc_isup = &g_ftdm_sngss7_data.cfg.isup_cc[x];
    } /* while (g_ftdm_sngss7_data.cfg.cc_isup[x]->id != 0) */
    
    SS7_DEBUG("Finished LibSngSS7 configuration...\n");
    return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_general_configuration(void)
{

    if(sng_cfg_mtp1_gen(&g_ftdm_sngss7_data.cfg)) {
        SS7_ERROR("General configuration for MTP1 failed!\n");
        return FTDM_FAIL;
    } else {
        SS7_INFO("General configuration for MTP1 was successful\n");
    }

    if(sng_cfg_mtp2_gen(&g_ftdm_sngss7_data.cfg)) {
        SS7_ERROR("General configuration for MTP2 failed!\n");
        return FTDM_FAIL;
    } else {
        SS7_INFO("General configuration for MTP2 was successful\n");
    }

    if(sng_cfg_mtp3_gen(&g_ftdm_sngss7_data.cfg)) {
        SS7_ERROR("General configuration for MTP3 failed!\n");
        return FTDM_FAIL;
    } else {
        SS7_INFO("General configuration for MTP3 was successful\n");
    }

    if(sng_cfg_isup_gen(&g_ftdm_sngss7_data.cfg)) {
        SS7_ERROR("General configuration for ISUP failed!\n");
        return FTDM_FAIL;
    } else {
        SS7_INFO("General configuration for ISUP was successful\n");
    }

    if(sng_cfg_cc_gen(&g_ftdm_sngss7_data.cfg)) {
        SS7_ERROR("General configuration for Call-Control failed!\n");
        return FTDM_FAIL;
    } else {
        SS7_INFO("General configuration for Call-Control was successful\n");
    }

    return FTDM_SUCCESS;
}

/******************************************************************************/
static int ftmod_ss7_configure_mtp1_link(int id)
{
    if(sng_cfg_mtp1_link(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_mtp2_link(int id)
{
    if(sng_cfg_mtp2_link(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_mtp3_link(int id)
{
    if(sng_cfg_mtp3_link(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_mtp3_linkset(int id)
{
    if(sng_cfg_mtp3_linkset(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_mtp3_route(int id)
{
    if(sng_cfg_mtp3_route(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_mtp3_isup(int id)
{
    if(sng_cfg_mtp3_isup_interface(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_isup_mtp3(int id)
{
    if(sng_cfg_isup_mtp3_interface(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_isup_interface(int id)
{
    if(sng_cfg_isup_interface(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_isup_circuit(int id)
{
    if(sng_cfg_isup_circuit(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_isup_cc(int id)
{
    if(sng_cfg_isup_cc_interface(&g_ftdm_sngss7_data.cfg, id)){
        return FTDM_FAIL;
    } else {
        return FTDM_SUCCESS;
    }
}

/******************************************************************************/
static int ftmod_ss7_configure_cc_isup(int id)
{
    if(sng_cfg_cc_isup_interface(&g_ftdm_sngss7_data.cfg, id)){
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

