/*******************************************************
 * jitterbuffer: 
 * an application-independent jitterbuffer, which tries 
 * to achieve the maximum user perception during a call.
 * For more information look at:
 * http://www.speakup.nl/opensource/jitterbuffer/
 *
 * Copyright on this file is held by:
 * - Jesse Kaijen <jesse@speakup.nl>  
 * - SpeakUp <info@speakup.nl>
 *
 * Contributors:
 * Jesse Kaijen <jesse@speakup.nl>
 *
 * Version: 1.1 (2006-03-24)
 * 
 * Changelog:
 * 1.0 => 1.1 (2006-03-24) (thanks to Micheal Jerris, freeswitch.org) 
 * - added MSVC 2005 project files 
 * - removed compile warnings (forced floating point)
 * - fixed minor bug in setting jb->target
 * - added JB_NOJB as return value
 * - added version numbering
 *
 * This program is free software, distributed under the terms of:
 * - the GNU Lesser (Library) General Public License
 * - the Mozilla Public License
 * 
 * if you are interested in an different licence type, please contact us.
 *
 * How to use the jitterbuffer, please look at the comments 
 * in the headerfile.
 *
 * Further details on specific implementations, 
 * please look at the comments in the code file.
 */

#include "jitterbuffer.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define jb_warn(...) (warnf ? warnf(__VA_ARGS__) : (void)0) 
#define jb_err(...) (errf ? errf(__VA_ARGS__) : (void)0) 
#define jb_dbg(...) (dbgf ? dbgf(__VA_ARGS__) : (void)0)


//public functions
jitterbuffer *jb_new();
void jb_reset(jitterbuffer *jb);
void jb_reset_all(jitterbuffer *jb);
void jb_destroy(jitterbuffer *jb);
void jb_set_settings(jitterbuffer *jb, jb_settings *settings);

void jb_get_info(jitterbuffer *jb, jb_info *stats); 
void jb_get_settings(jitterbuffer *jb, jb_settings *settings); 
float jb_guess_mos(float p, long d, int codec); 
int jb_has_frames(jitterbuffer *jb);

void jb_put(jitterbuffer *jb, void *data, int type, long ms, long ts, long now, int codec); 
int jb_get(jitterbuffer *jb, void **data, long now, long interpl);



//private functions
static void set_default_settings(jitterbuffer *jb); 
static void reset(jitterbuffer *jb); 
static long find_pointer(long *array, long max_index, long value); static void frame_free(jb_frame *frame);

static void put_control(jitterbuffer *jb, void *data, int type, long ts); 
static void put_voice(jitterbuffer *jb, void *data, int type, long ms, long ts, int codec); 
static void put_history(jitterbuffer *jb, long ts, long now, long ms, int codec); 
static void calculate_info(jitterbuffer *jb, long ts, long now, int codec);

static int get_control(jitterbuffer *jb, void **data); 
static int get_voice(jitterbuffer *jb, void **data, long now, long interpl); 
static int get_voicecase(jitterbuffer *jb, void **data, long now, long interpl, long diff);

static int get_next_frametype(jitterbuffer *jb, long ts); 
static long get_next_framets(jitterbuffer *jb); 
static jb_frame *get_frame(jitterbuffer *jb, long ts); 
static jb_frame *get_all_frames(jitterbuffer *jb);

//debug...
static jb_output_function_t warnf, errf, dbgf; 
void jb_setoutput(jb_output_function_t warn, jb_output_function_t err, jb_output_function_t dbg) {
    warnf = warn;
    errf = err;
    dbgf = dbg;
}


/***********
 * create a new jitterbuffer
 * return NULL if malloc doesn't work
 * else return jb with default_settings.
 */
jitterbuffer *jb_new() 
{
  jitterbuffer *jb;
  
  jb_dbg("N");
  jb = malloc(sizeof(jitterbuffer));
  if (!jb) {
    jb_err("cannot allocate jitterbuffer\n");
    return NULL;
  }
  set_default_settings(jb);
  reset(jb);
  return jb;
}


/***********
 * empty voice messages 
 * reset statistics 
 * keep the settings
 */
void jb_reset(jitterbuffer *jb) 
{
  jb_frame *frame;
  
  jb_dbg("R");
  if (jb == NULL) {
    jb_err("no jitterbuffer in jb_reset()\n");
    return;
  }
  
  //free voice
  while(jb->voiceframes) {
    frame = get_all_frames(jb);
    frame_free(frame);
  }
  //reset stats
  memset(&(jb->info),0,sizeof(jb_info) );
  // set default settings
  reset(jb);
}


/***********
 * empty nonvoice messages
 * empty voice messages
 * reset statistics 
 * reset settings to default
 */
void jb_reset_all(jitterbuffer *jb) 
{
  jb_frame *frame;
  
  jb_dbg("r");
  if (jb == NULL) {
    jb_err("no jitterbuffer in jb_reset_all()\n");
    return;
  }
  
  // free nonvoice
  while(jb->controlframes) {
    frame = jb->controlframes;
    jb->controlframes = frame->next;
    frame_free(frame);
  }
  // free voice and reset statistics is done by jb_reset
  jb_reset(jb);
  set_default_settings(jb);
}


/***********
 * destroy the jitterbuffer
 * free all the [non]voice frames with reset_all
 * free the jitterbuffer
 */
void jb_destroy(jitterbuffer *jb) 
{
  jb_dbg("D");
  if (jb == NULL) {
    jb_err("no jitterbuffer in jb_destroy()\n");
    return;
  }
  
  jb_reset_all(jb);
  free(jb);
}


/***********
 * Set settings for the jitterbuffer. 
 * Only if a setting is defined it will be written
 * in the jb->settings.
 * This means that no setting can be set to zero
 */
void jb_set_settings(jitterbuffer *jb, jb_settings *settings) 
{
  jb_dbg("S");
  if (jb == NULL) {
    jb_err("no jitterbuffer in jb_set_settings()\n");
    return;
  }
  
  if (settings->min_jb) {
    jb->settings.min_jb = settings->min_jb;
  }
  if (settings->max_jb) {
    jb->settings.max_jb = settings->max_jb;
  }
  if (settings->max_successive_interp) {
    jb->settings.max_successive_interp = settings->max_successive_interp;
  }
  if (settings->extra_delay) {
    jb->settings.extra_delay = settings->extra_delay;
  }
  if (settings->wait_grow) {
    jb->settings.wait_grow = settings->wait_grow;
  }
  if (settings->wait_shrink) {
    jb->settings.wait_shrink = settings->wait_shrink;
  }
  if (settings->max_diff) {
    jb->settings.max_diff = settings->max_diff;
  }
}


/***********
 * validates the statistics
 * the losspct due the jitterbuffer will be calculated.
 * delay and delay_target will be calculated
 * *stats = info
 */
void jb_get_info(jitterbuffer *jb, jb_info *stats) 
{
  long max_index, pointer;
  
  jb_dbg("I");
  if (jb == NULL) {
    jb_err("no jitterbuffer in jb_get_info()\n");
    return;
  }
  
  jb->info.delay = jb->current - jb->min;
  jb->info.delay_target = jb->target - jb->min;
  
  //calculate the losspct...
  max_index = (jb->hist_pointer < JB_HISTORY_SIZE) ? 
jb->hist_pointer : JB_HISTORY_SIZE-1;
  if (max_index>1) {
    pointer = find_pointer(&jb->hist_sorted_delay[0], max_index, 
jb->current);
    jb->info.losspct = ((max_index - pointer)*100/max_index);
    if (jb->info.losspct < 0) {
      jb->info.losspct = 0;
    }
  } else {
    jb->info.losspct = 0;
  }
  
  *stats = jb->info;
}


/***********
 * gives the settings for this jitterbuffer
 * *settings = settings
 */
void jb_get_settings(jitterbuffer *jb, jb_settings *settings) 
{
  jb_dbg("S");
  if (jb == NULL) {
    jb_err("no jitterbuffer in jb_get_settings()\n");
    return;
  }
  
  *settings = jb->settings;
}


/***********
 * returns an estimate on the MOS with given loss, delay and codec 
 * if the formula is not present the default will be used
 * please use the JB_CODEC_OTHER if you want to define your own formula
 * 
 */
float jb_guess_mos(float p, long d, int codec) 
{
  float result;
  
  switch (codec) {
    case JB_CODEC_GSM_EFR: 
      result = (4.31f - 0.23f*p - 0.0071f*d);
    break;

    case JB_CODEC_G723_1: 
      result = (3.99f - 0.16f*p - 0.0071f*d);
    break;

    case JB_CODEC_G729: 
    case JB_CODEC_G729A: 
      result = (4.13f - 0.14f*p - 0.0071f*d);
    break;

    case JB_CODEC_G711x_PLC:
      result = (4.42f - 0.087f*p - 0.0071f*d);
    break;

    case JB_CODEC_G711x:
      result = (4.42f - 0.63f*p - 0.0071f*d);
    break;
    
    case JB_CODEC_OTHER:
    default:
      result = (4.42f - 0.63f*p - 0.0071f*d);

  }
  return result;
}


/***********
 * if there are any frames left in JB returns JB_OK, otherwise returns JB_EMPTY
 */
int jb_has_frames(jitterbuffer *jb)
{
  jb_dbg("H");
  if (jb == NULL) {
    jb_err("no jitterbuffer in jb_has_frames()\n");
    return JB_NOJB;
  }
  
  if(jb->controlframes || jb->voiceframes) {
    return JB_OK;
  } else {
    return JB_EMPTY;
  }
}


/***********
 * Put a packet into the jitterbuffers 
 * Only the timestamps of voicepackets are put in the history
 * this because the jitterbuffer only works for voicepackets
 * don't put packets twice in history and queue (e.g. transmitting every frame twice)
 * keep track of statistics
 */
void jb_put(jitterbuffer *jb, void *data, int type, long ms, long ts, long now, int codec) 
{ 
  long pointer, max_index;
  
  if (jb == NULL) {
    jb_err("no jitterbuffer in jb_put()\n");
    return;
  }
  
  jb->info.frames_received++;

  if (type == JB_TYPE_CONTROL) {
    //put the packet into the contol-queue of the jitterbuffer
    jb_dbg("pC");
    put_control(jb,data,type,ts);

  } else if (type == JB_TYPE_VOICE) {
    // only add voice that aren't already in the buffer
    max_index = (jb->hist_pointer < JB_HISTORY_SIZE) ? jb->hist_pointer : JB_HISTORY_SIZE-1;
    pointer = find_pointer(&jb->hist_sorted_timestamp[0], max_index, ts);
    if (jb->hist_sorted_timestamp[pointer]==ts) { //timestamp already in queue
      jb_dbg("pT");
      free(data); 
      jb->info.frames_dropped_twice++;
    } else { //add
      jb_dbg("pV");
      /* add voicepacket to history */
      put_history(jb,ts,now,ms,codec);
      /*calculate jitterbuffer size*/
      calculate_info(jb, ts, now, codec);
      /*put the packet into the queue of the jitterbuffer*/
      put_voice(jb,data,type,ms,ts,codec);
    } 

  } else if (type == JB_TYPE_SILENCE){ //silence
    jb_dbg("pS");
    put_voice(jb,data,type,ms,ts,codec);

  } else {//should NEVER happen
    jb_err("jb_put(): type not known\n");
    free(data);
  }
}


/***********
 * control frames have a higher priority then voice frames
 * returns JB_OK if a frame is available and *data points to the packet
 * returns JB_NOFRAME if it's no time to play voice and no control available
 * returns JB_INTERP if interpolating is required
 * returns JB_EMPTY if no voice frame is in the jitterbuffer (only during silence)
 */
int jb_get(jitterbuffer *jb, void **data, long now, long interpl) 
{
  int result;
  
  jb_dbg("A");
  if (jb == NULL) {
    jb_err("no jitterbuffer in jb_get()\n");
    return JB_NOJB;
  }
  
  result = get_control(jb, data);
  if (result != JB_OK ) { //no control message available maybe there is voice...
    result = get_voice(jb, data, now, interpl);
  }
  return result;
}


/***********
 * set all the settings to default 
 */
static void set_default_settings(jitterbuffer *jb) 
{
  jb->settings.min_jb = JB_MIN_SIZE;
  jb->settings.max_jb = JB_MAX_SIZE;
  jb->settings.max_successive_interp = JB_MAX_SUCCESSIVE_INTERP;
  jb->settings.extra_delay = JB_ALLOW_EXTRA_DELAY;
  jb->settings.wait_grow = JB_WAIT_GROW;
  jb->settings.wait_shrink = JB_WAIT_SHRINK;
  jb->settings.max_diff = JB_MAX_DIFF;
}


/***********
 * reset the jitterbuffer so we can start in silence and 
 * we start with a new history
 */
static void reset(jitterbuffer *jb)
{
  jb->hist_pointer = 0; //start over
  jb->silence_begin_ts = 0; //no begin_ts defined
  jb->info.silence =1; //we always start in silence
}


/***********
 * Search algorithm
 * @REQUIRE max_index is within array
 *
 * Find the position of value in hist_sorted_delay
 * if value doesn't exist return first pointer where array[low]>value
 * int low;   //the lowest index being examined
 * int max_index; //the highest index being examined
 * int mid;  //the middle index between low and max_index. 
 * mid ==(low+max_index)/2
 * at the end low is the position of value or where array[low]>value
 */  
static long find_pointer(long *array, long max_index, long value) 
{
  long low, mid, high;
  low = 0;
  high = max_index;
  while (low<=high) {
    mid= (low+high)/2;
    if (array[mid] < value) {
      low = mid+1;
    } else {
      high = mid-1;
    }
  }
  while(low < max_index && (array[low]==array[(low+1)]) ) {
    low++;
  }
  return low;
}


/***********
 * free the given frame, afterwards the framepointer is undefined
 */
static void frame_free(jb_frame *frame) 
{
  if (frame->data) {
    free(frame->data);
  }
  free(frame);
}


/***********
 * put a nonvoice frame into the nonvoice queue
 */
static void put_control(jitterbuffer *jb, void *data, int type, long ts) 
{
  jb_frame *frame, *p;
    
  frame = malloc(sizeof(jb_frame));
  if(!frame) {
    jb_err("cannot allocate frame\n");
    return;
  }
  frame->data = data;
  frame->ts = ts;
  frame->type = type;
  frame->next = NULL;
  data = NULL;//to avoid stealing memory
  
  p = jb->controlframes;
  if (p) { //there are already control messages
    if (ts < p->ts) {
      jb->controlframes = frame;
      frame->next = p;
    } else {
      while (p->next && (ts >=p->next->ts)) {//sort on timestamps! so find place to put...
        p = p->next; 
      }
      if (p->next) {
        frame->next = p->next;
      }
      p->next = frame;
    }
  } else {
    jb->controlframes = frame;
  }
}


/***********
 * put a voice or silence frame into the jitterbuffer 
 */
static void put_voice(jitterbuffer *jb, void *data, int type, long ms, long ts, int codec) 
{
  jb_frame *frame, *p;
  frame = malloc(sizeof(jb_frame));
  if(!frame) {
    jb_err("cannot allocate frame\n");
    return;
  }
  
  frame->data = data;
  frame->ts = ts;
  frame->ms = ms;
  frame->type = type;
  frame->codec = codec;
  
  data = NULL; //to avoid stealing the memory location
  /* 
   * frames are a circular list, jb->voiceframes points to to the lowest ts, 
   * jb->voiceframes->prev points to the highest ts
   */
  if(!jb->voiceframes) {  /* queue is empty */
    jb->voiceframes = frame;
    frame->next = frame;
    frame->prev = frame;
  } else { 
    p = jb->voiceframes;
    if(ts < p->prev->ts) { //frame is out of order
      jb->info.frames_ooo++;
    }
    if (ts < p->ts) { //frame is lowest, let voiceframes point to it!
      jb->voiceframes = frame;
    } else {
      while(ts < p->prev->ts ) {
        p = p->prev;
      }
    }
    frame->next = p;
    frame->prev = p->prev;
    frame->next->prev = frame;
    frame->prev->next = frame;
  }
}


/***********
 * puts the timestamps of a received packet in the history of *jb
 * for later calculations of the size of jitterbuffer *jb.
 *  
 * summary of function: 
 * - calculate delay difference 
 * - delete old value from hist & sorted_history_delay & sorted_history_timestamp if needed 
 * - add new value to history & sorted_history_delay & sorted_history_timestamp
 * - we keep sorted_history_delay for calculations 
 * - we keep sorted_history_timestamp for ensuring each timestamp isn't put twice in the buffer.
 */
static void put_history(jitterbuffer *jb, long ts, long now, long ms, int codec) 
{
  jb_hist_element out, in;
  long max_index, pointer, location;
  
  // max_index is the highest possible index
  max_index = (jb->hist_pointer < JB_HISTORY_SIZE) ? jb->hist_pointer : JB_HISTORY_SIZE-1;
  location = (jb->hist_pointer % JB_HISTORY_SIZE);

  // we want to delete a value from the jitterbuffer
  // only when we are through the history.
  if (jb->hist_pointer > JB_HISTORY_SIZE-1) {
    /* the value we need to delete from sorted histories */
    out = jb->hist[location];
    //delete delay from hist_sorted_delay
    pointer = find_pointer(&jb->hist_sorted_delay[0], max_index, out.delay);
    /* move over pointer is the position of kicked*/
    if (pointer<max_index) { //only move if we have something to move
      memmove(  &(jb->hist_sorted_delay[pointer]), 
                &(jb->hist_sorted_delay[pointer+1]), 
                ((JB_HISTORY_SIZE-(pointer+1)) * sizeof(long)) );
    }
    
    //delete timestamp from hist_sorted_timestamp
    pointer = find_pointer(&jb->hist_sorted_timestamp[0], max_index, out.ts);
    /* move over pointer is the position of kicked*/
    if (pointer<max_index) { //only move if we have something to move
      memmove(  &(jb->hist_sorted_timestamp[pointer]), 
                &(jb->hist_sorted_timestamp[pointer+1]), 
                ((JB_HISTORY_SIZE-(pointer+1)) * sizeof(long)) );
    }
  }
    
  in.delay = now - ts;    //delay of current packet
  in.ts = ts;      //timestamp of current packet
  in.ms = ms;      //length of current packet
  in.codec = codec;      //codec of current packet
  
  /* adding the new delay to the sorted history
   * first special cases:
   * - delay is the first history stamp
   * - delay > highest history stamp 
   */
  if (max_index==0 || in.delay >= jb->hist_sorted_delay[max_index-1]) {
    jb->hist_sorted_delay[max_index] = in.delay;
  } else {
    pointer = find_pointer(&jb->hist_sorted_delay[0], (max_index-1), in.delay);
    /* move over and add delay */
    memmove(  &(jb->hist_sorted_delay[pointer+1]),
              &(jb->hist_sorted_delay[pointer]), 
              ((JB_HISTORY_SIZE-(pointer+1)) * sizeof(long)) );
    jb->hist_sorted_delay[pointer] = in.delay;
  }
  
  /* adding the new timestamp to the sorted history
   * first special cases:
   * - timestamp is the first history stamp
   * - timestamp > highest history stamp 
   */
  if (max_index==0 || in.ts >= jb->hist_sorted_timestamp[max_index-1]) {
    jb->hist_sorted_timestamp[max_index] = in.ts;
  } else {
    
    pointer = find_pointer(&jb->hist_sorted_timestamp[0], (max_index-1), in.ts);
    /* move over and add timestamp */
    memmove(  &(jb->hist_sorted_timestamp[pointer+1]),
              &(jb->hist_sorted_timestamp[pointer]), 
              ((JB_HISTORY_SIZE-(pointer+1)) * sizeof(long)) );
    jb->hist_sorted_timestamp[pointer] = in.ts;
  }
  
  /* put the jb_hist_element in the history 
  * then increase hist_pointer for next time
  */
  jb->hist[location] = in;
  jb->hist_pointer++;
}


/***********
 * this tries to make a jitterbuffer that behaves like
 * the jitterbuffer proposed in this article:
 * Adaptive Playout Buffer Algorithm for Enhancing Perceived Quality of Streaming Applications
 * by: Kouhei Fujimoto & Shingo Ata & Masayuki Murata
 * http://www.nal.ics.es.osaka-u.ac.jp/achievements/web2002/pdf/journal/k-fujimo02TSJ-AdaptivePlayoutBuffer.pdf
 * 
 * it calculates jitter and minimum delay
 * get the best delay for the specified codec
 
 */
static void calculate_info(jitterbuffer *jb, long ts, long now, int codec) 
{
  long diff, size, max_index, d, d1, d2, n;
  float p, p1, p2, A, B;
  //size = how many items there in the history
  size = (jb->hist_pointer < JB_HISTORY_SIZE) ? jb->hist_pointer : JB_HISTORY_SIZE;
  max_index = size-1;
  
  /* 
   * the Inter-Quartile Range can be used for estimating jitter
   * http://www.slac.stanford.edu/comp/net/wan-mon/tutorial.html#variable
   * just take the square root of the iqr for jitter
   */
  jb->info.iqr = jb->hist_sorted_delay[max_index*3/4] - jb->hist_sorted_delay[max_index/4];
  
  
  /*
   * The RTP way of calculating jitter.
   * This one is used at the moment, although it is not correct.
   * But in this way the other side understands us.
   */
  diff = now - ts - jb->last_delay;
  if (!jb->last_delay) {
    diff = 0; //this to make sure we won't get odd jitter due first ts.
  }
  jb->last_delay = now - ts;
  if (diff <0){
    diff = -diff;
  }
  jb->info.jitter = jb->info.jitter + (diff - jb->info.jitter)/16;
  
  /* jb->min is minimum delay in hist_sorted_delay, we don't look at the lowest 2% */
  /* because sometimes there are odd delays in there */
  jb->min = jb->hist_sorted_delay[(max_index*2/100)];
  
  /* 
   * calculating the preferred size of the jitterbuffer:
   * instead of calculating the optimum delay using the Pareto equation
   * I use look at the array of sorted delays and choose my optimum from there
   * always walk trough a percentage of the history this because imagine following tail: 
   * [...., 12, 300, 301 ,302]
   * her we want to discard last three but that won't happen if we won't walk the array
   * the number of frames we walk depends on how scattered the sorted delays are.
   * For that we look at the iqr. The dependencies of the iqr are based on 
   * tests we've done here in the lab. But are not optimized.
   */
  //init:
  //the higest delay..
  d = d1= d2 = jb->hist_sorted_delay[max_index]- jb->min; 
  A=B=LONG_MIN;
  p = p2 =0;
  n=0;
  p1 = 5; //always look at the top 5%
  if (jb->info.iqr >200) { //with more jitter look at more delays
    p1=25;
  } else if (jb->info.iqr >100) {
    p1=20;
  } else if (jb->info.iqr >50){ 
    p1=11;
  } 
  
  //find the optimum delay..
  while(max_index>10 && (B >= A ||p2<p1)) { 
    //the packetloss with this delay
    p2 =(n*100.0f/size);
    // estimate MOS-value
    B = jb_guess_mos(p2,d2,codec);
    if (B > A) {
      p = p2;
      d = d2;
      A = B;
    }
    d1 = d2;
    //find next delay != delay so the same delay isn't calculated twice
    //don't look further if we have seen half of the history
    while((d2>=d1) && ((n*2)<max_index) ) {
      n++;
      d2 = jb->hist_sorted_delay[(max_index-n)] - jb->min;
    }
  }
  //the targeted size of the jitterbuffer
  if (jb->settings.min_jb && (jb->settings.min_jb > d) ) {
    jb->target = jb->min + jb->settings.min_jb; 
  } else if (jb->settings.max_jb && (jb->settings.max_jb > d) ){
    jb->target = jb->min + jb->settings.max_jb;
  } else {
    jb->target = jb->min + d; 
  }
}


/***********
 * if there is a nonvoice frame it will be returned [*data] and the frame
 * will be made free
 */  
static int get_control(jitterbuffer *jb, void **data) 
{
  jb_frame *frame;
  int result;
  
  frame = jb->controlframes;
  if (frame) {
    jb_dbg("gC");
    *data = frame->data;
    frame->data = NULL;
    jb->controlframes = frame->next;
    frame_free(frame);
    result = JB_OK;
  } else {
    result = JB_NOFRAME;
  }
  return result;
}


/***********
 * returns JB_OK if a frame is available and *data points to the packet
 * returns JB_NOFRAME if it's no time to play voice and or no frame available
 * returns JB_INTERP if interpolating is required
 * returns JB_EMPTY if no voice frame is in the jitterbuffer (only during silence)
 * 
 * if the next frame is a silence frame we will go in silence-mode
 * each new instance of the jitterbuffer will start in silence mode
 * in silence mode we will set the jitterbuffer to the size we want
 * when we are not in silence mode get_voicecase will handle the rest. 
 */
static int get_voice(jitterbuffer *jb, void **data, long now, long interpl) 
{
  jb_frame *frame;
  long diff;
  int result;
  
  diff = jb->target - jb->current;
  
  //if the next frame is a silence frame, go in silence mode...
  if((get_next_frametype(jb, now - jb->current) == JB_TYPE_SILENCE) ) {
    jb_dbg("gs");
    frame = get_frame(jb, now - jb->current);
    *data = frame->data;
    frame->data = NULL;
    jb->info.silence =1;
    jb->silence_begin_ts = frame->ts;
    frame_free(frame);
    result = JB_OK;
  } else {  
    if(jb->info.silence) { // we are in silence
      /*
       * During silence we can set the jitterbuffer size to the size
       * we want...
       */
      if (diff) {
        jb->current = jb->target;
      }
      frame = get_frame(jb, now - jb->current);
      if (frame) {
        if (jb->silence_begin_ts && frame->ts < jb->silence_begin_ts) {
          jb_dbg("gL");
          /* voice frame is late, next!*/
          jb->info.frames_late++;
          frame_free(frame);
          result = get_voice(jb, data, now, interpl);
        } else {
          jb_dbg("gP"); 
          /* voice frame */
          jb->info.silence = 0;
          jb->silence_begin_ts = 0;
          jb->next_voice_time = frame->ts + frame->ms;
          jb->info.last_voice_ms = frame->ms;
          *data = frame->data;
          frame->data = NULL;
          frame_free(frame);
          result = JB_OK;
        }
      } else {    //no frame 
        jb_dbg("gS");
        result = JB_EMPTY;
      }
    } else { //voice case
      result = get_voicecase(jb,data,now,interpl,diff);
    }
  }
  return result;
}


/***********
 * The voicecase has four 'options'
 * - difference is way off, reset
 * - diff > 0, we may need to grow
 * - diff < 0, we may need to shrink
 * - everything else
 */
static int get_voicecase(jitterbuffer *jb, void **data, long now, long interpl, long diff) 
{
  jb_frame *frame;
  int result;
  
   // * - difference is way off, reset
  if (diff > jb->settings.max_diff || -diff > jb->settings.max_diff) {
    jb_err("wakko diff in get_voicecase\n");
    reset(jb); //reset hist because the timestamps are wakko. 
    result = JB_NOFRAME;
  //- diff > 0, we may need to grow
  } else if ((diff > 0) && 
                   (now > (jb->last_adjustment + jb->settings.wait_grow) 
                    || (now + jb->current + interpl) < get_next_framets(jb) ) ) { //grow
    /* first try to grow */
    if (diff<interpl/2) {
      jb_dbg("ag");
      jb->current +=diff;
    } else {
      jb_dbg("aG");
      /* grow by interp frame len */
      jb->current += interpl;
    }
    jb->last_adjustment = now;
    result = get_voice(jb, data, now, interpl);
  //- diff < 0, we may need to shrink
  } else if ( (diff < 0) 
                && (now > (jb->last_adjustment + jb->settings.wait_shrink)) 
                && ((-diff) > jb->settings.extra_delay) ) {
    /* now try to shrink
     * if there is a frame shrink by frame length
     * otherwise shrink by interpl
     */
    jb->last_adjustment = now;
    
    frame = get_frame(jb, now - jb->current);
    if(frame) {
      jb_dbg("as");
      /* shrink by frame size we're throwing out */
      jb->info.frames_dropped++;
      jb->current -= frame->ms;
      frame_free(frame);
    } else {
      jb_dbg("aS");
      /* shrink by interpl */
      jb->current -= interpl;
    }
    result = get_voice(jb, data, now, interpl);
  } else  { 
    /* if it is not the time to play a result = JB_NOFRAME
     * else We try to play a frame if a frame is available
     * and not late it is played otherwise 
     * if available it is dropped and the next is tried
     * last option is interpolating
     */
    if (now - jb->current < jb->next_voice_time) {
      jb_dbg("aN");
      result = JB_NOFRAME;
    } else {
      frame = get_frame(jb, now - jb->current);
      if (frame) { //there is a frame
        /* voice frame is late */
        if(frame->ts < jb->next_voice_time) {   //late
          jb_dbg("aL");
          jb->info.frames_late++;
          frame_free(frame);
          result = get_voice(jb, data, now, interpl);
        } else {
          jb_dbg("aP");
          /* normal case; return the frame, increment stuff */
          *data = frame->data;
          frame->data = NULL;
          jb->next_voice_time = frame->ts + frame->ms;
          jb->cnt_successive_interp = 0;
          frame_free(frame);
          result = JB_OK;
        }
      } else { // no frame, thus interpolate
        jb->cnt_successive_interp++;
        /* assume silence instead of continuing to interpolate */
        if (jb->settings.max_successive_interp && jb->cnt_successive_interp >= jb->settings.max_successive_interp) {
          jb->info.silence = 1;
          jb->silence_begin_ts = jb->next_voice_time;
        }
        jb_dbg("aI");
        jb->next_voice_time += interpl;
        result = JB_INTERP;
      }
    }
  }
  return result;

}


/***********
 * if there are frames and next frame->ts is smaller or equal ts 
 *   return type of next frame.
 * else return 0
 */
static int get_next_frametype(jitterbuffer *jb, long ts) 
{
  jb_frame *frame;
  int result;
  
  result = 0;
  frame = jb->voiceframes;
  if (frame && frame->ts <= ts) {
    result = frame->type;
  }
  return result;
}


/***********
 * returns ts from next frame in jb->voiceframes
 * or returns LONG_MAX if there is no frame
 */
static long get_next_framets(jitterbuffer *jb) 
{
  if (jb->voiceframes) {
    return jb->voiceframes->ts;
  }
  return LONG_MAX;
}


/***********
 * if there is a frame in jb->voiceframes and 
 * has a timestamp smaller/equal to ts
 * this frame will be returned and 
 * removed from the queue
 */
static jb_frame *get_frame(jitterbuffer *jb, long ts) 
{
  jb_frame *frame;
  
  frame = jb->voiceframes;
  if (frame && frame->ts <= ts) {
    if(frame->next == frame) {
      jb->voiceframes = NULL;
    } else {
      /* remove this frame */
      frame->prev->next = frame->next;
      frame->next->prev = frame->prev;
      jb->voiceframes = frame->next;
    }
    return frame;
  }
  return NULL;
}

/***********
 * if there is a frame in jb->voiceframes
 * this frame will be unconditionally returned and 
 * removed from the queue
 */
static jb_frame *get_all_frames(jitterbuffer *jb) 
{
  jb_frame *frame;
  
  frame = jb->voiceframes;
  if (frame) {
    if(frame->next == frame) {
      jb->voiceframes = NULL;
    } else {
      /* remove this frame */
      frame->prev->next = frame->next;
      frame->next->prev = frame->prev;
      jb->voiceframes = frame->next;
    }
    return frame;
  }
  return NULL;
}


//EOF
