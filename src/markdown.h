/*
**      wrap -- text reformatter
**      src/markdown.h
**
**      Copyright (C) 2016-2019  Paul J. Lucas
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

#ifndef wrap_markdown_H
#define wrap_markdown_H

/**
 * @file
 * Contains constants, enums, typedefs, data structures, and functions for
 * parsing Markdown.
 */

// local
#include "pjl_config.h"                 /* must go first */

// standard
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */

///////////////////////////////////////////////////////////////////////////////

#define MD_SEQ_NUM_INIT           1     /**< First Markdown state seq number. */
#define TAB_SPACES_MARKDOWN       4     /**< Number of spaces a tab equals. */

/**
 * Markdown line types.
 * (The values are arbitrary: they are mnemonic and useful when cast to \c char
 * and printed for debugging).
 */
enum md_line {
  MD_NONE         = '0',
  MD_CODE         = 'C',
  MD_DL           = ':',                ///< Definition list.
  MD_FOOTNOTE_DEF = '^',                ///< [^id]: footnote definition.
  MD_HEADER_ATX   = '#',                ///< # to ######
  MD_HEADER_LINE  = '=',                ///< ===== or -----
  MD_HR           = '_',                ///< ***, ---, or ___
  MD_HTML_ABBR    = 'A',                ///< *[abbr]: abbreviation.
  MD_HTML_BLOCK   = '<',                ///< HTML block.
  MD_LINK_LABEL   = '[',                ///< [id]: <URI>
  MD_OL           = '1',                ///< Ordered list: 1., 2., ....
  MD_TABLE        = '|',                ///< Markdown table.
  MD_TEXT         = 'T',                ///< Plain text.
  MD_UL           = '*',                ///< Unordered list: *, +, or -.
};
typedef enum md_line md_line_t;

typedef size_t   md_depth_t;            ///< How nested we are.
typedef unsigned md_seq_t;              ///< Parser state sequence number.
typedef size_t   md_indent_t;           ///< Indentation amount (in spaces).
typedef unsigned md_ol_t;               ///< Ordered list number.

/**
 * Markdown parser state.
 */
struct md_state {
  md_line_t   line_type;
  md_seq_t    seq_num;
  md_depth_t  depth;
  bool        fn_def_text;              // true if footnote def has text on line
  md_indent_t indent_left;
  md_indent_t indent_hang;              // hang-indent for lists
  char        ol_c;                     // ordered list character: . or )
  md_ol_t     ol_num;                   // ordered list number
};
typedef struct md_state md_state_t;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes the Markdown parser.
 */
void markdown_init( void );

/**
 * Cleans-up all Markdown data.
 */
void markdown_cleanup( void );

/**
 * Parses a line of Markdown text.  Note that this isn't a full Markdown parser
 * since it only really classifies entire lines of text.  Wrapping text only
 * needs to parse block elements (headers, lists, code blocks, horizontal
 * rules) and not span elements (links, emphasis, inline code).
 *
 * @param s The null-terminated string to parse.
 * @return Returns a pointer to the current Markdown state.
 */
PJL_WARN_UNUSED_RESULT
md_state_t const* markdown_parse( char *s );

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_markdown_H */
/* vim:set et sw=2 ts=2: */
