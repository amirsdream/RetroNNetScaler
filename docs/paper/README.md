# Technical paper (IEEEtran)

IEEE conference-format source:

| File | Role |
|------|------|
| [`retronnetscaler.tex`](retronnetscaler.tex) | IEEEtran conference paper |
| [`references.bib`](references.bib) | BibTeX bibliography |
| [`retronnetscaler.pdf`](retronnetscaler.pdf) | Compiled PDF |

## Build

Requires MiKTeX / TeX Live with `IEEEtran` and `IEEEtran.bst`.

```bat
cd docs\paper
pdflatex retronnetscaler.tex
bibtex retronnetscaler
pdflatex retronnetscaler.tex
pdflatex retronnetscaler.tex
```

Or:

```bat
latexmk -pdf retronnetscaler.tex
```
