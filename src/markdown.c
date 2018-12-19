/*
**      wrap -- text reformatter
**      src/markdown.c
**
**      Copyright (C) 2016-2017  Paul J. Lucas
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
#include "wrap.h"                       /* must go first */
#include "markdown.h"
#include "common.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <stddef.h>                     /* for size_t */
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

#define CLEAR_RETURN(TOKEN) \
  BLOCK( stack_clear(); stack_push( TOKEN, 0, 0 ); return &TOP; )

#define PREV_BOOL(NAME)           \
  bool const NAME = prev_##NAME;  \
  prev_##NAME = false

#define STRN_EQ_LIT(S,STRLIT) \
  (strncmp( (S), (STRLIT), sizeof( STRLIT ) - 1 ) == 0)

#define STACK(N)            (stack[ stack_top - (N) ])
#define TOP                 STACK(0)

/**
 * HTML markup types.  Every type that is not NONE nor ELEMENT is a "special"
 * type in that it (a) has a unique terminator and (b) does not nest.
 */
enum html_state {
  HTML_NONE,
  HTML_CDATA,                           // <![CDATA[ ... ]]>
  HTML_COMMENT,                         // <!-- ... -->
  HTML_DOCTYPE,                         // <!DOCTYPE ... >
  HTML_ELEMENT,                         // <tag> ... </tag>
  HTML_PI,                              // <? ... ?>
  HTML_PRE,                             // <pre>, <script>, or <style>
  HTML_END                              // ending HTML block
};
typedef enum html_state html_state_t;

/**
 * PHP Markdown Extra code fence info.
 */
struct md_code_fence {
  char    cf_c;                         // character of the fence: ~ or `
  size_t  cf_len;                       // length of the fence
};
typedef struct md_code_fence md_code_fence_t;

typedef ssize_t stack_pos_t;

// local constant definitions
#define             HTML_ELEMENT_CHAR_MAX    10   /* "blockquote" */
static size_t const MD_ATX_CHAR_MAX        =  6;  // max num of # in atx header
static size_t const MD_CODE_FENCE_CHAR_MIN =  3;  // min num of ~~~ or ```
static size_t const MD_CODE_INDENT_MIN     =  4;  // min indent for code
static size_t const MD_FOOTNOTE_INDENT     =  4;
static size_t const MD_HR_CHAR_MIN         =  3;  // min num of ***, ---, or ___
static size_t const MD_LINK_INDENT_MAX     =  3;  // max indent for [id]: URI
static size_t const MD_LIST_INDENT_MAX     =  4;  // max indent for all lists
static size_t const MD_DL_UL_INDENT_MIN    =  2;  // unordered list min indent
static size_t const MD_OL_CHAR_MAX         =  9;  // ordered list max digits
static size_t const MD_OL_INDENT_MIN       =  3;  // ordered list min indent
static size_t const STATE_ALLOC_DEFAULT    =  5;
static size_t const STATE_ALLOC_INCREMENT  =  5;

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
static html_state_t html_state;
static md_seq_t     next_seq_num;
static bool         prev_blank_line;
static bool         prev_code_fence_end;
static bool         prev_link_label_has_title;
static md_state_t  *stack;              // global stack of states
static stack_pos_t  stack_top;          // index of the top of the stack

// local functions
static int          bin_search_str_strptr_cmp( void const*, void const* );
static bool         md_is_code_fence( char const*, md_code_fence_t* );
static bool         md_is_dl_ul_helper( char const*, md_indent_t* );
static html_state_t md_is_html_tag( char const*, bool* );
static md_line_t    md_nested_within( void );
static char const*  skip_html_tag( char const*, bool* );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether \a s is an HTML block-level element.
 *
 * @param s The null-terminated string to check. It is assumed to have been
 * converted to lower-case.
 * @return Returns \c true only if \a s is an HTML block-level element.
 */
static inline bool is_html_block_element( char const *s ) {
  return NULL != bin_search(
    s, HTML_BLOCK_ELEMENT, ARRAY_SIZE( HTML_BLOCK_ELEMENT ),
    sizeof(char const*), &bin_search_str_strptr_cmp
  );
}

/**
 * Checks whether \a s is an HTML pre-formatted block-level element.
 *
 * @param s The null-terminated string to check. It is assumed to have been
 * converted to lower-case.
 * @return Returns \c true only if \a s is an HTML pre-formatted block-level
 * element.
 */
static inline bool is_html_pre_element( char const *s ) {
  return  strcmp( s, "pre"    ) == 0 ||
          strcmp( s, "script" ) == 0 ||
          strcmp( s, "style"  ) == 0;
}

/**
 * Checks whether \a c is an HTML element character.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is an HTML element character.
 */
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
 * @param fence A pointer to the \c struct containing the fence info.
 * @return Returns \c true only if it is.
 */
static inline bool md_is_code_fence_end( char const *s,
                                         md_code_fence_t *fence ) {
  return (s[0] == '`' || s[0] == '~') && md_is_code_fence( s, fence );
}

/**
 * Checks whether the line is a Markdown link title attribute.
 *
 * @param s The null-terminated line to check.
 * @return Returns \c true only if it is.
 */
static inline bool md_is_link_title( char const *s ) {
  return *s == '"' || *s == '\'' || *s == '(';
}

/**
 * Checks whether \a line_type can nest.
 *
 * @param line_type The line type to check.
 * @return Returns \c true only if \a line_type can nest.
 */
static inline bool md_is_nestable( md_line_t line_type ) {
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
 * Checks whether \a c is a Markdown ordered list delimiter character.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is an ordered list delimiter character.
 */
static inline bool md_is_ol_sep_char( char c ) {
  return c == '.' || c == ')';
}

/**
 * Clears the Markdown stack down to the initial element.
 */
static inline void stack_clear( void ) {
  stack_top = 0;
}

/**
 * Checks whether the Markdown state stack is empty.
 *
 * @return Returns \c true only if it is.
 */
static inline bool stack_empty( void ) {
  return stack_top < 0;
}

/**
 * Pops a Markdown state from the stack.
 */
static inline void stack_pop( void ) {
  MD_DEBUG( "%s()\n", __func__ );
  assert( !stack_empty() );
  --stack_top;
}

/**
 * Gets the size of the Markdown state stack.
 *
 * @return Returns said size.
 */
static inline size_t stack_size( void ) {
  return (size_t)(stack_top + 1);
}

/**
 * Checks whether the line type that's part of the Markdown state on the top of
 * the stack is a particular type.
 *
 * @param line_type The line type to check for.
 * @return Returns \c true only if it is.
 */
static inline bool top_is( md_line_t line_type ) {
  return TOP.line_type == line_type;
}

/**
 * Calculates the minimum indent needed to be considered a line of code.
 *
 * @return Returns said indent.
 */
static inline md_indent_t md_code_indent_min( void ) {
  return (stack_size() - top_is( MD_CODE )) * MD_CODE_INDENT_MIN;
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Comparison function for bin_search that compares a string key against an
 * element.
 *
 * @param key A pointer to the string being searched for.
 * @param elt A pointer to the pointer to the string of the current element to
 * compare againset.
 * @return Returns an integer less than zero, zero, or greater thatn zero if
 * the key is less than, equal to, or greater than the element, respectively.
 */
static int bin_search_str_strptr_cmp( void const *key, void const *elt ) {
  char const *const s_key = REINTERPRET_CAST( char const*, key );
  char const *const s_elt = *REINTERPRET_CAST( char const**, elt );
  return strcmp( s_key, s_elt );
}

/**
 * Gets a pointer to the first non-whitespace character in \a s.
 *
 * @param s The null-terminated line to use.
 * @param indent_left A pointer to receive the amount of indentation (with tabs
 * converted to equivalent spaces).
 * @return Returns a pointer to the first non-whitespace character.
 */
static char* first_non_whitespace( char *s, md_indent_t *indent_left ) {
  assert( s != NULL );
  assert( indent_left != NULL );

  *indent_left = 0;
  for ( size_t tab_pos = 0; *s != '\0'; ++s, ++tab_pos ) {
    switch ( *s ) {
      case '\t':
        *indent_left += TAB_SPACES_MARKDOWN - tab_pos % TAB_SPACES_MARKDOWN;
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
 * Checks whether the given string is a URI scheme followed by a ':'.
 *
 * See also:
 *  + RFC 3986: "Uniform Resource Identifier (URI): Generic Syntax," Section
 *    3.1, "Scheme," Network Working Group of the Internet Engineering Task
 *    Force, January 2005.
 *
 * @param s The null-terminated string to check.
 * @return Returns a pointer within \a s just after the ':' if \a s is a URI
 * scheme or null otherwise.
 */
static char const* is_uri_scheme( char const *s ) {
  assert( s != NULL );
  if ( isalpha( *s ) ) {                // must be ^[a-zA-Z][a-zA-Z0-9.+-]*
    while ( *++s != '\0' ) {
      if ( *s == ':' )
        return s + 1;
      if ( !(isalnum( *s ) || *s == '.' || *s == '+' || *s == '-') )
        return NULL;
    } // while
  }
  return NULL;
}

/**
 * Markdown allows 3, but prefers 4, spaces per indent.  Given an indent, gets
 * its preferred divisor.  For example, for \a indent values of 3, 6, 7, or 9
 * spaces, returns 3; for value of 4, 5, 8, or 12, returns 4.
 *
 * As a special case, we also allow 2 spaces per indent for definition and
 * unordered lists.
 *
 * @param indent_left The raw indent (in spaces).
 * @return Returns the preferred divisor.
 */
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
 * six \c # characters starting in column 1 and followed by a whitespace
 * character.
 *
 * @param s The null-terminated line to check.
 * @return Returns \c true only if it is.
 */
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
 * more \c ~ or \c ` characters.
 *
 * @param s The null-terminated line to check.
 * @param fence A pointer to the \c struct containing the fence info: if
 * \c fence->cf_c, return new fence info; otherwise checks to see if \a s
 * matches the existing fence info.

 * @return Returns \c true only if it is.
 */
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
 * @return Returns \c true only if \a s is a PHP Markdown Extra definition list
 * item.
 */
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
 * @return Returns \c true only if \a s is a PHP Markdown Extra definition list
 * item.
 */
static bool md_is_dl_ul_helper( char const *s, md_indent_t *indent_hang ) {
  assert( indent_hang != NULL );
  if ( is_space( s[1] ) ) {
    if ( s[1] == '\t' )
      *indent_hang = MD_LIST_INDENT_MAX;
    else {
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
 * Checks whether the line is a PHP Markdown Extra footnote definition.
 *
 * @param s The null-terminated line to check.
 * @param def_text A pointer to the variable to receive whether the footnote
 * definition line contains any text other than the marker.
 * @return Returns \c true onlf if \a s is a PHP Markdown Extra footnote
 * definition.
 */
static bool md_is_footnote_def( char const *s, bool *def_text ) {
  assert( s != NULL );
  assert( s[0] == '[' );
  assert( def_text != NULL );

  if ( *++s == '^' ) {
    while ( *++s != '\0' ) {
      if ( *s == ']' ) {
        if ( !(*++s == ':' && isspace( *++s )) )
          break;
        SKIP_CHARS( s, WS_STR );
        *def_text = *s && !isspace( *s );
        return true;
      }
    } // while
  }
  return false;
}

/**
 * Checks whether the line is a Markdown horizontal rule, a series of 3 or more
 * \c *, \c -, or \c _ characters that may optionally be separated by
 * whitespace.
 *
 * @param s The null-terminated line to check where \a s[0] is used as the rule
 * character to match.
 * @return Returns \c true only if it is.
 */
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
 * @return Returns \c true only if the line is a PHP Markdown Extra HTML
 * abbreviation.
 */
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
 * @return Returns \c true only if \a s ends \a html_state.
 */
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

  assert( (size_t)html_state < ARRAY_SIZE( HTML_END_STR ) );
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
 * special markup or HTML_NONE if not.
 */
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
    element[ len++ ] = tolower( *s++ );
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
 * @return Returns \c true only if \a s is a Markdown link label.
 */
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
 * Checks whether the line is a Markdown ordered list item: a sequence of
 * digits followed by either \c '.' or \c ')' and whitespace.
 *
 * @param s The null-terminated line to check.
 * @param ol_num A pointer to the variable to receive the ordered list number.
 * @param ol_c A pointer to the variable to receive the ordered list character.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns \c true only if \a s is a Markdown ordered list item.
 */
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
    *ol_num = *ol_num * 10 + (*d - '0');

  size_t const len = d - s;
  if ( len >= 1 && len <= MD_OL_CHAR_MAX && md_is_ol_sep_char( d[0] ) &&
       is_space( d[1] ) ) {
    *ol_c = d[0];
    *indent_hang = d[1] == '\t' ?
      MD_LIST_INDENT_MAX :
      MD_OL_INDENT_MIN + is_space( d[2] );
    return true;
  }
  return false;
}

/**
 * Checks whether the line is a PHP Markdown Extra table line.
 *
 * @param s The null-terminated line to check.
 * @return Returns \c true only if \a s is a PHP Markdown Extra table line.
 */
static bool md_is_table( char const *s ) {
  assert( s != NULL );
  bool nws = false;                     // encountered non-whitespace?

  for ( ; *s != '\0'; ++s ) {
    switch ( *s ) {
      case '\\':
        ++s;
        break;
      case '|':
        if ( nws ) {
          //
          // Insist on encountering at least one non-whitespace character on
          // the line before returning that it's a table.
          //
          return true;
        }
        break;
      default:
        if ( !isspace( *s ) )
          nws = true;
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
 * @return Returns \c true only if \a s is a Markdown unordered list item.
 */
static bool md_is_ul( char const *s, md_indent_t *indent_hang ) {
  assert( s != NULL );
  assert( s[0] == '*' || s[0] == '-' || s[0] == '+' );
  return md_is_dl_ul_helper( s, indent_hang );
}

/**
 * Checks whether the line is a Markdown Setext header line, a sequence of one
 * or more \c = or \c - characters starting in column 1.
 *
 * @param s The null-terminated line to check where \a s[0] is used as the
 * header character to match.
 * @return Returns \c true onlt if it is.
 */
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
 * @return Returns said line true or MD_NONE if none.
 */
static md_line_t md_nested_within( void ) {
  for ( stack_pos_t pos = stack_top; pos >= 0; --pos ) {
    md_line_t const line_type = STACK(pos).line_type;
    if ( md_is_nestable( line_type ) )
      return line_type;
  } // for
  return MD_NONE;
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
    size_t const old_digits = 1 + (old_n > 9) + (old_n > 99);

    // convert new_n to a string
    char new_buf[11];                   // enough for sizeof(md_ol_t) == 4
    snprintf( new_buf, sizeof new_buf, "%u", new_n );
    size_t const new_digits = strlen( new_buf );

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
 * @param is_end_tag A pointer to a \c bool: if \c true, we are skipping an
 * end tag; if \c false, set to \c true if the tag is an XML end tag on return.
 * @return Returns a pointer to just past the closing '>' of the tag or a
 * pointer to an empty string if \a s is not an HTML (or XML) tag.
 */
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
          // FALLTHROUGH
        case '>':                       // found closing '>'
          return ++s;
      } // switch
    }
  } // for
  return s;
}

/**
 * Pushes a new Markdown state onto the stack.
 *
 * @param line_type The type of line.
 * @param indent_left The left indent (in spaces).
 * @param indent_hang The indent relative to \a indent_left for a hang-indent.
 */
static void stack_push( md_line_t line_type, md_indent_t indent_left,
                        md_indent_t indent_hang ) {
  static size_t stack_capacity = 0;

  MD_DEBUG(
    "%s(): T=%c L=%u H=%u\n",
    __func__, line_type, indent_left, indent_hang
  );

  ++stack_top;
  if ( stack_capacity == 0 ) {
    stack_capacity = STATE_ALLOC_DEFAULT;
    stack = MALLOC( md_state_t, stack_capacity );
  } else if ( (size_t)stack_top >= stack_capacity ) {
    stack_capacity += STATE_ALLOC_INCREMENT;
    REALLOC( stack, md_state_t, stack_capacity );
  }
  if ( stack == NULL )
    perror_exit( EX_OSERR );

  md_state_t *const top = &TOP;
  top->line_type   = line_type;
  top->seq_num     = ++next_seq_num;
  top->depth       = stack_size() ? stack_size() - 1 : 0;
  top->indent_left = indent_left;
  top->indent_hang = indent_hang;
  top->ol_c        = '\0';
  top->ol_num      = 0;
}

////////// extern functions ///////////////////////////////////////////////////

void markdown_cleanup( void ) {
  FREE( stack );
}

void markdown_init( void ) {
  html_state = HTML_NONE;
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
  stack_top = -1;
  stack_push( MD_TEXT, 0, 0 );
}

md_state_t const* markdown_parse( char *s ) {
  assert( s != NULL );

  static md_code_fence_t code_fence;
  md_indent_t indent_left;
  char *const nws = first_non_whitespace( s, &indent_left );

  PREV_BOOL( code_fence_end );
  PREV_BOOL( link_label_has_title );

  /////////////////////////////////////////////////////////////////////////////

  switch ( TOP.line_type ) {
    case MD_CODE:
      //
      // Check to see whether we've hit the end of a PHP Markdown Extra code
      // fence.
      //
      if ( code_fence_end )
        stack_pop();
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
        return &TOP;
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
      stack_pop();
      break;

    case MD_LINK_LABEL:
      //
      // Markdown link label title attributes.
      //
      if ( !link_label_has_title && md_is_link_title( nws ) )
        return &TOP;
      stack_pop();
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
        return &TOP;
      }
      stack_pop();
      break;

    default:
      /* suppress warning */;
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
    return &TOP;
  }

  /////////////////////////////////////////////////////////////////////////////

  if ( top_is( MD_HTML_BLOCK ) ) {
    //
    // HTML blocks.
    //
    switch ( html_state ) {
      case HTML_ELEMENT:
        if ( blank_line )
          stack_pop();
        return &TOP;
      case HTML_END:
        stack_pop();
        break;
      default:
        if ( md_is_html_end( html_state, s ) )
          html_state = HTML_END;
        //
        // As long as we're in the MD_HTML_BLOCK state, we can just return
        // without further checks.
        //
        return &TOP;
    } // switch
  }
  else {
    //
    // Markdown code blocks.
    //
    md_indent_t const code_indent_min = md_code_indent_min();
    if ( indent_left >= code_indent_min ) {
      if ( !top_is( MD_CODE ) )
        stack_push( MD_CODE, code_indent_min, 0 );
      //
      // As long as we're in the MD_CODE state, we can just return without
      // further checks.
      //
      return &TOP;
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
        bool fn_def_text;
        if ( md_is_footnote_def( nws, &fn_def_text ) ) {
          stack_clear();
          stack_push( MD_FOOTNOTE_DEF, 0, MD_FOOTNOTE_INDENT );
          TOP.fn_def_text = fn_def_text;
          return &TOP;
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
    case '<': {
      bool is_end_tag;
      html_state = md_is_html_tag( nws, &is_end_tag );
      if ( html_state != HTML_NONE ) {
        if ( is_end_tag )               // HTML ends on same line as it begins
          html_state = HTML_END;
        stack_push( MD_HTML_BLOCK, indent_left, 0 );
        return &TOP;
      }
      break;
    }
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

    // Unordered lists.
    case '*':
    case '+':
    case '-':
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
  if ( (!blank_line && md_is_nestable( TOP.line_type )) ||
       md_is_nestable( curr_line_type ) ) {
    ++depth;
  }

  //
  // Pop states for decreases in indentation.
  //
  MD_DEBUG( "pop stack? D=%u TOP.D=%u\n", depth, TOP.depth );
  while ( depth < TOP.depth ) {
    MD_DEBUG( "+ D=%u < TOP.D=%u => ", depth, TOP.depth );
    stack_pop();
  } // while

  md_indent_t const nested_indent_min = TOP.depth * MD_LIST_INDENT_MAX;
  bool const is_nested = indent_left >= nested_indent_min;
  bool const is_same_type_not_nested = top_is( curr_line_type ) && !is_nested;

  switch ( curr_line_type ) {

    case MD_NONE:
      if ( blank_line && md_is_table( s ) ) {
        assert( !top_is( MD_TABLE ) );
        if ( is_nested )
          stack_push( MD_TABLE, indent_left, 0 );
      }
      break;

    case MD_OL: {
      bool const ol_same_char = TOP.ol_c == ol_c;
      if ( is_same_type_not_nested && ol_same_char ) {
        TOP.seq_num = ++next_seq_num;   // reuse current state
        md_ol_t const next_ol_num = ++TOP.ol_num;
        if ( next_ol_num == 10       ||
             next_ol_num == 100      ||
             next_ol_num == 1000     ||
             next_ol_num == 10000    ||
             next_ol_num == 100000   ||
             next_ol_num == 1000000  ||
             next_ol_num == 10000000 ||
             next_ol_num == 100000000 ) {
          ++TOP.indent_hang;            // digits just got 1 wider
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
          stack_pop();
        }
        stack_push( MD_OL, indent_left, indent_hang );
        TOP.ol_c   = ol_c;
        TOP.ol_num = ol_num;
      }
      break;
    }

    case MD_DL:
    case MD_UL:
      if ( is_same_type_not_nested )
        TOP.seq_num = ++next_seq_num;   // reuse current state
      else
        stack_push( curr_line_type, indent_left, indent_hang );
      break;

    default:
      /* suppress warning */;
  } // switch

  return &TOP;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
