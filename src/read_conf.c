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
**      along with this program; if not, write to the Free Software
**      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* system */
#include <ctype.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX */
#include <pwd.h>                        /* for getpwuid() */
#include <stdio.h>
#include <stdlib.h>                     /* for atoi(), ... */
#include <string.h>
#include <unistd.h>                     /* for getuid() */

/* local */
#include "alias.h"
#include "common.h"
#include "pattern.h"

#define CONF_FILE_NAME ".wraprc"

extern char const *me;

/*****************************************************************************/

static void path_append( char *path, char const *component ) {
  size_t len = strlen( path );
  if ( len ) {
    path += len - 1;
    if ( *path != '/' )
      strcpy( ++path, "/" );
    strcpy( ++path, component );
  }
}

static char* strip_comment( char *s ) {
  char *const comment = strchr( s, '#' );
  if ( comment )
    *comment = '\0';
  return s;
}

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
  enum section section = NONE;

  if ( !conf_file ) {
    char const *home = getenv( "HOME" );
    if ( !home ) {
      struct passwd *const pw = getpwuid( geteuid() );
      if ( !pw )
        return NULL;
      home = pw->pw_dir;
    }
    strcpy( conf_path_buf, home );
    path_append( conf_path_buf, CONF_FILE_NAME ); conf_file = conf_path_buf;
  }

  if ( !(fconf = fopen( conf_file, "r" )) ) {
    if ( explicit_conf_file ) {
      fprintf( stderr, "%s: %s: %s\n", me, conf_file, strerror( errno ) );
      exit( EXIT_CONF_ERROR );
    }
    return NULL;
  }

  while ( fgets( line_buf, LINE_BUF_SIZE, fconf ) ) {
    char const *const line = trim_ws( strip_comment( line_buf ) );
    ++line_no;
    if ( !*line )
      continue;
    if ( strcmp( line, "[ALIASES]" ) == 0 )
      section = ALIASES;
    else if ( strcmp( line, "[PATTERNS]" ) == 0 )
      section = PATTERNS;
    else {
      switch ( section ) {
        case NONE:
          fprintf(
            stderr, "%s: %s:%d: \"%s\": line not within any section\n",
            me, conf_file, line_no, line
          );
          exit( EXIT_CONF_ERROR );
        case ALIASES:
          alias_parse( line, conf_file, line_no );
          break;
        case PATTERNS:
          pattern_parse( line, conf_file, line_no );
          break;
      } /* switch */
    } /* else */
  } /* while */

  if ( ferror( fconf ) ) {
    fprintf( stderr, "%s: %s: %s\n", me, conf_file, strerror( errno ) );
    exit( EXIT_READ_ERROR );
  }
  fclose( fconf );
  return conf_file;
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
