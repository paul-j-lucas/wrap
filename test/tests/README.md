Test File Format
================

Wrap and Wrapc (`.test`) Files
------------------------------

Wrap and Wrapc Test files must be a single line in the following format:

*command* `|` *config* `|` *options* `|` *input* `|` *exit*

that is five fields separated by the pipe (`|`) character
(with optional whitespace)
where:

+ *command* = command to execute (`wrap` or `wrapc`)
+ *config*  = name of config file to use or `/dev/null` for none
+ *options* = command-line options or blank for none
+ *input*   = name of file to wrap
+ *exit*    = expected exit status code

Regular Expression (`.regex`) Files
-----------------------------------

Regular expression files
(that stand-alone test both the regular expressions used and the wrapper code)
are one or more lines in the following format:

[*expected*] *text*

that is two fields separated by the first space character
where:

+ *expected* = optional expected matching UTF-8 pattern
+ *text*     = UTF-8 text containing pattern

The *expected* field is optional.
If missing, it means that *text* is *not* expected to match a pattern.
Note that the space seperator character must remain
(hence non-matching *text* must start with a leading space).

Blank lines
and lines beginning with `#` (a comment)
are ignored.

Note on Test Names
------------------

Care must be taken when naming files that differ only in case
because of case-insensitive (but preserving) filesystems like HFS+
used on Mac OS X.

For example, tests such as these:

    ad-B2-e1-m.test
    ad-B2-E1-m.test

that are the same test but differ only in 'e' vs. 'E' will work fine
on every other Unix filesystem but cause a collision on HFS+.

One solution (the one used here) is to append a distinct number:

    ad-B2-e1-m_01.test
    ad-B2-E1-m_02.test

thus making the filenames unique regardless of case.
