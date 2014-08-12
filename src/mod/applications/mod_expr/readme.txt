ExprEval - A C/C++ based expression evaluation library
Written by: Brian Allen Vanderburg II
Licensed under the ExprEval License
------------------------------------------------------
ExprEval is a mostly a C based expression evaluation
library.  The only C++ part is the C++ Wrapper which
encapsulates the complexity of the library usage.

ExprEval supports the parsing of multiple expressions
in a single expression string.  Each sub-expression
must end with a semicolon.  It also supports the use
of variables, constants, and functions.  Functions
can take multiple arguments.  These arguments can
also be expressions.

ExprEval is very fast.  It first parses the expression
string into a tree of actions to take.  After it has
been parsed, an expression can be evaluated many times
over an over.

Functions, variables, and constants are stored in
their own seperate lists.  This makes is where the
lists can be shared among multiple expression objects.
A function list can add all the functions needed, and
then be added to each expression object, instead of
added each needed function to each object.  The same
goes for constant lists.  Variable lists make it where
one expression can depend on a variable set in another.


Saturday, July 1, 2006
----------------------
Version 2.6

* Added a new value list function 'exprValListGetNext' that can be used to
  enumerate the items in a value list.  Any of the items not needed (name,
  value, or address) can be NULL.  For example:
  
  char *name;
  EXPRTYPE val;
  void *cookie;
  
  cookie = exprValListGetNext(vlist, &name, &value, NULL, NULL);
  while(cookie)
    {
    /* Do something with name and value */
    cookie = exprValListGetNext(vlist, &name, &value, NULL, cookie);
    }
    
  You must make sure not to actually edit the returned name, because it is a
  pointer into the value list to the name.  This can also be used to have one
  value list store globals.  Global variables can be added to a value list, then
  additional lists can be created, and before any variables are added
  or the expression is parsed, the global list can be enumerated for name and
  address and the exprValListAddAddress can be used to add them.  This way,
  expressions can have their own private list, but some variables may be shared
  on each expression through the global list.  This is useful especially if the
  globals are not known at compile time, but can be adjusted by the user.
  For example:
  
  exprValList *globals;
  exprValList *v1;
  exprValList *v2;
  char *name;
  EXPRTYPE *addr;
  void *cookie;
  
  exprValListCreate(&globals);
  /* Add variables to the list, perhaps read from a user file or something */
  
  exprValListCreate(&v1);
  cookie = exprValListGetNext(globals, &name, NULL, &addr, NULL);
  while(cookie)
    {
    exprValListAddAddress(v1, name, addr);
    cookie = exprValListGetNext(globals, &name, NULL, &addr, cookie);
    }
  

Friday, June 30, 2006
---------------------
Version 2.5

* Added a new value list function 'exprValListAddAddress'.  This function adds
  a named value to the list, but uses the addresss of a stack variable.  The
  stack variable is then used to set/get the value instead of the internal list
  value.  You must ensure that the stack variable exists as long as it is used
  by the expression.  This can permit, for example, a value name to be shared
  with two different value lists like such:
  
  EXPRTYPE global_value;
  exprValListAddAddress(vlist, "global", &global_value);
  exprValListAddAddress(vlist2, "global", &global_value);
  
  Like this, the value can be directly accessed by the application, and each
  value list will share it.  This can also be used to replace code from this:
  
  EXPRTYPE *a;
  exprValListAdd(vlist, "var", 0.0);
  exprValListGetAddress(vlist, "var", &a);
  
  To look like this:
  
  EXPRTYPE a;
  exprValListAddAddress(vlist, "var", &a);
* Added a value list function exprValListSet to set the value of a variable
  (using the slow search method).  This is because the add functions now return
  and error if the item (function/value) already exists instead of setting the
  value of the item.  You can still use the fast direct access method.
* Changed internal lists for function and value lists from binary trees to
  linked lists.
  
  


Thursday, May 4, 2006
---------------------
Version 2.0

* All internal functions are evaluated directly in the exprEvalNode call.
  This gives some speed increase.
* Removed parameter and reference count macros as well as functin creation
  macro.  Parameter and reference count information can be set when adding
  a function solver.
* Removed exprMsgFuncType, since it is unused by the library.
* Changed much of the internal names from one-letter variable names to
  more meaningful names.

Thursday, December 1, 2005
--------------------------
Version 1.8

* Added support for the ^ operator to raise to a power.
  The pow function can still be used.
* Moved basic math code (add,subtract,multiply,divide,negate,exponent)
  and multiple expression support from function solvers to the exprEvalNode
  function.

Tuesday, November 22, 2005
--------------------------
I still haven't been keeping up with history much.

* Removed < and > as comments.  Instead use # as a 
  comment to the end of the line
* Added function exprGetErrorPosition to get start and
  end position of parse error.

Monday, May 3, 2004:  Version 1.0
---------------------------------
This is a pretty bad time to start the history part since
ExprEval is pretty much up and running and very operational.

* Added macro EXPR_MAJORVERSION
* Added macro EXPR_MINORVERSION
* Added function exprGetVersion
* Added macro to make declaring functions easy:
  EXPR_FUNCTIONSOLVER(func_name)
* Added support for passing variable references to functions
  with the ampersand.  Example: minmax(1,2,3,&min,&max)
* Added macros for reference support:
  EXPR_REQUIREREFCOUNT
  EXPR_REQUIREREFCOUNTMIN
  EXPR_REQUIREREFCOUNTMAX
  EXPR_REQUIREREFCOUNTRANGE
* Added feature to disable assigning to a variable with the
  same name as a constant.
* Added feature to enable applications to change the value of
  a constant while the expression can not.  You must add
  any constants to the constant list BEFORE you parse the
  expression.
