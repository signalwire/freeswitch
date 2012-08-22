/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_media_gateway.h"
#include "media_gateway_stack.h"


MgPackage_t mg_pkg_list [] = 
{
   {  /* INDEX : 0 */
      MGT_PKG_GENERIC,        /* Package Id 1 : Generic package */
      1,              /* Version 1 */
      "g",      /* Package name */
   },
   {  /* INDEX : 1 */
      MGT_PKG_ROOT,
      2,              /* Version 1 */
      "root",                 /* Package name */
   },
   {  /* INDEX : 2 */
      MGT_PKG_TONEDET,        /*4*/  
      1,              /* Version 1 */
      "tonedet",              /* Package name */
   },
   {  /* INDEX : 3 */
      MGT_PKG_DTMFDET,        /*6*/  
      1,              /* Version 1 */
      "dd",                   /* Package name */
   },
   {  /* INDEX : 4 */
      MGT_PKG_NETWORK,        /*11*/  
      1,              /* Version 1 */
      "nt",                   /* Package name */
   },
   {  /* INDEX : 5 */
      MGT_PKG_RTP,           /*12*/
      1,              /* Version 1 */
      "rtp",                 /* Package name */
   },
   {  /* INDEX : 6 */
      MGT_PKG_TDM_CKT,        /*13*/  
      1,              /* Version 1 */
      "tdmc",                 /* Package name */
   },
   /* TODO - not sure IF we need this */
   {  /* INDEX : 7 */
      MGT_PKG_SEGMENTATION,    
      1,              /* Version 1 */
      "seg",                  /* Package name */
   },
   {  /* INDEX : 8 */
      MGT_PKG_EN_ALERT,       /*59*/  
      2,              	      /* Version 2 */
      "alert",                /* Package name */
   },
   {  /* INDEX : 9 */
      MGT_PKG_CONTINUITY,        /*60*/  
      2,              /* Version 1 */
      "ct",               /* Package name */
   },
   {  /* INDEX : 10 */
      MGT_PKG_INACTTIMER,     /*69*/  
      1,              /* Version 1 */
      "it",                   /* Package name */
   },
   {  /* INDEX : 11 */
      MGT_PKG_STIMAL,        /* 147 */  
      1,             /* Version 1 */
      "stimal ",             /* Package name */
   },
   {  /* INDEX : 12 */
      MGT_PKG_CALLPROGGEN,      /* 7 */  
      1,              /* Version 1 */
      "cg",                    /* Package name */
   },
   {  /* INDEX : 13 */
      MGT_PKG_GENERIC_ANNC, /* 29 */  
      1,              /* Version 1 */
      "an",                    /* Package name */
   },
   {  /* INDEX : 14 */
      MGT_PKG_XD_CALPG_TNGN,      /* 36 */  
      1,              /* Version 1 */
      "xcg",                    /* Package name */
   },
   {  /* INDEX : 15 */
      MGT_PKG_BSC_SRV_TN,     /* 37 */  
      1,              /* Version 1 */
      "srvtn",                /* Package name */
    },
   {  /* INDEX : 16 */
      MGT_PKG_ETSI_NR,
      1,              /* Version 1 */
      "etsi_nr",              /* Package name */
   },
   {  /* INDEX : 17 */
      MGT_PKG_TONEGEN,
      1,              /* Version 1 */
      "tonegen",              /* Package name */
   },
   {  /* INDEX : 18 */
      MGT_PKG_DTMFGEN,
      1,              /* Version 1 */
      "tonegen",              /* Package name */
   },
   {  /* INDEX : 19 */
      MGT_PKG_CALLPROGGEN,
      1,              /* Version 1 */
      "tonegen",              /* Package name */
   },
   {  /* INDEX : 20 */
      MGT_PKG_CALLPROGDET,
      1,              /* Version 1 */
      "tonedet",              /* Package name */
   },
   {  /* INDEX : 21 */
      MGT_PKG_ANALOG,
      1,              /* Version 1 */
      "analog",              /* Package name */
   },
   {  /* INDEX : 22 */
      MGT_PKG_FAX_TONE_DET,
      1,              /* Version 1 */
      "ftmd",              /* Package name */
   },
   {  /* INDEX : 24 */
      MGT_PKG_CALL_TYP_DISCR,
      1,              /* Version 1 */
      "ctype",              /* Package name */
   },
   {  /* INDEX : 25 */
      MGT_PKG_IP_FAX,
      1,              /* Version 1 */
      "ipfax",              /* Package name */
   },
   {  /* INDEX : 26 */
      MGT_PKG_FAX,
      1,              /* Version 1 */
      "fax",              /* Package name */
   },
  /* Add more packages */
};

/***************************************************************************************/
switch_status_t mg_build_pkg_desc(MgMgcoPkgsDesc* pkg, CmMemListCp  *memCp)
{
	uint16_t i = 0x00;
	uint16_t num_of_pkgs = sizeof(mg_pkg_list)/sizeof(MgPackage_t);

	printf("mg_build_pkg_desc: num_of_pkgs[%d]\n",num_of_pkgs);

	for (i = 0; i < num_of_pkgs; i++) {

		if (mgUtlGrowList((void ***)&pkg->items,
					sizeof(MgMgcoPkgsItem), &pkg->num, memCp) != ROK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Package descriptor Grow List failed\n");
			return SWITCH_STATUS_FALSE;
		}

		pkg->items[pkg->num.val - 1 ]->pres.pres = PRSNT_NODEF;
		pkg->items[pkg->num.val - 1 ]->name.type.pres = PRSNT_NODEF;
		pkg->items[pkg->num.val - 1 ]->name.type.val = MGT_GEN_TYPE_KNOWN;

		pkg->items[pkg->num.val - 1 ]->name.u.val.pres = PRSNT_NODEF;
		pkg->items[pkg->num.val - 1 ]->name.u.val.val = mg_pkg_list[i].package_id;

		pkg->items[pkg->num.val - 1 ]->ver.pres = PRSNT_NODEF;
		pkg->items[pkg->num.val - 1 ]->ver.val = mg_pkg_list[i].version;

		printf("mg_build_pkg_desc: Inserted pkg_id[%d] with version[%d] into pkg list index[%d]\n",mg_pkg_list[i].package_id,mg_pkg_list[i].version,i);
	}

	return SWITCH_STATUS_SUCCESS;
}
/***************************************************************************************/

