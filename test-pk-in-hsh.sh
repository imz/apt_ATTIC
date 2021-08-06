#!/bin/sh -efuC
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

run_sh_e()
{
    local ret
    share_network=1 \
		 hsh-run --root "$HSHDIR" -- \
		 /bin/sh -exc "$*" \
	&& ret=$? || ret=$?

    echo "$ret"
    return "$ret"
}

set -x

# Prepare our hasher env
#hsh --ini --with-stuff "$HSHDIR"
hsh-install "$HSHDIR" \
	    apt-conf-sisyphus \
	    apt-repo \
	    packagekit

# The common setup for all tests, do just once (although we may repeat this, too).
run_sh_e 'apt-repo add sisyphus
	  apt-repo
	  apt-get update'

# Test whether there is a crash during an action ("search-detail"):

run_sh_e 'stat /var/cache/apt/*.bin' # Check the setup is OK before a test.
run_sh_e /usr/lib/packagekit-direct search-detail packagekit

# Test whether the "refresh" action forces a rebuild of the caches & saves them:

run_sh_e 'stat /var/cache/apt/*.bin' # Check the setup is OK before a test.
# Run it (and make sure "refresh" doesn't crash or fail).
run_sh_e /usr/lib/packagekit-direct refresh
# Now, there are two ways to test whether it has actually rebuilt the caches.
#
# 1. The quick way is to see whether the caches appeared after "refresh" as they
# should. (Also, they must be new files, because "refresh" unconditionally
# removes them first. This test doesn't check this formally.)
run_sh_e 'stat /var/cache/apt/*.bin'

# 2. Another way is to see how much work a subsequent command does to build
# cache: should be little work if "refresh" works correctly. Contrarily,
# the "refresh" command (see before) should at its final stage provoke much work
# because it must build a new fresh cache; if its final stage is quick,
# then it works incorrectly and doesn't really rebuild and save a fresh cache.
# (This test doesn't formally check the amount of work. Just see with your eyes
# that--if correct--the printed percentages in the final stage of "refresh"
# change slowly, and in the initial stage of the next command change fast.)
run_sh_e '/usr/lib/packagekit-direct search-detail packagekit'

# * * *
#
# Note that git-bisect(1) expects an "exit with a code between 1 and
# 127 (inclusive), except 125, if the current source code is bad".
# Therefore, I use this script like this:
#
#   git bisect start --no-checkout PK-BAD PK-GOOD
#   git bisect run /bin/sh -exc './gear-build-pair-in-hsh.sh . BISECT_HEAD ../packagekit/ revert-apt-API ~/hasher/; ./test-pk-in-hsh.sh ~/hasher/ || { echo BAD: $?; exit 1; }'
