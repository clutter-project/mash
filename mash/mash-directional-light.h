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

#ifndef __MASH_DIRECTIONAL_LIGHT_H__
#define __MASH_DIRECTIONAL_LIGHT_H__

#include <mash/mash-light.h>

G_BEGIN_DECLS

#define MASH_TYPE_DIRECTIONAL_LIGHT                                     \
  (mash_directional_light_get_type())
#define MASH_DIRECTIONAL_LIGHT(obj)                                     \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               MASH_TYPE_DIRECTIONAL_LIGHT,             \
                               MashDirectionalLight))
#define MASH_DIRECTIONAL_LIGHT_CLASS(klass)                             \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            MASH_TYPE_DIRECTIONAL_LIGHT,                \
                            MashDirectionalLightClass))
#define MASH_IS_DIRECTIONAL_LIGHT(obj)                                  \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               MASH_TYPE_DIRECTIONAL_LIGHT))
#define MASH_IS_DIRECTIONAL_LIGHT_CLASS(klass)                          \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            MASH_TYPE_DIRECTIONAL_LIGHT))
#define MASH_DIRECTIONAL_LIGHT_GET_CLASS(obj)                           \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              MASH_DIRECTIONAL_LIGHT,                   \
                              MashDirectionalLightClass))

typedef struct _MashDirectionalLight        MashDirectionalLight;
typedef struct _MashDirectionalLightClass   MashDirectionalLightClass;
typedef struct _MashDirectionalLightPrivate MashDirectionalLightPrivate;

/**
 * MashDirectionalLight:
 *
 * The #MashDirectionalLight structure contains only private data.
 */
struct _MashDirectionalLightClass
{
  /*< private >*/
  MashLightClass parent_class;
};

/**
 * MashDirectionalLight:
 *
 * The #MashDirectionalLight structure contains only private data.
 */
struct _MashDirectionalLight
{
  /*< private >*/
  MashLight parent;

  MashDirectionalLightPrivate *priv;
};

GType mash_directional_light_get_type (void) G_GNUC_CONST;

ClutterActor *mash_directional_light_new (void);

G_END_DECLS

#endif /* __MASH_DIRECTIONAL_LIGHT_H__ */
