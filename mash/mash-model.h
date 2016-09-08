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

#ifndef __MASH_MODEL_H__
#define __MASH_MODEL_H__

#include <glib-object.h>
#include <clutter/clutter.h>
#include <mash/mash-data.h>
#include <mash/mash-light-set.h>

G_BEGIN_DECLS

#define MASH_TYPE_MODEL                         \
  (mash_model_get_type())
#define MASH_MODEL(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
                               MASH_TYPE_MODEL, \
                               MashModel))
#define MASH_MODEL_CLASS(klass)                 \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
                            MASH_TYPE_MODEL,    \
                            MashModelClass))
#define MASH_IS_MODEL(obj)                              \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                   \
                               MASH_TYPE_MODEL))
#define MASH_IS_MODEL_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
                            MASH_TYPE_MODEL))
#define MASH_MODEL_GET_CLASS(obj)               \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
                              MASH_TYPE_MODEL,  \
                              MashModelClass))

typedef struct _MashModel        MashModel;
typedef struct _MashModelClass   MashModelClass;
typedef struct _MashModelPrivate MashModelPrivate;

/**
 * MashModelClass:
 *
 * The #MashModelClass structure contains only private data.
 */
struct _MashModelClass
{
  /*< private >*/
  ClutterActorClass parent_class;
};

/**
 * MashModel:
 *
 * The #MashModel structure contains only private data.
 */
struct _MashModel
{
  /*< private >*/
  ClutterActor parent;

  MashModelPrivate *priv;
};

GType mash_model_get_type (void) G_GNUC_CONST;

ClutterActor *mash_model_new (void);

ClutterActor *mash_model_new_from_file (MashDataFlags flags,
                                        const gchar *filename,
                                        GError **error);

CoglPipeline* mash_model_get_pipeline (MashModel *self);
void mash_model_set_pipeline (MashModel *self,
                              CoglPipeline *pipeline);

MashData *mash_model_get_data (MashModel *self);
void mash_model_set_data (MashModel *self,
                          MashData *data);

MashLightSet *mash_model_get_light_set (MashModel *self);
void mash_model_set_light_set (MashModel *self,
                               MashLightSet *light_set);

gboolean mash_model_get_fit_to_allocation (MashModel *self);
void mash_model_set_fit_to_allocation (MashModel *self,
                                       gboolean fit_to_allocation);

gfloat mash_model_get_model_depth (ClutterActor *actor);
gfloat mash_model_get_model_z_min (ClutterActor *actor);
gfloat mash_model_get_model_z_max (ClutterActor *actor);

G_END_DECLS

#endif /* __MASH_MODEL_H__ */
