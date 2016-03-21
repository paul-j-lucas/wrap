##
#	wrap -- text reformatter
#	test/Makefile.am
#
#	Copyright (C) 2013-2016  Paul J. Lucas
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

AUTOMAKE_OPTIONS = 1.12			# needed for TEST_LOG_DRIVER

TESTS =	tests/ETB-no_options-01.test \
	tests/ETB-no_options-02.test \
	tests/utf8-w77.test \
	tests/utf8-w78.test \
	tests/utf8-w79.test \
	tests/utf8-w80.test \
	tests/wrap-a.test \
	tests/wrap-alias-dup.test \
	tests/wrap-alias-no_equal.test \
	tests/wrap-alias-options_exp.test \
	tests/wrap-alias-unexp_char.test \
	tests/wrap-conf-not_found.test \
	tests/wrap-conf-no_section.test \
	tests/wrap-d.test \
	tests/wrap-dep.test \
	tests/wrap-f-F.test \
	tests/wrap-file-not_found.test \
	tests/wrap-h1-I5.test \
	tests/wrap-H3-t1-T.test \
	tests/wrap-H3.test \
	tests/wrap-I2-m1.test \
	tests/wrap-I2-W.test \
	tests/wrap-I2-w72.test \
	tests/wrap-i2.test \
	tests/wrap-long_line-01.test \
	tests/wrap-long_line-02.test \
	tests/wrap-long_line-03.test \
	tests/wrap-long_line-04.test \
	tests/wrap-n.test \
	tests/wrap-n-N.test \
	tests/wrap-no_options.test \
	tests/wrap-pattern-alias_exp.test \
	tests/wrap-pattern-no_equal.test \
	tests/wrap-pattern-unexp_char.test \
	tests/wrap-P-01.test \
	tests/wrap-P-02.test \
	tests/wrap-P-03.test \
	tests/wrap-t1.test \
	tests/wrap-t11.test \
	tests/wrapc-no_options-01.test \
	tests/wrapc-no_options-02.test \
	tests/wrapc-no_options-03.test \
	tests/wrapc-no_options-04.test \
	tests/wrapc-no_options-05.test \
	tests/wrapc-C_style-01.test \
	tests/wrapc-C_style-02.test \
	tests/wrapc-C_style-03.test \
	tests/wrapc-C_style-04.test \
	tests/wrapc-C_style-05.test \
	tests/wrapc-D-01.test

AM_TESTS_ENVIRONMENT = BUILD_SRC=$(top_builddir)/src; export BUILD_SRC ;
TEST_EXTENSIONS = .test
TEST_LOG_DRIVER = $(srcdir)/run_test.sh

EXTRA_DIST = run_test.sh tests data expected
dist-hook:
	cd $(distdir)/tests && rm -f *.log *.trs

# vim:set noet sw=8 ts=8:
