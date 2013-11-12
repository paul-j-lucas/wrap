##
#	wrap -- text reformatter
#	test/Makefile.am
#
#	Copyright (C) 2013  Paul J. Lucas
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

TESTS =	utf8-w77.test \
	utf8-w78.test \
	utf8-w79.test \
	utf8-w80.test \
	wrap-H3.test \
	wrap-I2-m1.test \
	wrap-I2-w72.test \
	wrap-bI2.test \
	wrap-d.test \
	wrap-dep.test \
	wrap-no_options.test \
	wrap-t1.test

AM_TESTS_ENVIRONMENT = WRAP=$(top_builddir)/src/wrap ;
TEST_EXTENSIONS = .test
TEST_LOG_DRIVER = $(srcdir)/run_test.sh

# vim:set noet sw=8 ts=8:
