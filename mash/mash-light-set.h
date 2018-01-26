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

#ifndef __MASH_LIGHT_SET_H__
#define __MASH_LIGHT_SET_H__

#include <clutter/clutter.h>
#include <mash/mash-light.h>

G_BEGIN_DECLS

#define MASH_TYPE_LIGHT_SET                                             \
  (mash_light_set_get_type())
#define MASH_LIGHT_SET(obj)                                             \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               MASH_TYPE_LIGHT_SET,                     \
                               MashLightSet))
#define MASH_LIGHT_SET_CLASS(klass)                                     \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            MASH_TYPE_LIGHT_SET,                        \
                            MashLightSetClass))
#define MASH_IS_LIGHT_SET(obj)                                          \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               MASH_TYPE_LIGHT_SET))
#define MASH_IS_LIGHT_SET_CLASS(klass)                                  \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            MASH_TYPE_LIGHT_SET))
#define MASH_LIGHT_SET_GET_CLASS(obj)                                   \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              MASH_LIGHT_SET,                           \
                              MashLightSetClass))

typedef struct _MashLightSet        MashLightSet;
typedef struct _MashLightSetClass   MashLightSetClass;
typedef struct _MashLightSetPrivate MashLightSetPrivate;

/**
 * MashLightSetClass:
 *
 * The #MashLightSetClass structure contains only private data.
 */
struct _MashLightSetClass
{
  /*< private >*/
  GObjectClass parent_class;
};

/**
 * MashLightSet:
 *
 * The #MashLightSet structure contains only private data.
 */
struct _MashLightSet
{
  /*< private >*/
  GObjectClass parent;

  MashLightSetPrivate *priv;
};

GType mash_light_set_get_type (void) G_GNUC_CONST;

MashLightSet *mash_light_set_new (void);

void mash_light_set_add_light (MashLightSet *light_set,
                               MashLight *light);

void mash_light_set_remove_light (MashLightSet *light_set,
                                  MashLight *light);

void mash_light_set_begin_paint (MashLightSet *light_set,
                                       CoglPipeline *pipeline);


gboolean mash_light_set_update_layer_indices (MashLightSet *light_set,
                      CoglPipeline *pipeline);

void mash_light_set_get_pipeline (MashLightSet *light_set, CoglPipeline* pipeline);

G_END_DECLS

#endif /* __MASH_LIGHT_SET_H__ */
