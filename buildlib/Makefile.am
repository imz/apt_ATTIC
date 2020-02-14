
all-local: copy-headers

EXTRA_DIST = \
	     archtable \
	     ostable \
	     config.h.in \
	     apti18n.h \
	     tools.m4

CLEANFILES  = $(top_builddir)/include/apti18n.h


COPYHEADERS = copy-apti18n

copy-headers: $(COPYHEADERS)

copy-apti18n:
	@$(mkinstalldirs) $(top_builddir)/include
	cp -f -p /usr/share/gettext/gettext.h $(srcdir)/apti18n.h \
		$(top_builddir)/include/
