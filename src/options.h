/*
**      wrap -- text reformatter
**      options.h
**
**      Copyright (C) 1996-2016  Paul J. Lucas
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

#ifndef wrap_options_H
#define wrap_options_H

// standard
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////

// Each character should appear only once.
#define COMMENT_CHARS_DEFAULT \
  "#"   /*    AWK, CMake, Julia, Make, Perl, Python, R, Ruby, Shell       */ \
  "/*"  /*    C, Objective C, C++, C#, D, Go, Java, Rust, Scala, Swift    */ \
  "+"   /* /+ D                                                           */ \
  "-"   /* -- Ada, AppleScript                                            */ \
  "(:"  /*    XQuery                                                      */ \
/* (*   |*    AppleScript, Delphi, ML, Modula-[23], Oberon, OCaml, Pascal */ \
  "{"   /* {- Haskell                                                     */ \
  "}"   /*    Pascal                                                      */ \
  "!"   /*    Fortran                                                     */ \
  "%"   /*    Erlang, PostScript, Prolog, TeX                             */ \
  ";"   /*    Assembly, Clojure, Lisp, Scheme                             */ \
  "<"   /* <# PowerShell                                                  */ \
  "="   /* #= Julia                                                       */ \
  ">"   /*    Quoted e-mail                                               */ \
/* *>"  |*    COBOL 2002                                                  */ \
  "|"   /* |# Lisp, Racket, Scheme                                        */

/**
 * End-of-Line formats.
 */
enum eol {
  EOL_INPUT   = 'i',                    // do whatever the input does
  EOL_UNIX    = 'u',
  EOL_WINDOWS = 'w'
};
typedef enum eol eol_t;

// extern option variables
extern char const  *opt_alias;
extern char const  *opt_comment_chars;
extern char const  *opt_conf_file;
extern bool         opt_data_link_esc;  // respond to in-band control
extern eol_t        opt_eol;
extern bool         opt_eos_delimit;    // end-of-sentence delimits para's?
extern char const  *opt_fin;            // file in path
extern char const  *opt_fin_name;       // file in name (only)
extern char const  *opt_fout;           // file out path
extern size_t       opt_hang_spaces;    // hanging-indent spaces
extern size_t       opt_hang_tabs;      // hanging-indent tabs
extern size_t       opt_indt_spaces;    // indent spaces
extern size_t       opt_indt_tabs;      // indent tabs
extern bool         opt_lead_dot_ignore;// ignore lines starting with '.'?
extern char const  *opt_lead_para_delims;
extern size_t       opt_lead_spaces;
extern char const  *opt_lead_string;
extern size_t       opt_lead_tabs;
extern bool         opt_lead_ws_delimit;// leading whitespace delimit para's?
extern size_t       opt_line_width;
extern bool         opt_markdown;
extern size_t       opt_mirror_spaces;
extern size_t       opt_mirror_tabs;
extern size_t       opt_newlines_delimit;
extern bool         opt_no_conf;        // do not read conf file
extern bool         opt_no_hyphen;      // do not treat hyphens specially
extern char const  *opt_para_delims;    // additional para delimiter chars
extern bool         opt_prototype;      // first line whitespace is prototype?
extern size_t       opt_tab_spaces;
extern bool         opt_title_line;     // 1st para line is title?

// other extern variables
extern FILE        *fin;                // file in
extern FILE        *fout;               // file out

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes command-line option variables.
 *
 * @param argc The argument count from \c main().
 * @param argv The argument values from \c main().
 * @param usage A pointer to a function to print a usage message.  It must not
 * return.
 */
void init_options( int argc, char const *argv[], void (*usage)(void) );

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_options_H */
/* vim:set et sw=2 ts=2: */
