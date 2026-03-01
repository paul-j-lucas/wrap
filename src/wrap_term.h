/*
**      wrap -- text reformatter
**      src/wrap_term.h
**
**      Copyright (C) 1996-2026  Paul J. Lucas
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

#ifndef wrap_term_H
#define wrap_term_H

/**
 * @file
 * Declares functions for dealing with the terminal.
 */

// local
#include "pjl_config.h"                 /* IWYU pragma: keep */

/**
 * @defgroup terminal-group Terminal Capabilities
 * Function for getting the number of columns of the terminal.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Gets the number of columns of the terminal.
 *
 * @return Returns said number of columns or 0 if it can not be determined.
 */
NODISCARD
unsigned term_get_columns( void );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* wrap_term_H */
/* vim:set et sw=2 ts=2: */
