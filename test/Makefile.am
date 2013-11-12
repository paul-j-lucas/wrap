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

TESTS =	tests/utf8-w77.test \
	tests/utf8-w78.test \
	tests/utf8-w79.test \
	tests/utf8-w80.test \
	tests/wrap-alias-dup.test \
	tests/wrap-bI2.test \
	tests/wrap-conf-no_section.test \
	tests/wrap-conf-not_found.test \
	tests/wrap-dep.test \
	tests/wrap-d.test \
	tests/wrap-file-not_found.test \
	tests/wrap-H3.test \
	tests/wrap-I2-m1.test \
	tests/wrap-I2-w72.test \
	tests/wrap-no_options.test \
	tests/wrap-n.test \
	tests/wrap-t1.test

AM_TESTS_ENVIRONMENT = WRAP=$(top_builddir)/src/wrap ;
TEST_EXTENSIONS = .test
TEST_LOG_DRIVER = $(srcdir)/run_test.sh

# vim:set noet sw=8 ts=8:
