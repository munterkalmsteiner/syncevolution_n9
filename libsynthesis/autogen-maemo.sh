#!/bin/sh

set -e

# wipe out temporary autotools files, necessary
# when switching between distros
rm -rf aclocal.m4 autom4te.cache config.guess config.sub config.h.in configure depcomp install-sh ltmain.sh missing

(cd src && ./gen-makefile-am.sh)

libtoolize -c
aclocal-1.10 -I m4-repo
autoheader
automake-1.10 -a -c
autoconf
