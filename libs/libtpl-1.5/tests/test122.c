#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tpl.h"

const char *filename = "/tmp/test122.tpl";

typedef struct {
  char c;
  int i;
} inner_t;

typedef struct {
    int i;
    char c[3];
    double f;
    inner_t inner;
} outer;

int main() {
    tpl_node *tn;
    outer ms = {1, {'a','b','c'}, 3.14, {'a',1}};
    outer os;

    tn = tpl_map( "S(ic#f$(ci))", &ms, 3);
    tpl_pack( tn, 0 );
    tpl_dump( tn, TPL_FILE, filename );
    tpl_free( tn );

    memset(&os, 0, sizeof(outer));
    tn = tpl_map( "S(ic#f$(ci))", &os, 3);
    tpl_load( tn, TPL_FILE, filename );
    tpl_unpack( tn, 0 );
    tpl_free( tn );

    printf("%d %c%c%c %f %c %d\n", os.i, os.c[0],os.c[1],os.c[2],os.f,
      os.inner.c, os.inner.i);

    return(0);
}
