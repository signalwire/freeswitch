BFCP messages library
=====================

This is a brief guide to the compilation of the BFCP (Binary Floor Control Protocol) building and parsing libraries, according to the RFC specifications (attached), and of the test application that has been made available to show its use. Notice that this library only implements the message build and parsing functionality: for a BFCP client and/or server, you'll the `bfcpclt`/`bfcpsrv` libraries as well, or you'll need to implement the client/server behaviour yourself.

## Compiling the library

Edit the Makefile according to your settings and compiler. By default gcc will be used, and each library installed to /usr as destination prefix (/usr/include, /usr/lib).

There are several available targets to compile the code:

- `make linux` will compile the testcode and the library, creating an executable file for Linux;
- `make win32` will compile the testcode and the library, creating an executable file for Windows;
- `make so` will only compile the library, creating a shared object for Linux;
- `make dll` will only compile the library, creating a DLL for Windows.

If you want to compile all the available targets, just use:

	make all

To install the compiled library (on Linux only), type, as root:

	make install

To install the libraries in a Windows environment you'll need to manually copy the headers file to your include folder, and copy the resulting DLL(s) where needed.

## Testing the library

A command line sample application is available to test the library. Just play with it according to your needs to learn how the library works and to make tests on your own.

The test code that is available is testcode.c, a small application that builds a BFCP message, saves it to a file (mesg.hex), opens the file again, reads its contents and finally parses the built message, showing all the attributes. By default the test code will build a ChairAction message: just change the primitive name and the required arguments to try to build a message of your choice.
