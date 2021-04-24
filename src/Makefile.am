## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPROCMAN_DATADIR=\""$(datadir)/procman/"\" \
	-DMATELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DPKGLIBEXECDIR=\""$(pkglibexecdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	@PROCMAN_CFLAGS@ \
	@SYSTEMD_CFLAGS@ \
	@WNCK_CFLAGS@

bin_PROGRAMS = mate-system-monitor

BUILT_SOURCES = msm-resources.c

mate_system_monitor_cpp_files = \
	argv.cpp \
	procman.cpp \
	interface.cpp \
	callbacks.cpp \
	load-graph.cpp \
	proctable.cpp \
	prettytable.cpp \
	util.cpp \
	procactions.cpp \
	procdialogs.cpp \
	memmaps.cpp \
	openfiles.cpp \
	procproperties.cpp \
	smooth_refresh.cpp \
	disks.cpp \
	selinux.cpp \
	cgroups.cpp \
	procman_gksu.cpp \
	procman_pkexec.cpp \
	sysinfo.cpp \
	lsof.cpp \
	selection.cpp \
	settings-keys.cpp \
	iconthemewrapper.cpp \
	procman-app.cpp

mate_system_monitor_c_files = \
	gsm_color_button.c

mate_system_monitor_h_files = \
	$(mate_system_monitor_cpp_files:.cpp=.h) \
	$(mate_system_monitor_c_files:.c=.h) \
	defaulttable.h

mate_system_monitor_SOURCES = \
	$(BUILT_SOURCES) \
	$(mate_system_monitor_h_files) \
	$(mate_system_monitor_cpp_files) \
	$(mate_system_monitor_c_files)

mate_system_monitor_CXXFLAGS = \
	$(WARN_CXXFLAGS)

mate_system_monitor_CFLAGS = \
	$(WARN_CFLAGS)

mate_system_monitor_LDADD = @PROCMAN_LIBS@ @SYSTEMD_LIBS@ @WNCK_LIBS@

specdir = $(datadir)/procman

# Generate resources
msm-resources.c: msm.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/msm.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name msm $<

gsettings_ENUM_NAMESPACE = org.mate.system-monitor
gsettings_ENUM_FILES = $(top_srcdir)/src/*.h

gsettingsschema_in_files = org.mate.system-monitor.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)

@GSETTINGS_RULES@

dist_noinst_DATA = \
	$(gsettings_in_file)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(gsettings_SCHEMAS)

EXTRA_DIST = \
	msm.gresource.xml \
	preferences.ui \
	$(gsettingsschema_in_files)

MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)

-include $(top_srcdir)/git.mk
