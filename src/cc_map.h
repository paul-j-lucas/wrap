/*
**      wrapc -- comment reformatter
**      src/cc_map.h
**
**      Copyright (C) 2017-2023  Paul J. Lucas
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

#ifndef wrap_cc_map_H
#define wrap_cc_map_H

/**
 * @file
 * Contains a type for a comment delimiter character map and functions to
 * manipulate it.
 */

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#include <string.h>

/// @endcond

_GL_INLINE_HEADER_BEGIN
#ifndef W_CC_MAP_H_INLINE
# define W_CC_MAP_H_INLINE _GL_INLINE
#endif /* W_CC_MAP_H_INLINE */

/**
 * @defgroup cc-map-group Comment Delimiter Character Map
 * Declares a \ref cc_map_t "comment delimiter character map" and related
 * functions.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Presence of this within a comment delimiter character map entry means the
 * delimeter is a single comment delimiter character.
 */
#define CC_SINGLE_CHAR            ' '

/**
 * Comment delimiter character map indexed by character.  If an entry is:
 *
 *  + NULL, that character is not a comment delimiter character.
 *
 *  + A non-empty string, that character followed by each character (except
 *    space) in said string forms a two-character comment delimiter, e.g., `//`
 *    or `(*`.  A space means it forms a single comment character delimiter,
 *    e.g., `#`.
 */
typedef char* cc_map_t[128];

////////// extern functions ///////////////////////////////////////////////////

/**
 * Gets whether the comment delimiter character referred to by the given map
 * entry is a single comment delimiter character.
 *
 * @param cc_map_entry A comment delimiter character map entry returned by
 * cc_map_get().
 * @return Returns `true` only if \a cc_map_entry is a single comment delimiter
 * character.
 */
NODISCARD W_CC_MAP_H_INLINE
bool cc_is_single( char const *cc_map_entry ) {
  return strchr( cc_map_entry, CC_SINGLE_CHAR ) != NULL;
}

/**
 * Compiles a set of comment delimiter characters into a string of distinct
 * comment delimiter characters and a \ref cc_map_t used by
 * align_eol_comments().
 *
 * @param in_cc The comment delimiter characters to compile.  It is one or more
 * one- or two-character comment delimiters separated by either commas or
 * whitesspace.
 * @return Returns said string of distinct comment delimiter characters.
 */
NODISCARD
char const* cc_map_compile( char const *in_cc );

/**
 * Gets the comment delimiter character map entry for the given character.
 *
 * @param c The character to get the comment delimiter character map entry for.
 * @return Returns said entry or null if \a c is not a comment delimiter
 * character.
 */
NODISCARD W_CC_MAP_H_INLINE
char const* cc_map_get( char c ) {
  extern cc_map_t cc_map;
  return cc_map[ (unsigned char)c ];
}

///////////////////////////////////////////////////////////////////////////////

_GL_INLINE_HEADER_END

/** @} */

#endif /* wrap_cc_map_H */
/* vim:set et sw=2 ts=2: */
