# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to imapact your build

ifeq ($(OSARCH),Darwin)
LOCAL_CFLAGS += -I/Library/Frameworks/swift.framework/Versions/4/Headers
LOCAL_LDFLAGS += -framework swift
else
LOCAL_LDFLAGS += -L/opt/swift/lib -lswift -lceplex_us -lceplang_en -lm
LOCAL_CFLAGS += -I/opt/swift/include
endif
LOCAL_OBJS=
include $(BASE)/build/modmake.rules

