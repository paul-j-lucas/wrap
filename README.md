# wrap

## Introduction

**wrap** is a filter for reformatting text
by wrapping and filling lines
to a given line-width.
It is like **fmt**(1)
or **fold**(1)
but has many more options
including, but not limited to, those for
indenting (with either tabs, spaces, or both),
hang indenting (with either tabs, spaces, or both),
mirroring (equal left and right margins),
when to delimit paragraphs,
and
title lines.
It also fully supports the UTF-8 encoding of Unicode
and additionally can reformat Markdown text.

It also includes **wrapc**
that is useful for reformatting comments
while editing source code in languages
including
(but not limited to if others do comments similarly):
Ada,
AppleScript,
Assembly,
AWK,
C,
C#,
C++,
Chapel,
Clojure,
CMake,
COBOL,
Crystal,
D,
Dart,
Delphi,
Eiffel,
Erlang,
F#,
Forth,
Fortran,
Go,
Haskell,
Java,
Julia,
Kotlin,
Lisp,
Lua,
Make,
Mathematica,
Matlab,
Maxima,
Metafont,
ML,
Modula-[23],
Nim,
Oberon,
Objective C,
Octave,
OCaml,
Pascal,
Perl,
Pike,
PL/I,
PostScript,
PowerShell,
Prolog,
Pure,
Python,
R,
Racket,
Ruby,
Rust,
Scala,
Scheme,
Shell,
Simula,
SQL,
Swift,
Tcl,
TeX,
VHDL,
and
XQuery.

## Installation

The git repository contains only the necessary source code.
Things like `configure` are _derived_ sources and
[should not be included in repositories](http://stackoverflow.com/a/18732931).
If you have
[`autoconf`](https://www.gnu.org/software/autoconf/),
[`automake`](https://www.gnu.org/software/automake/),
and
[`m4`](https://www.gnu.org/software/m4/)
installed,
you can generate `configure` yourself by doing:

    ./bootstrap

Then follow the generic installation instructions given in
[`INSTALL`](https://github.com/paul-j-lucas/wrap/blob/master/INSTALL).

If you would like to generate the developer documentation,
you will also need
[Doxygen](http://www.doxygen.org/);
then do:

    make doc                            # or: make docs

**Paul J. Lucas**  
San Francisco Bay Area, California, USA  
20 September 2023
