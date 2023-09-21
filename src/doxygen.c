/*
**      wrap -- text reformatter
**      src/doxygen.c
**
**      Copyright (C) 2018-2023  Paul J. Lucas
**
**      This program is free software: you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation, either version 3 of the License, or
**      (at your option) any later version.
**
**      This program is distributed in the hope that it will be useful,
**      but WITHOUT ANY WARRANTY; without even the implied warranty of
**      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**      GNU General Public License for more details.
**
**      You should have received a copy of the GNU General Public License
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file
 * Defines macros, data structures, and functions for reformatting
 * [Doxygen](http://www.doxygen.org/).
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "doxygen.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>

/// @endcond

/**
 * @addtogroup doxygen-group
 * @{
 */

/**
 * Valid characters comprising a Doxygen command.
 */
#define DOX_CMD_CHARS             "abcdefghijklmnopqrstuvwxyz()[]{}"

#define DOX_INIT_INLINE            DOX_INLINE
#define DOX_INIT_BOL               DOX_BOL
#define DOX_INIT_EOL              (DOX_BOL | DOX_EOL)
#define DOX_INIT_PAR              (DOX_BOL | DOX_PAR)
#define DOX_INIT_PRE              (DOX_INIT_PAR | DOX_PRE)

///////////////////////////////////////////////////////////////////////////////

/**
 * Array of all [Doxygen
 * commands](https://www.doxygen.nl/manual/commands.html).
 */
static dox_cmd_t const DOX_COMMANDS[] = {

  { "a",                      DOX_INIT_INLINE,  NULL },
  { "addindex",               DOX_INIT_EOL,     NULL },
  { "addtogroup",             DOX_INIT_EOL,     NULL },
  { "anchor",                 DOX_INIT_INLINE,  NULL },
  { "arg",                    DOX_INIT_PAR,     NULL },
  { "attention",              DOX_INIT_PAR,     NULL },
  { "author",                 DOX_INIT_PAR,     NULL },
  { "authors",                DOX_INIT_PAR,     NULL },

  { "b",                      DOX_INIT_INLINE,  NULL },
  { "brief",                  DOX_INIT_PAR,     NULL },
  { "bug",                    DOX_INIT_PAR,     NULL },

  { "c",                      DOX_INIT_INLINE,  NULL },
  { "callergraph",            DOX_INIT_EOL,     NULL },
  { "callgraph",              DOX_INIT_EOL,     NULL },
  { "category",               DOX_INIT_EOL,     NULL },
  { "cite",                   DOX_INIT_EOL,     NULL },
  { "class",                  DOX_INIT_EOL,     NULL },
  { "code",                   DOX_INIT_PRE,     "endcode" },
  { "collaborationgraph",     DOX_INIT_EOL,     NULL },
  { "concept",                DOX_INIT_EOL,     NULL },
  { "cond",                   DOX_INIT_PAR,     "endcond" },
  { "copybrief",              DOX_INIT_BOL,     NULL },
  { "copydetails",            DOX_INIT_BOL,     NULL },
  { "copydoc",                DOX_INIT_BOL,     NULL },
  { "copyright",              DOX_INIT_PAR,     NULL },

  { "date",                   DOX_INIT_PAR,     NULL },
  { "def",                    DOX_INIT_EOL,     NULL },
  { "defgroup",               DOX_INIT_EOL,     NULL },
  { "deprecated",             DOX_INIT_PAR,     NULL },
  { "details",                DOX_INIT_PAR,     NULL },
  { "diafile",                DOX_INIT_EOL,     NULL },
  { "dir",                    DOX_INIT_EOL,     NULL },
  { "directorygraph",         DOX_INIT_EOL,     NULL },
  { "docbookinclude",         DOX_INIT_EOL,     NULL },
  { "docbookonly",            DOX_INIT_PRE,     "enddocbookonly" },
  { "dontinclude",            DOX_INIT_EOL,     NULL },
  { "dot",                    DOX_INIT_PRE,     "enddot" },
  { "dotfile",                DOX_INIT_EOL,     NULL },
  { "doxyconfig",             DOX_INIT_EOL,     NULL },

  { "e",                      DOX_INIT_INLINE,  NULL },
  { "else",                   DOX_INIT_EOL,     NULL },
  { "elseif",                 DOX_INIT_EOL,     NULL },
  { "em",                     DOX_INIT_INLINE,  NULL },
  { "emoji",                  DOX_INIT_INLINE,  NULL },
  { "endcode",                DOX_INIT_EOL,     NULL },
  { "endcond",                DOX_INIT_EOL,     NULL },
  { "enddocbookonly",         DOX_INIT_EOL,     NULL },
  { "enddot",                 DOX_INIT_EOL,     NULL },
  { "endhtmlonly",            DOX_INIT_EOL,     NULL },
  { "endif",                  DOX_INIT_EOL,     NULL },
  { "endinternal",            DOX_INIT_EOL,     NULL },
  { "endlatexonly",           DOX_INIT_EOL,     NULL },
  { "endlink",                DOX_INIT_EOL,     NULL },
  { "endmanonly",             DOX_INIT_EOL,     NULL },
  { "endmsc",                 DOX_INIT_EOL,     NULL },
  { "endparblock",            DOX_INIT_EOL,     NULL },
  { "endrtfonly",             DOX_INIT_EOL,     NULL },
  { "endsecreflist",          DOX_INIT_EOL,     NULL },
  { "endverbatim",            DOX_INIT_EOL,     NULL },
  { "enduml",                 DOX_INIT_EOL,     NULL },
  { "endxmlonly",             DOX_INIT_EOL,     NULL },
  { "enum",                   DOX_INIT_INLINE,  NULL },
  { "example",                DOX_INIT_EOL,     NULL },
  { "exception",              DOX_INIT_PAR,     NULL },
  { "extends",                DOX_INIT_INLINE,  NULL },

  { "f$",                     DOX_INIT_INLINE,  NULL },
  { "f(",                     DOX_INIT_PRE,     "f)" },
  { "f)",                     DOX_INIT_EOL,     NULL },
  { "f[",                     DOX_INIT_PRE,     "f]" },
  { "f]",                     DOX_INIT_EOL,     NULL },
  { "f{",                     DOX_INIT_PRE,     "f}" },
  { "f}",                     DOX_INIT_EOL,     NULL },
  { "file",                   DOX_INIT_EOL,     NULL },
  { "fileinfo",               DOX_INIT_INLINE,  NULL },
  { "fn",                     DOX_INIT_EOL,     NULL },

  { "groupgraph",             DOX_INIT_EOL,     NULL },

  { "headerfile",             DOX_INIT_EOL,     NULL },
  { "hidecallergraph",        DOX_INIT_EOL,     NULL },
  { "hidecallgraph",          DOX_INIT_EOL,     NULL },
  { "hidecollaborationgraph", DOX_INIT_EOL,     NULL },
  { "hidedirectorygraph",     DOX_INIT_EOL,     NULL },
  { "hidegroupgraph",         DOX_INIT_EOL,     NULL },
  { "hideincludedbygraph",    DOX_INIT_EOL,     NULL },
  { "hideincludegraph",       DOX_INIT_EOL,     NULL },
  { "hideinitializer",        DOX_INIT_EOL,     NULL },
  { "hiderefby",              DOX_INIT_EOL,     NULL },
  { "hiderefs",               DOX_INIT_EOL,     NULL },
  { "htmlinclude",            DOX_INIT_EOL,     NULL },
  { "htmlonly",               DOX_INIT_PRE,     "endhtmlonly" },

  { "idlexcept",              DOX_INIT_EOL,     NULL },
  { "if",                     DOX_INIT_EOL,     "endif" },
  { "ifnot",                  DOX_INIT_EOL,     "endif" },
  { "image",                  DOX_INIT_EOL,     NULL },
  { "implements",             DOX_INIT_EOL,     NULL },
  { "include",                DOX_INIT_EOL,     NULL },
  { "includedbygraph",        DOX_INIT_EOL,     NULL },
  { "includedoc",             DOX_INIT_EOL,     NULL },
  { "includegraph",           DOX_INIT_EOL,     NULL },
  { "includelineno",          DOX_INIT_EOL,     NULL },
  { "ingroup",                DOX_INIT_EOL,     NULL },
  { "interface",              DOX_INIT_EOL,     NULL },
  { "internal",               DOX_INIT_EOL,     "endinternal" },
  { "invariant",              DOX_INIT_PAR,     NULL },

  { "latexinclude",           DOX_INIT_EOL,     NULL },
  { "latexonly",              DOX_INIT_PRE,     "endlatexonly" },
  { "li",                     DOX_INIT_PAR,     NULL },
  { "line",                   DOX_INIT_EOL,     NULL },
  { "link",                   DOX_INIT_INLINE,  "endlink" },

  { "mainpage",               DOX_INIT_EOL,     NULL },
  { "maninclude",             DOX_INIT_EOL,     NULL },
  { "manonly",                DOX_INIT_PRE,     "endmanonly" },
  { "memberof",               DOX_INIT_EOL,     NULL },
  { "module",                 DOX_INIT_EOL,     NULL },
  { "msc",                    DOX_INIT_PRE,     "endmsc" },
  { "mscfile",                DOX_INIT_EOL,     NULL },

  { "n",                      DOX_INIT_EOL,     NULL },
  { "name",                   DOX_INIT_EOL,     NULL },
  { "noop",                   DOX_INIT_EOL,     NULL },
  { "namespace",              DOX_INIT_EOL,     NULL },
  { "nosubgrouping",          DOX_INIT_EOL,     NULL },
  { "note",                   DOX_INIT_PAR,     NULL },

  { "overload",               DOX_INIT_EOL,     NULL },

  { "p",                      DOX_INIT_INLINE,  NULL },
  { "package",                DOX_INIT_EOL,     NULL },
  { "page",                   DOX_INIT_EOL,     NULL },
  { "par",                    DOX_INIT_EOL,     NULL },
  { "paragraph",              DOX_INIT_EOL,     NULL },
  { "param",                  DOX_INIT_PAR,     NULL },
  { "parblock",               DOX_INIT_EOL,     NULL },
  { "post",                   DOX_INIT_PAR,     NULL },
  { "pre",                    DOX_INIT_PAR,     NULL },
  { "private",                DOX_INIT_EOL,     NULL },
  { "privatesection",         DOX_INIT_EOL,     NULL },
  { "property",               DOX_INIT_EOL,     NULL },
  { "protected",              DOX_INIT_EOL,     NULL },
  { "protectedsection",       DOX_INIT_EOL,     NULL },
  { "protocol",               DOX_INIT_EOL,     NULL },
  { "public",                 DOX_INIT_EOL,     NULL },
  { "publicsection",          DOX_INIT_EOL,     NULL },
  { "pure",                   DOX_INIT_BOL,     NULL },

  { "qualifier",              DOX_INIT_EOL,     NULL },

  { "raisewarning",           DOX_INIT_EOL,     NULL },
  { "ref",                    DOX_INIT_INLINE,  NULL },
  { "refitem",                DOX_INIT_INLINE,  NULL },
  { "related",                DOX_INIT_EOL,     NULL },
  { "relates",                DOX_INIT_EOL,     NULL },
  { "relatedalso",            DOX_INIT_EOL,     NULL },
  { "relatesalso",            DOX_INIT_EOL,     NULL },
  { "remark",                 DOX_INIT_PAR,     NULL },
  { "remarks",                DOX_INIT_PAR,     NULL },
  { "result",                 DOX_INIT_PAR,     NULL },
  { "return",                 DOX_INIT_PAR,     NULL },
  { "returns",                DOX_INIT_PAR,     NULL },
  { "retval",                 DOX_INIT_PAR,     NULL },
  { "rtfonly",                DOX_INIT_PAR,     "endrtfonly" },

  { "sa",                     DOX_INIT_EOL,     NULL },
  { "secreflist",             DOX_INIT_EOL,     "endsecreflist" },
  { "section",                DOX_INIT_EOL,     NULL },
  { "see",                    DOX_INIT_PAR,     NULL },
  { "short",                  DOX_INIT_PAR,     NULL },
  { "showdate",               DOX_INIT_INLINE,  NULL },
  { "showinitializer",        DOX_INIT_EOL,     NULL },
  { "showrefby",              DOX_INIT_EOL,     NULL },
  { "showrefs",               DOX_INIT_EOL,     NULL },
  { "since",                  DOX_INIT_PAR,     NULL },
  { "skip",                   DOX_INIT_EOL,     NULL },
  { "skipline",               DOX_INIT_EOL,     NULL },
  { "snippet",                DOX_INIT_EOL,     NULL },
  { "snippetdoc",             DOX_INIT_EOL,     NULL },
  { "snippetlineno",          DOX_INIT_EOL,     NULL },
  { "startuml",               DOX_INIT_PRE,     "enduml" },
  { "static",                 DOX_INIT_EOL,     NULL },
  { "struct",                 DOX_INIT_INLINE,  NULL },
  { "subpage",                DOX_INIT_EOL,     NULL },
  { "subsection",             DOX_INIT_EOL,     NULL },
  { "subsubsection",          DOX_INIT_EOL,     NULL },

  { "tableofcontents",        DOX_INIT_EOL,     NULL },
  { "test",                   DOX_INIT_PAR,     NULL },
  { "throw",                  DOX_INIT_PAR,     NULL },
  { "throws",                 DOX_INIT_PAR,     NULL },
  { "todo",                   DOX_INIT_PAR,     NULL },
  { "tparam",                 DOX_INIT_PAR,     NULL },
  { "typedef",                DOX_INIT_EOL,     NULL },

  { "union",                  DOX_INIT_EOL,     NULL },
  { "until",                  DOX_INIT_EOL,     NULL },

  { "var",                    DOX_INIT_EOL,     NULL },
  { "verbatim",               DOX_INIT_PRE,     "endverbatim" },
  { "verbinclude",            DOX_INIT_EOL,     NULL },
  { "version",                DOX_INIT_PAR,     NULL },
  { "vhdlflow",               DOX_INIT_EOL,     NULL },

  { "warning",                DOX_INIT_PAR,     NULL },
  { "weakgroup",              DOX_INIT_EOL,     NULL },

  { "xmlinclude",             DOX_INIT_EOL,     NULL },
  { "xmlonly",                DOX_INIT_PRE,     "endxml" },
  { "xrefitem",               DOX_INIT_PAR,     NULL },

  { "{",                      DOX_INIT_EOL,     NULL },
  { "}",                      DOX_INIT_EOL,     NULL },
};

////////// local functions ////////////////////////////////////////////////////

/**
 * Comparison function for bsearch(3) that compares a string key against an
 * element of array of dox_cmd_t.
 *
 * @param key A pointer to the string being searched for.
 * @param elt A pointer to the current element to compare against.
 * @return Returns an integer less than zero, zero, or greater thatn zero if
 * the key is less than, equal to, or greater than the element's name,
 * respectively.
 */
NODISCARD
static int bsearch_str_dox_cmp( void const *key, void const *elt ) {
  char const *const key_s = key;
  dox_cmd_t const *const elt_dox = elt;
  return strcmp( key_s, elt_dox->name );
}

////////// extern functions ///////////////////////////////////////////////////

dox_cmd_t const* dox_find_cmd( char const *s ) {
  return bsearch(
    s, DOX_COMMANDS, ARRAY_SIZE( DOX_COMMANDS ),
    sizeof( dox_cmd_t ), &bsearch_str_dox_cmp
  );
}

bool dox_parse_cmd_name( char const *s, char *dox_cmd_name ) {
  assert( s != NULL );
  assert( dox_cmd_name != NULL );

  SKIP_CHARS( s, WS_ST );
  if ( !(s[0] == '@' || s[0] == '\\') )
    return false;

  size_t const len = strspn( s + 1, DOX_CMD_CHARS );
  if ( len > 0 && len <= DOX_CMD_NAME_SIZE_MAX ) {
    strncpy( dox_cmd_name, s + 1, len );
    dox_cmd_name[ len ] = '\0';
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
