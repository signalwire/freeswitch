#ifndef ASSERTX_HPP_INCLUDED
#define ASSERTX_HPP_INCLUDED

#include <cassert>

/* The compiler often warns you if you give a function formal parameter a
   name, but don't use it.  But because assert() disappears when doing an
   optimized build, the compiler doesn't recognize your reference to the
   parameter in the assert() argument.  To avoid the bogus warning in
   this case, we have ASSERT_ONLY_ARG(), which declares a name for a
   formal parameter for purposes of assert() only.  In cases where an
   assert() would disappear, ASSERT_ONLY_ARG() disappears too.

   E.g.

   void foo(int const ASSERT_ONLY_ARG(arg1)) {

       assert(arg1 > 0);
   }
*/
#ifdef NDEBUG
  #define ASSERT_ONLY_ARG(x)
#else
  #define ASSERT_ONLY_ARG(x) x
#endif

#endif
