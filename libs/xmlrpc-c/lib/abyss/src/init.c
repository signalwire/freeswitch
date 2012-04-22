/*=============================================================================
  The functions handle static stuff -- the functions do not operate on
  any particular Abyss object, but rather on the global environment of
  the program that uses the Abyss library.  abyssInit() is essentially
  and extension of the program loader.

  The intent is for any program that uses the Abyss library to call
  AbyssInit() just as it starts up, and AbyssTerm() just as it exits.
  It should do both when the program is only one thread, because they
  are not thread-safe.

  These functions do the bare minimum they can get away with; we prefer
  context to be local to invidual objects.
=============================================================================*/

#include <assert.h>

#include "xmlrpc-c/string_int.h"

#include "chanswitch.h"
#include "channel.h"

#include "xmlrpc-c/abyss.h"


static unsigned int AbyssInitCount = 0;

static void
initAbyss(const char ** const errorP) {

    const char * error;

    DateInit();
    MIMETypeInit();

    ChanSwitchInit(&error);

    if (error) {
        xmlrpc_asprintf(errorP,
                        "Could not initialize channel swtich class.  %s",
                        error);
            
        xmlrpc_strfree(error);
    } else {
        const char * error;
        ChannelInit(&error);

        if (error) {
            xmlrpc_asprintf(errorP, "Could not initialize Channel class.  %s",
                            error);
                
            xmlrpc_strfree(error);
        } else {
            AbyssInitCount = 1;
            *errorP = NULL;
                
            if (*errorP)
                ChannelTerm();
        }
        if (*errorP)
            ChanSwitchTerm();
    }
}



void
AbyssInit(const char ** const errorP) {

    if (AbyssInitCount > 0) {
        *errorP = NULL;
        ++AbyssInitCount;
    } else {
        initAbyss(errorP);
        if (!*errorP)
            AbyssInitCount = 1;
    }
}



static void
termAbyss(void) {

    ChannelTerm();
    ChanSwitchTerm();
    MIMETypeTerm();
}



void
AbyssTerm(void) {

    assert(AbyssInitCount > 0);

    --AbyssInitCount;
    
    if (AbyssInitCount == 0)
        termAbyss();
}
