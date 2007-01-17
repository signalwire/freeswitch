Hello

  This is a small list of steps in order to build portaudio
(Currently v19-devel) into a VC6 DLL and lib file.
This DLL contains all 3 current win32 PA APIS (MM/DS/ASIO)

1)Copy the source dirs that comes with the ASIO SDK inside src\hostapi\asio\ASIOSDK
  so you should now have example:
  
  portaudio19svn\src\hostapi\asio\ASIOSDK\common
  portaudio19svn\src\hostapi\asio\ASIOSDK\host
  portaudio19svn\src\hostapi\asio\ASIOSDK\host\sample
  portaudio19svn\src\hostapi\asio\ASIOSDK\host\pc
  portaudio19svn\src\hostapi\asio\ASIOSDK\host\mac (not needed)
  
  You dont need "driver"
  

2)If you have Visual Studio 6.0, 7.0(VC.NET/2001) or 7.1(VC.2003) 
  then I suggest you open portaudio.dsp (and convert if needed)
 
  If you have Visual Studio 2005, I suggest you open the portaudio.sln file
  which contains 4 configurations. Win32/x64 in both Release and Debug variants

  hit compile and hope for the best.
 
3)Now in any  project, in which you require portaudio,
  you can just link with portaudio_x86.lib, (or _x64) and of course include the 
  relevant headers
  (portaudio.h, and/or pa_asio.h , pa_x86_plain_converters.h) See (*)
  
4) Your new exe should now use portaudio_xXX.dll.


Have fun!

(*): you may want to add/remove some DLL entry points.
Right now those 6 entries are _not_ from portaudio.h

(from portaudio.def)
(...)
PaAsio_GetAvailableLatencyValues    @50
PaAsio_ShowControlPanel             @51
PaUtil_InitializeX86PlainConverters @52
PaAsio_GetInputChannelName          @53
PaAsio_GetOutputChannelName         @54
PaUtil_SetLogPrintFunction          @55

-----
David Viens, davidv@plogue.com