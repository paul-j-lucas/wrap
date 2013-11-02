/*
**      wrap -- text reformatter
**      pattern.h
**
**      Copyright (C) 1996-2013  Paul J. Lucas
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

#ifndef wrap_pattern_H
#define wrap_pattern_H

#include "alias.h"

/*****************************************************************************/

struct pattern_s {
  char const* pattern;
  alias_t const* alias;
};
typedef struct pattern_s pattern_t;

/**
 * Cleans-up all pattern data.
 */
void pattern_cleanup();

/**
 * Attempts to find a pattern from the internal list of patterns that matches
 * the given file-name and return the alias associated with that pattern.
 *
 * @param file_name The file-name to match.
 * @return Returns the alias associated with the pattern that matches
 * \a file-name or \c NULL if no matching pattern is found.
 */
alias_t const* pattern_find( char const *file_name );

/**
 * Parses a pattern from the given line and adds it to the internal list of
 * patterns.
 *
 * @param line The line from a configuration file to parse a pattern from.
 * @param conf_file The configuration file path-name.
 * @param line_no The line-number of \a line from \a conf_file.
 */
void pattern_parse( char const *line, char const *conf_file, int line_no );

/*****************************************************************************/

#endif /* wrap_pattern_H */
/* vim:set et sw=2 ts=2: */
