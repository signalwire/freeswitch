************************************************************************
Fixed Point SILK SDK 1.0.8 beta source code package
Copyright 2010 (c), Skype Limited
https://developer.skype.com/silk/
************************************************************************

Date: 15/06/2011 (Format: DD/MM/YYYY)

I. Description

This package contains files for compilation and evaluation of the fixed
point SILK SDK library. The following is included in this package:

    o Source code for the fixed point SILK SDK library
    o Source code for creating encoder and decoder executables
    o Test vectors
    o Comparison tool
    o Microsoft Visual Studio solution and project files
    o Makefile for GNU C-compiler (GCC)

II. Files and Folders

    o doc/          - contains more information about the SILK SDK
    o interface/    - contains API header files
    o src/          - contains all SILK SDK library source files
    o test/         - contains source files for testing the SILK SDK
    o test_vectors/ - contains test vectors
    o Makefile      - Makefile for compiling with GCC
    o readme.txt    - this file
    o Silk_SDK.sln  - Visual Studio solution for all SILK SDK code

III. How to use the Makefile

    1. How to clean and compile the SILK SDK library:

       make clean lib

    2. How to compile an encoder executable:

       make encoder

    3. How to compile a decoder executable:

       make decoder

    4. How to compile the comparison tool:

       make signalcompare

    5. How to clean and compile all of the above:

       make clean all

    6. How to build for big endian CPU's

       Make clean all ADDED_DEFINES+=_SYSTEM_IS_BIG_ENDIAN
       To be able to use the test vectors with big endian CPU's the test programs
       need to be compiled in a different way. Note that the 16 bit input and output
       from the test programs will have the upper and lower bytes swapped with this setting.

    7. How to use the comparison tool:

       See 'How to use the test vectors.txt' in the test_vectors folder.

IV. History

    Version 1.0.8 - Improved noise shaping, various other improvements, and various bugfixes. Added a MIPS version
    Version 1.0.7 - Updated with bugfixes for LBRR and pitch estimator. SignalCompare updated
    Version 1.0.6 - Updated with bugfixes for ARM builds
    Version 1.0.5 - Updated with bugfixes for ARM builds
    Version 1.0.4 - Updated with various bugfixes and improvements, including some API changes
                    Added support for big endian platforms
                    Added resampler support for additional API sample rates
    Version 1.0.3 - Updated with various bugfixes and improvements
    Version 1.0.2 - Updated with various bugfixes and improvements
    Version 1.0.1 - First beta source code release

V. Compatibility

    This package has been tested on the following platforms:

    Windows XP Home and Professional
    Windows Vista, 32-bit version
    Mac OSX intel
    Mac OSX ppc
    Ubuntu Linux 9.10, 64-bit version

VI. Known Issues

    None

VII. Additional Resources

    For more information, visit the SILK SDK web site at:

    <https://developer.skype.com/silk/>
