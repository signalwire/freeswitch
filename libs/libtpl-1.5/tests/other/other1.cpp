#include <stdio.h>
#include <string.h>
#include "tpl.h"
main() {
void *buffer;
size_t bsize;
	
struct ci {
    int i;
    char c[30];
	
	void pack( void **buffer, size_t *size )
	{
		tpl_node *tn = tpl_map("S(ic#)", this, 30);  /* pass structure address */
		tpl_pack(tn, 0);
		tpl_dump(tn, TPL_MEM, buffer, size);
		tpl_free(tn);
	}
	
	void unpack( void *buffer, size_t size )
	{
		tpl_node *tn = tpl_map("S(ic#)", this, 30);
		tpl_load(tn, TPL_MEM, buffer, size);
		tpl_unpack( tn, 0 );
		tpl_free(tn);
	}
};

struct ci s = {9999, "this is a test string."};

s.pack(&buffer, &bsize);
printf("buffer: %s\n", (char *)buffer);

struct ci b = { -1, "" };

b.unpack(buffer, bsize);

printf("i: %d\n", b.i);
printf("c: %s\n", b.c);
}
