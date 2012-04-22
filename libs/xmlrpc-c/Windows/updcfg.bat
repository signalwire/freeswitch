@echo This will COPY the current config.h, xmlrpc_config.h, transprt_config.h,
@echo version.h, overwriting files in this 'Windows' folder!
@echo ARE YOU SURE YOU WANT TO DO THIS??? Ctrl+C to abort ...
@pause
copy ..\include\xmlrpc-c\config.h win32_config.h
copy ..\xmlrpc_config.h xmlrpc_win32_config.h
copy ..\transport_config.h transport_config_win32.h
@echo all done ...

