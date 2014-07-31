/*
 *
 *
 *                     Win32 include file
 *          for accessing the 32-bit GPIB DLL (gpib-32.dll)
 *
 *
 *         Contains user variables (ibsta, iberr, ibcnt, ibcntl),
 *         function prototypes and useful defined constants for
 *         calling NI-488 and NI-488.2 routines from a Microsoft
 *         C/C++ Win32 application.
 *
 *
 *            Copyright 1998 National Instruments Corporation
 *
 */

#ifndef DECL_32_H       // ensure we are only included once
#define DECL_32_H

#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif


/***************************************************************************/
/*    HANDY CONSTANTS FOR USE BY APPLICATION PROGRAMS ...                  */
/***************************************************************************/
#define UNL  0x3f  /* GPIB unlisten command                */
#define UNT  0x5f  /* GPIB untalk command                  */
#define GTL  0x01  /* GPIB go to local                     */
#define SDC  0x04  /* GPIB selected device clear           */
#define PPC  0x05  /* GPIB parallel poll configure         */
#define GET  0x08  /* GPIB group execute trigger           */
#define TCT  0x09  /* GPIB take control                    */
#define LLO  0x11  /* GPIB local lock out                  */
#define DCL  0x14  /* GPIB device clear                    */
#define PPU  0.16  /* GPIB parallel poll unconfigure       */
#define SPE  0x18  /* GPIB serial poll enable              */
#define SPD  0x19  /* GPIB serial poll disable             */
#define PPE  0x60  /* GPIB parallel poll enable            */
#define PPD  0x70  /* GPIB parallel poll disable           */

/* GPIB status bit vector :                                */
/*       global variable ibsta and wait mask               */

#define ERR     (1<<15) /* Error detected                  */
#define TIMO    (1<<14) /* Timeout                         */
#define END     (1<<13) /* EOI or EOS detected             */
#define SRQI    (1<<12) /* SRQ detected by CIC             */
#define RQS     (1<<11) /* Device needs service            */
#define CMPL    (1<<8)  /* I/O completed                   */
#define LOK     (1<<7)  /* Local lockout state             */
#define REM     (1<<6)  /* Remote state                    */
#define CIC     (1<<5)  /* Controller-in-Charge            */
#define ATN     (1<<4)  /* Attention asserted              */
#define TACS    (1<<3)  /* Talker active                   */
#define LACS    (1<<2)  /* Listener active                 */
#define DTAS    (1<<1)  /* Device trigger state            */
#define DCAS    (1<<0)  /* Device clear state              */

/* Error messages returned in global variable iberr        */

#define EDVR  0  /* System error                           */
#define ECIC  1  /* Function requires GPIB board to be CIC */
#define ENOL  2  /* Write function detected no Listeners   */
#define EADR  3  /* Interface board not addressed correctly*/
#define EARG  4  /* Invalid argument to function call      */
#define ESAC  5  /* Function requires GPIB board to be SAC */
#define EABO  6  /* I/O operation aborted                  */
#define ENEB  7  /* Non-existent interface board           */
#define EDMA  8  /* Error performing DMA                   */
#define EOIP 10  /* I/O operation started before previous  */
                 /* operation completed                    */
#define ECAP 11  /* No capability for intended operation   */
#define EFSO 12  /* File system operation error            */
#define EBUS 14  /* Command error during device call       */
#define ESTB 15  /* Serial poll status byte lost           */
#define ESRQ 16  /* SRQ remains asserted                   */
#define ETAB 20  /* The return buffer is full.             */
#define ELCK 21  /* Address or board is locked.            */

/* EOS mode bits                                           */

#define BIN  (1<<12) /* Eight bit compare                  */
#define XEOS (1<<11) /* Send END with EOS byte             */
#define REOS (1<<10) /* Terminate read on EOS              */

/* Timeout values and meanings                             */

#define TNONE    0   /* Infinite timeout (disabled)        */
#define T10us    1   /* Timeout of 10 us (ideal)           */
#define T30us    2   /* Timeout of 30 us (ideal)           */
#define T100us   3   /* Timeout of 100 us (ideal)          */
#define T300us   4   /* Timeout of 300 us (ideal)          */
#define T1ms     5   /* Timeout of 1 ms (ideal)            */
#define T3ms     6   /* Timeout of 3 ms (ideal)            */
#define T10ms    7   /* Timeout of 10 ms (ideal)           */
#define T30ms    8   /* Timeout of 30 ms (ideal)           */
#define T100ms   9   /* Timeout of 100 ms (ideal)          */
#define T300ms  10   /* Timeout of 300 ms (ideal)          */
#define T1s     11   /* Timeout of 1 s (ideal)             */
#define T3s     12   /* Timeout of 3 s (ideal)             */
#define T10s    13   /* Timeout of 10 s (ideal)            */
#define T30s    14   /* Timeout of 30 s (ideal)            */
#define T100s   15   /* Timeout of 100 s (ideal)           */
#define T300s   16   /* Timeout of 300 s (ideal)           */
#define T1000s  17   /* Timeout of 1000 s (ideal)          */


/*  IBLN Constants                                         */
#define NO_SAD   0
#define ALL_SAD -1


/*  The following constants are used for the second parameter of the
 *  ibconfig function.  They are the "option" selection codes.
 */
#define  IbcPAD        0x0001      /* Primary Address                      */
#define  IbcSAD        0x0002      /* Secondary Address                    */
#define  IbcTMO        0x0003      /* Timeout Value                        */
#define  IbcEOT        0x0004      /* Send EOI with last data byte?        */
#define  IbcPPC        0x0005      /* Parallel Poll Configure              */
#define  IbcREADDR     0x0006      /* Repeat Addressing                    */
#define  IbcAUTOPOLL   0x0007      /* Disable Auto Serial Polling          */
#define  IbcCICPROT    0x0008      /* Use the CIC Protocol?                */
#define  IbcIRQ        0x0009      /* Use PIO for I/O                      */
#define  IbcSC         0x000A      /* Board is System Controller?          */
#define  IbcSRE        0x000B      /* Assert SRE on device calls?          */
#define  IbcEOSrd      0x000C      /* Terminate reads on EOS               */
#define  IbcEOSwrt     0x000D      /* Send EOI with EOS character          */
#define  IbcEOScmp     0x000E      /* Use 7 or 8-bit EOS compare           */
#define  IbcEOSchar    0x000F      /* The EOS character.                   */
#define  IbcPP2        0x0010      /* Use Parallel Poll Mode 2.            */
#define  IbcTIMING     0x0011      /* NORMAL, HIGH, or VERY_HIGH timing.   */
#define  IbcDMA        0x0012      /* Use DMA for I/O                      */
#define  IbcReadAdjust 0x0013      /* Swap bytes during an ibrd.           */
#define  IbcWriteAdjust 0x014      /* Swap bytes during an ibwrt.          */
#define  IbcSendLLO    0x0017      /* Enable/disable the sending of LLO.      */
#define  IbcSPollTime  0x0018      /* Set the timeout value for serial polls. */
#define  IbcPPollTime  0x0019      /* Set the parallel poll length period.    */
#define  IbcEndBitIsNormal 0x001A  /* Remove EOS from END bit of IBSTA.       */
#define  IbcUnAddr         0x001B  /* Enable/disable device unaddressing.     */
#define  IbcSignalNumber   0x001C  /* Set UNIX signal number - unsupported */
#define  IbcBlockIfLocked  0x001D   /* Enable/disable blocking for locked boards/devices         */
#define  IbcHSCableLength  0x001F  /* Length of cable specified for high speed timing.*/
#define  IbcIst        0x0020      /* Set the IST bit.                     */
#define  IbcRsv        0x0021      /* Set the RSV byte.                    */

/*
 *    Constants that can be used (in addition to the ibconfig constants)
 *    when calling the ibask() function.
 */

#define  IbaPAD            IbcPAD
#define  IbaSAD            IbcSAD
#define  IbaTMO            IbcTMO
#define  IbaEOT            IbcEOT
#define  IbaPPC            IbcPPC
#define  IbaREADDR         IbcREADDR
#define  IbaAUTOPOLL       IbcAUTOPOLL
#define  IbaCICPROT        IbcCICPROT
#define  IbaIRQ            IbcIRQ
#define  IbaSC             IbcSC
#define  IbaSRE            IbcSRE
#define  IbaEOSrd          IbcEOSrd
#define  IbaEOSwrt         IbcEOSwrt
#define  IbaEOScmp         IbcEOScmp
#define  IbaEOSchar        IbcEOSchar
#define  IbaPP2            IbcPP2
#define  IbaTIMING         IbcTIMING
#define  IbaDMA            IbcDMA
#define  IbaReadAdjust     IbcReadAdjust
#define  IbaWriteAdjust    IbcWriteAdjust
#define  IbaSendLLO        IbcSendLLO
#define  IbaSPollTime      IbcSPollTime
#define  IbaPPollTime      IbcPPollTime
#define  IbaEndBitIsNormal IbcEndBitIsNormal
#define  IbaUnAddr         IbcUnAddr
#define  IbaSignalNumber   IbcSignalNumber
#define  IbaBlockIfLocked  IbcBlockIfLocked
#define  IbaHSCableLength  IbcHSCableLength
#define  IbaIst            IbcIst
#define  IbaRsv            IbcRsv

#define  IbaBNA            0x0200   /* A device's access board.                  */


/* Values used by the Send 488.2 command. */
#define  NULLend 0x00  /* Do nothing at the end of a transfer.*/
#define  NLend   0x01  /* Send NL with EOI after a transfer.  */
#define  DABend  0x02  /* Send EOI with the last DAB.         */

/* Value used by the 488.2 Receive command.
 */
#define  STOPend     0x0100


/* Address type (for 488.2 calls) */

typedef short Addr4882_t;       /* System dependent: must be 16 bits */

/*
 *  This macro can be used to easily create an entry in address list
 *  that is required by many of the 488.2 functions. The primary address goes in the
 *  lower 8-bits and the secondary address goes in the upper 8-bits.
 */
#define  MakeAddr(pad, sad)   ((Addr4882_t)(((pad)&0xFF) | ((sad)<<8)))

/*
 *  This value is used to terminate an address list.  It should be
 *  assigned to the last entry.
 */
#ifndef NOADDR
#define  NOADDR    (Addr4882_t)((unsigned short)0xFFFF)
#endif

/*
 *  The following two macros are used to "break apart" an address list
 *  entry.  They take an unsigned integer and return either the primary
 *  or secondary address stored in the integer.
 */
#define  GetPAD(val)    ((val) & 0xFF)
#define  GetSAD(val)    (((val) >> 8) & 0xFF)

/* iblines constants */

#define  ValidEOI   (short)0x0080
#define  ValidATN   (short)0x0040
#define  ValidSRQ   (short)0x0020
#define  ValidREN   (short)0x0010
#define  ValidIFC   (short)0x0008
#define  ValidNRFD  (short)0x0004
#define  ValidNDAC  (short)0x0002
#define  ValidDAV   (short)0x0001
#define  BusEOI     (short)0x8000
#define  BusATN     (short)0x4000
#define  BusSRQ     (short)0x2000
#define  BusREN     (short)0x1000
#define  BusIFC     (short)0x0800
#define  BusNRFD    (short)0x0400
#define  BusNDAC    (short)0x0200
#define  BusDAV     (short)0x0100

/****
 **** typedef for ibnotify callback ****
 ****/
typedef int (__stdcall * GpibNotifyCallback_t)(int, int, int, long, PVOID);

#define IBNOTIFY_REARM_FAILED    0xE00A003F


/*************************************************************************/
/*                                                                       */
/*  iblockx and ibunlockx definitions --- for use with GPIB-ENET only !! */
/*                                                                       */
/*************************************************************************/
#define  TIMMEDIATE                 -1
#define  TINFINITE                  -2
#define  MAX_LOCKSHARENAME_LENGTH   64

#if defined(UNICODE)
   #define iblockx iblockxW
#else
   #define iblockx iblockxA
#endif

extern int __stdcall iblockxA (int ud, int LockWaitTime, PCHAR LockShareName);
extern int __stdcall iblockxW (int ud, int LockWaitTime, PWCHAR LockShareName);
extern int __stdcall ibunlockx (int ud);


/***************************************************************************/
/*         IBSTA, IBERR, IBCNT, IBCNTL and FUNCTION PROTOTYPES             */
/*      ( only included if not accessing the 32-bit DLL directly )         */
/***************************************************************************/
#if !defined(GPIB_DIRECT_ACCESS)

/*
 *  Set up access to the user variables (ibsta, iberr, ibcnt, ibcntl).
 *  These are declared and exported by the 32-bit DLL.  Separate copies
 *  exist for each process that accesses the DLL.  They are shared by
 *  multiple threads of a single process.
 */

extern int  ibsta;
extern int  iberr;
extern int  ibcnt;
extern long ibcntl;


#if defined(UNICODE)
   #define ibbna  ibbnaW
   #define ibfind ibfindW
   #define ibrdf  ibrdfW
   #define ibwrtf ibwrtfW
#else
   #define ibbna  ibbnaA
   #define ibfind ibfindA
   #define ibrdf  ibrdfA
   #define ibwrtf ibwrtfA
#endif

/*
 *  Extern 32-bit GPIB DLL functions
 */

/*  NI-488 Function Prototypes  */
extern int __stdcall ibfindA   (LPCSTR udname);
extern int __stdcall ibbnaA    (int ud, LPCSTR udname);
extern int __stdcall ibrdfA    (int ud, LPCSTR filename);
extern int __stdcall ibwrtfA   (int ud, LPCSTR filename);

extern int __stdcall ibfindW   (LPCWSTR udname);
extern int __stdcall ibbnaW    (int ud, LPCWSTR udname);
extern int __stdcall ibrdfW    (int ud, LPCWSTR filename);
extern int __stdcall ibwrtfW   (int ud, LPCWSTR filename);

extern int __stdcall ibask    (int ud, int option, PINT v);
extern int __stdcall ibcac    (int ud, int v);
extern int __stdcall ibclr    (int ud);
extern int __stdcall ibcmd    (int ud, PVOID buf, long cnt);
extern int __stdcall ibcmda   (int ud, PVOID buf, long cnt);
extern int __stdcall ibconfig (int ud, int option, int v);
extern int __stdcall ibdev    (int boardID, int pad, int sad, int tmo, int eot, int eos);
extern int __stdcall ibdiag   (int ud, PVOID buf, long cnt);
extern int __stdcall ibdma    (int ud, int v);
extern int __stdcall ibeos    (int ud, int v);
extern int __stdcall ibeot    (int ud, int v);
extern int __stdcall ibgts    (int ud, int v);
extern int __stdcall ibist    (int ud, int v);
extern int __stdcall iblines  (int ud, PSHORT result);
extern int __stdcall ibln     (int ud, int pad, int sad, PSHORT listen);
extern int __stdcall ibloc    (int ud);
extern int __stdcall ibnotify (int ud, int mask, GpibNotifyCallback_t Callback, PVOID RefData);
extern int __stdcall ibonl    (int ud, int v);
extern int __stdcall ibpad    (int ud, int v);
extern int __stdcall ibpct    (int ud);
extern int __stdcall ibpoke   (int ud, long option, long v);
extern int __stdcall ibppc    (int ud, int v);
extern int __stdcall ibrd     (int ud, PVOID buf, long cnt);
extern int __stdcall ibrda    (int ud, PVOID buf, long cnt);
extern int __stdcall ibrpp    (int ud, PCHAR ppr);
extern int __stdcall ibrsc    (int ud, int v);
extern int __stdcall ibrsp    (int ud, PCHAR spr);
extern int __stdcall ibrsv    (int ud, int v);
extern int __stdcall ibsad    (int ud, int v);
extern int __stdcall ibsic    (int ud);
extern int __stdcall ibsre    (int ud, int v);
extern int __stdcall ibstop   (int ud);
extern int __stdcall ibtmo    (int ud, int v);
extern int __stdcall ibtrg    (int ud);
extern int __stdcall ibwait   (int ud, int mask);
extern int __stdcall ibwrt    (int ud, PVOID buf, long cnt);
extern int __stdcall ibwrta   (int ud, PVOID buf, long cnt);

// GPIB-ENET only functions to support locking across machines
extern int __stdcall iblock   (int ud);
extern int __stdcall ibunlock (int ud);

/**************************************************************************/
/*  Functions to access Thread-Specific copies of the GPIB global vars */

extern int  __stdcall ThreadIbsta (void);
extern int  __stdcall ThreadIberr (void);
extern int  __stdcall ThreadIbcnt (void);
extern long __stdcall ThreadIbcntl (void);


/**************************************************************************/
/*  NI-488.2 Function Prototypes  */

extern void __stdcall AllSpoll      (int boardID, Addr4882_t * addrlist, PSHORT results);
extern void __stdcall DevClear      (int boardID, Addr4882_t addr);
extern void __stdcall DevClearList  (int boardID, Addr4882_t * addrlist);
extern void __stdcall EnableLocal   (int boardID, Addr4882_t * addrlist);
extern void __stdcall EnableRemote  (int boardID, Addr4882_t * addrlist);
extern void __stdcall FindLstn      (int boardID, Addr4882_t * addrlist, Addr4882_t * results, int limit);
extern void __stdcall FindRQS       (int boardID, Addr4882_t * addrlist, PSHORT dev_stat);
extern void __stdcall PPoll         (int boardID, PSHORT result);
extern void __stdcall PPollConfig   (int boardID, Addr4882_t addr, int dataLine, int lineSense);
extern void __stdcall PPollUnconfig (int boardID, Addr4882_t * addrlist);
extern void __stdcall PassControl   (int boardID, Addr4882_t addr);
extern void __stdcall RcvRespMsg    (int boardID, PVOID buffer, long cnt, int Termination);
extern void __stdcall ReadStatusByte(int boardID, Addr4882_t addr, PSHORT result);
extern void __stdcall Receive       (int boardID, Addr4882_t addr, PVOID buffer, long cnt, int Termination);
extern void __stdcall ReceiveSetup  (int boardID, Addr4882_t addr);
extern void __stdcall ResetSys      (int boardID, Addr4882_t * addrlist);
extern void __stdcall Send          (int boardID, Addr4882_t addr, PVOID databuf, long datacnt, int eotMode);
extern void __stdcall SendCmds      (int boardID, PVOID buffer, long cnt);
extern void __stdcall SendDataBytes (int boardID, PVOID buffer, long cnt, int eot_mode);
extern void __stdcall SendIFC       (int boardID);
extern void __stdcall SendLLO       (int boardID);
extern void __stdcall SendList      (int boardID, Addr4882_t * addrlist, PVOID databuf, long datacnt, int eotMode);
extern void __stdcall SendSetup     (int boardID, Addr4882_t * addrlist);
extern void __stdcall SetRWLS       (int boardID, Addr4882_t * addrlist);
extern void __stdcall TestSRQ       (int boardID, PSHORT result);
extern void __stdcall TestSys       (int boardID, Addr4882_t * addrlist, PSHORT results);
extern void __stdcall Trigger       (int boardID, Addr4882_t addr);
extern void __stdcall TriggerList   (int boardID, Addr4882_t * addrlist);
extern void __stdcall WaitSRQ       (int boardID, PSHORT result);

#endif


#ifdef __cplusplus
}
#endif


#endif   // DECL_32_H

