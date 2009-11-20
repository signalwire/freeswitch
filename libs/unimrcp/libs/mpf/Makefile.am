MAINTAINERCLEANFILES 	 = Makefile.in

INCLUDES                 = -I$(top_srcdir)/libs/mpf/codecs \
                           -I$(top_srcdir)/libs/mpf/include \
                           -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES       = libmpf.la

include_HEADERS          = codecs/g711/g711.h \
                           include/mpf.h \
                           include/mpf_activity_detector.h \
                           include/mpf_audio_file_descriptor.h \
                           include/mpf_audio_file_stream.h \
                           include/mpf_bridge.h \
                           include/mpf_buffer.h \
                           include/mpf_codec.h \
                           include/mpf_codec_descriptor.h \
                           include/mpf_codec_manager.h \
                           include/mpf_context.h \
                           include/mpf_dtmf_detector.h \
                           include/mpf_dtmf_generator.h \
                           include/mpf_engine.h \
                           include/mpf_frame.h \
                           include/mpf_frame_buffer.h \
                           include/mpf_message.h \
                           include/mpf_mixer.h \
                           include/mpf_multiplier.h \
                           include/mpf_named_event.h \
                           include/mpf_object.h \
                           include/mpf_stream.h \
                           include/mpf_stream_descriptor.h \
                           include/mpf_termination.h \
                           include/mpf_termination_factory.h \
                           include/mpf_rtp_termination_factory.h \
                           include/mpf_file_termination_factory.h \
                           include/mpf_scheduler.h \
                           include/mpf_timer_manager.h \
                           include/mpf_types.h \
                           include/mpf_encoder.h \
                           include/mpf_decoder.h \
                           include/mpf_jitter_buffer.h \
                           include/mpf_rtp_header.h \
                           include/mpf_rtp_descriptor.h \
                           include/mpf_rtp_stream.h \
                           include/mpf_rtp_stat.h \
                           include/mpf_rtp_defs.h \
                           include/mpf_rtp_attribs.h \
                           include/mpf_rtp_pt.h \
                           include/mpf_rtcp_packet.h \
                           include/mpf_resampler.h

libmpf_la_SOURCES        = codecs/g711/g711.c \
                           src/mpf_activity_detector.c \
                           src/mpf_audio_file_stream.c \
                           src/mpf_bridge.c \
                           src/mpf_buffer.c \
                           src/mpf_codec_descriptor.c \
                           src/mpf_codec_g711.c \
                           src/mpf_codec_linear.c \
                           src/mpf_codec_manager.c \
                           src/mpf_context.c \
                           src/mpf_dtmf_detector.c \
                           src/mpf_dtmf_generator.c \
                           src/mpf_engine.c \
                           src/mpf_mixer.c \
                           src/mpf_multiplier.c \
                           src/mpf_named_event.c \
                           src/mpf_termination.c \
                           src/mpf_termination_factory.c \
                           src/mpf_rtp_termination_factory.c \
                           src/mpf_file_termination_factory.c \
                           src/mpf_frame_buffer.c \
                           src/mpf_scheduler.c \
                           src/mpf_timer_manager.c \
                           src/mpf_encoder.c \
                           src/mpf_decoder.c \
                           src/mpf_jitter_buffer.c \
                           src/mpf_rtp_stream.c \
                           src/mpf_rtp_attribs.c \
                           src/mpf_resampler.c \
                           src/mpf_stream.c
