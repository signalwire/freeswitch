#include <fspr.h>
#include <fspr_general.h>

int main(int argc, const char * const * argv, const char * const *env)
{
    fspr_app_initialize(&argc, &argv, &env);


    fspr_terminate();
}
