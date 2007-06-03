I have only tested the library with samples at 8kHz.  It *should* work with arbitrary sample rates; the "optr" variable in the code samples below is the sampling rate in Hz.

The first thing you need to do is initialize a context structure:

	dsp_fsk_attr_t		fsk1200_attr;				// attributes structure for FSK 1200 baud modem
	dsp_fsk_handle_t	*fsk1200_handle;			// context structure for FSK 1200 baud modem

	// initialize:
	dsp_fsk_attr_init (&fsk1200_attr);				// clear attributes structure
	dsp_fsk_attr_set_samplerate (&fsk1200_attr, optr);	// set sample rate
	dsp_fsk_attr_set_bytehandler (&fsk1200_attr, clid_byte_handler, ch);	// bind byte handler

	// create context:
	fsk1200_handle = dsp_fsk_create (&fsk1200_attr);

	// error check:
	if (fsk1200_handle == NULL) {
		fprintf (stderr, "%s:  can't dsp_fsk_create, errno %d (%s)\n", progname, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

If you are decoding multiple channels, you will need multiple context structures; one per channel.
The attributes ("dsp_fsk_attr_t") do not have to be persistent, but the handle does.
There's even a "dsp_fsk_destroy()" function call to remove the context structure; I don't believe I've ever used it, my stuff hangs around forever.

Then, you need to feed samples into the software modem:
	dsp_fsk_sample (fsk1200_handle, (double) sample / 32767.);

It assumes the samples are between -1 and 1 as a double.

It will outcall to your provided "clid_byte_handler()" function, which needs to have a prototype as follows:

	void clid_byte_handler (void *x, int data);

The "x" is a context "void *" that you can associate in the fsk1200_handle (great for multiple channels).

This will dribble bytes out to you at 1200 baud.

From there, you are on your own.  Canadian caller ID streams are different format than US caller ID streams IIRC.  Both are trivial to figure out, as they have lots of ASCII data.  I can supply some more code later on the Canadian one if you need it.

Here's a sample of a Canadian caller ID stream from when you called:

00000000:  55 55 55 55 55 55 55 55-55 55 55 55 55 55 55 55  UUUUUUUUUUUUUUUU
00000010:  55 55 D5 80 1D 01 08 30-36 30 31 31 36 31 36 08  UU.....06011616.
00000020:  01 4F 03 0B 31 32 34 38-35 35 35 31 32 31 32 06  .O..12485551212.
00000030:  01 4C D1 85 00 02 54 00-02 C5 60 28 10 0A 80 30  .L....T...`(...0

The "UUUUU" is an alternating series of ones and zeros (which, when combined with the start and stop bits, creates the 0x55 character "U") used to train analog modems.
06011616 is the date and time (JUN 01 16:16), the "O" at 0x21 is an "Out of area"
qualifier for why your name didn't appear, and "1248..." is your DN.  The "L" at 0x31 is a long distance qualifier.
