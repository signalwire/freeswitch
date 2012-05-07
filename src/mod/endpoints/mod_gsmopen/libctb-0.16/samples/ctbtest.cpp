#include "ctb-0.16/ctb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <string>

using namespace std;

// ----------------- options -------------------------------
const char* options="a:b:d:e:hlp:t:";

const char* helpMessage =
{
    "A simple serial port class test\n"
    "ctbtest [options]\n"
    "available options are:\n"
    "-a     : address (only GPIB)\n"
    "-b     : baudrate [any value], default is 38400\n"
    "-d     : connected device, default is COM1\n"
    "-e     : eos\n"
    "-h     : print this\n"
    "-l     : list all available serial ports\n"
    "-p     : protocol like 8N1\n"
    "-t     : communication timeout in ms (default is 100ms)\n"
};

int main(int argc,char* argv[])
{
    int address = 15;

    int baudrate = 19200;

    string devname = ctb::COM1;

    string eos = "\r\n";

    string protocol = "8N1";

    int timeout = 100;

    bool showAvailablePorts = false;

    int quit = 0;

    int val;

    while ( ( val=getopt( argc, argv, (char*)options ) ) != EOF ) {
	   switch ( val ) {
	   case 'a' : address = strtol( optarg, NULL, 10 ); break;
	   case 'b' : baudrate = strtol( optarg, NULL, 10 ); break;
	   case 'd' : devname = optarg; break;
	   case 'h' : cerr << helpMessage << endl; exit( 0 );
	   case 'l' : showAvailablePorts = true; break;
	   case 'p' : protocol = optarg; break;
	   case 't' : timeout = strtol( optarg, NULL, 10 ); break;
	   }
    }

    ctb::IOBase* device = NULL;

    std::vector<std::string> ports;

    if( ctb::GetAvailablePorts( ports ) && showAvailablePorts ) {

	   for( int i = 0; i < ports.size(); 
		   std::cout << ports[ i++ ] << endl ) {};

	   return 0;

    }


#if ( GPIB )
    if( ( devname == ctb::GPIB1 ) || ( devname == ctb::GPIB2 ) ) {

	   ctb::GpibDevice* gpibDevice = new ctb::GpibDevice();

	   if( gpibDevice->Open( devname.c_str(), address ) >= 0 ) {

		  device = gpibDevice;

	   }

    }
    else {
#endif
      
	   ctb::SerialPort* serialPort = new ctb::SerialPort();

 	   if( serialPort->Open( devname.c_str(), baudrate, 
					    protocol.c_str(), 
					    ctb::SerialPort::NoFlowControl ) >= 0 ) {

		  device = serialPort;

	   }

#if ( GPIB )
    }
#endif

    if( ! device ) {

	   cout << "Cannot open " << devname.c_str() << endl;

	   return -1;
    
    }

    // up to know you don't have to worry any longer about the kind of
    // the connected device.  As long as you do nothing something device
    // specific (like toggle some modem control lines), the access is
    // the same for each device.

    string line;

    char receiveBuf[ 128 ];

    cout << "Enter your command or just press Enter without any\n"
	   "input for exit!";

    while( true ) {

	   cout << endl << "Your input >";

	   // read the string to send
	   getline( cin, line );

	   // add some defined EOS (end of string sequence or character)
	   if( line.empty() ) {

		  break;

	   }

	   line += eos;
      
	   // send data throughout the connected device independent of the typ
	   if( device->Writev( (char*)line.c_str(),
					   line.size(), 
					   timeout ) != line.size() ) {
			 
		  cerr << "Incomplete data transmission" << endl;

	   }

	   int readed = 0;

	   do {

		  // in case of an event driven GUI you better use a non blocking
		  // Read(...) in your idle function. Here we have to wait for the 
		  // response before we send another user command...
		  readed = device->Readv( receiveBuf, 
							 sizeof( receiveBuf ) - 1, 
							 timeout);

		  // something received?
		  if( readed > 0 ) {

			 receiveBuf[ readed ] = 0;

			 cout << receiveBuf;

		  }

	   } while( readed > 0 );
		  
	   cout << endl;
	   
    } // while( true )

    device->Close();

    delete device;

    return 0;
}
