/*
**      wrap -- text reformatter
**      src/unicode.c
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
 * Contains constants for Unicode code-points as well as functions for
 * manipulating them.
 */

// local
#include "pjl_config.h"                 /* must go first */
#define W_UNICODE_H_INLINE _GL_EXTERN_INLINE
#include "unicode.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>

/// @endcond

/**
 * @addtogroup unicode-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/// Unicode code-point surrogate high start.
static char32_t const CP_SURROGATE_HIGH_START = 0x00D800u;

/// Unicode code-point surrogate low end.
static char32_t const CP_SURROGATE_LOW_END    = 0x00DFFFu;

/// Maximum valid Unicode code-point.
static char32_t const CP_VALID_MAX            = 0x10FFFFu;

/**
 * UTF-8 character length table indexed by the first octet of the character.
 */
char8_t const UTF8_LEN_TABLE[] = {
  /*      0 1 2 3 4 5 6 7 8 9 A B C D E F */
  /* 0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 1 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 2 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 3 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 4 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 5 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 6 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 7 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 8 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // continuation bytes
  /* 9 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  //        |
  /* A */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  //        |
  /* B */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  //        |
  /* C */ 0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  // C0 & C1 are overlong ASCII
  /* D */ 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  /* E */ 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
  /* F */ 4,4,4,4,4,4,4,4,5,5,5,5,6,6,0,0
};

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether a given Unicode code-point is valid.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if valid.
 */
NODISCARD
static inline bool cp_is_valid( char32_t cp ) {
  return  cp < CP_SURROGATE_HIGH_START
      || (cp > CP_SURROGATE_LOW_END && cp <= CP_VALID_MAX);
}

////////// extern functions ///////////////////////////////////////////////////

bool cp_is_eos( char32_t cp ) {
  switch ( cp ) {
    case '.'   :  // FULL STOP
    case 0xFF0E:  // FULLWIDTH FULL STOP

    case '?'   :  // QUESTION MARK
    case 0x037E:  // GREEK QUESTION MARK
    case 0x055E:  // ARMENIAN QUESTION MARK
    case 0x1367:  // ETHIOPIC QUESTION MARK
    case 0x1945:  // LIMBU QUESTION MARK
    case 0x2047:  // DOUBLE QUESTION MARK
    case 0x2048:  // QUESTION EXCLAMATION MARK
    case 0xA60F:  // VAI QUESTION MARK
    case 0xA6F7:  // BAMUM QUESTION MARK
    case 0xFE16:  // PRESENTATION FORM FOR VERTICAL QUESTION MARK
    case 0xFE56:  // SMALL QUESTION MARK
    case 0xFF1F:  // FULLWIDTH QUESTION MARK

    case '!'   :  // EXCLAMATION MARK
    case 0x055C:  // ARMENIAN EXCLAMATION MARK
    case 0x07F9:  // NKO EXCLAMATION MARK
    case 0x1944:  // LIMBU EXCLAMATION MARK
    case 0x203C:  // DOUBLE EXCLAMATION MARK
    case 0x2049:  // EXCLAMATION QUESTION MARK
    case 0x2757:  // HEAVY EXCLAMATION MARK SYMBOL
    case 0x2762:  // HEAVY EXCLAMATION MARK ORNAMENT
    case 0x2763:  // HEAVY HEART EXCLAMATION MARK ORNAMENT
    case 0xFE15:  // PRESENTATION FORM FOR VERTICAL EXCLAMATION MARK
    case 0xFE57:  // SMALL EXCLAMATION MARK
    case 0xFF01:  // FULLWIDTH EXCLAMATION MARK
      return true;

    default:
      return false;
  } // switch
}

bool cp_is_eos_ext( char32_t cp ) {
  switch ( cp ) {
    case '\''  :  // APOSTROPHE
    case 0x2019:  // RIGHT SINGLE QUOTATION MARK
    case 0x203A:  // SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
    case 0x275C:  // HEAVY SINGLE COMMA QUOTATION MARK ORNAMENT

    case '"'   :  // QUOTATION MARK
    case 0x00BB:  // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    case 0x201D:  // RIGHT DOUBLE QUOTATION MARK
    case 0x275E:  // HEAVY DOUBLE COMMA QUOTATION MARK ORNAMENT
    case 0x276F:  // HEAVY RIGHT-POINTING ANGLE QUOTATION MARK ORNAMENT
    case 0xFF02:  // FULLWIDTH QUOTATION MARK

    case ')'   :  // RIGHT PARENTHESIS
    case 0x2769:  // MEDIUM RIGHT PARENTHESIS ORNAMENT
    case 0x276B:  // MEDIUM FLATTENED RIGHT PARENTHESIS ORNAMENT
    case 0x27EF:  // MATHEMATICAL RIGHT FLATTENED PARENTHESIS
    case 0x2986:  // RIGHT WHITE PARENTHESIS
    case 0x2E29:  // RIGHT DOUBLE PARENTHESIS
    case 0xFD3F:  // ORNATE RIGHT PARENTHESIS
    case 0xFE5A:  // SMALL RIGHT PARENTHESIS
    case 0xFF09:  // FULLWIDTH RIGHT PARENTHESIS
    case 0xFF60:  // FULLWIDTH RIGHT WHITE PARENTHESIS

    case ']'   :  // RIGHT SQUATE BRACKET
    case 0x2046:  // RIGHT SQUARE BRACKET WITH QUILL
    case 0x27E7:  // MATHEMATICAL RIGHT WHITE SQUARE BRACKET
    case 0x298C:  // RIGHT SQUARE BRACKET WITH UNDERBAR
    case 0x298E:  // RIGHT SQUARE BRACKET WITH TICK IN BOTTOM CORNER
    case 0x2990:  // RIGHT SQUARE BRACKET WITH TICK IN TOP CORNER
    case 0x301B:  // RIGHT WHITE SQUARE BRACKET
    case 0xFF3D:  // FULLWIDTH RIGHT SQUARE BRACKET
      return true;

    default:
      return false;
  } // switch
}

bool cp_is_hyphen( char32_t cp ) {
  switch ( cp ) {
    case '-'   :  // HYPHEN-MINUS
    case 0x00AD:  // SOFT HYPHEN
    case 0x058A:  // ARMENIAN HYPHEN
    case 0x05BE:  // HEBREW PUNCTUATION MAQAF
    case 0x1400:  // CANADIAN SYLLABICS HYPHEN
    case 0x1806:  // MONGOLIAN SOFT HYPHEN
    case 0x2010:  // HYPHEN
//  case 0x2011:  // NON-BREAKING HYPHEN // obviously don't want to wrap here
    case 0x2013:  // EN DASH
    case 0x2014:  // EM DASH
    case 0x2015:  // HORIZONTAL BAR
    case 0x2027:  // HYPHENATION POINT
    case 0x2043:  // HYPHEN BULLET
    case 0x2053:  // SWUNG DASH
    case 0x2E17:  // DOUBLE OBLIQUE HYPHEN
    case 0x2E1A:  // HYPHEN WITH DIAERESIS
    case 0x2E40:  // DOUBLE HYPHEN
    case 0x301C:  // WAVE DASH
    case 0x3030:  // WAVY DASH
    case 0x30A0:  // KATAKANA-HIRAGANA DOUBLE HYPHEN
    case 0x30FB:  // KATAKANA MIDDLE DOT
    case 0xFE58:  // SMALL EM DASH
    case 0xFE63:  // SMALL HYPHEN-MINUS
    case 0xFF0D:  // FULLWIDTH HYPHEN-MINUS
    case 0xFF65:  // HALFWIDTH KATAKANA MIDDLE DOT
      return true;

    default:
      return false;
  } // switch
}

/**
 * Decodes a UTF-8 encoded character into its corresponding Unicode code-point.
 *
 * @note This out-of-line version is for non-ASCII characters.
 *
 * @param s A pointer to the first byte of the UTF-8 encoded character.
 * @return Returns said code-point or \ref #CP_INVALID if the UTF-8 byte
 * sequence is invalid.
 *
 * @sa utf8_decode()
 */
char32_t utf8_decode_impl( char const *s ) {
  assert( s != NULL );
  size_t const len = utf8_len( *s );
  assert( len >= 1 );

  char32_t cp = 0;
  char8_t const *c8 = POINTER_CAST( char8_t const*, s );

  switch ( len ) {
    case 6: cp += *c8++; cp <<= 6; FALLTHROUGH;
    case 5: cp += *c8++; cp <<= 6; FALLTHROUGH;
    case 4: cp += *c8++; cp <<= 6; FALLTHROUGH;
    case 3: cp += *c8++; cp <<= 6; FALLTHROUGH;
    case 2: cp += *c8++; cp <<= 6; FALLTHROUGH;
    case 1: cp += *c8;
  } // switch

  static char32_t const OFFSET_TABLE[] = {
    0, // unused
    0x0, 0x3080, 0xE2080, 0x3C82080, 0xFA082080, 0x82082080
  };
  cp -= OFFSET_TABLE[ len ];
  return cp_is_valid( cp ) ? cp : CP_INVALID;
}

/**
 * Given a pointer to any byte within a UTF-8 encoded string, synchronizes in
 * reverse to find the first byte of the UTF-8 character byte sequence the
 * pointer is pointing within.
 *
 * @note This out-of-line version is for non-ASCII characters.
 *
 * @param buf A pointer to the beginning of the buffer.
 * @param pos A pointer to any byte with the buffer.
 * @return Returns a pointer less than or equal to \a pos that points to the
 * first byte of a UTF-8 encoded character byte sequence or NULL if there is
 * none.
 *
 * @sa utf8_rsync()
 */
char const* utf8_rsync_impl( char const *buf, char const *pos ) {
  while ( pos > buf && utf8_is_cont( *pos ) )
    --pos;
  return utf8_is_start( *pos ) ? pos : NULL;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
