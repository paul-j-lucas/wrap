##
#	wrap -- text reformatter
#	src/Makefile.am
#
#	Copyright (C) 2013-2019  Paul J. Lucas
#
#	This program is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
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
check_PROGRAMS = regex_test

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_builddir)/lib
LDADD = $(top_builddir)/lib/libgnu.a

COMMON_SOURCES = \
	alias.c alias.h \
	common.c common.h \
	options.c options.h \
	pattern.c pattern.h \
	read_conf.c read_conf.h \
	util.c util.h \
	wrap.h

wrap_SOURCES = $(COMMON_SOURCES) \
	markdown.c markdown.h \
	unicode.c unicode.h \
	wrap.c \
	wregex.c wregex.h

wrapc_SOURCES = $(COMMON_SOURCES) \
	align.c \
	cc_map.c cc_map.h \
	doxygen.c doxygen.h \
	wrapc.c

regex_test_SOURCES = \
	regex_test.c \
	unicode.c unicode.h \
	util.c util.h \
	wrap.h \
	wregex.c wregex.h

# vim:set noet sw=8 ts=8:
