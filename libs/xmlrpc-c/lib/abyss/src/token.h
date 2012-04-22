#ifndef ABYSS_TOKEN_H_INCLUDED
#define ABYSS_TOKEN_H_INCLUDED

void
NextToken(const char ** const pP);

char *
GetToken(char ** const pP);

void
GetTokenConst(char **       const pP,
              const char ** const tokenP);

#endif
