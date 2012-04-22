@echo Compile getgateway
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR getgateway.c
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR testgetgateway.c
gcc -o testgetgateway getgateway.o testgetgateway.o -lws2_32

@echo Compile natpmp:
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR getgateway.c
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR natpmp.c
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR natpmpc.c
gcc -c -Wall -Os -DWIN32 wingettimeofday.c
gcc -o natpmpc getgateway.o natpmp.o natpmpc.o wingettimeofday.o -lws2_32

@echo Create natpmp.dll:
gcc -c -Wall -Os -DWIN32 -DENABLE_STRNATPMPERR -DNATPMP_EXPORTS getgateway.c
gcc -c -Wall -Os -DWIN32 -DENABLE_STRNATPMPERR -DNATPMP_EXPORTS natpmp.c
dllwrap -k --driver-name gcc --def natpmp.def --output-def natpmp.dll.def --implib natpmp.lib -o natpmp.dll getgateway.o natpmp.o wingettimeofday.o -lws2_32

