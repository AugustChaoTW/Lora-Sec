TEX=main

all:
	pdflatex -interaction=nonstopmode -halt-on-error $(TEX).tex
	pdflatex -interaction=nonstopmode -halt-on-error $(TEX).tex

clean:
	rm -f $(TEX).aux $(TEX).log $(TEX).out $(TEX).toc $(TEX).lof $(TEX).lot $(TEX).bbl $(TEX).blg $(TEX).pdf
