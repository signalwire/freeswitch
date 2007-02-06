#CFLAGS  += -DSWITCH_QUEUE_ENHANCED
#LDFLAGS += -lcurl
CPPCC = g++
OBJS=cdrcontainer.o basecdr.o baseregistry.o pddcdr.o csvcdr.o xmlcdr.o sqlitecdr.o

ifneq ($(shell which mysql_config),)
  CFLAGS  += $(shell mysql_config --include)
  LDFLAGS += $(shell mysql_config --libs)
  OBJS    += mysqlcdr.o
endif

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
