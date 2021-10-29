#!/bin/bash
set -eu

readonly CKSUM_TYPE="$1"; shift

# The "cdrom" method seemingly doesn't compute
# and pass the cksums to apt.
case "$APT_TEST_METHOD" in
	cdrom*) APT_TEST_XFAIL=yes
	       ;;
esac

TESTDIR=$(readlink -f $(dirname $0))
. $TESTDIR/framework

setupenvironment

buildpackage 'simple-package-noarch'

generaterepository_and_switch_sources "$TMPWORKINGDIRECTORY/usr/src/RPM/RPMS"

# Fake the cksum of the rpm.
#
# (For faking the rpm itself, see other tests,
# like test-apt-install-rejects-fake-rpm-archive.)
fake_repo_rpm_cksum "$CKSUM_TYPE" simple-package-noarch

testsuccess aptget update

# Cksum verification shouldn't pass.

testpkgnotinstalled 'simple-package-noarch'
testregexmatch '.*MD5Sum mismatch.*' aptget install simple-package-noarch
testfailure
testpkgnotinstalled 'simple-package-noarch'
