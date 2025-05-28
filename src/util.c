/*
**      wrap -- text reformatter
**      src/util.c
**
**      Copyright (C) 2013-2024  Paul J. Lucas
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
 * Defines utility data structures, variables, and functions.
 */

// local
#include "pjl_config.h"                 /* must go first */
#define W_UTIL_H_INLINE _GL_EXTERN_INLINE
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#ifndef NDEBUG
#include <signal.h>                     /* for raise(3) */
#endif /* NDEBUG */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>                     /* for malloc(), ... */
#include <string.h>
#include <sysexits.h>
#include <unistd.h>                     /* for close(2), getpid(3) */

#ifdef WITH_WIDTH_TERM
# include <fcntl.h>                     /* for open(2) */
# if HAVE_CURSES_H
#   define _BOOL /* nothing */          /* prevent bool clash on AIX/Solaris */
#   include <curses.h>
#   undef _BOOL
# elif HAVE_NCURSES_H
#   include <ncurses.h>
# endif
# include <term.h>                      /* for setupterm(3) */
#endif /* WITH_WIDTH_TERM */

/// @endcond

/**
 * @addtogroup util-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * A node for a singly linked list of pointers to memory to be freed via
 * `atexit()`.
 */
struct free_node {
  void             *fn_ptr;
  struct free_node *fn_next;
};
typedef struct free_node free_node_t;

// local variable definitions
static free_node_t *free_head;          // linked list of stuff to free

////////// extern functions ///////////////////////////////////////////////////

char const* base_name( char const *path_name ) {
  assert( path_name != NULL );
  char const *const slash = strrchr( path_name, '/' );
  if ( slash != NULL )
    return slash[1] ? slash + 1 : path_name;
  return path_name;
}

unsigned check_atou( char const *s ) {
  assert( s != NULL );
  if ( !is_digits( s ) )
    fatal_error( EX_USAGE, "\"%s\": invalid integer\n", s );
  errno = 0;
  unsigned long const rv = strtoul( s, NULL, 10 );
  PERROR_EXIT_IF( errno != 0, EX_USAGE );
  return STATIC_CAST( unsigned, rv );
}

void* check_realloc( void *p, size_t size ) {
  assert( size > 0 );
  p = p != NULL ? realloc( p, size ) : malloc( size );
  PERROR_EXIT_IF( p == NULL, EX_OSERR );
  return p;
}

char* check_strdup( char const *s ) {
  assert( s != NULL );
  char *const dup = strdup( s );
  PERROR_EXIT_IF( dup == NULL, EX_OSERR );
  return dup;
}

size_t chop_eol( char *s, size_t s_len ) {
  assert( s != NULL );
  if ( s_len > 0 && s[ s_len-1 ] == '\n' ) {
    if ( --s_len > 0 && s[ s_len-1 ] == '\r' )
      --s_len;
    s[ s_len ] = '\0';
  }
  return s_len;
}

char closing_char( char c ) {
  switch ( c ) {
    case '(': return ')' ;
    case '<': return '>' ;
    case '[': return ']' ;
    case '{': return '}' ;
    default : return '\0';
  } // switch
}

void fatal_error( int status, char const *format, ... ) {
  EPRINTF( "%s: ", me );
  va_list args;
  va_start( args, format );
  vfprintf( stderr, format, args );
  va_end( args );
  _Exit( status );
}

void fcopy( FILE *ffrom, FILE *fto ) {
  assert( ffrom != NULL );
  assert( fto != NULL );

  char buf[ 8192 ];
  for ( size_t size; (size = fread( buf, 1, sizeof buf, ffrom )) > 0; )
    PERROR_EXIT_IF( fwrite( buf, 1, size, fto ) < size, EX_IOERR );
  FERROR( ffrom );
}

char* fgetsz( char *buf, size_t *size, FILE *ffrom ) {
  assert( buf != NULL );
  assert( size != NULL );
  assert( ffrom != NULL );
  //
  // Based on the implementation given in:
  //
  //  + Brian W. Kernighan and Dennis M. Ritchie: "The C Programming Language,"
  //    2nd ed., section 7.7 "Line Input and Output," Prentice Hall, 1988, p.
  //    134.
  //
  int c = 0;
  char *s = buf;

  for ( size_t n = *size; n > 0 && (c = getc( ffrom )) != EOF; --n )
    if ( (*s++ = STATIC_CAST( char, c )) == '\n' )
      break;

  *s = '\0';
  *size = STATIC_CAST( size_t, s - buf );

  return c == EOF && *size == 0 ? NULL : buf;
}

void* free_later( void *p ) {
  assert( p != NULL );
  free_node_t *const new_node = MALLOC( free_node_t, 1 );
  new_node->fn_ptr = p;
  new_node->fn_next = free_head;
  free_head = new_node;
  return p;
}

void free_now( void ) {
  for ( free_node_t *p = free_head; p != NULL; ) {
    free_node_t *const next = p->fn_next;
    FREE( p->fn_ptr );
    FREE( p );
    p = next;
  } // for
  free_head = NULL;
}

#ifdef WITH_WIDTH_TERM
unsigned get_term_columns( void ) {
  static unsigned const UNSET = STATIC_CAST( unsigned, -1 );
  static unsigned cols = UNSET;

  if ( cols == UNSET ) {
    cols = 0;

    int         cterm_fd = -1;
    char        reason_buf[ 128 ];
    char const *reason = NULL;

    char const *const term = getenv( "TERM" );
    if ( unlikely( term == NULL ) ) {
      reason = "TERM environment variable not set";
      goto error;
    }

    char const *const cterm_path = ctermid( NULL );
    if ( unlikely( cterm_path == NULL || *cterm_path == '\0' ) ) {
      reason = "ctermid(3) failed to get controlling terminal";
      goto error;
    }

    if ( unlikely( (cterm_fd = open( cterm_path, O_RDWR )) == -1 ) ) {
      reason = STRERROR();
      goto error;
    }

    int sut_err;
    if ( setupterm( CONST_CAST( char*, term ), cterm_fd, &sut_err ) == ERR ) {
      reason = reason_buf;
      switch ( sut_err ) {
        case -1:
          reason = "terminfo database not found";
          break;
        case 0:
          snprintf(
            reason_buf, sizeof reason_buf,
            "TERM=%s not found in database or too generic", term
          );
          break;
        case 1:
          reason = "terminal is harcopy";
          break;
        default:
          snprintf(
            reason_buf, sizeof reason_buf,
            "setupterm(3) returned error code %d", sut_err
          );
      } // switch
      goto error;
    }

    int const ti_cols = tigetnum( CONST_CAST( char*, "cols" ) );
    if ( unlikely( ti_cols < 0 ) ) {
      snprintf(
        reason_buf, sizeof reason_buf,
        "tigetnum(\"cols\") returned error code %d", ti_cols
      );
      goto error;
    }

    cols = STATIC_CAST( unsigned, ti_cols );

error:
    if ( likely( cterm_fd != -1 ) )
      close( cterm_fd );
    if ( unlikely( reason != NULL ) ) {
      fatal_error( EX_UNAVAILABLE,
        "failed to determine number of columns in terminal: %s\n",
        reason
      );
    }
  }

  return cols;
}
#endif /* WITH_WIDTH_TERM */

#ifndef NDEBUG
bool is_affirmative( char const *s ) {
  static char const *const AFFIRMATIVES[] = {
    "1",
    "t",
    "true",
    "y",
    "yes",
    NULL
  };
  return is_any( s, AFFIRMATIVES );
}
#endif /* NDEBUG */

bool is_any( char const *s, char const *const matches[const static 2] ) {
  if ( s != NULL ) {
    for ( char const *const *match = matches; *match != NULL; ++match ) {
      if ( strcasecmp( s, *match ) == 0 )
        return true;
    } // for
  }
  return false;
}

void perror_exit( int status ) {
  perror( me );
  exit( status );
}

void setlocale_utf8( void ) {
  static char const *const UTF8_LOCALES[] = {
    "UTF-8",        "UTF8",
    "en_US.UTF-8",  "en_US.UTF8",
    "C.UTF-8",      "C.UTF8",
    NULL
  };
  for ( char const *const *loc = UTF8_LOCALES; *loc != NULL; ++loc ) {
    if ( setlocale( LC_COLLATE, *loc ) && setlocale( LC_CTYPE, *loc ) )
      return;
  } // for

  EPRINTF( "%s: could not set locale to UTF-8; tried: ", me );
  bool comma = false;
  for ( char const *const *loc = UTF8_LOCALES; *loc != NULL; ++loc ) {
    EPRINTF( "%s%s", (comma ? ", " : ""), *loc );
    comma = true;
  } // for
  EPUTC( '\n' );

  exit( EX_UNAVAILABLE );
}

void split_tws( char buf[const], size_t buf_len, char tws[const] ) {
  size_t const tnws_len = buf_len - strrspn( buf, WS_ST );
  strcpy( tws, buf + tnws_len );
  buf[ tnws_len ] = '\0';
}

size_t strcpy_len( char *dst, char const *src ) {
  assert( dst != NULL );
  assert( src != NULL );
  char const *const dst0 = dst;
  while ( (*dst++ = *src++) != '\0' )
    /* empty */;
  return STATIC_CAST( size_t, dst - dst0 - 1 );
}

size_t strrspn( char const *s, char const *set ) {
  assert( s != NULL );
  assert( set != NULL );

  size_t n = 0;
  for ( char const *t = s + strlen( s );
        t-- > s && strchr( set, *t ) != NULL;
        ++n ) {
    // empty
  } // for
  return n;
}

#ifndef NDEBUG
void wait_for_debugger_attach( char const *env_var ) {
  assert( env_var != NULL );
  if ( is_affirmative( getenv( env_var ) ) ) {
    EPRINTF( "pid=%u: waiting for debugger to attach...\n", getpid() );
    PERROR_EXIT_IF( raise( SIGSTOP ) == -1, EX_OSERR );
  }
}
#endif /* NDEBUG */

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
