dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.52)

AC_INIT([mate-system-monitor], [1.7.90],
        [http://www.mate-desktop.org/])
AC_CONFIG_SRCDIR(configure.ac)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([no-dist-gzip dist-xz check-news])
AC_SUBST(ACLOCAL_AMFLAGS, [\${ACLOCAL_FLAGS}])

AM_PROG_LIBTOOL

IT_PROG_INTLTOOL([0.35.0])
PKG_PROG_PKG_CONFIG([0.19])

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CXX
AC_LANG_CPLUSPLUS
AC_LANG_COMPILER_REQUIRE
AC_HEADER_STDC
MATE_COMPILE_WARNINGS

GLIB_REQUIRED=2.28.0
LIBGTOP_REQUIRED=2.23.1
MATE_ICON_THEME_REQUIRED=1.1.0
GIOMM_REQUIRED=2.26.0
GLIBMM_REQUIRED=2.22
LIBXML_REQUIRED=2.0
RSVG_REQUIRED=2.12
DBUS_REQUIRED=0.7

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
PKG_CHECK_MODULES(PROCMAN,$GMODULE_ADD glib-2.0 >= $GLIB_REQUIRED libgtop-2.0 >= $LIBGTOP_REQUIRED libwnck-$LIBWNCK_API_VERSION >= $LIBWNCK_REQUIRED gtk+-$GTK_API_VERSION >= $GTK_REQUIRED mate-icon-theme >= $MATE_ICON_THEME_REQUIRED gtkmm-$GTKMM_API_VERSION >= $GTKMM_REQUIRED libxml-2.0 >= $LIBXML_REQUIRED librsvg-2.0 >= $RSVG_REQUIRED glibmm-2.4 >= $GLIBMM_REQUIRED giomm-2.4 >= $GIOMM_REQUIRED dbus-glib-1 >= $DBUS_REQUIRED)


AC_ARG_ENABLE(more-warnings,
[AC_HELP_STRING([--enable-more-warnings], [Maximum compiler warnings])],
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
    -std=gnu89 \
    $CFLAGS"
    CXXFLAGS="-Wall -std=c++98 $CXXFLAGS"
else
    AC_MSG_RESULT(no)
fi

dnl CXXFLAGS="-fvisibility=hidden -fvisibility-inlines-hidden $CXXFLAGS"
dnl CXXFLAGS="-fvisibility-inlines-hidden $CXXFLAGS"

GLIB_GSETTINGS

GETTEXT_PACKAGE=mate-system-monitor
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [The gettext catalog name])
AM_GLIB_GNU_GETTEXT

YELP_HELP_INIT

dnl Set PACKAGE_LOCALE_DIR in config.h.
GLIB_DEFINE_LOCALEDIR(PACKAGE_LOCALE_DIR)


AC_CONFIG_FILES([
Makefile
src/Makefile
pixmaps/Makefile
po/Makefile.in
help/Makefile
mate-system-monitor.desktop.in
])

AC_OUTPUT

echo "

Configuration:

	Source code location:	${srcdir}
	C Compiler:		${CC}
	C++ Compiler:		${CXX}
	CFLAGS:			${CFLAGS}
	CXXFLAGS:		${CXXFLAGS}
	Gtk+ Version:		${GTK_API_VERSION}
	Maintainer mode:	${USER_MAINTAINER_MODE}
	Languages:		${ALL_LINGUAS}
"
