/*
**      wrap -- text reformatter
**      util.c
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
#include "config.h"
#define WRAP_UTIL_INLINE _GL_EXTERN_INLINE
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>                     /* for malloc(), ... */
#include <string.h>
#include <sysexits.h>

#ifdef WITH_WIDTH_TERM
# if defined(HAVE_CURSES_H)
#   define _BOOL                        /* prevents clash of bool on Solaris */
#   include <curses.h>
#   undef _BOOL
# elif defined(HAVE_NCURSES_H)
#   include <ncurses.h>
# endif
# include <fcntl.h>                     /* for open(2) */
# include <term.h>                      /* for setupterm(3) */
#endif /* WITH_WIDTH_TERM */

#ifndef NDEBUG
# include <signal.h>                    /* for raise(3) */
# include <unistd.h>                    /* for getpid(3) */
#endif /* NDEBUG */

///////////////////////////////////////////////////////////////////////////////

/**
 * A node for a singly linked list of pointers to memory to be freed via
 * \c atexit().
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
  assert( path_name );
  char const *const slash = strrchr( path_name, '/' );
  if ( slash )
    return slash[1] ? slash + 1 : slash;
  return path_name;
}

unsigned check_atou( char const *s ) {
  assert( s );
  if ( !is_digits( s ) )
    PMESSAGE_EXIT( EX_USAGE, "\"%s\": invalid integer\n", s );
  return (unsigned)strtoul( s, NULL, 10 );
}

void* check_realloc( void *p, size_t size ) {
  //
  // Autoconf, 5.5.1:
  //
  // realloc
  //    The C standard says a call realloc(NULL, size) is equivalent to
  //    malloc(size), but some old systems don't support this (e.g., NextStep).
  //
  if ( !size )
    size = 1;
  void *const r = p ? realloc( p, size ) : malloc( size );
  if ( !r )
    PERROR_EXIT( EX_OSERR );
  return r;
}

char* check_strdup( char const *s ) {
  assert( s );
  char *const dup = strdup( s );
  if ( !dup )
    PERROR_EXIT( EX_OSERR );
  return dup;
}

void fcopy( FILE *ffrom, FILE *fto ) {
  assert( ffrom );
  assert( fto );

  char buf[ 8192 ];
  for ( size_t size; (size = fread( buf, 1, sizeof buf, ffrom )) > 0; )
    W_FWRITE( buf, 1, size, fto );
  FERROR( ffrom );
}

char* fgetsz( char *buf, size_t *size, FILE *ffrom ) {
  assert( buf );
  assert( size );
  assert( ffrom );
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
    if ( (*s++ = c) == '\n' )
      break;

  *s = '\0';
  *size = s - buf;

  return c == EOF && !*size ? NULL : buf;
}

void* free_later( void *p ) {
  assert( p );
  free_node_t *const new_node = MALLOC( free_node_t, 1 );
  new_node->fn_ptr = p;
  new_node->fn_next = free_head;
  free_head = new_node;
  return p;
}

void free_now( void ) {
  for ( free_node_t *p = free_head; p; ) {
    free_node_t *const next = p->fn_next;
    free( p->fn_ptr );
    free( p );
    p = next;
  } // for
  free_head = NULL;
}

#ifdef WITH_WIDTH_TERM
unsigned get_term_columns( void ) {
  static unsigned const UNSET = (unsigned)-1;
  static unsigned cols = UNSET;

  if ( cols == UNSET ) {
    cols = 0;

    char reason_buf[ 128 ];
    char const *reason = NULL;
    int tty_fd = -1;

    char const *const ctty_path = ctermid( NULL );
    if ( !ctty_path || !*ctty_path ) {
      reason = "ctermid(3) failed to get controlling terminal";
      goto error;
    }

    if ( (tty_fd = open( ctty_path, O_RDWR )) == -1 ) {
      reason = STRERROR;
      goto error;
    }

    char const *const term = getenv( "TERM" );
    if ( !term ) {
      reason = "TERM environment variable not set";
      goto error;
    }

    int setupterm_err;
    if ( setupterm( (char*)term, tty_fd, &setupterm_err ) == ERR ) {
      reason = reason_buf;
      switch ( setupterm_err ) {
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
            "setupterm(3) returned error code %d", setupterm_err
          );
      } // switch
      goto error;
    }

    int const ti_cols = tigetnum( (char*)"cols" );
    if ( ti_cols < 0 ) {
      snprintf(
        reason_buf, sizeof reason_buf,
        "tigetnum(\"cols\") returned error code %d", ti_cols
      );
      goto error;
    }

    cols = (unsigned)ti_cols;

error:
    if ( tty_fd != -1 )
      close( tty_fd );
    if ( reason )
      PMESSAGE_EXIT( EX_UNAVAILABLE,
        "failed to determine number of columns in terminal: %s\n",
        reason
      );
  }

  return cols;
}
#endif /* WITH_WIDTH_TERM */

void setlocale_utf8( void ) {
  static char const *const UTF8_LOCALES[] = {
    "UTF-8", "UTF8",
    "en_US.UTF-8", "en_US.UTF8",
    NULL
  };
  for ( char const *const *loc = UTF8_LOCALES; *loc; ++loc ) {
    if ( setlocale( LC_COLLATE, *loc ) && setlocale( LC_CTYPE, *loc ) )
      return;
  } // for

  PRINT_ERR( "%s: could not set locale to UTF-8; tried: ", me );
  bool comma = false;
  for ( char const *const *loc = UTF8_LOCALES; *loc; ++loc ) {
    PRINT_ERR( "%s%s", (comma ? ", " : ""), *loc );
    comma = true;
  } // for
  FPUTC( '\n', stderr );

  exit( EX_UNAVAILABLE );
}

void split_tws( char buf[], size_t buf_len, char tws[] ) {
  size_t const tnws_len = buf_len - strrspn( buf, WS_ST );
  strcpy( tws, buf + tnws_len );
  buf[ tnws_len ] = '\0';
}

size_t strrspn( char const *s, char const *set ) {
  assert( s );
  assert( set );

  size_t n = 0;
  for ( char const *t = s + strlen( s ); t-- > s && strchr( set, *t ); ++n )
    /* empty */;
  return n;
}

char* tolower_s( char *s ) {
  assert( s );
  for ( char *t = s; *t; ++t )
    *t = tolower( *t );
  return s;
}

#ifndef NDEBUG
void wait_for_debugger_attach( char const *env_var ) {
  static char const *const AFFIRMATIVES[] = {
    "1",
    "t",
    "true",
    "y",
    "yes",
    NULL
  };

  assert( env_var );
  char *value = getenv( env_var );
  if ( value ) {
    value = tolower_s( value );
    for ( char const *const *a = AFFIRMATIVES; *a; ++a ) {
      if ( strcmp( value, *a ) == 0 ) {
        PRINT_ERR( "pid=%u: waiting for debugger to attach...\n", getpid() );
        if ( raise( SIGSTOP ) == -1 )
          PERROR_EXIT( EX_OSERR );
        break;
      }
    } // for
  }
}
#endif /* NDEBUG */

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
