# DSL = /usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/html/docbook.dsl
DSL = /usr/share/sgml/docbook/stylesheet/dsssl/modular/html/docbook.dsl
JADE = jade
HTML_RENDER = lynx -force_html -nolist -dump

SOURCES = API README

all:
	@for i in $(SOURCES); do \
		echo $$i - building html; \
		$(JADE) -t sgml -V nochunks -d $(DSL) $$i.sgml > $$i.html; \
		\
		echo $$i - building txt; \
		$(HTML_RENDER) $$i.html > $$i.txt; \
		\
		echo $$i - building multipage HTML; \
		mkdir -p $$i; \
		cd $$i; \
		$(JADE) -t sgml -d $(DSL) ../$$i.sgml; \
		cd ..; \
	done
