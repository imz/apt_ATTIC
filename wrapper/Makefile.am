bin_PROGRAMS = apt-get apt-shell

AM_CFLAGS = $(WARN_CFLAGS)
AM_CXXFLAGS = $(WARN_CXXFLAGS)

apt_get_SOURCES = apt-get.c
apt_get_CFLAGS = -DWRAP='"/usr/lib/apt/apt-get"'
apt_shell_SOURCES = apt-get.c
apt_shell_CFLAGS = -DWRAP='"/usr/lib/apt/apt-shell"'
