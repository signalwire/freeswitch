JSFILES=src/jquery.FSRTC.js src/jquery.jsonrpcclient.js src/jquery.verto.js

all: jsmin verto-min.js

jsmin: jsmin.c
	$(CC) $< -o $@

verto-min.js: jsmin $(JSFILES)
	cat $(JSFILES) | ./jsmin > $@

verto-max.js: jsmin $(JSFILES)
	cat $(JSFILES) > $@

clean:
	rm -f verto-min.js verto-max.js jsmin *~

install-demo: all
	cp verto-min.js ../demo/js

install-maxdemo: all verto-max.js
	cp verto-max.js ../demo/js/verto-min.js

install-video_demo: all
	cp verto-min.js ../video_demo/js

install-video_maxdemo: all verto-max.js
	cp verto-max.js ../video_demo/js/verto-min.js
