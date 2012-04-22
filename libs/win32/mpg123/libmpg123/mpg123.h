/*
	libmpg123: MPEG Audio Decoder library (version @PACKAGE_VERSION@)

	copyright 1995-2007 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
*/

#ifndef MPG123_LIB_H
#define MPG123_LIB_H

/** \file mpg123.h The header file for the libmpg123 MPEG Audio decoder */

/* These aren't actually in use... seems to work without using libtool. */
#ifdef BUILD_MPG123_DLL
/* The dll exports. */
#define EXPORT __declspec(dllexport)
#else
#ifdef LINK_MPG123_DLL
/* The exe imports. */
#define EXPORT __declspec(dllimport)
#else
/* Nothing on normal/UNIX builds */
#define EXPORT
#endif
#endif

#include <stdlib.h>
#include <sys/types.h>
typedef int  ssize_t;

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup mpg123_init mpg123 library and handle setup
 *
 * Functions to initialise and shutdown the mpg123 library and handles.
 * The parameters of handles have workable defaults, you only have to tune them when you want to tune something;-)
 * Tip: Use a RVA setting...
 *
 * @{
 */

/** Opaque structure for the libmpg123 decoder handle. */
struct mpg123_handle_struct;

/** Opaque structure for the libmpg123 decoder handle.
 *  Most functions take a pointer to a mpg123_handle as first argument and operate on its data in an object-oriented manner.
 */
typedef struct mpg123_handle_struct mpg123_handle;

/** Function to initialise the mpg123 library. 
 *	This function is not thread-safe. Call it exactly once per process, before any other (possibly threaded) work with the library.
 *
 *	\return MPG123_OK if successful, otherwise an error number.
 */
EXPORT int  mpg123_init(void);

/** Function to close down the mpg123 library. 
 *	This function is not thread-safe. Call it exactly once per process, before any other (possibly threaded) work with the library. */
EXPORT void mpg123_exit(void);

/** Create a handle with optional choice of decoder (named by a string, see mpg123_decoders() or mpg123_supported_decoders()).
 *  and optional retrieval of an error code to feed to mpg123_plain_strerror().
 *  Optional means: Any of or both the parameters may be NULL.
 *
 *  \return Non-NULL pointer when successful.
 */
EXPORT mpg123_handle *mpg123_new(const char* decoder, int *error);

/** Delete handle, mh is either a valid mpg123 handle or NULL. */
EXPORT void mpg123_delete(mpg123_handle *mh);

/** Enumeration of the parameters types that it is possible to set/get. */
enum mpg123_parms
{
	MPG123_VERBOSE,        /**< set verbosity value for enabling messages to stderr, >= 0 makes sense */
	MPG123_FLAGS,          /**< set all flags, p.ex val = MPG123_GAPLESS|MPG123_MONO_MIX */
	MPG123_ADD_FLAGS,      /**< add some flags */
	MPG123_FORCE_RATE,     /**< when value > 0, force output rate to that value */
	MPG123_DOWN_SAMPLE,    /**< 0=native rate, 1=half rate, 2=quarter rate */
	MPG123_RVA,            /**< one of the RVA choices above */
	MPG123_DOWNSPEED,      /**< play a frame N times */
	MPG123_UPSPEED,        /**< play every Nth frame */
	MPG123_START_FRAME,    /**< start with this frame (skip frames before that) */ 
	MPG123_DECODE_FRAMES,  /**< decode only this number of frames */
	MPG123_ICY_INTERVAL,   /**< stream contains ICY metadata with this interval */
	MPG123_OUTSCALE,       /**< the scale for output samples (amplitude) */
	MPG123_TIMEOUT,        /**< timeout for reading from a stream (not supported on win32) */
	MPG123_REMOVE_FLAGS,   /**< remove some flags (inverse of MPG123_ADD_FLAGS) */
	MPG123_RESYNC_LIMIT    /**< Try resync on frame parsing for that many bytes or until end of stream (<0). */
};

/** Flag bits for MPG123_FLAGS, use the usual binary or to combine. */
enum mpg123_param_flags
{
	 MPG123_FORCE_MONO   = 0x7  /**<     0111 Force some mono mode: This is a test bitmask for seeing if any mono forcing is active. */
	,MPG123_MONO_LEFT    = 0x1  /**<     0001 Force playback of left channel only.  */
	,MPG123_MONO_RIGHT   = 0x2  /**<     0010 Force playback of right channel only. */
	,MPG123_MONO_MIX     = 0x4  /**<     0100 Force playback of mixed mono.         */
	,MPG123_FORCE_STEREO = 0x8  /**<     1000 Force stereo output.                  */
	,MPG123_FORCE_8BIT   = 0x10 /**< 00010000 Force 8bit formats.                   */
	,MPG123_QUIET        = 0x20 /**< 00100000 Suppress any printouts (overrules verbose).                    */
	,MPG123_GAPLESS      = 0x40 /**< 01000000 Enable gapless decoding (default on if libmpg123 has support). */
	,MPG123_NO_RESYNC    = 0x80 /**< 10000000 Disable resync stream after error.                             */
	,MPG123_SEEKBUFFER   = 0x100 /**< 000100000000 Enable small buffer on non-seekable streams to allow some peek-ahead (for better MPEG sync). */
};

/** choices for MPG123_RVA */
enum mpg123_param_rva
{
	 MPG123_RVA_OFF   = 0 /**< RVA disabled (default).   */
	,MPG123_RVA_MIX   = 1 /**< Use mix/track/radio gain. */
	,MPG123_RVA_ALBUM = 2 /**< Use album/audiophile gain */
	,MPG123_RVA_MAX   = MPG123_RVA_ALBUM /**< The maximum RVA code, may increase in future. */
};

/* TODO: Assess the possibilities and troubles of changing parameters during playback. */

/** Set a specific parameter, for a specific mpg123_handle, using a parameter 
 *  type key chosen from the mpg123_parms enumeration, to the specified value. */
EXPORT int mpg123_param(mpg123_handle *mh, enum mpg123_parms type, long value, double fvalue);

/** Get a specific parameter, for a specific mpg123_handle. 
 *  See the mpg123_parms enumeration for a list of available parameters. */
EXPORT int mpg123_getparam(mpg123_handle *mh, enum mpg123_parms type, long *val, double *fval);

/* @} */


/** \defgroup mpg123_error mpg123 error handling
 *
 * Functions to get text version of the error numbers and an enumeration
 * of the error codes returned by libmpg123.
 *
 * Most functions operating on a mpg123_handle simply return MPG123_OK on success and MPG123_ERR on failure (setting the internal error variable of the handle to the specific error code).
 * Decoding/seek functions may also return message codes MPG123_DONE, MPG123_NEW_FORMAT and MPG123_NEED_MORE.
 * The positive range of return values is used for "useful" values when appropriate.
 *
 * @{
 */

/** Enumeration of the message and error codes and returned by libmpg123 functions. */
enum mpg123_errors
{
	MPG123_DONE=-12,	/**< Message: Track ended. */
	MPG123_NEW_FORMAT=-11,	/**< Message: Output format will be different on next call. */
	MPG123_NEED_MORE=-10,	/**< Message: For feed reader: "Feed me more!" */
	MPG123_ERR=-1,			/**< Generic Error */
	MPG123_OK=0, 			/**< Success */
	MPG123_BAD_OUTFORMAT, 	/**< Unable to set up output format! */
	MPG123_BAD_CHANNEL,		/**< Invalid channel number specified. */
	MPG123_BAD_RATE,		/**< Invalid sample rate specified.  */
	MPG123_ERR_16TO8TABLE,	/**< Unable to allocate memory for 16 to 8 converter table! */
	MPG123_BAD_PARAM,		/**< Bad parameter id! */
	MPG123_BAD_BUFFER,		/**< Bad buffer given -- invalid pointer or too small size. */
	MPG123_OUT_OF_MEM,		/**< Out of memory -- some malloc() failed. */
	MPG123_NOT_INITIALIZED,	/**< You didn't initialize the library! */
	MPG123_BAD_DECODER,		/**< Invalid decoder choice. */
	MPG123_BAD_HANDLE,		/**< Invalid mpg123 handle. */
	MPG123_NO_BUFFERS,		/**< Unable to initialize frame buffers (out of memory?). */
	MPG123_BAD_RVA,			/**< Invalid RVA mode. */
	MPG123_NO_GAPLESS,		/**< This build doesn't support gapless decoding. */
	MPG123_NO_SPACE,		/**< Not enough buffer space. */
	MPG123_BAD_TYPES,		/**< Incompatible numeric data types. */
	MPG123_BAD_BAND,		/**< Bad equalizer band. */
	MPG123_ERR_NULL,		/**< Null pointer given where valid storage address needed. */
	MPG123_ERR_READER,		/**< Error reading the stream. */
	MPG123_NO_SEEK_FROM_END,/**< Cannot seek from end (end is not known). */
	MPG123_BAD_WHENCE,		/**< Invalid 'whence' for seek function.*/
	MPG123_NO_TIMEOUT,		/**< Build does not support stream timeouts. */
	MPG123_BAD_FILE,		/**< File access error. */
	MPG123_NO_SEEK,			/**< Seek not supported by stream. */
	MPG123_NO_READER,		/**< No stream opened. */
	MPG123_BAD_PARS,		/**< Bad parameter handle. */
	MPG123_BAD_INDEX_PAR,	/**< Bad parameters to mpg123_index() */
	MPG123_OUT_OF_SYNC,	/**< Lost track in bytestream and did not try to resync. */
	MPG123_RESYNC_FAIL,	/**< Resync failed to find valid MPEG data. */
	MPG123_NO_8BIT,	/**< No 8bit encoding possible. */
	MPG123_BAD_ALIGN,	/**< Stack aligmnent error */
	MPG123_NULL_BUFFER	/**< NULL input buffer with non-zero size... */
};

/** Return a string describing that error errcode means. */
EXPORT const char* mpg123_plain_strerror(int errcode);

/** Give string describing what error has occured in the context of handle mh.
 *  When a function operating on an mpg123 handle returns MPG123_ERR, you should check for the actual reason via
 *  char *errmsg = mpg123_strerror(mh)
 *  This function will catch mh == NULL and return the message for MPG123_BAD_HANDLE. */
EXPORT const char* mpg123_strerror(mpg123_handle *mh);

/** Return the plain errcode intead of a string. */
EXPORT int mpg123_errcode(mpg123_handle *mh);

/*@}*/


/** \defgroup mpg123_decoder mpg123 decoder selection
 *
 * Functions to list and select the available decoders.
 * Perhaps the most prominent feature of mpg123: You have several (optimized) decoders to choose from (on x86 and PPC (MacOS) systems, that is).
 *
 * @{
 */

/** Return a NULL-terminated array of generally available decoder names (plain 8bit ASCII). */
EXPORT char **mpg123_decoders();

/** Return a NULL-terminated array of the decoders supported by the CPU (plain 8bit ASCII). */
EXPORT char **mpg123_supported_decoders();

/** Set the chosen decoder to 'decoder_name' */
EXPORT int mpg123_decoder(mpg123_handle *mh, const char* decoder_name);

/*@}*/


/** \defgroup mpg123_output mpg123 output audio format 
 *
 * Functions to get and select the format of the decoded audio.
 *
 * @{
 */

/** 16 or 8 bits, signed or unsigned... all flags fit into 15 bits and are designed to have meaningful binary AND/OR.
 * Adding float and 32bit int definitions for experimental fun. Only 32bit (and possibly 64bit) float is
 * somewhat there with a dedicated library build. */
enum mpg123_enc_enum
{
	 MPG123_ENC_8      = 0x00f  /**< 0000 0000 1111 Some 8 bit  integer encoding. */ 
	,MPG123_ENC_16     = 0x040  /**< 0000 0100 0000 Some 16 bit integer encoding. */
	,MPG123_ENC_32     = 0x100  /**< 0001 0000 0000 Some 32 bit integer encoding. */
	,MPG123_ENC_SIGNED = 0x080  /**< 0000 1000 0000 Some signed integer encoding. */
	,MPG123_ENC_FLOAT  = 0x800  /**< 1110 0000 0000 Some float encoding. */
	,MPG123_ENC_SIGNED_16   = (MPG123_ENC_16|MPG123_ENC_SIGNED|0x10) /**< 0000 1101 0000 signed 16 bit */
	,MPG123_ENC_UNSIGNED_16 = (MPG123_ENC_16|0x20)                   /**< 0000 0110 0000 unsigned 16 bit*/
	,MPG123_ENC_UNSIGNED_8  = 0x01                                   /**< 0000 0000 0001 unsigned 8 bit*/
	,MPG123_ENC_SIGNED_8    = (MPG123_ENC_SIGNED|0x02)               /**< 0000 1000 0010 signed 8 bit*/
	,MPG123_ENC_ULAW_8      = 0x04                                   /**< 0000 0000 0100 ulaw 8 bit*/
	,MPG123_ENC_ALAW_8      = 0x08                                   /**< 0000 0000 1000 alaw 8 bit */
	,MPG123_ENC_SIGNED_32   = MPG123_ENC_32|MPG123_ENC_SIGNED|0x10   /**< 0001 1001 0000 signed 32 bit */
	,MPG123_ENC_UNSIGNED_32 = MPG123_ENC_32|0x20                     /**< 0001 0010 0000 unsigned 32 bit */
	,MPG123_ENC_FLOAT_32    = 0x200                                  /**< 0010 0000 0000 32bit float */
	,MPG123_ENC_FLOAT_64    = 0x400                                  /**< 0100 0000 0000 64bit float */
	,MPG123_ENC_ANY = ( MPG123_ENC_SIGNED_16  | MPG123_ENC_UNSIGNED_16 | MPG123_ENC_UNSIGNED_8 
	                  | MPG123_ENC_SIGNED_8   | MPG123_ENC_ULAW_8      | MPG123_ENC_ALAW_8
	                  | MPG123_ENC_FLOAT_32   | MPG123_ENC_FLOAT_64 ) /**< any encoding */
};

/** They can be combined into one number (3) to indicate mono and stereo... */
enum mpg123_channelcount
{
	 MPG123_MONO   = 1
	,MPG123_STEREO = 2
};

/** An array of supported standard sample rates
 *  These are possible native sample rates of MPEG audio files.
 *  You can still force mpg123 to resample to a different one, but by default you will only get audio in one of these samplings.
 *  \param list Store a pointer to the sample rates array there.
 *  \param number Store the number of sample rates there. */
EXPORT void mpg123_rates(const long **list, size_t *number);

/** An array of supported audio encodings.
 *  An audio encoding is one of the fully qualified members of mpg123_enc_enum (MPG123_ENC_SIGNED_16, not MPG123_SIGNED).
 *  \param list Store a pointer to the encodings array there.
 *  \param number Store the number of encodings there. */
EXPORT void mpg123_encodings(const int **list, size_t *number);

/** Configure a mpg123 handle to accept no output format at all, 
 *  use before specifying supported formats with mpg123_format */
EXPORT int mpg123_format_none(mpg123_handle *mh);

/** Configure mpg123 handle to accept all formats 
 *  (also any custom rate you may set) -- this is default. */
EXPORT int mpg123_format_all(mpg123_handle *mh);

/** Set the audio format support of a mpg123_handle in detail:
 *  \param mh audio decoder handle
 *  \param rate The sample rate value (in Hertz).
 *  \param channels A combination of MPG123_STEREO and MPG123_MONO.
 *  \param encodings A combination of accepted encodings for rate and channels, p.ex MPG123_ENC_SIGNED16 | MPG123_ENC_ULAW_8 (or 0 for no support). Please note that some encodings may not be supported in the library build and thus will be ignored here.
 *  \return MPG123_OK on success, MPG123_ERR if there was an error. */
EXPORT int mpg123_format(mpg123_handle *mh, long rate, int channels, int encodings);

/** Check to see if a specific format at a specific rate is supported 
 *  by mpg123_handle.
 *  \return 0 for no support (that includes invalid parameters), MPG123_STEREO, 
 *          MPG123_MONO or MPG123_STEREO|MPG123_MONO. */
EXPORT int mpg123_format_support(mpg123_handle *mh, long rate, int encoding);

/** Get the current output format written to the addresses givenr. */
EXPORT int mpg123_getformat(mpg123_handle *mh, long *rate, int *channels, int *encoding);

/*@}*/


/** \defgroup mpg123_input mpg123 file input and decoding
 *
 * Functions for input bitstream and decoding operations.
 *
 * @{
 */

/* reading samples / triggering decoding, possible return values: */
/** Enumeration of the error codes returned by libmpg123 functions. */

/** Open and prepare to decode the specified file by filesystem path.
 *  This does not open HTTP urls; libmpg123 contains no networking code.
 *  If you want to decode internet streams, use mpg123_open_fd() or mpg123_open_feed().
 */
EXPORT int mpg123_open(mpg123_handle *mh, const char *path);

/** Use an already opened file descriptor as the bitstream input
 *  mpg123_close() will _not_ close the file descriptor.
 */
EXPORT int mpg123_open_fd(mpg123_handle *mh, int fd);

/** Open a new bitstream and prepare for direct feeding
 *  This works together with mpg123_decode(); you are responsible for reading and feeding the input bitstream.
 */
EXPORT int mpg123_open_feed(mpg123_handle *mh);

/** Closes the source, if libmpg123 opened it. */
EXPORT int mpg123_close(mpg123_handle *mh);

/** Read from stream and decode up to outmemsize bytes.
 *  \param outmemory address of output buffer to write to
 *  \param outmemsize maximum number of bytes to write
 *  \param done address to store the number of actually decoded bytes to
 *  \return error/message code (watch out for MPG123_DONE and friends!) */
EXPORT int mpg123_read(mpg123_handle *mh, unsigned char *outmemory, size_t outmemsize, size_t *done);

/** Feed data for a stream that has been opened with mpg123_open_feed().
 *  It's give and take: You provide the bytestream, mpg123 gives you the decoded samples.
 *  \param in input buffer
 *  \param size number of input bytes
 *  \return error/message code. */
EXPORT int mpg123_feed(mpg123_handle *mh, const unsigned char *in, size_t size);

/** Decode MPEG Audio from inmemory to outmemory. 
 *  This is very close to a drop-in replacement for old mpglib.
 *  When you give zero-sized output buffer the input will be parsed until 
 *  decoded data is available. This enables you to get MPG123_NEW_FORMAT (and query it) 
 *  without taking decoded data.
 *  Think of this function being the union of mpg123_read() and mpg123_feed() (which it actually is, sort of;-).
 *  You can actually always decide if you want those specialized functions in separate steps or one call this one here.
 *  \param inmemory input buffer
 *  \param inmemsize number of input bytes
 *  \param outmemory output buffer
 *  \param outmemsize maximum number of output bytes
 *  \param done address to store the number of actually decoded bytes to
 *  \return error/message code (watch out especially for MPG123_NEED_MORE)
 */
EXPORT int mpg123_decode(mpg123_handle *mh, const unsigned char *inmemory, size_t inmemsize,
                         unsigned char *outmemory, size_t outmemsize, size_t *done);

/** Decode next MPEG frame to internal buffer
 *  or read a frame and return after setting a new format.
 *  \param num current frame offset gets stored there
 *  \param audio This pointer is set to the internal buffer to read the decoded audio from.
 *  \param bytes number of output bytes ready in the buffer
 */
EXPORT int mpg123_decode_frame(mpg123_handle *mh, off_t *num, unsigned char **audio, size_t *bytes);

/*@}*/


/** \defgroup mpg123_seek mpg123 position and seeking
 *
 * Functions querying and manipulating position in the decoded audio bitstream.
 * The position is measured in decoded audio samples, or MPEG frame offset for the specific functions.
 * If gapless code is in effect, the positions are adjusted to compensate the skipped padding/delay - meaning, you should not care about that at all and just use the position defined for the samples you get out of the decoder;-)
 * The general usage is modelled after stdlib's ftell() and fseek().
 * Especially, the whence parameter for the seek functions has the same meaning as the one for fseek() and needs the same constants from stdlib.h: 
 * - SEEK_SET: set position to (or near to) specified offset
 * - SEEK_CUR: change position by offset from now
 * - SEEK_END: set position to offset from end
 *
 * Note that sample-afccurate seek only works when gapless support has been enabled at compile time; seek is frame-accurate otherwise.
 * Also, seeking is not guaranteed to work for all streams (underlying stream may not support it).
 *
 * @{
 */

/** Returns the current position in samples.
 *  On the next read, you'd get that sample. */
EXPORT off_t mpg123_tell(mpg123_handle *mh);

/** Returns the frame number that the next read will give you data from. */
EXPORT off_t mpg123_tellframe(mpg123_handle *mh);

/** Seek to a desired sample offset. 
 *  Set whence to SEEK_SET, SEEK_CUR or SEEK_END.
 *  \return The resulting offset >= 0 or error/message code */
EXPORT off_t mpg123_seek(mpg123_handle *mh, off_t sampleoff, int whence);

/** Seek to a desired sample offset in data feeding mode. 
 *  This just prepares things to be right only if you ensure that the next chunk of input data will be from input_offset byte position.
 *  \param input_offset The position it expects to be at the 
 *                      next time data is fed to mpg123_decode().
 *  \return The resulting offset >= 0 or error/message code */
EXPORT off_t mpg123_feedseek(mpg123_handle *mh, off_t sampleoff, int whence, off_t *input_offset);

/** Seek to a desired MPEG frame index.
 *  Set whence to SEEK_SET, SEEK_CUR or SEEK_END.
 *  \return The resulting offset >= 0 or error/message code */
EXPORT off_t mpg123_seek_frame(mpg123_handle *mh, off_t frameoff, int whence);

/** Return a MPEG frame offset corresponding to an offset in seconds.
 *  This assumes that the samples per frame do not change in the file/stream, which is a good assumption for any sane file/stream only.
 *  \return frame offset >= 0 or error/message code */
EXPORT off_t mpg123_timeframe(mpg123_handle *mh, double sec);

/** Give access to the frame index table that is managed for seeking.
 *  You are asked not to modify the values... unless you are really aware of what you are doing.
 *  \param offsets pointer to the index array
 *  \param step    one index byte offset advances this many MPEG frames
 *  \param fill    number of recorded index offsets; size of the array */
EXPORT int mpg123_index(mpg123_handle *mh, off_t **offsets, off_t *step, size_t *fill);

/** Get information about current and remaining frames/seconds.
 *  WARNING: This function is there because of special usage by standalone mpg123 and may be removed in the final version of libmpg123!
 *  You provide an offset (in frames) from now and a number of output bytes 
 *  served by libmpg123 but not yet played. You get the projected current frame 
 *  and seconds, as well as the remaining frames/seconds. This does _not_ care 
 *  about skipped samples due to gapless playback. */
EXPORT int mpg123_position( mpg123_handle *mh, off_t frame_offset,
                            off_t buffered_bytes,  off_t *current_frame,  
                            off_t *frames_left, double *current_seconds,
                            double *seconds_left);

/*@}*/


/** \defgroup mpg123_voleq mpg123 volume and equalizer
 *
 * @{
 */

enum mpg123_channels
{
	 MPG123_LEFT=0x1	/**< The Left Channel. */
	,MPG123_RIGHT=0x2	/**< The Right Channel. */
	,MPG123_LR=0x3	/**< Both left and right channel; same as MPG123_LEFT|MPG123_RIGHT */
};

/** Set the 32 Band Audio Equalizer settings.
 *  \param channel Can be MPG123_LEFT, MPG123_RIGHT or MPG123_LEFT|MPG123_RIGHT for both.
 *  \param band The equaliser band to change (from 0 to 31)
 *  \param val The (linear) adjustment factor. */
EXPORT int mpg123_eq(mpg123_handle *mh, enum mpg123_channels channel, int band, double val);

/** Reset the 32 Band Audio Equalizer settings to flat */
EXPORT int mpg123_reset_eq(mpg123_handle *mh);

/** Set the absolute output volume including the RVA setting, 
 *  vol<0 just applies (a possibly changed) RVA setting. */
EXPORT int mpg123_volume(mpg123_handle *mh, double vol);

/** Adjust output volume including the RVA setting by chosen amount */
EXPORT int mpg123_volume_change(mpg123_handle *mh, double change);

/** Return current volume setting, the actual value due to RVA, and the RVA 
 *  adjustment itself. It's all as double float value to abstract the sample 
 *  format. The volume values are linear factors / amplitudes (not percent) 
 *  and the RVA value is in decibels. */
EXPORT int mpg123_getvolume(mpg123_handle *mh, double *base, double *really, double *rva_db);

/* TODO: Set some preamp in addition / to replace internal RVA handling? */

/*@}*/


/** \defgroup mpg123_status mpg123 status and information
 *
 * @{
 */

/** Enumeration of the mode types of Variable Bitrate */
enum mpg123_vbr {
	MPG123_CBR=0,	/**< Constant Bitrate Mode (default) */
	MPG123_VBR,		/**< Variable Bitrate Mode */
	MPG123_ABR		/**< Average Bitrate Mode */
};

/** Enumeration of the MPEG Versions */
enum mpg123_version {
	MPG123_1_0=0,	/**< MPEG Version 1.0 */
	MPG123_2_0,		/**< MPEG Version 2.0 */
	MPG123_2_5		/**< MPEG Version 2.5 */
};


/** Enumeration of the MPEG Audio mode.
 *  Only the mono mode has 1 channel, the others have 2 channels. */
enum mpg123_mode {
	MPG123_M_STEREO=0,	/**< Standard Stereo. */
	MPG123_M_JOINT,		/**< Joint Stereo. */
	MPG123_M_DUAL,		/**< Dual Channel. */
	MPG123_M_MONO		/**< Single Channel. */
};


/** Enumeration of the MPEG Audio flag bits */
enum mpg123_flags {
	MPG123_CRC=0x1,			/**< The bitstream is error protected using 16-bit CRC. */
	MPG123_COPYRIGHT=0x2,	/**< The bitstream is copyrighted. */
	MPG123_PRIVATE=0x4,		/**< The private bit has been set. */
	MPG123_ORIGINAL=0x8	/**< The bitstream is an original, not a copy. */
};

/** Data structure for storing information about a frame of MPEG Audio */
struct mpg123_frameinfo
{
	enum mpg123_version version;	/**< The MPEG version (1.0/2.0/2.5). */
	int layer;						/**< The MPEG Audio Layer (MP1/MP2/MP3). */
	long rate; 						/**< The sampling rate in Hz. */
	enum mpg123_mode mode;			/**< The audio mode (Mono, Stereo, Joint-stero, Dual Channel). */
	int mode_ext;					/**< The mode extension bit flag. */
	int framesize;					/**< The size of the frame (in bytes). */
	enum mpg123_flags flags;		/**< MPEG Audio flag bits. */
	int emphasis;					/**< The emphasis type. */
	int bitrate;					/**< Bitrate of the frame (kbps). */
	int abr_rate;					/**< The target average bitrate. */
	enum mpg123_vbr vbr;			/**< The VBR mode. */
};

/** Get frame information about the MPEG audio bitstream and store it in a mpg123_frameinfo structure. */
EXPORT int mpg123_info(mpg123_handle *mh, struct mpg123_frameinfo *mi);

/** Get the safe output buffer size for all cases (when you want to replace the internal buffer) */
EXPORT size_t mpg123_safe_buffer(); 

/** Make a full parsing scan of each frame in the file. ID3 tags are found. An accurate length 
 *  value is stored. Seek index will be filled. A seek back to current position 
 *  is performed. At all, this function refuses work when stream is 
 *  not seekable. 
 *  \return MPG123_OK or MPG123_ERR.
 */
EXPORT int mpg123_scan(mpg123_handle *mh);

/** Return, if possible, the full (expected) length of current track in samples.
  * \return length >= 0 or MPG123_ERR if there is no length guess possible. */
EXPORT off_t mpg123_length(mpg123_handle *mh);

/** Override the value for file size in bytes.
  * Useful for getting sensible track length values in feed mode or for HTTP streams.
  * \return MPG123_OK or MPG123_ERR */
EXPORT int mpg123_set_filesize(mpg123_handle *mh, off_t size);

/** Returns the time (seconds) per frame; <0 is error. */
EXPORT double mpg123_tpf(mpg123_handle *mh);

/** Get and reset the clip count. */
EXPORT long mpg123_clip(mpg123_handle *mh);

/*@}*/


/** \defgroup mpg123_metadata mpg123 metadata handling
 *
 * Functions to retrieve the metadata from MPEG Audio files and streams.
 * Also includes string handling functions.
 *
 * @{
 */

/** Data structure for storing strings in a safer way than a standard C-String.
 *  Can also hold a number of null-terminated strings. */
typedef struct 
{
	char* p;     /**< pointer to the string data */
	size_t size; /**< raw number of bytes allocated */
	size_t fill; /**< number of used bytes (including closing zero byte) */
} mpg123_string;

/** Create and allocate memory for a new mpg123_string */
EXPORT void mpg123_init_string(mpg123_string* sb);

/** Free-up mempory for an existing mpg123_string */
EXPORT void mpg123_free_string(mpg123_string* sb);

/** Change the size of a mpg123_string
 *  \return 0 on error, 1 on success */
EXPORT int  mpg123_resize_string(mpg123_string* sb, size_t news);

/** Increase size of a mpg123_string if necessary (it may stay larger).
 *  Note that the functions for adding and setting in current libmpg123 use this instead of mpg123_resize_string().
 *  That way, you can preallocate memory and safely work afterwards with pieces.
 *  \return 0 on error, 1 on success */
EXPORT int  mpg123_grow_string(mpg123_string* sb, size_t news);

/** Copy the contents of one mpg123_string string to another.
 *  \return 0 on error, 1 on success */
EXPORT int  mpg123_copy_string(mpg123_string* from, mpg123_string* to);

/** Append a C-String to an mpg123_string
 *  \return 0 on error, 1 on success */
EXPORT int  mpg123_add_string(mpg123_string* sb, const char* stuff);

/** Append a C-substring to an mpg123 string
 *  \return 0 on error, 1 on success
 *  \param from offset to copy from
 *  \param count number of characters to copy (a null-byte is always appended) */
EXPORT int  mpg123_add_substring(mpg123_string *sb, const char *stuff, size_t from, size_t count);

/** Set the conents of a mpg123_string to a C-string
 *  \return 0 on error, 1 on success */
EXPORT int  mpg123_set_string(mpg123_string* sb, const char* stuff);

/** Set the contents of a mpg123_string to a C-substring
 *  \return 0 on error, 1 on success
 *  \param from offset to copy from
 *  \param count number of characters to copy (a null-byte is always appended) */
EXPORT int  mpg123_set_substring(mpg123_string *sb, const char *stuff, size_t from, size_t count);


/** Sub data structure for ID3v2, for storing various text fields (including comments).
 *  This is for ID3v2 COMM, TXXX and all the other text fields.
 *  Only COMM and TXXX have a description, only COMM has a language.
 *  You should consult the ID3v2 specification for the use of the various text fields ("frames" in ID3v2 documentation, I use "fields" here to separate from MPEG frames). */
typedef struct
{
	char lang[3]; /**< Three-letter language code (not terminated). */
	char id[4];   /**< The ID3v2 text field id, like TALB, TPE2, ... (4 characters, no string termination). */
	mpg123_string description; /**< Empty for the generic comment... */
	mpg123_string text;        /**< ... */
} mpg123_text;

/** Data structure for storing IDV3v2 tags.
 *  This structure is not a direct binary mapping with the file contents.
 *  The ID3v2 text frames are allowed to contain multiple strings.
 *  So check for null bytes until you reach the mpg123_string fill.
 *  All text is encoded in UTF-8. */
typedef struct
{
	unsigned char version; /**< 3 or 4 for ID3v2.3 or ID3v2.4. */
	mpg123_string *title;   /**< Title string (pointer into text_list). */
	mpg123_string *artist;  /**< Artist string (pointer into text_list). */
	mpg123_string *album;   /**< Album string (pointer into text_list). */
	mpg123_string *year;    /**< The year as a string (pointer into text_list). */
	mpg123_string *genre;   /**< Genre String (pointer into text_list). The genre string(s) may very well need postprocessing, esp. for ID3v2.3. */
	mpg123_string *comment; /**< Pointer to last encountered comment text with empty description. */
	/* Encountered ID3v2 fields are appended to these lists.
	   There can be multiple occurences, the pointers above always point to the last encountered data. */
	mpg123_text    *comment_list; /**< Array of comments. */
	size_t          comments;     /**< Number of comments. */
	mpg123_text    *text;         /**< Array of ID3v2 text fields */
	size_t          texts;        /**< Numer of text fields. */
	mpg123_text    *extra;        /**< The array of extra (TXXX) fields. */
	size_t          extras;       /**< Number of extra text (TXXX) fields. */
} mpg123_id3v2;

/** Data structure for ID3v1 tags (the last 128 bytes of a file).
 *  Don't take anything for granted (like string termination)!
 *  Also note the change ID3v1.1 did: comment[28] = 0; comment[19] = track_number
 *  It is your task to support ID3v1 only or ID3v1.1 ...*/
typedef struct
{
	char tag[3];         /**< Always the string "TAG", the classic intro. */
	char title[30];      /**< Title string.  */
	char artist[30];     /**< Artist string. */
	char album[30];      /**< Album string. */
	char year[4];        /**< Year string. */
	char comment[30];    /**< Comment string. */
	unsigned char genre; /**< Genre index. */
} mpg123_id3v1;

#define MPG123_ID3     0x3 /**< 0011 There is some ID3 info. Also matches 0010 or NEW_ID3. */
#define MPG123_NEW_ID3 0x1 /**< 0001 There is ID3 info that changed since last call to mpg123_id3. */
#define MPG123_ICY     0xc /**< 1100 There is some ICY info. Also matches 0100 or NEW_ICY.*/
#define MPG123_NEW_ICY 0x4 /**< 0100 There is ICY info that changed since last call to mpg123_icy. */

/** Query if there is (new) meta info, be it ID3 or ICY (or something new in future).
   The check function returns a combination of flags. */
EXPORT int mpg123_meta_check(mpg123_handle *mh); /* On error (no valid handle) just 0 is returned. */

/** Point v1 and v2 to existing data structures wich may change on any next read/decode function call.
 *  v1 and/or v2 can be set to NULL when there is no corresponding data.
 *  \return Return value is MPG123_OK or MPG123_ERR,  */
EXPORT int mpg123_id3(mpg123_handle *mh, mpg123_id3v1 **v1, mpg123_id3v2 **v2);

/** Point icy_meta to existing data structure wich may change on any next read/decode function call.
 *  \return Return value is MPG123_OK or MPG123_ERR,  */
EXPORT int mpg123_icy(mpg123_handle *mh, char **icy_meta); /* same for ICY meta string */

/** Decode from windows-1252 (the encoding ICY metainfo used) to UTF-8.
 *  \param icy_text The input data in ICY encoding
 *  \return pointer to newly allocated buffer with UTF-8 data (You free() it!) */
EXPORT char* mpg123_icy2utf8(const char* icy_text);


/* @} */


/** \defgroup mpg123_advpar mpg123 advanced parameter API
 *
 *  Direct access to a parameter set without full handle around it.
 *	Possible uses:
 *    - Influence behaviour of library _during_ initialization of handle (MPG123_VERBOSE).
 *    - Use one set of parameters for multiple handles.
 *
 *	The functions for handling mpg123_pars (mpg123_par() and mpg123_fmt() 
 *  family) directly return a fully qualified mpg123 error code, the ones 
 *  operating on full handles normally MPG123_OK or MPG123_ERR, storing the 
 *  specific error code itseld inside the handle. 
 *
 * @{
 */

/** Opaque structure for the libmpg123 decoder parameters. */
struct mpg123_pars_struct;

/** Opaque structure for the libmpg123 decoder parameters. */
typedef struct mpg123_pars_struct   mpg123_pars;

/** Create a handle with preset parameters. */
EXPORT mpg123_handle *mpg123_parnew(mpg123_pars *mp, const char* decoder, int *error);

/** Allocate memory for and return a pointer to a new mpg123_pars */
EXPORT mpg123_pars *mpg123_new_pars(int *error);

/** Delete and free up memory used by a mpg123_pars data structure */
EXPORT void         mpg123_delete_pars(mpg123_pars* mp);

/** Configure mpg123 parameters to accept no output format at all, 
 * use before specifying supported formats with mpg123_format */
EXPORT int mpg123_fmt_none(mpg123_pars *mp);

/** Configure mpg123 parameters to accept all formats 
 *  (also any custom rate you may set) -- this is default. */
EXPORT int mpg123_fmt_all(mpg123_pars *mp);

/** Set the audio format support of a mpg123_pars in detail:
	\param rate The sample rate value (in Hertz).
	\param channels A combination of MPG123_STEREO and MPG123_MONO.
	\param encodings A combination of accepted encodings for rate and channels, p.ex MPG123_ENC_SIGNED16|MPG123_ENC_ULAW_8 (or 0 for no support).
	\return 0 on success, -1 if there was an error. /
*/
EXPORT int mpg123_fmt(mpg123_pars *mh, long rate, int channels, int encodings); /* 0 is good, -1 is error */

/** Check to see if a specific format at a specific rate is supported 
 *  by mpg123_pars.
 *  \return 0 for no support (that includes invalid parameters), MPG123_STEREO, 
 *          MPG123_MONO or MPG123_STEREO|MPG123_MONO. */
EXPORT int mpg123_fmt_support(mpg123_pars *mh,   long rate, int encoding);

/** Set a specific parameter, for a specific mpg123_pars, using a parameter 
 *  type key chosen from the mpg123_parms enumeration, to the specified value. */
EXPORT int mpg123_par(mpg123_pars *mp, enum mpg123_parms type, long value, double fvalue);

/** Get a specific parameter, for a specific mpg123_pars. 
 *  See the mpg123_parms enumeration for a list of available parameters. */
EXPORT int mpg123_getpar(mpg123_pars *mp, enum mpg123_parms type, long *val, double *fval);

/* @} */


/** \defgroup mpg123_lowio mpg123 low level I/O
  * You may want to do tricky stuff with I/O that does not work with mpg123's default file access or you want to make it decode into your own pocket...
  *
  * @{ */

/** Replace default internal buffer with user-supplied buffer.
  * Instead of working on it's own private buffer, mpg123 will directly use the one you provide for storing decoded audio. */
EXPORT int mpg123_replace_buffer(mpg123_handle *mh, unsigned char *data, size_t size);

/** The max size of one frame's decoded output with current settings.
 *  Use that to determine an appropriate minimum buffer size for decoding one frame. */
EXPORT size_t mpg123_outblock(mpg123_handle *mh);

/** Replace low-level stream access functions; read and lseek as known in POSIX.
 *  You can use this to make any fancy file opening/closing yourself, 
 *  using open_fd to set the file descriptor for your read/lseek (doesn't need to be a "real" file descriptor...).
 *  Setting a function to NULL means that the default internal read is 
 *  used (active from next mpg123_open call on). */
EXPORT int mpg123_replace_reader( mpg123_handle *mh,
                                  ssize_t (*r_read) (int, void *, size_t),
                                  off_t   (*r_lseek)(int, off_t, int) );

/* @} */


#ifdef __cplusplus
}
#endif

#endif
