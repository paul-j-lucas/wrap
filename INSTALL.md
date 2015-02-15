Installation
============

The git repository contains only the necessary source code.
Things like `configure` are _derived_ sources and
[should not be included in repositories](http://stackoverflow.com/a/18732931).
If you have `autoconf`, `automake`, and `m4` installed,
you can generate `configure` yourself by doing:

    autoreconf -fiv

Then follow the normal instructions given in `INSTALL.fsf`.
