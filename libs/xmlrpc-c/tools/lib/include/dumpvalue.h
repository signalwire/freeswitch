#ifndef DUMPVALUE_H_INCLUDED
#define DUMPVALUE_H_INCLUDED

struct _xmlrpc_value;

void
dumpValue(const char *           const prefix,
          struct _xmlrpc_value * const valueP);

#endif
