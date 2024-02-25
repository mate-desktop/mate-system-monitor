dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])

AC_INIT([mate-system-monitor],
	[1.28.1],
	[https://github.com/mate-desktop/mate-system-monitor/issues],
	[mate-system-monitor],
	[https://mate-desktop.org])
AC_CONFIG_SRCDIR(configure.ac)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz foreign check-news])
AM_SILENT_RULES([yes])

AM_MAINTAINER_MODE([enable])
MATE_MAINTAINER_MODE_DEFINES
MATE_DEBUG_CHECK([no])

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
GLIB_REQUIRED=2.56.0
LIBGTOP_REQUIRED=2.37.2
GIOMM_REQUIRED=2.26.0
GLIBMM_REQUIRED=2.22
GTK_REQUIRED=3.22.0
GTKMM_REQUIRED=3.8.1
LIBWNCK_REQUIRED=3.0.0
LIBXML_REQUIRED=2.0
RSVG_REQUIRED=2.35
SYSTEMD_REQUIRED=44

PKG_CHECK_MODULES(PROCMAN, [
    glib-2.0 >= $GLIB_REQUIRED
    libgtop-2.0 >= $LIBGTOP_REQUIRED
    gtk+-3.0 >= $GTK_REQUIRED
    gtkmm-3.0 >= $GTKMM_REQUIRED
    libxml-2.0 >= $LIBXML_REQUIRED
    librsvg-2.0 >= $RSVG_REQUIRED
    glibmm-2.4 >= $GLIBMM_REQUIRED
    giomm-2.4 >= $GIOMM_REQUIRED
    gmodule-2.0
])

AC_ARG_ENABLE(wnck, AS_HELP_STRING([--enable-wnck], [enable wnck support]), enable_wnck="$enableval", enable_wnck=no)
if test "x$enable_wnck" != "xno"; then
        PKG_CHECK_MODULES(WNCK, libwnck-3.0 >= $LIBWNCK_REQUIRED gdk-x11-3.0)
        AC_DEFINE(HAVE_WNCK, 1, [Define if libwnck is available])
fi
AM_CONDITIONAL(HAVE_WNCK, [test "enable_wnck" = "yes"])

PKG_CHECK_MODULES(TOOLS, glib-2.0 >= $GLIB_REQUIRED)

have_systemd=no
AC_ARG_ENABLE(systemd, AS_HELP_STRING([--disable-systemd], [disable systemd support]),enable_systemd="$enableval",enable_systemd=auto)
if test "x$enable_systemd" != "xno"; then
	PKG_CHECK_MODULES(SYSTEMD, [libsystemd], [have_systemd=yes],
	                  [PKG_CHECK_MODULES(SYSTEMD, [libsystemd-login >= $SYSTEMD_REQUIRED],
	                  [have_systemd=yes])])
	if test "x$have_systemd" = xno && test "x$enable_systemd" = xyes; then
		AC_MSG_ERROR([*** systemd support requested but libraries not found])
	elif test "x$have_systemd" = xyes; then
		AC_DEFINE(HAVE_SYSTEMD, 1, [Define if systemd is available])
	fi
fi

AM_CONDITIONAL(HAVE_SYSTEMD, [test "$have_systemd" = "yes"])

# Compiler warnings
MATE_COMPILE_WARNINGS
MATE_CXX_WARNINGS

dnl CXXFLAGS="-fvisibility=hidden -fvisibility-inlines-hidden $CXXFLAGS"
dnl CXXFLAGS="-fvisibility-inlines-hidden $CXXFLAGS"

GLIB_GSETTINGS

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=mate-system-monitor
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [The gettext catalog name])

# **********************
# GLIB_COMPILE_RESOURCES
# **********************
AC_ARG_VAR([GLIB_COMPILE_RESOURCES],[glib-compile-resources bin])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[])
if test -z "$GLIB_COMPILE_RESOURCES"; then
  AC_MSG_ERROR([glib-compile-resources not found])
fi

# *******
# XMLLINT
# *******
AC_ARG_VAR([XMLLINT],[xmllint bin])
AC_PATH_PROG([XMLLINT],[xmllint],[])
if test -z "$XMLLINT"; then
  AC_MSG_ERROR([xmllint not found])
fi

# Documentation
YELP_HELP_INIT

AC_CONFIG_FILES([
Makefile
src/Makefile
src/org.mate.system-monitor.gschema.xml
pixmaps/Makefile
po/Makefile.in
help/Makefile
tools/Makefile
mate-system-monitor.desktop.in
])

AC_OUTPUT

echo "
Configure summary:

        ${PACKAGE_STRING}
        `echo $PACKAGE_STRING | sed "s/./=/g"`

        Source code location:   ${srcdir}
        C Compiler:             ${CC}
        CFLAGS:                 ${CFLAGS}
        WARN_CFLAGS:            ${WARN_CFLAGS}
        C++ Compiler:           ${CXX}
        CXXFLAGS:               ${CXXFLAGS}
        WARN_CXXFLAGS:          ${WARN_CXXFLAGS}
        Maintainer mode:        ${USE_MAINTAINER_MODE}
        Systemd support:        ${have_systemd}
        wnck support:           ${enable_wnck}
"
