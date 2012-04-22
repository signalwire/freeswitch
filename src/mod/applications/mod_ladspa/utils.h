/* utils.h

   Free software by Richard W.E. Furse. Do with as you will. No
   warranty. */

#ifndef LADSPA_SDK_LOAD_PLUGIN_LIB
#define LADSPA_SDK_LOAD_PLUGIN_LIB

/*****************************************************************************/

#include "ladspa.h"

/*****************************************************************************/

/* Functions in load.c: */

/* This function call takes a plugin library filename, searches for
   the library along the LADSPA_PATH, loads it with dlopen() and
   returns a plugin handle for use with findPluginDescriptor() or
   unloadLADSPAPluginLibrary(). Errors are handled by writing a
   message to stderr and calling exit(1). It is alright (although
   inefficient) to call this more than once for the same file. */
void * loadLADSPAPluginLibrary(const char * pcPluginFilename);

/* This function unloads a LADSPA plugin library. */
void unloadLADSPAPluginLibrary(void * pvLADSPAPluginLibrary);

/* This function locates a LADSPA plugin within a plugin library
   loaded with loadLADSPAPluginLibrary(). Errors are handled by
   writing a message to stderr and calling exit(1). Note that the
   plugin library filename is only included to help provide
   informative error messages. */
const LADSPA_Descriptor *
findLADSPAPluginDescriptor(void * pvLADSPAPluginLibrary,
			   const char * pcPluginLibraryFilename,
			   const char * pcPluginLabel);

/*****************************************************************************/

/* Functions in search.c: */

/* Callback function for use with LADSPAPluginSearch(). The callback
   function passes the filename (full path), a plugin handle (dlopen()
   style) and a LADSPA_DescriptorFunction (from which
   LADSPA_Descriptors can be acquired). */
typedef void LADSPAPluginSearchCallbackFunction
(const char * pcFullFilename, 
 void * pvPluginHandle,
 LADSPA_Descriptor_Function fDescriptorFunction);

/* Search through the $(LADSPA_PATH) (or a default path) for any
   LADSPA plugin libraries. Each plugin library is tested using
   dlopen() and dlsym(,"ladspa_descriptor"). After loading each
   library, the callback function is called to process it. This
   function leaves items passed to the callback function open. */
void LADSPAPluginSearch(LADSPAPluginSearchCallbackFunction fCallbackFunction);

/*****************************************************************************/

/* Function in default.c: */

/* Find the default value for a port. Return 0 if a default is found
   and -1 if not. */
int getLADSPADefault(const LADSPA_PortRangeHint * psPortRangeHint,
		     const unsigned long          lSampleRate,
		     LADSPA_Data                * pfResult);

/*****************************************************************************/

#endif

/* EOF */
