#include <stdio.h>
#include <stdint.h>

#include <tpl.h>

int main ( int n , char* a [ ] )
{

       tpl_node* tn ;
       char c='a',c2='b',c3,c4;
       int64_t cn = -100, cn2 ;
       uint64_t ucn = 200, ucn2;

       tn = tpl_map ( "A(cIcU)" , &c, &cn, &c2, &ucn ) ;
       tpl_pack ( tn , 1 ) ;
       c += 1;
       cn -= 1;
       c2 += 1;
       ucn += 1;
       tpl_pack ( tn , 1 ) ;
       tpl_dump ( tn , TPL_FILE , "/tmp/test92.tpl" ) ;
       tpl_free ( tn ) ;

       tn = tpl_map ( "A(cIcU)" , &c3, &cn2, &c4, &ucn2 ) ;
       tpl_load(tn,TPL_FILE,"/tmp/test92.tpl");
       /* Hesitant to rely on portability of %lld to print int64_t.
          At least on MinGW it is questionable. */
       /*
        * while (tpl_unpack(tn,1) > 0) {
        *  printf("%c %lld %c %llu\n", c3, cn2, c4, ucn2);
        * }
       */
       tpl_unpack(tn,1);
       if (c3 != 'a' || cn2 != -100 || c4 != 'b' || ucn2 != 200) printf("unpack error 1\n");
       tpl_unpack(tn,1);
       if (c3 != 'b' || cn2 != -101 || c4 != 'c' || ucn2 != 201) printf("unpack error 2\n");
       return ( 0 ) ;

}
