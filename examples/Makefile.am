noinst_PROGRAMS = viewer

AM_CPPFLAGS = \
        -I $(top_srcdir) \
        -I $(top_builddir) \
        -I $(top_builddir)/mash
	@CLUTTER_CFLAGS@

viewer_SOURCES = \
	viewer.c

viewer_LDADD = \
	@CLUTTER_LIBS@ \
	$(top_builddir)/mash/libmash-@MASH_API_VERSION@.la

if HAVE_MX

AM_CPPFLAGS += \
	@MX_CFLAGS@

noinst_PROGRAMS += lights

lights_SOURCES = \
	lights.c

lights_LDADD = \
	@CLUTTER_LIBS@ \
	@MX_LIBS@ \
	$(top_builddir)/mash/libmash-@MASH_API_VERSION@.la

endif # HAVE_MX

EXTRA_DIST = suzanne.ply
