#include "ctb-0.16/serportx.h"

#include <sstream>

namespace ctb {

    int SerialPort_x::Open( const char* portname, int baudrate,
					   const char* protocol,
					   FlowControl flowControl )
    {
	   SerialPort_DCS dcs;

	   dcs.baud = baudrate;

	   // default wordlen is 8
	   if( ( protocol[ 0 ] >= '5' ) && ( protocol[ 0 ] <= '8' )) {
		  
		  dcs.wordlen = protocol[ 0 ] - '0';

	   }
	   else {

		  return -1;

	   }

	   // protocol is given as a string like "8N1", the first
	   // character specifies the data bits (5...8), the second
	   // the parity (None,Odd,Even,Mark,Space).
	   // The third character defines the stopbit (1...2).
	   switch( protocol[ 1 ] ) {
	   case 'N': case 'n': dcs.parity = ParityNone; break;
	   case 'O': case 'o': dcs.parity = ParityOdd; break;
	   case 'E': case 'e': dcs.parity = ParityEven; break;
	   case 'M': case 'm': dcs.parity = ParityMark; break;
	   case 'S': case 's': dcs.parity = ParitySpace; break;
		  // all other parameters cause an error!
	   default: return -1;
	   }
	   // default stopbits is 1
	   if( ( protocol[ 2 ] >= '1' ) && ( protocol[ 2 ] <= '2' )) {
		  
		  dcs.stopbits = protocol[ 2 ] - '0';

	   }
	   else {

		  return -1;

	   }
	   // default flow control is disabled
	   dcs.rtscts = ( flowControl == RtsCtsFlowControl );

	   dcs.xonxoff = ( flowControl == XonXoffFlowControl );

	   // save the settings in the internal dcs for later use
	   m_dcs = dcs;

	   return OpenDevice( portname, &m_dcs );

    }

    int SerialPort_x::Open( const int portnumber, int baudrate,
					   const char* protocol,
					   FlowControl flowControl )
    {
	   // portnumbers start with 1
	   if( portnumber < 1 ) {

		  return -1;

	   }
 	   std::stringstream devname;

#if defined ( WIN32 )
	   // some systems like WinCE doesn't like the extended port numbering...
	   portnumber < 10 ? devname << "com" << portnumber : 
		  devname << "\\\\.\\com" << portnumber;
#else
	   devname << "/dev/ttyS" << ( portnumber - 1 );
#endif
    
	   return Open( devname.str().c_str(), baudrate, protocol, flowControl );
    }

    bool SerialPort_x::IsStandardRate( int rate )
    {
	   const int rates[] = {
		  150, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600,
		  115200, 230400, 460800, 921600
	   };

	   for( unsigned int i = 0; i < ( sizeof( rates ) / sizeof( int ) ); i++ ) {

		  if( rate == rates[ i ] ) {

			 return true;

		  }
	   }
	   return false;
    }

} // namespace ctb
