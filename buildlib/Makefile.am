
all-local: copy-headers

EXTRA_DIST = \
	     archtable \
	     ostable \
	     config.h.in \
	     gettext.h \
	     statvfs.h \
	     tools.m4

CLEANFILES  = $(top_builddir)/include/apti18n.h \
              $(top_builddir)/include/sys/statvfs.h


COPYHEADERS = copy-apti18n

if NEED_STATVFS_H
COPYHEADERS += copy-statvfs
endif

copy-headers: $(COPYHEADERS)

copy-apti18n:
	@$(mkinstalldirs) $(top_builddir)/include
	@echo "Copying gettext.h to $(top_builddir)/include/apti18n.h"
	@cp -f -p $(srcdir)/gettext.h $(top_builddir)/include/apti18n.h

copy-statvfs:
	@$(mkinstalldirs) $(top_builddir)/include/sys
	@echo "Copying statvfs.h to $(top_builddir)/include/sys/statvfs.h"
	@cp -f -p $(srcdir)/statvfs.h $(top_builddir)/include/sys/statvfs.h
