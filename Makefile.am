SUBDIRS = mash example docs

ACLOCAL_AMFLAGS = -I m4

pcfile = mash-@MASH_API_VERSION@.pc

pkgconfig_DATA = $(pcfile)
pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST = mash.pc.in

DISTCLEANFILES = $(pcfile)

DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc
