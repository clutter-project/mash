noinst_PROGRAMS = viewer

INCLUDES = \
	-I $(top_srcdir) \
	-I $(top_builddir) \
	-I $(top_builddir)/mash

AM_CPPFLAGS = \
	@CLUTTER_CFLAGS@

viewer_SOURCES = \
	viewer.c

viewer_LDADD = \
	@CLUTTER_LIBS@ \
	$(top_builddir)/mash/libmash.la

EXTRA_DIST = TheMonkey.ply
