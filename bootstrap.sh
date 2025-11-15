#!/bin/sh
# Simple bootstrap script to generate configure script

echo "Generating configure script..."

if command -v autoreconf >/dev/null 2>&1; then
    autoreconf -i
else
    echo "Error: autoreconf not found. Please install autoconf and automake."
    exit 1
fi

echo "Done. You can now run:"
echo "  ./configure"
echo "  make"
