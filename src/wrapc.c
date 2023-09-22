/*
**      wrapc -- comment reformatter
**      src/wrapc.c
**
**      Copyright (C) 1996-2023  Paul J. Lucas
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
 * Implements **wrapc**(1).
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "alias.h"
#include "cc_map.h"
#include "common.h"
#include "doxygen.h"
#include "markdown.h"
#include "options.h"
#include "pattern.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <limits.h>                     /* for PATH_MAX */
#include <signal.h>                     /* for kill() */
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <sys/wait.h>                   /* for wait() */
#include <sysexits.h>
#include <unistd.h>                     /* for close(), fork(), ... */

/*
 * Uncomment the line below to debug read_source_write_wrap() (RSWW) by not
 * forking or exec'ing and just writing to stdout directly.
 */
//#define DEBUG_RSWW

#ifdef DEBUG_RSWW
# undef PIPE
# define PIPE(P) NO_OP
#endif /* DEBUG_RSWW */

/// @endcond

/**
 * @defgroup wrapc-group Wrapc
 * Implements **wrapc**(1), a filter for reformatting source code comments (or
 * quoted e-mail) by wrapping and filling lines to a given line-width.
 * @{
 */

/// Maximum **wrap**(1) command-line argument size.
#define ARG_BUF_SIZE              25

/**
 * Aligns end-of-line comments to a particular column.
 *
 * @param input_buf The input buffer to use.  It must contain the first line of
 * text read.
 */
void align_eol_comments( char input_buf[const] );

///////////////////////////////////////////////////////////////////////////////

/**
 * Types of comment delimiters.
 */
enum delim {
  /// To-end-of-line comment delimiter, e.g., `#` or `//`.
  DELIM_EOL,

  /// Single character comment delimiter, e.g., `{` (Pascal).
  DELIM_SINGLE,

  /// Double character comment delimiter, e.g., `/*` (but not `//`).
  DELIM_DOUBLE,
};
typedef enum delim delim_t;

/**
 * Redirects \a FD file-descriptor to/from pipe \a P.
 *
 * @param FD The file descriptor to redirect.
 * @param P The pipe index, 0 or 1, to redirect to.
 */
#define REDIRECT(FD,P) \
  BLOCK( close( FD ); DUP( pipes[P][FD] ); close_pipe( pipes[P] ); )

/**
 * Contains the current and next lines of input so the next line can be peeked
 * at to determine how to proceed.
 */
struct dual_line {
  line_buf_t  dl_line[2];               ///< Two lines.
  char       *dl_curr;                  ///< Pointer to current line.
  char       *dl_next;                  ///< Pointer to next line.
};
typedef struct dual_line dual_line_t;

// extern variable definitions
char const         *me;                 // executable name

// local variable definitions
static char         close_cc[2];        ///< Closing comment delimiter char(s).
static delim_t      delim;              ///< Comment delimiter type.
static dual_line_t  input_buf;          ///< Input buffer.
static line_buf_t   prefix_buf;         ///< Characters stripped/prepended.
static size_t       prefix_len0;        ///< Length of prefix_buf
static line_buf_t   suffix_buf;         ///< Characters stripped/appended.
static size_t       suffix_len;         ///< Length of suffix_buf.
/**
 * Two pipes:
 *
 *      pipes[0][1] <- read_source_write_wrap()  [child 1]
 *              [0] -> wrap(1)                   [child 2]
 *
 *      pipes[1][1] <- wrap(1)                   [child 2]
 *              [0] -> read_wrap_write_stdout()  [parent]
 *
 * @remarks
 * @parblock
 * **wrapc**(1) forks twice:
 *
 *  1. Child 1, via read_source_write_wrap(), reads the original source from
 *     stdin, strips leading whitespace and comment delimiter characters, and
 *     writes to `pipe[0][1]` connected to child 2.
 *
 *  2. Child 2 exec's itself into **wrap**(1), reads text from stdin via
 *     `pipes[0][0]` connected to child 1, reformats it, and writes to stdout
 *     via `pipe[1][1]` connected to the parent.
 *
 * The parent process reads from `pipes[1][0]` connected to child 2, prepends
 * the original comment delimiter characters, and writes to stdout via
 * read_wrap_write_stdout().
 * @endparblock
 */
static int          pipes[2][2];

#define CURR        input_buf.dl_curr   /**< Shorthand for current line. */
#define NEXT        input_buf.dl_next   /**< Shorthand for next line. */

#define TO_WRAP     0                   /**< To refer to \ref pipes[0]. */
#define FROM_WRAP   1                   /**< To refer to \ref pipes[1]. */

// local functions
static void         adjust_comment_width( char* );
static void         chop_suffix( char* );
static void         fork_exec_wrap( pid_t );
static void         init( int, char const*[] );

NODISCARD
static bool         is_block_comment( char const* );

NODISCARD
static char const*  is_line_comment( char const* );

NODISCARD
static char const*  is_terminated_comment( char* );

NODISCARD
static size_t       prefix_span( char const* );

static void         read_prototype( void );

NODISCARD
static pid_t        read_source_write_wrap( void );

static void         read_wrap_write_stdout( void );
static void         set_prefix( char const*, size_t );

NODISCARD
static char*        skip_c( char*, char );

NODISCARD
static char*        skip_n( char*, size_t );

NODISCARD
static size_t       strlen_no_eol( char const* );

NODISCARD
static char const*  str_status( int );

NODISCARD
static size_t       str_width( char const* );

_Noreturn
static void         usage( int );
static void         wait_for_child_processes( void );

NODISCARD
static bool         wrap_dox_line( char const*, FILE* );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Closes both ends of a pipe.
 *
 * @param pipe The pipe to close.
 */
static inline void close_pipe( int pipe[const static 2] ) {
  PJL_IGNORE_RV( close( pipe[ STDIN_FILENO ] ) );
  PJL_IGNORE_RV( close( pipe[ STDOUT_FILENO ] ) );
}

/**
 * Gets whether \a c is a comment delimiter character.
 *
 * @param c The character to check.
 * @return Returns `true` only if it is.
 */
NODISCARD
static inline bool is_comment_char( char c ) {
  return c != '\0' && strchr( opt_comment_chars, c ) != NULL;
}

/**
 * Gets whether the first non-whitespace character in \a s is a comment
 * character.
 *
 * @param s The string to check.
 * @return Returns a pointer to the first non-whitespace character in \a s only
 * if it's a comment delimiter character; NULL otherwise.
 */
NODISCARD
static inline char const* is_line_comment( char const *s ) {
  return is_comment_char( SKIP_CHARS( s, WS_ST )[0] ) ? s : NULL;
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

/**
 * The main entry point.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns 0 on success, non-zero on failure.
 */
int main( int argc, char const *argv[] ) {
  wait_for_debugger_attach( "WRAPC_DEBUG" );
  init( argc, argv );
  if ( opt_align_column > 0 ) {
    align_eol_comments( CURR );
  } else {
    read_prototype();
    PIPE( pipes[ TO_WRAP ] );
    PIPE( pipes[ FROM_WRAP ] );
    fork_exec_wrap( read_source_write_wrap() );
    read_wrap_write_stdout();
    wait_for_child_processes();
  }
  exit( EX_OK );
}

////////// IPC functions //////////////////////////////////////////////////////

/**
 * Forks and execs into **wrap**(1).
 *
 * @param read_source_write_wrap_pid The process ID of read_source_write_wrap()
 * (in case we need to kill it).
 */
static void fork_exec_wrap( pid_t read_source_write_wrap_pid ) {
#ifdef DEBUG_RSWW
  (void)ARG_BUF_SIZE;
  (void)pipes;
  (void)read_source_write_wrap_pid;
#else
  pid_t const pid = fork();
  if ( unlikely( pid == -1 ) ) {        // we failed, so kill the first child
    kill( read_source_write_wrap_pid, SIGTERM );
    perror_exit( EX_OSERR );
  }
  if ( pid != 0 )                       // parent process
    return;

  typedef char arg_buf_t[ ARG_BUF_SIZE ];
  typedef char path_buf_t[ PATH_MAX ];

  arg_buf_t   arg_opt_alias;
  arg_buf_t   arg_opt_block_regex;
  arg_buf_t   arg_opt_conf_file;
  arg_buf_t   arg_opt_eol;
  arg_buf_t   arg_opt_eos_spaces;
  path_buf_t  arg_opt_fin_name;
  arg_buf_t   arg_opt_line_width;
  arg_buf_t   arg_opt_para_delims;
  arg_buf_t   arg_opt_tab_spaces;

  size_t argc = 0;
  char *argv[17];                       // must be +1 of greatest arg below

#define ARG_CHECK                 assert( argc < ARRAY_SIZE( argv ) )
#define ARG_SET(ARG)              BLOCK( ARG_CHECK; argv[ argc++ ] = (ARG); )
#define ARG_DUP(FMT)              ARG_SET( check_strdup( FMT ) )
#define ARG_END                   ARG_SET( NULL )

#define ARG_FMT(ARG,FMT) BLOCK( \
  snprintf( arg_##ARG, sizeof arg_##ARG, (FMT), (ARG) ); ARG_SET( arg_##ARG ); )

// These intentionally do NOT use BLOCK().
#define IF_ARG_DUP(ARG,FMT)       if ( ARG ) ARG_DUP( FMT )
#define IF_ARG_FMT(ARG,FMT)       if ( ARG ) ARG_FMT( ARG, FMT )

  // Quoting string arguments is unnecessary since no shell is involved.

  /*  0 */    ARG_DUP(                  PACKAGE );
  /*  1 */ IF_ARG_FMT( opt_alias      , "-a%s"  );
  /*  2 */ IF_ARG_FMT( opt_block_regex, "-b%s"  );
  /*  3 */ IF_ARG_FMT( opt_conf_file  , "-c%s"  );
  /*  4 */ IF_ARG_DUP( opt_no_conf    , "-C"    );
  /*  5 */ IF_ARG_DUP( opt_eos_delimit, "-e"    );
  /*  6 */ IF_ARG_FMT( opt_eos_spaces , "-E%zu" );
  /*  7 */ IF_ARG_FMT( opt_fin_name   , "-F%s"  );
  /*  8 */    ARG_FMT( opt_eol        , "-l%c"  );
  /*  9 */ IF_ARG_FMT( opt_para_delims, "-p%s"  );
  /* 10 */ IF_ARG_DUP( opt_markdown   , "-u"    );
         else ARG_FMT( opt_tab_spaces , "-s%zu" );
  /* 11 */ IF_ARG_DUP( opt_title_line , "-T"    );
  /* 12 */    ARG_FMT( opt_line_width , "-w%zu" );
  /* 13 */ IF_ARG_DUP( opt_doxygen    , "-x"    );
  /* 14 */ IF_ARG_DUP( opt_no_hyphen  , "-y"    );
  /* 15 */    ARG_DUP(                  "-Z"    );
  /* 16 */    ARG_END;

  //
  // Read from pipes[TO_WRAP] (read_source_write_wrap() in child 1) and write
  // to pipes[FROM_WRAP] (read_wrap_write_stdout() in parent); exec into
  // wrap(1).
  //
  REDIRECT( STDIN_FILENO, TO_WRAP );
  REDIRECT( STDOUT_FILENO, FROM_WRAP );
  execvp( PACKAGE, argv );              // should not return
  perror_exit( EX_OSERR );
#endif /* DEBUG_RSWW */
}

/**
 * Reads the source text to be wrapped and writes it, with the leading
 * whitespace and comment delimiter characters stripped from each line, to
 * **wrap**(1).
 *
 * @return Returns the child's process ID.
 *
 * @sa read_wrap_write_stdout()
 */
NODISCARD
static pid_t read_source_write_wrap( void ) {
#ifndef DEBUG_RSWW
  pid_t const pid = fork();
  PERROR_EXIT_IF( pid == -1, EX_OSERR );
  if ( pid != 0 )                       // parent process
    return pid;
  //
  // We don't use these here.
  //
  close_pipe( pipes[ FROM_WRAP ] );
  PJL_IGNORE_RV( close( pipes[ TO_WRAP ][ STDIN_FILENO ] ) );
  //
  // Read from stdin and write to pipes[TO_WRAP] (wrap).
  //
  FILE *const fwrap = fdopen( pipes[ TO_WRAP ][ STDOUT_FILENO ], "w" );
  if ( unlikely( fwrap == NULL ) ) {
    fatal_error( EX_OSERR,
      "child can't open pipe for writing: %s\n", STRERROR()
    );
  }
  wait_for_debugger_attach( "WRAPC_DEBUG_RSRW" );
#else
  FILE *const fwrap = stdout;
#endif /* DEBUG_RSWW */

  if ( NEXT[0] != '\0' ) {
    //
    // For block comments, write the first line directly to the output.
    //
    adjust_comment_width( CURR );
    PUTS( CURR );
    swap_line_bufs();
  }

  //
  // As a special case, if the first line is NOT a comment, then just wrap all
  // lines using the leading whitespace of the first line as a prototype for
  // all subsequent lines, i.e., do NOT ever tell wrap(1) to pass text through
  // verbatim (below).
  //
  bool const proto_is_comment = is_line_comment( CURR ) != NULL;

  for ( ; CURR[0] != '\0'; swap_line_bufs() ) {
    //
    // In order to know when a comment ends, we have to peek at the next line.
    //
    PJL_IGNORE_RV( check_readline( NEXT, stdin ) );

    if ( proto_is_comment && is_line_comment( CURR ) == NULL ) {
      //
      // This handles cases like:
      //
      //      proto     ->  # This is a comment.
      //      curr_buf  ->  not_a_comment();
      //
      goto verbatim;
    }

    if ( !(proto_is_comment && is_line_comment( NEXT ) != NULL) &&
         is_block_comment( CURR ) ) {
      //
      // This handles cases like:
      //
      //                    /*
      //      proto     ->  This is a comment.
      //      curr_buf  ->  */
      //
      // or:
      //                    /*
      //                     * This is a comment.
      //      curr_buf  ->   */
      //      next_buf  ->  [empty]
      //
      adjust_comment_width( CURR );
      goto verbatim;
    }

    size_t prefix_len = prefix_span( CURR );
    if ( opt_doxygen || opt_markdown ) {
      if ( prefix_len > prefix_len0 ) {
        //
        // We can't strip all whitespace after the comment delimiter characters
        // because:
        //
        // 1. Doxygen needs the whitespace when doing preformatted text.
        // 2. Markdown relies on indentation for state changes.
        //
        // Hence we strip only the length of the initial prototype -- but only
        // if it's less.
        //
        prefix_len = prefix_len0;
      }
      else if ( prefix_len < prefix_len0 && !is_eol( CURR[ prefix_len ] ) ) {
        //
        // The leading comment delimiter characters and/or whitespace length
        // has decreased.  This can happen in a case like:
        //
        //      *  + This is a list item.
        //      *
        //      * Not part of the list item.
        //
        // where the list item was indented 2 spaces after the * but the
        // regular text was indented only 1 space.  If this check were not
        // done, then the "Not" text would end up also being indented 2 spaces.
        //
        // We therefore have to increase opt_line_width by the delta and also
        // notify both wrap(1) and the other wrapc(1) processes of the changes.
        //
        opt_line_width += prefix_len0 - prefix_len;
        set_prefix( CURR, prefix_len );
        WIPC_SENDF(
          fwrap, WIPC_CODE_NEW_LEADER, "%zu" WIPC_PARAM_SEP "%s\n",
          opt_line_width, prefix_buf
        );
      }
    }

    // Skip over the prefix and chop off the suffix.
    char *const line = skip_n( CURR, prefix_len );
    if ( suffix_buf[0] != '\0' )
      chop_suffix( line );

    if ( opt_doxygen && wrap_dox_line( line, fwrap ) )
      continue;

    FPUTS( line, fwrap );
  } // for
  exit( EX_OK );

verbatim:
  //
  // We've reached the end of the comment: signal wrap(1) that we're now
  // ending wrapping, write any remaining lines, then just copy text through
  // verbatim.
  //
  WIPC_SEND( fwrap, WIPC_CODE_WRAP_END );
  FPUTS( CURR, fwrap );
  FPUTS( NEXT, fwrap );
  fcopy( stdin, fwrap );
  exit( EX_OK );
}

/**
 * Reads the output of **wrap**(1), prepends the leading whitespace and comment
 * characters back to each line, and writes to stdout.
 *
 * @sa read_source_write_wrap()
 */
static void read_wrap_write_stdout( void ) {
#ifndef DEBUG_RSWW
  //
  // We don't use these here.
  //
  close_pipe( pipes[ TO_WRAP ] );
  PJL_IGNORE_RV( close( pipes[ FROM_WRAP ][ STDOUT_FILENO ] ) );
  //
  // Read from pipes[FROM_WRAP] (wrap) and write to stdout.
  //
  FILE *const fwrap = fdopen( pipes[ FROM_WRAP ][ STDIN_FILENO ], "r" );
  if ( unlikely( fwrap == NULL ) ) {
    fatal_error( EX_OSERR,
      "parent can't open pipe for reading: %s\n", STRERROR()
    );
  }

  wait_for_debugger_attach( "WRAPC_DEBUG_RW" );

  //
  // Split off the trailing whitespace (tws) from the prototype so that if we
  // read a comment line that's empty except for the prototype, we won't emit
  // trailing whitespace. For example, given:
  //
  //      # foo
  //      #
  //      # bar
  //
  // the prototype initially is "# " (because that's what's before "foo").  If
  // we didn't split off trailing whitespace, then when we wrapped the comment
  // above, the second line would become "# " containing a trailing whitespace.
  //
  line_buf_t proto_tws;                 // prototype trailing whitespace, if any
  split_tws( prefix_buf, prefix_len0, proto_tws );

  line_buf_t line_buf;

  for (;;) {
    size_t line_size = sizeof line_buf;
    if ( unlikely( fgetsz( line_buf, &line_size, fwrap ) == NULL ) )
      break;
    line_size = chop_eol( line_buf, line_size );
    char *line = line_buf;

    if ( line[0] == WIPC_CODE_HELLO ) {
      switch ( STATIC_CAST( wipc_code_t, line[1] ) ) {
        case WIPC_CODE_HELLO:           // shouldn't happen
          break;

        case WIPC_CODE_NEW_LEADER: {
          //
          // We've been told by child 1 (read_source_write_wrap(), via child 2,
          // wrap) that the leading comment delimiter characters and/or
          // whitespace has changed: adjust opt_line_width and prefix_buf.
          //
          char *sep;
          opt_line_width = strtoul( line + 2, &sep, 10 );
          prefix_len0 = strcpy_len( prefix_buf, sep + 1 );
          split_tws( prefix_buf, prefix_len0, proto_tws );
          continue;
        }

        case WIPC_CODE_DELIMIT_PARAGRAPH:
        case WIPC_CODE_PREFORMATTED_BEGIN:
        case WIPC_CODE_PREFORMATTED_END:
          //
          // We only have to "eat" these and do nothing else.
          //
          continue;

        case WIPC_CODE_WRAP_END:
          //
          // We've been told by child 1 (read_source_write_wrap(), via child 2,
          // wrap) that we've reached the end of the comment: dump any
          // remaining buffer and pass text through verbatim.
          //
          fcopy( fwrap, stdout );
          goto done;
      } // switch

      //
      // We got a HELLO followed by an unknown WIPC code: skip over the HELLO
      // and format the remaining buffer.
      //
      ++line;
      --line_size;
    }

    if ( suffix_buf[0] != '\0' ) {
      //
      // Pad the width with spaces in order to append the terminating comment
      // character(s) back.
      //
      while ( line_size < opt_line_width )
        line[ line_size++ ] = ' ';
      line[ line_size ] = '\0';
    }

    // don't emit proto_tws for blank lines
    PRINTF(
      "%s%s%s%s%s",
      prefix_buf, is_blank_line( line ) ? "" : proto_tws, line, suffix_buf,
      eol()
    );
  } // for

done:
  FERROR( fwrap );
#endif /* DEBUG_RSWW */
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Adjusts the width of a comment line so that it either does not exceed or is
 * lengthened to meet the line width.
 *
 * @param s The newline- and null-terminated string to adjust.
 */
static void adjust_comment_width( char *s ) {
  assert( s != NULL );
  size_t const delim_len = suffix_buf[0] ? suffix_len : 1 + !!close_cc[1];
  size_t const width = opt_line_width + prefix_len0 + suffix_len;
  size_t s_len = strlen_no_eol( s );

  if ( s_len > width ) {
    //
    // Shorten the comment line by sliding the closing comment delimiter
    // character(s) to the left.
    //
    memmove(
      s + width - delim_len,
      suffix_buf[0] ? suffix_buf : s + s_len - delim_len,
      delim_len
    );
    strcpy( s + width, eol() );
  }
  else if ( suffix_buf[0] != '\0' && s_len < width ) {
    //
    // If we're doing terminated comments, lengthen the comment line by
    // "inserting" a multiple of the first comment delimiter character.
    //
    switch ( delim ) {
      case DELIM_EOL:
        for ( ; s_len < width; ++s_len )
          s[ s_len ] = close_cc[0];
        break;
      case DELIM_SINGLE:
        for ( --s_len; s_len < width; ++s_len )
          s[ s_len ] = ' ';
        s[ s_len - 1 ] = close_cc[0];
        break;
      case DELIM_DOUBLE:
        for ( --s_len; s_len < width; ++s_len )
          s[ s_len ] = close_cc[0];
        s[ s_len - 1 ] = close_cc[1];
        break;
    } // switch
    strcpy( s + width, eol() );
  }
}

/**
 * Chops off the termininating comment delimiter character(s), if any.
 *
 * @param s The newline- and null-terminated string to chop.
 */
static void chop_suffix( char *s ) {
  assert( s != NULL );
  assert( suffix_buf[0] != '\0' );
  char *cc = s;

  for ( ; (cc = strchr( cc, suffix_buf[0] )) != NULL; ++cc ) {
    switch ( delim ) {
      case DELIM_EOL: {
        //
        // We've found the terminator character, but it may be the first in a
        // sequence of them, e.g.:
        //
        //      ### This is a comment. ###
        //                             ^
        // We therefore first have to skip over all of them.  Second, we have
        // to check that the terminator character isn't in the middle of a
        // line, e.g.:
        //
        //      # This is a comment that has a '#' in it. #
        //
        // We always want to chop off the very last terminator character(s) so
        // we then check to see that the remaining characters on the line, if
        // any, are only whitespace.  If not, then it's not the last terminator
        // character on the line.
        //
        char *const after_cc = skip_c( cc, suffix_buf[0] );
        if ( after_cc[ strspn( after_cc, WS_STRN ) ] == '\0' )
          goto done;
        cc = after_cc - 1;
        break;
      }
      case DELIM_SINGLE:
        goto done;
      case DELIM_DOUBLE:
        if ( strncmp( cc, suffix_buf, suffix_len ) == 0 )
          goto done;
        break;
    } // switch
  } // for

done:
  if ( cc != NULL )
    *cc = '\0';
}

/**
 * Parses command-line options, sets-up I/O, sets-up the input buffers, sets
 * the end-of-lines.
 *
 * @param argc The number of command-line arguments from main().
 * @param argv The command-line arguments from main().
 */
static void init( int argc, char const *argv[] ) {
  ASSERT_RUN_ONCE();
  ATEXIT( common_cleanup );

  options_init( argc, argv, usage );
  opt_comment_chars = cc_map_compile( opt_comment_chars );

  CURR = input_buf.dl_line[0];
  NEXT = input_buf.dl_line[1];

  size_t const size = check_readline( CURR, stdin );
  if ( size == 0 )
    exit( EX_OK );

  if ( opt_eol == EOL_INPUT && is_windows_eol( CURR, size ) ) {
    //
    // Retroactively set opt_eol because we pass it to wrap(1).
    //
    opt_eol = EOL_WINDOWS;
  }
}

/**
 * Checks whether the given string is the beginning of a block comment: starts
 * with a comment delimiter character and contains only non-alpha characters
 * thereafter.
 *
 * @param s The string to check.
 * @return Returns `true` only if \a s is the beginning of a block comment.
 */
NODISCARD
static bool is_block_comment( char const *s ) {
  assert( s != NULL );
  if ( (s = is_line_comment( s )) != NULL ) {
    for ( ++s; *s && *s != '\n' && !isalpha( *s ); ++s )
      /* empty */;
    return *s == '\n';
  }
  return false;
}

/**
 * Checks whether the given string is a terminated comment, that is a string
 * that both begins and ends with comment delimiters, e.g.:
 *
 *      (* This is a terminated comment. *)
 *      # This is another terminated comment. #
 *
 * Note that the second example is merely "visually" terminated and not
 * lexically terminated since languages that use a single \c '#' for comments
 * use them to mean from the \c '#' until the end of the line.
 *
 * @param s The newline- and null-terminated string to check.
 * @return Returns a pointer to the start of the terminating comment delimiter
 * or null if it's not a terminated comment.
 */
NODISCARD
static char const* is_terminated_comment( char *s ) {
  assert( s != NULL );
  char const *cc = NULL;
  char *tws = NULL;                     // trailing whitespace

  if ( (s = CONST_CAST(char*, is_line_comment( s ))) != NULL ) {
    switch ( delim ) {
      case DELIM_EOL:
        while ( *++s != '\0' && *s == close_cc[0] )
          /* empty */;

        for ( ; *s != '\0'; ++s ) {
          if ( isspace( *s ) ) {
            if ( tws == NULL )
              tws = s;
            continue;
          }
          tws = NULL;

          if ( *s == close_cc[0] ) {
            //
            // We've found the comment delimiter character: if it's the first
            // time we've found it, mark its position to return.
            //
            // As a special case, we also remark its position if it's the first
            // after whitespace, e.g.:
            //
            //      # This is a comment. # #
            //
            if ( cc == NULL || is_space( s[-1] ) )
              cc = s;
          } else {
            //
            // Ignore a mid-comment delimiter character, that is a delimiter
            // character followed by a non-whitespace character, e.g.:
            //
            //      # This is a comment with a '#' in it.
            //
            cc = NULL;
          }
        } // for
        break;

      case DELIM_SINGLE:
        while ( *++s != '\0' ) {
          if ( cc == NULL ) {
            if ( *s == close_cc[0] ) {
              //
              // We've found the first occurrence of the comment delimiter
              // character: mark its position to return.
              //
              cc = s;
              tws = s + 1;
            }
          } else if ( !isspace( *s ) ) {
            //
            // We've found something other than whitespace after the comment
            // delimiter character: don't consider the comment a terminated
            // comment.
            //
            return NULL;
          }
        } // while
        break;

      case DELIM_DOUBLE:
        while ( *++s != '\0' ) {
          if ( cc == NULL ) {
            if ( *s == close_cc[0] ) {
              //
              // We've found the first occurrence of the comment delimiter
              // character: mark its position to return.
              //
              cc = s;
            }
          } else if ( *s != close_cc[0] ) {
            if ( *s == close_cc[1] && s[-1] == close_cc[0] ) {
              tws = s + 1;
            } else if ( !isspace( *s ) ) {
              if ( tws != NULL )
                return NULL;
              cc = NULL;
            }
          }
        } // while
        break;
    } // switch
  }

  if ( tws != NULL && cc != NULL )
    strcpy( tws, eol() );
  return cc;
}

/**
 * Spans the initial part of \a s for the prefix "prototype."  The prefix is
 * defined as \c ^{WS}*{CC}*{WS}* where \c WS is whitespace and \c CC are
 * comment delimiter characters.
 *
 * @param s The string to span.
 * @return Returns the length of the prototype.
 */
NODISCARD
static size_t prefix_span( char const *s ) {
  assert( s != NULL );
  size_t ws_len = strspn( s, WS_ST );
  size_t const cc_len = strspn( s += ws_len, opt_comment_chars );
  if ( cc_len > 0 )
    ws_len += strspn( s + cc_len, WS_ST );
  return ws_len + cc_len;
}

/**
 * Reads the first line of input to obtain a sequence of leading characters to
 * be the prototype for all lines.  Handles C-style block comments as a special
 * case.
 */
static void read_prototype( void ) {
  char const *const cc = is_line_comment( CURR );
  if ( cc != NULL ) {
    //
    // From now on, recognize only the comment delimiter character(s) found as
    // comment delimiters.  This handles cases like:
    //
    //      // This is a comment
    //      #define MACRO
    //
    // where a comment is followed by a line that is not part of the comment
    // even though it starts with the comment delimiter '#'.
    //
    // The variable cc_buf holds the final set of recognized comment delimiter
    // character(s).  There are three cases where it contains:
    //
    //  1. A single character, e.g., '#', that is a to-end-of-line comment.
    //     This is the DELIM_EOL delimiter type.
    //
    //  2. Two characters:
    //      * E.g., "/*", the characters of both the opening and closing
    //        comment delimiters where the closing delimiter shares a character
    //        with the opening delimiter (hence the final '/' in "*/" isn't
    //        repeated).  This is the DELIM_DOUBLE delimiter type.
    //      * E.g., "{}" (Pascal), two single-character open/close comment
    //        delimiter characters.  This is the DELIM_SINGLE delimiter type.
    //
    //  3. Three characters, e.g., "(*)", the characters of both the opening
    //     and closing comment delimiters where the closing delimiter is
    //     different from the opening delimiter (hence, the opening delimiter
    //     is "(*" and the closing is "*)").  This is also the DELIM_DOUBLE
    //     delimiter type.
    //
    // While this is useful (and efficient) for checking whether a character is
    // a comment delimiter character, it's difficult to use for other purposes.
    // Therefore, the global close_cc[] and delim variables help out.
    //
    char cc_buf[ 3 + 1/*null*/ ] = { '\0' };
    char *s = cc_buf;
    *s++ = cc[0];
    delim = DELIM_EOL;                  // assume this to start

    //
    // Get the closing comment delimiter character corresponding to the first,
    // if any.
    //
    char closing = closing_char( cc[0] );

    switch ( cc[0] ) {
      //
      // We also have to recognize the second character of two-character
      // comment delimiters, but only if it's not the same as the first
      // character (to exclude delimiters like "//") and among the originally
      // specified set of delimiter characters.
      //
      case '#': // "#|": Lisp, Racket, Scheme
      case '(': // "(*": AppleScript, Delphi, ML, OCaml, Pascal; "(:": XQuery
      case '/': // "/*": C, Objective C, C++, C#, D, Go, Java, Rust, Swift
      case '<': // "<#": PowerShell
      case '{': // "{-": Haskell
        if ( cc[1] != cc[0] && is_comment_char( cc[1] ) && cc[1] != closing ) {
          *s++ = cc[1];
          delim = DELIM_DOUBLE;         // e.g., "/*"
        }
        break;
      case '*': // "*>": COBOL 2002
        if ( cc[1] == '>' && is_comment_char( '>' ) )
          *s++ = '>';
        break;
      //
      // Special case for Simula, the only supported language that has two
      // single-character open/close comment delimiter characters where the
      // closing character is not a "conventional" closing character of the
      // opening character, i.e., not one of ")>]}".
      //
      case '!': // "! ... ;": Simula
        if ( is_comment_char( ';' ) &&
             cc[1] != '!' && !is_comment_char( cc[1] ) ) {
          closing = ';';
        }
        break;
    } // switch

    //
    // We also have to recognize the closing delimiter character, if any.
    //
    if ( closing != '\0' ) {
      *s++ = closing;
      if ( delim == DELIM_EOL )
        delim = DELIM_SINGLE;           // e.g., "}" (Pascal)
    }

    //
    // For later convenience, pluck out the terminating comment delimiter
    // character(s) in order.
    //
    switch ( delim ) {
      case DELIM_EOL:
        close_cc[0] = cc_buf[0];        // e.g., "#"
        break;
      case DELIM_SINGLE:
        close_cc[0] = cc_buf[1];        // e.g., "}" (Pascal)
        break;
      case DELIM_DOUBLE:
        close_cc[0] = cc_buf[1];
        close_cc[1] = cc_buf[2] != '\0' ?
          cc_buf[2]                     // e.g., "(*)" (Pascal)
        : cc_buf[0];                    // e.g., "/*"  (C)
    } // switch

    // restrict recognized comment characters to those found
    cc_buf[2] = '\0';
    opt_comment_chars = cc_map_compile( cc_buf );
  }

  char *proto = CURR;
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
    PJL_IGNORE_RV( check_readline( NEXT, stdin ) );
    proto = NEXT;
  }

  int line_width = STATIC_CAST( int, opt_line_width );
  //
  // Initialize the prefix and adjust the line width accordingly.
  //
  set_prefix( proto, prefix_span( proto ) );
  line_width -= STATIC_CAST( int, str_width( prefix_buf ) );
  //
  // Initialize the suffix, if any, and adjust the line width accordingly.
  //
  char const *const tc = is_terminated_comment( proto );
  if ( tc != NULL ) {
    suffix_len = chop_eol( suffix_buf, strcpy_len( suffix_buf, tc ) );
    line_width -= 1/*space*/ + STATIC_CAST( int, suffix_len );
  }

  if ( line_width < LINE_WIDTH_MINIMUM ) {
    fatal_error( EX_USAGE,
      "line-width (%d) is too small (<%d)\n",
      line_width, LINE_WIDTH_MINIMUM
    );
  }
  opt_line_width = STATIC_CAST( size_t, line_width );
}

/**
 * Sets the prefix string.
 *
 * @param prefix The new prefix string.
 * @param len The number of characters of \a prefix to use.
 */
static void set_prefix( char const *prefix, size_t len ) {
  assert( prefix != NULL );
  strncpy( prefix_buf, prefix, len );
  prefix_buf[ len ] = '\0';
  prefix_len0 = len;
}

/**
 * Skips all consecutive occurrences of \a c in \a s.
 *
 * @param s The null-terminated string to use.
 * @param c The character to skip.
 * @return Returns a pointer within \a s just past the last consecutive
 * occurrence of \a c.
 */
NODISCARD
static char* skip_c( char *s, char c ) {
  assert( s != NULL );
  for ( ; *s == c; ++s )
    /* empty */;
  return s;
}

/**
 * Skips \a n characters or to an end-of-line character, whichever is first.
 *
 * @param s The null-terminated string to use.
 * @param n The maximum number of characters to skip.
 * @return Returns either \a s+n or a pointer to an end-of-line character.
 */
NODISCARD
static char* skip_n( char *s, size_t n ) {
  assert( s != NULL );
  for ( ; *s != '\0' && !is_eol( *s ) && n > 0; ++s, --n )
    /* empty */;
  return s;
}

/**
 * A special variant of **strlen**(3) that gets the length not including
 * trailing end-of-line characters, if any.
 *
 * @param s The null-terminated string to get the length of.
 * @return Returns said length.
 */
NODISCARD
static size_t strlen_no_eol( char const *s ) {
  assert( s != NULL );
  size_t n = strlen( s );
  (void)(n > 0 && s[ n - 1 ] == '\n' &&
       --n > 0 && s[ n - 1 ] == '\r' &&
       --n > 0);
  return n;
}

/**
 * Gets the text associated with the given exit status.
 *
 * @param status The exit status to get the text for.
 * @return Returns said text or "unknown status."
 */
NODISCARD
static char const* str_status( int status ) {
  switch ( status ) {
    case /*  0 */ EX_OK         : return "success";
    case /* 64 */ EX_USAGE      : return "usage error";
    case /* 65 */ EX_DATAERR    : return "input data error";
    case /* 66 */ EX_NOINPUT    : return "error opening file";
    case /* 69 */ EX_UNAVAILABLE: return "service unavailable";
    case /* 70 */ EX_SOFTWARE   : return "internal error";
    case /* 71 */ EX_OSERR      : return "system error (e.g., can't fork)";
    case /* 72 */ EX_OSFILE     : return "error opening system file";
    case /* 73 */ EX_CANTCREAT  : return "error creating file";
    case /* 74 */ EX_IOERR      : return "I/O error";
    case /* 78 */ EX_CONFIG     : return "configuration file error";
    default                     : return "unknown status";
  } // switch
}

/**
 * Computes the width of a string where tabs have a width of opt_tab_spaces
 * spaces minus the number of spaces we're into a tab-stop; all others
 * characters have a width of 1.
 *
 * @param s The null-terminated string to calculate the width of.
 * @return Returns said width.
 */
NODISCARD
static size_t str_width( char const *s ) {
  assert( s != NULL );
  size_t width = 0;
  while ( *s != '\0' )
    width += char_width( *s++, width );
  return width;
}

/**
 * Prints the usage message and exits.
 *
 * @param status The status to exit with.  If it is `EX_OK`, prints to standard
 * output; otherwise prints to standard error.
 */
static void usage( int status ) {
  fprintf( status == EX_OK ? stdout : stderr,
"usage: " PACKAGE "c [options]\n"
"options:\n"
"  --alias=NAME           " UOPT(ALIAS)
                          "Use alias from configuration file.\n"
"  --align-column=NUM[,S] " UOPT(ALIGN_COLUMN)
                          "Column to align end-of-line comments on.\n"
"  --block-regex=REGEX    " UOPT(BLOCK_REGEX)
                          "Block leading regular expression.\n"
"  --comment-chars=STR    " UOPT(COMMENT_CHARS)
                          "Comment delimiter characters.\n"
"  --config=FILE          " UOPT(CONFIG)
                          "The configuration file [default: ~/" CONF_FILE_NAME_DEFAULT "].\n"
"  --doxygen              " UOPT(DOXYGEN)
                          "Format Doxygen.\n"
"  --eol=STR              " UOPT(EOL) "\n"
"      Set line-endings as input/Unix/Windows [default: input].\n"
"  --eos-delimit          " UOPT(EOS_DELIMIT) "\n"
"      Treat whitespace after end-of-sentence as a paragraph delimiter.\n"
"  --eos-spaces=NUM       " UOPT(EOS_SPACES)
                          "Spaces after end-of-sentence [default: " STRINGIFY(EOS_SPACES_DEFAULT) "].\n"
"  --file=FILE            " UOPT(FILE)
                          "Read from this file [default: stdin].\n"
"  --file-name=NAME       " UOPT(FILE_NAME)
                          "Filename for stdin.\n"
"  --help                 " UOPT(HELP)
                          "Print this help and exit.\n"
"  --markdown             " UOPT(MARKDOWN)
                          "Format Markdown.\n"
"  --no-config            " UOPT(NO_CONFIG)
                          "Suppress reading configuration file.\n"
"  --no-hyphen            " UOPT(NO_HYPHEN)
                          "Suppress wrapping at hyphen characters.\n"
"  --output=FILE          " UOPT(OUTPUT)
                          "Write to this file [default: stdout].\n"
"  --para-chars=STR       " UOPT(PARA_CHARS)
                          "Additional paragraph delimiter characters.\n"
"  --tab-spaces=NUM       " UOPT(TAB_SPACES)
                          "Tab-spaces equivalence [default: " STRINGIFY(TAB_SPACES_DEFAULT) "].\n"
"  --title                " UOPT(TITLE_LINE)
                          "Treat paragraph's first line as title.\n"
"  --version              " UOPT(VERSION)
                          "Print version and exit.\n"
"  --width=NUM|terminal   " UOPT(WIDTH) "Line width [default: " STRINGIFY(LINE_WIDTH_DEFAULT) "].\n"
"\n"
PACKAGE_NAME " home page: " PACKAGE_URL "\n"
"Report bugs to: " PACKAGE_BUGREPORT "\n"
  );
  exit( status );
}

/**
 * Wait for child processes to terminate so they don't become zombies.
 */
static void wait_for_child_processes( void ) {
#ifndef DEBUG_RSWW
  int wait_status;
  for ( pid_t pid; (pid = wait( &wait_status )) > 0; ) {
    if ( WIFEXITED( wait_status ) ) {
      int const exit_status = WEXITSTATUS( wait_status );
      if ( exit_status != 0 ) {
        fatal_error( exit_status,
          "child process exited with status %d: %s\n",
          exit_status, str_status( exit_status )
        );
      }
    } else if ( WIFSIGNALED( wait_status ) ) {
      int const signal = WTERMSIG( wait_status );
      fatal_error( EX_OSERR,
        "child process terminated with signal %d: %s\n",
        signal, strsignal( signal )
      );
    }
  } // for
#endif /* DEBUG_RSWW */
}

/**
 * If \a line starts with a Doxygen command, handle it.
 *
 * @param line The line of text to check.
 * @param fout The `FILE` to possibly write to.
 * @return Returns `true` only if a Doxygen command was handled; `false`
 * otherwise.
 *
 * @note The caller _must_ write \a line only if this function returns \a
 * false.
 */
static bool wrap_dox_line( char const *line, FILE *fout ) {
  char dox_cmd_name[ DOX_CMD_NAME_SIZE_MAX + 1 ];
  if ( !dox_parse_cmd_name( line, dox_cmd_name ) )
    return false;

  static dox_cmd_t const *prev_dox_cmd;
  if ( prev_dox_cmd != NULL ) {
    if ( strcmp( dox_cmd_name, prev_dox_cmd->end_name ) == 0 ) {
      //
      // We've encountered the previous Doxygen command's corresponding end
      // command: put that line, then tell wrap to resume wrapping.
      //
      FPUTS( line, fout );
      WIPC_SEND( fout, WIPC_CODE_PREFORMATTED_END );
      prev_dox_cmd = NULL;
      return true;
    }

    //
    // It's not the corresponding end command for the previous Doxygen command
    // that copies preformatted text: just pass it along to wrap as-is.
    //
    return false;
  }

  //
  // See if it's a known Doxygen command.
  //
  dox_cmd_t const *const dox_cmd = dox_find_cmd( dox_cmd_name );
  if ( dox_cmd == NULL ) {
    //
    // It's a Doxygen command we know nothing about (or something that looks
    // like a Doxgen command, e.g., "\t"): just pass it along to wrap as-is
    // and hope for the best.
    //
    return false;
  }

  if ( (dox_cmd->type & DOX_BOL) != 0 )
    WIPC_SEND( fout, WIPC_CODE_DELIMIT_PARAGRAPH );

  if ( (dox_cmd->type & DOX_EOL) != 0 ) {
    //
    // The Doxygen command continues until the end of the line: treat it as
    // preformatted.
    //
    WIPC_SEND( fout, WIPC_CODE_PREFORMATTED_BEGIN );
  }

  FPUTS( line, fout );

  if ( (dox_cmd->type & DOX_EOL) != 0 )
    WIPC_SEND( fout, WIPC_CODE_PREFORMATTED_END );

  if ( (dox_cmd->type & DOX_PRE) != 0 ) {
    //
    // The Doxygen command is for a block of preformatted text (e.g., @code,
    // @dot, @verbatim, etc.): tell wrap to suspend wrapping and begin sending
    // preformatted text through verbatim until we encounter the command's
    // corresponding end command.
    //
    WIPC_SEND( fout, WIPC_CODE_PREFORMATTED_BEGIN );
    prev_dox_cmd = dox_cmd;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
