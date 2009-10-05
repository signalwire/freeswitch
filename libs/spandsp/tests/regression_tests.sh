#!/bin/sh
#
# SpanDSP - a series of DSP components for telephony
#
# regression_tests.sh
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 2.1,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: regression_tests.sh,v 1.59 2009/09/22 13:28:18 steveu Exp $
#

ITUTESTS_TIF=../test-data/itu/fax/itutests.tif
MIXEDSIZES_TIF=../test-data/itu/fax/mixed_size_pages.tif
STDOUT_DEST=xyzzy
STDERR_DEST=xyzzy2

echo Performing basic spandsp regression tests
echo

./adsi_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo adsi_tests failed!
    exit $RETVAL
fi
echo adsi_tests completed OK

./async_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo async_tests failed!
    exit $RETVAL
fi
echo async_tests completed OK

./at_interpreter_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo at_interpreter_tests failed!
    exit $RETVAL
fi
echo at_interpreter_tests completed OK

./awgn_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo awgn_tests failed!
    exit $RETVAL
fi
echo awgn_tests completed OK

./bell_mf_rx_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo bell_mf_rx_tests failed!
    exit $RETVAL
fi
echo bell_mf_rx_tests completed OK

./bell_mf_tx_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo bell_mf_tx_tests failed!
    exit $RETVAL
fi
echo bell_mf_tx_tests completed OK

./bert_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo bert_tests failed!
    exit $RETVAL
fi
echo bert_tests completed OK

./bit_operations_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo bit_operations_tests failed!
    exit $RETVAL
fi
echo bit_operations_tests completed OK

./complex_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo complex_tests failed!
    exit $RETVAL
fi
echo complex_tests completed OK

./complex_vector_float_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo complex_vector_float_tests failed!
    exit $RETVAL
fi
echo complex_vector_float_tests completed OK

./complex_vector_int_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo complex_vector_int_tests failed!
    exit $RETVAL
fi
echo complex_vector_int_tests completed OK

./crc_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo crc_tests failed!
    exit $RETVAL
fi
echo crc_tests completed OK

./dc_restore_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo dc_restore_tests failed!
    exit $RETVAL
fi
echo dc_restore_tests completed OK

./dds_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo dds_tests failed!
    exit $RETVAL
fi
echo dds_tests completed OK

./dtmf_rx_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo dtmf_rx_tests failed!
    exit $RETVAL
fi
echo dtmf_rx_tests completed OK

./dtmf_tx_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo dtmf_tx_tests failed!
    exit $RETVAL
fi
echo dtmf_tx_tests completed OK

#./echo_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo echo_tests failed!
#    exit $RETVAL
#fi
#echo echo_tests completed OK
echo echo_tests not enabled

#Try the ITU test pages without ECM
rm -f fax_tests_1.tif
./fax_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo fax_tests failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} fax_tests_1.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo fax_tests failed!
    exit $RETVAL
fi
#Try the ITU test pages with ECM
rm -f fax_tests_1.tif
./fax_tests -e >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo fax_tests -e failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} fax_tests_1.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo fax_tests -e failed!
    exit $RETVAL
fi
#Try some mixed sized test pages without ECM
rm -f fax_tests_1.tif
./fax_tests -i ${MIXEDSIZES_TIF} >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo fax_tests failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${MIXEDSIZES_TIF} fax_tests_1.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo fax_tests failed!
    exit $RETVAL
fi
echo fax_tests completed OK

./fsk_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo fsk_tests failed!
    exit $RETVAL
fi
echo fsk_tests completed OK

#./g1050_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo g1050_tests failed!
#    exit $RETVAL
#fi
#echo g1050_tests completed OK

./g711_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo g711_tests failed!
    exit $RETVAL
fi
echo g711_tests completed OK

./g722_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo g722_tests failed!
    exit $RETVAL
fi
echo g722_tests completed OK

./g726_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo g726_tests failed!
    exit $RETVAL
fi
echo g726_tests completed OK

./gsm0610_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo gsm0610_tests failed!
    exit $RETVAL
fi
echo gsm0610_tests completed OK

./hdlc_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo hdlc_tests failed!
    exit $RETVAL
fi
echo hdlc_tests completed OK

./ima_adpcm_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ima_adpcm_tests failed!
    exit $RETVAL
fi
echo ima_adpcm_tests completed OK

./logging_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo logging_tests failed!
    exit $RETVAL
fi
echo logging_tests completed OK

./lpc10_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo lpc10_tests failed!
    exit $RETVAL
fi
echo lpc10_tests completed OK

./modem_echo_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo modem_echo_tests failed!
    exit $RETVAL
fi
echo modem_echo_tests completed OK

./modem_connect_tones_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo modem_connect_tones_tests failed!
    exit $RETVAL
fi
echo modem_connect_tones_tests completed OK

./noise_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo noise_tests failed!
    exit $RETVAL
fi
echo noise_tests completed OK

./oki_adpcm_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo oki_adpcm_tests failed!
    exit $RETVAL
fi
echo oki_adpcm_tests completed OK

#./playout_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo playout_tests failed!
#    exit $RETVAL
#fi
#echo playout_tests completed OK
echo playout_tests not enabled

#./plc_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo plc_tests failed!
#    exit $RETVAL
#fi
#echo plc_tests completed OK
echo plc_tests not enabled

./power_meter_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo power_meter_tests failed!
    exit $RETVAL
fi
echo power_meter_tests completed OK

./queue_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo queue_tests failed!
    exit $RETVAL
fi
echo queue_tests completed OK

./r2_mf_rx_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo r2_mf_rx_tests failed!
    exit $RETVAL
fi
echo r2_mf_rx_tests completed OK

./r2_mf_tx_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo r2_mf_tx_tests failed!
    exit $RETVAL
fi
echo r2_mf_tx_tests completed OK

#./rfc2198_sim_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo rfc2198_sim_tests failed!
#    exit $RETVAL
#fi
#echo rfc2198_sim_tests completed OK

./schedule_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo schedule_tests failed!
    exit $RETVAL
fi
echo schedule_tests completed OK

#./sig_tone_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo sig_tone_tests failed!
#    exit $RETVAL
#fi
#echo sig_tone_tests completed OK
echo sig_tone_tests not enabled

#./super_tone_rx_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo super_tone_rx_tests failed!
#    exit $RETVAL
#fi
#echo super_tone_rx_tests completed OK
echo super_tone_rx_tests not enabled

#./super_tone_tx_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo super_tone_tx_tests failed!
#    exit $RETVAL
#fi
#echo super_tone_tx_tests completed OK
echo super_tone_tx_tests not enabled

#./swept_tone_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo swept_tone_tests failed!
#    exit $RETVAL
#fi
#echo swept_tone_tests completed OK
echo swept_tone_tests not enabled

./t31_tests -r >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t31_tests -r failed!
    exit $RETVAL
fi
./t31_tests -s >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t31_tests -s failed!
    exit $RETVAL
fi
echo t31_tests completed OK

./t38_core_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_core_tests failed!
    exit $RETVAL
fi
echo t38_core_tests completed OK

rm -f t38.tif
./t38_gateway_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_gateway_tests failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} t38.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_gateway_tests failed!
    exit $RETVAL
fi
rm -f t38.tif
./t38_gateway_tests -e >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_gateway_tests -e failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} t38.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_gateway_tests -e failed!
    exit $RETVAL
fi
echo t38_gateway_tests completed OK

rm -f t38.tif
./t38_gateway_to_terminal_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_gateway_to_terminal_tests failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} t38.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_gateway_to_terminal_tests failed!
    exit $RETVAL
fi
rm -f t38.tif
./t38_gateway_to_terminal_tests -e >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_gateway_to_terminal_tests -e failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} t38.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_gateway_to_terminal_tests -e failed!
    exit $RETVAL
fi
echo t38_gateway_to_terminal_tests completed OK

./t38_non_ecm_buffer_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_non_ecm_buffer_tests failed!
    exit $RETVAL
fi
echo t38_non_ecm_buffer_tests completed OK

rm -f t38.tif
./t38_terminal_to_gateway_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_terminal_to_gateway_tests failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} t38.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_terminal_to_gateway_tests failed!
    exit $RETVAL
fi
rm -f t38.tif
./t38_terminal_to_gateway_tests -e >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_terminal_to_gateway_tests -e failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} t38.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_terminal_to_gateway_tests -e failed!
    exit $RETVAL
fi
echo t38_terminal_to_gateway_tests completed OK

rm -f t38.tif
./t38_terminal_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_terminal_tests failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} t38.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_terminal_tests failed!
    exit $RETVAL
fi
rm -f t38.tif
./t38_terminal_tests -e >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_terminal_tests -e failed!
    exit $RETVAL
fi
# Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
# option means the normal differences in tags will be ignored.
tiffcmp -t ${ITUTESTS_TIF} t38.tif >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_terminal_tests -e failed!
    exit $RETVAL
fi
echo t38_terminal_tests completed OK

rm -f t4_tests_receive.tif
./t4_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t4_tests failed!
    exit $RETVAL
fi
echo t4_tests completed OK

#./time_scale_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo time_scale_tests failed!
#    exit $RETVAL
#fi
#echo time_scale_tests completed OK
echo time_scale_tests not enabled

#./tone_detect_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo tone_detect_tests failed!
#    exit $RETVAL
#fi
#echo tone_detect_tests completed OK
echo tone_detect_tests not enabled

#./tone_generate_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo tone_generate_tests failed!
#    exit $RETVAL
#fi
#echo tone_generate_tests completed OK
echo tone_generate_tests not enabled

./v17_tests -b 14400 -s -42 -n -66 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v17_tests failed!
    exit $RETVAL
fi
./v17_tests -b 12000 -s -42 -n -61 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v17_tests failed!
    exit $RETVAL
fi
./v17_tests -b 9600 -s -42 -n -59 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v17_tests failed!
    exit $RETVAL
fi
./v17_tests -b 7200 -s -42 -n -56 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v17_tests failed!
    exit $RETVAL
fi
echo v17_tests completed OK

#./v22bis_tests -b 2400 >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo v22bis_tests failed!
#    exit $RETVAL
#fi
#./v22bis_tests -b 1200 >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo v22bis_tests failed!
#    exit $RETVAL
#fi
#echo v22bis_tests completed OK
echo v22bis_tests not enabled

./v27ter_tests -b 4800 -s -42 -n -57 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v27ter_tests failed!
    exit $RETVAL
fi
./v27ter_tests -b 2400 -s -42 -n -51 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v27ter_tests failed!
    exit $RETVAL
fi
echo v27ter_tests completed OK

./v29_tests -b 9600 -s -42 -n -62 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v29_tests failed!
    exit $RETVAL
fi
./v29_tests -b 7200 -s -42 -n -58 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v29_tests failed!
    exit $RETVAL
fi
./v29_tests -b 4800 -s -42 -n -54 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v29_tests failed!
    exit $RETVAL
fi
echo v29_tests completed OK

#./v42_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo v42_tests failed!
#    exit $RETVAL
#fi
#echo v42_tests completed OK
echo v42_tests not enabled

./v42bis_tests.sh >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v42bis_tests failed!
    exit $RETVAL
fi
echo v42bis_tests completed OK

./v8_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v8_tests failed!
    exit $RETVAL
fi
echo v8_tests completed OK

./v18_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v18_tests failed!
    exit $RETVAL
fi
echo v18_tests completed OK

./vector_float_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo vector_float_tests failed!
    exit $RETVAL
fi
echo vector_float_tests completed OK

./vector_int_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo vector_int_tests failed!
    exit $RETVAL
fi
echo vector_int_tests completed OK

echo
echo All regression tests successfully completed
