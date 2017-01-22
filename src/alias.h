/*
**      wrap -- text reformatter
**      alias.h
**
**      Copyright (C) 2013-2017  Paul J. Lucas
**
**      This program is free software; you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the Licence, or
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

// standard
#include <stddef.h>                     /* for size_t */

///////////////////////////////////////////////////////////////////////////////

/**
 * Contains a configuration file alias and its associated command-line options.
 */
struct alias {
  unsigned      argc;                   // number of arguments + 1
  char const  **argv;                   // argv[0] = alias name
  unsigned      line_no;                // line in conf file defined on
};
typedef struct alias alias_t;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Cleans-up all alias data.
 */
void alias_cleanup( void );

/**
 * Attempts to find an alias from the internal list of aliases having the given
 * name and return that alias.
 *
 * @param name The name to match.
 * @return Returns the alias having \a name or \c NULL if no matching alias is
 * found.
 */
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

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_alias_H */
/* vim:set et sw=2 ts=2: */
