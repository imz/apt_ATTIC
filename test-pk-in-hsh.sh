#!/bin/sh -efuCx
set -o pipefail

# How to test PackageKit (in hasher).
#
# The described way to test has actually been able to expose a crash,
# which happened with some of the changes in APT, but did so only with
# the real Sisyphus indices fetched via network, and there was no crash
# observed when running this test against just the local RPM db in the hasher.
#
# One can first build packagekit with the current APT in hasher (--with-stuff)
# and then test it there with this script or the like.
#
# (The idea could be implemented in an automated test
# required by apt-checkinstall and packagekit-checkinstall.)

readonly HSHDIR="$1"; shift

#hsh --ini --with-stuff "$HSHDIR"
hsh-install "$HSHDIR" apt-conf-sisyphus apt-repo packagekit

share_network=1 \
hsh-run --root "$HSHDIR" -- \
/bin/sh -xc 'apt-repo add sisyphus &&
	    apt-repo &&
	    apt-get update &&
	    /usr/lib/packagekit-direct search-detail packagekit;
	    ret=$?;
	    echo "$ret";
	    exit "$ret"'

# Note that git-bisect(1) expects an "exit with a code between 1 and
# 127 (inclusive), except 125, if the current source code is bad".
# Therefore, I use this script like this:
#
#   git bisect start --no-checkout PK-BAD PK-GOOD
#   git bisect run /bin/sh -exc './gear-build-pair-in-hsh.sh . BISECT_HEAD ../packagekit/ revert-apt-API ~/hasher/; ./test-pk-in-hsh.sh ~/hasher/ || { echo BAD: $?; exit 1; }'
