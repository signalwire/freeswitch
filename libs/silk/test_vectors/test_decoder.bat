@echo off

SET BITSTREAMPATH=./test_vectors/bitstream/
SET OUTPUTPATH=./test_vectors/output/
SET DEC=Decoder.exe
SET COMP=SignalCompare.exe

cd ..

:: 8 kHz

:: 8 kHz, 60 ms, 8 kbps, complexity 0
SET PARAMS=8_kHz_60_ms_8_kbps
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm 
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm -fs 24000 > test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 8000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_8_kHz_out.pcm tmp.pcm -fs 8000 >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 12000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_12_kHz_out.pcm tmp.pcm -fs 12000 >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 16000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_16_kHz_out.pcm tmp.pcm -fs 16000 >> test_decoder_report.txt

:: 8 kHz, 40 ms, 12 kbps, complexity 1
SET PARAMS=8_kHz_40_ms_12_kbps
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm 
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt

:: 8 kHz, 20 ms, 20 kbps, 10% packet loss, FEC
SET PARAMS=8_kHz_20_ms_20_kbps_10_loss_FEC
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -loss 10
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt


:: 12 kHz

:: 12 kHz, 60 ms, 10 kbps, complexity 0
SET PARAMS=12_kHz_60_ms_10_kbps
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm 
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 12000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_12_kHz_out.pcm tmp.pcm -fs 12000 >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 16000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_16_kHz_out.pcm tmp.pcm -fs 16000 >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 32000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_32_kHz_out.pcm tmp.pcm -fs 32000 >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 44100
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_44100_Hz_out.pcm tmp.pcm -fs 44100 >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 48000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_48_kHz_out.pcm tmp.pcm -fs 48000 >> test_decoder_report.txt

:: 12 kHz, 40 ms, 16 kbps, complexity 1
SET PARAMS=12_kHz_40_ms_16_kbps
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm 
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt

:: 12 kHz, 20 ms, 24 kbps, 10% packet loss, FEC
SET PARAMS=12_kHz_20_ms_24_kbps_10_loss_FEC
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -loss 10
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt


:: 16 kHz

:: 16 kHz, 60 ms, 12 kbps, complexity 0
SET PARAMS=16_kHz_60_ms_12_kbps
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm 
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 16000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_16_kHz_out.pcm tmp.pcm -fs 16000 >> test_decoder_report.txt

:: 16 kHz, 40 ms, 20 kbps, complexity 1
SET PARAMS=16_kHz_40_ms_20_kbps
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm 
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt

:: 16 kHz, 20 ms, 32 kbps, 10% packet loss, FEC
SET PARAMS=16_kHz_20_ms_32_kbps_10_loss_FEC
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -loss 10
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt


:: 24 kHz

:: 24 kHz, 60 ms, 16 kbps, complexity 0
SET PARAMS=24_kHz_60_ms_16_kbps
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm 
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt

:: 24 kHz, 40 ms, 24 kbps, complexity 1
SET PARAMS=24_kHz_40_ms_24_kbps
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm 
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt

:: 24 kHz, 20 ms, 40 kbps, 10% packet loss, FEC
SET PARAMS=24_kHz_20_ms_40_kbps_10_loss_FEC
%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -loss 10
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt


:: 32 kHz

:: 32 kHz, 20 ms, 8 kbps, maxInternal 8kHz
SET PARAMS=32_kHz_max_8_kHz_20_ms_8_kbps

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 32000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_32_kHz_out.pcm tmp.pcm -fs 32000 >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 44100
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_44100_Hz_out.pcm tmp.pcm -fs 44100 >> test_decoder_report.txt

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm -Fs_API 48000
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%_48_kHz_out.pcm tmp.pcm -fs 48000 >> test_decoder_report.txt


:: 44100 Hz

:: 44100 Hz, 20 ms, 40 kbps
SET PARAMS=44100_Hz_20_ms_7_kbps

%DEC% %BITSTREAMPATH%payload_%PARAMS%.bit tmp.pcm
%COMP% %OUTPUTPATH%testvector_output_%PARAMS%.pcm tmp.pcm >> test_decoder_report.txt


del tmp.pcm
move test_decoder_report.txt ./test_vectors/test_decoder_report.txt

echo.
echo The results have been saved as test_decoder_report.txt
echo.

pause
