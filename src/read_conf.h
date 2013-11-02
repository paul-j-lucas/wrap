/*
**      wrap -- text reformatter
**      read_conf.h
**
**      Copyright (C) 1996-2013  Paul J. Lucas
**
**      This program is free software; you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the Licence, or
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

#ifndef wrap_read_conf_H
#define wrap_read_conf_H

/*****************************************************************************/

/**
 * Reads the configuration file.
 *
 * @param conf_file The full-path of the configuration file to read.  If NULL,
 * then the user's home directory is checked for the presence of the default
 * configuration file.  If found, that file is read.
 * @return Returns the full-path of the configuration file that was read.
 */
char const* read_conf( char const *conf_file );

/*****************************************************************************/

#endif /* wrap_read_conf_H */
/* vim:set et sw=2 ts=2: */
