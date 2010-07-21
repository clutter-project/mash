SUBDIRS = mash example docs

ACLOCAL_AMFLAGS = -I m4

pkgconfig_DATA = mash-@MASH_API_VERSION@.pc
pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST = mash.pc.in

DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc
