/*
**      wrap -- text reformatter
**      src/read_conf.c
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

/**
 * @file
 * Defines the function to read a **wrap**(1) configuration file.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "alias.h"
#include "common.h"
#include "pattern.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX */
#if HAVE_PWD_H
# include <pwd.h>                       /* for getpwuid() */
#endif /* HAVE_PWD_H */
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <stdlib.h>                     /* for getenv(), ... */
#include <string.h>
#include <unistd.h>                     /* for geteuid() */

/// @endcond

/**
 * @defgroup config-file-group Configuration File
 * Function to read **wrap**(1) a configuration file.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/** 
 * Configuration file section.
 */
enum section {
  SECTION_NONE,                         ///< No section.
  SECTION_ALIASES,                      ///< `[ALIASES]` section.
  SECTION_PATTERNS                      ///< `[PATTERNS]` section.
};
typedef enum section section_t;

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the full path of the user's home directory.
 *
 * @return Returns said directory or NULL if it is not obtainable.
 */
NODISCARD
static char const* home_dir( void ) {
  static char const *home;

  RUN_ONCE {
    home = null_if_empty( getenv( "HOME" ) );
#if HAVE_GETEUID && HAVE_GETPWUID && HAVE_STRUCT_PASSWD_PW_DIR
    if ( home == NULL ) {
      struct passwd const *const pw = getpwuid( geteuid() );
      if ( pw != NULL )
        home = null_if_empty( pw->pw_dir );
    }
#endif /* HAVE_GETEUID && && HAVE_GETPWUID && HAVE_STRUCT_PASSWD_PW_DIR */
  }

  return home;
}

/**
 * Appends a component to a path ensuring that exactly one `/ `separates them.
 *
 * @param path The path to append to.
 * The buffer pointed to must be big enough to hold the new path.
 * @param component The component to append.
 */
static void path_append( char *path, char const *component ) {
  assert( path != NULL );
  assert( component != NULL );

  size_t const len = strlen( path );
  if ( len > 0 ) {
    path += len - 1;
    if ( *path != '/' )
      *++path = '/';
    strcpy( ++path, component );
  }
}

/**
 * Parses a section name.
 *
 * @param s The line containing the section name to parse.
 * @return Returns the corresponding section name or #SECTION_NONE.
 */
NODISCARD
static section_t section_parse( char const *s ) {
  assert( s != NULL );

  if ( strcmp( s, "[ALIASES]" ) == 0 )
    return SECTION_ALIASES;
  if ( strcmp( s, "[PATTERNS]" ) == 0 )
    return SECTION_PATTERNS;
  return SECTION_NONE;
}

/**
 * Strips a `#` comment from a line minding quotes and backslashes.
 *
 * @param s The null-terminated line to strip the comment from.
 * @return Returns \a s on success or NULL for an unclosed quote.
 */
NODISCARD
static char* strip_comment( char *s ) {
  assert( s != NULL );
  char *const s0 = s;
  char quote = '\0';

  for ( ; *s != '\0'; ++s ) {
    switch ( *s ) {
      case '#':
        if ( quote != '\0' )
          break;
        *s = '\0';
        return s0;
      case '"':
      case '\'':
        if ( quote == '\0' )
          quote = *s;
        else if ( *s == quote )
          quote = '\0';
        break;
      case '\\':
        ++s;
        break;
    } // switch
  } // for

  return quote != '\0' ? NULL : s0;
}

/**
 * Trims both leading and trailing whitespace from a string.
 *
 * @param s The string to trim whitespace from.
 * @return Returns a pointer to within \a s having all whitespace trimmed.
 */
NODISCARD
static char* trim_ws( char *s ) {
  assert( s != NULL );
  SKIP_CHARS( s, WS_STR );
  for ( size_t len = strlen( s ); len > 0 && isspace( s[ --len ] ); )
    s[ len ] = '\0';
  return s;
}

////////// extern functions ///////////////////////////////////////////////////

/**
 * Reads a **wrap**(1) configuration file.
 *
 * @param conf_file The full path of the configuration file to read.  If NULL,
 * then the user's home directory is checked for the presence of the default
 * configuration file.  If found, that file is read.
 * @return Returns the full path of the configuration file that was read or
 * NULL if none.
 */
char const* read_conf( char const *conf_file ) {
  bool const is_explicit_conf_file = (conf_file != NULL);

  section_t section = SECTION_NONE;     // section we're in

  if ( !is_explicit_conf_file ) {       // no explicit conf file: use default
    char const *const home = home_dir();
    if ( home == NULL )
      return NULL;
    static char conf_path_buf[ PATH_MAX ];
    strcpy( conf_path_buf, home );
    path_append( conf_path_buf, CONF_FILE_NAME_DEFAULT );
    conf_file = conf_path_buf;
  }

  // open configuration file
  FILE *const fconf = fopen( conf_file, "r" );
  if ( fconf == NULL ) {
    if ( is_explicit_conf_file )
      fatal_error( EX_NOINPUT, "%s: %s\n", conf_file, STRERROR() );
    return NULL;
  }

  // parse configuration file
  line_buf_t line_buf;
  unsigned line_no = 0;
  while ( fgets( line_buf, sizeof line_buf, fconf ) != NULL ) {
    ++line_no;
    char *line = strip_comment( line_buf );
    if ( line == NULL ) {
      fatal_error( EX_CONFIG,
        "%s:%u: \"%s\": unclosed quote\n",
        conf_file, line_no, trim_ws( line_buf )
      );
    }
    line = trim_ws( line );
    if ( *line == '\0' )                // line was entirely whitespace
      continue;

    // parse section line
    if ( line[0] == '[' ) {
      section = section_parse( line );
      if ( section == SECTION_NONE ) {
        fatal_error( EX_CONFIG,
          "%s:%u: \"%s\": invalid section\n",
          conf_file, line_no, line
        );
      }
      continue;
    }

    // parse line within section
    switch ( section ) {
      case SECTION_NONE:
        fatal_error( EX_CONFIG,
          "%s:%u: \"%s\": line not within any section\n",
          conf_file, line_no, line
        );
      case SECTION_ALIASES:
        alias_parse( line, conf_file, line_no );
        break;
      case SECTION_PATTERNS:
        pattern_parse( line, conf_file, line_no );
        break;
    } // switch
  } // while

  if ( unlikely( ferror( fconf ) ) )
    fatal_error( EX_IOERR, "%s: %s\n", conf_file, STRERROR() );
  fclose( fconf );

#ifndef NDEBUG
  if ( is_affirmative( getenv( "WRAP_DUMP_CONF" ) ) ) {
    dump_aliases();
    dump_patterns();
    exit( EX_OK );
  }
#endif /* NDEBUG */

  return conf_file;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
