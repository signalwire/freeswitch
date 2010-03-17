************************************************************************
Fixed Point SILK SDK 1.0.2 beta source code package
Copyright 2010 (c), Skype Limited
https://developer.skype.com/silk/
************************************************************************

Date: 09/03/2010 (Format: DD/MM/YYYY)

I. Description

This package contains files for compiling and testing the fixed
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

    6. How to use the comparison tool:
       
       See 'How to use the test vectors.txt' in the test_vectors folder.     	

IV. History

    Version 1.0.2 - Updated with various bugfixes and improvements
    Version 1.0.1 - First beta source code release
    
V. Compatibility

    This package has been tested under the following platforms:

    Windows XP Home and Professional
    Windows Vista, 32-bit version
    Mac OS X Version 10.5.8
    Ubuntu Linux 9.10, 64-bit version 

VI. Known Issues

    None

VII. Additional Resources

    For more information, visit the SILK SDK web site at:

    <https://developer.skype.com/silk/>
