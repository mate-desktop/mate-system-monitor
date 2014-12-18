## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPROCMAN_DATADIR=\""$(datadir)/procman/"\" \
	-DMATELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATADIR=\""$(datadir)"\" \
	@PROCMAN_CFLAGS@

bin_PROGRAMS = mate-system-monitor

mate_system_monitor_SOURCES = \
	argv.h argv.cpp \
	procman.cpp  procman.h \
	interface.cpp interface.h \
	callbacks.cpp callbacks.h \
	load-graph.cpp load-graph.h \
	proctable.cpp proctable.h \
	prettytable.cpp prettytable.h \
	util.cpp util.h \
	procactions.cpp procactions.h \
	procdialogs.cpp procdialogs.h \
	memmaps.cpp memmaps.h \
	openfiles.cpp openfiles.h \
	smooth_refresh.cpp smooth_refresh.h \
	defaulttable.h \
	disks.cpp disks.h \
	selinux.h selinux.cpp \
	cgroups.h cgroups.cpp \
	procman_matesu.h procman_matesu.cpp \
	procman_gksu.h procman_gksu.cpp \
	sysinfo.cpp sysinfo.h \
	lsof.cpp lsof.h \
	selection.cpp selection.h \
	settings-keys.cpp settings-keys.h \
	iconthemewrapper.cpp iconthemewrapper.h \
	e_date.c e_date.h \
	gsm_color_button.c gsm_color_button.h


mate_system_monitor_LDADD = @PROCMAN_LIBS@ libbacon.la


noinst_LTLIBRARIES = libbacon.la
libbacon_la_SOURCES = \
	bacon-message-connection.c \
	bacon-message-connection.h


specdir = $(datadir)/procman


# GSettings schemas, enum files and conversion file
gsettings_ENUM_NAMESPACE = org.mate.system-monitor
gsettings_ENUM_FILES = $(top_srcdir)/src/*.h
gsettings_SCHEMAS = org.mate.system-monitor.gschema.xml
@GSETTINGS_RULES@
@INTLTOOL_XML_NOMERGE_RULE@

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

convertdir = $(datadir)/MateConf/gsettings
dist_convert_DATA = mate-system-monitor.convert

dist_noinst_DATA = \
	$(gsettings_in_file)

CLEANFILES = \
	$(gsettings_SCHEMAS) \
	$(gsettings_SCHEMAS).in

EXTRA_DIST = \
	$(gsettings_SCHEMAS).in.in

MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)
