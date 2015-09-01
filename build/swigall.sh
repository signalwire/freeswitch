cd src/mod/languages/mod_lua
make swigclean
make lua_wrap
cd ../../../..

cd src/mod/languages/mod_perl
make swigclean
make mod_perl_wrap.cpp
cd ../../../..

cd src/mod/languages/mod_python
make swigclean
make mod_python_wrap.cpp
cd ../../../..

cd src/mod/languages/mod_java
make reswig
cd ../../../..

cd src/mod/languages/mod_managed
make reswig
cd ../../../..



