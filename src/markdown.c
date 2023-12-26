/*
**      wrap -- text reformatter
**      src/markdown.c
**
**      Copyright (C) 2016-2023  Paul J. Lucas
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
 * Defines macros, types, data structures, and functions for reformatting
 * Markdown.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "markdown.h"
#include "common.h"
#include "options.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <stddef.h>                     /* for size_t */
#include <stdlib.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup markdown-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Calls \ref md_stack_clear(), \ref md_stack_push( \a TOKEN ), and returns \a
 * TOKEN.
 *
 * @param TOKEN The token to push and return.
 */
#define CLEAR_RETURN(TOKEN) \
  BLOCK( md_stack_clear(); md_stack_push( (TOKEN), 0, 0 ); return &MD_TOP; )

/**
 * Gets an lvalue reference to the Nth Markdown state down from the top of the
 * stack.
 *
 * @param N The Nth item to get (0 = top of stack).
 * @return Returns an lvalue reference to the Nth Markdown state.
 *
 * @note This is a macro instead of an inline function so it'll be an lvalue
 * reference.
 */
#define MD_STACK(N)               (md_stack[ md_stack_top - (N) ])

/**
 * Gets an lvalue reference to the top Markdown state on the stack.
 *
 * @return Returns an lvalue reference to the top Markdown state.
 *
 * @note This is a macro instead of an inline function so it'll be an lvalue
 * reference.
 */
#define MD_TOP                    MD_STACK(0)

/**
 * Declares \a NAME as a local, `const` `bool` variable, initializes it with a
 * variable named `prev_`<i>NAME</i>, and sets `prev_`<i>NAME</i> to `false`.
 *
 * @param NAME The name of the local variable.
 */
#define PREV_BOOL(NAME)           \
  bool const NAME = prev_##NAME;  \
  prev_##NAME = false

/**
 * Compares \a S to \a STRLIT for equality.
 *
 * @param S The null-terminated string to check.
 * @param STRLIT The C string literal to check against.
 * @return Returns `true` only if \a S equals \a STRLIT.
 */
#define STRN_EQ_LIT(S,STRLIT) \
  (strncmp( (S), (STRLIT ""), sizeof( STRLIT "" ) - 1 ) == 0)

/**
 * HTML markup types.
 *
 * @remarks Every type that is not #HTML_NONE nor #HTML_ELEMENT is a "special"
 * type in that it (a) has a unique terminator and (b) does not nest.
 */
enum html_state {
  /// No state.
  HTML_NONE,

  /// <tt>&lt;![CDATA[</tt>...<tt>]]&gt;</tt>
  HTML_CDATA,

  /// <tt>&lt;!-\-</tt> ... <tt>-\-&gt;</tt>
  HTML_COMMENT,

  /// <tt>&lt;!DOCTYPE</tt> ...<tt>&gt;</tt>
  HTML_DOCTYPE,

  /// <tt>&lt;</tt> _tag_ <tt>&gt;</tt> ... <tt>&lt;/</tt> _tag_ <tt>&gt;</tt>
  HTML_ELEMENT,

  /// <tt>&lt;?</tt> ... <tt>?&gt;</tt>
  HTML_PI,

  /// <tt>&lt;pre&gt;</tt>, <tt>&lt;script&gt;</tt>, or <tt>&lt;style&gt;</tt>
  HTML_PRE,

  /// Ending HTML block.
  HTML_END
};
typedef enum html_state html_state_t;

/**
 * PHP Markdown Extra code fence info.
 */
struct md_code_fence {
  char    cf_c;                 ///< Character of the fence: `~` or <tt>`</tt>.
  size_t  cf_len;               ///< Length of the fence.
};
typedef struct md_code_fence md_code_fence_t;

typedef ssize_t md_stack_pos_t;         ///< Markdown stack position type.

// local constant definitions

/// HTML element name maximum length.
#define HTML_ELEMENT_CHAR_MAX    10

/// Maximum number of `#` in an atx header.
#define MD_ATX_CHAR_MAX           6

/// Minimum number of `~~~` or <tt>```</tt> characters for a code fence.
#define MD_CODE_FENCE_CHAR_MIN    3

/// Minimum indent for code.
#define MD_CODE_INDENT_MIN        4

/// Unordered list minimum indent.
#define MD_DL_UL_INDENT_MIN       2

/// Footnote indent.
#define MD_FOOTNOTE_INDENT        4

/// Minimum number of `***`, `---`, or `___` characters for a horizontal rule.
#define MD_HR_CHAR_MIN            3

/// Maximum indent for `[id]:' _URI_.
#define MD_LINK_INDENT_MAX        3

/// Maximum indent for all lists.
#define MD_LIST_INDENT_MAX        4

/// Ordered list maximum number of digits.
#define MD_OL_DIGIT_MAX           9

/// Ordered list minimum indent.
#define MD_OL_INDENT_MIN          3

/// Number of md_state objects to allocate by default.
#define MD_STATE_ALLOC_DEFAULT    5

/// Number of md_state objects to increment by.
#define MD_STATE_ALLOC_INCREMENT  5

/**
 * Block-level HTML 5 elements.
 */
static char const *const HTML_BLOCK_ELEMENT[] = {
  "article", "aside",
  "base", "basefont", "blockquote", "body", "br", "button",
  "canvas", "caption", "center", "col", "colgroup",
  "dd", "details", "dialog", "dir", "div", "dl", "dt",
  "embed",
  "fieldset", "figcaption", "figure", "footer", "form", "frame", "frameset",
  "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "hgroup", "hr", "html",
  "iframe",
  "legend", "li", "link",
  "main", "map", "menu", "menuitem", "meta",
  "nav", "noframes",
  "object", "ol", "optgroup", "option",
  "p", "param", "progress",
  "section", "source", "summary",
  "table", "tbody", "td", "textarea", "tfoot", "th", "thead", "title", "tr",
  "track",
  "ul",
  "video"
};

// local variable definitions
static html_state_t   curr_html_state;  ///< Current HTML state.
static md_state_t    *md_stack;         ///< Global stack of Markdown states.
static md_stack_pos_t md_stack_top;     ///< Top of \ref md_stack.
static md_seq_t       next_seq_num;     ///< Next sequence number.
static bool           prev_blank_line;  ///< Previous blank line.

/// Previous value for `code_fence_end` in markdown_parse().
static bool           prev_code_fence_end;

/// Previous value for `link_lable_has_title` in markdown_parse().
static bool           prev_link_label_has_title;

// local functions
NODISCARD
static bool           md_is_code_fence( char const*, md_code_fence_t* ),
                      md_is_dl_ul_helper( char const*, md_indent_t* );

NODISCARD
static html_state_t   md_is_html_tag( char const*, bool* );

NODISCARD
static md_line_t      md_nested_within( void );

NODISCARD
static unsigned       md_ol_digits( md_ol_t );

NODISCARD
static char const*    skip_html_tag( char const*, bool* );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether \a s is an HTML block-level element.
 *
 * @param s The null-terminated string to check. It is assumed to have been
 * converted to lower-case.
 * @return Returns `true` only if \a s is an HTML block-level element.
 */
NODISCARD
static inline bool is_html_block_element( char const *s ) {
  return NULL != bsearch(
    s, HTML_BLOCK_ELEMENT, ARRAY_SIZE( HTML_BLOCK_ELEMENT ),
    sizeof( HTML_BLOCK_ELEMENT[0] ), &bsearch_str_strptr_cmp
  );
}

/**
 * Checks whether \a s is an HTML pre-formatted block-level element.
 *
 * @param s The null-terminated string to check. It is assumed to have been
 * converted to lower-case.
 * @return Returns `true` only if \a s is an HTML pre-formatted block-level
 * element.
 */
NODISCARD
static inline bool is_html_pre_element( char const *s ) {
  return  strcmp( s, "pre"    ) == 0 ||
          strcmp( s, "script" ) == 0 ||
          strcmp( s, "style"  ) == 0;
}

/**
 * Checks whether \a c is an HTML element character.
 *
 * @param c The character to check.
 * @return Returns `true` only if \a c is an HTML element character.
 */
NODISCARD
static inline bool is_html_element_char( char c ) {
  return isalpha( c ) || c == '-';
}

/**
 * Initializes an md_code_fence_t.
 *
 * @param fence A pointer to the md_code_fence_t to initialize.
 */
static inline void md_code_fence_init( md_code_fence_t *fence ) {
  fence->cf_c  = '\0';
  fence->cf_len = 0;
}

/**
 * Checks whether the line is the end of a PHP Markdown Extra code fence.
 *
 * @param s The null-terminated line to check.
 * @param fence A pointer to the `struct` containing the fence info.
 * @return Returns `true` only if it is.
 */
NODISCARD
static inline bool md_is_code_fence_end( char const *s,
                                         md_code_fence_t *fence ) {
  return (s[0] == '`' || s[0] == '~') && md_is_code_fence( s, fence );
}

/**
 * Checks whether the line is a Markdown link title attribute.
 *
 * @param s The null-terminated line to check.
 * @return Returns `true` only if it is.
 */
NODISCARD
static inline bool md_is_link_title( char const *s ) {
  return s[0] == '"' || s[0] == '\'' || s[0] == '(';
}

/**
 * Checks whether \a c is a Markdown ordered list delimiter character.
 *
 * @param c The character to check.
 * @return Returns `true` only if \a c is an ordered list delimiter character.
 */
NODISCARD
static inline bool md_is_ol_sep_char( char c ) {
  return c == '.' || c == ')';
}

/**
 * Clears the Markdown stack down to the initial element.
 */
static inline void md_stack_clear( void ) {
  md_stack_top = 0;
}

/**
 * Checks whether the Markdown state stack is empty.
 *
 * @return Returns `true` only if it is.
 */
NODISCARD
static inline bool md_stack_empty( void ) {
  return md_stack_top < 0;
}

/**
 * Gets the size of the Markdown state stack.
 *
 * @return Returns said size.
 */
NODISCARD
static inline size_t md_stack_size( void ) {
  return STATIC_CAST( size_t, md_stack_top + 1 );
}

/**
 * Checks whether the line type that's part of the Markdown state on the top of
 * the stack is a particular type.
 *
 * @param line_type The line type to check for.
 * @return Returns `true` only if it is.
 */
NODISCARD
static inline bool md_top_is( md_line_t line_type ) {
  return MD_TOP.line_type == line_type;
}

/**
 * Calculates the minimum indent needed to be considered a line of code.
 *
 * @return Returns said indent.
 */
NODISCARD
static inline md_indent_t md_code_indent_min( void ) {
  return (md_stack_size() - md_top_is( MD_CODE )) * MD_CODE_INDENT_MIN;
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets a pointer to the first non-whitespace character in \a s.
 *
 * @param s The null-terminated line to use.
 * @param indent_left A pointer to receive the amount of indentation (with tabs
 * converted to equivalent spaces).
 * @return Returns a pointer to the first non-whitespace character.
 */
NODISCARD
static char* first_non_whitespace( char *s, md_indent_t *indent_left ) {
  assert( s != NULL );
  assert( indent_left != NULL );

  *indent_left = 0;
  for ( size_t tab_pos = 0; *s != '\0'; ++s, ++tab_pos ) {
    switch ( *s ) {
      case '\t':
        *indent_left += MD_TAB_SPACES - tab_pos % MD_TAB_SPACES;
        break;
      case '\r':
      case '\n':
        break;
      case ' ':
        ++*indent_left;
        break;
      default:
        goto done;
    } // switch
  } // for

done:
  return s;
}

/**
 * Checks whether the given string is a URI scheme followed by a `:`.
 *
 * @param s The null-terminated string to check.
 * @return Returns a pointer within \a s just after the `:` if \a s is a URI
 * scheme or null otherwise.
 *
 * @sa [RFC 3986: Uniform Resource Identifier (URI): Generic Syntax](https://datatracker.ietf.org/doc/html/rfc3986)
 */
NODISCARD
static char const* is_uri_scheme( char const *s ) {
  assert( s != NULL );
  if ( isalpha( *s ) ) {                // must be ^[a-zA-Z][a-zA-Z0-9.+-]*
    while ( *++s != '\0' ) {
      if ( *s == ':' )
        return s + 1;
      if ( !(isalnum( *s ) || *s == '.' || *s == '+' || *s == '-') )
        break;
    } // while
  }
  return NULL;
}

/**
 * Cleans-up all Markdown data.
 */
static void markdown_cleanup( void ) {
  FREE( md_stack );
}

/**
 * Given an indent, gets its preferred divisor.
 *
 * @remarks Markdown allows 3, but prefers 4, spaces per indent.  For example,
 * for \a indent_left values of 3, 6, 7, or 9 spaces, returns 3; for value of
 * 4, 5, 8, or 12, returns 4.
 * @par
 * As a special case, we also allow 2 spaces per indent for definition and
 * unordered lists.
 *
 * @param indent_left The raw indent (in spaces).
 * @return Returns the preferred divisor.
 */
NODISCARD
static md_indent_t md_indent_divisor( md_indent_t indent_left ) {
  md_line_t const line_type = md_nested_within();
  bool const dl_or_ul = line_type == MD_DL || line_type == MD_UL;
  md_indent_t const mod_a =            indent_left % MD_LIST_INDENT_MAX     ;
  md_indent_t const mod_b =            indent_left % MD_OL_INDENT_MIN       ;
  md_indent_t const mod_c = dl_or_ul ? indent_left % MD_DL_UL_INDENT_MIN : 9;
  return mod_a <= mod_b ?
    (mod_a <= mod_c ? MD_LIST_INDENT_MAX : MD_DL_UL_INDENT_MIN) :
    (mod_b <= mod_c ? MD_OL_INDENT_MIN   : MD_DL_UL_INDENT_MIN);
}

/**
 * Checks whether the line is a Markdown atx header line, a sequence of one to
 * six `#` characters starting in column 1 and followed by a whitespace
 * character.
 *
 * @param s The null-terminated line to check.
 * @return Returns `true` only if it is.
 */
NODISCARD
static bool md_is_atx_header( char const *s ) {
  assert( s != NULL );
  assert( s[0] == '#' );

  size_t n_atx = 0;
  for ( ; *s == '#'; ++s ) {
    if ( ++n_atx > MD_ATX_CHAR_MAX )
      return false;
  } // for
  return n_atx > 0 && isspace( *s );
}

/**
 * Checks whether the line is a PHP Markdown Extra code fence, a series of 3 or
 * more `~` or <tt>\`</tt> characters.
 *
 * @param s The null-terminated line to check.
 * @param fence The \ref md_code_fence to use: if `fence->cf_c`, return new
 * fence info; otherwise checks to see if \a s matches the existing fence info.
 * @return Returns `true` only if it is.
 */
NODISCARD
static bool md_is_code_fence( char const *s, md_code_fence_t *fence ) {
  assert( s != NULL );
  assert( fence != NULL );
  assert( s[0] == '~' || s[0] == '`' );
  assert(
    (fence->cf_c == '\0' && fence->cf_len == 0) ||
    (fence->cf_c != '\0' && fence->cf_len >= MD_CODE_FENCE_CHAR_MIN)
  );

  char const c = fence->cf_c != '\0' ? fence->cf_c : s[0];
  size_t len = 0;

  for ( ; *s == c; ++s, ++len )
    /* empty */;

  if ( fence->cf_len > 0 )              // closing code fence
    return len >= fence->cf_len && is_blank_line( s );

  if ( len >= MD_CODE_FENCE_CHAR_MIN ) {
    fence->cf_c   = c;
    fence->cf_len = len;
    return true;
  }
  return false;
}

/**
 * Checks whether the line is a PHP Markdown Extra definition list item: a :
 * followed by whitespace.
 *
 * @param s The null-terminated line to check.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns `true` only if \a s is a PHP Markdown Extra definition list
 * item.
 */
NODISCARD
static bool md_is_dl( char const *s, md_indent_t *indent_hang ) {
  assert( s != NULL );
  assert( s[0] == ':' );
  return md_is_dl_ul_helper( s, indent_hang );
}

/**
 * Checks whether the line is either a Markdown unordered list item or a PHP
 * Markdown Extra definition list item.
 *
 * @param s The null-terminated line to check.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns `true` only if \a s is a PHP Markdown Extra definition list
 * item.
 */
NODISCARD
static bool md_is_dl_ul_helper( char const *s, md_indent_t *indent_hang ) {
  assert( indent_hang != NULL );
  if ( is_space( s[1] ) ) {
    if ( s[1] == '\t' ) {
      *indent_hang = MD_LIST_INDENT_MAX;
    } else {
      *indent_hang = MD_DL_UL_INDENT_MIN;
      if ( is_space( s[2] ) ) {
        ++*indent_hang;
        if ( is_space( s[3] ) )
          ++*indent_hang;
      }
    }
    return true;
  }
  return false;
}

/**
 * Checks whether the line is a Doxygen ordered list item: a `-#` followed by
 * whitespace.
 *
 * @param s The null-terminated line to check.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns `true` only if \a s is a Doxygen ordered list item.
 */
NODISCARD
static bool md_is_dox_ol( char const *s, md_indent_t *indent_hang ) {
  assert( s != NULL );
  assert( s[0] == '-' );
  if ( s[1] == '#' && s[2] == ' ' ) {
    *indent_hang = MD_OL_INDENT_MIN;
    if ( is_space( s[3] ) )
      ++*indent_hang;
    return true;
  }
  return false;
}

/**
 * Checks whether the line is a PHP Markdown Extra footnote definition.
 *
 * @param s The null-terminated line to check.
 * @param def_has_text A pointer to the variable to receive whether the
 * footnote definition line contains any text other than the marker.
 * @return Returns `true` onlf if \a s is a PHP Markdown Extra footnote
 * definition.
 */
NODISCARD
static bool md_is_footnote_def( char const *s, bool *def_has_text ) {
  assert( s != NULL );
  assert( s[0] == '[' );
  assert( def_has_text != NULL );

  if ( *++s == '^' ) {
    while ( *++s != '\0' ) {
      if ( *s == ']' ) {
        if ( !(*++s == ':' && isspace( *++s )) )
          break;
        SKIP_CHARS( s, WS_STR );
        *def_has_text = *s && !isspace( *s );
        return true;
      }
    } // while
  }
  return false;
}

/**
 * Checks whether the line is a Markdown horizontal rule, a series of 3 or more
 * `*`, `-`, or `_` characters that may optionally be separated by whitespace.
 *
 * @param s The null-terminated line to check where \a s[0] is used as the rule
 * character to match.
 * @return Returns `true` only if it is.
 */
NODISCARD
static bool md_is_hr( char const *s ) {
  assert( s != NULL );
  assert( s[0] == '*' || s[0] == '-' || s[0] == '_' );

  size_t n_hr = 0;
  for ( char const hr = *s; *s != '\0'; ++s ) {
    if ( !isspace( *s ) ) {
      if ( *s != hr )
        return false;
      ++n_hr;
    }
  } // for

  return n_hr >= MD_HR_CHAR_MIN;
}

/**
 * Checks whether the line is a PHP Markdown Extra HTML abbreviation.
 *
 * @param s The null-terminated line to check.
 * @return Returns `true` only if the line is a PHP Markdown Extra HTML
 * abbreviation.
 */
NODISCARD
static bool md_is_html_abbr( char const *s ) {
  assert( s != NULL );
  assert( s[0] == '*' );

  if ( *++s == '[' ) {
    while ( *++s != '\0' ) {
      switch ( *s ) {
        case '\\':
          ++s;
          break;
        case ']':
          return *++s == ':';
      } // switch
    } // while
  }
  return false;
}

/**
 * Checks to see whether \a s ends the current HTML state.
 *
 * @param html_state The HTML state.
 * @param s The null-terminated line to check.
 * @return Returns `true` only if \a s ends \a html_state.
 */
NODISCARD
static bool md_is_html_end( html_state_t html_state, char const *s ) {
  //
  // For each HTML state, the string that ends it.  NULL entries are handled
  // differently.
  //
  static char const *const HTML_END_STR[] = {
    NULL,   // HTML_NONE
    "]]>",  // HTML_CDATA
    "-->",  // HTML_COMMENT
    ">",    // HTML_DOCTYPE
    NULL,   // HTML_ELEMENT
    "?>",   // HTML_PI
    NULL,   // HTML_PRE
    NULL,   // HTML_END
  };

  assert( STATIC_CAST( size_t, html_state ) < ARRAY_SIZE( HTML_END_STR ) );
  assert( s != NULL );

  if ( html_state == HTML_PRE ) {
    //
    // Does the line contain an end HTML_PRE tag?
    //
    for (;;) {
      if ( (s = strchr( s, '<' )) == NULL )
        return false;
      bool is_end_tag;
      if ( md_is_html_tag( s, &is_end_tag ) == HTML_PRE && is_end_tag )
        return true;
      s = skip_html_tag( s, &is_end_tag );
    } // for
  }
  //
  // Does the line contain the end string?
  //
  char const *const end = HTML_END_STR[ html_state ];
  return end != NULL ? strstr( s, end ) != NULL : is_blank_line( s );
}

/**
 * Checks whether the line is a block-level HTML element.
 *
 * @param s The null-terminated line to check.
 * @param is_end_tag A pointer to the variable to receive whether the HTML tag
 * is an end tag.
 * @return Returns the HTML type only if the line is a block-level HTML tag or
 * special markup or #HTML_NONE if not.
 */
NODISCARD
static html_state_t md_is_html_tag( char const *s, bool *is_end_tag ) {
  assert( s != NULL );
  assert( s[0] == '<' );
  assert( is_end_tag != NULL );

  if ( *++s == '\0' )
    return HTML_NONE;

  html_state_t html_state = HTML_NONE;
  //
  // Is this a "special" HTML block?
  //
  if ( s[0] == '?' )
    html_state = HTML_PI;
  else if ( s[0] == '!' ) {
    ++s;
    if ( isupper( s[0] ) )
      html_state = HTML_DOCTYPE;
    else if ( STRN_EQ_LIT( s, "--" ) )
      html_state = HTML_COMMENT;
    else if ( STRN_EQ_LIT( s, "[CDATA[" ) )
      html_state = HTML_CDATA;
    else
      return HTML_NONE;
  }

  if ( html_state != HTML_NONE ) {
    //
    // Does the HTML block end on the same line as it starts?
    //
    *is_end_tag = md_is_html_end( html_state, s );
    return html_state;
  }

  if ( (*is_end_tag = s[0] == '/') )    // </tag>
    ++s;

  char element[ HTML_ELEMENT_CHAR_MAX + 1/*null*/ ];

  for ( size_t len = 0; ; ) {
    if ( len == sizeof element - 1 )    // element too long
      return HTML_NONE;
    if ( !is_html_element_char( *s ) ) {
      if ( !(isspace( *s ) || *s == '>' || *s == '/') )
        return HTML_NONE;
      if ( *s == '/' ) {
        if ( *is_end_tag )              // invalid: more than one '/'
          return HTML_NONE;
        *is_end_tag = true;
      }
      element[ len ] = '\0';
      break;
    }
    element[ len++ ] = STATIC_CAST( char, tolower( *s++ ) );
  } // for

  if ( is_html_pre_element( element ) ) {
    if ( !*is_end_tag ) {
      //
      // Does the HTML block end on the same line as it starts?
      //
      *is_end_tag = md_is_html_end( HTML_PRE, s );
    }
    return HTML_PRE;
  }

  if ( is_html_block_element( element ) )
    return HTML_ELEMENT;

  //
  // In order for inline HTML to be considered an HTML block, it has to be on
  // a line by itself.
  //
  s = skip_html_tag( s, is_end_tag );
  return s != NULL && is_blank_line( s ) ? HTML_ELEMENT : HTML_NONE;
}

/**
 * Checks whether the line is a Markdown link label.
 *
 * @param s The null-terminated line to check.
 * @param has_title A pointer to the variable to receive whether the link label
 * has a title attribute on the same line.
 * @return Returns `true` only if \a s is a Markdown link label.
 */
NODISCARD
static bool md_is_link_label( char const *s, bool *has_title ) {
  assert( s != NULL );
  assert( s[0] == '[' );
  assert( has_title != NULL );

  while ( *++s != '\0' ) {
    if ( *s == ']' ) {
      if ( !(*++s == ':' && is_space( *++s )) )
        break;
      SKIP_CHARS( s, WS_STR );
      if ( *s == '<' )
        ++s;
      //
      // We only check whether the URI scheme is valid and, if it is, that's
      // good enough and we don't bother validating the syntax of the URI.
      //
      if ( (s = is_uri_scheme( s )) == NULL )
        break;
      s += strcspn( s, " \t\r\n" );
      *has_title = false;
      if ( !is_blank_line( s ) ) {
        SKIP_CHARS( s, WS_STR );
        if ( !md_is_link_title( s ) )
          break;
        *has_title = true;
      }
      return true;
    }
  } // while

  return false;
}

/**
 * Checks whether \a line_type can nest.
 *
 * @param line_type The line type to check.
 * @return Returns `true` only if \a line_type can nest.
 */
NODISCARD
static bool md_is_nestable( md_line_t line_type ) {
  switch ( line_type ) {
    case MD_DL:
    case MD_FOOTNOTE_DEF:
    case MD_OL:
    case MD_UL:
      return true;
    default:
      return false;
  } // switch
}

/**
 * Checks whether the line is a Markdown ordered list item: a sequence of
 * digits followed by either `'.'` or `')'` and whitespace.
 *
 * @param s The null-terminated line to check.
 * @param ol_num A pointer to the variable to receive the ordered list number.
 * @param ol_c A pointer to the variable to receive the ordered list character.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns `true` only if \a s is a Markdown ordered list item.
 */
NODISCARD
static bool md_is_ol( char const *s, md_ol_t *ol_num, char *ol_c,
                      md_indent_t *indent_hang ) {
  assert( s != NULL );
  assert( isdigit( s[0] ) );
  assert( ol_num != NULL );
  assert( ol_c != NULL );
  assert( indent_hang != NULL );

  *ol_num = 0;
  char const *d = s;
  for ( ; isdigit( *d ); ++d )
    *ol_num = *ol_num * 10 + STATIC_CAST( unsigned, *d - '0' );

  size_t const len = STATIC_CAST( size_t, d - s );
  if ( len >= 1 && len <= MD_OL_DIGIT_MAX && md_is_ol_sep_char( d[0] ) &&
       is_space( d[1] ) ) {
    *ol_c = d[0];
    *indent_hang = d[1] == '\t' ?
      MD_LIST_INDENT_MAX :
      MD_OL_INDENT_MIN + is_space( d[2] ) + md_ol_digits( *ol_num ) - 1;
    return true;
  }
  return false;
}

/**
 * Checks whether the line is a PHP Markdown Extra table line.
 *
 * @param s The null-terminated line to check.
 * @return Returns `true` only if \a s is a PHP Markdown Extra table line.
 */
NODISCARD
static bool md_is_table( char const *s ) {
  assert( s != NULL );
  bool found_nws = false;               // encountered non-whitespace?

  for ( ; *s != '\0'; ++s ) {
    switch ( *s ) {
      case '\\':
        ++s;
        break;
      case '|':
        if ( found_nws ) {
          //
          // Insist on encountering at least one non-whitespace character on
          // the line before returning that it's a table.
          //
          return true;
        }
        break;
      default:
        if ( !isspace( *s ) )
          found_nws = true;
    } // switch
  } // for
  return false;
}

/**
 * Checks whether the line is a Markdown unordered list item: a *, -, or +
 * followed by whitespace.
 *
 * @param s The null-terminated line to check.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns `true` only if \a s is a Markdown unordered list item.
 */
NODISCARD
static bool md_is_ul( char const *s, md_indent_t *indent_hang ) {
  assert( s != NULL );
  assert( s[0] == '*' || s[0] == '-' || s[0] == '+' );
  return md_is_dl_ul_helper( s, indent_hang );
}

/**
 * Checks whether the line is a Markdown Setext header line, a sequence of one
 * or more `=` or `-` characters starting in column 1.
 *
 * @param s The null-terminated line to check where \a s[0] is used as the
 * header character to match.
 * @return Returns `true` onlt if it is.
 */
NODISCARD
static bool md_is_Setext_header( char const *s ) {
  assert( s != NULL );
  assert( s[0] == '-' || s[0] == '=' );

  for ( char const c = *s; *s != '\0' && !is_eol( *s ); ++s ) {
    if ( *s != c )
      return is_blank_line( s );
  } // for
  return true;
}

/**
 * Checks the innermost enclosing nestable line type, if any.
 *
 * @return Returns said line type or MD_NONE if none.
 */
NODISCARD
static md_line_t md_nested_within( void ) {
  for ( md_stack_pos_t pos = md_stack_top; pos >= 0; --pos ) {
    md_line_t const line_type = MD_STACK(pos).line_type;
    if ( md_is_nestable( line_type ) )
      return line_type;
  } // for
  return MD_NONE;
}

/**
 * Gets the number of digits \a n would take to print.
 *
 * @return Returns said number of digits.
 */
NODISCARD
static unsigned md_ol_digits( md_ol_t n ) {
  char c;
  int const len = snprintf( &c, 1, "%u", n );
  PERROR_EXIT_IF( len < 0, EX_IOERR );
  return STATIC_CAST( unsigned, len );
}

/**
 * Correctly numbers a Markdown ordered list by replacing whatever number is
 * present with \a new_n.
 *
 * @param s The null-terminated line starting with a Markdown ordered list
 * where \a s[0] is assumed to be a digit.  The line is renumbered in-place.
 * @param old_n The old nunber.
 * @param new_n The new number.
 */
static void md_renumber_ol( char *s, md_ol_t old_n, md_ol_t new_n ) {
  assert( s != NULL );
  if ( new_n != old_n ) {
    size_t const s_len = strlen( s );
    size_t const old_digits = md_ol_digits( old_n );

    // convert new_n to a string
    char new_buf[11];                   // enough for sizeof(md_ol_t) == 4
    int const new_len = snprintf( new_buf, sizeof new_buf, "%u", new_n );
    PERROR_EXIT_IF( new_len < 0, EX_IOERR );
    size_t const new_digits = STATIC_CAST( size_t, new_len );
    assert( new_digits < sizeof new_buf );

    // ensure there's the exact right amount of space for new_n
    if ( old_digits < new_digits )
      memmove( s+2, s+1, s_len - old_digits + 1 );
    else if ( old_digits > new_digits )
      memmove( s+1, s+2, s_len - old_digits + 1 );

    // copy new_n into place
    memcpy( s, new_buf, new_digits );
  }
}

/**
 * Skips past the end of the current HTML (or XML) tag.
 *
 * @param s A pointer to within the text of an HTML (or XML) tag.
 * @param is_end_tag A pointer to a `bool`: if `true` we are skipping an end
 * tag; if `false`, set to `true` if the tag is an XML end tag on return.
 * @return Returns a pointer to just past the closing '>' of the tag or a
 * pointer to an empty string if \a s is not an HTML (or XML) tag.
 */
NODISCARD
static char const* skip_html_tag( char const *s, bool *is_end_tag ) {
  assert( s != NULL );
  assert( is_end_tag != NULL );

  if ( *s == '<' ) {
    //
    // If we're starting at the very beginning of the tag, we don't yet know
    // whether it's an end tag, so ignore whatever value we were passed in.
    //
    *is_end_tag = false;
  }

  char quote = '\0';
  for ( ; *s != '\0'; ++s ) {
    if ( quote != '\0' ) {              // ignore everything ...
      if ( *s == quote )                // ... until matching quote
        quote = '\0';
    } else {
      switch ( *s ) {
        case '"':
        case '\'':
          if ( *is_end_tag )            // quotes illegal in end tag
            return "";
          quote = *s;                   // start ignoring everything
          break;
        case '/':                       // </tag> or <tag/>
          if ( *is_end_tag )            // invalid: more than one '/'
            return "";
          *is_end_tag = true;
          FALLTHROUGH;
        case '>':                       // found closing '>'
          return ++s;
      } // switch
    }
  } // for
  return s;
}

/**
 * Pops a Markdown state from the stack.
 */
static void md_stack_pop( void ) {
  MD_DEBUG( "%s()\n", __func__ );
  assert( !md_stack_empty() );
  --md_stack_top;
}

/**
 * Pushes a new Markdown state onto the stack.
 *
 * @param line_type The type of line.
 * @param indent_left The left indent (in spaces).
 * @param indent_hang The indent relative to \a indent_left for a hang-indent.
 */
static void md_stack_push( md_line_t line_type, md_indent_t indent_left,
                           md_indent_t indent_hang ) {
  static size_t md_stack_capacity = 0;

  MD_DEBUG(
    "%s(): T=%c L=%u H=%u\n",
    __func__, line_type, indent_left, indent_hang
  );

  ++md_stack_top;
  if ( md_stack_capacity == 0 ) {
    md_stack_capacity = MD_STATE_ALLOC_DEFAULT;
    md_stack = MALLOC( md_state_t, md_stack_capacity );
  } else if ( STATIC_CAST( size_t, md_stack_top ) >= md_stack_capacity ) {
    md_stack_capacity += MD_STATE_ALLOC_INCREMENT;
    REALLOC( md_stack, md_state_t, md_stack_capacity );
  }
  PERROR_EXIT_IF( md_stack == NULL, EX_OSERR );

  md_state_t *const top = &MD_TOP;
  *top = (md_state_t){
    .line_type   = line_type,
    .seq_num     = ++next_seq_num,
    .depth       = md_stack_size() > 0 ? md_stack_size() - 1 : 0,
    .indent_left = indent_left,
    .indent_hang = indent_hang,
    .ol_c        = '\0',
    .ol_num      = 0
  };
}

////////// extern functions ///////////////////////////////////////////////////

void markdown_init( void ) {
  RUN_ONCE ATEXIT( &markdown_cleanup );

  curr_html_state = HTML_NONE;
  prev_code_fence_end = false;
  prev_link_label_has_title = false;
  //
  // We have to start out prev_blank_line = true because if a "---" occurs as
  // the first line, there is no text line before it so it must be a horizontal
  // rule and not a Setext 2nd-level header.
  //
  prev_blank_line = true;
  //
  // Initialize the stack so that it always contains at least one element.
  //
  md_stack_top = -1;
  md_stack_push( MD_TEXT, 0, 0 );
}

md_state_t const* markdown_parse( char *s ) {
  assert( s != NULL );

  static md_code_fence_t code_fence;
  md_indent_t indent_left;
  char *const nws = first_non_whitespace( s, &indent_left );

  PREV_BOOL( code_fence_end );
  PREV_BOOL( link_label_has_title );

  /////////////////////////////////////////////////////////////////////////////

  switch ( MD_TOP.line_type ) {
    case MD_CODE:
      //
      // Check to see whether we've hit the end of a PHP Markdown Extra code
      // fence.
      //
      if ( code_fence_end )
        md_stack_pop();
      else if ( code_fence.cf_c != '\0' ) {
        //
        // If code_fence.cf_c is set, that distinguishes a code fence from
        // indented code.
        //
        if ( md_is_code_fence_end( nws, &code_fence ) )
          prev_code_fence_end = true;
        //
        // As long as we're in the MD_CODE state, we can just return without
        // further checks.
        //
        return &MD_TOP;
      }
      break;

    case MD_HEADER_ATX:
    case MD_HEADER_LINE:
    case MD_HR:
    case MD_HTML_ABBR:
      //
      // These tokens are "one-shot," i.e., they never span multipe lines, so
      // pop them off the stack.
      //
      md_stack_pop();
      break;

    case MD_LINK_LABEL:
      //
      // Markdown link label title attributes.
      //
      if ( !link_label_has_title && md_is_link_title( nws ) )
        return &MD_TOP;
      md_stack_pop();
      break;

    case MD_TABLE:
      //
      // PHP Markdown Extra table.
      //
      if ( md_is_table( s ) ) {
        //
        // As long as we're in the MD_TABLE state and lines continue to be
        // table lines, we can just return without further checks.
        //
        return &MD_TOP;
      }
      md_stack_pop();
      break;

    case MD_DL:
    case MD_FOOTNOTE_DEF:
    case MD_HTML_BLOCK:
    case MD_NONE:
    case MD_OL:
    case MD_TEXT:
    case MD_UL:
      // nothing to do
      break;
  } // switch

  /////////////////////////////////////////////////////////////////////////////

  //
  // While blank lines don't change state directly, we do have to keep track of
  // when we've seen one because:
  //
  //  + They help determine whether the line after the blank line is a
  //    continuation of the current state based on indentation.
  //  + They disambiguate "---" between a Setext 2nd-level header (that has to
  //    have a text line before it) and a horizontal rule (that doesn't).
  //
  PREV_BOOL( blank_line );
  if ( nws[0] == '\0' ) {               // blank line
    prev_blank_line = true;
    return &MD_TOP;
  }

  /////////////////////////////////////////////////////////////////////////////

  if ( md_top_is( MD_HTML_BLOCK ) ) {
    //
    // HTML blocks.
    //
    switch ( curr_html_state ) {
      case HTML_ELEMENT:
        if ( blank_line )
          md_stack_pop();
        return &MD_TOP;
      case HTML_END:
        md_stack_pop();
        break;
      default:
        if ( md_is_html_end( curr_html_state, s ) )
          curr_html_state = HTML_END;
        //
        // As long as we're in the MD_HTML_BLOCK state, we can just return
        // without further checks.
        //
        return &MD_TOP;
    } // switch
  }
  else {
    //
    // Markdown code blocks.
    //
    md_indent_t const code_indent_min = md_code_indent_min();
    if ( indent_left >= code_indent_min ) {
      if ( !md_top_is( MD_CODE ) )
        md_stack_push( MD_CODE, code_indent_min, 0 );
      //
      // As long as we're in the MD_CODE state, we can just return without
      // further checks.
      //
      return &MD_TOP;
    }
  }

  /////////////////////////////////////////////////////////////////////////////

  switch ( nws[0] ) {

    // atx headers.
    case '#':
      if ( md_is_atx_header( nws ) )
        CLEAR_RETURN( MD_HEADER_ATX );
      break;

    // PHP Markdown Extra abbreviations.
    case '*':
      if ( md_is_html_abbr( nws ) )
        CLEAR_RETURN( MD_HTML_ABBR );
      break;

    // Setext headers.
    case '-':
    case '=':
      if ( !blank_line && md_is_Setext_header( nws ) )
        CLEAR_RETURN( MD_HEADER_LINE );
      break;

    // Markdown link labels or PHP Markdown Extra footnote definitions.
    case '[':
      if ( indent_left <= MD_LINK_INDENT_MAX ) {
        bool def_has_text;
        if ( md_is_footnote_def( nws, &def_has_text ) ) {
          md_stack_clear();
          md_stack_push( MD_FOOTNOTE_DEF, 0, MD_FOOTNOTE_INDENT );
          MD_TOP.footnote_def_has_text = def_has_text;
          return &MD_TOP;
        }
        if ( md_is_link_label( nws, &prev_link_label_has_title ) )
          CLEAR_RETURN( MD_LINK_LABEL );
      }
      break;

    // PHP Markdown Extra code fences.
    case '`':
    case '~':
      md_code_fence_init( &code_fence );
      if ( md_is_code_fence( nws, &code_fence ) )
        CLEAR_RETURN( MD_CODE );
      break;

    // Block-level HTML.
    case '<':
      NO_OP;
      bool is_end_tag;
      curr_html_state = md_is_html_tag( nws, &is_end_tag );
      if ( curr_html_state != HTML_NONE ) {
        if ( is_end_tag )               // HTML ends on same line as it begins
          curr_html_state = HTML_END;
        md_stack_push( MD_HTML_BLOCK, indent_left, 0 );
        return &MD_TOP;
      }
      break;
  } // switch

  //
  // Markdown horizontal rules: these are handled more simply if they are in
  // their own switch statement since the * and - would otherwise be duplicate
  // case values in the above switch statement.
  //
  switch ( nws[0] ) {
    case '*':
    case '-':
    case '_':
      if ( md_is_hr( nws ) )
        CLEAR_RETURN( MD_HR );
  } // switch

  /////////////////////////////////////////////////////////////////////////////

  md_line_t   curr_line_type = MD_NONE;
  md_indent_t indent_hang = 0;
  char        ol_c = '\0';
  md_ol_t     ol_num = 0;

  //
  // We first have to determine the type of the current line because it affects
  // the depth calculation.
  //
  switch ( nws[0] ) {

    // Ordered lists.
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      if ( md_is_ol( nws, &ol_num, &ol_c, &indent_hang ) )
        curr_line_type = MD_OL;
      break;

    // Doxygen ordered lists.
    case '-':
      if ( opt_doxygen && md_is_dox_ol( nws, &indent_hang ) ) {
        //
        // Even though it's a Doxygen ordered list, we treat is as unordered
        // since we leave the list marker alone, i.e., we don't renumber it.
        //
        curr_line_type = MD_UL;
        break;
      }
      FALLTHROUGH;

    // Unordered lists.
    case '*':
    case '+':
      if ( md_is_ul( nws, &indent_hang ) )
        curr_line_type = MD_UL;
      break;

    // Definition lists.
    case ':':
      if ( md_is_dl( nws, &indent_hang ) )
        curr_line_type = MD_DL;
  } // switch

  //
  // Based on the indent, previous, and current line types, calculate the depth
  // of the current line.
  //
  md_depth_t depth = indent_left / md_indent_divisor( indent_left );
  if ( (!blank_line && md_is_nestable( MD_TOP.line_type )) ||
       md_is_nestable( curr_line_type ) ) {
    ++depth;
  }

  //
  // Pop states for decreases in indentation.
  //
  MD_DEBUG( "pop stack? D=%u MD_TOP.D=%u\n", depth, MD_TOP.depth );
  while ( depth < MD_TOP.depth ) {
    MD_DEBUG( "+ D=%u < MD_TOP.D=%u => ", depth, MD_TOP.depth );
    md_stack_pop();
  } // while

  md_indent_t const nested_indent_min = MD_TOP.depth * MD_LIST_INDENT_MAX;
  bool const is_nested = indent_left >= nested_indent_min;
  bool const is_same_type_not_nested =
    md_top_is( curr_line_type ) && !is_nested;

  switch ( curr_line_type ) {

    case MD_NONE:
      if ( blank_line && md_is_table( s ) ) {
        assert( !md_top_is( MD_TABLE ) );
        if ( is_nested )
          md_stack_push( MD_TABLE, indent_left, 0 );
      }
      break;

    case MD_OL:
      NO_OP;
      bool const ol_same_char = MD_TOP.ol_c == ol_c;
      if ( is_same_type_not_nested && ol_same_char ) {
        MD_TOP.seq_num = ++next_seq_num;  // reuse current state
        md_ol_t const next_ol_num = ++MD_TOP.ol_num;
        if ( next_ol_num ==        10 ||
             next_ol_num ==       100 ||
             next_ol_num ==      1000 ||
             next_ol_num ==     10000 ||
             next_ol_num ==    100000 ||
             next_ol_num ==   1000000 ||
             next_ol_num ==  10000000 ||
             next_ol_num == 100000000 ) {
          ++MD_TOP.indent_hang;         // digits just got 1 wider
        }
        md_renumber_ol( nws, ol_num, next_ol_num );
      } else {
        if ( is_same_type_not_nested && !ol_same_char ) {
          //
          // This handles the case when the ordered list delimiter changes:
          //
          //  1. This is one list.
          //  2) This is another list.
          //
          // Just get rid of the current list (effectively replacing it with
          // the new list).
          //
          md_stack_pop();
        }
        md_stack_push( MD_OL, indent_left, indent_hang );
        MD_TOP.ol_c   = ol_c;
        MD_TOP.ol_num = ol_num;
      }
      break;

    case MD_DL:
    case MD_UL:
      if ( is_same_type_not_nested )
        MD_TOP.seq_num = ++next_seq_num;  // reuse current state
      else
        md_stack_push( curr_line_type, indent_left, indent_hang );
      break;

    default:
      /* suppress warning */;
  } // switch

  return &MD_TOP;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
