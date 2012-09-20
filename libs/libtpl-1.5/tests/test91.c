#include <stdio.h>
#include <stdint.h>

#include <tpl.h>

int main ( int n , char* a [ ] )
{

       tpl_node* tn ;
       uint64_t cn = 100, cn2 ;

       tn = tpl_map ( "U" , &cn ) ;
       tpl_pack ( tn , 0 ) ;
       tpl_dump ( tn , TPL_FILE , "/tmp/test91.tpl" ) ;
       tpl_free ( tn ) ;

       tn = tpl_map ( "U" , &cn2 ) ;
       tpl_load ( tn , TPL_FILE , "/tmp/test91.tpl" ) ;
       tpl_unpack ( tn , 0 ) ;
       printf("cn is %sequal to cn2\n", (cn == cn2) ? "" : "not");
       tpl_free ( tn ) ;
       return ( 0 ) ;
}
