#ifndef CMDLINE_PARSER_H
#define CMDLINE_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

#include "int.h"

/*

   NOTE NOTE NOTE: cmd_getOptionValueString() and
   cmd_getArgument() return malloc'ed memory (and abort the program if
   out of memory).  You must free it.

*/

enum optiontype {
    OPTTYPE_FLAG,
    OPTTYPE_INT,
    OPTTYPE_UINT,
    OPTTYPE_STRING,
    OPTTYPE_BINUINT,
    OPTTYPE_FLOAT
};

struct cmdlineParserCtl;

typedef struct cmdlineParserCtl * cmdlineParser;

void
cmd_processOptions(cmdlineParser const cpP,
                   int           const argc,
                   const char ** const argv, 
                   const char ** const errorP);

cmdlineParser
cmd_createOptionParser(void);

void
cmd_destroyOptionParser(cmdlineParser const cpP);

void
cmd_defineOption(cmdlineParser   const cpP,
                 const char *    const name, 
                 enum optiontype const type);
    
int
cmd_optionIsPresent(cmdlineParser const cpP,
                    const char *  const name);

int
cmd_getOptionValueInt(cmdlineParser const cpP,
                      const char *  const name);

unsigned int
cmd_getOptionValueUint(cmdlineParser const cpP,
                       const char *  const name);

const char *
cmd_getOptionValueString(cmdlineParser const cpP,
                         const char *  const name);

uint64_t
cmd_getOptionValueBinUint(cmdlineParser const cpP,
                          const char *  const name);

double
cmd_getOptionValueFloat(cmdlineParser const cpP,
                        const char *  const name);

unsigned int 
cmd_argumentCount(cmdlineParser const cpP);

const char * 
cmd_getArgument(cmdlineParser const cpP, 
                unsigned int  const argNumber); 

#ifdef __cplusplus
}
#endif

#endif
