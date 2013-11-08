## Process this file with automake to produce Makefile.in

SUBDIRS = pixmaps po src help

man_MANS = mate-system-monitor.1

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	$(man_MANS) \
	mate-doc-utils.make \
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

distuninstallcheck_listfiles = find . -type f -print | grep -v 'scrollkeeper'

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update \
  mate-system-monitor.desktop

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper
