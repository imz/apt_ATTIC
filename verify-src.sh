#!/bin/sh -euC
set -o pipefail
shopt -s nullglob

# Verify that the source code conforms to some policies.
# (Some policies--other or the same--are verified by compiler flags.)
# It's to be invoked from the .spec file; and one could invoke it
# from a Git hook on commit.

MY="$(dirname -- "$(realpath -- "$0")")"
readonly MY

for f in "$MY"/verify-src.d/*; do
    [ -x "$f" ] || continue
    case "$f" in
	*~)
	    continue
	    ;;
	*.verify)
	    "$f"
	    ;;
	*) # ignore errors
	    "$f" ||:
	    ;;
    esac
done
