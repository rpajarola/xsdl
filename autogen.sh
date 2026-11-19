#!/bin/sh
# Generate configure and Makefile.in, then run configure with any arguments.
set -e
srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
autoreconf --force --install --verbose "$srcdir"
if test -z "$NOCONFIGURE"; then
    exec "$srcdir"/configure "$@"
fi
