%module serport

%{
#include "ctb-0.16/linux/serport.h"
%}

%include ../serportx.i

namespace ctb {

%pythoncode {
COM1 = "/dev/ttyS0"
COM2 = "/dev/ttyS1"
COM3 = "/dev/ttyS2"
COM4 = "/dev/ttyS3"
COM5 = "/dev/ttyS4"
COM6 = "/dev/ttyS5"
COM7 = "/dev/ttyS6"
COM8 = "/dev/ttyS7"
COM9 = "/dev/ttyS8"
};

class SerialPort : public SerialPort_x
{
protected:
    int fd;
    struct termios t, save_t;
    struct serial_icounter_struct save_info, last_info;
    speed_t AdaptBaudrate(int baud);
    
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
