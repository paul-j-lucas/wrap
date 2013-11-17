/*
**      wrap -- text reformatter
**      read_conf.c
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

#include "common.h"

/* system */
#include <ctype.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX */
#if HAVE_PWD_H
# include <pwd.h>                       /* for getpwuid() */
#endif /* HAVE_PWD_H */
#include <stdio.h>
#include <stdlib.h>                     /* for atoi(), ... */
#include <string.h>
#include <unistd.h>                     /* for geteuid() */

/* local */
#include "alias.h"
#ifdef WITH_PATTERNS
# include "pattern.h"
#endif /* WITH_PATTERNS */

#define CONF_FILE_NAME ".wraprc"

extern char const *me;

/*****************************************************************************/

/**
 * Appends a component to a path ensuring that exactly one \c / separates them.
 *
 * @param path The path to append to.
 * @param component The component to append.
 */
static void path_append( char *path, char const *component ) {
  size_t len = strlen( path );
  if ( len ) {
    path += len - 1;
    if ( *path != '/' )
      strcpy( ++path, "/" );
    strcpy( ++path, component );
  }
}

/**
 * Strips a \c # comment from a line.
 *
 * @param line The line to strip the comment from.
 * @return Returns \a line for convenience.
 */
static char* strip_comment( char *line ) {
  char *const comment = strchr( line, '#' );
  if ( comment )
    *comment = '\0';
  return line;
}

/**
 * Trims both leading and trailing whitespace from a string.
 *
 * @param s The string to trim whitespace from.
 * @return Returns a pointer to within \a s having all whitespace trimmed.
 */
static char* trim_ws( char *s ) {
  size_t len;
  s += strspn( s, " \t" );
  len = strlen( s );
  while ( len > 0 && isspace( s[ --len ] ) )
    s[ len ] = '\0';
  return s;
}

/*****************************************************************************/

char const* read_conf( char const *conf_file ) {
  static char conf_path_buf[ PATH_MAX ];
  bool const explicit_conf_file = (conf_file != NULL);
  FILE *fconf;
  char line_buf[ LINE_BUF_SIZE ];
  int line_no = 0;

  enum section { NONE, ALIASES, PATTERNS };
  enum section section = NONE;          /* section we're in */

  /* locate default configuration file */
  if ( !conf_file ) {
    char const *home = getenv( "HOME" );
    if ( !home ) {
#if HAVE_GETEUID && HAVE_GETPWUID && HAVE_STRUCT_PASSWD_PW_DIR
      struct passwd *const pw = getpwuid( geteuid() );
      if ( !pw )
        return NULL;
      home = pw->pw_dir;
#else
      return NULL;
#endif /* HAVE_GETEUID && && HAVE_GETPWUID && HAVE_STRUCT_PASSWD_PW_DIR */
    }
    strcpy( conf_path_buf, home );
    path_append( conf_path_buf, CONF_FILE_NAME );
    conf_file = conf_path_buf;
  }

  /* open configuration file */
  if ( !(fconf = fopen( conf_file, "r" )) ) {
    if ( explicit_conf_file )
      PMESSAGE_EXIT( READ_OPEN, "%s: %s\n", conf_file, strerror( errno ) );
    return NULL;
  }

  /* parse configuration file */
  while ( fgets( line_buf, LINE_BUF_SIZE, fconf ) ) {
    char const *const line = trim_ws( strip_comment( line_buf ) );
    ++line_no;
    if ( !*line )
      continue;

    /* parse section line */
    if ( line[0] == '[' ) {
      if ( strcmp( line, "[ALIASES]" ) == 0 )
        section = ALIASES;
      else if ( strcmp( line, "[PATTERNS]" ) == 0 )
        section = PATTERNS;
      else
        PMESSAGE_EXIT( CONF_ERROR,
          "%s:%d: \"%s\": invalid section\n",
          conf_file, line_no, line
        );
      continue;
    }

    /* parse line within section */
    switch ( section ) {
      case NONE:
        PMESSAGE_EXIT( CONF_ERROR,
          "%s:%d: \"%s\": line not within any section\n",
          conf_file, line_no, line
        );
      case ALIASES:
        alias_parse( line, conf_file, line_no );
        break;
#ifdef WITH_PATTERNS
      case PATTERNS:
        pattern_parse( line, conf_file, line_no );
        break;
#endif /* WITH_PATTERNS */
    } /* switch */
  } /* while */

  if ( ferror( fconf ) )
    PMESSAGE_EXIT( READ_ERROR, "%s: %s\n", conf_file, strerror( errno ) );
  fclose( fconf );
  return conf_file;
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
