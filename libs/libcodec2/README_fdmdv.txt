
README_fdmdv.txt
David Rowe
Created March 2012

Introduction
------------

A 1400 bit/s Frequency Division Multiplexed Digital Voice (FDMDV) modem
based on [1].  Used for digital audio over HF SSB.
                     
The FDMDV modem was first implemented in GNU Octave, then ported to C.
Algorithm development is generally easier in Octave, but for real time
work we need the C version.  Automated units tests ensure the
operation of the Octave and C versions are identical.

Quickstart
----------

$ cd codec2-dev
$ ./configure && make
$ cd src

1. Generate some test bits and modulate them:

    $ ./fdmdv_get_test_bits test.c2 1400
    $ ./fdmdv_mod test.c2 test.raw
    $ play -r 8000 -s -2 test.raw
    
2. Two seconds of test frame data modulated and sent out of sound device: 
    
    $ ./fdmdv_get_test_bits - 2800 | ./fdmdv_mod - - | play -t raw -r 8000 -s -2 -
 
3. Send 14000 modulated bits (10 seconds) to the demod and count errors:

    $  ./fdmdv_get_test_bits - 14000 | ./fdmdv_mod - - | ./fdmdv_demod - - demod_dump.txt | ./fdmdv_put_test_bits -

    Use Octave to look at plots of 1 second (1400 bits) of modem operation:

    $ cd ../octave
    $ octave
    octave:1> fdmdv_demod_c("../src/demod_dump.txt",1400)

4. Run Octave simulation of entire modem and AWGN channel:

    $ cd ../octave
    $ octave
    octave:1> fdmdv_ut

5. NOTE: If you would like to play modem samples over the air please
   convert the 8 kHz samples to 48 kHz.  Many PC sound cards have
   wildly inaccurate sample clock rates when set to 8 kHz, but seem to
   perform OK when set for 48 kHz.  If playing and recording files you
   can use the sox utility:

   $ sox -r 8000 -s -2 modem_sample_8kHz.raw -r 48000 modem_sample_48kHz.wav

   For real time applications, the fdmdv.[ch] library includes functions to
   convert between 48 and 8 kHz sample rates.

References
----------

[1] http://n1su.com/fdmdv/FDMDV_Docs_Rel_1_4b.pdf
[2] http://n1su.com/fdmdv/
[3] http://www.rowetel.com/blog/?p=2433 "Testing a FDMDV Modem"
[4] http://www.rowetel.com/blog/?p=2458 "FDMDV Modem Page" on David's web site

C Code
------

src/fdmdv_mod.c - C version of modulator that takes a file of bits and
                  converts it to a raw file of modulated samples.

src/fdmdv_demod.c - C version of demodulator that takes a raw file of
                    modulated samples and outputs a file of bits.
                    Optionally dumps demod states to a text file which
                    can be plotted using the Octave script
                    fdmdv_demod_c.m

src/fdmdv.h - Header file that exposes FDMDV C API functions.  Include
              this file in your application program.

src/fdmdv.c - C functions that implement the FDMDV modem.

src/fdmdv-internal.h - internal states and constants for FDMDV modem,
                       shouldn't be exposed to application program.


unittest/tfdmdv.c - Used to conjunction with unittest/tfdmdv.m to
                    automatically test C FDMDV functions against
                    Octave versions.

Octave Scripts
--------------

(Note these require some Octave packages to be installed, see
octave/README.txt).

fdmdv.m - Functions and variables that implement the Octave version of
          the FDMDV modem.

fdmdv_ut.m - Unit test for fdmdv Octave code, useful while
             developing algorithm.  Includes tx/rx plus basic channel
             simulation.

             Typical run:

               octave:6> fdmdv_ut
               Eb/No (meas): 7.30 (8.29) dB
               bits........: 2464
               errors......: 20
               BER.........: 0.0081
               PAPR........: 13.54 dB
               SNR.........: 4.0 dB

               It also outputs lots of nice plots that show the
	       operation of the modem.

               For a 1400 bit/s DQPSK modem we expect about 1% BER for
               Eb/No = 7.3dB, which corresponds to SNR = 4dB (3kHz
               noise BW). The extra dB of measured power is due to the
               DBPSK pilot. Currently the noise generation code
               doesn't take the pilot power into account, so in this
               example the real SNR is actually 5dB.
             
fdmdv_mod.m - Octave version of modulator that outputs a raw file.
              The modulator is driven by a test frame of bits.  This
              can then be played over a real channel or through a
              channel simulator like PathSim.  The sample rate can be
              changed using "sox" to simulate differences in tx/rx
              sample clocks.

	      To generate 10 seconds of modulated signal:
                
                octave:8> fdmdv_mod("test.raw",1400*10);

fdmdv_demod.m - Demodulator program that takes a raw file as input,
                and works out the bit error rate using the known test
                frame.  Can be used to test the demod performs with
                off-air signals, or signals that have been passed
                through a channel simulator.

		To demodulate 2 seconds of the test.raw file generated
		above:

                octave:9> fdmdv_demod("test.raw",1400*2);
                2464 bits  0 errors  BER: 0.0000

                It also produces several plots showing the internal
		states of the demod.  Useful for debugging and
		observing what happens with various channels.

fdmdv_demod_c.m - Takes an output text file from the C demod
                  fdmdv_demod.c and produces plots and measures BER.
                  Useful for evaluating fdmdv_demod.c performance.
                  The plots produced are identical to the Octave
                  version fdmdv_demod.m, allowing direct comparison of
                  the C and Octave versions.

tfdmdv.m - Automatic tests that compare the Octave and C versions of
           the FDMDV modem functions.  First run unittest/tfdmdv, this
           will generate a text file with test vectors from the C
           version.  Then run the Octave script tfdmdv and it will
           generate Octave versions of the test vectors and compare
           each vector with the C equivalent.  Its plots the vectors
           and and errors (green).  Its also produces an automatic
           check list based on test results.  If the Octave or C modem
           code is changed, this script should be used to ensure the
           C and Octave versions remain identical.

Modelling sample clock errors using sox
---------------------------------------

This introduces a simulated 1000ppm error:

  sox -r 8000 -s -2 mod_dqpsk.raw -s -2 mod_dqpsk_8008hz.raw rate -h 8008

TODO
----

[ ] implement ppm measurements in fdmdv_get_demod_stats()
[ ] try interfering sine wave
    + maybe swept
    + does modem fall over?
[ ] try non-flat channel, e.g. 3dB difference between hi and low tones
    + make sure all estimators keep working
[ ] test rx level sensitivity, i.e. 0 to 20dB attenuation
[ ] make fine freq indep of amplitude
    + use angle rather than imag coord
[ ] document use of fdmdv_ut and fdmdv_demod + PathSim
[ ] more positive form of sync reqd for DV frames?
    + like using coarse_fine==1 to decode valid DV frame bit?
    + when should we start decoding?
[ ] more robust track/acquite state machine?
    + e.g. hang on thru the fades?
[ ] PAPR idea
    + automatically tweak phases to reduce PAPR, e.g. slow variations in freq...
[ ] why is pilot noise_est twice as big as other carriers
