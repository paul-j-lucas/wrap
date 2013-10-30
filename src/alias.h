/*
**      wrap -- text reformatter
**      alias.h
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
**      along with this program; if not, write to the Free Software
**      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef wrap_alias_H
#define wrap_alias_H

/*****************************************************************************/

struct alias_s {
  int line_no;                          /* line in conf file defined on */
  int argc;                             /* number of arguments + 1 */
  char const** argv;                    /* argv[0] = alias name */
};
typedef struct alias_s alias_t;

/**
 * Cleans-up all alias data.
 */
void alias_cleanup();

/**
 * Attempts to find an alias from the internal list of aliases having the given
 * name and return that alias.
 *
 * @param name The name to match.
 * @return Returns the alias having the given name \c NULL if no matching alias
 * is found.
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
void alias_parse( char const *line, char const *conf_file, int line_no );

/*****************************************************************************/

#endif /* wrap_alias_H */
/* vim:set et sw=2 ts=2: */
