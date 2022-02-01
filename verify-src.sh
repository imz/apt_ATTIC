#!/bin/sh -euC
set -o pipefail

# Verify that the source code conforms to some policies.
# (Some policies--other or the same--are verified by compiler flags.)
# It's to be invoked from the .spec file; and one could invoke it
# from a Git hook on commit.

MY="$(dirname -- "$(realpath -- "$0")")"
readonly MY

"$MY"/run-tests-dir "$MY"/verify-src.d
