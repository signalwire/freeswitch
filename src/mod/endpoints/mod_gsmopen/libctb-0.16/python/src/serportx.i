%{
#include "ctb-0.16/serportx.h"
%}

%include iobase.i

namespace ctb {

enum Parity
{
    ParityNone,
    ParityOdd,
    ParityEven,
    ParityMark,
    ParitySpace
};

enum SerialLineState
{
    LinestateDcd = 0x040,
    LinestateCts = 0x020,
    LinestateDsr = 0x100,
    LinestateDtr = 0x002,
    LinestateRing = 0x080,
    LinestateRts = 0x004,
    LinestateNull = 0x000
};

struct SerialPort_DCS
{
    int baud;
    Parity parity;
    unsigned char wordlen;
    unsigned char stopbits;
    bool rtscts;
    bool xonxoff;
    char buf[16];
    SerialPort_DCS();
    ~SerialPort_DCS();
    char* GetSettings();
}; 

struct SerialPort_EINFO
{
    int brk;
    int frame;
    int overrun;
    int parity;
    SerialPort_EINFO();
    ~SerialPort_EINFO();
};

enum {
    CTB_SER_GETEINFO = CTB_SERIAL,
    CTB_SER_GETBRK,
    CTB_SER_GETFRM,
    CTB_SER_GETOVR,
    CTB_SER_GETPAR,
    CTB_SER_GETINQUE,
    CTB_SER_SETPAR,
};

class SerialPort_x : public IOBase
{
protected:
    SerialPort_DCS m_dcs;
    char m_devname[SERIALPORT_NAME_LEN];
public:
    SerialPort_x();
    virtual ~SerialPort_x();
    const char* ClassName();
    virtual int ChangeLineState(SerialLineState flags) = 0;
    virtual int ClrLineState(SerialLineState flags) = 0;
    virtual int GetLineState() = 0;
    virtual char* GetSettingsAsString();
    virtual int Ioctl(int cmd,void* args);
    virtual int SendBreak(int duration) = 0;
    virtual int SetBaudrate(int baudrate) = 0;
    virtual int SetLineState(SerialLineState flags) = 0;
    virtual int SetParityBit( bool parity ) = 0;
    static bool IsStandardRate( long rate );
};

};
