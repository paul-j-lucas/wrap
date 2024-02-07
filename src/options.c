/*
**      wrap -- text reformatter
**      src/options.c
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
 * Defines variables for all **wrap**(1) and **wrapc**(1) command-line and
 * configuration file options as well as functions to read and parse them.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "options.h"
#include "alias.h"
#include "common.h"
#include "pattern.h"
#include "read_conf.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>                   /* for SIZE_MAX */
#include <stdbool.h>

/// @endcond

/**
 * @addtogroup options-group
 * @{
 */

#define OPT_BUF_SIZE              32    /**< Used by opt_format(). */

///////////////////////////////////////////////////////////////////////////////

// local constants
static char const   COMMENT_CHARS_DEFAULT[] =
  "!"  ","  // Fortran, Simula
  "#"  ","  // AWK, CMake, Crystal, Julia, Make, Nim, Octave, Perl, Python, R,
            //    Ruby, Shell, Tcl
  "#=" ","  // Julia
  "#|" ","  // Lisp, Racket, Scheme
  "%"  ","  // Erlang, Matlab, Metafont, Octave, PostScript, Prolog, TeX
  "(*" ","  // AppleScript, Delphi, Mathematica, ML, Modula-[23], Oberon,
            //    OCaml, Pascal
  "(:" ","  // XQuery
  "*>" ","  // COBOL 2002
  "--" ","  // Ada, AppleScript, Eiffel, Haskell, Lua, SQL, VHDL
  "/*" ","  // C, Objective C, C#, C++, D, Go, Kotlin, Java, Maxima, Pike,
            //    PL/I, Pure, Rust, Scala, SQL, Swift
  "/+" ","  // D
  "//" ","  // C, Objective C, C#, C++, D, F#, Go, Kotlin, Java, Pike, Pure,
            //    Rust, Scala, Swift, Zig
  ";"  ","  // Assembly, Clojure, Lisp, Scheme
  "<#" ","  // PowerShell
  ">"  ","  // Quoted e-mail
  "{"  ","  // Pascal
  "{-" ","  // Haskell
  "\\" ","  // Forth
  ;

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries for each option.

// extern option variables
char const         *opt_alias;
char                opt_align_char;
size_t              opt_align_column;
char const         *opt_block_regex;
char const         *opt_comment_chars = COMMENT_CHARS_DEFAULT;
char const         *opt_conf_file;
bool                opt_doxygen;
eol_t               opt_eol = EOL_INPUT;
bool                opt_eos_delimit;
size_t              opt_eos_spaces = EOS_SPACES_DEFAULT;
bool                opt_data_link_esc;
char const         *opt_fin_name;
size_t              opt_hang_spaces;
size_t              opt_hang_tabs;
size_t              opt_indt_spaces;
size_t              opt_indt_tabs;
bool                opt_lead_dot_ignore;
size_t              opt_lead_spaces;
char const         *opt_lead_string;
size_t              opt_lead_tabs;
bool                opt_lead_ws_delimit;
size_t              opt_line_width = LINE_WIDTH_DEFAULT;
bool                opt_markdown;
size_t              opt_mirror_spaces;
size_t              opt_mirror_tabs;
size_t              opt_newlines_delimit = NEWLINES_DELIMIT_DEFAULT;
bool                opt_no_conf;
bool                opt_no_hyphen;
char const         *opt_para_delims;
bool                opt_prototype;
size_t              opt_tab_spaces = TAB_SPACES_DEFAULT;
bool                opt_title_line;

/// @endcond

// local variables
static char const  *fin_path = "-";     ///< File in path.
static char const  *fout_path = "-";    ///< File out path.
static bool         is_wrapc;           ///< Are we **wrapc**(1)?
static bool         opts_given[ 128 ];  ///< Options given indexed by `char`.

// local functions
NODISCARD
static unsigned     parse_width( char const* );

///////////////////////////////////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
#define SOPT_NO_ARGUMENT          /* nothing */
#define SOPT_REQUIRED_ARGUMENT    ":"
#define SOPT_OPTIONAL_ARGUMENT    "::"
/// @endcond

/**
 * Command-line short options common to both **wrap**(1) and **wrapc**(1).
 */
#define COMMON_OPTS_SHORT                             \
  SOPT(ALIAS)                 SOPT_REQUIRED_ARGUMENT  \
  SOPT(BLOCK_REGEX)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(CONFIG)                SOPT_REQUIRED_ARGUMENT  \
  SOPT(DOXYGEN)               SOPT_NO_ARGUMENT        \
  SOPT(EOL)                   SOPT_REQUIRED_ARGUMENT  \
  SOPT(EOS_DELIMIT)           SOPT_NO_ARGUMENT        \
  SOPT(EOS_SPACES)            SOPT_REQUIRED_ARGUMENT  \
  SOPT(FILE)                  SOPT_REQUIRED_ARGUMENT  \
  SOPT(FILE_NAME)             SOPT_REQUIRED_ARGUMENT  \
  SOPT(HELP)                  SOPT_OPTIONAL_ARGUMENT  \
  SOPT(MARKDOWN)              SOPT_NO_ARGUMENT        \
  SOPT(NO_CONFIG)             SOPT_NO_ARGUMENT        \
  SOPT(NO_HYPHEN)             SOPT_NO_ARGUMENT        \
  SOPT(OUTPUT)                SOPT_REQUIRED_ARGUMENT  \
  SOPT(PARA_CHARS)            SOPT_REQUIRED_ARGUMENT  \
  SOPT(TAB_SPACES)            SOPT_REQUIRED_ARGUMENT  \
  SOPT(TITLE_LINE)            SOPT_NO_ARGUMENT        \
  SOPT(VERSION)               SOPT_NO_ARGUMENT        \
  SOPT(WIDTH)                 SOPT_REQUIRED_ARGUMENT

/**
 * Command-line options forbidden in configuration files.
 */
#define CONF_FORBIDDEN_OPTS_SHORT \
  SOPT(ALIAS)                     \
  SOPT(CONFIG)                    \
  SOPT(FILE)                      \
  SOPT(FILE_NAME)                 \
  SOPT(NO_CONFIG)                 \
  SOPT(OUTPUT)                    \
  SOPT(VERSION)

/**
 * Command-line short options specific to **wrap**(1).
 *
 * @note The short option for `--hang-tabs` isn't here because it shares `-h`
 * with `--help` that is common to both **wrap**(1) and **wrapc**(1).  There's
 * special-case code in parse_options() that disambiguates `-h`.
 */
#define WRAP_SPECIFIC_OPTS_SHORT                      \
  SOPT(ALL_NEWLINES_DELIMIT)  SOPT_NO_ARGUMENT        \
  SOPT(ENABLE_IPC)            SOPT_NO_ARGUMENT        \
  SOPT(DOT_IGNORE)            SOPT_NO_ARGUMENT        \
  SOPT(HANG_SPACES)           SOPT_REQUIRED_ARGUMENT  \
/*SOPT(HANG_TABS)             SOPT_REQUIRED_ARGUMENT*/\
  SOPT(INDENT_SPACES)         SOPT_REQUIRED_ARGUMENT  \
  SOPT(INDENT_TABS)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(LEAD_SPACES)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(LEAD_STRING)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(LEAD_TABS)             SOPT_REQUIRED_ARGUMENT  \
  SOPT(MIRROR_SPACES)         SOPT_REQUIRED_ARGUMENT  \
  SOPT(MIRROR_TABS)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(NO_NEWLINES_DELIMIT)   SOPT_NO_ARGUMENT        \
  SOPT(PROTOTYPE)             SOPT_NO_ARGUMENT        \
  SOPT(WHITESPACE_DELIMIT)    SOPT_NO_ARGUMENT

/**
 * Command-line short options specific to **wrapc**(1).
 */
#define WRAPC_SPECIFIC_OPTS_SHORT                     \
  SOPT(ALIGN_COLUMN)          SOPT_REQUIRED_ARGUMENT  \
  SOPT(COMMENT_CHARS)         SOPT_REQUIRED_ARGUMENT

//
// Each command forbids the others' specific options, but only on the command-
// line and not in configuration files.
//
static char const *const CMDLINE_FORBIDDEN_OPTS_SHORT[] = {
  WRAPC_SPECIFIC_OPTS_SHORT,            // wrap
  WRAP_SPECIFIC_OPTS_SHORT              // wrapc
};

/**
 * Command-line short options array:
 *
 *  + 0 = wrap's options
 *  + 1 = wrapc's options
 */
static char const *const OPTS_SHORT[] = {
  // wrap's options have to include wrapc's specific options so they're
  // accepted (but ignored) in conf files.
  ":" COMMON_OPTS_SHORT WRAP_SPECIFIC_OPTS_SHORT WRAPC_SPECIFIC_OPTS_SHORT,

  ":" COMMON_OPTS_SHORT WRAPC_SPECIFIC_OPTS_SHORT
};

/**
 * Command-line common long options.
 */
#define COMMON_OPTS_LONG                                                      \
  { "alias",                required_argument,  NULL, COPT(ALIAS)         },  \
  { "block-regex",          required_argument,  NULL, COPT(BLOCK_REGEX)   },  \
  { "config",               required_argument,  NULL, COPT(CONFIG)        },  \
  { "doxygen",              no_argument,        NULL, COPT(DOXYGEN)       },  \
  { "eol",                  required_argument,  NULL, COPT(EOL)           },  \
  { "eos-delimit",          no_argument,        NULL, COPT(EOS_DELIMIT)   },  \
  { "eos-spaces",           required_argument,  NULL, COPT(EOS_SPACES)    },  \
  { "file",                 required_argument,  NULL, COPT(FILE)          },  \
  { "file-name",            required_argument,  NULL, COPT(FILE_NAME)     },  \
  { "help",                 no_argument,        NULL, COPT(HELP)          },  \
  { "markdown",             no_argument,        NULL, COPT(MARKDOWN)      },  \
  { "no-config",            no_argument,        NULL, COPT(NO_CONFIG)     },  \
  { "no-hyphen",            no_argument,        NULL, COPT(NO_HYPHEN)     },  \
  { "output",               required_argument,  NULL, COPT(OUTPUT)        },  \
  { "para-chars",           required_argument,  NULL, COPT(PARA_CHARS)    },  \
  { "tab-spaces",           required_argument,  NULL, COPT(TAB_SPACES)    },  \
  { "title-line",           no_argument,        NULL, COPT(TITLE_LINE)    },  \
  { "version",              no_argument,        NULL, COPT(VERSION)       },  \
  { "width",                required_argument,  NULL, COPT(WIDTH)         }

/**
 * Command-line wrapc-specific long options.
 */
#define WRAPC_SPECIFIC_OPTS_LONG                                              \
  { "align-column",         required_argument,  NULL, COPT(ALIGN_COLUMN)  },  \
  { "comment-chars",        required_argument,  NULL, COPT(COMMENT_CHARS) }

/**
 * Command-line wrap long options.
 */
static struct option const WRAP_OPTS_LONG[] = {
  COMMON_OPTS_LONG,
  { "all-newlines-delimit", no_argument,        NULL, COPT(ALL_NEWLINES_DELIMIT) },
  { "dot-ignore",           no_argument,        NULL, COPT(DOT_IGNORE)    },
  { "hang-spaces",          required_argument,  NULL, COPT(HANG_SPACES)   },
  { "hang-tabs",            required_argument,  NULL, COPT(HANG_TABS)     },
  { "indent-spaces",        required_argument,  NULL, COPT(INDENT_SPACES) },
  { "indent-tabs",          required_argument,  NULL, COPT(INDENT_TABS)   },
  { "lead-spaces",          required_argument,  NULL, COPT(LEAD_SPACES)   },
  { "lead-string",          required_argument,  NULL, COPT(LEAD_STRING)   },
  { "lead-tabs",            required_argument,  NULL, COPT(LEAD_TABS)     },
  { "mirror-spaces",        required_argument,  NULL, COPT(MIRROR_SPACES) },
  { "mirror-tabs",          required_argument,  NULL, COPT(MIRROR_TABS)   },
  { "no-newlines-delimit",  no_argument,        NULL, COPT(NO_NEWLINES_DELIMIT) },
  { "prototype",            no_argument,        NULL, COPT(PROTOTYPE)     },
  { "whitespace-delimit",   no_argument,        NULL, COPT(WHITESPACE_DELIMIT) },
  { "_ENABLE-IPC",          no_argument,        NULL, COPT(ENABLE_IPC)    },

  // wrap's options have to include wrapc's specific options so they're
  // accepted (but ignored) in conf files.
  WRAPC_SPECIFIC_OPTS_LONG,

  { NULL,                   0,                  NULL, 0                   }
};

/**
 * Command-line wrapc long options.
 */
static struct option const WRAPC_OPTS_LONG[] = {
  COMMON_OPTS_LONG,
  WRAPC_SPECIFIC_OPTS_LONG,
  { NULL,                   0,                  NULL, 0                   }
};

/**
 * Command-line long options array:
 *
 *  + 0 = wrap's options
 *  + 1 = wrapc's options
 */
static struct option const *const OPTS_LONG[] = {
  WRAP_OPTS_LONG,
  WRAPC_OPTS_LONG
};

////////// local functions ////////////////////////////////////////////////////

/**
 * If \a opt was given, checks that _only_ it was given and, if not, prints an
 * error message and exits; if \a opt was not given, does nothing.
 *
 * @param opt The option to check for.
 */
static void opt_check_exclusive( char opt ) {
  if ( !opts_given[ STATIC_CAST( unsigned, opt ) ] )
    return;
  for ( size_t i = 0; i < ARRAY_SIZE( opts_given ); ++i ) {
    char const curr_opt = STATIC_CAST( char, i );
    if ( curr_opt == opt )
      continue;
    if ( opts_given[ STATIC_CAST( unsigned, curr_opt ) ] ) {
      fatal_error( EX_USAGE,
        "%s can be given only by itself\n", opt_format( opt )
      );
    }
  } // for
}

/**
 * If \a opt was given, checks that no option in \a opts was also given.  If it
 * was, prints an error message and exits; if it wasn't, does nothing.
 *
 * @param opt The option.
 * @param opts The set of options.
 *
 * @sa opt_check_s_mutually_exclusive()
 */
static void opt_check_mutually_exclusive( char opt, char const *opts ) {
  assert( opts != NULL );
  if ( !opts_given[ STATIC_CAST( unsigned, opt ) ] )
    return;
  for ( ; *opts != '\0'; ++opts ) {
    assert( *opts != opt );
    if ( opts_given[ STATIC_CAST( unsigned, *opts ) ] ) {
      fatal_error( EX_USAGE,
        "%s and %s are mutually exclusive\n",
        opt_format( opt ),
        opt_format( *opts )
      );
    }
  } // for
}

/**
 * Checks that no options were given that are among the two given mutually
 * exclusive sets of short options.
 * Prints an error message and exits if any such options are found.
 *
 * @param opts1 The first set of short options.
 * @param opts2 The second set of short options.
 *
 * @sa opt_check_mutually_exclusive()
 */
static void opt_check_s_mutually_exclusive( char const *opts1,
                                            char const *opts2 ) {
  assert( opts1 != NULL );
  for ( ; *opts1 != '\0'; ++opts1 )
    opt_check_mutually_exclusive( *opts1, opts2 );
}

/**
 * Gets the corresponding name of the long option for the given short option.
 *
 * @param short_opt The short option to get the corresponding long option for.
 * @return Returns the said option or the empty string if none.
 */
NODISCARD
static char const* opt_get_long( char short_opt ) {
  for ( struct option const *long_opt = OPTS_LONG[ is_wrapc ];
        long_opt->name != NULL;
        ++long_opt ) {
    if ( long_opt->val == short_opt )
      return long_opt->name;
  } // for
  return "";
}

/**
 * Parses an alignment column specification, that is an integer optionally
 * followed by an alignment character specification.
 *
 * @param s The null-terminated string to parse.
 * @param align_char A pointer to the character to set if an alignment
 * character specification is given.
 * @return Returns the alignment column.
 */
NODISCARD
static unsigned parse_align( char const *s, char *align_char ) {
  assert( s != NULL );
  assert( align_char != NULL );

  static char const *const AUTO  [] = { "a", "auto", NULL };
  static char const *const SPACES[] = { "s", "space", "spaces", NULL };
  static char const *const TABS  [] = { "t", "tab", "tabs", NULL };

  char *end = NULL;
  errno = 0;
  unsigned const col = STATIC_CAST( unsigned, strtoul( s, &end, 10 ) );
  if ( unlikely( errno != 0 || end == s ) )
    goto error;
  if ( *end != '\0' ) {
    if ( *end == ',' )
      ++end;
    if ( is_any( end, AUTO ) )
      /* do nothing */;
    else if ( is_any( end, SPACES ) )
      *align_char = ' ';
    else if ( is_any( end, TABS ) )
      *align_char = '\t';
    else
      goto error;
  }
  return col;

error:
  fatal_error( EX_USAGE,
    "\"%s\": invalid value for %s;"
    " must be digits followed by one of:"
    " a, auto, s, space, spaces, t, tab, tabs\n",
    s, opt_format( 'A' )
  );
}

/**
 * Parses an End-of-Line value.
 *
 * @param s The null-terminated string to parse.
 * @return Returns the corresponding \ref eol_t or prints an error message and
 * exits if \a s is invalid.
 */
NODISCARD
static eol_t parse_eol( char const *s ) {
  struct eol_map {
    char const *em_name;
    eol_t       em_eol;
  };
  typedef struct eol_map eol_map_t;

  static eol_map_t const EOL_MAP[] = {
    { "-",        EOL_INPUT   },
    { "crlf",     EOL_WINDOWS },
    { "d",        EOL_WINDOWS },
    { "dos",      EOL_WINDOWS },
    { "i",        EOL_INPUT   },
    { "input",    EOL_INPUT   },
    { "lf",       EOL_UNIX    },
    { "u",        EOL_UNIX    },
    { "unix",     EOL_UNIX    },
    { "w",        EOL_WINDOWS },
    { "windows",  EOL_WINDOWS },
    { NULL,       EOL_INPUT   }
  };

  assert( s != NULL );
  size_t values_buf_size = 1;           // for trailing null

  for ( eol_map_t const *m = EOL_MAP; m->em_name != NULL; ++m ) {
    if ( strcasecmp( s, m->em_name ) == 0 )
      return m->em_eol;
    // sum sizes of names in case we need to construct an error message
    values_buf_size += strlen( m->em_name ) + 2 /* ", " */;
  } // for

  // name not found: construct valid name list for an error message
  char *const values_buf = free_later( MALLOC( char, values_buf_size ) );
  char *pvalues = values_buf;
  for ( eol_map_t const *m = EOL_MAP; m->em_name != NULL; ++m ) {
    if ( pvalues > values_buf ) {
      strcpy( pvalues, ", " );
      pvalues += 2;
    }
    strcpy( pvalues, m->em_name );
    pvalues += strlen( m->em_name );
  } // for

  fatal_error( EX_USAGE,
    "\"%s\": invalid value for %s; must be one of:\n\t%s\n",
    s, opt_format( 'l' ), values_buf
  );
}

/**
 * Parses command-line options.
 *
 * @param argc The argument count from main().
 * @param argv The argument values from main().
 * @param short_opts The set of short options.
 * @param long_opts The set of long options.
 * @param cmdline_forbidden_opts The set of options that are forbidden on the
 * command line.
 * @param usage A pointer to a function that prints a usage message and exits.
 * If called, it _must not_ return.
 * @param line_no When parsing options from a configuration file, the
 * originating line number; zero otherwise.
 */
static void parse_options( int argc, char const *argv[],
                           char const short_opts[const static 2],
                           struct option const long_opts[const static 2],
                           char const cmdline_forbidden_opts[const static 1],
                           /*_Noreturn*/ void (*usage)(int),
                           unsigned line_no ) {
  assert( argv != NULL );
  assert( usage != NULL );

  opterr = 0;
  optind = 1;

  int opt;
  bool opt_help = false;
  bool opt_version = false;
  MEM_ZERO( &opts_given );

  for (;;) {
    opt = getopt_long(
      argc, CONST_CAST( char**, argv ), short_opts, long_opts,
      /*longindex=*/NULL
    );
    if ( opt == -1 )
      break;
    if ( opt != ':' ) {
      if ( line_no > 0 ) {                // we're parsing a conf file
        if ( strchr( CONF_FORBIDDEN_OPTS_SHORT, opt ) != NULL ) {
          fatal_error( EX_CONFIG,
            "%s:%u: %s option not allowed in configuration file\n",
            opt_conf_file, line_no, opt_format( STATIC_CAST( char, opt ) )
          );
        }
      }
      else if ( strchr( cmdline_forbidden_opts, opt ) != NULL ) {
        goto invalid_opt;
      }
    }

    switch ( opt ) {
      case COPT(ALIAS):
        if ( SKIP_CHARS( optarg, WS_ST )[0] == '\0' )
          goto missing_arg;
        opt_alias = optarg;
        break;
      case COPT(ALIGN_COLUMN):
        opt_align_column = parse_align( optarg, &opt_align_char );
        break;
      case COPT(ALL_NEWLINES_DELIMIT):
        opt_newlines_delimit = 1;
        break;
      case COPT(BLOCK_REGEX):
        if ( SKIP_CHARS( optarg, WS_ST )[0] == '\0' )
          goto missing_arg;
        opt_block_regex = optarg;
        break;
      case COPT(COMMENT_CHARS):
        if ( SKIP_CHARS( optarg, WS_ST )[0] == '\0' )
          goto missing_arg;
        opt_comment_chars = optarg;
        break;
      case COPT(CONFIG):
        if ( SKIP_CHARS( optarg, WS_ST )[0] == '\0' )
          goto missing_arg;
        opt_conf_file = optarg;
        break;
      case COPT(DOT_IGNORE):
        opt_lead_dot_ignore = true;
        break;
      case COPT(DOXYGEN):
        opt_doxygen = true;
        break;
      case COPT(ENABLE_IPC):
        opt_data_link_esc = true;
        break;
      case COPT(EOL):
        opt_eol = parse_eol( optarg );
        break;
      case COPT(EOS_DELIMIT):
        opt_eos_delimit = true;
        break;
      case COPT(EOS_SPACES):
        opt_eos_spaces = check_atou( optarg );
        break;
      case COPT(FILE):
        if ( SKIP_CHARS( optarg, WS_ST )[0] == '\0' )
          goto missing_arg;
        fin_path = optarg;
        FALLTHROUGH;
      case COPT(FILE_NAME):
        if ( SKIP_CHARS( optarg, WS_ST )[0] == '\0' )
          goto missing_arg;
        opt_fin_name = base_name( optarg );
        break;
      case COPT(HANG_TABS):
//    case COPT(HELP):
        //
        // The --hang-tabs and --help options share the same -h short option.
        // We made its argument optional to disambiguate it: if it's absent,
        // assume it's for --help; if present, assume it's for --hand-tabs.
        //
        if ( optarg == NULL )
          opt_help = true;
        else
          opt_hang_tabs = check_atou( optarg );
        break;
      case COPT(HANG_SPACES):
        opt_hang_spaces = check_atou( optarg );
        break;
      case COPT(INDENT_SPACES):
        opt_indt_spaces = check_atou( optarg );
        break;
      case COPT(INDENT_TABS):
        opt_indt_tabs = check_atou( optarg );
        break;
      case COPT(LEAD_SPACES):
        opt_lead_spaces = check_atou( optarg );
        break;
      case COPT(LEAD_STRING):
        // do NOT skip leading whitespace here
        opt_lead_string = optarg;
        break;
      case COPT(LEAD_TABS):
        opt_lead_tabs = check_atou( optarg );
        break;
      case COPT(MARKDOWN):
        opt_markdown = true;
        break;
      case COPT(MIRROR_SPACES):
        opt_mirror_spaces = check_atou( optarg );
        break;
      case COPT(MIRROR_TABS):
        opt_mirror_tabs = check_atou( optarg );
        break;
      case COPT(NO_CONFIG):
        opt_no_conf = true;
        break;
      case COPT(NO_HYPHEN):
        opt_no_hyphen = true;
        break;
      case COPT(NO_NEWLINES_DELIMIT):
        opt_newlines_delimit = SIZE_MAX;
        break;
      case COPT(OUTPUT):
        if ( SKIP_CHARS( optarg, WS_ST )[0] == '\0' )
          goto missing_arg;
        fout_path = optarg;
        break;
      case COPT(PARA_CHARS):
        if ( SKIP_CHARS( optarg, WS_ST )[0] == '\0' )
          goto missing_arg;
        opt_para_delims = optarg;
        break;
      case COPT(PROTOTYPE):
        opt_prototype = true;
        break;
      case COPT(TAB_SPACES):
        opt_tab_spaces = check_atou( optarg );
        break;
      case COPT(TITLE_LINE):
        opt_title_line = true;
        break;
      case COPT(VERSION):
        opt_version = true;
        break;
      case COPT(WHITESPACE_DELIMIT):
        opt_lead_ws_delimit = true;
        break;
      case COPT(WIDTH):
        opt_line_width = parse_width( optarg );
        break;

      case ':':
        goto missing_arg;
      case '?':
        goto invalid_opt;

      default:
        if ( isprint( opt ) )
          INTERNAL_ERROR(
            "'%c': unaccounted-for getopt_long() return value\n", opt
          );
        INTERNAL_ERROR(
          "%d: unaccounted-for getopt_long() return value\n", opt
        );
    } // switch
    opts_given[ opt ] = true;
  } // for

  if ( line_no == 0 ) {
    //
    // Check for mutually exclusive options only when parsing the command-line.
    //
    opt_check_mutually_exclusive( COPT(ALIGN_COLUMN),
      SOPT(ALIAS)
      SOPT(ALL_NEWLINES_DELIMIT)
      SOPT(BLOCK_REGEX)
      SOPT(DOT_IGNORE)
      SOPT(DOXYGEN)
      SOPT(EOS_DELIMIT)
      SOPT(EOS_SPACES)
      SOPT(HANG_SPACES)
      SOPT(HANG_TABS)
      SOPT(INDENT_SPACES)
      SOPT(INDENT_TABS)
      SOPT(LEAD_STRING)
      SOPT(MARKDOWN)
      SOPT(MIRROR_SPACES)
      SOPT(MIRROR_TABS)
      SOPT(NO_HYPHEN)
      SOPT(NO_NEWLINES_DELIMIT)
      SOPT(PARA_CHARS)
      SOPT(PROTOTYPE)
      SOPT(TITLE_LINE)
      SOPT(WHITESPACE_DELIMIT)
      SOPT(WIDTH)
    );
    opt_check_mutually_exclusive( COPT(ALL_NEWLINES_DELIMIT),
      SOPT(NO_NEWLINES_DELIMIT)
    );
    opt_check_mutually_exclusive( COPT(FILE), SOPT(FILE_NAME) );
    opt_check_mutually_exclusive( COPT(MARKDOWN),
      SOPT(TAB_SPACES)
      SOPT(TITLE_LINE)
    );
    opt_check_s_mutually_exclusive( SOPT(MARKDOWN) SOPT(PROTOTYPE),
      SOPT(DOT_IGNORE)
      SOPT(HANG_SPACES)
      SOPT(HANG_TABS)
      SOPT(INDENT_SPACES)
      SOPT(INDENT_TABS)
      SOPT(LEAD_SPACES)
      SOPT(LEAD_STRING)
      SOPT(LEAD_TABS)
      SOPT(MIRROR_SPACES)
      SOPT(MIRROR_TABS)
      SOPT(WHITESPACE_DELIMIT)
    );
    opt_check_exclusive( COPT(VERSION) );
  }

  if ( opt_help ) {
    (*usage)( EX_OK );
    unreachable();
  }

  if ( opt_version ) {
    puts( PACKAGE_STRING );
    exit( EX_OK );
  }

  return;

invalid_opt:
  NO_OP;
  // Determine whether the invalid option was short or long.
  char const *const invalid_opt = argv[ optind - 1 ];
  EPRINTF( "%s: ", me );
  if ( invalid_opt != NULL && strncmp( invalid_opt, "--", 2 ) == 0 )
    EPRINTF( "\"%s\"", invalid_opt + 2/*skip over "--"*/ );
  else
    EPRINTF( "'%c'", STATIC_CAST( char, optopt ) );
  EPUTS( ": invalid option; use --help or -h for help\n" );
  exit( EX_USAGE );

missing_arg:
  fatal_error( EX_USAGE,
    "\"%s\" requires an argument\n",
    opt_format( STATIC_CAST( char, opt == ':' ? optopt : opt ) )
  );
}

/**
 * Parses a width value.
 *
 * @param s The null-terminated string to parse.
 * @return Returns the width value
 * or prints an error message and exits if \a s is invalid.
 */
NODISCARD
static unsigned parse_width( char const *s ) {
#ifdef WITH_WIDTH_TERM
  static char const *const TERM[] = {
    "t",
    "term",
    "terminal",
    NULL
  };

  assert( s != NULL );
  if ( is_digits( s ) )
    return STATIC_CAST( unsigned, strtoul( s, NULL, 10 ) );

  size_t values_buf_size = 1;           // for trailing null
  for ( char const *const *t = TERM; *t != NULL; ++t ) {
    if ( strcasecmp( s, *t ) == 0 )
      return get_term_columns();
    // sum sizes of values in case we need to construct an error message
    values_buf_size += strlen( *t ) + 2 /* ", " */;
  } // for

  // value not found: construct valid value list for an error message
  char values_buf[ values_buf_size ];
  char *pvalues = values_buf;
  for ( char const *const *t = TERM; *t != NULL; ++t ) {
    if ( pvalues > values_buf ) {
      strcpy( pvalues, ", " );
      pvalues += 2;
    }
    strcpy( pvalues, *t );
    pvalues += strlen( *t );
  } // for

  fatal_error( EX_USAGE,
    "\"%s\": invalid value for %s; must be one of:\n\t%s\n",
    s, opt_format( 'w' ), values_buf
  );
#else
  return check_atou( s );
#endif /* WITH_WIDTH_TERM */
}

////////// extern functions ///////////////////////////////////////////////////

char const* opt_format( char short_opt ) {
  static char bufs[ 2 ][ OPT_BUF_SIZE ];
  static unsigned buf_index;
  char *const buf = bufs[ buf_index++ % 2 ];

  char const *const long_opt = opt_get_long( short_opt );
  snprintf(
    buf, OPT_BUF_SIZE, "%s%s%s-%c",
    long_opt[0] != '\0' ? "--" : "", long_opt,
    long_opt[0] != '\0' ? "/"  : "", short_opt
  );
  return buf;
}

void options_init( int argc, char const *argv[], void (*usage)(int) ) {
  ASSERT_RUN_ONCE();
  assert( usage != NULL );

  me = base_name( argv[0] );
  is_wrapc = strcmp( me, PACKAGE "c" ) == 0;

  parse_options(
    argc, argv, OPTS_SHORT[ is_wrapc ], OPTS_LONG[ is_wrapc ],
    CMDLINE_FORBIDDEN_OPTS_SHORT[ is_wrapc ], usage, /*line_no=*/0
  );
  argc -= optind;
  argv += optind;
  if ( argc > 0 ) {
    (*usage)( EX_USAGE );
    unreachable();
  }

  if ( !opt_no_conf && (opt_alias != NULL || opt_fin_name != NULL) ) {
    alias_t const *alias = NULL;
    opt_conf_file = read_conf( opt_conf_file );
    if ( opt_alias != NULL ) {
      if ( (alias = alias_find( opt_alias )) == NULL ) {
        fatal_error( EX_USAGE,
          "\"%s\": no such alias in %s\n",
          opt_alias, opt_conf_file
        );
      }
    }
    else if ( opt_fin_name != NULL ) {
      alias = pattern_find( opt_fin_name );
    }
    if ( alias != NULL ) {
      parse_options(
        alias->argc, alias->argv, OPTS_SHORT[0], OPTS_LONG[0],
        /*cmdline_forbidden_opts=*/"", usage, alias->line_no
      );
    }
  }

  if ( strcmp( fin_path, "-" ) != 0 && !freopen( fin_path, "r", stdin ) )
    fatal_error( EX_NOINPUT, "\"%s\": %s\n", fin_path, STRERROR() );

  if ( strcmp( fout_path, "-" ) != 0 && !freopen( fout_path, "w", stdout ) )
    fatal_error( EX_CANTCREAT, "\"%s\": %s\n", fout_path, STRERROR() );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
