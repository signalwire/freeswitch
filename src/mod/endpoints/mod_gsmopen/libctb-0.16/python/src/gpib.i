%{
#include "ctb-0.16/gpib.h"
%}

%include iobase.i

namespace ctb {

%typemap(in) void * dcs (Gpib_DCS tmp) {
	/* dont check for list */
	$1 = &tmp;
}

enum GpibTimeout
{
    GpibTimeoutNONE = 0, 
    GpibTimeout10us, 
    GpibTimeout30us, 
    GpibTimeout100us,
    GpibTimeout300us,
    GpibTimeout1ms,  
    GpibTimeout3ms,  
    GpibTimeout10ms, 
    GpibTimeout30ms, 
    GpibTimeout100ms,
    GpibTimeout300ms,
    GpibTimeout1s,   
    GpibTimeout3s,   
    GpibTimeout10s,  
    GpibTimeout30s,  
    GpibTimeout100s, 
    GpibTimeout300s, 
    GpibTimeout1000s
};

struct Gpib_DCS
{
    int m_address1;
    int m_address2;
    GpibTimeout m_timeout;
    bool m_eot;
    unsigned char m_eosChar;
    unsigned char m_eosMode;
    Gpib_DCS();
    ~Gpib_DCS();
    char* GetSettings();
}; 

enum {
    CTB_GPIB_SETADR = CTB_GPIB,
    CTB_GPIB_GETRSP,
    CTB_GPIB_GETSTA,
    CTB_GPIB_GETERR,
    CTB_GPIB_GETLINES,
    CTB_GPIB_SETTIMEOUT,
    CTB_GPIB_GTL,
    CTB_GPIB_REN,
    CTB_GPIB_RESET_BUS,
    CTB_GPIB_SET_EOS_CHAR,
    CTB_GPIB_GET_EOS_CHAR,
    CTB_GPIB_SET_EOS_MODE,
    CTB_GPIB_GET_EOS_MODE
};

class GpibDevice : public IOBase
{
protected:
    int m_board;
    int m_hd;
    int m_state;
    int m_error;
    int m_count;
    int m_asyncio;
    Gpib_DCS m_dcs;
    int CloseDevice();
    int OpenDevice(const char* devname, void* dcs);
    virtual const char* GetErrorString(int error,bool detailed);
public:
    GpibDevice();
    virtual ~GpibDevice();
    const char* ClassName();
    virtual const char* GetErrorDescription(int error);
    virtual const char* GetErrorNotation(int error);
    virtual char* GetSettingsAsString();
    int Ibrd(char* buf,size_t len);
    int Ibwrt(char* buf,size_t len);
    virtual int Ioctl(int cmd,void* args);
    int IsOpen();
    int Read(char* buf,size_t len);
    int Write(char* buf,size_t len);

    static int FindListeners(int board = 0);

};

};
