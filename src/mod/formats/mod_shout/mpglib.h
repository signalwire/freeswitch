
struct buf {
        unsigned char *pnt;
	long size;
	long pos;
        struct buf *next;
        struct buf *prev;
};

struct framebuf {
	struct buf *buf;
	long pos;
	struct frame *next;
	struct frame *prev;
};

struct mpstr {
	struct buf *head,*tail;
	int bsize;
	int framesize;
        int fsizeold;
	struct frame fr;
        unsigned char bsspace[2][MAXFRAMESIZE+512]; /* MAXFRAMESIZE */
	real hybrid_block[2][2][SBLIMIT*SSLIMIT];
	int hybrid_blc[2];
	unsigned long header;
	int bsnum;
	real synth_buffs[2][2][0x110];
        int  synth_bo;
	long outscale; /* volume control default value 32768 */
	long outsamplerate; /* raw output rate default same as mp3 sample rate*/
	struct pcm_workingsample worksample; /* keep the state of the working sample for threads */
	int initmp3; /* flag for first initialisation */
	int longLimit[9][23]; /*sample limits re setting volume */
	int shortLimit[9][14];
	real decwin[512+32]; /* scale table */
	
	};

#define BOOL int

#define MP3_ERR -1
#define MP3_OK  0
#define MP3_NEED_MORE 1


void InitMP3Constants(void);
BOOL InitMP3(struct mpstr *mp, long outscale);
int decodeMP3(struct mpstr *mp,char *inmemory,int inmemsize,
     char *outmemory,int outmemsize,int *done);
void ExitMP3(struct mpstr *mp);

extern int synth_ntom_set_step(long,long);
extern int synth_ntom(struct mpstr *mp, real *bandPtr,int channel,unsigned char *out,int *pnt);
extern int synth_ntom_mono (struct mpstr *mp, real *bandPtr,unsigned char *samples,int *pnt);
extern int synth_ntom_8bit (real *,int,unsigned char *,int *);
extern int synth_ntom_mono2stereo (real *,unsigned char *,int *);
extern int synth_ntom_8bit_mono (real *,unsigned char *,int *);
extern int synth_ntom_8bit_mono2stereo (real *,unsigned char *,int *);

extern void init_layer3_sample_limits(struct mpstr *mp, int down_sample_sblimit);
extern void init_layer3_const(void);
extern int do_layer3(struct mpstr *mp,unsigned char *,int *);

extern void make_decode_tables_scale(struct mpstr *mp, long scaleval);
extern void make_decode_tables_const(void);
extern void make_conv16to8_table(int);

extern void dct64(real *,real *,real *);

extern unsigned int   get1bit(struct mpstr *mp);
extern unsigned int   getbits(struct mpstr *mp, int);
extern unsigned int   getbits_fast(struct mpstr *mp, int);
extern int set_pointer(struct mpstr *mp, long backstep);

