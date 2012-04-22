#ifndef STRING_PARSER_H_INCLUDED
#define STRING_PARSER_H_INCLUDED

#include "int.h"

void
interpretUll(const char *  const string,
             uint64_t *    const ullP,
             const char ** const errorP);

void
interpretLl(const char *  const string,
            int64_t *     const llP,
            const char ** const errorP);

void
interpretUint(const char *   const string,
              unsigned int * const uintP,
              const char **  const errorP);

void
interpretInt(const char *  const string,
             int *         const uintP,
             const char ** const errorP);

void
interpretBinUint(const char *  const string,
                 uint64_t *    const valueP,
                 const char ** const errorP);

#endif
