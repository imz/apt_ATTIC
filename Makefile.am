
SUBDIRS = buildlib apt-pkg methods cmdline doc po

ACLOCAL_AMFLAGS = -I m4 -I buildlib

EXTRA_DIST = \
		AUTHORS.RPM \
		COPYING.GPL \
		rpmpriorities \
		test/conf.cc \
		test/extract-control.cc \
		test/hash.cc \
		test/makefile \
		test/mthdcat.cc \
		test/rpmver.cc \
		test/scratch.cc \
		test/testdeb.cc \
		test/testextract.cc \
		test/uri.cc \
		test/versions.lst \
		test/versiontest.cc

EXTRA_DIST += contrib/gpg-check/README
EXTRA_DIST += contrib/gpg-check/gpg-check.conf
EXTRA_DIST += contrib/gpg-check/gpg-import.lua
EXTRA_DIST += contrib/gpg-check/gpg-check.lua
EXTRA_DIST += contrib/gpg-check/gpg-import.conf

EXTRA_DIST += contrib/apt-groupinstall/apt-groupinstall.conf
EXTRA_DIST += contrib/apt-groupinstall/README
EXTRA_DIST += contrib/apt-groupinstall/groupinstall-backend-suse
EXTRA_DIST += contrib/apt-groupinstall/groupinstall-backend-comps.py
EXTRA_DIST += contrib/apt-groupinstall/apt-groupinstall.lua

EXTRA_DIST += contrib/log/README
EXTRA_DIST += contrib/log/log.lua
EXTRA_DIST += contrib/log/log.conf

EXTRA_DIST += contrib/apt-wrapper/apt.1
EXTRA_DIST += contrib/apt-wrapper/apt
EXTRA_DIST += contrib/apt-wrapper/apt.1.xml
EXTRA_DIST += contrib/apt-wrapper/apt.ent

EXTRA_DIST += contrib/list-extras/README
EXTRA_DIST += contrib/list-extras/list-extras.conf
EXTRA_DIST += contrib/list-extras/list-extras.lua

EXTRA_DIST += contrib/allow-duplicated/README
EXTRA_DIST += contrib/allow-duplicated/allow-duplicated.conf
EXTRA_DIST += contrib/allow-duplicated/allow-duplicated.lua

EXTRA_DIST += contrib/list-nodeps/README
EXTRA_DIST += contrib/list-nodeps/list-nodeps.lua
EXTRA_DIST += contrib/list-nodeps/list-nodeps.conf

EXTRA_DIST += contrib/apt-files/apt-files.lua
EXTRA_DIST += contrib/apt-files/README
EXTRA_DIST += contrib/apt-files/convert.py
EXTRA_DIST += contrib/apt-files/apt-files.conf

EXTRA_DIST += contrib/install-devel/README
EXTRA_DIST += contrib/install-devel/install-devel.lua

EXTRA_DIST += contrib/preserve-conf/preserve-conf.lua
EXTRA_DIST += contrib/preserve-conf/preserve-conf.conf

EXTRA_DIST += contrib/proxypasswd/README
EXTRA_DIST += contrib/proxypasswd/proxypasswd.lua

EXTRA_DIST += contrib/guess/README
EXTRA_DIST += contrib/guess/guess.conf
EXTRA_DIST += contrib/guess/guess.lua

aptscriptsdir = $(pkgdatadir)/scripts
aptscripts_SCRIPTS =

aptconfigdir = ${sysconfdir}/apt

aptsources_list_ddir = $(aptconfigdir)/sources.list.d
aptsources_list_d_DATA =

aptconf_ddir = $(aptconfigdir)/apt.conf.d
aptconf_d_DATA =

aptvendors_list_ddir = $(aptconfigdir)/vendors.list.d
aptvendors_list_d_DATA =

aptcachedir = $(localstatedir)/cache/apt
aptcache_DATA =

aptcache_archives_partialdir = $(aptcachedir)/archives/partial
aptcache_archives_partial_DATA =

aptcache_genpkglistdir = $(aptcachedir)/genpkglist
aptcache_genpkglist_DATA =

aptcache_gensrclistdir = $(aptcachedir)/gensrclist
aptcache_gensrclist_DATA =

aptstatedir = $(localstatedir)/lib/apt
aptstate_DATA =

aptstate_lists_partialdir = $(aptstatedir)/lists/partial
aptstate_lists_partial_DATA =
