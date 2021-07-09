#!/bin/sh -efuC
set -o pipefail

# sed is simpler than grep (especially, under xarg) w.r.t. the exit status.
# However, it's more complex to output the filename and line number.
# (= outputs a line number, but it counts total lines in all files.)
virtual_with_default_param="$(
    find -type f -'(' -name '*.h' -or -name '*.cc' -')' -print0 \
    | xargs -0 --no-run-if-empty \
      sed -nEe '/virtual [^(]*\([^)]*=|=.*\).*override/ {F; p; }'
)"
readonly virtual_with_default_param

[ -z "$virtual_with_default_param" ] || {
    printf '%s: Found default parameters in virtual methods:\n%s\n' \
	   "${0##*/}" \
	   "$virtual_with_default_param"
    exit 1
} >&2
