/*
**      wrap -- text reformatter
**      src/alias.h
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

#ifndef wrap_alias_H
#define wrap_alias_H

/**
 * @file
 * Declares a data structure for a **wrap**(1) alias in configuration files and
 * functions to manipulate it.
 */

// local
#include "pjl_config.h"                 /* must go first */

/**
 * @ingroup config-file-group
 * @defgroup alias-group Aliases
 * A data structure and functions for **wrap**(1) aliases within configuration
 * files.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Contains a **wrap**(1) configuration file alias and its associated command-
 * line options.
 */
struct alias {
  int           argc;                   ///< Number of arguments + 1.
  char const  **argv;                   ///< `argv[0]` = alias name.
  unsigned      line_no;                ///< Line in conf. file defined on.
};
typedef struct alias alias_t;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Attempts to find an alias from the internal list of aliases having the given
 * name and return that alias.
 *
 * @param name The name to match.
 * @return Returns the alias having \a name or NULL if no matching alias is
 * found.
 */
NODISCARD
alias_t const* alias_find( char const *name );

/**
 * Parses an alias from the given line and adds it to the internal list of
 * aliases.
 *
 * @param line The line from a configuration file to parse an alias from.
 * @param conf_file The configuration file path-name.
 * @param line_no The line-number of \a line from \a conf_file.
 */
void alias_parse( char const *line, char const *conf_file, unsigned line_no );

#ifndef NDEBUG
/**
 * Dumps the in-memory data structures for aliases read from a configuration
 * file back out.  (This is used to test the alias parsing code.)
 */
void dump_aliases( void );
#endif /* NDEBUG */

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* wrap_alias_H */
/* vim:set et sw=2 ts=2: */
