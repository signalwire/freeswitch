/*
  fl_fdmdv.cxx
  Created 14 June 2012
  David Rowe

  Fltk 1.3 based GUI program to prototype FDMDV & Codec 2 integration
  issues such as:

    + spectrum, waterfall, and other FDMDV GUI displays
    + integration with real time audio I/O using portaudio
    + what we do with audio when out of sync
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Group.H>
#include <FL/names.h>

#include "portaudio.h"

#include "fdmdv.h"
#include "codec2.h"

#define MIN_DB             -40.0 
#define MAX_DB               0.0
#define BETA                 0.1  // constant for time averageing spectrum data
#define MIN_HZ               0
#define MAX_HZ            4000
#define WATERFALL_SECS_Y     5    // number of seconds respresented by y axis of waterfall
#define DT                   0.02 // time between samples 
#define FS                8000    // FDMDV modem sample rate

#define SCATTER_MEM       (FDMDV_NSYM)*50
#define SCATTER_X_MAX        3.0
#define SCATTER_Y_MAX        3.0

// main window params

#define W                  1200
#define W3                 (W/3)
#define H                  600
#define H2                 (H/2)
#define SP                  20

// sound card

#define SAMPLE_RATE  48000                        /* 48 kHz sampling rate rec. as we
				                     can trust accuracy of sound
				                     card                                    */
#define N8           FDMDV_NOM_SAMPLES_PER_FRAME  /* processing buffer size at 8 kHz         */
#define MEM8 (FDMDV_OS_TAPS/FDMDV_OS)
#define N48          (N8*FDMDV_OS)                /* processing buffer size at 48 kHz        */
#define NUM_CHANNELS 2                            /* I think most sound cards prefer stereo,
				                     we will convert to mono                 */

#define BITS_PER_CODEC_FRAME (2*FDMDV_BITS_PER_FRAME)
#define BYTES_PER_CODEC_FRAME (BITS_PER_CODEC_FRAME/8)

// forward class declarations

class Spectrum;
class Waterfall;
class Scatter;
class Scalar;

// Globals --------------------------------------

char         *fin_name = NULL;
char         *fout_name = NULL;
char         *sound_dev_name = NULL;
FILE         *fin = NULL;
FILE         *fout = NULL;
struct FDMDV *fdmdv;
struct CODEC2 *codec2;
float         av_mag[FDMDV_NSPEC]; // shared between a few classes

// GUI variables --------------------------------

Fl_Group     *agroup;
Fl_Window    *window;
Fl_Window    *zoomSpectrumWindow = NULL;
Fl_Window    *zoomWaterfallWindow = NULL;
Spectrum     *aSpectrum;
Spectrum     *aZoomedSpectrum;
Waterfall    *aWaterfall;
Waterfall    *aZoomedWaterfall;
Scatter      *aScatter;
Scalar       *aTimingEst;
Scalar       *aFreqEst;
Scalar       *aSNR;
int          zoom_spectrum = 0;

// Main processing loop states ------------------

float  Ts = 0.0;
short  input_buf[2*FDMDV_NOM_SAMPLES_PER_FRAME];
int    n_input_buf = 0;
int    nin = FDMDV_NOM_SAMPLES_PER_FRAME;
short *output_buf;
int    n_output_buf = 0;
int    codec_bits[2*FDMDV_BITS_PER_FRAME];
int    state = 0;

// Portaudio states -----------------------------

PaStream *stream = NULL;
PaError err;

typedef struct {
    float               in48k[FDMDV_OS_TAPS + N48];
    float               in8k[MEM8 + N8];
} paCallBackData;

// Class for each window type  ------------------

class Spectrum: public Fl_Box {
protected:
    int handle(int event) {

	//  detect a left mouse down if inside the spectrum window

	if ((event == FL_NO_EVENT) && (Fl::event_button() == 1)) {
	    if ((Fl::event_x() > x()) && (Fl::event_x() < (x() + w())) &&
		(Fl::event_y() > y()) && (Fl::event_y() < (y() + h()))) {

		// show zoomed spectrum window

		zoomSpectrumWindow->show();
	    }
	    
	}
	return 0;
    }

    void draw() {
	float x_px_per_point = 0.0;
	float y_px_per_dB = 0.0;
	int   i, x1, y1, x2, y2;
	float mag1, mag2;
	char  label[20];
	float px_per_hz;

	Fl_Box::draw();
	fl_color(FL_BLACK);
	fl_rectf(x(),y(),w(),h());
	fl_color(FL_GREEN);
	fl_line_style(FL_SOLID);

	fl_push_clip(x(),y(),w(),h());
	//printf("%d %d\n", w(), h());
	x_px_per_point = (float)w()/FDMDV_NSPEC;
	y_px_per_dB = (float)h()/(MAX_DB - MIN_DB);

	// plot spectrum

	for(i=0; i<FDMDV_NSPEC-1; i++) {
	    mag1 = av_mag[i];
	    mag2 = av_mag[i+1];

	    x1 = x() + i*x_px_per_point;
	    y1 = y() + -mag1*y_px_per_dB;
	    x2 = x() + (i+1)*x_px_per_point;
	    y2 = y() + -mag2*y_px_per_dB;
	    fl_line(x1,y1,x2,y2);   
	}

	// y axis graticule

	fl_line_style(FL_DOT);
	for(i=MIN_DB; i<MAX_DB; i+=10) {
	    x1 = x();
	    y1 = y() + -i*y_px_per_dB;
	    x2 = x() + w();
	    y2 = y1;
	    //printf("%d %d %d %d\n", x1, y1, x2, y2);
	    fl_line(x1,y1,x2,y2);   
	    sprintf(label, "%d", i);
	    fl_draw(label, x1, y1);
	}

	// x axis graticule

	px_per_hz = (float)w()/(MAX_HZ-MIN_HZ);
	fl_line_style(FL_DOT);
	for(i=500; i<MAX_HZ; i+=500) {
	    x1 = x() + i*px_per_hz;
	    y1 = y();
	    x2 = x1;
	    y2 = y() + h();
	    //printf("i=%d %d %d %d %d\n", i, x1, y1, x2, y2);
	    fl_line(x1,y1,x2,y2);   
	    sprintf(label, "%d", i);
	    fl_draw(label, x1, y2);
	}

	fl_pop_clip();
    }

public:
    Spectrum(int x, int y, int w, int h): Fl_Box(x, y, w, h, "Spectrum")
    {
	align(FL_ALIGN_TOP);
	labelsize(10);
    };

};


/*

  Notes:

  The height h() pixels represents WATERFALL_SECS_Y of data.  Every DT
  seconds we get a vector of FDMDV_NSPEC spectrum samples which we use
  to update the last row.  The height of each row is dy pixels, which
  maps to DT seconds.  We call each dy high rectangle of pixels a
  block.

*/

class Waterfall: public Fl_Box {
protected:

    int       prev_w, prev_h;
    unsigned *pixel_buf;
    unsigned  heatmap_lut[256];
    int       greyscale;

    void new_pixel_buf(int w, int h) {
	int buf_sz, i;

	prev_w = w; prev_h = h;
	buf_sz = h*w;
	pixel_buf = new unsigned[buf_sz];
	for(i=0; i<buf_sz; i++)
	    pixel_buf[i] = 0;
    }
    
    int handle(int event) {

	//  detect a left mouse down if inside the window

	if ((event == FL_NO_EVENT) && (Fl::event_button() == 1)) {
	    if ((Fl::event_x() > x()) && (Fl::event_x() < (x() + w())) &&
		(Fl::event_y() > y()) && (Fl::event_y() < (y() + h()))) {

		// show zoomed spectrum window

		zoomWaterfallWindow->show();
	    }
	    
	}
	return 0;
    }

    // map val to a rgb colour
    // from http://eddiema.ca/2011/01/21/c-sharp-heatmaps/

    unsigned heatmap(float val, float min, float max) {
	unsigned r = 0;
	unsigned g = 0;
	unsigned b = 0;

	val = (val - min) / (max - min);
	if(val <= 0.2) {
	    b = (unsigned)((val / 0.2) * 255);
	} else if(val >  0.2 &&  val <= 0.7) {
	    b = (unsigned)((1.0 - ((val - 0.2) / 0.5)) * 255);
	}
	if(val >= 0.2 &&  val <= 0.6) {
	    g = (unsigned)(((val - 0.2) / 0.4) * 255);
	} else if(val >  0.6 &&  val <= 0.9) {
	    g = (unsigned)((1.0 - ((val - 0.6) / 0.3)) * 255);
	}
	if(val >= 0.5) {
	    r = (unsigned)(((val - 0.5) / 0.5) * 255);
	}
    
	//printf("%f %x %x %x\n", val, r, g, b);

	return  (b << 16) + (g << 8) + r;
    }

    void draw() {
	float  spec_index_per_px, intensity_per_dB;
	int    px_per_sec;
	int    index, dy, dy_blocks, bytes_in_row_of_blocks, b;
	int    px, py, intensity;
	unsigned *last_row, *pdest, *psrc;

	/* detect resizing of window */

	if ((h() != prev_h) || (w() != prev_w)) {
	    delete pixel_buf;
	    new_pixel_buf(w(), h());
	}

	Fl_Box::draw();

	// determine dy, the height of one "block"

	px_per_sec = (float)h()/WATERFALL_SECS_Y;
	dy = DT*px_per_sec;

	// number of dy high blocks in spectrogram

	dy_blocks = h()/dy;

	// shift previous bit map
					       
	bytes_in_row_of_blocks = dy*w()*sizeof(unsigned);

	for(b=0; b<dy_blocks-1; b++) {
	    pdest = pixel_buf + b*w()*dy;
	    psrc  = pixel_buf + (b+1)*w()*dy;
	    memcpy(pdest, psrc, bytes_in_row_of_blocks);
	}

	// create a new row of blocks at bottom

	spec_index_per_px = (float)FDMDV_NSPEC/(float)w();
	intensity_per_dB = (float)256/(MAX_DB - MIN_DB);
	last_row = pixel_buf + dy*(dy_blocks - 1)*w();

	for(px=0; px<w(); px++) {
	    index = px*spec_index_per_px;
	    intensity = intensity_per_dB * (av_mag[index] - MIN_DB);
	    if (intensity > 255) intensity = 255;
	    if (intensity < 0) intensity = 0;

	    if (greyscale) {
		for(py=0; py<dy; py++)
		    last_row[px+py*w()] = intensity<<8;
	    }
	    else {
		for(py=0; py<dy; py++)
		    last_row[px+py*w()] = heatmap_lut[intensity];
	    }
	}

	// update bit map

	fl_draw_image((uchar*)pixel_buf, x(), y(), w(), h(), 4, 0);

    }

public:

    Waterfall(int x, int y, int w, int h): Fl_Box(x, y, w, h, "Waterfall")
    {
	int   i;

	for(i=0; i<255; i++) {
	    heatmap_lut[i] = heatmap((float)i, 0.0, 255.0);
	}
	greyscale = 0;

	align(FL_ALIGN_TOP);
	labelsize(10);
	new_pixel_buf(w,h);
    };

    ~Waterfall() {
	delete pixel_buf;
    }
};


class Scatter: public Fl_Box {
protected:
    COMP mem[SCATTER_MEM];
    COMP new_samples[FDMDV_NSYM];
    int  prev_w, prev_h, prev_x, prev_y;

    void draw() {
	float x_scale;
	float y_scale;
	int   i, j, x1, y1;

	Fl_Box::draw();

	/* detect resizing of window */

	if ((h() != prev_h) || (w() != prev_w) || (x() != prev_x) || (y() != prev_y)) {
	    fl_color(FL_BLACK);
	    fl_rectf(x(),y(),w(),h());
	    prev_h = h(); prev_w = w(); prev_x = x(); prev_y = y();
	}

	fl_push_clip(x(),y(),w(),h());

	x_scale = w()/SCATTER_X_MAX;
	y_scale = h()/SCATTER_Y_MAX;

	// erase last samples

	fl_color(FL_BLACK);
	for(i=0; i<FDMDV_NSYM; i++) {
	    x1 = x_scale * mem[i].real + x() + w()/2;
	    y1 = y_scale * mem[i].imag + y() + h()/2;
	    fl_point(x1, y1);
	    mem[i] = mem[i+FDMDV_NSYM];
	}

	// shift memory

	for(i=FDMDV_NSYM; i<SCATTER_MEM-FDMDV_NSYM; i++) {
	    mem[i] = mem[i+FDMDV_NSYM];
	}

	// draw new samples

	fl_color(FL_GREEN);
	for(i=SCATTER_MEM-FDMDV_NSYM, j=0; i<SCATTER_MEM; i++,j++) {
	    x1 = x_scale * new_samples[j].real + x() + w()/2;
	    y1 = y_scale * new_samples[j].imag + y() + h()/2;
	    fl_point(x1, y1);
	    mem[i] = new_samples[j];
	}
	fl_pop_clip();
    }

public:
    Scatter(int x, int y, int w, int h): Fl_Box(x, y, w, h, "Scatter")
    {
	int i;

	align(FL_ALIGN_TOP);
	labelsize(10);

	for(i=0; i<SCATTER_MEM; i++) {
	    mem[i].real = 0.0;
	    mem[i].imag = 0.0;
	}

	prev_w = 0; prev_h = 0; prev_x = 0; prev_y = 0;
    };

    void add_new_samples(COMP samples[]) {
	int i;

	for(i=0; i<FDMDV_NSYM; i++)
	    new_samples[i] = samples[i];
    }

};


// general purpose way of plotting scalar values that are 
// updated once per frame

class Scalar: public Fl_Box {
protected:
    int    x_max, y_max;
    float *mem;              /* array of x_max samples */
    float  new_sample;
    int    index, step;
    int    prev_w, prev_h, prev_x, prev_y;

    int clip(int y1) {
	if (y1 > (h()/2 - 10))
	    y1 = h()/2 - 10;       
	if (y1 < -(h()/2 - 10))
	    y1 = -(h()/2 - 10);       
	return y1;
    }

    void draw() {
	float x_scale;
	float y_scale;
	int   i, x1, y1, x2, y2;
	char  label[100];

	Fl_Box::draw();

	/* detect resizing of window */

	if ((h() != prev_h) || (w() != prev_w) || (x() != prev_x) || (y() != prev_y)) {
	    fl_color(FL_BLACK);
	    fl_rectf(x(),y(),w(),h());
	    prev_h = h(); prev_w = w(); prev_x = x(); prev_y = y();
	}

	fl_push_clip(x(),y(),w(),h());

	x_scale = (float)w()/x_max;
	y_scale = (float)h()/(2.0*y_max);

	// erase last sample

	fl_color(FL_BLACK);
	x1 = x_scale * index + x();
	y1 = y_scale * mem[index];
	y1 = clip(y1);
	y1 = y() + h()/2 - y1;
	fl_point(x1, y1);

	// draw new sample

	fl_color(FL_GREEN);
	x1 = x_scale * index + x();
	y1 = y_scale * new_sample;
	y1 = clip(y1);
	y1 = y() + h()/2 - y1;
	fl_point(x1, y1);
	mem[index] = new_sample;

	index++;
	if (index >=  x_max)
	    index = 0;

	// y axis graticule

	step = 10;

	while ((2.0*y_max/step) > 10)
	    step *= 2.0;
	while ((2.0*y_max/step) < 4)
	    step /= 2.0;

	fl_color(FL_DARK_GREEN);
	fl_line_style(FL_DOT);
	for(i=-y_max; i<y_max; i+=step) {
	    x1 = x();
	    y1 = y() + h()/2 - i*y_scale;
	    x2 = x() + w();
	    y2 = y1;
	    fl_line(x1,y1,x2,y2);   
	}

	// y axis graticule labels

	fl_color(FL_GREEN);
	fl_line_style(FL_SOLID);
	for(i=-y_max; i<y_max; i+=step) {
	    x1 = x();
	    y1 = y() + h()/2 - i*y_scale;
	    sprintf(label, "%d", i);
	    fl_draw(label, x1, y1);
	}
	fl_pop_clip();
    }

public:
    Scalar(int x, int y, int w, int h, int x_max_, int y_max_, const char name[]): Fl_Box(x, y, w, h, name)
    {
	int i;

	align(FL_ALIGN_TOP);
	labelsize(10);

	x_max = x_max_; y_max = y_max_;
	mem = new float[x_max];
	for(i=0; i<x_max; i++) {
	    mem[i] = 0.0;
	}

	prev_w = 0; prev_h = 0; prev_x = 0; prev_y = 0;
	index = 0;
    };

    ~Scalar() {
	delete mem;
    }

    void add_new_sample(float sample) {
	new_sample = sample;
    }

};


// update average of each spectrum point
    
void new_data(float mag_dB[]) {
    int i;

    for(i=0; i<FDMDV_NSPEC; i++)
	av_mag[i] = (1.0 - BETA)*av_mag[i] + BETA*mag_dB[i];
}


/*------------------------------------------------------------------*\

  FUNCTION: per_frame_rx_processing()
  AUTHOR..: David Rowe
  DATE....: July 2012
  
  Called every rx frame to take a buffer of input modem samples and
  convert them to a buffer of output speech samples.

  The sample source could be a sound card or file.  The sample source
  supplies a fixed number of samples with each call.  However
  fdmdv_demod requires a variable number of samples for each call.
  This function will buffer as appropriate and call fdmdv_demod with
  the correct number of samples.

  The processing sequence is:

  collect demod input samples from sound card 1 A/D
  while we have enough samples:
    demod samples into bits
    decode bits into speech samples
    output a buffer of speech samples to sound card 2 D/A

  Note that sound card 1 and sound card 2 will have slightly different
  sample rates, as their sample clocks are not syncronised.  We
  effectively lock the system to the demod A/D (sound card 1) sample
  rate. This ensures the demod gets a continuous sequence of samples,
  maintaining sync. Sample underflow or overflow will instead occur on
  the sound card 2 D/A.  This is acceptable as a buffer of lost or
  extra speech samples is unlikely to be noticed.

  The situation is actually a little more complex than that.  Through
  the demod timing estimation the buffers supplied to sound card D/A 2
  are effectively clocked at the remote modulator sound card D/A clock
  rate.  We slip/gain buffers supplied to sound card 2 to compensate.

  The current demod handles varying clock rates by having a variable
  number of input samples, e.g. 120 160 (nominal) or 200.  However the
  A/D always delivers a fixed number of samples.

  So we currently need some logic between the A/D and the demod:
    + A/D delivers fixed number of samples
    + demod processes a variable number of samples
    + this means we run demod 0,1 or 2 times, depending 
      on number of buffered A/D samples
    + demod always outputs 1 frame of bits
    + so run demod and speech decoder 0, 1 or 2 times
  
  The ouput of the demod is codec voice data so it's OK if we miss or
  repeat a frame every now and again.

  TODOs:

    + this might work with arbitrary input and output buffer lengths,
    0,1, or 2 only apply if we are inputting the nominal number of
    samples on every call.

    + so the I/O buffer sizes might not matter, as long as they of
    reasonable size (say twice the nominal size).

\*------------------------------------------------------------------*/

void per_frame_rx_processing(short  output_buf[], /* output buf of decoded speech samples          */
			     int   *n_output_buf, /* how many samples currently in output_buf[]    */
                             int    codec_bits[], /* current frame of bits for decoder             */
			     short  input_buf[],  /* input buf of modem samples input to demod     */ 
			     int   *n_input_buf,  /* how many samples currently in input_buf[]     */
			     int   *nin,          /* amount of samples demod needs for next call   */
			     int   *state,        /* used to collect codec_bits[] halves           */
			     struct CODEC2 *c2    /* Codec 2 states                                */
			     )
{
    struct FDMDV_STATS stats;
    int    sync_bit;
    float  rx_fdm[FDMDV_MAX_SAMPLES_PER_FRAME];
    int    rx_bits[FDMDV_BITS_PER_FRAME];
    uchar  packed_bits[BYTES_PER_CODEC_FRAME];
    float  rx_spec[FDMDV_NSPEC];
    int    i, nin_prev, bit, byte;
    int    next_state;

    assert(*n_input_buf <= (2*FDMDV_NOM_SAMPLES_PER_FRAME));    
   
    /*
      This while loop will run the demod 0, 1 (nominal) or 2 times:

      0: when tx sample clock runs faster than rx, occasionally we
         will run out of samples

      1: normal, run decoder once, every 2nd frame output a frame of
         speech samples to D/A

      2: when tx sample clock runs slower than rx, occasionally we will
         have enough samples to run demod twice.

      With a +/- 10 Hz sample clock difference at FS=8000Hz (+/- 1250
      ppm), case 0 or 1 occured about once every 30 seconds.  This is
      no problem for the decoded audio.
    */

    while(*n_input_buf >= *nin) {

	// demod per frame processing

	for(i=0; i<*nin; i++)
	    rx_fdm[i] = (float)input_buf[i]/FDMDV_SCALE;
	nin_prev = *nin;
	fdmdv_demod(fdmdv, rx_bits, &sync_bit, rx_fdm, nin);
	*n_input_buf -= nin_prev;
	assert(*n_input_buf >= 0);

	// shift input buffer

	for(i=0; i<*n_input_buf; i++)
	    input_buf[i] = input_buf[i+nin_prev];

	// compute rx spectrum & get demod stats, and update GUI plot data

	fdmdv_get_rx_spectrum(fdmdv, rx_spec, rx_fdm, nin_prev);
	fdmdv_get_demod_stats(fdmdv, &stats);
	new_data(rx_spec);
	aScatter->add_new_samples(stats.rx_symbols);
	aTimingEst->add_new_sample(stats.rx_timing);
	aFreqEst->add_new_sample(stats.foff);
	aSNR->add_new_sample(stats.snr_est);

	/* 
	   State machine to:

	   + Mute decoded audio when out of sync.  The demod is synced
	     when we are using the fine freq estimate and SNR is above
	     a thresh.

	   + Decode codec bits only if we have a 0,1 sync bit
	     sequence.  Collects two frames of demod bits to decode
	     one frame of codec bits.
	*/

	next_state = *state;
	switch (*state) {
	case 0:
	    /* mute output audio when out of sync */

	    if (*n_output_buf < 2*codec2_samples_per_frame(c2) - N8) {
		for(i=0; i<N8; i++)
		    output_buf[*n_output_buf + i] = 0;
		*n_output_buf += N8;
	    }
	    assert(*n_output_buf <= (2*codec2_samples_per_frame(c2)));  

	    if ((stats.fest_coarse_fine == 1) && (stats.snr_est > 3.0))
		next_state = 1;

	    break;
	case 1:
	    if (sync_bit == 0) {
		next_state = 2;

		/* first half of frame of codec bits */

		memcpy(codec_bits, rx_bits, FDMDV_BITS_PER_FRAME*sizeof(int));
	    }
	    else
		next_state = 1;
	    
	    if (stats.fest_coarse_fine == 0)
		next_state = 0;

	    break;
	case 2:
	    next_state = 1;

	    if (stats.fest_coarse_fine == 0)
		next_state = 0;

	    if (sync_bit == 1) {
		/* second half of frame of codec bits */

		memcpy(&codec_bits[FDMDV_BITS_PER_FRAME], rx_bits, FDMDV_BITS_PER_FRAME*sizeof(int));

		/* pack bits, MSB received first  */

		bit = 7; byte = 0;
		memset(packed_bits, 0, BYTES_PER_CODEC_FRAME);
		for(i=0; i<BITS_PER_CODEC_FRAME; i++) {
		    packed_bits[byte] |= (codec_bits[i] << bit);
		    bit--;
		    if (bit < 0) {
			bit = 7;
			byte++;
		    }
		}
		assert(byte == BYTES_PER_CODEC_FRAME);

		/* add decoded speech to end of output buffer */

		if (*n_output_buf <= codec2_samples_per_frame(c2)) {
		    codec2_decode(c2, &output_buf[*n_output_buf], packed_bits);
		    *n_output_buf += codec2_samples_per_frame(c2);
		}
		assert(*n_output_buf <= (2*codec2_samples_per_frame(c2)));  
		
	    }
	    break;
	}	
	*state = next_state;
    }
}


/* 
   Redraw windows every DT seconds.
*/

void update_gui(int nin, float *Ts) {

    *Ts += (float)nin/FS;
	
    *Ts += (float)nin/FS;
    if (*Ts >= DT) {
	*Ts -= DT;
	if (!zoomSpectrumWindow->shown() && !zoomWaterfallWindow->shown()) {
	    aSpectrum->redraw();
	    aWaterfall->redraw();
	    aScatter->redraw();
	    aTimingEst->redraw();
	    aFreqEst->redraw();
	    aSNR->redraw();
	}
	if (zoomSpectrumWindow->shown())		
	    aZoomedSpectrum->redraw();		
	if (zoomWaterfallWindow->shown())		
	    aZoomedWaterfall->redraw();		
    }
}


/*
  idle() is the FLTK function that gets continusouly called when FLTK
  is not doing GUI work.  We use this function for providing file
  input to update the GUI when simulating real time operation.
*/

void idle(void*) {
    int ret, i;

    if (fin_name != NULL) {
	ret = fread(&input_buf[n_input_buf], 
		    sizeof(short), 
		    FDMDV_NOM_SAMPLES_PER_FRAME, 
		    fin);
	n_input_buf += FDMDV_NOM_SAMPLES_PER_FRAME;             

	per_frame_rx_processing(output_buf, &n_output_buf,
				codec_bits,
				input_buf, &n_input_buf, 
				&nin, &state, codec2);

	if (fout_name != NULL) {
	    if (n_output_buf >= N8) {
		ret = fwrite(output_buf, sizeof(short), N8, fout);
		n_output_buf -= N8;
		assert(n_output_buf >= 0);
		
		/* shift speech sample output buffer */

		for(i=0; i<n_output_buf; i++)
		    output_buf[i] = output_buf[i+N8];
	    }
	}
    }

    update_gui(nin, &Ts);

    // simulate time delay from real world A/D input, and pause betwen
    // screen updates

    usleep(20000);
}


/* 
   This routine will be called by the PortAudio engine when audio is
   available.
*/

static int callback( const void *inputBuffer, void *outputBuffer,
		     unsigned long framesPerBuffer,
		     const PaStreamCallbackTimeInfo* timeInfo,
		     PaStreamCallbackFlags statusFlags,
		     void *userData )
{
    paCallBackData *cbData = (paCallBackData*)userData;
    uint        i;
    short      *rptr = (short*)inputBuffer;
    short      *wptr = (short*)outputBuffer;
    float      *in8k = cbData->in8k;
    float      *in48k = cbData->in48k;
    float       out8k[N8];
    float       out48k[N48];
    short       out48k_short[N48];

    (void) timeInfo;
    (void) statusFlags;

    assert(inputBuffer != NULL);

    /* Convert input model samples from 48 to 8 kHz ------------ */

    /* just use left channel */

    for(i=0; i<framesPerBuffer; i++,rptr+=2)
	in48k[i+FDMDV_OS_TAPS] = *rptr; 

    /* downsample and update filter memory */

    fdmdv_48_to_8(out8k, &in48k[FDMDV_OS_TAPS], N8);
    for(i=0; i<FDMDV_OS_TAPS; i++)
	in48k[i] = in48k[i+framesPerBuffer];

    /* run demod, decoder and update GUI info ------------------ */

    for(i=0; i<N8; i++)
	input_buf[n_input_buf+i] = (short)out8k[i];
    n_input_buf += FDMDV_NOM_SAMPLES_PER_FRAME;             

    per_frame_rx_processing(output_buf, &n_output_buf,
			    codec_bits,
			    input_buf, &n_input_buf, 
			    &nin, &state, codec2);

    /* if demod out of sync copy input audio from A/D to aid in tuning */

    if (n_output_buf >= N8) {
       if (state == 0) {
	   for(i=0; i<N8; i++)
	       in8k[MEM8+i] = out8k[i];       /* A/D signal */
       }
       else {
	   for(i=0; i<N8; i++)
	       in8k[MEM8+i] = output_buf[i];  /* decoded spech */
       }
       n_output_buf -= N8;
    }
    assert(n_output_buf >= 0);

    /* shift speech samples in output buffer */

    for(i=0; i<(uint)n_output_buf; i++)
	output_buf[i] = output_buf[i+N8];

    /* Convert output speech to 48 kHz sample rate ------------- */

    /* upsample and update filter memory */

    fdmdv_8_to_48(out48k, &in8k[MEM8], N8);
    for(i=0; i<MEM8; i++)
	in8k[i] = in8k[i+N8];

    assert(outputBuffer != NULL);

    /* write signal to both channels */

    for(i=0; i<N48; i++)
	out48k_short[i] = (short)out48k[i];
    for(i=0; i<framesPerBuffer; i++,wptr+=2) {
	wptr[0] = out48k_short[i]; 
	wptr[1] = out48k_short[i]; 
    }

    return paContinue;
}


int arg_callback(int argc, char **argv, int &i) {
    if (argv[i][1] == 'i') {
	if ((i+1) >= argc) 
	    return 0;
	fin_name = argv[i+1];
	i += 2;
	return 2;
    }
    if (argv[i][1] == 'o') {
	if ((i+1) >= argc) 
	    return 0;
	fout_name = argv[i+1];
	i += 2;
	return 2;
    }
    if (argv[i][1] == 's') {
	if ((i+1) >= argc) 
	    return 0;
	sound_dev_name = argv[i+1];
	i += 2;
	return 2;
    }
    return 0;
}

/*------------------------------------------------------------*\

                                 MAIN

\*------------------------------------------------------------*/

int main(int argc, char **argv) {
    int                ret;
    int                i;
    PaStreamParameters inputParameters, outputParameters;
    paCallBackData     cbData;

    i = 1;
    Fl::args(argc,argv,i,arg_callback);

    if (argc == 1) {
	printf("usage: %s [-i inputFdmdvRawFile] [-o outputRawSoundFile] [-s inputSoundDevice]\n", argv[0]);
	exit(0);
    }

    if (fin_name != NULL) {
	fin = fopen(fin_name,"rb");
	if (fin == NULL) {
	    fprintf(stderr, "Error opening input fdmdv raw file %s\n", fin_name);
	    exit(1);
	}
    }
    
    if (fout_name != NULL) {
	fout = fopen(fout_name,"wb");
	if (fout == NULL) {
	    fprintf(stderr, "Error opening output speech raw file %s\n", fout_name);
	    exit(1);
	}
    }
    
    for(i=0; i<FDMDV_NSPEC; i++)
	av_mag[i] = -40.0;

    fdmdv = fdmdv_create();
    codec2 = codec2_create(CODEC2_MODE_1400);
    output_buf = (short*)malloc(2*sizeof(short)*codec2_samples_per_frame(codec2));

    /*------------------------------------------------------------------------*\

                           Init Sound Card I/O

    \*------------------------------------------------------------------------*/

    if (sound_dev_name != NULL) {
	for(i=0; i<MEM8; i++)
	    cbData.in8k[i] = 0.0;
	for(i=0; i<FDMDV_OS_TAPS; i++)
	    cbData.in48k[i] = 0.0;

	err = Pa_Initialize();
	if( err != paNoError ) goto pa_error;
	inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
	if (inputParameters.device == paNoDevice) {
	    fprintf(stderr,"Error: No default input device.\n");
	    goto pa_error;
	}
	inputParameters.channelCount =  NUM_CHANNELS;        /* stereo input */
	inputParameters.sampleFormat = paInt16;
	inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
	inputParameters.hostApiSpecificStreamInfo = NULL;

	outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
	if (outputParameters.device == paNoDevice) {
	    fprintf(stderr,"Error: No default output device.\n");
	    goto pa_error;
	}
	outputParameters.channelCount = NUM_CHANNELS;         /* stereo output */
	outputParameters.sampleFormat = paInt16;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;

	err = Pa_OpenStream(
			    &stream,
			    &inputParameters,
			    &outputParameters,                  
			    SAMPLE_RATE,
			    N48,
			    paClipOff,      
			    callback,
			    &cbData);

	if( err != paNoError ) goto pa_error;
    }

    /*------------------------------------------------------------------------*\

                                 Init GUI

    \*------------------------------------------------------------------------*/

    // recommended to prevent dithering and stopped display being
    // covered by black flickering squares

    Fl::visual(FL_RGB);

    // set up main window

    window = new Fl_Window(W, SP+H2+SP+SP+H2+SP, "fl_fmdv");
    //window->size_range(100, 100);
    //window->resizable();
    aSpectrum = new Spectrum(SP, SP, W3-2*SP, H2);
    aWaterfall = new Waterfall(SP, SP+H2+SP+SP, W3-2*SP, H2);
    aScatter = new Scatter(W3+SP, SP, W3-2*SP, H2);
    aTimingEst = new Scalar(W3+SP, SP+H2+SP+SP, W3-2*SP, H2, 100, 80, "Timing Est");
    aFreqEst = new Scalar(2*W3+SP, SP, W3-2*SP, H2, 100, 100, "Frequency Est");
    aSNR = new Scalar(2*W3+SP, SP+H2+SP+SP, W3-2*SP, H2, 100, 20, "SNR");

    Fl::add_idle(idle);

    window->end();

    // set up zoomed spectrum window

    zoomSpectrumWindow = new Fl_Window(W, H, "Spectrum");
    aZoomedSpectrum = new Spectrum(SP, SP, W-2*SP, H-2*SP);
    zoomSpectrumWindow->end();

    // set up zoomed waterfall window

    zoomWaterfallWindow = new Fl_Window(W, H, "Waterfall");
    aZoomedWaterfall = new Waterfall(SP, SP, W-2*SP, H-2*SP);
    zoomWaterfallWindow->end();

    if (sound_dev_name != NULL) {
	err = Pa_StartStream( stream );
	if( err != paNoError ) goto pa_error;
    }

    // show the main window and start running

    window->show(argc, argv);
    Fl::run();

    if (sound_dev_name != NULL) {
	err = Pa_StopStream( stream );
	if( err != paNoError ) goto pa_error;
	Pa_CloseStream( stream );
	Pa_Terminate();
    }

    fdmdv_destroy(fdmdv);
    codec2_destroy(codec2);
    free(output_buf);

    if (fin_name != NULL)
	fclose(fin);
    if (fout_name != NULL)
	fclose(fout);

    return ret;

    // Portaudio error handling

pa_error:
    if( stream ) {
       Pa_AbortStream( stream );
       Pa_CloseStream( stream );
    }
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return -1;
}
