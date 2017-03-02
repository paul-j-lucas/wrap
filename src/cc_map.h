/*
**      wrapc -- comment reformatter
**      cc_map.h
**
**      Copyright (C) 2017  Paul J. Lucas
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

///////////////////////////////////////////////////////////////////////////////

#define CC_SINGLE_CHAR            ' '

/**
 * Comment delimiter character map indexed by character.  If an entry is:
 *
 *  + NULL, that character is not a comment delimiter character.
 *
 *  + A non-empty string, that character followed by each character (except
 *    space) in said string forms a two-character comment delimiter, e.g., "//"
 *    or "(*".  A space means it forms a single comment character delimiter,
 *    e.g., "#".
 */
typedef char* cc_map_t[128];

extern cc_map_t	cc_map;                 // comment delimiter character map

////////// extern functions ///////////////////////////////////////////////////

/**
 * Compiles a set of comment delimiter characters into a string of distinct
 * comment delimiter characters and a cc_map_t used by align_eol_comments().
 *
 * @param in_cc The comment delimiter characters to compile.
 * @return Returns said string of distinct comment delimiter characters.
 */
char const* cc_map_compile( char const *in_cc );

/**
 * Frees the memory used by the comment delimiter character map.
 */
void cc_map_free( void );

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_cc_map_H */
/* vim:set et sw=2 ts=2: */
