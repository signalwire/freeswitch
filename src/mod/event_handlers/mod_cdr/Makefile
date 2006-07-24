LDFLAGS += -I/usr/include/mysql -L/usr/lib64/mysql -lmysqlclient -lz -lcrypt -lnsl -lm -lssl -lcrypto
CFLAGS += -I/usr/include/mysql -L/usr/lib64/mysql 
CPPCC = g++
OBJS=cdrcontainer.o basecdr.o baseregistry.o mysqlcdr.o pddcdr.o


all:	depends $(OBJS) $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(OBJS) $(MODNAME).cpp
	$(CPPCC) $(CFLAGS) -fPIC -c $(MODNAME).cpp -o $(MODNAME).o 
	$(CPPCC) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)


%.o:  %.cpp
	$(CPPCC) -Wall -Werror -fPIC $(CFLAGS) -c -o $@ $<

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
