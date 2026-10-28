#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <output-directory>" >&2
    exit 1
fi

out=$1
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
legacy_commit=2d2b3bfb

if [ -e "$out" ]; then
    echo "Refusing to overwrite existing path: $out" >&2
    exit 1
fi

git clone --shared --no-checkout "$root" "$out"
git -C "$out" checkout "$legacy_commit"
git -C "$out" submodule update --init --recursive

(
    cd "$out"
    ./configure
    ${MAKE:-make}
)

echo "$out/wraith"
