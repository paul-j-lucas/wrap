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

// Close both ends of pipe P.
#define CLOSE_PIPES(P) \
  BLOCK( close( pipes[P][ STDIN_FILENO ] ); close( pipes[P][ STDOUT_FILENO ] ); )

// Redirect file-descriptor FD to/from pipe P.
#define REDIRECT(FD,P) \
  BLOCK( close( FD ); DUP( pipes[P][FD] ); CLOSE_PIPES( P ); )

// extern variable definitions
FILE               *fin;                // file in
FILE               *fout;               // file out
char const         *me;                 // executable name

// local constant definitions
static size_t const ARG_BUF_SIZE = 25;  // max wrap command-line arg size
static char const   COMMENT_DELIMS_DEFAULT[] =
  "!"   // HTML and XML comments
  "#"   // CMake, Make, Perl, Python, Ruby, and Shell comments
  "%"   // PostScript comments
  "/*"  // C, C++, and Java comments
  ":"   // XQuery comments
  ";"   // Assember and Lisp comments
  ">";  // Forward mail indicator
static char const   WS_CHARS[] = " \t"; // whitespace characters

// option local variable definitions
static char const  *opt_alias;
static char const  *opt_conf_file;      // full path to conf file
static char const  *opt_comment_delims = COMMENT_DELIMS_DEFAULT;
static bool         opt_eos_delimit;    // end-of-sentence delimits para's?
static char const  *opt_fin_name;       // file in name
static size_t       opt_line_width = LINE_WIDTH_DEFAULT;
static bool         opt_no_conf;        // do not read conf file
static char const  *opt_para_delims;    // additional para delimiter chars
static size_t       opt_tab_spaces = TAB_SPACES_DEFAULT;
static bool         opt_title_line;     // 1st para line is title?

// other local variable definitions
static char         line_buf[ LINE_BUF_SIZE ];
static char         leader[ LINE_BUF_SIZE ]; // characters stripped/prepended
static size_t       leader_len;
//
// Two pipes:
// + pipes[0][0] -> wrap(1)                   [child 2]
//           [1] <- read_source_write_wrap()  [child 1]
// + pipes[1][0] -> read_wrap()               [parent]
//           [1] <- wrap(1)                   [child 2]
//
static int          pipes[2][2];
static bool         set_comment_delims; // was -D given on the command-line?

#define TO_WRAP     0                   /* to refer to pipes[0] */
#define FROM_WRAP   1                   /* to refer to pipes[1] */

// local functions
static void         fork_exec_wrap( pid_t );
static size_t       leader_span( char const* );
static size_t       leader_width( char const* );
static void         parse_leader( char const*, char*, size_t* );
static void         process_options( int, char const*[] );
static pid_t        read_source_write_wrap( void );
static void         read_wrap( void );
static char const*  str_status( int );
static void         usage( void );
static void         wait_for_child_processes( void );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Gets whether the first non-whitespace character in \a buf is a comment
 * character.
 *
 * @param buf The buffer to check.
 * @return Returns \c true only if the first non-whitespace character in \a buf
 * is a comment character.
 */
static inline bool first_nws_is_comment( char const *buf ) {
  char const *const first_nws = &buf[ strspn( buf, WS_CHARS ) ];
  if ( !set_comment_delims && strncmp( first_nws, "*/", 2 ) == 0 ) {
    //
    // As a special-case, treat */ (the end of a C-style comment) as not being
    // a comment so we'll tell wrap(1) to pass it through verbatim.
    //
    return false;
  }
  return strchr( opt_comment_delims, *first_nws );
}

///////////////////////////////////////////////////////////////////////////////

int main( int argc, char const *argv[] ) {
  process_options( argc, argv );

  //
  // Read the first line of input to obtain a sequence of leading characters to
  // be the prototype for all lines.
  //
  if ( !fgets( line_buf, sizeof( line_buf ), fin ) ) {
    CHECK_FERROR( fin );
    exit( EX_OK );
  }
  parse_leader( line_buf, leader, &leader_len );

  //
  // Open the pipes, fork the child processes, and go.
  //
  PIPE( pipes[ TO_WRAP ] );
  PIPE( pipes[ FROM_WRAP ] );
  fork_exec_wrap( read_source_write_wrap() );
  read_wrap();

  wait_for_child_processes();
  exit( EX_OK );
}

///////////////////////////////////////////////////////////////////////////////

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
  arg_buf arg_opt_line_width;
  arg_buf arg_opt_para_delims;
  arg_buf arg_opt_tab_spaces;

  int argc = 0;
  char *argv[12];                       // must be +1 of greatest arg below

#define ARG_SET(ARG)          argv[ argc++ ] = (ARG)
#define ARG_DUP(FMT)          ARG_SET( strdup( FMT ) )

#define ARG_SPRINTF(ARG,FMT) \
  snprintf( arg_##ARG, sizeof arg_##ARG, (FMT), (ARG) )
#define ARG_FMT(ARG,FMT) \
  BLOCK( ARG_SPRINTF( ARG, (FMT) ); ARG_SET( arg_##ARG ); )

#define IF_ARG_DUP(ARG,FMT)   if ( ARG ) ARG_DUP( FMT )
#define IF_ARG_FMT(ARG,FMT)   if ( ARG ) ARG_FMT( ARG, FMT )

  // Quoting string arguments is unnecessary since no shell is involved.

  /*  0 */    ARG_DUP(                  PACKAGE );
  /*  1 */ IF_ARG_FMT( opt_alias      , "-a%s"  );
  /*  2 */ IF_ARG_FMT( opt_conf_file  , "-c%s"  );
  /*  3 */ IF_ARG_DUP( opt_no_conf    , "-C"    );
  /*  4 */    ARG_DUP(                  "-D"    );
  /*  5 */ IF_ARG_DUP( opt_eos_delimit, "-e"    );
  /*  6 */ IF_ARG_FMT( opt_fin_name   , "-F%s"  );
  /*  7 */ IF_ARG_FMT( opt_para_delims, "-p%s"  );
  /*  8 */    ARG_FMT( opt_tab_spaces , "-s%zu" );
  /*  9 */ IF_ARG_DUP( opt_title_line , "-T"    );
  /* 10 */    ARG_FMT( opt_line_width , "-w%zu" );
  /* 11 */ argv[ argc ] = NULL;

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

  // write first line to wrap
  FPUTS( line_buf + leader_len, fwrap );

  //
  // As a special-case, if the first line is NOT a comment, then just wrap all
  // lines using the leading whitespace of the first line as a prototype for
  // all subsequent lines, i.e., do NOT ever tell wrap(1) to pass text through
  // verbatim (below).
  //
  bool const first_line_is_comment = first_nws_is_comment( line_buf );

  while ( fgets( line_buf, sizeof( line_buf ), fin ) ) {
    if ( first_line_is_comment && !first_nws_is_comment( line_buf ) ) {
      //
      // The line's first non-whitespace character isn't a comment character:
      // we've reached the end of the comment. Signal wrap that we're now
      // passing text through verbatim.
      //
      FPRINTF( fwrap, "%c%c%s", ASCII_DLE, ASCII_ETB, line_buf );
      fcopy( fin, fwrap );
      exit( EX_OK );
    }
    leader_len = leader_span( line_buf );
    if ( !line_buf[ leader_len ] )      // no non-leading characters
      FPUTC( '\n', fwrap );
    else
      FPUTS( line_buf + leader_len, fwrap );
  } // while
  CHECK_FERROR( fin );
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
  // Split off the trailing non-whitespace (tnws) from the leader so that if we
  // read a comment that's empty except for the leader, we won't emit trailing
  // whitespace. For example, given:
  //
  //    # foo
  //    #
  //    # bar
  //
  // the leader initially is "# " (because that's what's before "foo").  If we
  // didn't split off trailing non-whitespace, then when we wrapped the comment
  // above, the second line would become "# " containing a trailing whitespace.
  //
  size_t const tnws_len = leader_len - strrspn( leader, WS_CHARS );
  char leader_tws[ LINE_BUF_SIZE ];     // leader trailing whitespace, if any
  strcpy( leader_tws, leader + tnws_len );
  leader[ tnws_len ] = '\0';

  while ( fgets( line_buf, sizeof( line_buf ), fwrap ) ) {
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
    // don't emit leader_tws for blank lines
    bool const is_blank_line = strcmp( line, "\n" ) == 0;
    FPRINTF( fout, "%s%s%s", leader, is_blank_line ? "" : leader_tws, line );
  } // while
break_loop:

  CHECK_FERROR( fwrap );
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Spans the initial part of \a s for the "leader."  The leader is defined as
 * \c ^{WS}*{CC}+{WS}* where \c WS is whitespace and \c CC are comment
 * characters.
 *
 * @param s The string to span.
 * @return Returns the length of the leader.
 */
static size_t leader_span( char const *s ) {
  assert( s );
  size_t ws_len = strspn( s, WS_CHARS );
  size_t const cc_len = strspn( s += ws_len, opt_comment_delims );
  if ( cc_len )
    ws_len += strspn( s + cc_len, WS_CHARS );
  return ws_len + cc_len;
}

/**
 * Compute the actual width of the leader: tabs are equal to <opt_tab_spaces>
 * spaces minus the number of spaces we're into a tab-stop; all others are
 * equal to 1.
 *
 * @param leader The leader string to calculate the width of.
 * @return Returns said width.
 */
static size_t leader_width( char const *leader ) {
  assert( leader );

  size_t spaces = 0, width = 0;
  for ( char const *c = leader; *c; ++c ) {
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

static void parse_leader( char const *buf, char *leader, size_t *leader_len ) {
  assert( buf );
  assert( leader );
  assert( leader_len );

  *leader_len = leader_span( buf );
  strncpy( leader, buf, *leader_len );
  leader[ *leader_len ] = '\0';

  opt_line_width -= leader_width( leader );
  if ( opt_line_width < LINE_WIDTH_MINIMUM )
    PMESSAGE_EXIT( EX_USAGE,
      "line-width (%zu) is too small (<%d)\n",
      opt_line_width, LINE_WIDTH_MINIMUM
    );
}

static void process_options( int argc, char const *argv[] ) {
  char const *opt_fin = NULL;           // file in name
  char const *opt_fout = NULL;          // file out name
  char const  opts[] = "a:c:CD:ef:F:l:o:p:s:Tvw:";
  bool        print_version = false;

  me = base_name( argv[0] );

  opterr = 1;
  for ( int opt; (opt = getopt( argc, (char**)argv, opts )) != EOF; ) {
    SET_OPTION( opt );
    switch ( opt ) {
      case 'a': opt_alias           = optarg;                       break;
      case 'c': opt_conf_file       = optarg;                       break;
      case 'C': opt_no_conf         = true;                         break;
      case 'D': opt_comment_delims  = optarg;
                set_comment_delims  = true;                         break;
      case 'e': opt_eos_delimit     = true;                         break;
      case 'f': opt_fin             = optarg;                 // no break;
      case 'F': opt_fin_name        = base_name( optarg );          break;
      case 'o': opt_fout            = optarg;                       break;
      case 'p': opt_para_delims     = optarg;                       break;
      case 's': opt_tab_spaces      = check_atou( optarg );         break;
      case 'T': opt_title_line      = true;                         break;
      case 'v': print_version       = true;                         break;
      case 'l': // deprecated: now synonym for -w
      case 'w': opt_line_width      = check_atou( optarg );         break;
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
"       -a alias   Use alias from configuration file.\n"
"       -c file    Specify the configuration file [default: ~/%s].\n"
"       -C         Supress reading configuration file.\n"
"       -D chars   Specify comment delimiter characters [default: %s].\n"
"       -e         Treat white-space after end-of-sentence as new paragraph.\n"
"       -f file    Read from this file [default: stdin].\n"
"       -F string  Specify filename for stdin.\n"
"       -o file    Write to this file [default: stdout].\n"
"       -p chars   Specify additional paragraph delimiter characters.\n"
"       -s number  Specify tab-spaces equivalence [default: %d].\n"
"       -T         Treat paragraph first line as title.\n"
"       -w number  Specify line width [default: %d]\n"
"       -v         Print version an exit.\n"
    , me, me, CONF_FILE_NAME, COMMENT_DELIMS_DEFAULT, TAB_SPACES_DEFAULT,
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
