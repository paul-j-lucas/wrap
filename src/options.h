/*
**      wrap -- text reformatter
**      src/options.h
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

#ifndef wrap_options_H
#define wrap_options_H

/**
 * @file
 * Contains constants, enums, and variables for all wrap and wrapc command-line
 * and configuration file options as well as a function to read and parse them.
 */

// local
#include "pjl_config.h"                 /* must go first */

// standard
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>                      /* for FILE */

///////////////////////////////////////////////////////////////////////////////

#define OPT_ALIAS                 a
#define OPT_ALIGN_COLUMN          A
#define OPT_BLOCK_REGEX           b
#define OPT_CONFIG                c
#define OPT_NO_CONFIG             C
#define OPT_DOT_IGNORE            d
#define OPT_COMMENT_CHARS         D
#define OPT_EOS_DELIMIT           e
#define OPT_EOS_SPACES            E
#define OPT_FILE                  f
#define OPT_FILE_NAME             F
#define OPT_HANG_TABS             h
#define OPT_HANG_SPACES           H
#define OPT_INDENT_TABS           i
#define OPT_INDENT_SPACES         I
#define OPT_EOL                   l
#define OPT_LEAD_STRING           L
#define OPT_MIRROR_TABS           m
#define OPT_MIRROR_SPACES         M
#define OPT_NO_NEWLINES_DELIMIT   n
#define OPT_ALL_NEWLINES_DELIMIT  N
#define OPT_OUTPUT                o
#define OPT_PARA_CHARS            p
#define OPT_PROTOTYPE             P
#define OPT_TAB_SPACES            s
#define OPT_LEAD_SPACES           S
#define OPT_LEAD_TABS             t
#define OPT_TITLE_LINE            T
#define OPT_MARKDOWN              u
#define OPT_VERSION               v
#define OPT_WIDTH                 w
#define OPT_WHITESPACE_DELIMIT    W
#define OPT_DOXYGEN               x
#define OPT_NO_HYPHEN             y
#define OPT_ENABLE_IPC            Z

/// Command-line option character as a character literal.
#define COPT(X)                   CHARIFY(OPT_##X)

/// Command-line option character as a single-character string literal.
#define SOPT(X)                   STRINGIFY(OPT_##X)

#define OPT_BUF_SIZE              32    /* used for format_opt() */

/**
 * End-of-Line formats.
 */
enum eol {
  EOL_INPUT   = 'i',                    ///< Do whatever the input does.
  EOL_UNIX    = 'u',                    ///< \\n
  EOL_WINDOWS = 'w'                     ///< \\r\\n
};
typedef enum eol eol_t;

// extern option variables
extern char const  *opt_alias;
extern char         opt_align_char;     ///< Use this to pad comment alignment.
extern size_t       opt_align_column;   ///< Align comment on given column.
extern char const  *opt_block_regex;    ///< Block regular expression.
extern char const  *opt_comment_chars;  ///< Chars that delimit comments.
extern char const  *opt_conf_file;      ///< Configuration file path.
extern bool         opt_data_link_esc;  ///< Respond to in-band control.
extern bool         opt_doxygen;        ///< Handle Doxygen commands?
extern eol_t        opt_eol;            ///< End-of-line treatment.
extern bool         opt_eos_delimit;    ///< End-of-sentence delimits para's?
extern size_t       opt_eos_spaces;     ///< Spaces after end-of-sentence.
extern char const  *opt_fin;            ///< File in path.
extern char const  *opt_fin_name;       ///< File in name (only).
extern char const  *opt_fout;           ///< File out path.
extern size_t       opt_hang_spaces;    ///< Hanging-indent spaces.
extern size_t       opt_hang_tabs;      ///< Hanging-indent tabs.
extern size_t       opt_indt_spaces;    ///< Indent spaces.
extern size_t       opt_indt_tabs;      ///< Indent tabs.
extern bool         opt_lead_dot_ignore;///< Ignore lines starting with '.'?
extern size_t       opt_lead_spaces;
extern char const  *opt_lead_string;
extern size_t       opt_lead_tabs;
extern bool         opt_lead_ws_delimit;///< Leading whitespace delimit para's?
extern size_t       opt_line_width;
extern bool         opt_markdown;
extern size_t       opt_mirror_spaces;
extern size_t       opt_mirror_tabs;
extern size_t       opt_newlines_delimit;
extern bool         opt_no_conf;        ///< Do not read conf file.
extern bool         opt_no_hyphen;      ///< Do not treat hyphens specially.
extern char const  *opt_para_delims;    ///< Additional para delimiter chars.
extern bool         opt_prototype;      ///< First line whitespace is prototype?
extern size_t       opt_tab_spaces;
extern bool         opt_title_line;     ///< 1st para line is title?

// other extern variables
extern FILE        *fin;                ///< File in.
extern FILE        *fout;               ///< File out.

////////// extern functions ///////////////////////////////////////////////////

/**
 * Formats an option as <code>[--%s/]-%c</code> where \c %s is the long option
 * (if any) and %c is the short option.
 *
 * @param short_opt The short option (along with its corresponding long option,
 * if any) to format.
 * @param buf The buffer to use.
 * @param buf_size The size of \a buf.
 * @return Returns \a buf.
 */
PJL_DISCARD
char* opt_format( char short_opt, char buf[], size_t buf_size );

/**
 * Initializes command-line option variables.
 *
 * @param argc The argument count from \c main().
 * @param argv The argument values from \c main().
 * @param usage A pointer to a function to print a usage message.  It must not
 * return.
 */
void options_init( int argc, char const *argv[], void (*usage)(int) );

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_options_H */
/* vim:set et sw=2 ts=2: */
