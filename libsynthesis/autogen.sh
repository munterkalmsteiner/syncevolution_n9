#!/bin/sh

set -e

# wipe out temporary autotools files, necessary
# when switching between distros
rm -rf m4 aclocal.m4 autom4te.cache config.guess config.sub config.h.in configure depcomp install-sh ltmain.sh missing
mkdir m4

(cd src && ./gen-makefile-am.sh)

libtoolize -c
aclocal -I m4 -I m4-repo
autoheader
automake -a -c
autoconf
