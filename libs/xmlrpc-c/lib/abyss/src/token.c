#include "xmlrpc-c/abyss.h"

#include "token.h"

void
NextToken(char ** const p) {

    abyss_bool gotToken;

    gotToken = FALSE;

    while (!gotToken) {
        switch (**p) {
        case '\t':
        case ' ':
            ++(*p);
            break;
        default:
            gotToken = TRUE;
        };
    }
}



char *
GetToken(char ** const p) {

    char * p0;
        
    p0 = *p;

    while (1) {
        switch (**p) {
        case '\t':
        case ' ':
        case CR:
        case LF:
        case '\0':
            if (p0 == *p)
                return NULL;

            if (**p) {
                **p = '\0';
                ++(*p);
            };
            return p0;

        default:
            ++(*p);
        };
    }
}
