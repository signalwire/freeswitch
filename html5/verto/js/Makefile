JSFILES=src/jquery.FSRTC.js src/jquery.jsonrpcclient.js src/jquery.verto.js

all: jsmin verto-min.js

jsmin: jsmin.c
	$(CC) $< -o $@

verto-min.js: jsmin $(JSFILES)
	cat $(JSFILES) | ./jsmin > $@

clean:
	rm -f verto-min.js jsmin *~

install-demo: all
	cp verto-min.js ../demo/js
