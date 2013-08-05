##
## SpanDSP - a series of DSP components for telephony
##
## Makefile.am - Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License version 2, as
## published by the Free Software Foundation.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

LIBS += $(TESTLIBS)

noinst_DATA = sound_c1_8k.wav sound_c3_8k.wav

EXTRA_DIST = fax_tests.sh \
             regression_tests.sh \
             tsb85_extra_tests.sh \
             tsb85_tests.sh \
             v42bis_tests.sh \
             msvc/adsi_tests.vcproj \
             msvc/complex_tests.vcproj \
             msvc/complex_vector_float_tests.vcproj \
             msvc/complex_vector_int_tests.vcproj \
             msvc/dtmf_rx_tests.vcproj \
             msvc/dtmf_tx_tests.vcproj \
             msvc/queue_tests.vcproj \
             msvc/t38_core_tests.vcproj \
             msvc/t38_non_ecm_buffer_tests.vcproj \
             msvc/v22bis_tests.vcproj \
             msvc/v29_tests.vcproj \
             msvc/v8_tests.vcproj \
             msvc/v80_tests.vcproj \
             msvc/vector_float_tests.vcproj \
             msvc/vector_int_tests.vcproj
 
MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_builddir)/spandsp-sim -DDATADIR="\"$(pkgdatadir)\""

LIBDIR = -L$(top_builddir)/src

noinst_PROGRAMS =   ademco_contactid_tests \
                    adsi_tests \
                    alloc_tests \
                    async_tests \
                    at_interpreter_tests \
                    awgn_tests \
                    bell_mf_rx_tests \
                    bell_mf_tx_tests \
                    bert_tests \
                    bit_operations_tests \
                    bitstream_tests \
                    complex_tests \
                    complex_vector_float_tests \
                    complex_vector_int_tests \
                    crc_tests \
                    dc_restore_tests \
                    dds_tests \
                    dtmf_rx_tests \
                    dtmf_tx_tests \
                    echo_tests \
                    fax_decode \
                    fax_tests \
                    fsk_tests \
                    g1050_tests \
                    g168_tests \
                    g711_tests \
                    g722_tests \
                    g726_tests \
                    gsm0610_tests \
                    hdlc_tests \
                    ima_adpcm_tests \
                    image_translate_tests \
                    line_model_tests \
                    logging_tests \
                    lpc10_tests \
                    math_fixed_tests \
                    make_g168_css \
                    modem_connect_tones_tests \
                    modem_echo_tests \
                    noise_tests \
                    oki_adpcm_tests \
                    playout_tests \
                    plc_tests \
                    power_meter_tests \
                    queue_tests \
                    r2_mf_rx_tests \
                    r2_mf_tx_tests \
                    rfc2198_sim_tests \
                    saturated_tests \
                    schedule_tests \
                    sig_tone_tests \
                    super_tone_rx_tests \
                    super_tone_tx_tests \
                    swept_tone_tests \
                    t31_pseudo_terminal_tests \
                    t31_tests \
                    t35_tests \
                    t38_core_tests \
                    t38_decode \
                    t38_non_ecm_buffer_tests \
                    t4_tests \
                    t4_t6_tests \
                    t42_tests \
                    t43_tests \
                    t81_t82_arith_coding_tests \
                    t85_tests \
                    time_scale_tests \
                    timezone_tests \
                    tone_detect_tests \
                    tone_generate_tests \
                    tsb85_tests \
                    v17_tests \
                    v18_tests \
                    v22bis_tests \
                    v27ter_tests \
                    v29_tests \
                    v42_tests \
                    v42bis_tests \
                    v8_tests \
                    vector_float_tests \
                    vector_int_tests

noinst_HEADERS =    echo_monitor.h \
                    fax_tester.h \
                    fax_utils.h \
                    line_model_monitor.h \
                    media_monitor.h \
                    modem_monitor.h \
                    pcap_parse.h \
                    udptl.h

ademco_contactid_tests_SOURCES = ademco_contactid_tests.c
ademco_contactid_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

adsi_tests_SOURCES = adsi_tests.c
adsi_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

alloc_tests_SOURCES = alloc_tests.c
alloc_tests_LDADD = $(LIBDIR) -lspandsp

async_tests_SOURCES = async_tests.c
async_tests_LDADD = $(LIBDIR) -lspandsp

at_interpreter_tests_SOURCES = at_interpreter_tests.c
at_interpreter_tests_LDADD = $(LIBDIR) -lspandsp

awgn_tests_SOURCES = awgn_tests.c
awgn_tests_LDADD = $(LIBDIR) -lspandsp

bell_mf_rx_tests_SOURCES = bell_mf_rx_tests.c
bell_mf_rx_tests_LDADD = $(LIBDIR) -lspandsp

bell_mf_tx_tests_SOURCES = bell_mf_tx_tests.c
bell_mf_tx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

bert_tests_SOURCES = bert_tests.c
bert_tests_LDADD = $(LIBDIR) -lspandsp

bit_operations_tests_SOURCES = bit_operations_tests.c
bit_operations_tests_LDADD = $(LIBDIR) -lspandsp

bitstream_tests_SOURCES = bitstream_tests.c
bitstream_tests_LDADD = $(LIBDIR) -lspandsp

complex_tests_SOURCES = complex_tests.c
complex_tests_LDADD = $(LIBDIR) -lspandsp

complex_vector_float_tests_SOURCES = complex_vector_float_tests.c
complex_vector_float_tests_LDADD = $(LIBDIR) -lspandsp

complex_vector_int_tests_SOURCES = complex_vector_int_tests.c
complex_vector_int_tests_LDADD = $(LIBDIR) -lspandsp

crc_tests_SOURCES = crc_tests.c
crc_tests_LDADD = $(LIBDIR) -lspandsp

dc_restore_tests_SOURCES = dc_restore_tests.c
dc_restore_tests_LDADD = $(LIBDIR) -lspandsp

dds_tests_SOURCES = dds_tests.c
dds_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

dtmf_rx_tests_SOURCES = dtmf_rx_tests.c
dtmf_rx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

dtmf_tx_tests_SOURCES = dtmf_tx_tests.c
dtmf_tx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

echo_tests_SOURCES = echo_tests.c echo_monitor.cpp
echo_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

fax_decode_SOURCES = fax_decode.c
fax_decode_LDADD = $(LIBDIR) -lspandsp

fax_tests_SOURCES = fax_tests.c fax_utils.c media_monitor.cpp
fax_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

fsk_tests_SOURCES = fsk_tests.c
fsk_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

g1050_tests_SOURCES = g1050_tests.c media_monitor.cpp
g1050_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

g168_tests_SOURCES = g168_tests.c
g168_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

g711_tests_SOURCES = g711_tests.c
g711_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

g722_tests_SOURCES = g722_tests.c
g722_tests_LDADD = $(LIBDIR) -lspandsp

g726_tests_SOURCES = g726_tests.c
g726_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

gsm0610_tests_SOURCES = gsm0610_tests.c
gsm0610_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

hdlc_tests_SOURCES = hdlc_tests.c
hdlc_tests_LDADD = $(LIBDIR) -lspandsp

ima_adpcm_tests_SOURCES = ima_adpcm_tests.c
ima_adpcm_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

image_translate_tests_SOURCES = image_translate_tests.c
image_translate_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

line_model_tests_SOURCES = line_model_tests.c
line_model_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

logging_tests_SOURCES = logging_tests.c
logging_tests_LDADD = $(LIBDIR) -lspandsp

lpc10_tests_SOURCES = lpc10_tests.c
lpc10_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

math_fixed_tests_SOURCES = math_fixed_tests.c
math_fixed_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

make_g168_css_SOURCES = make_g168_css.c
make_g168_css_LDADD = $(LIBDIR) -lspandsp

modem_echo_tests_SOURCES = modem_echo_tests.c echo_monitor.cpp
modem_echo_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

modem_connect_tones_tests_SOURCES = modem_connect_tones_tests.c
modem_connect_tones_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

noise_tests_SOURCES = noise_tests.c
noise_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

oki_adpcm_tests_SOURCES = oki_adpcm_tests.c
oki_adpcm_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

playout_tests_SOURCES = playout_tests.c media_monitor.cpp
playout_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

plc_tests_SOURCES = plc_tests.c
plc_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

power_meter_tests_SOURCES = power_meter_tests.c
power_meter_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

queue_tests_SOURCES = queue_tests.c
queue_tests_LDADD = $(LIBDIR) -lspandsp

r2_mf_rx_tests_SOURCES = r2_mf_rx_tests.c
r2_mf_rx_tests_LDADD = $(LIBDIR) -lspandsp

r2_mf_tx_tests_SOURCES = r2_mf_tx_tests.c
r2_mf_tx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

rfc2198_sim_tests_SOURCES = rfc2198_sim_tests.c media_monitor.cpp
rfc2198_sim_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

saturated_tests_SOURCES = saturated_tests.c
saturated_tests_LDADD = $(LIBDIR) -lspandsp 

schedule_tests_SOURCES = schedule_tests.c
schedule_tests_LDADD = $(LIBDIR) -lspandsp 

sig_tone_tests_SOURCES = sig_tone_tests.c
sig_tone_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp 

super_tone_rx_tests_SOURCES = super_tone_rx_tests.c
super_tone_rx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

super_tone_tx_tests_SOURCES = super_tone_tx_tests.c
super_tone_tx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

swept_tone_tests_SOURCES = swept_tone_tests.c
swept_tone_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

t31_pseudo_terminal_tests_SOURCES = t31_pseudo_terminal_tests.c fax_utils.c pseudo_terminals.c
t31_pseudo_terminal_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim -lspandsp -lutil

t31_tests_SOURCES = t31_tests.c fax_utils.c media_monitor.cpp
t31_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

t35_tests_SOURCES = t35_tests.c
t35_tests_LDADD = $(LIBDIR) -lspandsp

t38_core_tests_SOURCES = t38_core_tests.c
t38_core_tests_LDADD = $(LIBDIR) -lspandsp

t38_decode_SOURCES = t38_decode.c fax_utils.c pcap_parse.c udptl.c
t38_decode_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

t38_non_ecm_buffer_tests_SOURCES = t38_non_ecm_buffer_tests.c
t38_non_ecm_buffer_tests_LDADD = $(LIBDIR) -lspandsp

t4_tests_SOURCES = t4_tests.c
t4_tests_LDADD = $(LIBDIR) -lspandsp

t4_t6_tests_SOURCES = t4_t6_tests.c
t4_t6_tests_LDADD = $(LIBDIR) -lspandsp

t42_tests_SOURCES = t42_tests.c
t42_tests_LDADD = $(LIBDIR) -lspandsp

t43_tests_SOURCES = t43_tests.c
t43_tests_LDADD = $(LIBDIR) -lspandsp

t81_t82_arith_coding_tests_SOURCES = t81_t82_arith_coding_tests.c
t81_t82_arith_coding_tests_LDADD = $(LIBDIR) -lspandsp

t85_tests_SOURCES = t85_tests.c
t85_tests_LDADD = $(LIBDIR) -lspandsp

time_scale_tests_SOURCES = time_scale_tests.c
time_scale_tests_LDADD = $(LIBDIR) -lspandsp

timezone_tests_SOURCES = timezone_tests.c
timezone_tests_LDADD = $(LIBDIR) -lspandsp

tone_detect_tests_SOURCES = tone_detect_tests.c
tone_detect_tests_LDADD = $(LIBDIR) -lspandsp

tone_generate_tests_SOURCES = tone_generate_tests.c
tone_generate_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

tsb85_tests_SOURCES = tsb85_tests.c fax_utils.c fax_tester.c
tsb85_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

v17_tests_SOURCES = v17_tests.c line_model_monitor.cpp modem_monitor.cpp
v17_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

v18_tests_SOURCES = v18_tests.c
v18_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

v22bis_tests_SOURCES = v22bis_tests.c line_model_monitor.cpp modem_monitor.cpp
v22bis_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

v27ter_tests_SOURCES = v27ter_tests.c line_model_monitor.cpp modem_monitor.cpp
v27ter_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

v29_tests_SOURCES = v29_tests.c line_model_monitor.cpp modem_monitor.cpp
v29_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

v42_tests_SOURCES = v42_tests.c
v42_tests_LDADD = $(LIBDIR) -lspandsp

v42bis_tests_SOURCES = v42bis_tests.c
v42bis_tests_LDADD = $(LIBDIR) -lspandsp

v8_tests_SOURCES = v8_tests.c
v8_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(LIBDIR) -lspandsp

vector_float_tests_SOURCES = vector_float_tests.c
vector_float_tests_LDADD = $(LIBDIR) -lspandsp

vector_int_tests_SOURCES = vector_int_tests.c
vector_int_tests_LDADD = $(LIBDIR) -lspandsp

# We need to create the CSS files for echo cancellation tests.

sound_c1_8k.wav sound_c3_8k.wav: make_g168_css$(EXEEXT)
	./make_g168_css$(EXEEXT)
	sox sound_c1.wav -r8000 sound_c1_8k.wav
	sox sound_c3.wav -r8000 sound_c3_8k.wav
	rm sound_c1.wav sound_c3.wav
