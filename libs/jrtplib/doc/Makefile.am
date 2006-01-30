EXTRA_DIST = jrtplib.tex manual.tex

doc: jrtplib.pdf
	
jrtplib.pdf: jrtplib.tex
	pdflatex jrtplib.tex
	pdflatex jrtplib.tex

clean:
	rm -f jrtplib.{toc,aux,log,pdf}
