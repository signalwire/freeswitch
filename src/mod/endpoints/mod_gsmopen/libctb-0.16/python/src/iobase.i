%{
#include "ctb-0.16/iobase.h"
%}

namespace ctb {

enum {
    CTB_RESET = CTB_COMMON
};

%typemap(in) char *& readbuf (char * tmp) {
	/* dont check for list */
	$1 = &tmp;
}

%typemap(argout) char *& readbuf {
	PyObject * plist = PyList_New(2);
	PyList_SetItem(plist, 0, PyString_FromString(*$1));
	PyList_SetItem(plist, 1, $result);
	$result = plist;
	delete *$1;
}

%typemap(in) size_t * readedBytes (size_t tmp) {
	/* dont check for list */
	$1 = &tmp;
}

%typemap(argout) size_t * readedBytes {
     PyList_Append($result, PyInt_FromLong(*$1));
}

class IOBase
{
protected:
    virtual int CloseDevice() = 0;
    virtual int OpenDevice(const char* devname, void* dcs = 0L) = 0;
public:
    IOBase();
    virtual ~IOBase();

    virtual const char* ClassName();
    int Close();
    virtual int Ioctl(int cmd,void* args);
    virtual int IsOpen() = 0;
    int Open(const char* devname,void* dcs=0L);
    int PutBack(char ch);
    virtual int Read(char* buf,size_t len) = 0;
    virtual int ReadUntilEOS(char*& readbuf,
					    size_t* readedBytes,
					    char* eosString = "\n",
					    long timeout_in_ms = 1000L,
					    char quota = 0);
    int Readv(char* buf,size_t len,unsigned int timeout_in_ms);
    virtual int Write(char* buf,size_t len) = 0;
    int Writev(char* buf,size_t len,unsigned int timeout_in_ms);
};

};
