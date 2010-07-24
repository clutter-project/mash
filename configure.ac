dnl The version system for Mash has a major, minor and micro version
dnl number. The ABI and API is stable only between micro version
dnl numbers (although this will likely change if Mash ever makes a
dnl 1.0.0 release). The micro is odd numbered between releases. Only
dnl on the commit that contains a release should it be an even number.

m4_define([mash_major_version], [0])
m4_define([mash_minor_version], [1])
m4_define([mash_micro_version], [1])
m4_define([mash_version],
          [mash_major_version.mash_minor_version.mash_micro_version])

dnl The major and minor currently number define the API version so
dnl these become part of the library name.

m4_define([mash_api_version], [mash_major_version.mash_minor_version])

dnl The interface version is managed manually, separately from the
dnl public package version number. The numbers here are modified
dnl according to the guidelines set out in the libtool manual. This is
dnl different from the semi-automatic usage implemented in Gtk and
dnl Clutter. I did this because I was convinced by the reasoning in
dnl the UProf configure script. The 'current' age here will
dnl effectively always be zero because whenever Mash breaks API it
dnl will change the library name so it's effectively a completely
dnl different library.

m4_define([mash_lt_current], 0)
m4_define([mash_lt_revision], 0)
m4_define([mash_lt_age], 0)

AC_INIT([mash], [mash_version])

AC_CONFIG_SRCDIR([mash/mash.h])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_LIBTOOL

dnl Version number variables for the Makefiles
MASH_MAJOR_VERSION=mash_major_version
MASH_MINOR_VERSION=mash_minor_version
MASH_MICRO_VERSION=mash_micro_version
MASH_VERSION=mash_version
MASH_API_VERSION=mash_api_version
AC_SUBST(MASH_MAJOR_VERSION)
AC_SUBST(MASH_MINOR_VERSION)
AC_SUBST(MASH_MICRO_VERSION)
AC_SUBST(MASH_VERSION)
AC_SUBST(MASH_API_VERSION)

MASH_LT_CURRENT=mash_lt_current
MASH_LT_REVISION=mash_lt_revision
MASH_LT_AGE=mash_lt_age
AC_SUBST(MASH_LT_CURRENT)
AC_SUBST(MASH_LT_REVISION)
AC_SUBST(MASH_LT_AGE)

dnl Check if we need to link to -lm
AC_SEARCH_LIBS([atan2f], [m], [have_atan2f=yes], [have_atan2f=no])
AS_IF([test "x$have_atan2f" = "xno"],
      AC_MSG_ERROR([Could not find math library]))

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.16 gobject-2.0 >= 2.16])
PKG_CHECK_MODULES(CLUTTER, [clutter-1.0 >= 1.2])

dnl Optionally depend on Mx just for the test-lights example
PKG_CHECK_MODULES(MX, [mx-1.0 >= 1.1.0], [have_mx=yes], [have_mx=no])

AS_IF([test "x$have_mx" = "xno"],
      AC_MSG_WARN([Mx was not found. It is not required but the lighting example will not be built without it]))

AM_CONDITIONAL([HAVE_MX], [test "x$have_mx" = "xyes"])

# prefixes for fixing gtk-doc references
CLUTTER_PREFIX="`$PKG_CONFIG --variable=prefix clutter-1.0`"
AC_SUBST(CLUTTER_PREFIX)

GTK_DOC_CHECK([1.14],[--flavour no-tmpl])

GOBJECT_INTROSPECTION_CHECK([0.6.1])

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])

AC_CONFIG_FILES([
        Makefile
        mash/Makefile
        mash/rply/Makefile
        example/Makefile
        docs/Makefile
        docs/reference/Makefile
        docs/reference/mash-docs.sgml
        mash-$MASH_API_VERSION.pc:mash.pc.in
])

AC_OUTPUT
