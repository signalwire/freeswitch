LDFLAGS += -L/opt/lumenvox/engine_7.0/lib -llv_lvspeechport
CFLAGS += -fpermissive -Wno-deprecated -Wno-conversion -fpermissive -Wno-unused -Wno-comment -Wno-sign-compare -Wno-conversion -Wno-reorder -I/opt/lumenvox/engine_7.0/include
LINKER=g++
CC=g++

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).cpp
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).cpp -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
