## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = pixmaps po src help

man_MANS = mate-system-monitor.1

@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/appdata
appdata_in_files = mate-system-monitor.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

EXTRA_DIST = \
	autogen.sh \
	$(man_MANS) \
	$(appdata_in_files) \
	mate-system-monitor.desktop.in \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	omf.make \
	xmldocs.make

Applicationsdir = $(datadir)/applications
Applications_in_files = mate-system-monitor.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

distuninstallcheck_listfiles = find . -type f -print

CLEANFILES = mate-system-monitor.appdata.xml

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update \
  mate-system-monitor.desktop

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		(GIT_DIR=$(top_srcdir)/.git \
		 ./missing --run git log --stat -M -C --name-status --date=short --no-color) | \
		 fmt --split-only > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi
