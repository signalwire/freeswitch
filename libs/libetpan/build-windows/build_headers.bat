@mkdir include
@mkdir include\libetpan
@for /F "eol=" %%i in (build_headers.list) do @copy "%%i" include\libetpan
@echo "done" >_headers_depends