import wxctb, sys, re

DCD = wxctb.LinestateDcd
CTS = wxctb.LinestateCts
DSR = wxctb.LinestateDsr
DTR = wxctb.LinestateDtr
RING = wxctb.LinestateRing
RTS = wxctb.LinestateRts
NULL = wxctb.LinestateNull

def abstract():
    import inspect
    caller = inspect.getouterframes(inspect.currentframe())[1][3]
    raise NotImplementedError(caller + ' must be implemented in subclass')

class IOBase:
    def __init__(self):
        self.device = None
        # set timeout to 1000ms (the default)
        self.timeout = 1000
        
    def __del__(self):
        pass
    
    def Close(self):
        if self.device:
            self.device.Close()

    def GetTimeout(self):
        """
        Returns the internal timeout value in milliseconds
        """
        return self.timeout

    def Ioctl(self,cmd,arg):
        if self.device:
            self.device.Ioctl(cmd,arg)
            
    def Open(self):
        abstract()

    def PutBack(self,char):
        return self.device.PutBack(char)
    
    def Read(self,length):
        """
        Try to read the given count of data (length) and returns the
        successfully readed number of data. The function never blocks.
        For example:
        readed = dev.Read(100)
        """
        buf = "\x00"*(length+1)
        rd = self.device.Read(buf,length)
        return buf[0:rd]

    def ReadBinary(self,eos="\n"):
        """
        Special SCPI command. Read the next data coded as a SCPI
        binary format.
        A binary data transfer will be startet by '#'. The next byte
        tells the count of bytes for the binary length header,
        following by the length bytes. After these the data begins.
        For example:
        #500004xxxx
        The header length covers 5 Byte, the length of the binary
        data is 4 (x means the binary data bytes)
        """
        try:
            eoslen = len(eos)
            b=self.Readv(2)
            if len(b) == 2:
                hl = int(b[1])
                b = self.Readv(hl)
                if len(b) == hl:
                    dl = int(b)
                    # don't left over the eos string or character in the
                    # device input buffer
                    data = self.Readv(dl+eoslen)
                    # check, if the binary data block is complete
                    if data[dl] == '#':
                        # not complete, another block is following
                        for c in data[dl:dl+eoslen]:
                            self.PutBack(c)

                        data = data[:dl] + self.ReadBinary()
                    return data
        except:
            pass
        return ''

    def ReadUntilEOS(self,eos="\n",quota=0):
        """
        ReadUntilEOS(eosString=\"\\n\",timeout=1000)
        Reads data until the given eos string was received (default is
        the linefeed character (0x0a) or the internal timeout
        (default 1000ms) was reached.
        ReadUntilEOS returns the result as the following tuple:
        ['received string',state,readedBytes]
        If a timeout occurred, state is 0, otherwise 1
        """
        return self.device.ReadUntilEOS("",0,eos,self.timeout,quota)

    def Readv(self,length):
        """
        Try to read the given count of data. Readv blocks until all data
        was readed successfully or the internal timeout, set with the
        class member function SetTimeout(timeout), was reached.
        Returns the readed data.
        """
        buf = "\x00"*length
        rd = self.device.Readv(buf,length,self.timeout)
        return buf[0:rd]

    def ResetBus(self):
        """
        If the underlaying interface needs some special reset operations
        (for instance the GPIB distinguish between a normal device reset
        and a special bus reset), you can put some code here)
        """
        pass

    def SetTimeout(self,timeout):
        """
        Set the internal timeout value in milliseconds for all blocked
        operations like ReadUntilEOS, Readv and Writev.
        """
        self.timeout = timeout
        
    def Write(self,string):
        """
        Writes the given string to the device and returns immediately.
        Write returns the number of data bytes successfully written or a
        negativ number if an error occured. For some circumstances, not
        the complete string was written.
        So you have to verify the return value to check this out.
        """
        return self.device.Write(string,len(string))

    def Writev(self,string):
        """
        Writes the given string to the device. The function blocks until
        the complete string was written or the internal timeout, set with
        SetTimeout(timeout), was reached.
        Writev returns the number of data successfully written or a
        negativ value, if an errors occurred.
        """
        return self.device.Writev(string,len(string),self.timeout)

class SerialPort(IOBase):
    def __init__(self):
        IOBase.__init__(self)

    def __del__(self):
        self.Close()
        
    def ChangeLineState(self,lineState):
        """
        Change (toggle) the state of each the lines given in the
        linestate parameter. Possible values are DTR and/or RTS.
        For example to toggle the RTS line only:
        dev.ChangeLineState(RTS)
        """
        self.device.ChangeLineState(lineState)

    def ClrLineState(self,lineState):
        """
        Clear the lines given in the linestate parameter. Possible
        values are DTR and/or RTS. For example to clear only
        the RTS line:
        dev.ClrLineState(RTS)
        """
        self.device.ClrLineState(lineState)

    def GetAvailableBytes(self):
        """
        Returns the available bytes in the input queue of the serial
        driver.
        """
        n = wxctb.new_intp()
        wxctb.intp_assign(n, 0)
        self.device.Ioctl(wxctb.CTB_SER_GETINQUE,n)
        return wxctb.intp_value(n)

    def GetCommErrors(self):
        """
        Get the internal communication errors like breaks, framing,
        parity or overrun errors.
        Returns the count of each error as a tuple like this:
        (b,f,o,p) = dev.GetCommErrors()
        b: breaks, f: framing errors,  o: overruns, p: parity errors
        """
        einfo = wxctb.SerialPort_EINFO()
        self.device.Ioctl(wxctb.CTB_SER_GETEINFO,einfo)
        return einfo.brk,einfo.frame,einfo.overrun,einfo.parity
    
    def GetLineState(self):
        """
        Returns the current linestates of the CTS, DCD, DSR and RING
        signal line as an integer value with the appropriate bits or
        -1 on error.
        For example:
        lines = dev.GetLineState()
        if lines & CTS:
            print \"CTS is on\"
        """
        return self.device.GetLineState()

    def Open(self,devname,baudrate,protocol='8N1',handshake='no_handshake'):
        """
        Open the device devname with the given baudrate, the protocol
        like '8N1' (default) and the use of the handshake [no_handshake
        (default), rtscts or xonxoff]
        For example:
        At Linux:
        dev = SerialPort()
        dev.Open(\"/dev/ttyS0\",115200)
        or with a datalen of 7 bits, even parity, 2 stopbits and rts/cts
        handshake:
        dev.Open(\"/dev/ttyS0\",115200,'7E2',True)
        At Windows:
        dev = SerialPort()
        dev.Open(\"COM1\",115200)
        dev.Open(\"COM1\",115200,'7E2',True)
        Returns the handle on success or a negativ value on failure.
        """
        # the following parity values are valid:
        # N:None, O:Odd, E:Even, M:Mark, S:Space
        parity = {'N':0,'O':1,'E':2,'M':3,'S':4}
        # the regular expression ensures a valid value for the datalen
        # (5...8 bit) and the count of stopbits (1,2)
        reg=re.compile(r"(?P<w>[8765])"r"(?P<p>[NOEMS])"r"(?P<s>[12])")
        self.device = wxctb.SerialPort()
        dcs = wxctb.SerialPort_DCS()
        dcs.baud = baudrate
        res = reg.search(protocol)
        # handle the given protocol
        if res:
            dcs.wordlen = int(res.group('w'))
            dcs.stopbits = int(res.group('s'))
            dcs.parity = parity[res.group('p')]
        # valid handshake are no one, rts/cts or xon/xoff
        if handshake == 'rtscts':
            dcs.rtscts = True
        elif handshake == 'xonxoff':
            dcs.xonxoff = True
        
        return self.device.Open(devname,dcs)
    
    def Reset(self):
        """
        Send a break for 0.25s.
        """
        self.device.SendBreak(0)

    def SetBaudrate(self,baudrate):
        """
        Set the baudrate for the device.
        """
        self.device.SetBaudrate(baudrate)

    def SetLineState(self,lineState):
        """
        Set the lines given in the linestate parameter. Possible
        values are DTR and/or RTS. For example to set both:
        dev.SetLineState( DTR | RTS)
        """
        self.device.SetLineState(lineState)

    def SetParityBit(self,parity):
        """
        Set the parity bit explicitly to 0 or 1. Use this function, if
        you would like to simulate a 9 bit wordlen at what the ninth bit
        was represented by the parity bit value. For example:
        dev.SetParityBit( 0 )
        dev.Write('some data sent with parity 0')
        dev.SetParityBit( 1 )
        dev.Write('another sequence with parity 1')
        """
        return self.device.SetParityBit( parity )

class GpibDevice(IOBase):
    """
    GPIB class
    """
    def __init__(self):
        IOBase.__init__(self)

    def __del__(self):
        self.Close()

    def FindListeners(self,board = 0):
        """
        Returns the address of the connected devices as a list.
        If no device is listening, the list is empty. If an error
        occurs an IOError exception raised. For example:
        g = GPIB()
        listeners = g.FindListeners()
        """
        listeners = wxctb.GPIB_x_FindListeners(board)
        if listeners < 0:
            raise IOError("GPIB board error")
        result = []
        for i in range(1,31):
            if listeners & (1 << i):
                result.append(i)
        return result

    def GetEosChar(self):
        """
        Get the internal EOS termination character (see SetEosChar).
        For example:
        g = GPIB()
        g.Open(\"gpib1\",1)
        eos = g.GetEosChar()
        """
        eos = wxctb.new_intp()
        wxctb.intp_assign(eos, 0)
        self.device.Ioctl(wxctb.CTB_GPIB_GET_EOS_CHAR,eos)
        return wxctb.intp_value(eos)
        
    def GetEosMode(self):
        """
        Get the internal EOS mode (see SetEosMode).
        For example:
        g = GPIB()
        g.Open(\"gpib1\",1)
        eos = g.GetEosMode()
        """
        mode = wxctb.new_intp()
        wxctb.intp_assign(mode, 0)
        self.device.Ioctl(wxctb.CTB_GPIB_GET_EOS_MODE,mode)
        return wxctb.intp_value(mode)

    def GetError(self):
        errorString = " "*256
        self.device.GetError(errorString,256)
        return errorString

    def GetSTB(self):
        """
        Returns the value of the internal GPIB status byte register.
        """
        stb = wxctb.new_intp()
        wxctb.intp_assign(stb, 0)
        self.device.Ioctl(wxctb.CTB_GPIB_GETRSP,stb)
        return wxctb.intp_value(stb)

    # This is only for internal usage!!!
    def Ibrd(self,length):
        buf = "\x00"*length
        state = self.device.Ibrd(buf,length)
        return state,buf

    # This is only for internal usage!!!
    def Ibwrt(self,string):
        return self.device.Ibwrt(string,len(string))

    def Open(self,devname,adr,eosChar=10,eosMode=0x08|0x04):
        """
        Open(gpibdevice,address,eosChar,eosMode)
        Opens a connected device at the GPIB bus. gpibdevice means the
        controller, (mostly \"gpib1\"), address the address of the desired
        device in the range 1...31. The eosChar defines the EOS character
        (default is linefeed), eosMode may be a combination of bits ORed
        together. The following bits can be used:
        0x04: Terminate read when EOS is detected.
        0x08: Set EOI (End or identify line) with EOS on write function
        0x10: Compare all 8 bits of EOS byte rather than low 7 bits
              (all read and write functions). Default is 0x12
        For example:
        dev = GPIB()
        dev.Open(\"gpib1\",17)
        Opens the device with the address 17, linefeed as EOS (default)
        and eos mode with 0x04 and 0x08.
        Open returns >= 0 or a negativ value, if something going wrong.
        """
        self.device = wxctb.GpibDevice()
        dcs = wxctb.Gpib_DCS()
        dcs.m_address1 = adr
        dcs.m_eosChar = eosChar
        dcs.m_eosMode = eosMode
        result = self.device.Open(devname,dcs)
        return result
    
    def Reset(self):
        """
        Resets the connected device. In the GPIB definition, the device
        should be reset to it's initial state, so you can restart a
        formely lost communication.
        """
        self.device.Ioctl(wxctb.CTB_RESET,None)

    def ResetBus(self):
        """
        The command asserts the GPIB interface clear (IFC) line for
        ast least 100us if the GPIB board is the system controller.
        This initializes the GPIB and makes the interface CIC and
        active controller with ATN asserted.
        Note! The IFC signal resets only the GPIB interface functions
        of the bus devices and not the internal device functions.
        For a device reset you should use the Reset() command above.
        """
        self.device.Ioctl(wxctb.CTB_GPIB_RESET_BUS,None)

    def SetEosChar(self,eos):
        """
        Configure the end-of-string (EOS) termination character.
        Note! Defining an EOS byte does not cause the driver to
        automatically send that byte at the end of write I/O
        operations. The application is responsible for placing the
        EOS byte at the end of the data strings that it defines.
        (National Instruments NI-488.2M Function Reference Manual)
        For example:
        g = GPIB()
        g.Open(\"gpib1\",1)
        eos = g.GetEosChar(0x10)
        """
        intp = wxctb.new_intp()
        wxctb.intp_assign(intp, eos)
        return self.device.Ioctl(wxctb.CTB_GPIB_SET_EOS_CHAR,intp)
        
    def SetEosMode(self,mode):
        """
        Set the EOS mode (handling).m_eosMode may be a combination 
        of bits ORed together. The following bits can be used:
        0x04: Terminate read when EOS is detected.
        0x08: Set EOI (End or identify line) with EOS on write function
        0x10: Compare all 8 bits of EOS byte rather than low 7 bits
        (all read and write functions). For example:
        g = GPIB()
        g.Open(\"gpib1\",1)
        eos = g.GetEosMode(0x04 | 0x08)
        """
        intp = wxctb.new_intp()
        wxctb.intp_assign(intp, mode)
        return self.device.Ioctl(wxctb.CTB_GPIB_SET_EOS_MODE,intp)
        
def GetKey():
    """
    Returns the current pressed key or '\0', if no key is pressed.
    You can simply create a query loop with:
    while GetKey() == '\0':
        ... make some stuff ...

    """
    return wxctb.GetKey()

def GetVersion():
    """
    Returns the version of the ctb python module. The numbering
    has the following format: x.y.z
    x.y means the version of the underlaying ctb lib, z the version
    of the python port.
    """
    return "0.16"
