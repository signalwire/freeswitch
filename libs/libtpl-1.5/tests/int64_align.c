#include <stdio.h>
#include <inttypes.h>

/* try compiling this with -m32 vs -m64
 * 
 * with mac os x and gcc, 
 * on -m32 the int64_t gets aligned at +4
 * on -m64 the int64_t gets aligned at +8
 */

static const struct s_t {
        int i;
        int64_t j;
} s;

int main() {
        if ((long)&s.j % 8 != 0) printf("non-aligned int64\n");
        else printf("aligned int64\n");
}
