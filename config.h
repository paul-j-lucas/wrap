/*
**      wrap -- text reformatter
**      config.h
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
**      along with this program; if not, write to the Free Software
**      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef wrap_config_H
#define wrap_config_H

/*****************************************************************************/

/**
 * Reads the configuration file.
 *
 * @param config_file The full-path of the configuration file to read.  If
 * NULL, then the user's home directory is checked for the presence of the
 * default configuration file.  If found, that file is read.
 * @return Returns the full-path of the configuration file that was read.
 */
char const* read_config( char const *config_file );

/*****************************************************************************/

#endif /* wrap_config_H */
/* vim:set et sw=2 ts=2: */
