%{
#include "ctb-0.16/win32/serport.h"	
%}

%include ../serportx.i

namespace ctb {

%pythoncode {
COM1 = "com1"
COM2 = "com2"
COM3 = "com3"
COM4 = "com4"
COM5 = "com5"
COM6 = "com6"
COM7 = "com7"
COM8 = "com8"
COM9 = "com9"
COM10 = "\\\\.\\com10"
COM11 = "\\\\.\\com11"
COM12 = "\\\\.\\com12"
COM13 = "\\\\.\\com13"
COM14 = "\\\\.\\com14"
COM15 = "\\\\.\\com15"
COM16 = "\\\\.\\com16"
COM17 = "\\\\.\\com17"
COM18 = "\\\\.\\com18"
COM19 = "\\\\.\\com19"
};

class SerialPort : public SerialPort_x
{
protected:
    HANDLE fd;
    OVERLAPPED ov;
    SerialPort_EINFO einfo;
      
    int CloseDevice();
    int OpenDevice(const char* devname, void* dcs);
public:
    SerialPort();
    ~SerialPort();

    int ChangeLineState(SerialLineState flags);
    int ClrLineState(SerialLineState flags);
    int GetLineState();
    int Ioctl(int cmd,void* args);
    int IsOpen();
    int Read(char* buf,size_t len);
    int SendBreak(int duration);
    int SetBaudrate(int baudrate);
    int SetLineState(SerialLineState flags);
    int SetParityBit( bool parity );
    int Write(char* buf,size_t len);
};

};
