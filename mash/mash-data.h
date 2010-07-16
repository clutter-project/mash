/*
 * Mash - A library for displaying PLY models in a Clutter scene
 * Copyright (C) 2010  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__MASH_H_INSIDE__) && !defined(MASH_COMPILATION)
#error "Only <mash/mash.h> can be included directly."
#endif

#ifndef __MASH_DATA_H__
#define __MASH_DATA_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define MASH_TYPE_DATA                          \
  (mash_data_get_type())
#define MASH_DATA(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
                               MASH_TYPE_DATA,  \
                               MashData))
#define MASH_DATA_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
                            MASH_TYPE_DATA,     \
                            MashDataClass))
#define MASH_IS_DATA(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
                               MASH_TYPE_DATA))
#define MASH_IS_DATA_CLASS(klass)               \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
                            MASH_TYPE_DATA))
#define MASH_DATA_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
                              MASH_TYPE_DATA,   \
                              MashDataClass))

/**
 * MASH_DATA_ERROR:
 *
 * Error domain for #MashData errors
 */
#define MASH_DATA_ERROR mash_data_error_quark ()

typedef struct _MashData        MashData;
typedef struct _MashDataClass   MashDataClass;
typedef struct _MashDataPrivate MashDataPrivate;

/**
 * MashDataClass:
 *
 * The #MashDataClass structure contains only private data.
 */
struct _MashDataClass
{
  /*< private >*/
  GObjectClass parent_class;
};

/**
 * MashData:
 *
 * The #MashData structure contains only private data.
 */
struct _MashData
{
  /*< private >*/
  GObject parent;

  MashDataPrivate *priv;
};

/**
 * MashDataError:
 * @MASH_DATA_ERROR_PLY: The underlying PLY library reported an
 *  error.
 * @MASH_DATA_ERROR_MISSING_PROPERTY: A property that is needed
 *  by #MashData is not present in the file. For example, this
 *  will happen if the file does not contain the x, y and z properties.
 * @MASH_DATA_ERROR_INVALID: The PLY file is not valid.
 * @MASH_DATA_ERROR_UNSUPPORTED: The PLY file is not supported
 *  by your GL driver. This will happen if your driver can't support
 *  GL_UNSIGNED_INT indices but the model has more than 65,536
 *  vertices.
 *
 * Error enumeration for #MashData
 */
typedef enum
  {
    MASH_DATA_ERROR_PLY,
    MASH_DATA_ERROR_MISSING_PROPERTY,
    MASH_DATA_ERROR_INVALID,
    MASH_DATA_ERROR_UNSUPPORTED
  } MashDataError;

GType mash_data_get_type (void) G_GNUC_CONST;

MashData *mash_data_new (void);

gboolean mash_data_load (MashData *self,
                         const gchar *filename,
                         GError **error);

void mash_data_render (MashData *self);

GQuark mash_data_error_quark (void);

void mash_data_get_extents (MashData *self,
                            ClutterVertex *min_vertex,
                            ClutterVertex *max_vertex);

G_END_DECLS

#endif /* __MASH_DATA_H__ */
