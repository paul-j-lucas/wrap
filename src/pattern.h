/*
**      wrap -- text reformatter
**      src/pattern.h
**
**      Copyright (C) 2013-2026  Paul J. Lucas
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

#ifndef wrap_pattern_H
#define wrap_pattern_H

/**
 * @file
 * Declares a data structure for a **wrap**(1) configuration file filename
 * pattern and functions to manipulate it.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "alias.h"

/**
 * @ingroup config-file-group
 * @defgroup patterns-group Filename Patterns
 * A data structure and functions for **wrap**(1) filename patterns within
 * configuration files.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Contains a **wrap**(1) configuration file filename pattern and the \ref
 * alias to which it maps.
 */
struct pattern {
  char const     *pattern;              ///< File-name pattern.
  alias_t const  *alias;                ///< Alias to which the pattern maps.
};
typedef struct pattern pattern_t;

////////// extern functions ///////////////////////////////////////////////////

#ifndef NDEBUG
/**
 * Dumps the in-memory data structures for patterns read from a configuration
 * file back out.  (This is used to test the pattern parsing code.)
 */
void dump_patterns( void );
#endif /* NDEBUG */

/**
 * Attempts to find a pattern from the internal list of patterns that matches
 * the given file-name and return the \ref alias associated with that pattern.
 *
 * @param file_name The file-name to match.
 * @return Returns the \ref alias associated with the pattern that matches
 * \a file_name or NULL if no matching pattern is found.
 */
NODISCARD
alias_t const* pattern_find( char const *file_name );

/**
 * Parses a pattern from the given line and adds it to the internal list of
 * patterns.
 *
 * @param line The line from a configuration file to parse a pattern from.
 * @param conf_file The configuration file path-name.
 * @param line_no The line-number of \a line from \a conf_file.
 */
void pattern_parse( char const *line, char const *conf_file, unsigned line_no );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* wrap_pattern_H */
/* vim:set et sw=2 ts=2: */
