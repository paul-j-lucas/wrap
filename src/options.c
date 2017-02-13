/*
**      wrap -- text reformatter
**      options.c
**
**      Copyright (C) 1996-2017  Paul J. Lucas
**
**      This program is free software; you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the License, or
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
#include <string.h>                     /* for memset(3) */

///////////////////////////////////////////////////////////////////////////////

#define CLEAR_OPTIONS()           memset( opts_given, 0, sizeof opts_given )
#define GAVE_OPTION(OPT)          (opts_given[ (unsigned char)(OPT) ])
#define SET_OPTION(OPT)           (opts_given[ (unsigned char)(OPT) ] = (OPT))

// local constants
static char const   COMMENT_CHARS_DEFAULT[] =
  "!"  ","  // Fortran
  "#"  ","  // AWK, CMake, Julia, Make, Perl, Python, R, Ruby, Shell
  "#=" ","  // Julia
  "#|" ","  // Lisp, Racket, Scheme
  "%"  ","  // Erlang, PostScript, Prolog, TeX
  "(*" ","  // AppleScript, Delphi, ML, Modula-[23], Oberon, OCaml, Pascal
  "(:" ","  // XQuery
  "*>" ","  // COBOL 2002
  "--" ","  // Ada, AppleScript
  "/*" ","  // C, Objective C, C++, C#, D, Go, Java, Rust, Scala, Swift
  "/+" ","  // D
  "//" ","  // C, Objective C, C++, C#, D, Go, Java, Rust, Scala, Swift
  ";"  ","  // Assembly, Clojure, Lisp, Scheme
  "<#" ","  // PowerShell
  ">"  ","  // Quoted e-mail
  "{"  ","  // Pascal
  "{-" ","  // Haskell
  ;

// extern option variables
char const         *opt_alias;
char                opt_align_char;
size_t              opt_align_column;
char const         *opt_comment_chars = COMMENT_CHARS_DEFAULT;
char const         *opt_conf_file;
bool                opt_eos_delimit;
size_t              opt_eos_spaces = EOS_SPACES_DEFAULT;
bool                opt_data_link_esc;
eol_t               opt_eol = EOL_INPUT;
char const         *opt_fin;
char const         *opt_fin_name;
char const         *opt_fout;
size_t              opt_hang_spaces;
size_t              opt_hang_tabs;
size_t              opt_indt_spaces;
size_t              opt_indt_tabs;
bool                opt_lead_dot_ignore;
char const         *opt_lead_para_delims;
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
static unsigned     parse_width( char const* );

///////////////////////////////////////////////////////////////////////////////

#define COMMON_OPTS         "a:b:c:CeE:f:F:l:o:p:s:Tuvw:y"
#define CONF_FORBIDDEN_OPTS "acCfFov"
#define WRAP_SPECIFIC_OPTS  "dh:H:i:I:L:m:M:nNPS:t:WZ"
#define WRAPC_SPECIFIC_OPTS "A:D:"

//
// Each command forbids the others' specific options, but only on the command-
// line and not in the conf file.
//
static char const *const CMDLINE_FORBIDDEN_OPTS[] = {
  WRAPC_SPECIFIC_OPTS,                  // wrap
  WRAP_SPECIFIC_OPTS                    // wrapc
};

static char const *const SHORT_OPTS[] = {
  //
  // wrap's options have to include wrapc's specific options so they're
  // accepted (but ignored) in conf files.
  //
  COMMON_OPTS WRAP_SPECIFIC_OPTS WRAPC_SPECIFIC_OPTS,
  COMMON_OPTS WRAPC_SPECIFIC_OPTS
};

static struct option const WRAP_LONG_OPTS[] = {
  { "alias",                required_argument,  NULL, 'a' },
  { "align-column",         required_argument,  NULL, 'A' },  // conf ignored
  { "block-chars",          required_argument,  NULL, 'b' },
  { "config",               required_argument,  NULL, 'c' },
  { "no-config",            no_argument,        NULL, 'C' },
  { "dot-ignore",           no_argument,        NULL, 'd' },
  { "comment-chars",        required_argument,  NULL, 'D' },  // conf ignored
  { "eos-delimit",          no_argument,        NULL, 'e' },
  { "eos-spaces",           required_argument,  NULL, 'E' },
  { "file",                 required_argument,  NULL, 'f' },
  { "file-name",            required_argument,  NULL, 'F' },
  { "hang-tabs",            required_argument,  NULL, 'h' },
  { "hang-spaces",          required_argument,  NULL, 'H' },
  { "indent-tabs",          required_argument,  NULL, 'i' },
  { "indent-spaces",        required_argument,  NULL, 'I' },
  { "eol",                  required_argument,  NULL, 'l' },
  { "lead-string",          required_argument,  NULL, 'L' },
  { "mirror-tabs",          required_argument,  NULL, 'm' },
  { "mirror-spaces",        required_argument,  NULL, 'M' },
  { "no-newlines-delimit",  no_argument,        NULL, 'n' },
  { "all-newlines-delimit", no_argument,        NULL, 'N' },
  { "output",               required_argument,  NULL, 'o' },
  { "para-chars",           required_argument,  NULL, 'p' },
  { "prototype",            no_argument,        NULL, 'P' },
  { "tab-spaces",           required_argument,  NULL, 's' },
  { "lead-spaces",          required_argument,  NULL, 'S' },
  { "lead-tabs",            required_argument,  NULL, 't' },
  { "title-line",           no_argument,        NULL, 'T' },
  { "markdown",             no_argument,        NULL, 'u' },
  { "version",              no_argument,        NULL, 'v' },
  { "width",                required_argument,  NULL, 'w' },
  { "whitespace-delimit",   no_argument,        NULL, 'W' },
  { "no-hyphen",            no_argument,        NULL, 'y' },
  { "_INTERNAL-DLE",        no_argument,        NULL, 'Z' },
  { NULL,                   0,                  NULL, 0   }
};

static struct option const WRAPC_LONG_OPTS[] = {
  { "alias",                required_argument,  NULL, 'a' },
  { "align-column",         required_argument,  NULL, 'A' },  // not in wrap
  { "block-chars",          required_argument,  NULL, 'b' },
  { "config",               required_argument,  NULL, 'c' },
  { "no-config",            no_argument,        NULL, 'C' },
  { "comment-chars",        required_argument,  NULL, 'D' },  // not in wrap
  { "eos-delimit",          no_argument,        NULL, 'e' },
  { "eos-spaces",           required_argument,  NULL, 'E' },
  { "file",                 required_argument,  NULL, 'f' },
  { "file-name",            required_argument,  NULL, 'F' },
  { "eol",                  required_argument,  NULL, 'l' },
  { "output",               required_argument,  NULL, 'o' },
  { "para-chars",           required_argument,  NULL, 'p' },
  { "tab-spaces",           required_argument,  NULL, 's' },
  { "title-line",           no_argument,        NULL, 'T' },
  { "markdown",             no_argument,        NULL, 'u' },
  { "version",              no_argument,        NULL, 'v' },
  { "width",                required_argument,  NULL, 'w' },
  { "no-hyphen",            no_argument,        NULL, 'y' },
  { NULL,                   0,                  NULL, 0   }
};

static struct option const *const LONG_OPTS[] = {
  WRAP_LONG_OPTS,
  WRAPC_LONG_OPTS
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
  assert( opts1 );
  assert( opts2 );

  unsigned gave_count = 0;
  char const *opt = opts1;
  char gave_opt1 = '\0';

  for ( unsigned i = 0; i < 2; ++i ) {
    for ( ; *opt; ++opt ) {
      if ( GAVE_OPTION( *opt ) ) {
        if ( ++gave_count > 1 ) {
          char const gave_opt2 = *opt;
          char opt1_buf[ OPT_BUF_SIZE ];
          char opt2_buf[ OPT_BUF_SIZE ];
          PMESSAGE_EXIT( EX_USAGE,
            "%s and %s are mutually exclusive\n",
            format_opt( gave_opt1, opt1_buf, sizeof opt1_buf ),
            format_opt( gave_opt2, opt2_buf, sizeof opt2_buf  )
          );
        }
        gave_opt1 = *opt;
        break;
      }
    } // for
    if ( !gave_count )
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
static char const* get_long_opt( char short_opt ) {
  for ( struct option const *long_opt = LONG_OPTS[ is_wrapc ]; long_opt->name;
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
static unsigned parse_align( char const *s, char *align_char ) {
  assert( s );
  assert( align_char );

  static char const *const AUTO  [] = { "a", "auto", NULL };
  static char const *const SPACES[] = { "s", "space", "spaces", NULL };
  static char const *const TABS  [] = { "t", "tab", "tabs", NULL };

  char *end = NULL;
  errno = 0;
  unsigned const col = strtoul( s, &end, 10 );
  if ( errno || end == s )
    goto error;
  if ( *end ) {
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
  return (unsigned)col;

error:
  NO_OP;
  char opt_buf[ OPT_BUF_SIZE ];
  PMESSAGE_EXIT( EX_USAGE,
    "\"%s\": invalid value for %s; %s\n",
    s, format_opt( 'A', opt_buf, sizeof opt_buf ),
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

  assert( s );
  size_t values_buf_size = 1;           // for trailing null

  for ( eol_map_t const *m = EOL_MAP; m->em_name; ++m ) {
    if ( strcasecmp( s, m->em_name ) == 0 )
      return m->em_eol;
    // sum sizes of names in case we need to construct an error message
    values_buf_size += strlen( m->em_name ) + 2 /* ", " */;
  } // for

  // name not found: construct valid name list for an error message
  char *const values_buf = (char*)free_later( MALLOC( char, values_buf_size ) );
  char *pvalues = values_buf;
  for ( eol_map_t const *m = EOL_MAP; m->em_name; ++m ) {
    if ( pvalues > values_buf ) {
      strcpy( pvalues, ", " );
      pvalues += 2;
    }
    strcpy( pvalues, m->em_name );
    pvalues += strlen( m->em_name );
  } // for

  char opt_buf[ OPT_BUF_SIZE ];
  PMESSAGE_EXIT( EX_USAGE,
    "\"%s\": invalid value for %s; must be one of:\n\t%s\n",
    s, format_opt( 'l', opt_buf, sizeof opt_buf ), values_buf
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
                           char const short_opts[],
                           struct option const long_opts[],
                           char const cmdline_forbidden_opts[],
                           void (*usage)(void), unsigned line_no ) {
  assert( usage );

  optind = opterr = 1;
  bool print_version = false;
  CLEAR_OPTIONS();

  for (;;) {
    int opt = getopt_long( argc, (char**)argv, short_opts, long_opts, NULL );
    if ( opt == -1 )
      break;
    SET_OPTION( opt );

    if ( line_no ) {                    // we're parsing a conf file
      if ( strchr( CONF_FORBIDDEN_OPTS, opt ) )
        PMESSAGE_EXIT( EX_CONFIG,
          "%s:%u: '%c': option not allowed in configuration file\n",
          opt_conf_file, line_no, opt
        );
    }
    else if ( strchr( cmdline_forbidden_opts, opt ) ) {
      PRINT_ERR( "%s: invalid option -- '%c'\n", me, opt );
      usage();
    }

    switch ( opt ) {
      case 'a': opt_alias             = optarg;                 break;
      case 'A': opt_align_column      = parse_align( optarg, &opt_align_char );
                                                                break;
      case 'b': opt_lead_para_delims  = optarg;                 break;
      case 'c': opt_conf_file         = optarg;                 break;
      case 'C': opt_no_conf           = true;                   break;
      case 'd': opt_lead_dot_ignore   = true;                   break;
      case 'D': opt_comment_chars     = optarg;                 break;
      case 'e': opt_eos_delimit       = true;                   break;
      case 'E': opt_eos_spaces        = check_atou( optarg );   break;
      case 'f': opt_fin               = optarg;           // no break;
      case 'F': opt_fin_name          = base_name( optarg );    break;
      case 'h': opt_hang_tabs         = check_atou( optarg );   break;
      case 'H': opt_hang_spaces       = check_atou( optarg );   break;
      case 'i': opt_indt_tabs         = check_atou( optarg );   break;
      case 'I': opt_indt_spaces       = check_atou( optarg );   break;
      case 'l': opt_eol               = parse_eol( optarg );    break;
      case 'L': opt_lead_string       = optarg;                 break;
      case 'm': opt_mirror_tabs       = check_atou( optarg );   break;
      case 'M': opt_mirror_spaces     = check_atou( optarg );   break;
      case 'n': opt_newlines_delimit  = SIZE_MAX;               break;
      case 'N': opt_newlines_delimit  = 1;                      break;
      case 'o': opt_fout              = optarg;                 break;
      case 'p': opt_para_delims       = optarg;                 break;
      case 'P': opt_prototype         = true;                   break;
      case 's': opt_tab_spaces        = check_atou( optarg );   break;
      case 'S': opt_lead_spaces       = check_atou( optarg );   break;
      case 't': opt_lead_tabs         = check_atou( optarg );   break;
      case 'T': opt_title_line        = true;                   break;
      case 'u': opt_markdown          = true;                   break;
      case 'v': print_version         = true;                   break;
      case 'w': opt_line_width        = parse_width( optarg );  break;
      case 'W': opt_lead_ws_delimit   = true;                   break;
      case 'y': opt_no_hyphen         = true;                   break;
      case 'Z': opt_data_link_esc     = true;                   break;
      default : usage();
    } // switch
  } // for

  if ( !line_no ) {
    //
    // Check for mutually exclusive options only when parsing the command-line.
    //
    check_mutually_exclusive( "A", "abdeEhHiILmMnNpPTuwWy" );
    check_mutually_exclusive( "f", "F" );
    check_mutually_exclusive( "n", "N" );
    check_mutually_exclusive( "Pu", "dhHiILmMStW" );
    check_mutually_exclusive( "u", "sT" );
    check_mutually_exclusive( "v", "aAbcCdeEfFhHiIlmMnNopsStTuwWy" );
  }

  if ( print_version ) {
    PRINT_ERR( "%s\n", PACKAGE_STRING );
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
static unsigned parse_width( char const *s ) {
#ifdef WITH_WIDTH_TERM
  static char const *const TERM[] = {
    "t",
    "term",
    "terminal",
    NULL
  };

  assert( s );
  if ( is_digits( s ) )
    return (unsigned)strtoul( s, NULL, 10 );

  size_t values_buf_size = 1;           // for trailing null
  for ( char const *const *t = TERM; *t; ++t ) {
    if ( strcasecmp( s, *t ) == 0 )
      return get_term_columns();
    // sum sizes of values in case we need to construct an error message
    values_buf_size += strlen( *t ) + 2 /* ", " */;
  } // for

  // value not found: construct valid value list for an error message
  char *const values_buf = (char*)free_later( MALLOC( char, values_buf_size ) );
  char *pvalues = values_buf;
  for ( char const *const *t = TERM; *t; ++t ) {
    if ( pvalues > values_buf ) {
      strcpy( pvalues, ", " );
      pvalues += 2;
    }
    strcpy( pvalues, *t );
    pvalues += strlen( *t );
  } // for

  char opt_buf[ OPT_BUF_SIZE ];
  PMESSAGE_EXIT( EX_USAGE,
    "\"%s\": invalid value for %s; must be one of:\n\t%s\n",
    s, format_opt( 'w', opt_buf, sizeof opt_buf ), values_buf
  );
#else
  return check_atou( s );
#endif /* WITH_WIDTH_TERM */
}

////////// extern functions ///////////////////////////////////////////////////

char* format_opt( char short_opt, char buf[], size_t size ) {
  char const *const long_opt = get_long_opt( short_opt );
  snprintf(
    buf, size, "%s%s%s-%c",
    *long_opt ? "--" : "", long_opt, *long_opt ? "/" : "", short_opt
  );
  return buf;
}

void options_init( int argc, char const *argv[], void (*usage)(void) ) {
  assert( usage );
  me = base_name( argv[0] );
  is_wrapc = strcmp( me, PACKAGE "c" ) == 0;

  parse_options(
    argc, argv, SHORT_OPTS[ is_wrapc ], LONG_OPTS[ is_wrapc ],
    CMDLINE_FORBIDDEN_OPTS[ is_wrapc ], usage, 0
  );
  argc -= optind, argv += optind;
  if ( argc )
    usage();

  if ( !opt_no_conf && (opt_alias || opt_fin_name) ) {
    alias_t const *alias = NULL;
    opt_conf_file = read_conf( opt_conf_file );
    if ( opt_alias ) {
      if ( !(alias = alias_find( opt_alias )) )
        PMESSAGE_EXIT( EX_USAGE,
          "\"%s\": no such alias in %s\n",
          opt_alias, opt_conf_file
        );
    }
    else if ( opt_fin_name )
      alias = pattern_find( opt_fin_name );
    if ( alias )
      parse_options(
        alias->argc, alias->argv, SHORT_OPTS[0], LONG_OPTS[0], "", usage,
        alias->line_no
      );
  }

  if ( opt_fin && !(fin = fopen( opt_fin, "r" )) )
    PMESSAGE_EXIT( EX_NOINPUT, "\"%s\": %s\n", opt_fin, STRERROR );
  if ( opt_fout && !(fout = fopen( opt_fout, "w" )) )
    PMESSAGE_EXIT( EX_CANTCREAT, "\"%s\": %s\n", opt_fout, STRERROR );

  if ( !fin )
    fin = stdin;
  if ( !fout )
    fout = stdout;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
