## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = po pixmaps src tools help

man_MANS = mate-system-monitor.1

appdatadir = $(datadir)/metainfo
appdata_in_files = mate-system-monitor.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

$(appdata_DATA): $(appdata_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

org.mate.mate-system-monitor.policy.in: org.mate.mate-system-monitor.policy.in.in Makefile
	$(AM_V_GEN) sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-compile-warnings=no \
	CFLAGS='-Wno-deprecated-declarations'

EXTRA_DIST = \
	autogen.sh \
	$(man_MANS) \
	$(appdata_in_files) \
	org.mate.mate-system-monitor.policy.in.in \
	mate-system-monitor.desktop.in \
	omf.make \
	xmldocs.make

polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_in_files = org.mate.mate-system-monitor.policy.in
polkit_policy_DATA = $(polkit_policy_in_files:.policy.in=.policy)

$(polkit_policy_DATA): $(polkit_policy_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

Applicationsdir = $(datadir)/applications
Applications_in_files = mate-system-monitor.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)

$(Applications_DATA): $(Applications_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

distuninstallcheck_listfiles = find . -type f -print

CLEANFILES = mate-system-monitor.appdata.xml

DISTCLEANFILES = \
  mate-system-monitor.desktop \
  org.mate.mate-system-monitor.policy \
  org.mate.mate-system-monitor.policy.in

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

