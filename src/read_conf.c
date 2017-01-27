/*
**      wrap -- text reformatter
**      read_conf.c
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

// local
#include "alias.h"
#include "common.h"
#include "config.h"
#include "pattern.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX */
#if HAVE_PWD_H
# include <pwd.h>                       /* for getpwuid() */
#endif /* HAVE_PWD_H */
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <stdlib.h>                     /* for getenv(), ... */
#include <string.h>
#include <unistd.h>                     /* for geteuid() */

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the full path of the user's home directory.
 *
 * @return Returns said directory or NULL if it is not obtainable.
 */
static char const* home_dir( void ) {
  char const *home = getenv( "HOME" );
#if HAVE_GETEUID && HAVE_GETPWUID && HAVE_STRUCT_PASSWD_PW_DIR
  if ( !home ) {
    struct passwd *const pw = getpwuid( geteuid() );
    if ( pw )
      home = pw->pw_dir;
  }
#endif /* HAVE_GETEUID && && HAVE_GETPWUID && HAVE_STRUCT_PASSWD_PW_DIR */
  return home;
}

/**
 * Appends a component to a path ensuring that exactly one \c / separates them.
 *
 * @param path The path to append to.
 * The buffer pointed to must be big enough to hold the new path.
 * @param component The component to append.
 */
static void path_append( char *path, char const *component ) {
  assert( path );
  assert( component );

  size_t const len = strlen( path );
  if ( len ) {
    path += len - 1;
    if ( *path != '/' )
      strcpy( ++path, "/" );
    strcpy( ++path, component );
  }
}

/**
 * Strips a \c # comment from a line minding quotes and backslashes.
 *
 * @param s The null-terminated line to strip the comment from.
 * @return Returns \a s on success or NULL for an unclosed quote.
 */
static char* strip_comment( char *s ) {
  assert( s );
  char *const s0 = s;
  char quote = '\0';

  for ( ; *s; ++s ) {
    switch ( *s ) {
      case '#':
        if ( quote )
          break;
        *s = '\0';
        return s0;
      case '"':
      case '\'':
        if ( !quote )
          quote = *s;
        else if ( *s == quote )
          quote = '\0';
        break;
      case '\\':
        ++s;
        break;
    } // switch
  } // for

  return quote ? NULL : s0;
}

/**
 * Trims both leading and trailing whitespace from a string.
 *
 * @param s The string to trim whitespace from.
 * @return Returns a pointer to within \a s having all whitespace trimmed.
 */
static char* trim_ws( char *s ) {
  assert( s );
  SKIP_CHARS( s, WS_STR );
  for ( size_t len = strlen( s ); len > 0 && isspace( s[ --len ] ); )
    s[ len ] = '\0';
  return s;
}

////////// extern functions ///////////////////////////////////////////////////

char const* read_conf( char const *conf_file ) {
  static char conf_path_buf[ PATH_MAX ];
  bool const explicit_conf_file = (conf_file != NULL);

  enum section { NONE, ALIASES, PATTERNS };
  enum section section = NONE;          // section we're in

  // locate default configuration file
  if ( !conf_file ) {
    char const *const home = home_dir();
    if ( !home )
      return NULL;
    strcpy( conf_path_buf, home );
    path_append( conf_path_buf, CONF_FILE_NAME );
    conf_file = conf_path_buf;
  }

  // open configuration file
  FILE *const fconf = fopen( conf_file, "r" );
  if ( !fconf ) {
    if ( explicit_conf_file )
      PMESSAGE_EXIT( EX_NOINPUT, "%s: %s\n", conf_file, STRERROR );
    return NULL;
  }

  // parse configuration file
  line_buf_t line_buf;
  unsigned line_no = 0;
  while ( fgets( line_buf, sizeof line_buf, fconf ) ) {
    ++line_no;
    char *line = strip_comment( line_buf );
    if ( !line )
      PMESSAGE_EXIT( EX_CONFIG,
        "%s:%u: \"%s\": unclosed quote\n",
        conf_file, line_no, trim_ws( line_buf )
      );
    line = trim_ws( line );
    if ( !*line )
      continue;

    // parse section line
    if ( line[0] == '[' ) {
      if ( strcmp( line, "[ALIASES]" ) == 0 )
        section = ALIASES;
      else if ( strcmp( line, "[PATTERNS]" ) == 0 )
        section = PATTERNS;
      else
        PMESSAGE_EXIT( EX_CONFIG,
          "%s:%u: \"%s\": invalid section\n",
          conf_file, line_no, line
        );
      continue;
    }

    // parse line within section
    switch ( section ) {
      case NONE:
        PMESSAGE_EXIT( EX_CONFIG,
          "%s:%u: \"%s\": line not within any section\n",
          conf_file, line_no, line
        );
      case ALIASES:
        alias_parse( line, conf_file, line_no );
        break;
      case PATTERNS:
        pattern_parse( line, conf_file, line_no );
        break;
    } // switch
  } // while

  if ( ferror( fconf ) )
    PMESSAGE_EXIT( EX_IOERR, "%s: %s\n", conf_file, STRERROR );
  fclose( fconf );
  return conf_file;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
