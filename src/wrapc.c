/*
**      wrapc -- comment reformatter
**      wrapc.c
**
**      Copyright (C) 1996-2016  Paul J. Lucas
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
#include "markdown.h"
#include "options.h"
#include "pattern.h"
#include "util.h"

// standard
#include <assert.h>
#include <limits.h>                     /* for PATH_MAX */
#include <signal.h>                     /* for kill() */
#include <stdio.h>
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <sys/wait.h>                   /* for wait() */
#include <unistd.h>                     /* for close(), fork(), ... */

///////////////////////////////////////////////////////////////////////////////

/**
 * Uncomment the line below to debug read_source_write_wrap() (RSWW) by not
 * forking or exec'ing and just writing to stdout directly.
 */
//#define DEBUG_RSWW

#ifdef DEBUG_RSWW
# undef PIPE
# define PIPE(P) (void)0                /* need something to eat the ';' */
#endif /* DEBUG_RSWW */

// Closes both ends of pipe P.
#define CLOSE_PIPES(P) \
  BLOCK( close( pipes[P][ STDIN_FILENO ] ); close( pipes[P][ STDOUT_FILENO ] ); )

// Redirects file-descriptor FD to/from pipe P.
#define REDIRECT(FD,P) \
  BLOCK( close( FD ); DUP( pipes[P][FD] ); CLOSE_PIPES( P ); )

/**
 * Contains the current and next lines of input so the next line can be peeked
 * at to determine how to proceed.
 */
struct dual_line {
  line_buf_t  line[2];
  char       *curr;                     // pointer to current line
  char       *next;                     // pointer to next line
};
typedef struct dual_line dual_line_t;

// extern variable definitions
char const         *me;                 // executable name

// local constant definitions
static size_t const ARG_BUF_SIZE = 25;  // max wrap command-line arg size
static char const   WS_CHARS[] = " \t"; // whitespace characters

// local variable definitions
static dual_line_t  input_buf;
static line_buf_t   proto_buf;          // characters stripped/prepended
static size_t       proto_len0;         // length of initial proto_buf
//
// Two pipes:
// + pipes[0][0] -> wrap(1)                   [child 2]
//           [1] <- read_source_write_wrap()  [child 1]
// + pipes[1][0] -> read_wrap()               [parent]
//           [1] <- wrap(1)                   [child 2]
//
static int          pipes[2][2];

#define CURR        input_buf.curr      /* current line */
#define NEXT        input_buf.next      /* next line */

#define TO_WRAP     0                   /* to refer to pipes[0] */
#define FROM_WRAP   1                   /* to refer to pipes[1] */

// local functions
static void         fork_exec_wrap( pid_t );
static void         init( int, char const*[] );
static bool         is_block_comment( char const* );
static char const*  is_line_comment( char const* );
static size_t       proto_span( char const* );
static size_t       proto_width( char const* );
static void         read_prototype( void );
static pid_t        read_source_write_wrap( void );
static void         read_wrap( void );
static char const*  skip_n( char const*, size_t );
static char const*  str_status( int );
static void         usage( void );
static void         wait_for_child_processes( void );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Gets whether \a c is a comment character.
 *
 * @param c The character to check.
 * @return Returns \c true only if it is.
 */
static inline bool is_comment_char( char c ) {
  return c && strchr( opt_comment_chars, c ) != NULL;
}

/**
 * Gets whether the first non-whitespace character in \a s is a comment
 * character.
 *
 * @param s The string to check.
 * @return Returns a pointer to the first non-whitespace character in \a s only
 * if it's a comment character; NULL otherwise.
 */
static inline char const* is_line_comment( char const *s ) {
  SKIP_WS( s );
  return is_comment_char( s[0] ) ? s : NULL;
}

/**
 * Swaps the two line buffers.
 */
static inline void swap_line_bufs( void ) {
  char *const temp = CURR;
  CURR = NEXT;
  NEXT = temp;
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *argv[] ) {
  init( argc, argv );
  fork_exec_wrap( read_source_write_wrap() );
  read_wrap();
  wait_for_child_processes();
  exit( EX_OK );
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Forks and execs into wrap(1).
 *
 * @param read_source_write_wrap_pid The process ID of read_source_write_wrap()
 * (in case we need to kill it).
 */
static void fork_exec_wrap( pid_t read_source_write_wrap_pid ) {
#ifndef DEBUG_RSWW
  pid_t const pid = fork();
  if ( pid == -1 ) {                    // we failed, so kill the first child
    kill( read_source_write_wrap_pid, SIGTERM );
    PERROR_EXIT( EX_OSERR );
  }
  if ( pid != 0 )                       // parent process
    return;

  typedef char arg_buf_t[ ARG_BUF_SIZE ];

  arg_buf_t arg_opt_alias;
  arg_buf_t arg_opt_conf_file;
  arg_buf_t arg_opt_eol;
  char      arg_opt_fin_name[ PATH_MAX ];
  arg_buf_t arg_opt_lead_para_delims;
  arg_buf_t arg_opt_line_width;
  arg_buf_t arg_opt_para_delims;
  arg_buf_t arg_opt_tab_spaces;

  int argc = 0;
  char *argv[14];                       // must be +1 of greatest arg below

#define ARG_SET(ARG)          argv[ argc++ ] = (ARG)
#define ARG_DUP(FMT)          ARG_SET( check_strdup( FMT ) )
#define ARG_END               argv[ argc ] = NULL

#define ARG_SPRINTF(ARG,FMT) \
  snprintf( arg_##ARG, sizeof arg_##ARG, (FMT), (ARG) )
#define ARG_FMT(ARG,FMT) \
  BLOCK( ARG_SPRINTF( ARG, (FMT) ); ARG_SET( arg_##ARG ); )

#define IF_ARG_DUP(ARG,FMT)   if ( ARG ) ARG_DUP( FMT )
#define IF_ARG_FMT(ARG,FMT)   if ( ARG ) ARG_FMT( ARG, FMT )

  // Quoting string arguments is unnecessary since no shell is involved.

  /*  0 */    ARG_DUP(                       PACKAGE );
  /*  1 */ IF_ARG_FMT( opt_alias           , "-a%s"  );
  /*  2 */ IF_ARG_FMT( opt_lead_para_delims, "-b%s"  );
  /*  3 */ IF_ARG_FMT( opt_conf_file       , "-c%s"  );
  /*  4 */ IF_ARG_DUP( opt_no_conf         , "-C"    );
  /*  5 */ IF_ARG_DUP( opt_eos_delimit     , "-e"    );
  /*  6 */    ARG_DUP(                       "-E"    );
  /*  7 */ IF_ARG_FMT( opt_fin_name        , "-F%s"  );
  /*  8 */    ARG_FMT( opt_eol             , "-l%c"  );
  /*  9 */ IF_ARG_FMT( opt_para_delims     , "-p%s"  );
  /* 10 */ IF_ARG_DUP( opt_markdown        , "-u"    );
         else ARG_FMT( opt_tab_spaces      , "-s%zu" );
  /* 11 */ IF_ARG_DUP( opt_title_line      , "-T"    );
  /* 12 */    ARG_FMT( opt_line_width      , "-w%zu" );
  /* 13 */    ARG_END;

  //
  // Read from pipes[TO_WRAP] (read_source_write_wrap() in child 1) and write
  // to pipes[FROM_WRAP] (read_wrap() in parent); exec into wrap(1).
  //
  REDIRECT( STDIN_FILENO, TO_WRAP );
  REDIRECT( STDOUT_FILENO, FROM_WRAP );
  execvp( PACKAGE, argv );
  PERROR_EXIT( EX_OSERR );
#endif /* DEBUG_RSWW */
}

/**
 * Checks whether the given string is the beginning of a block comment: starts
 * with a comment character and contains only non-alpha characters thereafter.
 *
 * @param s The string to check.
 * @return Returns \c true only if \a s is the beginning of a block comment.
 */
static bool is_block_comment( char const *s ) {
  if ( (s = is_line_comment( s )) ) {
    for ( ++s; *s && *s != '\n' && !isalpha( *s ); ++s )
      /* empty */;
    return *s == '\n';
  }
  return false;
}

/**
 * Reads the first line of input to obtain a sequence of leading characters to
 * be the prototype for all lines.  Handles C-style block comments as a special
 * case.
 */
static void read_prototype( void ) {
  size_t const size = buf_read( CURR, fin );
  if ( !size )
    exit( EX_OK );

  if ( opt_eol == EOL_INPUT && size >= 2 && CURR[ size - 2 ] == '\r' ) {
    //
    // Retroactively set opt_eol because we pass it to wrap(1).
    //
    opt_eol = EOL_WINDOWS;
  }

  char const *const cc = is_line_comment( CURR );
  if ( cc ) {
    static char comment_chars_buf[4];   // 1-3 chars + NULL
    char *s = comment_chars_buf;
    //
    // From now on, recognize only the comment character found as a comment
    // delimiter.  This handles cases like:
    //
    //      // This is a comment
    //      #define MACRO
    //
    // where a comment is followed by a line that is not part of the comment
    // even though it starts with the comment delimiter '#'.
    //
    *s++ = cc[0];
    //
    // As special-cases, we also have to recognize the second character of
    // two-character delimiters, but only if it's not the same as the first
    // character and among the set of specified comment characters.
    //
    switch ( cc[0] ) {
      case '{':     // {- Haskell
        //
        // As even a more special-case for Pascal (which is perhaps unique
        // among languages since it uses a single-character delimiter for block
        // comments), this handles the case like:
        //
        //      {
        //        This is a block comment.
        //      }
        //
        if ( is_comment_char( '}' ) )
          *s++ = '}';
        // no break;
      case '#':     // #| Lisp, Racket, Scheme
      case '(':     // (* AppleScript, Delphi, ML, OCaml, Pascal; (: XQuery
      case '/':     // /* C, Objective C, C++, C#, D, Go, Java, Rust, Swift
      case '<':     // <# PowerShell
        if ( cc[1] != cc[0] && is_comment_char( cc[1] ) )
          *s++ = cc[1];
    } // switch
    opt_comment_chars = comment_chars_buf;
  }

  char const *proto = CURR;
  if ( is_block_comment( CURR ) ) {
    //
    // This handles cases like:
    //
    //      /*
    //       * This is a comment.
    //       */
    //
    // where the first line is the start of a block comment so:
    // + The first line should not be altered.
    // + The second line becomes the prototype.
    //
    (void)buf_read( NEXT, fin );
    proto = NEXT;
  }

  proto_len0 = proto_span( proto );
  strncpy( proto_buf, proto, proto_len0 );
  proto_buf[ proto_len0 ] = '\0';

  opt_line_width -= proto_width( proto_buf );
  if ( opt_line_width < LINE_WIDTH_MINIMUM )
    PMESSAGE_EXIT( EX_USAGE,
      "line-width (%zu) is too small (<%d)\n",
      opt_line_width, LINE_WIDTH_MINIMUM
    );
}

/**
 * Reads the source text to be wrapped and writes it, with the leading
 * whitespace and comment characters stripped from each line, to wrap(1).
 *
 * @return Returns the child's process ID.
 */
static pid_t read_source_write_wrap( void ) {
#ifndef DEBUG_RSWW
  pid_t const pid = fork();
  if ( pid == -1 )
    PERROR_EXIT( EX_OSERR );
  if ( pid != 0 )                       // parent process
    return pid;
  //
  // We don't use these here.
  //
  CLOSE_PIPES( FROM_WRAP );
  close( pipes[ TO_WRAP ][ STDIN_FILENO ] );
  //
  // Read from fin and write to pipes[TO_WRAP] (wrap).
  //
  FILE *const fwrap = fdopen( pipes[ TO_WRAP ][ STDOUT_FILENO ], "w" );
  if ( !fwrap )
    PMESSAGE_EXIT( EX_OSERR,
      "child can't open pipe for writing: %s\n", ERROR_STR
    );
#else
  FILE *const fwrap = stdout;
#endif /* DEBUG_RSWW */

  if ( NEXT[0] ) {
    //
    // For block comments, write the first line verbatim directly to the
    // output.
    //
    FPUTS( CURR, fout );
    swap_line_bufs();
  }

  //
  // As a special-case, if the first line is NOT a comment, then just wrap all
  // lines using the leading whitespace of the first line as a prototype for
  // all subsequent lines, i.e., do NOT ever tell wrap(1) to pass text through
  // verbatim (below).
  //
  bool const proto_is_comment = is_line_comment( CURR );

  while ( CURR[0] ) {
    //
    // In order to know when a comment ends, we have to peek at the next line.
    //
    (void)buf_read( NEXT, fin );

    if ( proto_is_comment && !is_line_comment( CURR ) ) {
      //
      // This handles cases like:
      //
      //      proto_buf ->  # This is a comment.
      //      cur_buf   ->  not_a_comment();
      //
      goto verbatim;
    }

    if ( !(proto_is_comment && is_line_comment( NEXT )) &&
         is_block_comment( CURR ) ) {
      //
      // This handles cases like:
      //
      //                    /*
      //      proto_buf ->  This is a comment.
      //      cur_buf   ->  */
      //
      // or:
      //                    /*
      //                     * This is a comment.
      //      cur_buf   ->   */
      //      next_buf  ->  [empty]
      //
      goto verbatim;
    }

    //
    // When wrapping Markdown, we can't strip all whitespace after the comment
    // characters because Markdown relies on indentation; hence we strip only
    // the length of the initial prototype -- but only if its less.
    //
    size_t proto_len = proto_span( CURR );
    if ( opt_markdown && proto_len > proto_len0 )
      proto_len = proto_len0;

    FPUTS( skip_n( CURR, proto_len ), fwrap );
    swap_line_bufs();
  } // while
  exit( EX_OK );

verbatim:
  //
  // We've reached the end of the comment: signal wrap(1) that we're now
  // passing text through verbatim and do so.
  //
  FPRINTF( fwrap, "%c%c%s%s", ASCII_DLE, ASCII_ETB, CURR, NEXT );
  fcopy( fin, fwrap );
  exit( EX_OK );
}

/**
 * Reads the output of wrap(1) and prepends the leading whitespace and comment
 * characters back to each line.
 */
static void read_wrap( void ) {
#ifndef DEBUG_RSWW
  //
  // We don't use these here.
  //
  CLOSE_PIPES( TO_WRAP );
  close( pipes[ FROM_WRAP ][ STDOUT_FILENO ] );
  //
  // Read from pipes[FROM_WRAP] (wrap) and write to fout.
  //
  FILE *const fwrap = fdopen( pipes[ FROM_WRAP ][ STDIN_FILENO ], "r" );
  if ( !fwrap )
    PMESSAGE_EXIT( EX_OSERR,
      "parent can't open pipe for reading: %s\n", ERROR_STR
    );

  //
  // Split off the trailing non-whitespace (tnws) from the prototype so that if
  // we read a comment that's empty except for the prototype, we won't emit
  // trailing whitespace. For example, given:
  //
  //      # foo
  //      #
  //      # bar
  //
  // the prototype initially is "# " (because that's what's before "foo").  If
  // we didn't split off trailing non-whitespace, then when we wrapped the
  // comment above, the second line would become "# " containing a trailing
  // whitespace.
  //
  size_t const tnws_len = proto_len0 - strrspn( proto_buf, WS_CHARS );
  line_buf_t proto_tws;                 // prototype trailing whitespace, if any
  strcpy( proto_tws, proto_buf + tnws_len );
  proto_buf[ tnws_len ] = '\0';

  line_buf_t line_buf;

  while ( fgets( line_buf, sizeof line_buf, fwrap ) ) {
    char const *line = line_buf;
    if ( line[0] == ASCII_DLE ) {
      switch ( line[1] ) {
        case ASCII_ETB:
          //
          // We've been told by child 1 (read_source_write_wrap(), via child 2,
          // wrap) that we've reached the end of the comment: dump any
          // remaining buffer and pass text through verbatim.
          //
          FPUTS( line + 2, fout );
          fcopy( fwrap, fout );
          goto break_loop;
        default:
          //
          // We got an ETB followed by an unexpected character: skip over the
          // ETB and format the remaining buffer.
          //
          ++line;
      } // switch
    }
    // don't emit proto_tws for blank lines
    FPRINTF(
      fout, "%s%s%s", proto_buf, is_blank_line( line ) ? "" : proto_tws, line
    );
  } // while
break_loop:

  FERROR( fwrap );
#endif /* DEBUG_RSWW */
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Parses command-line options, sets-up I/O, sets-up the input buffers, reads
 * the line prototype, and sets-up the pipes.
 *
 * @param argc The number of command-line arguments from main().
 * @param argv The command-line arguments from main().
 */
static void init( int argc, char const *argv[] ) {
  atexit( clean_up );
  init_options( argc, argv, usage );

  CURR = input_buf.line[0];
  NEXT = input_buf.line[1];
  read_prototype();
  PIPE( pipes[ TO_WRAP ] );
  PIPE( pipes[ FROM_WRAP ] );
}

/**
 * Spans the initial part of \a s for the "prototype."  The prototype is
 * defined as \c ^{WS}*{CC}*{WS}* where \c WS is whitespace and \c CC are
 * comment characters.
 *
 * @param s The string to span.
 * @return Returns the length of the prototype.
 */
static size_t proto_span( char const *s ) {
  assert( s );
  size_t ws_len = strspn( s, WS_CHARS );
  size_t const cc_len = strspn( s += ws_len, opt_comment_chars );
  if ( cc_len )
    ws_len += strspn( s + cc_len, WS_CHARS );
  return ws_len + cc_len;
}

/**
 * Compute the actual width of the prototype: tabs are equal to <opt_tab_spaces>
 * spaces minus the number of spaces we're into a tab-stop; all others are
 * equal to 1.
 *
 * @param prototype The prototype string to calculate the width of.
 * @return Returns said width.
 */
static size_t proto_width( char const *prototype ) {
  assert( prototype );

  size_t spaces = 0, width = 0;
  for ( char const *c = prototype; *c; ++c ) {
    if ( *c == '\t' ) {
      width += opt_tab_spaces - spaces;
      spaces = 0;
    } else {
      ++width;
      spaces = (spaces + 1) % opt_tab_spaces;
    }
  } // for
  return width;
}

/**
 * Skips \a n characters or to an end-of-line character, whichever is first.
 *
 * @param s The null-terminated string to use.
 * @param n The maximum number of characters to skip.
 * @return Returns either \a s+n or a pointer to and end-of-line character.
 */
static char const* skip_n( char const *s, size_t n ) {
  for ( ; *s && !is_eol( *s ) && n > 0; ++s, --n )
    /* empty */;
  return s;
}

static char const* str_status( int status ) {
  switch ( status ) {
    case EX_OK        : return "success";
    case EX_USAGE     : return "usage error";
    case EX_NOINPUT   : return "error opening file";
    case EX_OSERR     : return "system error (e.g., can't fork)";
    case EX_CANTCREAT : return "error creating file";
    case EX_IOERR     : return "I/O error";
    case EX_CONFIG    : return "configuration file error";
    default           : return "unknown status";
  } // switch
}

static void usage( void ) {
  PRINT_ERR(
"usage: %s [options]\n"
"       %s -v\n"
"\n"
"options:\n"
"  -a alias   Use alias from configuration file.\n"
"  -b chars   Specify block leading delimiter characters.\n"
"  -c file    Specify the configuration file [default: ~/%s].\n"
"  -C         Suppress reading configuration file.\n"
"  -D chars   Specify comment delimiter characters [default: %s].\n"
"  -e         Treat whitespace after end-of-sentence as new paragraph.\n"
"  -f file    Read from this file [default: stdin].\n"
"  -F string  Specify filename for stdin.\n"
"  -l char    Specify line-endings as input/Unix/Windows [default: input].\n"
"  -o file    Write to this file [default: stdout].\n"
"  -p chars   Specify additional paragraph delimiter characters.\n"
"  -s number  Specify tab-spaces equivalence [default: %d].\n"
"  -T         Treat paragraph first line as title.\n"
"  -u         Format Markdown.\n"
"  -w number  Specify line width [default: %d]\n"
"  -v         Print version an exit.\n"
    , me, me, CONF_FILE_NAME, COMMENT_CHARS_DEFAULT, TAB_SPACES_DEFAULT,
    LINE_WIDTH_DEFAULT
  );
  exit( EX_USAGE );
}

static void wait_for_child_processes( void ) {
#ifndef DEBUG_RSWW
  int wait_status;
  for ( pid_t pid; (pid = wait( &wait_status )) > 0; ) {
    if ( WIFEXITED( wait_status ) ) {
      int const exit_status = WEXITSTATUS( wait_status );
      if ( exit_status != 0 ) {
        PMESSAGE_EXIT( exit_status,
          "child process exited with status %d: %s\n",
          exit_status, str_status( exit_status )
        );
      }
    } else if ( WIFSIGNALED( wait_status ) ) {
      int const signal = WTERMSIG( wait_status );
      PMESSAGE_EXIT( EX_OSERR,
        "child process terminated with signal %d: %s\n",
        signal, strsignal( signal )
      );
    }
  } // for
#endif /* DEBUG_RSWW */
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
