#include <stdio.h>

/* try compiling this with and without aligned doubles 
 * 
 * cc -malign-double -o malign malign.c
 * cc -mno-align-double -o malign malign.c
 * 
 * on x86, double is not normally aligned (unless -malign-double is used).
 * but on Sparc, or x86-64, double is aligned.
 */

static const struct s_t {
        char a;
        double d;
} s;

int main() {
        if ((long)&s.d % 8 != 0) printf("-mno-align-double\n");
        else printf("-malign-double\n");
}
