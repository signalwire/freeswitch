/*

The contents of this file are subject to the Mozilla Public License
Version 1.1 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is MP4 Helper Library to Freeswitch MP4 module.

The Initial Developer of the Original Code is 
Paulo Rogério Panhoto <paulo@voicetechnology.com.br>.
Portions created by the Initial Developer are Copyright (C)
the Initial Developer. All Rights Reserved.

Contributor(s):

	Seven Du <dujinfang@gmail.com>

*/

#ifndef MP4_HELPER_HPP_
#define MP4_HELPER_HPP_

#include <mp4v2/mp4v2.h>
#include <string>
#include <exception>
#include <string>

typedef unsigned int u_int;

namespace MP4
{
	class Exception: public std::exception {
		public:
			Exception(const std::string & file, const std::string & error)
			: description_(file + ':' + error)
			{
			}
			
			const char * what() const throw()
			{
				return description_.c_str();
			}
			
			~Exception() throw()
			{
			}
			
		private:
			std::string description_;
	};
	
	struct RuntimeProperties {
		u_int32_t frame; // sampleID
		u_int16_t packetsPerFrame;
		u_int16_t packet; // packetID
		u_int32_t last_frame; // timestamp

		RuntimeProperties(): frame(0), packetsPerFrame(0), packet(0)
		{
		}
	};


	struct TrackProperties {
		MP4TrackId hint;
		MP4TrackId track;

		char * codecName;
		u_int8_t payload;
		u_int32_t clock;
		u_int32_t packetLength; // packet Length in time (ms)
		
		RuntimeProperties runtime;

		TrackProperties(): hint(MP4_INVALID_TRACK_ID), track(MP4_INVALID_TRACK_ID), 
			codecName(NULL), payload(0), clock(0), packetLength(0)
		{
		}
	};

	typedef TrackProperties AudioProperties;

	struct VideoProperties {
		TrackProperties track;
		std::string fmtp;

		VideoProperties()
		{
		}

		VideoProperties(const TrackProperties & rhs): track(rhs)
		{
		}
	};

	class Context {
	public:

		Context(const char * file, bool create = false);
		~Context();

		void open(const char * file);
		
		void create(const char * file);

		void close();

		// returns: TRUE = has more data, FALSE = end-of-stream or failure
		bool getVideoPacket(void * buffer, u_int & size, u_int & ts);

		// returns: TRUE = has more data, FALSE = end-of-stream or failure
		bool getAudioPacket(void * buffer, u_int & size, u_int & ts);

		bool isOpen() const { return fh != MP4_INVALID_FILE_HANDLE; }

		bool isSupported() const { return audio.track != MP4_INVALID_TRACK_ID && video.track.track != MP4_INVALID_TRACK_ID; }

		const AudioProperties & audioTrack() const { return audio; }

		const VideoProperties & videoTrack() const { return video; }

	private:
		MP4FileHandle fh;
		AudioProperties audio;

		VideoProperties video;

		// Prevent copy construction.
		Context(const Context &);

		bool getPacket(MP4TrackId hint, RuntimeProperties & rt,
				bool header, void * buffer, u_int & size, u_int & ts);

		void getTracks(const char * file);
	};
}
#endif
