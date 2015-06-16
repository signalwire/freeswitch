include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_cv

if HAVE_YUV
if HAVE_VPX
if HAVE_OPENCV

mod_LTLIBRARIES = mod_cv.la
mod_cv_la_SOURCES  = mod_cv.cpp
mod_cv_la_CXXFLAGS   = $(AM_CXXFLAGS) $(OPENCV_CFLAGS)
mod_cv_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(OPENCV_LIBS)
mod_cv_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz

else
install: error
all: error
error:
	$(error You must install libopencv-dev to build mod_cv)
endif

else
install: error
all: error
error:
	$(error You must install libvpx2-dev to build mod_cv)
endif


else
install: error
all: error
error:
	$(error You must install libyuv-dev to build mod_cv)
endif

