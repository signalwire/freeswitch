BFCP server and client libraries
================================

This is a brief guide to the compilation of the BFCP (Binary Floor Control Protocol) libraries implementing the behavior layer for both servers and clients, according to the RFC specifications, and of the sample applications that have been made available to test the libraries.

The main files that are both available for the BFCP participant and BFCP server are main.c, a small list of operations to test all the options of the BFCP libraries.

## Compiling the libraries

Before compiling these libraries, you'll need the BFCP messages library (`bfcpmsg`) installed. Besides, the libraries depend on pthread and OpenSSL, so be sure to install them before proceeding.

Edit the `Makefile` according to your settings and compiler. By default gcc will be used, and each library installed to /usr as destination prefix (/usr/include, /usr/lib).

There are several available targets to compile the code:

* `make linux` will compile the testcode and the library, creating an executable file for Linux;
* `make win32` will compile the testcode and the library, creating an executable file for Windows;
* `make so` will only compile the library, creating a shared object for Linux;
* `make dll` will only compile the library, creating a DLL for Windows.

If you want to compile all the available targets, just use:

	make all

To install the compiled library (on Linux only), type, as root:

	make install

To install the libraries in a Windows environment you'll need to manually copy the headers file to your include folder, and copy the resulting DLL(s) where needed.

## Testing the libraries

You can execute the sample applications to test the just compiled libraries by typing:

	./bfcp_participant

in the `bfcpclt` folder, and:

	./bfcp_server

in the `bfcpsrv` folder. If you want to exploit the BFCP over TLS functionality, you'll need to create a certificate file and a private key for the server. By default the BFCP server will use `server.pem` as certificate filename and `server.key` as private key filename, and it will look for those file in the current folder. In case you want to use different files in the test application, edit the opportune line of code in `main.c` to meet your requirements and recompile. No certificates and keys are currently used for the participant side.

A menu option has been made available in both the applications to test the libraries. Just play with it according to your needs to learn how the library works and to make tests on your own.
