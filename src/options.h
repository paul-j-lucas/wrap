/*
**      wrap -- text reformatter
**      options.h
**
**      Copyright (C) 1996-2015  Paul J. Lucas
**
**      This program is free software; you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the License, or
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

#ifndef wrap_options_H
#define wrap_options_H

// standard
#include <ctype.h>
#include <string.h>                     /* for memset(3) */

#define CLEAR_OPTIONS()   memset( opts_given, 0, sizeof opts_given )
#define GAVE_OPTION(OPT)  isalpha( OPTION_VALUE(OPT) )
#define OPTION_VALUE(OPT) opts_given[ !islower(OPT) ][ toupper(OPT) - 'A' ]
#define SET_OPTION(OPT)   OPTION_VALUE(OPT) = (OPT)

/**
 * End-of-Line formats.
 */
enum eol {
  EOL_INPUT   = 'i',                    // do newlines the way the input does
  EOL_UNIX    = 'u',
  EOL_WINDOWS = 'w'
};
typedef enum eol eol_t;

////////// extern variables ///////////////////////////////////////////////////

typedef char opts_given_t[ 2 /* lower/upper */ ][ 26 + 1 /*NULL*/ ];

extern opts_given_t opts_given;         // options given

////////// extern functions ///////////////////////////////////////////////////

/**
 * Checks that no options were given that are among the two given mutually
 * exclusive sets of short options.
 * Prints an error message and exits if any such options are found.
 *
 * @param opts1 The first set of short options.
 * @param opts2 The second set of short options.
 */
void check_mutually_exclusive( char const *opts1, char const *opts2 );

/**
 * Parses an End-of-Line value.
 *
 * @param s The NULL-terminated string to parse.
 * @return Returns the corresponding \c eol_t
 * or prints an error message and exits if \a s is invalid.
 */
eol_t parse_eol( char const *s );

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_options_H */
/* vim:set et sw=2 ts=2: */
