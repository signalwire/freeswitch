/////////////////////////////////////////////////////////////////////////////
// Name:        gpibx.cpp
// Purpose:
// Author:      Joachim Buermann
// Id:          $Id: gpibx.cpp,v 1.1.1.1 2004/11/24 10:30:11 jb Exp $
// Copyright:   (c) 2001,2004 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "ctb-0.16/gpib.h"
#include "ctb-0.16/timer.h"
#include <stdio.h>
#include <string.h>
#ifdef WIN32
# include "ctb-0.16/win32/gpib-32.h"
#if _MSC_VER < 1900
# define snprintf _snprintf
#endif
#elif __GNUG__
# include <gpib/ib.h>
#endif

namespace ctb {

    const char* GPIB1 = "gpib1";
    const char* GPIB2 = "gpib2";

    struct gpibErr_t {
	   int m_errno;
	   const char* m_notation;
	   const char* m_description;
    };

    static gpibErr_t gpibErrors[] = {
	   {0,"EDVR","DOS Error"},
	   {1,"ECIC","Specified GPIB Interface Board is Not Active Controller"},
	   {2,"ENOL","No present listing device"},
	   {3,"EADR","GPIB Board has not been addressed properly"},
	   {4,"EARG","Invalid argument"},
	   {5,"ESAC","Specified GPIB Interface Board is not System Controller"},
	   {6,"EABO","I/O operation aborted (time-out)"},
	   {7,"ENEB","Non-existent GPIB board"},
	   {10,"EOIP","Routine not allowed during asynchronous I/O operation"},
	   {11,"ECAP","No capability for operation"},
	   {12,"EFSO","File System Error"},
	   {14,"EBUS","Command byte transfer error"},
	   {15,"ESTB","Serial poll status byte lost"},
	   {16,"ESQR","SRQ stuck in ON position"},
	   {20,"ETAB","Table problem"},
	   {247,"EINT","No interrupt configured on board"},
	   {248,"EWMD","Windows is not in Enhanced mode"},
	   {249,"EVDD","GPIB driver is not installed"},
	   {250,"EOVR","Buffer Overflow"},
	   {251,"ESML","Two library calls running simultaneously"},
	   {252,"ECFG","Board type does not match GPIB.CFG"},
	   {253,"ETMR","No Windows timers available"},
	   {254,"ESLC","No Windows selectors available"},
	   {255,"EBRK","Control-Break pressed"}
    };

    char* Gpib_DCS::GetSettings()
    {
	   const char* to[] = {
		  "None","10us","30us","100us","300us","1ms","3ms","10ms","30ms", 
		  "100ms","300ms","1s","3s","10s","30s","100s","300s","1000s"
	   };
	   memset(m_buf,0,sizeof(m_buf));
	   snprintf(m_buf,sizeof(m_buf)-1,"Adr: (%i,%i) to:%s",
			  m_address1,
			  m_address2,
			  to[m_timeout]);
	   return m_buf;
    };

    int GpibDevice::CloseDevice()
    {
	   if(m_hd != -1) {
		  // goto local...
		  ibloc(m_hd);
		  // ...and switch device offline
		  ibonl(m_hd,0);
		  m_hd = -1;
		  m_board = -1;
	   }
	   return 0;
    };

    const char* GpibDevice::GetErrorString(int error,bool detailed)
    {
	   for(size_t i=0;i<(sizeof(gpibErrors)/sizeof(gpibErr_t));i++) {
		  if(gpibErrors[i].m_errno == error) {
			 if(detailed) {
				return gpibErrors[i].m_description;
			 }
			 else {
				return gpibErrors[i].m_notation;
			 }
		  }
	   }
	   return 0;
    };

// This is only for internal usage
    int GpibDevice::Ibrd(char* buf,size_t len)
    {
	   return ibrd(m_hd,buf,len);
    };

// This is only for internal usage
    int GpibDevice::Ibwrt(char* buf,size_t len)
    {
	   return ibwrt(m_hd,buf,len);
    };

    int GpibDevice::Ioctl(int cmd,void* args)
    {
	   switch(cmd) {
	   case CTB_RESET:
		  if(m_hd >= 0) {
			 ibclr(m_hd);
			 return 0;
		  }
		  return -1;
	   case CTB_GPIB_GETRSP: {
		  char spr = 0;
		  if(m_hd >= 0) {
			 ibrsp(m_hd,&spr);
			 *(int*)args = (int)spr;
			 return 0;
		  }
		  return 1; }
	   case CTB_GPIB_GETSTA:
		  *(int*)args = m_state;
		  return 0;
	   case CTB_GPIB_GETERR:
		  *(int*)args = m_error;
		  return 0;
	   case CTB_GPIB_GETLINES: {
		  short state = 0;
		  if(m_hd >= 0) {
			 iblines(m_board,&state);
			 *(int*)args = (int)state;
			 return 0;
		  }
		  return -1; }
	   case CTB_GPIB_SETTIMEOUT: {
		  if(m_hd >= 0) {
			 GpibTimeout timeout;
			 unsigned long to = *(unsigned long*)args;
			 // convert the timeout in ms (given by args) into the
			 // traditional NI-488.2 timeout period 
			 if(to > 1000000) timeout = GpibTimeout1000s;
			 else if(to >= 300000) timeout = GpibTimeout300s;
			 else if(to >= 100000) timeout = GpibTimeout100s;
			 else if(to >= 30000) timeout = GpibTimeout30s;
			 else if(to >= 10000) timeout = GpibTimeout10s;
			 else if(to >= 3000) timeout = GpibTimeout3s;
			 else if(to >= 1000) timeout = GpibTimeout1s;
			 else if(to >= 300) timeout = GpibTimeout300ms;
			 else if(to >= 100) timeout = GpibTimeout100ms;
			 else if(to >= 30) timeout = GpibTimeout30ms;
			 else if(to >= 10) timeout = GpibTimeout10ms;
			 else if(to >= 3) timeout = GpibTimeout3ms;
			 else if(to >= 1) timeout = GpibTimeout1ms;
			 else timeout = GpibTimeoutNone;
			 ibtmo(m_hd,timeout);
			 return 0; 
		  }
		  return -1; }
	   case CTB_GPIB_GTL:
		  // Forces the specified device to go to local program mode
		  if(m_hd >= 0) {
			 ibloc(m_hd);
			 return 0;
		  }
		  return -1;
	   case CTB_GPIB_REN:
		  // This routine can only be used if the specified GPIB 
		  // Interface Board is the System Controller.
		  // Remember that even though the REN line is asserted, 
		  // the device(s) will not be put into remote state until is
		  // addressed to listen by the Active Controller
		  if(m_hd) {
			 char adr = (char)m_dcs.m_address1;
			 ibsre(m_board,1);
			 ibcmd(m_board,&adr,1);
			 return 0;
		  }
		  return -1;
	   case CTB_GPIB_RESET_BUS:
		  ibsic(m_board);
		  return 0;
	   case CTB_GPIB_GET_EOS_CHAR:
		  if( m_hd ) {
			 *(int*)args = (int)m_dcs.m_eosChar;
			 return 0;
		  }
		  return -1;
	   case CTB_GPIB_SET_EOS_CHAR:
#ifdef __GNUG__
		  // FIXME!
		  // Doesn't work with linux-gpib-3.2.08. All EOS beside 0x00
		  // are blocking during sending data to the device. (Look at 
		  // function my_ibwrt in linux-gpib-3.2.08/lib/ibWrt.c
		  if( m_hd ) {
			 m_dcs.m_eosChar = (char)*(int*)args;
			 ibeos(m_hd,(m_dcs.m_eosMode << 8) | m_dcs.m_eosChar);
			 return 0;
		  }
#endif
		  return -1;
	   case CTB_GPIB_GET_EOS_MODE:
		  if( m_hd ) {
			 *(int*)args = (int)m_dcs.m_eosMode;
			 return 0;
		  }
		  return -1;
	   case CTB_GPIB_SET_EOS_MODE:
		  if( m_hd ) {
			 m_dcs.m_eosMode = (char)*(int*)args;
			 ibeos(m_hd,(m_dcs.m_eosMode << 8) | m_dcs.m_eosChar);
			 return 0;
		  }
		  return -1;
	   }
	   // error or unknown command
	   return -1;
    };

    int GpibDevice::FindListeners(int board)
    {
	   int listeners = 0;
	   if((unsigned int)board > 1) {
		  return -1;
	   }
	   // reset the GPIB, otherwise no connected device is found (linux)
	   SendIFC(board);
	   // list of primary addresses to searching for. Must be terminated
	   // with NOADDR.
	   Addr4882_t addrlist[31];
	   // The range of valid addresses is 1...30, 0 is reservated by the 
	   // controller, 31 is not valid
	   for(int i = 0;i < 30; i++) addrlist[i] = (Addr4882_t) i + 1;
	   addrlist[30] = NOADDR;
	   // place to store the results
	   Addr4882_t results[31];
	   memset(results,0,sizeof(results));
	   FindLstn(board, addrlist, results, 31);
	   if(ibsta & ERR) {
		  return -1;
	   }
	   for(int i=0;i<=30;i++) {
		  if(results[i]) {
			 listeners |= 1 << results[i];
		  } 
	   }
	   return listeners;
    };

    int GpibDevice::Open( const char* devname, int address )
    {
	   m_dcs.m_address1 = address;

	   return OpenDevice( devname, &m_dcs );
    }


    int GpibDevice::OpenDevice(const char* devname, void* dcs)
    {
	   // if dcs isn't NULL, type cast
	   if(dcs) m_dcs = *(Gpib_DCS*)dcs;
    
	   if(strncmp(devname,"gpib1",5) == 0) m_board = 0;
	   else if(strncmp(devname,"gpib2",5) == 0) m_board = 1;
	   if(m_board < 0) {
		  return -1;
	   }
	   // check for a valid timeout
	   if((unsigned int)m_dcs.m_timeout > GpibTimeout1000s) {
		  m_dcs.m_timeout = GpibTimeout10us;
	   }

	   m_hd = ibdev(m_board,
				 m_dcs.m_address1,
				 m_dcs.m_address2,
				 m_dcs.m_timeout,
				 m_dcs.m_eot,
#ifdef __GNUG__
				 // FIXME!
				 // linux-gpib-3.2.08 doesn't work with any EOS (blocks). 
				 // Because we always has to add an EOS on the message 
				 // (independent of the m_eosChar setting), we can ignore it! 
				 0
#else
				 (m_dcs.m_eosMode << 8) | m_dcs.m_eosChar
#endif
		  );
	   if(m_hd < 0) {
		  // no gpib controller installed (not found)
		  return -2;
	   }
	   // test for a connected listener (device with given address)
	   short int listen = 0;
	   ibln(m_board,m_dcs.m_address1,NO_SAD,&listen);
	   if(!listen) {
		  // no listener at the given address
		  CloseDevice();
		  return -3;
	   }
	   // reset device
	   ibclr(m_hd);
	   // save state, error and count
	   m_state = ThreadIbsta();
	   m_count = ThreadIbcnt();
	   m_error = ThreadIberr();
	   // no error
	   return 0;
    };

    int GpibDevice::Read(char* buf,size_t len)
    {
	   // if something is in the fifo, first read that
	   if(m_fifo->items() > 0) {
		  return m_fifo->read(buf,len);
	   }
	   m_state = ibrd(m_hd,buf,len);
	   m_error = ThreadIberr();
	   m_count = ThreadIbcnt();
	   return m_count;
    };

    int GpibDevice::Write(char* buf,size_t len)
    {
	   m_state = ibwrt(m_hd,buf,len);
	   m_error = ThreadIberr();
	   m_count = ThreadIbcnt();
	   return m_count;
    };

} // namespace ctb    
