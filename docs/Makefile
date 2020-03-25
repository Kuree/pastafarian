all: main.pdf

figure1.tex: figure1.dot
	dot2tex -ftikz --codeonly figure1.dot > figure1.tex

figure2.tex: figure2.dot
	dot2tex -ftikz --codeonly figure2.dot > figure2.tex

main.pdf: main.tex figure1.tex figure2.tex
	pdflatex main.tex
