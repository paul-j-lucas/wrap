/*
**      wrap -- text reformatter
**      src/options.c
**
**      Copyright (C) 1996-2019  Paul J. Lucas
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

// local
#include "pjl_config.h"                 /* must go first */
#include "options.h"
#include "alias.h"
#include "common.h"
#include "pattern.h"
#include "read_conf.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>                   /* for SIZE_MAX */
#include <stdbool.h>
#include <string.h>                     /* for memset(3) */

///////////////////////////////////////////////////////////////////////////////

#define CLEAR_OPTIONS()     memset( opts_given, 0, sizeof opts_given )
#define GAVE_OPTION(OPT)    (opts_given[ (unsigned char)(OPT) ])
#define SET_OPTION(OPT)     (opts_given[ (unsigned char)(OPT) ] = (char)(OPT))

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
            //    Rust, Scala, Swift
  ";"  ","  // Assembly, Clojure, Lisp, Scheme
  "<#" ","  // PowerShell
  ">"  ","  // Quoted e-mail
  "{"  ","  // Pascal
  "{-" ","  // Haskell
  "\\" ","  // Forth
  ;

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
char const         *opt_fin;
char const         *opt_fin_name;
char const         *opt_fout;
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

// other extern variables
FILE               *fin;
FILE               *fout;

// local variables
static bool         is_wrapc;           // are we wrapc?
static char         opts_given[ 128 ];

// local functions
NODISCARD
static unsigned     parse_width( char const* );

///////////////////////////////////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
#define SOPT_NO_ARGUMENT          /* nothing */
#define SOPT_REQUIRED_ARGUMENT    ":"
/// @endcond

#define COMMON_OPTS                                   \
  SOPT(ALIAS)                 SOPT_REQUIRED_ARGUMENT  \
  SOPT(BLOCK_REGEX)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(CONFIG)                SOPT_REQUIRED_ARGUMENT  \
  SOPT(DOXYGEN)               SOPT_NO_ARGUMENT        \
  SOPT(EOL)                   SOPT_REQUIRED_ARGUMENT  \
  SOPT(EOS_DELIMIT)           SOPT_NO_ARGUMENT        \
  SOPT(EOS_SPACES)            SOPT_REQUIRED_ARGUMENT  \
  SOPT(FILE)                  SOPT_REQUIRED_ARGUMENT  \
  SOPT(FILE_NAME)             SOPT_REQUIRED_ARGUMENT  \
  SOPT(MARKDOWN)              SOPT_NO_ARGUMENT        \
  SOPT(NO_CONFIG)             SOPT_NO_ARGUMENT        \
  SOPT(NO_HYPHEN)             SOPT_NO_ARGUMENT        \
  SOPT(OUTPUT)                SOPT_REQUIRED_ARGUMENT  \
  SOPT(PARA_CHARS)            SOPT_REQUIRED_ARGUMENT  \
  SOPT(TAB_SPACES)            SOPT_REQUIRED_ARGUMENT  \
  SOPT(TITLE_LINE)            SOPT_NO_ARGUMENT        \
  SOPT(VERSION)               SOPT_NO_ARGUMENT        \
  SOPT(WIDTH)                 SOPT_REQUIRED_ARGUMENT

#define CONF_FORBIDDEN_OPTS \
  SOPT(ALIAS)               \
  SOPT(CONFIG)              \
  SOPT(FILE)                \
  SOPT(FILE_NAME)           \
  SOPT(NO_CONFIG)           \
  SOPT(OUTPUT)              \
  SOPT(VERSION)

#define WRAP_SPECIFIC_OPTS                            \
  SOPT(DOT_IGNORE)            SOPT_NO_ARGUMENT        \
  SOPT(HANG_TABS)             SOPT_REQUIRED_ARGUMENT  \
  SOPT(HANG_SPACES)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(INDENT_TABS)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(INDENT_SPACES)         SOPT_REQUIRED_ARGUMENT  \
  SOPT(LEAD_STRING)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(MIRROR_TABS)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(MIRROR_SPACES)         SOPT_REQUIRED_ARGUMENT  \
  SOPT(NO_NEWLINES_DELIMIT)   SOPT_NO_ARGUMENT        \
  SOPT(ALL_NEWLINES_DELIMIT)  SOPT_NO_ARGUMENT        \
  SOPT(PROTOTYPE)             SOPT_NO_ARGUMENT        \
  SOPT(LEAD_SPACES)           SOPT_REQUIRED_ARGUMENT  \
  SOPT(LEAD_TABS)             SOPT_REQUIRED_ARGUMENT  \
  SOPT(WHITESPACE_DELIMIT)    SOPT_NO_ARGUMENT        \
  SOPT(ENABLE_IPC)            SOPT_NO_ARGUMENT

#define WRAPC_SPECIFIC_OPTS                           \
  SOPT(ALIGN_COLUMN)          SOPT_REQUIRED_ARGUMENT  \
  SOPT(COMMENT_CHARS)         SOPT_REQUIRED_ARGUMENT

//
// Each command forbids the others' specific options, but only on the command-
// line and not in the conf file.
//
static char const *const CMDLINE_FORBIDDEN_OPTS[] = {
  WRAPC_SPECIFIC_OPTS,                  // wrap
  WRAP_SPECIFIC_OPTS                    // wrapc
};

static char const *const OPTS_SHORT[] = {
  //
  // wrap's options have to include wrapc's specific options so they're
  // accepted (but ignored) in conf files.
  //
  ":" COMMON_OPTS WRAP_SPECIFIC_OPTS WRAPC_SPECIFIC_OPTS, // wrap
  ":" COMMON_OPTS WRAPC_SPECIFIC_OPTS                     // wrapc
};

static struct option const WRAP_OPTS_LONG[] = {
  { "alias",                required_argument,  NULL, COPT(ALIAS)           },
  { "align-column",         required_argument,  NULL, COPT(ALIGN_COLUMN)    },
  { "block-regex",          required_argument,  NULL, COPT(BLOCK_REGEX)     },
  { "config",               required_argument,  NULL, COPT(CONFIG)          },
  { "no-config",            no_argument,        NULL, COPT(NO_CONFIG)       },
  { "dot-ignore",           no_argument,        NULL, COPT(DOT_IGNORE)      },
  { "comment-chars",        required_argument,  NULL, COPT(COMMENT_CHARS)   },
  { "eos-delimit",          no_argument,        NULL, COPT(EOS_DELIMIT)     },
  { "eos-spaces",           required_argument,  NULL, COPT(EOS_SPACES)      },
  { "file",                 required_argument,  NULL, COPT(FILE)            },
  { "file-name",            required_argument,  NULL, COPT(FILE_NAME)       },
  { "hang-tabs",            required_argument,  NULL, COPT(HANG_TABS)       },
  { "hang-spaces",          required_argument,  NULL, COPT(HANG_SPACES)     },
  { "indent-tabs",          required_argument,  NULL, COPT(INDENT_TABS)     },
  { "indent-spaces",        required_argument,  NULL, COPT(INDENT_SPACES)   },
  { "eol",                  required_argument,  NULL, COPT(EOL)             },
  { "lead-string",          required_argument,  NULL, COPT(LEAD_STRING)     },
  { "mirror-tabs",          required_argument,  NULL, COPT(MIRROR_TABS)     },
  { "mirror-spaces",        required_argument,  NULL, COPT(MIRROR_SPACES)   },
  { "no-newlines-delimit",  no_argument,        NULL, COPT(NO_NEWLINES_DELIMIT) },
  { "all-newlines-delimit", no_argument,        NULL, COPT(ALL_NEWLINES_DELIMIT) },
  { "output",               required_argument,  NULL, COPT(OUTPUT)          },
  { "para-chars",           required_argument,  NULL, COPT(PARA_CHARS)      },
  { "prototype",            no_argument,        NULL, COPT(PROTOTYPE)       },
  { "tab-spaces",           required_argument,  NULL, COPT(TAB_SPACES)      },
  { "lead-spaces",          required_argument,  NULL, COPT(LEAD_SPACES)     },
  { "lead-tabs",            required_argument,  NULL, COPT(LEAD_TABS)       },
  { "title-line",           no_argument,        NULL, COPT(TITLE_LINE)      },
  { "markdown",             no_argument,        NULL, COPT(MARKDOWN)        },
  { "version",              no_argument,        NULL, COPT(VERSION)         },
  { "width",                required_argument,  NULL, COPT(WIDTH)           },
  { "whitespace-delimit",   no_argument,        NULL, COPT(WHITESPACE_DELIMIT) },
  { "doxygen",              no_argument,        NULL, COPT(DOXYGEN)         },
  { "no-hyphen",            no_argument,        NULL, COPT(NO_HYPHEN)       },
  { "_ENABLE-IPC",          no_argument,        NULL, COPT(ENABLE_IPC)      },
  { NULL,                   0,                  NULL, 0   }
};

static struct option const WRAPC_OPTS_LONG[] = {
  { "alias",                required_argument,  NULL, COPT(ALIAS)         },
  { "align-column",         required_argument,  NULL, COPT(ALIGN_COLUMN)  },
  { "block-regex",          required_argument,  NULL, COPT(BLOCK_REGEX)   },
  { "config",               required_argument,  NULL, COPT(CONFIG)        },
  { "no-config",            no_argument,        NULL, COPT(NO_CONFIG)     },
  { "comment-chars",        required_argument,  NULL, COPT(COMMENT_CHARS) },
  { "eos-delimit",          no_argument,        NULL, COPT(EOS_DELIMIT)   },
  { "eos-spaces",           required_argument,  NULL, COPT(EOS_SPACES)    },
  { "file",                 required_argument,  NULL, COPT(FILE)          },
  { "file-name",            required_argument,  NULL, COPT(FILE_NAME)     },
  { "eol",                  required_argument,  NULL, COPT(EOL)           },
  { "output",               required_argument,  NULL, COPT(OUTPUT)        },
  { "para-chars",           required_argument,  NULL, COPT(PARA_CHARS)    },
  { "tab-spaces",           required_argument,  NULL, COPT(TAB_SPACES)    },
  { "title-line",           no_argument,        NULL, COPT(TITLE_LINE)    },
  { "markdown",             no_argument,        NULL, COPT(MARKDOWN)      },
  { "version",              no_argument,        NULL, COPT(VERSION)       },
  { "width",                required_argument,  NULL, COPT(WIDTH)         },
  { "doxygen",              no_argument,        NULL, COPT(DOXYGEN)       },
  { "no-hyphen",            no_argument,        NULL, COPT(NO_HYPHEN)     },
  { NULL,                   0,                  NULL, 0   }
};

static struct option const *const OPTS_LONG[] = {
  WRAP_OPTS_LONG,
  WRAPC_OPTS_LONG
};

////////// local functions ////////////////////////////////////////////////////

/**
 * Checks that no options were given that are among the two given mutually
 * exclusive sets of short options.
 * Prints an error message and exits if any such options are found.
 *
 * @param opts1 The first set of short options.
 * @param opts2 The second set of short options.
 */
static void check_mutually_exclusive( char const *opts1, char const *opts2 ) {
  assert( opts1 != NULL );
  assert( opts2 != NULL );

  unsigned gave_count = 0;
  char const *opt = opts1;
  char gave_opt1 = '\0';

  for ( unsigned i = 0; i < 2; ++i ) {
    for ( ; *opt != '\0'; ++opt ) {
      if ( GAVE_OPTION( *opt ) ) {
        if ( ++gave_count > 1 ) {
          char const gave_opt2 = *opt;
          char opt1_buf[ OPT_BUF_SIZE ];
          char opt2_buf[ OPT_BUF_SIZE ];
          fatal_error( EX_USAGE,
            "%s and %s are mutually exclusive\n",
            opt_format( gave_opt1, opt1_buf, sizeof opt1_buf ),
            opt_format( gave_opt2, opt2_buf, sizeof opt2_buf  )
          );
        }
        gave_opt1 = *opt;
        break;
      }
    } // for
    if ( gave_count == 0 )
      break;
    opt = opts2;
  } // for
}

/**
 * Gets the corresponding name of the long option for the given short option.
 *
 * @param short_opt The short option to get the corresponding long option for.
 * @return Returns the said option or the empty string if none.
 */
NODISCARD
static char const* get_long_opt( char short_opt ) {
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
  unsigned const col = (unsigned)strtoul( s, &end, 10 );
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
  NO_OP;
  char opt_buf[ OPT_BUF_SIZE ];
  fatal_error( EX_USAGE,
    "\"%s\": invalid value for %s; %s\n",
    s, opt_format( 'A', opt_buf, sizeof opt_buf ),
    "must be digits followed by one of:"
    " a, auto, s, space, spaces, t, tab, tabs"
  );
}

/**
 * Parses an End-of-Line value.
 *
 * @param s The null-terminated string to parse.
 * @return Returns the corresponding \c eol_t
 * or prints an error message and exits if \a s is invalid.
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
  char values_buf[ values_buf_size ];
  char *pvalues = values_buf;
  for ( eol_map_t const *m = EOL_MAP; m->em_name != NULL; ++m ) {
    if ( pvalues > values_buf ) {
      strcpy( pvalues, ", " );
      pvalues += 2;
    }
    strcpy( pvalues, m->em_name );
    pvalues += strlen( m->em_name );
  } // for

  char opt_buf[ OPT_BUF_SIZE ];
  fatal_error( EX_USAGE,
    "\"%s\": invalid value for %s; must be one of:\n\t%s\n",
    s, opt_format( 'l', opt_buf, sizeof opt_buf ), values_buf
  );
}

/**
 * Parses command-line options.
 *
 * @param argc The argument count from \c main().
 * @param argv The argument values from \c main().
 * @param short_opts The set of short options.
 * @param long_opts The set of long options.
 * @param cmdline_forbidden_opts The set of options that are forbidden on the
 * command line.
 * @param usage A pointer to a function that prints a usage message and exits.
 * @param line_no When parsing options from a configuration file, the
 * originating line number; zero otherwise.
 */
static void parse_options( int argc, char const *argv[],
                           char const short_opts[const],
                           struct option const long_opts[const],
                           char const cmdline_forbidden_opts[const],
                           void (*usage)(int), unsigned line_no ) {
  assert( usage != NULL );

  optind = opterr = 1;
  bool print_version = false;
  CLEAR_OPTIONS();

  for (;;) {
    int const opt = getopt_long(
      argc, CONST_CAST( char**, argv ), short_opts, long_opts,
      /*longindex=*/NULL
    );
    if ( opt == -1 )
      break;
    SET_OPTION( opt );

    if ( line_no > 0 ) {                // we're parsing a conf file
      if ( strchr( CONF_FORBIDDEN_OPTS, opt ) != NULL )
        fatal_error( EX_CONFIG,
          "%s:%u: '%c': option not allowed in configuration file\n",
          opt_conf_file, line_no, opt
        );
    }
    else if ( strchr( cmdline_forbidden_opts, opt ) != NULL ) {
      fatal_error( EX_USAGE, "%s: invalid option -- '%c'\n", me, opt );
    }

    switch ( opt ) {
      case COPT(ALIAS):
        opt_alias = optarg;
        break;
      case COPT(ALIGN_COLUMN):
        opt_align_column = parse_align( optarg, &opt_align_char );
        break;
      case COPT(ALL_NEWLINES_DELIMIT):
        opt_newlines_delimit  = 1;
        break;
      case COPT(BLOCK_REGEX):
        opt_block_regex = optarg;
        break;
      case COPT(COMMENT_CHARS):
        opt_comment_chars = optarg;
        break;
      case COPT(CONFIG):
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
        opt_fin = optarg;
        FALLTHROUGH;
      case COPT(FILE_NAME):
        opt_fin_name = base_name( optarg );
        break;
      case COPT(HANG_TABS):
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
        opt_newlines_delimit  = SIZE_MAX;
        break;
      case COPT(OUTPUT):
        opt_fout = optarg;
        break;
      case COPT(PARA_CHARS):
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
        print_version = true;
        break;
      case COPT(WHITESPACE_DELIMIT):
        opt_lead_ws_delimit = true;
        break;
      case COPT(WIDTH):
        opt_line_width = parse_width( optarg );
        break;

      case ':': {                       // option missing required argument
        char opt_buf[ OPT_BUF_SIZE ];
        fatal_error( EX_USAGE,
          "\"%s\" requires an argument\n",
          opt_format( STATIC_CAST( char, optopt ), opt_buf, sizeof opt_buf )
        );
      }

      case '?':                         // invalid option
        fatal_error( EX_USAGE,
          "'%c': invalid option\n", STATIC_CAST( char, optopt )
        );

      default:
        if ( isprint( opt ) )
          INTERNAL_ERROR(
            "'%c': unaccounted-for getopt_long() return value\n", opt
          );
        INTERNAL_ERROR(
          "%d: unaccounted-for getopt_long() return value\n", opt
        );
    } // switch
  } // for

  if ( line_no == 0 ) {
    //
    // Check for mutually exclusive options only when parsing the command-line.
    //
    check_mutually_exclusive( SOPT(ALIGN_COLUMN),
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
    check_mutually_exclusive( SOPT(ALL_NEWLINES_DELIMIT),
      SOPT(NO_NEWLINES_DELIMIT)
    );
    check_mutually_exclusive( SOPT(FILE), SOPT(FILE_NAME) );
    check_mutually_exclusive( SOPT(MARKDOWN),
      SOPT(TAB_SPACES)
      SOPT(TITLE_LINE)
    );
    check_mutually_exclusive( SOPT(MARKDOWN) SOPT(PROTOTYPE),
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
    check_mutually_exclusive( SOPT(VERSION),
      SOPT(ALIAS)
      SOPT(ALIGN_COLUMN)
      SOPT(ALL_NEWLINES_DELIMIT)
      SOPT(BLOCK_REGEX)
      SOPT(CONFIG)
      SOPT(DOT_IGNORE)
      SOPT(DOXYGEN)
      SOPT(EOL)
      SOPT(EOS_DELIMIT)
      SOPT(EOS_SPACES)
      SOPT(FILE)
      SOPT(FILE_NAME)
      SOPT(HANG_SPACES)
      SOPT(HANG_TABS)
      SOPT(INDENT_SPACES)
      SOPT(INDENT_TABS)
      SOPT(LEAD_SPACES)
      SOPT(LEAD_TABS)
      SOPT(MARKDOWN)
      SOPT(MIRROR_SPACES)
      SOPT(MIRROR_TABS)
      SOPT(NO_CONFIG)
      SOPT(NO_HYPHEN)
      SOPT(NO_NEWLINES_DELIMIT)
      SOPT(OUTPUT)
      SOPT(PARA_CHARS)
      SOPT(TAB_SPACES)
      SOPT(TITLE_LINE)
      SOPT(WHITESPACE_DELIMIT)
      SOPT(WIDTH)
    );
  }

  if ( print_version ) {
    EPRINTF( "%s\n", PACKAGE_STRING );
    exit( EX_OK );
  }
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
    return (unsigned)strtoul( s, NULL, 10 );

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

  char opt_buf[ OPT_BUF_SIZE ];
  fatal_error( EX_USAGE,
    "\"%s\": invalid value for %s; must be one of:\n\t%s\n",
    s, opt_format( 'w', opt_buf, sizeof opt_buf ), values_buf
  );
#else
  return check_atou( s );
#endif /* WITH_WIDTH_TERM */
}

////////// extern functions ///////////////////////////////////////////////////

char* opt_format( char short_opt, char buf[], size_t size ) {
  char const *const long_opt = get_long_opt( short_opt );
  snprintf(
    buf, size, "%s%s%s-%c",
    *long_opt != '\0' ? "--" : "", long_opt,
    *long_opt != '\0' ? "/"  : "", short_opt
  );
  return buf;
}

void options_init( int argc, char const *argv[], void (*usage)(int) ) {
  assert( usage != NULL );
  me = base_name( argv[0] );
  is_wrapc = strcmp( me, PACKAGE "c" ) == 0;

  parse_options(
    argc, argv, OPTS_SHORT[ is_wrapc ], OPTS_LONG[ is_wrapc ],
    CMDLINE_FORBIDDEN_OPTS[ is_wrapc ], usage, 0
  );
  argc -= optind;
  argv += optind;
  if ( argc > 0 )
    usage( EX_USAGE );

  if ( !opt_no_conf && (opt_alias != NULL || opt_fin_name != NULL) ) {
    alias_t const *alias = NULL;
    opt_conf_file = read_conf( opt_conf_file );
    if ( opt_alias != NULL ) {
      if ( (alias = alias_find( opt_alias )) == NULL )
        fatal_error( EX_USAGE,
          "\"%s\": no such alias in %s\n",
          opt_alias, opt_conf_file
        );
    }
    else if ( opt_fin_name != NULL ) {
      alias = pattern_find( opt_fin_name );
    }
    if ( alias != NULL )
      parse_options(
        alias->argc, alias->argv, OPTS_SHORT[0], OPTS_LONG[0],
        /*cmdline_forbidden_opts=*/"", usage, alias->line_no
      );
  }

  if ( opt_fin != NULL && (fin = fopen( opt_fin, "r" )) == NULL )
    fatal_error( EX_NOINPUT, "\"%s\": %s\n", opt_fin, STRERROR );
  if ( opt_fout != NULL && (fout = fopen( opt_fout, "w" )) == NULL )
    fatal_error( EX_CANTCREAT, "\"%s\": %s\n", opt_fout, STRERROR );

  if ( fin == NULL )
    fin = stdin;
  if ( fout == NULL )
    fout = stdout;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
