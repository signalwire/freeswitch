#!/bin/bash

INPUTPATH=./test_vectors/input/
BITSTREAMPATH=./test_vectors/bitstream/
ENC=encoder
COMP=signalcompare

cd ..

# 8 kHz
INPUTFILE=testvector_input_8_kHz.pcm

# 8 kHz, 60 ms, 8 kbps, complexity 0
PARAMS=8_kHz_60_ms_8_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 8000 -packetlength 60 -rate 8000 -complexity 0
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff > test_encoder_report.txt

# 8 kHz, 40 ms, 12 kbps, complexity 1
PARAMS=8_kHz_40_ms_12_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 8000 -packetlength 40 -rate 12000 -complexity 1
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt

# 8 kHz, 20 ms, 20 kbps, 10% packet loss, FEC
PARAMS=8_kHz_20_ms_20_kbps_10_loss_FEC
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 8000 -packetlength 20 -rate 20000 -loss 10 -inbandFEC 1
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt


# 12 kHz
INPUTFILE=testvector_input_12_kHz.pcm

# 12 kHz, 60 ms, 10 kbps, complexity 0
PARAMS=12_kHz_60_ms_10_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 12000 -packetlength 60 -rate 10000 -complexity 0
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt

# 12 kHz, 40 ms, 16 kbps, complexity 1
PARAMS=12_kHz_40_ms_16_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 12000 -packetlength 40 -rate 16000 -complexity 1
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt

# 12 kHz, 20 ms, 24 kbps, 10% packet loss, FEC
PARAMS=12_kHz_20_ms_24_kbps_10_loss_FEC
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 12000 -packetlength 20 -rate 24000 -loss 10 -inbandFEC 1
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt


# 16 kHz
INPUTFILE=testvector_input_16_kHz.pcm

# 16 kHz, 60 ms, 12 kbps, complexity 0
PARAMS=16_kHz_60_ms_12_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 16000 -packetlength 60 -rate 12000 -complexity 0
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt

# 16 kHz, 40 ms, 20 kbps, complexity 1
PARAMS=16_kHz_40_ms_20_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 16000 -packetlength 40 -rate 20000 -complexity 1
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt

# 16 kHz, 20 ms, 32 kbps, 10% packet loss, FEC
PARAMS=16_kHz_20_ms_32_kbps_10_loss_FEC
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 16000 -packetlength 20 -rate 32000 -loss 10 -inbandFEC 1
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt


# 24 kHz
INPUTFILE=testvector_input_24_kHz.pcm

# 24 kHz, 60 ms, 16 kbps, complexity 0
PARAMS=24_kHz_60_ms_16_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 24000 -packetlength 60 -rate 16000 -complexity 0
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt

# 24 kHz, 40 ms, 24 kbps, complexity 1
PARAMS=24_kHz_40_ms_24_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 24000 -packetlength 40 -rate 24000 -complexity 1
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt

# 24 kHz, 20 ms, 40 kbps, 10% packet loss, FEC
PARAMS=24_kHz_20_ms_40_kbps_10_loss_FEC
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 24000 -packetlength 20 -rate 40000 -loss 10 -inbandFEC 1
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt


# 32 kHz
INPUTFILE=testvector_input_32_kHz.pcm

# 32 kHz, 20 ms, 8 kbps, maxInternal 8kHz
PARAMS=32_kHz_max_8_kHz_20_ms_8_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 32000 -Fs_maxInternal 8000 -packetlength 20 -rate 8000
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt


# 44100 Hz
INPUTFILE=testvector_input_44100_Hz.pcm

# 44100 Hz, 20 ms, 40 kbps
PARAMS=44100_Hz_20_ms_7_kbps
./${ENC} ${INPUTPATH}${INPUTFILE} tmp.bit -Fs_API 44100 -packetlength 20 -rate 7000
./${COMP} ${BITSTREAMPATH}payload_${PARAMS}.bit tmp.bit -diff >> test_encoder_report.txt


rm tmp.bit
mv test_encoder_report.txt ./test_vectors/test_encoder_report.txt

echo ""
echo "The results have been saved as test_encoder_report.txt"
echo ""