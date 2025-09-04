/*
**      wrap -- text reformatter
**      src/markdown.h
**
**      Copyright (C) 2016-2025  Paul J. Lucas
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
 * Declares macros, types, data structures, and functions for reformatting
 * Markdown.
 */

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */

/// @endcond

/**
 * @defgroup markdown-group Markdown Support
 * Macros, types, data structures, and functions for reformatting Markdown.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

#define MD_SEQ_NUM_INIT           1u    /**< First Markdown state seq number. */
#define MD_TAB_SPACES             4u    /**< Number of spaces a tab equals. */

/**
 * Markdown line types.
 *
 * @note The values are arbitrary. They are mnemonic and useful when cast to
 * `char` and printed for debugging.
 */
enum md_line {
  MD_NONE         = '0',                ///< None.
  MD_CODE         = 'C',                ///< Code block.
  MD_DL           = ':',                ///< Definition list.
  MD_FOOTNOTE_DEF = '^',                ///< `[^id]`: footnote definition.
  MD_HEADER_ATX   = '#',                ///< `#` to `######`
  MD_HEADER_LINE  = '=',                ///< `=====` or `-----`
  MD_HR           = '_',                ///< `***`, `---`, or `___`
  MD_HTML_ABBR    = 'A',                ///< `*[abbr]`: abbreviation.
  MD_HTML_BLOCK   = '<',                ///< HTML block.
  MD_LINK_LABEL   = '[',                ///< `[id]:` _URI_
  MD_OL           = '1',                ///< Ordered list: `1.`, `2.`, ....
  MD_TABLE        = '|',                ///< Table.
  MD_TEXT         = 'T',                ///< Plain text.
  MD_UL           = '*',                ///< Unordered list: `*`, `+`, or `-`.
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
  md_line_t   line_type;                ///< Current line type.

  /**
   * Sequence number.
   *
   * @remarks This is an arbitrary number. It increases when either the list
   * type changes or, for ordered lists only, additionally when the list
   * character changes to be able to know when a new list starts.  For example,
   * in:
   *
   *      1. First list, first item.
   *      1. First list, second item.
   *      1) Second list, first item.
   *
   * the line type of all the lines is #MD_OL and the `seq_num` of the first
   * two lines is the same, but the `seq_num` of the third line is different.
   */
  md_seq_t    seq_num;

  /**
   * Nesting depth.
   *
   * @remarks This is for things like lists within lists or code blocks within
   * lists.
   */
  md_depth_t  depth;

  /**
   * Only when \ref line_type is #MD_FOOTNOTE_DEF, this indicates whether the
   * footnote has additional text on the _same_ line beyond the marker.  For
   * example, in:
   *
   *      [^1]: This is a footnote.
   *
   * this would be `true` whereas in:
   *
   *      [^lorum]:
   *          Lorem ipsum dolor sit amet, ligula suspendisse nulla pretium,
   *          rhoncus tempor fermentum, enim integer ad vestibulum volutpat.
   *
   *          Nisl rhoncus turpis est, vel elit, congue wisi enim nunc
   *          ultricies sit, magna tincidunt.
   *
   * this would be `false`.
   */
  bool        footnote_def_has_text;

  md_indent_t indent_left;              ///< Left-indent for lines.
  md_indent_t indent_hang;              ///< Hang-indent for lists.
  char        ol_c;                     ///< Ordered list character: `.` or `)`.
  md_ol_t     ol_num;                   ///< Ordered list number.
};
typedef struct md_state md_state_t;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes the Markdown parser.
 */
void markdown_init( void );

/**
 * Parses a line of Markdown text.  Note that this isn't a full Markdown parser
 * since it only really classifies entire lines of text.  Wrapping text only
 * needs to parse block elements (headers, lists, code blocks, horizontal
 * rules) and not span elements (links, emphasis, inline code).
 *
 * @param s The null-terminated string to parse.
 * @return Returns a pointer to the current Markdown state.
 */
NODISCARD
md_state_t const* markdown_parse( char *s );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* wrap_markdown_H */
/* vim:set et sw=2 ts=2: */
