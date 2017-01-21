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

#define CLEAR_OPTIONS()     memset( opts_given, 0, sizeof opts_given )
#define GAVE_OPTION(OPT)    isalpha( OPTION_VALUE(OPT) )
#define OPTION_VALUE(OPT)   opts_given[ !islower(OPT) ][ toupper(OPT) - 'A' ]
#define SET_OPTION(OPT)     OPTION_VALUE(OPT) = (OPT)

typedef char opts_given_t[ 2 /* lower/upper */ ][ 26 + 1/*null*/ ];

// extern constants
char const          COMMENT_CHARS_DEFAULT[] =
    // Each character should appear only once.
    "#"   //    AWK, CMake, Julia, Make, Perl, Python, R, Ruby, Shell
    "/*"  //    C, Objective C, C++, C#, D, Go, Java, Rust, Scala, Swift
    "+"   // /+ D
    "-"   // -- Ada, AppleScript
    "(:"  //    XQuery
  // (*   //    AppleScript, Delphi, ML, Modula-[23], Oberon, OCaml, Pascal
    "{"   // {- Haskell
    "}"   //    Pascal
    "!"   //    Fortran
    "%"   //    Erlang, PostScript, Prolog, TeX
    ";"   //    Assembly, Clojure, Lisp, Scheme
    "<"   // <# PowerShell
    "="   // #= Julia
    ">"   //    Quoted e-mail
  // *>"  //    COBOL 2002
    "|"   // |# Lisp, Racket, Scheme
    ;

// extern option variables
char const         *opt_alias;
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
static opts_given_t opts_given;         // options given

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

  int gave_count = 0;
  char const *opt = opts1;
  char gave_opt1 = '\0';

  for ( int i = 0; i < 2; ++i ) {
    for ( ; *opt; ++opt ) {
      if ( GAVE_OPTION( *opt ) ) {
        if ( ++gave_count > 1 ) {
          char const gave_opt2 = *opt;
          PMESSAGE_EXIT( EX_USAGE,
            "-%c and -%c are mutually exclusive\n", gave_opt1, gave_opt2
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

  static eol_map_t const eol_map[] = {
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
  char const *const s_lc = tolower_s( (char*)free_later( check_strdup( s ) ) );
  size_t names_buf_size = 1;            // for trailing null

  for ( eol_map_t const *m = eol_map; m->em_name; ++m ) {
    if ( strcmp( s_lc, m->em_name ) == 0 )
      return m->em_eol;
    // sum sizes of names in case we need to construct an error message
    names_buf_size += strlen( m->em_name ) + 2 /* ", " */;
  } // for

  // name not found: construct valid name list for an error message
  char *const names_buf = (char*)free_later( MALLOC( char, names_buf_size ) );
  char *pnames = names_buf;
  for ( eol_map_t const *m = eol_map; m->em_name; ++m ) {
    if ( pnames > names_buf ) {
      strcpy( pnames, ", " );
      pnames += 2;
    }
    strcpy( pnames, m->em_name );
    pnames += strlen( m->em_name );
  } // for
  PMESSAGE_EXIT( EX_USAGE,
    "\"%s\": invalid value for -%c; must be one of:\n\t%s\n",
    s, 'l', names_buf
  );
}

/**
 * Parses command-line options.
 *
 * @param argc The argument count from \c main().
 * @param argv The argument values from \c main().
 * @param opts The set of options as required by \c getopt(3).
 * @param usage A pointer to a function that prints a usage message and exits.
 * @param line_no When parsing options from a configuration file, the
 * originating line number; zero otherwise.
 */
static void parse_options( int argc, char const *argv[], char const *opts,
                           void (*usage)(void), unsigned line_no ) {
  assert( opts );

  optind = opterr = 1;
  bool print_version = false;
  CLEAR_OPTIONS();

  for ( int opt; (opt = getopt( argc, (char**)argv, opts )) != EOF; ) {
    SET_OPTION( opt );
    if ( line_no && strchr( "acCfFov", opt ) )
      PMESSAGE_EXIT( EX_CONFIG,
        "%s:%u: '%c': option not allowed in configuration file\n",
        opt_conf_file, line_no, opt
      );
    switch ( opt ) {
      case 'a': opt_alias             = optarg;               break;
      case 'b': opt_lead_para_delims  = optarg;               break;
      case 'c': opt_conf_file         = optarg;               break;
      case 'C': opt_no_conf           = true;                 break;
      case 'd': opt_lead_dot_ignore   = true;                 break;
      case 'D': opt_comment_chars     = optarg;               break;
      case 'e': opt_eos_delimit       = true;                 break;
      case 'E': opt_eos_spaces        = check_atou( optarg ); break;
      case 'f': opt_fin               = optarg;         // no break;
      case 'F': opt_fin_name          = base_name( optarg );  break;
      case 'h': opt_hang_tabs         = check_atou( optarg ); break;
      case 'H': opt_hang_spaces       = check_atou( optarg ); break;
      case 'i': opt_indt_tabs         = check_atou( optarg ); break;
      case 'I': opt_indt_spaces       = check_atou( optarg ); break;
      case 'l': opt_eol               = parse_eol( optarg );  break;
      case 'L': opt_lead_string       = optarg;               break;
      case 'm': opt_mirror_tabs       = check_atou( optarg ); break;
      case 'M': opt_mirror_spaces     = check_atou( optarg ); break;
      case 'n': opt_newlines_delimit  = SIZE_MAX;             break;
      case 'N': opt_newlines_delimit  = 1;                    break;
      case 'o': opt_fout              = optarg;               break;
      case 'p': opt_para_delims       = optarg;               break;
      case 'P': opt_prototype         = true;                 break;
      case 's': opt_tab_spaces        = check_atou( optarg ); break;
      case 'S': opt_lead_spaces       = check_atou( optarg ); break;
      case 't': opt_lead_tabs         = check_atou( optarg ); break;
      case 'T': opt_title_line        = true;                 break;
      case 'u': opt_markdown          = true;                 break;
      case 'v': print_version         = true;                 break;
      case 'w': opt_line_width        = check_atou( optarg ); break;
      case 'W': opt_lead_ws_delimit   = true;                 break;
      case 'y': opt_no_hyphen         = true;                 break;
      case 'Z': opt_data_link_esc     = true;                 break;
      default : usage();
    } // switch
  } // for

  check_mutually_exclusive( "f", "F" );
  check_mutually_exclusive( "n", "N" );
  check_mutually_exclusive( "Pu", "dhHiILmMStW" );
  check_mutually_exclusive( "u", "sT" );
  check_mutually_exclusive( "v", "abcCdeEfFhHiIlmMnNopsStTuwWy" );

  if ( print_version ) {
    PRINT_ERR( "%s\n", PACKAGE_STRING );
    exit( EX_OK );
  }
}

////////// extern functions ///////////////////////////////////////////////////

#define COMMON_OPTS         "a:b:c:CeE:f:F:l:o:p:s:Tuvw:y"

void init_options( int argc, char const *argv[], void (*usage)(void) ) {
  static char const WRAP_OPTS [] = COMMON_OPTS "dh:H:i:I:L:m:M:nNPS:t:WZ";
  static char const WRAPC_OPTS[] = COMMON_OPTS "D:";

  assert( usage );
  me = base_name( argv[0] );
  bool const is_wrap = strcmp( me, PACKAGE ) == 0;

  parse_options( argc, argv, is_wrap ? WRAP_OPTS : WRAPC_OPTS, usage, 0 );
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
        alias->argc, alias->argv, WRAP_OPTS, usage, alias->line_no
      );
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

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
