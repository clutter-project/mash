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

if HAVE_MX

AM_CPPFLAGS += \
	@MX_CFLAGS@

noinst_PROGRAMS += lights

lights_SOURCES = \
	lights.c

lights_LDADD = \
	@CLUTTER_LIBS@ \
	@MX_LIBS@ \
	$(top_builddir)/mash/libmash.la

endif # HAVE_MX

EXTRA_DIST = TheMonkey.ply
