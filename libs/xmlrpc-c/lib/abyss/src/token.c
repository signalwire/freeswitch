#include "xmlrpc-c/abyss.h"

#include "bool.h"

#include "token.h"

void
NextToken(const char ** const pP) {

    bool gotToken;

    gotToken = FALSE;

    while (!gotToken) {
        switch (**pP) {
        case '\t':
        case ' ':
            ++(*pP);
            break;
        default:
            gotToken = TRUE;
        };
    }
}



char *
GetToken(char ** const pP) {

    char * p0;
        
    p0 = *pP;

    while (1) {
        switch (**pP) {
        case '\t':
        case ' ':
        case CR:
        case LF:
        case '\0':
            if (p0 == *pP)
                return NULL;

            if (**pP) {
                **pP = '\0';
                ++(*pP);
            };
            return p0;

        default:
            ++(*pP);
        };
    }
}



void
GetTokenConst(char **       const pP,
              const char ** const tokenP) {

    *tokenP = GetToken(pP);
}
