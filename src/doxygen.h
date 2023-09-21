/*
**      wrap -- text reformatter
**      src/doxygen.h
**
**      Copyright (C) 2018-2023  Paul J. Lucas
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

#ifndef wrap_doxygen_H
#define wrap_doxygen_H

/**
 * @file
 * Declares macros, data structures, and functions for reformatting
 * [Doxygen](http://www.doxygen.org/).
 */

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */

/// @endcond

/**
 * @defgroup doxygen-group Doxygen Support
 * Macros, data structures, and functions for reformatting
 * [Doxygen](http://www.doxygen.org/).
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Doxygen command maximum size.
 * (The longest command is currently `hidecollaborationgraph`.)
 */
#define DOX_CMD_NAME_SIZE_MAX     22

/**
 * Doxygen command type.
 */
enum dox_cmd_type {
  /**
   * Doxygen command is "inline" and needs no special treatment.  Examples
   * include `\a`, `\b`, and `\c`.
   */
  DOX_INLINE  = (1u << 0),

  /**
   * Doxygen command should be at the beginning of a line.  Examples include
   * `\pure`.
   */
  DOX_BOL     = (1u << 1),

  /**
   * Like #DOX_BOL, but Doxygen command continues until the end of the line.
   * Examples include `\def`, `\hideinitializer`, and `\sa`.
   */
  DOX_EOL     = (1u << 2),

  /**
   * Like #DOX_BOL, but Doxygen command continues until either the end of the
   * paragraph; or, if it has a corresponding end command, until said command.
   * Examples include `\brief`, `\details`, and `\param`.
   */
  DOX_PAR     = (1u << 3),

  /**
   * Like #DOX_PAR, but doxygen command continues until its corresponding end
   * command and all text in between shall be considered preformatted and
   * passed through verbatim.  Examples include `\code`, `\latexonly`, and
   * `\verbatim`.
   */
  DOX_PRE     = (1u << 4),
};
typedef enum dox_cmd_type dox_cmd_type_t;

/**
 * Contains information about a [Doxygen](http://www.doxygen.org/) command.
 *
 * @sa [Special Commands](https://www.doxygen.nl/manual/commands.html)
 */
struct dox_cmd {
  char const     *name;                 ///< Command's name.
  dox_cmd_type_t  type;                 ///< Command's type.
  char const     *end_name;             ///< Corresponding end command, if any.
};
typedef struct dox_cmd dox_cmd_t;

///////////////////////////////////////////////////////////////////////////////

/**
 * Attempts to find \a s among Doxygen's set of commands.
 *
 * @param s The string (without the leading `\` or `@`) that may be a Doxygen
 * command's name.
 * @return Returns a pointer to the Doxygen command or null if not found.
 */
NODISCARD
dox_cmd_t const* dox_find_cmd( char const *s );

/**
 * Attempts to parse a Doxygen command at the start of \a s.
 *
 * @param s The string to parse.
 * @param dox_cmd_name If \a s starts with optional whitespace followed by a
 * Doxygen command, then the command is copied into \a dox_cmd_name (without
 * the leading `\` or `@`).
 * @return Returns `true` only if \a s starts with a Doxygen command.
 */
NODISCARD
bool dox_parse_cmd_name( char const *s, char *dox_cmd_name );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* wrap_doxygen_H */
/* vim:set et sw=2 ts=2: */
