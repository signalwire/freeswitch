cmake_minimum_required (VERSION 2.8)
project (mpf)

# Set header files
set (MPF_HEADERS
	include/mpf.h
	include/mpf_activity_detector.h
	include/mpf_audio_file_descriptor.h
	include/mpf_audio_file_stream.h
	include/mpf_bridge.h
	include/mpf_buffer.h
	include/mpf_codec.h
	include/mpf_codec_descriptor.h
	include/mpf_codec_manager.h
	include/mpf_context.h
	include/mpf_dtmf_detector.h
	include/mpf_dtmf_generator.h
	include/mpf_engine.h
	include/mpf_engine_factory.h
	include/mpf_frame.h
	include/mpf_frame_buffer.h
	include/mpf_message.h
	include/mpf_mixer.h
	include/mpf_multiplier.h
	include/mpf_named_event.h
	include/mpf_object.h
	include/mpf_stream.h
	include/mpf_stream_descriptor.h
	include/mpf_termination.h
	include/mpf_termination_factory.h
	include/mpf_rtp_termination_factory.h
	include/mpf_file_termination_factory.h
	include/mpf_scheduler.h
	include/mpf_types.h
	include/mpf_encoder.h
	include/mpf_decoder.h
	include/mpf_jitter_buffer.h
	include/mpf_rtp_header.h
	include/mpf_rtp_descriptor.h
	include/mpf_rtp_stream.h
	include/mpf_rtp_stat.h
	include/mpf_rtp_defs.h
	include/mpf_rtp_attribs.h
	include/mpf_rtp_pt.h
	include/mpf_rtcp_packet.h
	include/mpf_resampler.h
)
source_group ("include" FILES ${MPF_HEADERS})

# Set source files
set (MPF_SOURCES
	src/mpf_activity_detector.c
	src/mpf_audio_file_stream.c
	src/mpf_bridge.c
	src/mpf_buffer.c
	src/mpf_codec_descriptor.c
	src/mpf_codec_g711.c
	src/mpf_codec_linear.c
	src/mpf_codec_manager.c
	src/mpf_context.c
	src/mpf_dtmf_detector.c
	src/mpf_dtmf_generator.c
	src/mpf_engine.c
	src/mpf_engine_factory.c
	src/mpf_mixer.c
	src/mpf_multiplier.c
	src/mpf_named_event.c
	src/mpf_termination.c
	src/mpf_termination_factory.c
	src/mpf_rtp_termination_factory.c
	src/mpf_file_termination_factory.c
	src/mpf_frame_buffer.c
	src/mpf_scheduler.c
	src/mpf_encoder.c
	src/mpf_decoder.c
	src/mpf_jitter_buffer.c
	src/mpf_rtp_stream.c
	src/mpf_rtp_attribs.c
	src/mpf_resampler.c
	src/mpf_stream.c
)
source_group ("src" FILES ${MPF_SOURCES})

set (MPF_G711_HEADERS
	codecs/g711/g711.h
)
set (MPF_G711_SOURCES
	codecs/g711/g711.c
)
source_group ("codecs\\g711" FILES ${MPF_G711_HEADERS} ${MPF_G711_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${MPF_SOURCES} ${MPF_G711_SOURCES} ${MPF_HEADERS} ${MPF_G711_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "libs")

# Preprocessor definitions
add_definitions (
	${MPF_DEFINES}
	${APR_TOOLKIT_DEFINES} 
	${APR_DEFINES} 
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/codecs
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)
