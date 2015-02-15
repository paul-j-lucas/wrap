# wrap

**wrap** is a filter for reformatting text
by wrapping and filling lines
to a given line-width.
It is like **fmt**(1)
but has many more options
including, but not limited to, those for
indenting (with either tabs, spaces, or both),
hang indenting (with either tabs, spaces, or both),
mirroring (equal left and right margins),
when to delimit paragraphs,
and
title lines.

It also includes **wrapc**
that is useful for reformatting comments
in source code.

## Installation

The git repository contains only the necessary source code.
Things like `configure` are _derived_ sources and
[should not be included in repositories](http://stackoverflow.com/a/18732931).
If you have `autoconf`, `automake`, and `m4` installed,
you can generate `configure` yourself by doing:

    autoreconf -fiv

Then follow the generic installation instructions given in `INSTALL`.
