SUBDIRS = rply

lib_LTLIBRARIES = libmash-@MASH_API_VERSION@.la

INCLUDES = \
	-I$(top_srcdir)

AM_CPPFLAGS = \
	-DMASH_COMPILATION=1 \
	@CLUTTER_CFLAGS@

enum_h = \
	$(srcdir)/mash-data.h

private_h = \
	$(srcdir)/mash-data-loaders.h \
	$(srcdir)/mash-data-loader.h \
	$(srcdir)/mash-ply-loader.h

public_h = \
	$(enum_h) \
	$(srcdir)/mash.h \
	$(srcdir)/mash-model.h \
	$(srcdir)/mash-light-box.h \
	$(srcdir)/mash-light.h \
	$(srcdir)/mash-directional-light.h \
	$(srcdir)/mash-spot-light.h \
	$(srcdir)/mash-point-light.h

source_h = \
	$(public_h) \
	$(private_h)

built_source_h = \
	$(builddir)/mash-enum-types.h

built_source_c = \
	$(builddir)/mash-enum-types.c

loaders_c = \
	$(srcdir)/mash-ply-loader.c

libmash_@MASH_API_VERSION@_la_SOURCES = \
	$(source_h) \
	$(loaders_c) \
	$(srcdir)/mash-data.c \
	$(srcdir)/mash-data-loader.c \
	$(srcdir)/mash-model.c \
	$(srcdir)/mash-light-box.c \
	$(srcdir)/mash-light.c \
	$(srcdir)/mash-directional-light.c \
	$(srcdir)/mash-spot-light.c \
	$(srcdir)/mash-point-light.c

nodist_libmash_@MASH_API_VERSION@_la_SOURCES = \
	$(built_source_h) \
	$(built_source_c)

mashdir = $(includedir)/mash-@MASH_API_VERSION@/mash

mash_HEADERS = \
	$(public_h)

nodist_mash_HEADERS = \
	$(built_source_h)

libmash_@MASH_API_VERSION@_la_LDFLAGS = \
	-version-info "@MASH_LT_CURRENT@:@MASH_LT_REVISION@:@MASH_LT_AGE@"

libmash_@MASH_API_VERSION@_la_LIBADD = \
	@CLUTTER_LIBS@ \
	rply/librply.la

CLEANFILES =

BUILT_GIRSOURCES =

if HAVE_INTROSPECTION

Mash-@MASH_API_VERSION@.gir: $(INTROSPECTION_SCANNER) libmash-@MASH_API_VERSION@.la
	$(QUIET_GEN)$(INTROSPECTION_SCANNER) -v \
		--namespace Mash \
		--nsversion=@MASH_API_VERSION@ \
		$(INCLUDES) \
		$(AM_CPPFLAGS) \
		$(json_gir_include_path) \
		--c-include='mash/mash.h' \
		--include=Clutter-1.0 \
		--output $@ \
		--library=libmash-@MASH_API_VERSION@.la \
		$(libmash_@MASH_API_VERSION@_la_SOURCES)

BUILT_GIRSOURCES += Mash-@MASH_API_VERSION@.gir

# INTROSPECTION_GIRDIR/INTROSPECTION_TYPELIBDIR aren't the right place to
# install anything - we need to install inside our prefix.
girdir = $(datadir)/gir-1.0
gir_DATA = $(BUILT_GIRSOURCES)

typelibsdir = $(libdir)/girepository-1.0/

typelibs_DATA = $(BUILT_GIRSOURCES:.gir=.typelib)

.gir.typelib : %.typelib: $(INTROSPECTION_COMPILER)
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

