LOCAL_CFLAGS=`python ./python-config --includes`
LOCAL_LDFLAGS=`python ./python-config --ldflags`
SITE_DIR=$(DESTDIR)/`python -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)"`

all: _ESL.so

esl_wrap.cpp:
	swig2.0 -module ESL -classic -python -c++ -DMULTIPLICITY -threads -I../src/include -o esl_wrap.cpp ../ESL.i

esl_wrap.o: esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(LOCAL_CFLAGS) -c esl_wrap.cpp -o esl_wrap.o

_ESL.so: esl_wrap.o
	$(CXX) $(SOLINK) esl_wrap.o $(MYLIB) $(LOCAL_LDFLAGS) -o _ESL.so -L. $(LIBS)

install: _ESL.so
	mkdir -p $(SITE_DIR)
	install -m 755 _ESL.so $(SITE_DIR)
	install -m 755 ESL.py $(SITE_DIR)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so

reswig:	swigclean esl_wrap.cpp

