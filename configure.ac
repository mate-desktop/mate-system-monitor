dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])

AC_INIT([mate-system-monitor],
	[1.12.1],
        [http://www.mate-desktop.org/])
AC_CONFIG_SRCDIR(configure.ac)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz foreign check-news])
AM_SILENT_RULES([yes])

AM_MAINTAINER_MODE([enable])
MATE_MAINTAINER_MODE_DEFINES

# Check For programs
AC_PROG_CC
AC_PROG_CXX
AC_LANG([C++])
AC_LANG_COMPILER_REQUIRE
AX_CXX_COMPILE_STDCXX_11([],[mandatory])
PKG_PROG_PKG_CONFIG([0.19])

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

# Package dependencies
GLIB_REQUIRED=2.36.0
LIBGTOP_REQUIRED=2.23.1
GIOMM_REQUIRED=2.26.0
GLIBMM_REQUIRED=2.22
LIBXML_REQUIRED=2.0
RSVG_REQUIRED=2.35
SYSTEMD_REQUIRED=44

# GLib min required versions
AC_DEFINE([GLIB_VERSION_MIN_REQUIRED], [GLIB_VERSION_2_36],
	[Warn on use of APIs deprecated before GLib 2.36])

AC_MSG_CHECKING([which gtk+ version to compile against])
AC_ARG_WITH([gtk],
  [AS_HELP_STRING([--with-gtk=2.0|3.0],[which gtk+ version to compile against (default: 2.0)])],
  [case "$with_gtk" in
    2.0|3.0) ;;
    *) AC_MSG_ERROR([invalid gtk version specified]) ;;
    esac],
    [with_gtk=2.0])
  AC_MSG_RESULT([$with_gtk])
  case "$with_gtk" in
    2.0) GTK_API_VERSION=2.0
         GTK_REQUIRED=2.20.0
         GTKMM_API_VERSION=2.4
         GTKMM_REQUIRED=2.22
         LIBWNCK_API_VERSION=1.0
         LIBWNCK_REQUIRED=2.5.0
         ;;
    3.0) GTK_API_VERSION=3.0
         GTK_REQUIRED=3.0.0
         GTKMM_API_VERSION=3.0
         GTKMM_REQUIRED=3.0.0
         LIBWNCK_API_VERSION=3.0
         LIBWNCK_REQUIRED=2.91.0
         ;;
  esac

PKG_CHECK_MODULES(GMODULE,gmodule-2.0,[GMODULE_ADD="gmodule-2.0"],[GMODULE_ADD=""])
PKG_CHECK_MODULES(PROCMAN,$GMODULE_ADD glib-2.0 >= $GLIB_REQUIRED libgtop-2.0 >= $LIBGTOP_REQUIRED libwnck-$LIBWNCK_API_VERSION >= $LIBWNCK_REQUIRED gtk+-$GTK_API_VERSION >= $GTK_REQUIRED gtkmm-$GTKMM_API_VERSION >= $GTKMM_REQUIRED libxml-2.0 >= $LIBXML_REQUIRED librsvg-2.0 >= $RSVG_REQUIRED glibmm-2.4 >= $GLIBMM_REQUIRED giomm-2.4 >= $GIOMM_REQUIRED)

have_systemd=no
AC_ARG_ENABLE(systemd, AS_HELP_STRING([--disable-systemd], [disable systemd support]),,enable_systemd=no)
if test "x$enable_systemd" != "xno"; then
	PKG_CHECK_MODULES(SYSTEMD, [libsystemd], [have_systemd=yes],
	                  [PKG_CHECK_MODULES(SYSTEMD, [libsystemd-login >= $SYSTEMD_REQUIRED],
	                  [have_systemd=yes])])
	if test "x$have_systemd" = xno; then
		AC_MSG_ERROR([*** systemd support requested but libraries not found])
	else
		AC_DEFINE(HAVE_SYSTEMD, 1, [Define if systemd is available])
	fi
fi

AM_CONDITIONAL(HAVE_SYSTEMD, [test "$have_systemd" = "yes"])

# Compiler warnings
MATE_COMPILE_WARNINGS([maximum])

AC_ARG_ENABLE(more-warnings,
[AC_HELP_STRING([--enable-more-warnings],[Maximum compiler warnings])],
set_more_warnings="$enableval",[
    set_more_warnings=yes
])
AC_MSG_CHECKING(for more warnings, including -Werror)
if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
    AC_MSG_RESULT(yes)
    CFLAGS="\
    -Wall \
    -Winline \
    -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
    -Wnested-externs -Wpointer-arith \
    -Wcast-align -Wsign-compare \
    $CFLAGS"
    CXXFLAGS="-Wall $CXXFLAGS"
else
    AC_MSG_RESULT(no)
fi

dnl CXXFLAGS="-fvisibility=hidden -fvisibility-inlines-hidden $CXXFLAGS"
dnl CXXFLAGS="-fvisibility-inlines-hidden $CXXFLAGS"

GLIB_GSETTINGS

# i18n stuff
IT_PROG_INTLTOOL([0.50.1])

GETTEXT_PACKAGE=mate-system-monitor
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [The gettext catalog name])

# Documentation
YELP_HELP_INIT

AC_CONFIG_FILES([
Makefile
src/Makefile
src/org.mate.system-monitor.gschema.xml
pixmaps/Makefile
po/Makefile.in
help/Makefile
mate-system-monitor.desktop.in
])

AC_OUTPUT

echo "

Configuration:

        Source code location:   ${srcdir}
        C Compiler:             ${CC}
        C++ Compiler:           ${CXX}
        CFLAGS:                 ${CFLAGS}
        CXXFLAGS:               ${CXXFLAGS}
        Gtk+ Version:           ${GTK_API_VERSION}
        Maintainer mode:        ${USE_MAINTAINER_MODE}
        Systemd support:        ${have_systemd}
"
