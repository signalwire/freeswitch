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

ITUTESTS_TIF=../test-data/itu/fax/itutests.tif
MIXEDSIZES_TIF=../test-data/itu/fax/mixed_size_pages.tif
STDOUT_DEST=xyzzy
STDERR_DEST=xyzzy2

echo Performing basic spandsp regression tests
echo

./ademco_contactid_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ademco_contactid_tests failed!
    exit $RETVAL
fi
echo ademco_contactid_tests completed OK

./adsi_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo adsi_tests failed!
    exit $RETVAL
fi
echo adsi_tests completed OK

./alloc_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo alloc_tests failed!
    exit $RETVAL
fi
echo alloc_tests completed OK

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

./fax_tests.sh
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo fax_tests.sh failed!
    exit $RETVAL
fi
echo fax_tests.sh completed OK

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

./image_translate_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo image_translate_tests failed!
    exit $RETVAL
fi
echo image_translate_tests completed OK

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

./math_fixed_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo math_fixed_tests failed!
    exit $RETVAL
fi
echo math_fixed_tests completed OK

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

./saturated_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo saturated_tests failed!
    exit $RETVAL
fi
echo saturated_tests completed OK

./schedule_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo schedule_tests failed!
    exit $RETVAL
fi
echo schedule_tests completed OK

./sig_tone_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo sig_tone_tests failed!
    exit $RETVAL
fi
echo sig_tone_tests completed OK

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

./swept_tone_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo swept_tone_tests failed!
    exit $RETVAL
fi
echo swept_tone_tests completed OK

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

./t35_tests -s >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t35_tests -s failed!
    exit $RETVAL
fi
echo t35_tests completed OK

./t38_core_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_core_tests failed!
    exit $RETVAL
fi
echo t38_core_tests completed OK

./t38_non_ecm_buffer_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t38_non_ecm_buffer_tests failed!
    exit $RETVAL
fi
echo t38_non_ecm_buffer_tests completed OK

rm -f t4_tests_receive.tif
./t4_tests -b 0 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t4_tests failed!
    exit $RETVAL
fi
rm -f t4_tests_receive.tif
./t4_tests -b 1 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t4_tests failed!
    exit $RETVAL
fi
rm -f t4_tests_receive.tif
./t4_tests -b 10 >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t4_tests failed!
    exit $RETVAL
fi
echo t4_tests completed OK

rm -f t4_t6_tests_receive.tif
./t4_t6_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t4_t6_tests failed!
    exit $RETVAL
fi
echo t4_t6_tests completed OK

rm -f t81_t82_arith_coding_tests_receive.tif
./t81_t82_arith_coding_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t81_t82_arith_coding_tests failed!
    exit $RETVAL
fi
echo t81_t82_arith_coding_tests completed OK

rm -f t85_tests_receive.tif
./t85_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo t85_tests failed!
    exit $RETVAL
fi
echo t85_tests completed OK

#./time_scale_tests >$STDOUT_DEST 2>$STDERR_DEST
#RETVAL=$?
#if [ $RETVAL != 0 ]
#then
#    echo time_scale_tests failed!
#    exit $RETVAL
#fi
#echo time_scale_tests completed OK
echo time_scale_tests not enabled

./timezone_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo timezone_tests failed!
    exit $RETVAL
fi
echo timezone_tests completed OK

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

./tsb85_tests.sh >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ./tsb85_tests.sh failed!
    exit $RETVAL
fi
./tsb85_extra_tests.sh
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ./tsb85_extra_tests.sh failed!
    exit $RETVAL
fi
echo tsb85_tests.sh completed OK

for OPTS in "-b 14400 -s -42 -n -66" "-b 12000 -s -42 -n -61" "-b 9600 -s -42 -n -59" "-b 7200 -s -42 -n -56"
do
    ./v17_tests ${OPTS} >$STDOUT_DEST 2>$STDERR_DEST
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo v17_tests ${OPTS} failed!
        exit $RETVAL
    fi
done
echo v17_tests completed OK

#for OPTS in "-b 2400" "-b 1200"
#do
#    ./v22bis_tests ${OPTS} >$STDOUT_DEST 2>$STDERR_DEST
#    RETVAL=$?
#    if [ $RETVAL != 0 ]
#    then
#        echo v22bis_tests ${OPTS} failed!
#        exit $RETVAL
#    fi
#done
#echo v22bis_tests completed OK
echo v22bis_tests not enabled

for OPTS in "-b 4800 -s -42 -n -57" "-b 2400 -s -42 -n -51"
do
    ./v27ter_tests ${OPTS} >$STDOUT_DEST 2>$STDERR_DEST
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo v27ter_tests ${OPTS} failed!
        exit $RETVAL
    fi
done
echo v27ter_tests completed OK

for OPTS in "-b 9600 -s -42 -n -62" "-b 7200 -s -42 -n -59" "-b 4800 -s -42 -n -54"
do
    ./v29_tests ${OPTS} >$STDOUT_DEST 2>$STDERR_DEST
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo v29_tests ${OPTS} failed!
        exit $RETVAL
    fi
done
echo v29_tests completed OK

#for OPTS in "-b 14400 -s -42 -n -66" "-b 12000 -s -42 -n -61" "-b 9600 -s -42 -n -59" "-b 7200 -s -42 -n -56"
#do
#    ./v32bis_tests ${OPTS} >$STDOUT_DEST 2>$STDERR_DEST
#    RETVAL=$?
#    if [ $RETVAL != 0 ]
#    then
#        echo v32bis_tests ${OPTS} failed!
#        exit $RETVAL
#    fi
#done
#echo v32bis_tests completed OK
echo v32bis_tests not enabled

./v42_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v42_tests failed!
    exit $RETVAL
fi
echo v42_tests completed OK

./v42bis_tests.sh >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo v42bis_tests.sh failed!
    exit $RETVAL
fi
echo v42bis_tests.sh completed OK

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
