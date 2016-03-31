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
#include "common.h"
#include "options.h"
#include "util.h"

// standard
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>                     /* for PATH_MAX */
#include <signal.h>                     /* for kill() */
#include <stdio.h>
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <sys/wait.h>                   /* for wait() */
#include <unistd.h>                     /* for close(), fork(), ... */

// Closes both ends of pipe P.
#define CLOSE_PIPES(P) \
  BLOCK( close( pipes[P][ STDIN_FILENO ] ); close( pipes[P][ STDOUT_FILENO ] ); )

// Redirects file-descriptor FD to/from pipe P.
#define REDIRECT(FD,P) \
  BLOCK( close( FD ); DUP( pipes[P][FD] ); CLOSE_PIPES( P ); )

// extern variable definitions
FILE               *fin;                // file in
FILE               *fout;               // file out
char const         *me;                 // executable name

// local constant definitions
static size_t const ARG_BUF_SIZE = 25;  // max wrap command-line arg size
static char const   COMMENT_CHARS_DEFAULT[] =
  "#"   //    CMake, Make, Perl, Python, R, Ruby, Shell
  "/*"  //    C, Objective C, C++, C#, D, Go, Java, Rust, Swift
  "+"   // /+ D
  "(:"  //    XQuery
// (*   //    AppleScript, Delphi, ML, Modula-[23], Oberon, OCaml, Pascal
  "{-"  //    Haskell
  "}"   //    Pascal
  "!"   //    Fortran
  "%"   //    Erlang, PostScript, Prolog, TeX
  ";"   //    Assembly, Clojure, Lisp, Scheme
  "<"   // <# PowerShell
  ">"   //    Quoted e-mail
  "|";  // |# Lisp, Racket, Scheme
static char const   WS_CHARS[] = " \t"; // whitespace characters

// option local variable definitions
static char const  *opt_alias;
static char const  *opt_conf_file;      // full path to conf file
static char const  *opt_comment_chars = COMMENT_CHARS_DEFAULT;
static bool         opt_eos_delimit;    // end-of-sentence delimits para's?
static char const  *opt_fin_name;       // file in name
static char const  *opt_lead_para_delims;
static size_t       opt_line_width = LINE_WIDTH_DEFAULT;
static bool         opt_no_conf;        // do not read conf file
static char const  *opt_para_delims;    // additional para delimiter chars
static size_t       opt_tab_spaces = TAB_SPACES_DEFAULT;
static bool         opt_title_line;     // 1st para line is title?

// other local variable definitions
static char         line_buf[ LINE_BUF_SIZE ];
static char         line_buf2[ LINE_BUF_SIZE ];
static char        *cur_buf = line_buf;
static char        *next_buf = line_buf2;
static char         proto_buf[ LINE_BUF_SIZE ]; // characters stripped/prepended
static size_t       proto_len;
//
// Two pipes:
// + pipes[0][0] -> wrap(1)                   [child 2]
//           [1] <- read_source_write_wrap()  [child 1]
// + pipes[1][0] -> read_wrap()               [parent]
//           [1] <- wrap(1)                   [child 2]
//
static int          pipes[2][2];

#define TO_WRAP     0                   /* to refer to pipes[0] */
#define FROM_WRAP   1                   /* to refer to pipes[1] */

// local functions
static void         fork_exec_wrap( pid_t );
static bool         is_block_comment( char const* );
static char const*  is_line_comment( char const* );
static void         process_options( int, char const*[] );
static size_t       proto_span( char const* );
static size_t       proto_width( char const* );
static void         read_prototype( void );
static pid_t        read_source_write_wrap( void );
static void         read_wrap( void );
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
 * Gets a pointer to the first non-whitespace character in \a s.
 *
 * @param s The string to use.
 * @return Returns a pointer to the first non-whitespace character (that may be
 * a pointer to the terminating NULL if there are no other non-whitespace
 * characters).
 */
static inline char const* first_nws( char const *s ) {
  return s + strspn( s, WS_CHARS );
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
  char const *const nws = first_nws( s );
  return *nws && is_comment_char( *nws ) ? nws : NULL;
}

/**
 * Swaps the line buffers.
 */
static inline void swap_bufs() {
  char *const temp = cur_buf;
  cur_buf = next_buf, next_buf = temp;
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *argv[] ) {
  process_options( argc, argv );
  read_prototype();
  PIPE( pipes[ TO_WRAP ] );
  PIPE( pipes[ FROM_WRAP ] );
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
  pid_t const pid = fork();
  if ( pid == -1 ) {                    // we failed, so kill the first child
    kill( read_source_write_wrap_pid, SIGTERM );
    PERROR_EXIT( EX_OSERR );
  }
  if ( pid != 0 )                       // parent process
    return;

  typedef char arg_buf[ ARG_BUF_SIZE ];

  arg_buf arg_opt_alias;
  arg_buf arg_opt_conf_file;
  char    arg_opt_fin_name[ PATH_MAX ];
  arg_buf arg_opt_lead_para_delims;
  arg_buf arg_opt_line_width;
  arg_buf arg_opt_para_delims;
  arg_buf arg_opt_tab_spaces;

  int argc = 0;
  char *argv[13];                       // must be +1 of greatest arg below

#define ARG_SET(ARG)          argv[ argc++ ] = (ARG)
#define ARG_DUP(FMT)          ARG_SET( strdup( FMT ) )

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
  /*  5 */    ARG_DUP(                       "-D"    );
  /*  6 */ IF_ARG_DUP( opt_eos_delimit     , "-e"    );
  /*  7 */ IF_ARG_FMT( opt_fin_name        , "-F%s"  );
  /*  8 */ IF_ARG_FMT( opt_para_delims     , "-p%s"  );
  /*  9 */    ARG_FMT( opt_tab_spaces      , "-s%zu" );
  /* 10 */ IF_ARG_DUP( opt_title_line      , "-T"    );
  /* 11 */    ARG_FMT( opt_line_width      , "-w%zu" );
  /* 12 */ argv[ argc ] = NULL;

  //
  // Read from pipes[TO_WRAP] (read_source_write_wrap() in child 1) and write
  // to pipes[FROM_WRAP] (read_wrap() in parent); exec into wrap(1).
  //
  REDIRECT( STDIN_FILENO, TO_WRAP );
  REDIRECT( STDOUT_FILENO, FROM_WRAP );
  execvp( PACKAGE, argv );
  PERROR_EXIT( EX_OSERR );
}

/**
 * Checks whether the given string is the beginning of a block comment:
 * starts with '/', '*', and contains only non-alpha characters thereafter.
 *
 * @param s The string to check.
 * @return Returns \c true only if \a s is the beginning of a block comment.
 */
static bool is_block_comment( char const *s ) {
  s = first_nws( s );
  if ( s[0] && is_comment_char( s[0] ) ) {
    for ( ++s; *s && *s != '\n' && !isalpha( *s ); ++s )
      ;
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
  if ( !fgets( cur_buf, LINE_BUF_SIZE, fin ) ) {
    CHECK_FERROR( fin );
    exit( EX_OK );
  }

  char const *const cc = is_line_comment( cur_buf );
  if ( cc ) {
    static char comment_chars_buf[4];   // 1-3 chars + NULL
    char *s = comment_chars_buf;
    //
    // From now on, recognize only the comment character found as a comment
    // delimiter.  This handles cases like:
    //
    //    // This is a comment
    //    #define MACRO
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
        // As even a more special-case for Pascal, this handles the case like:
        //
        //    {
        //      This is a block comment.
        //    }
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

  char const *proto = cur_buf;
  if ( is_block_comment( cur_buf ) ) {
    //
    // This handles cases like:
    //
    //    /*
    //     * This is a comment.
    //     */
    //
    // where the first line is the start of a block comment so:
    // + The first line should not be altered.
    // + The second line becomes the prototype.
    //
    if ( !fgets( next_buf, LINE_BUF_SIZE, fin ) )
      CHECK_FERROR( fin );
    proto = next_buf;
  }

  proto_len = proto_span( proto );
  strncpy( proto_buf, proto, proto_len );
  proto_buf[ proto_len ] = '\0';

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

  if ( next_buf[0] ) {
    //
    // For block comments, write the first line verbatim directly to the
    // output.
    //
    FPUTS( cur_buf, fout );
    swap_bufs();
  }

  //
  // As a special-case, if the first line is NOT a comment, then just wrap all
  // lines using the leading whitespace of the first line as a prototype for
  // all subsequent lines, i.e., do NOT ever tell wrap(1) to pass text through
  // verbatim (below).
  //
  bool const prototype_is_comment = is_line_comment( cur_buf );

  while ( cur_buf[0] ) {
    //
    // In order to know when a comment ends, we have to peek at the next line.
    //
    if ( !fgets( next_buf, LINE_BUF_SIZE, fin ) ) {
      CHECK_FERROR( fin );
      next_buf[0] = '\0';
    }

    if ( prototype_is_comment ) {
      if ( next_buf[0] ) {
        if ( !is_line_comment( next_buf ) ) {
          //
          // This handles cases like:
          //
          //    cur_buf   ->  # This is a comment.
          //    next_buf  ->  this_is_code();
          //
          goto verbatim;
        }
      } else {
        if ( is_block_comment( cur_buf ) ) {
          //
          // This handles cases like:
          //
          //                  /*
          //                   * This is a comment.
          //    cur_buf   ->   */
          //    next_buf  ->  [empty]
          //
          goto verbatim;
        }
      }
    } else if ( is_block_comment( cur_buf ) ) {
      //
      // This handles cases like:
      //
      //                 /*
      //    proto_buf -> This is a comment.
      //    cur_buf   -> */
      //
      // where the prototype text isn't a comment.
      //
      goto verbatim;
    }

    proto_len = proto_span( cur_buf );
    if ( !cur_buf[ proto_len ] )        // no non-leading characters
      FPUTC( '\n', fwrap );
    else
      FPUTS( cur_buf + proto_len, fwrap );

    swap_bufs();
  } // while
  exit( EX_OK );

verbatim:
  //
  // We've reached the end of the comment: signal wrap(1) that we're now
  // passing text through verbatim and do so.
  //
  FPRINTF( fwrap, "%c%c%s", ASCII_DLE, ASCII_ETB, cur_buf );
  FPUTS( next_buf, fwrap );
  fcopy( fin, fwrap );
  exit( EX_OK );
}

/**
 * Reads the output of wrap(1) and prepends the leading whitespace and comment
 * characters back to each line.
 */
static void read_wrap( void ) {
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
  //    # foo
  //    #
  //    # bar
  //
  // the prototype initially is "# " (because that's what's before "foo").  If
  // we didn't split off trailing non-whitespace, then when we wrapped the
  // comment above, the second line would become "# " containing a trailing
  // whitespace.
  //
  size_t const tnws_len = proto_len - strrspn( proto_buf, WS_CHARS );
  char proto_tws[ LINE_BUF_SIZE ];      // prototype trailing whitespace, if any
  strcpy( proto_tws, proto_buf + tnws_len );
  proto_buf[ tnws_len ] = '\0';

  while ( fgets( line_buf, LINE_BUF_SIZE, fwrap ) ) {
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
    bool const is_blank_line = strcmp( line, "\n" ) == 0;
    FPRINTF( fout, "%s%s%s", proto_buf, is_blank_line ? "" : proto_tws, line );
  } // while
break_loop:

  CHECK_FERROR( fwrap );
}

///////////////////////////////////////////////////////////////////////////////

static void process_options( int argc, char const *argv[] ) {
  char const *opt_fin = NULL;           // file in name
  char const *opt_fout = NULL;          // file out name
  char const  opts[] = "a:b:c:CD:ef:F:o:p:s:Tvw:";
  bool        print_version = false;

  me = base_name( argv[0] );

  opterr = 1;
  for ( int opt; (opt = getopt( argc, (char**)argv, opts )) != EOF; ) {
    SET_OPTION( opt );
    switch ( opt ) {
      case 'a': opt_alias             = optarg;                     break;
      case 'b': opt_lead_para_delims  = optarg;                     break;
      case 'c': opt_conf_file         = optarg;                     break;
      case 'C': opt_no_conf           = true;                       break;
      case 'D': opt_comment_chars     = optarg;                     break;
      case 'e': opt_eos_delimit       = true;                       break;
      case 'f': opt_fin               = optarg;               // no break;
      case 'F': opt_fin_name          = base_name( optarg );        break;
      case 'o': opt_fout              = optarg;                     break;
      case 'p': opt_para_delims       = optarg;                     break;
      case 's': opt_tab_spaces        = check_atou( optarg );       break;
      case 'T': opt_title_line        = true;                       break;
      case 'v': print_version         = true;                       break;
      case 'w': opt_line_width        = check_atou( optarg );       break;
      default : usage();
    } // switch
  } // for
  argc -= optind, argv += optind;
  if ( argc )
    usage();

  check_mutually_exclusive( "f", "F" );
  check_mutually_exclusive( "v", "acCefFlopsTw" );

  if ( print_version ) {
    PRINT_ERR( "%s\n", PACKAGE_STRING );
    exit( EX_OK );
  }

  if ( opt_fin && !(fin = fopen( opt_fin, "r" )) )
    PMESSAGE_EXIT( EX_NOINPUT, "\"%s\": %s\n", opt_fin, ERROR_STR );
  if ( opt_fout && !(fout = fopen( opt_fout, "w" )) )
    PMESSAGE_EXIT( EX_CANTCREAT, "\"%s\": %s\n", opt_fout, ERROR_STR );

  if ( !fin )
    fin = stdin;
  if ( !fout )
    fout = stdout;
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
"  -o file    Write to this file [default: stdout].\n"
"  -p chars   Specify additional paragraph delimiter characters.\n"
"  -s number  Specify tab-spaces equivalence [default: %d].\n"
"  -T         Treat paragraph first line as title.\n"
"  -w number  Specify line width [default: %d]\n"
"  -v         Print version an exit.\n"
    , me, me, CONF_FILE_NAME, COMMENT_CHARS_DEFAULT, TAB_SPACES_DEFAULT,
    LINE_WIDTH_DEFAULT
  );
  exit( EX_USAGE );
}

static void wait_for_child_processes( void ) {
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
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
