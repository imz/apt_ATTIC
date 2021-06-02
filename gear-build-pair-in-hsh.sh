#!/bin/sh -efuCx

readonly X_repo="$1"; shift
readonly X_rev="$1"; shift

readonly Y_repo="$1"; shift
readonly Y_rev="$1"; shift

#readonly HSHDIR="$1"; shift
# the rest ("$@") is for hsh

hsh --clean "$@"

(cd "$X_repo"
 gear -t "$X_rev" --no-compress --hasher -- \
      hsh --with-stuff "$@" \
      --build-args="--define 'disttag sisyphus+$(git rev-parse "$X_rev")'"
)

(cd "$Y_repo"
 gear -t "$Y_rev" --no-compress --hasher -- \
      hsh --with-stuff "$@" \
      --build-args="--define 'disttag sisyphus+$(git rev-parse "$Y_rev")'"
)
