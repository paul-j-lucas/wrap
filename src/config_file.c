/*
**      wrap -- text reformatter
**      src/config_file.c
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
#include <limits.h>                     /* for PATH_MAX, SIZE_MAX */
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
 * Options for the config_open() function.
 */
enum config_opts {
  CONFIG_OPT_NONE              = 0,       ///< No options.
  CONFIG_OPT_ERROR_IS_FATAL    = 1 << 0,  ///< An error is fatal.
  CONFIG_OPT_IGNORE_NOT_FOUND  = 1 << 1   ///< Ignore file not found.
};
typedef enum config_opts config_opts_t;

/** 
 * Configuration file section.
 */
enum config_section {
  CONFIG_SECTION_NONE,                  ///< No section.
  CONFIG_SECTION_ALIASES,               ///< `[ALIASES]` section.
  CONFIG_SECTION_PATTERNS               ///< `[PATTERNS]` section.
};
typedef enum config_section config_section_t;

////////// local functions ////////////////////////////////////////////////////

/**
 * Tries to open a configuration file given by \a path.
 *
 * @param path The full path to try to open.  May be NULL.
 * @param opts The configuration options, if any.
 * @return Returns a `FILE*` to the open file upon success or NULL upon either
 * error or if \a path is NULL.
 */
NODISCARD
static FILE* config_open( char const *path, config_opts_t opts ) {
  if ( path == NULL )
    return NULL;
  FILE *const config_file = fopen( path, "r" );
  if ( config_file == NULL ) {
    switch ( errno ) {
      case ENOENT:
        if ( (opts & CONFIG_OPT_IGNORE_NOT_FOUND) != 0 )
          break;
        FALLTHROUGH;
      default:
        if ( (opts & CONFIG_OPT_ERROR_IS_FATAL) != 0 ) {
          fatal_error( EX_NOINPUT,
            "configuration file \"%s\": %s\n", path, STRERROR()
          );
        }
        EPRINTF(
          "%s: warning: configuration file \"%s\": %s\n",
          prog_name, path, STRERROR()
        );
        break;
    } // switch
  }
  return config_file;
}

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
 * @param path The path to append to.  The buffer pointed to must be big enough
 * to hold the new path.
 * @param path_len The length of \a path.  If `SIZE_MAX`,
 * <code>strlen(</code>\a path<code>)</code> is used instead.
 * @param component The component to append.
 */
static void path_append( char *path, size_t path_len, char const *component ) {
  assert( path != NULL );
  assert( component != NULL );

  if ( path_len == SIZE_MAX )
    path_len = strlen( path );

  if ( path_len > 0 ) {
    path += path_len - 1;
    if ( *path != '/' )
      *++path = '/';
    strcpy( ++path, component );
  }
}

/**
 * Parses a section name.
 *
 * @param s The line containing the section name to parse.
 * @return Returns the corresponding section name or #CONFIG_SECTION_NONE.
 */
NODISCARD
static config_section_t section_parse( char const *s ) {
  assert( s != NULL );

  if ( strcmp( s, "[ALIASES]" ) == 0 )
    return CONFIG_SECTION_ALIASES;
  if ( strcmp( s, "[PATTERNS]" ) == 0 )
    return CONFIG_SECTION_PATTERNS;
  return CONFIG_SECTION_NONE;
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
static char* str_trim( char *s ) {
  assert( s != NULL );
  SKIP_CHARS( s, WS_STR );
  for ( size_t len = strlen( s ); len > 0 && isspace( s[ --len ] ); )
    s[ len ] = '\0';
  return s;
}

////////// extern functions ///////////////////////////////////////////////////

/**
 * Reads a **wrap**(1) configuration file, if any.
 *
 * @param config_path The full path of the configuration file to read.  If NULL,
 * then path of the configuration file is determined as follows (in priority
 * order):
 * @parblock
 *  1. The value of either the `--config` or `-c` command-line option; or:
 *  2. `~/.wraprc`; or:
 *  3. `$XDG_CONFIG_HOME/wrap` or `~/.config/wrap`; or:
 *  4. `$XDG_CONFIG_DIRS/wrap` for each path or `/etc/xdg/wrap`.
 * @endparblock
 * @return Returns the full path of the configuration file that was read or
 * NULL if none.
 */
char const* config_init( char const *config_path ) {
  ASSERT_RUN_ONCE();

  char const *home = NULL;
  static char path_buf[ PATH_MAX ];

  // 1. Try --config/-c command-line option.
  FILE *config_file = config_open( config_path, CONFIG_OPT_ERROR_IS_FATAL );

  // 2. Try $HOME/.wraprc.
  if ( config_file == NULL ) {
    config_path = path_buf;
    home = home_dir();
    if ( home != NULL ) {
      strcpy( path_buf, home );
      path_append( path_buf, SIZE_MAX, "." PACKAGE "rc" );
      config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
    }
  }

  // 3. Try $XDG_CONFIG_HOME/cdecl and $HOME/.config/cdecl.
  if ( config_file == NULL ) {
    char const *const config_dir = null_if_empty( getenv( "XDG_CONFIG_HOME" ) );
    if ( config_dir != NULL ) {
      strcpy( path_buf, config_dir );
    }
    else if ( home != NULL ) {
      // LCOV_EXCL_START
      strcpy( path_buf, home );
      path_append( path_buf, SIZE_MAX, ".config" );
      // LCOV_EXCL_STOP
    }
    if ( path_buf[0] != '\0' ) {
      path_append( path_buf, SIZE_MAX, PACKAGE );
      config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
      path_buf[0] = '\0';
    }
  }

  // 4. Try $XDG_CONFIG_DIRS/cdecl and /etc/xdg/cdecl.
  if ( config_file == NULL ) {
    char const *config_dirs = null_if_empty( getenv( "XDG_CONFIG_DIRS" ) );
    if ( config_dirs == NULL )
      config_dirs = "/etc/xdg";         // LCOV_EXCL_LINE
    for (;;) {
      char const *const next_sep = strchr( config_dirs, ':' );
      size_t const dir_len = next_sep != NULL ?
        STATIC_CAST( size_t, next_sep - config_dirs ) : strlen( config_dirs );
      if ( dir_len > 0 ) {
        strncpy( path_buf, config_dirs, dir_len );
        path_buf[ dir_len ] = '\0';
        path_append( path_buf, dir_len, PACKAGE );
        config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
        path_buf[0] = '\0';
        if ( config_file != NULL )
          break;
      }
      if ( next_sep == NULL )
        break;
      config_dirs = next_sep + 1;
    } // for
  }

  config_section_t curr_section = CONFIG_SECTION_NONE;

  // parse configuration file
  line_buf_t line_buf;
  unsigned line_no = 0;
  while ( fgets( line_buf, sizeof line_buf, config_file ) != NULL ) {
    ++line_no;
    char *line = strip_comment( line_buf );
    if ( line == NULL ) {
      fatal_error( EX_CONFIG,
        "%s:%u: \"%s\": unclosed quote\n",
        config_path, line_no, str_trim( line_buf )
      );
    }
    line = str_trim( line );
    if ( *line == '\0' )                // line was entirely whitespace
      continue;

    // parse section line
    if ( line[0] == '[' ) {
      curr_section = section_parse( line );
      if ( curr_section == CONFIG_SECTION_NONE ) {
        fatal_error( EX_CONFIG,
          "%s:%u: \"%s\": invalid section\n",
          config_path, line_no, line
        );
      }
      continue;
    }

    // parse line within section
    switch ( curr_section ) {
      case CONFIG_SECTION_NONE:
        fatal_error( EX_CONFIG,
          "%s:%u: \"%s\": line not within any section\n",
          config_path, line_no, line
        );
      case CONFIG_SECTION_ALIASES:
        alias_parse( line, config_path, line_no );
        break;
      case CONFIG_SECTION_PATTERNS:
        pattern_parse( line, config_path, line_no );
        break;
    } // switch
  } // while

  if ( unlikely( ferror( config_file ) ) )
    fatal_error( EX_IOERR, "%s: %s\n", config_path, STRERROR() );
  fclose( config_file );

#ifndef NDEBUG
  if ( is_affirmative( getenv( "WRAP_DUMP_CONF" ) ) ) {
    dump_aliases();
    dump_patterns();
    exit( EX_OK );
  }
#endif /* NDEBUG */

  return config_path;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
