LOCAL_CFLAGS=-I../src/include -I/usr/java/jdk1.6.0_14/include -I/usr/java/jdk1.6.0_14/include/linux -I/usr/lib/jvm/java-6-openjdk/include/ -I/usr/lib/jvm/java/include -I/usr/lib/jvm/java/include/linux
GCC_WARNING_JUNK=-w
CLASSES=org/freeswitch/esl/*

all: esl.jar

esl_wrap.cpp:
	swig2.0 -module esl -java -c++ $(LOCAL_CFLAGS) -package org.freeswitch.esl -outdir org/freeswitch/esl -o esl_wrap.cpp ../ESL.i

esl_wrap.o: esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(LOCAL_CFLAGS) $(CXXFLAGS) $(GCC_WARNING_JUNK) $(PERL_INC) -c esl_wrap.cpp -o esl_wrap.o

libesljni.so: esl_wrap.o
	$(CXX) $(SOLINK) -o libesljni.so esl_wrap.o $(MYLIB)

esl.jar: libesljni.so
	mkdir -p classes
	javac -sourcepath org -d classes -source 6 -target 6 $(CLASSES)
	jar cf esl.jar -C classes org

clean:
	rm -f *.o *.so *~ *.jar

swigclean:
	rm -f esl_wrap.* 

reswig:	swigclean esl_wrap.cpp

