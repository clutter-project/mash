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

#ifndef __MASH_POINT_LIGHT_H__
#define __MASH_POINT_LIGHT_H__

#include <mash/mash-light.h>

G_BEGIN_DECLS

#define MASH_TYPE_POINT_LIGHT                                           \
  (mash_point_light_get_type())
#define MASH_POINT_LIGHT(obj)                                           \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               MASH_TYPE_POINT_LIGHT,                   \
                               MashPointLight))
#define MASH_POINT_LIGHT_CLASS(klass)                                   \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            MASH_TYPE_POINT_LIGHT,                      \
                            MashPointLightClass))
#define MASH_IS_POINT_LIGHT(obj)                                        \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               MASH_TYPE_POINT_LIGHT))
#define MASH_IS_POINT_LIGHT_CLASS(klass)                                \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            MASH_TYPE_POINT_LIGHT))
#define MASH_POINT_LIGHT_GET_CLASS(obj)                                 \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              MASH_POINT_LIGHT,                         \
                              MashPointLightClass))

typedef struct _MashPointLight        MashPointLight;
typedef struct _MashPointLightClass   MashPointLightClass;
typedef struct _MashPointLightPrivate MashPointLightPrivate;

struct _MashPointLightClass
{
  MashLightClass parent_class;
};

struct _MashPointLight
{
  MashLight parent;

  MashPointLightPrivate *priv;
};

GType mash_point_light_get_type (void) G_GNUC_CONST;

ClutterActor *mash_point_light_new (void);

void mash_point_light_set_constant_attenuation (MashPointLight *light,
                                                gfloat value);
gfloat mash_point_light_get_constant_attenuation (MashPointLight *light);

void mash_point_light_set_linear_attenuation (MashPointLight *light,
                                              gfloat value);
gfloat mash_point_light_get_linear_attenuation (MashPointLight *light);

void mash_point_light_set_quadratic_attenuation (MashPointLight *light,
                                                 gfloat value);
gfloat mash_point_light_get_quadratic_attenuation (MashPointLight *light);

G_END_DECLS

#endif /* __MASH_POINT_LIGHT_H__ */
