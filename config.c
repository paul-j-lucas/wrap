/*
**      wrap -- text reformatter
**      config.c
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

#define CONFIG_FILE_NAME ".wraprc"

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

char const* read_config( char const *config_file ) {
  static char config_path_buf[ PATH_MAX ];
  bool const explicit_config_file = (config_file != NULL);
  FILE *fconfig;
  char line_buf[ LINE_BUF_SIZE ];
  int line_no = 0;

  enum section { NONE, ALIASES, PATTERNS };
  enum section section = NONE;

  if ( !config_file ) {
    char const *home = getenv( "HOME" );
    if ( !home ) {
      struct passwd *const pw = getpwuid( geteuid() );
      if ( !pw )
        return NULL;
      home = pw->pw_dir;
    }
    strcpy( config_path_buf, home );
    path_append( config_path_buf, CONFIG_FILE_NAME ); config_file = config_path_buf;
  }

  if ( !(fconfig = fopen( config_file, "r" )) ) {
    if ( explicit_config_file ) {
      fprintf( stderr, "%s: %s: %s\n", me, config_file, strerror( errno ) );
      exit( EXIT_CONFIG_ERROR );
    }
    return NULL;
  }

  while ( fgets( line_buf, LINE_BUF_SIZE, fconfig ) ) {
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
            me, config_file, line_no, line
          );
          exit( EXIT_CONFIG_ERROR );
        case ALIASES:
          alias_parse( line, config_file, line_no );
          break;
        case PATTERNS:
          pattern_parse( line, config_file, line_no );
          break;
      } /* switch */
    } /* else */
  } /* while */

  if ( ferror( fconfig ) ) {
    fprintf( stderr, "%s: %s: %s\n", me, config_file, strerror( errno ) );
    exit( EXIT_READ_ERROR );
  }
  fclose( fconfig );
  return config_file;
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
