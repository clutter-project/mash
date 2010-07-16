SUBDIRS = rply

lib_LTLIBRARIES = libmash.la

INCLUDES = \
	-I$(top_srcdir)

AM_CPPFLAGS = \
	-DMASH_COMPILATION=1 \
	@CLUTTER_CFLAGS@

enum_h = $(srcdir)/mash-data.h

source_h = \
	$(enum_h) \
	$(srcdir)/mash.h \
	$(srcdir)/mash-model.h

built_source_h = \
	$(builddir)/mash-enum-types.h

built_source_c = \
	$(builddir)/mash-enum-types.c

libmash_la_SOURCES = \
	$(source_h) \
	$(srcdir)/mash-data.c \
	$(srcdir)/mash-model.c

nodist_libmash_la_SOURCES = \
	$(built_source_h) \
	$(built_source_c)

mashdir = $(includedir)/mash

mash_HEADERS = \
	$(source_h)

nodist_mash_HEADERS = \
	$(built_source_h)

libmash_la_LIBADD = \
	@CLUTTER_LIBS@ \
	rply/librply.la

CLEANFILES =

BUILT_GIRSOURCES =

if HAVE_INTROSPECTION

ClutterPly.gir: $(INTROSPECTION_SCANNER) libmash.la
	$(QUIET_GEN)$(INTROSPECTION_SCANNER) -v \
		--namespace ClutterPly --nsversion=0.1 \
		$(INCLUDES) \
		$(AM_CPPFLAGS) \
		--add-include-path=cogl/cogl \
		$(json_gir_include_path) \
		--c-include='mash/mash.h' \
		--include=Clutter-1.0 \
		--output $@ \
		--library=libmash.la \
		$(libmash_la_SOURCES)

BUILT_GIRSOURCES += ClutterPly.gir

# INTROSPECTION_GIRDIR/INTROSPECTION_TYPELIBDIR aren't the right place to
# install anything - we need to install inside our prefix.
girdir = $(datadir)/gir-1.0
gir_DATA = $(BUILT_GIRSOURCES)

typelibsdir = $(libdir)/girepository-1.0/

typelibs_DATA = $(BUILT_GIRSOURCES:.gir=.typelib)

%.typelib: %.gir $(INTROSPECTION_COMPILER)
	$(QUIET_GEN) \
	LD_LIBRARY_PATH=.libs$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} \
	$(INTROSPECTION_COMPILER) \
		--includedir=$(srcdir) \
		--includedir=. \
		$(INTROSPECTION_COMPILER_OPTS) $< -o $(@F)

CLEANFILES += $(BUILT_GIRSOURCES) $(typelibs_DATA)

endif # HAVE_INTROSPECTION

# These need to be defined before we can include the glib Makefiles
# below
DISTCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST =

# glib-mkenums rules
glib_enum_h = mash-enum-types.h
glib_enum_c = mash-enum-types.c
glib_enum_headers = $(enum_h)

# This used to use ebassi's magic Makefile.am.enums but I can't really
# use that anymore because I need to do a sed hack to post-process the
# output to cope with the fact that CLUTTER_PLY is a two-word
# namespace. glib-mkenums doesn't seem to deal with this as far as I
# can tell.

enum_tmpl_h=$(glib_enum_h:.h=.h.in)
enum_tmpl_c=$(glib_enum_c:.c=.c.in)

CLEANFILES += stamp-enum-types
DISTCLEANFILES += $(glib_enum_h) $(glib_enum_c)
BUILT_SOURCES += $(glib_enum_h) $(glib_enum_c)
EXTRA_DIST += $(srcdir)/$(enum_tmpl_h) $(srcdir)/$(enum_tmpl_c)

stamp-enum-types: $(glib_enum_headers) $(srcdir)/$(enum_tmpl_h)
	$(QUIET_GEN)$(GLIB_MKENUMS) \
		--template $(srcdir)/$(enum_tmpl_h) \
	$(glib_enum_headers) \
	| sed s/CLUTTER_TYPE_PLY_/CLUTTER_PLY_TYPE_/g > xgen-eh \
	&& (cmp -s xgen-eh $(glib_enum_h) || cp -f xgen-eh $(glib_enum_h)) \
	&& rm -f xgen-eh \
	&& echo timestamp > $(@F)

$(glib_enum_h): stamp-enum-types
	@true

$(glib_enum_c): $(glib_enum_h) $(srcdir)/$(enum_tmpl_c)
	$(QUIET_GEN)$(GLIB_MKENUMS) \
		--template $(srcdir)/$(enum_tmpl_c) \
	$(glib_enum_headers) > xgen-ec \
	&& cp -f xgen-ec $(glib_enum_c) \
	&& rm -f xgen-ec

