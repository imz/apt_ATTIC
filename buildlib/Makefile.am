
all-local: copy-headers

EXTRA_DIST = \
	     archtable \
	     ostable \
	     config.h.in \
	     gettext.h \
	     tools.m4

CLEANFILES  = $(top_builddir)/include/apti18n.h


COPYHEADERS = copy-apti18n

copy-headers: $(COPYHEADERS)

copy-apti18n:
	@$(mkinstalldirs) $(top_builddir)/include
	@echo "Copying gettext.h to $(top_builddir)/include/apti18n.h"
	@cp -f -p $(srcdir)/gettext.h $(top_builddir)/include/apti18n.h
