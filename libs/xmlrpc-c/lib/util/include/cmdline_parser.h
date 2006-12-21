#ifndef CMDLINE_PARSER_H
#define CMDLINE_PARSER_H


/*

   NOTE NOTE NOTE: cmd_getOptionValueString() and
   cmd_getArgument() return malloc'ed memory (and abort the program if
   out of memory).  You must free it.

*/

enum optiontype {OPTTYPE_FLAG, OPTTYPE_INT, OPTTYPE_UINT, OPTTYPE_STRING};

struct cmdlineParserCtl;

typedef struct cmdlineParserCtl * cmdlineParser;

void
cmd_processOptions(cmdlineParser   const cpP,
                   int             const argc,
                   const char **   const argv, 
                   const char **   const errorP);

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

unsigned int
cmd_getOptionValueUint(cmdlineParser const cpP,
                       const char *  const name);

int
cmd_getOptionValueInt(cmdlineParser const cpP,
                      const char *  const name);

const char *
cmd_getOptionValueString(cmdlineParser const cpP,
                         const char *  const name);

unsigned int 
cmd_argumentCount(cmdlineParser const cpP);

const char * 
cmd_getArgument(cmdlineParser const cpP, 
                unsigned int  const argNumber); 

#endif
