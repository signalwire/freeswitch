/***************************************************************************** 
 * 
 *  MODULE NAME : GETOPT.H 
 * 
 *  COPYRIGHTS: 
 *             This module contains code made available by IBM 
 *             Corporation on an AS IS basis.  Any one receiving the 
 *             module is considered to be licensed under IBM copyrights 
 *             to use the IBM-provided source code in any way he or she 
 *             deems fit, including copying it, compiling it, modifying 
 *             it, and redistributing it, with or without 
 *             modifications.  No license under any IBM patents or 
 *             patent applications is to be implied from this copyright 
 *             license. 
 * 
 *             A user of the module should understand that IBM cannot 
 *             provide technical support for the module and will not be 
 *             responsible for any consequences of use of the program. 
 * 
 *             Any notices, including this one, are not to be removed 
 *             from the module without the prior written consent of 
 *             IBM. 
 * 
 *  AUTHOR:   Original author: 
 *                 G. R. Blair (BOBBLAIR at AUSVM1) 
 *                 Internet: bobblair@bobblair.austin.ibm.com 
 * 
 *            Extensively revised by: 
 *                 John Q. Walker II, Ph.D. (JOHHQ at RALVM6) 
 *                 Internet: johnq@ralvm6.vnet.ibm.com 
 * 
 *****************************************************************************/ 
#ifndef WIN32_GETOPT_H
#define WIN32_GETOPT_H

#ifdef __cplusplus
extern "C" {
#endif

extern char * optarg; 
extern int    optind; 
 
int getopt ( int argc, char **argv, char *optstring); 

#ifdef __cplusplus
}
#endif
 
#endif // WIN32_GETOPT_H
