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

#ifndef __MASH_LIGHT_H__
#define __MASH_LIGHT_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define MASH_TYPE_LIGHT                                                 \
  (mash_light_get_type())
#define MASH_LIGHT(obj)                                                 \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               MASH_TYPE_LIGHT,                         \
                               MashLight))
#define MASH_LIGHT_CLASS(klass)                                         \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            MASH_TYPE_LIGHT,                            \
                            MashLightClass))
#define MASH_IS_LIGHT(obj)                                              \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               MASH_TYPE_LIGHT))
#define MASH_IS_LIGHT_CLASS(klass)                                      \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            MASH_TYPE_LIGHT))
#define MASH_LIGHT_GET_CLASS(obj)                                       \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              MASH_LIGHT,                               \
                              MashLightClass))

typedef struct _MashLight        MashLight;
typedef struct _MashLightClass   MashLightClass;
typedef struct _MashLightPrivate MashLightPrivate;

/**
 * MashLightClass:
 * @generate_shader: Virtual used for creating custom light types
 * @update_uniforms: Virtual used for creating custom light types
 */
struct _MashLightClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /*< public >*/
  void (* generate_shader) (MashLight *light,
                            GString *uniform_source,
                            GString *main_source);
  void (* update_uniforms) (MashLight *light,
                            CoglHandle program);
};

/**
 * MashLight:
 *
 * The #MashLight structure contains only private data.
 */
struct _MashLight
{
  /*< private >*/
  ClutterActor parent;

  MashLightPrivate *priv;
};

GType mash_light_get_type (void) G_GNUC_CONST;

void mash_light_set_ambient (MashLight *light, const ClutterColor *ambient);
void mash_light_get_ambient (MashLight *light, ClutterColor *ambient);

void mash_light_set_diffuse (MashLight *light, const ClutterColor *diffuse);
void mash_light_get_diffuse (MashLight *light, ClutterColor *diffuse);

void mash_light_set_specular (MashLight *light, const ClutterColor *specular);
void mash_light_get_specular (MashLight *light, ClutterColor *specular);

void mash_light_generate_shader (MashLight *light,
                                 GString *uniform_source,
                                 GString *main_source);
void mash_light_update_uniforms (MashLight *light,
                                 CoglHandle program);

int mash_light_get_uniform_location (MashLight *light,
                                     CoglHandle program,
                                     const char *uniform_name);

void mash_light_append_shader (MashLight *light,
                               GString *shader_source,
                               const char *snippet);

void mash_light_get_modelview_matrix (MashLight *light,
                                      CoglMatrix *matrix);

void mash_light_set_direction_uniform (MashLight *light,
                                       CoglHandle program,
                                       int uniform_location,
                                       const float *direction_in);

G_END_DECLS

#endif /* __MASH_LIGHT_H__ */
