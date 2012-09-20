all: css userguide pdf changelog perl 

userguide:	txt/userguide.txt
	asciidoc --unsafe --out-file=html/userguide.html -a linkcss=1 -a theme=tdh txt/userguide.txt 

changelog:	txt/ChangeLog.txt
	asciidoc --out-file=html/ChangeLog.html txt/ChangeLog.txt 

.PHONY: pdf

pdf: txt/userguide.txt
	a2x -f pdf $<
	mv txt/userguide.pdf pdf/
	cd html && ln -sf ../pdf/userguide.pdf userguide.pdf
	rm txt/userguide.xml

perl: txt/perl.txt
	asciidoc --unsafe --out-file=html/perl.html -a linkcss=1 -a theme=tdh txt/perl.txt 

css: html/toc.css
	cat /etc/asciidoc/stylesheets/xhtml11.css html/toc.css > html/tdh.css
	cp /etc/asciidoc/stylesheets/xhtml11-quirks.css html/tdh-quirks.css 

docbook: txt/userguide.txt
	asciidoc -b docbook --out-file=/tmp/userguide.xml txt/userguide.txt 
	xmlto -o html html-nochunks /tmp/userguide.xml 
