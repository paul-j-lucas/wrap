##
#	wrap -- text reformatter
#	src/Makefile.am
#
#	Copyright (C) 2013-2015  Paul J. Lucas
#
#	This program is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 of the Licence, or
#	(at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

bin_PROGRAMS = wrap wrapc

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_builddir)/lib -L$(top_srcdir)/lib

wrap_SOURCES = alias.c getopt.c read_conf.c pattern.c options.c util.c wrap.c
wrap_LDADD = $(top_builddir)/lib/libgnu.a

wrapc_SOURCES = getopt.c options.c util.c wrapc.c

# vim:set noet sw=8 ts=8:
