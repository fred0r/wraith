#! /bin/sh
# Generate autotools aux files required by ./configure
# Run this on a fresh clone before ./configure if the files in
# build/autotools/ are missing.

auxdir="build/autotools"
files="config.guess config.sub depcomp install-sh"

for f in $files; do
  if ! [ -f "$auxdir/$f" ]; then
    echo "Generating $auxdir/$f..."
    cp -f "$(automake --print-libdir)/$f" "$auxdir/$f" || {
      echo "Error: unable to generate $auxdir/$f (automake required)" >&2
      exit 1
    }
  fi
done
