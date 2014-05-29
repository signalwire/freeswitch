include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_java

mod_LTLIBRARIES = mod_java.la
mod_java_la_SOURCES  = mod_java.cpp freeswitch_java.cpp switch_swig_wrap.cpp modjava.c
mod_java_la_CFLAGS   = $(AM_CFLAGS)
mod_java_la_CPPFLAGS = -I/usr/lib/jvm/java-gcj/include
mod_java_la_CPPFLAGS+= -I/usr/lib/jvm/java-gcj/include/linux
mod_java_la_CPPFLAGS+= $(AM_CPPFLAGS)
mod_java_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_java_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

# Without -fno-strict-aliasing, g++ generates invalid code for Java_org_freeswitch_freeswitchJNI_SWIGJavaSessionUpcast, which segfaults
mod_java_la_CFLAGS  += $(JAVA_FLAGS) -fno-strict-aliasing

CLASSES=src/org/freeswitch/Launcher.java \
	src/org/freeswitch/HangupHook.java \
	src/org/freeswitch/DTMFCallback.java \
	src/org/freeswitch/FreeswitchScript.java \
	src/org/freeswitch/Event.java \
	src/org/freeswitch/swig/*

BUILT_SOURCES=classes

classes:
	mkdir -p classes
	javac -sourcepath src -d classes $(CLASSES)
	jar cf freeswitch.jar -C classes org

reswig:
	rm -f switch_swig_wrap.cpp
	swig -java -c++ -I../../../include -package org.freeswitch.swig -outdir src/org/freeswitch/swig -o switch_swig_wrap.cpp mod_java.i

install-data-local:
	cp freeswitch.jar $(DESTDIR)@scriptdir@

