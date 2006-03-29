lib_LTLIBRARIES = libjrtp.la
libjrtp_la_SOURCES = rtpdebug.cpp rtpsession.cpp rtpconfig_win.h \
		     rtperrors.cpp rtpudpv4transmitter.cpp \
		     rtcpsdesinfo.cpp rtppollthread.cpp rtppacket.cpp \
		     rtppacketbuilder.cpp rtpsessionparams.cpp \
		     rtpsources.cpp rtpinternalsourcedata.cpp \
		     rtpsourcedata.cpp rtpipv4address.cpp \
		     rtcpcompoundpacket.cpp jrtp4c.cpp\
		     rtcpapppacket.cpp rtcpbyepacket.cpp \
		     rtcprrpacket.cpp rtcpsdespacket.cpp \
		     rtcpsrpacket.cpp rtplibraryversion.cpp \
		     rtcppacket.cpp rtcpcompoundpacketbuilder.cpp \
		     rtprandom.cpp rtcpscheduler.cpp \
		     rtcppacketbuilder.cpp rtpsessionsources.cpp \
		     rtpcollisionlist.cpp rtpipv6address.cpp \
		     rtpudpv6transmitter.cpp rtptimeutilities.cpp
libjrtp_la_LDFLAGS = -release 3.5.2 @RTP_LINKLIBS@
libjrtpinclude_HEADERS = rtcpapppacket.h rtcpbyepacket.h rtcpcompoundpacket.h \
			 rtcpcompoundpacketbuilder.h rtcppacket.h rtcppacketbuilder.h \
			 rtcprrpacket.h rtcpscheduler.h rtcpsdesinfo.h rtcpsdespacket.h \
			 rtcpsrpacket.h rtcpunknownpacket.h rtpaddress.h rtpconfig.h \
			 rtpconfig_win.h rtpdebug.h rtpdefines.h rtperrors.h rtphashtable.h \
			 rtpconfig_unix.h rtpinternalsourcedata.h rtpipv4address.h \
			 rtpipv4destination.h rtpkeyhashtable.h rtplibraryversion.h rtppacket.h \
			 rtppacketbuilder.h rtppollthread.h rtprandom.h rtprawpacket.h \
			 rtpsession.h rtpsessionparams.h rtpsourcedata.h rtpsources.h \
			 rtpstructs.h rtptimeutilities.h rtptransmitter.h \
			 rtptypes_win.h rtpudpv4transmitter.h rtpsessionsources.h \
			 rtpcollisionlist.h rtpipv6address.h rtpipv6destination.h \
			 rtpudpv6transmitter.h rtptypes.h rtptypes_unix.h \
			 jrtp4c.h
EXTRA_DIST = rtpconfig_unix.h.in
libjrtpincludedir = ${includedir}/jrtplib3



