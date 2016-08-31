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

/**
 * SECTION:mash-directional-light
 * @short_description: An light with a direction
 *
 * #MashDirectionalLight is the simplest light type implemented in
 * Mash. It is intended to model a light that has a direction but it
 * is infinitely far away. This means that the light will always reach
 * the model regardless of its position. The light does however have a
 * direction so the light intensity will be altered depending on the
 * orientation of the vertex. Directional lights are useful for
 * example to model the light emitted from the sun in an outdoor
 * scene.
 *
 * The actor position of a #MashDirectionalLight is ignored. The
 * direction of the light is always along the positive y axis (which
 * is towards the bottom of the stage by default in Clutter). However
 * the direction of the light is affected by the actor's
 * transformation so it can be modified using the rotation properties.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <clutter/clutter.h>

#include "mash-light.h"
#include "mash-directional-light.h"

static void mash_directional_light_generate_shader (MashLight *light,
                                                    GString *uniform_source,
                                                    GString *main_source);
static void mash_directional_light_update_uniforms (MashLight *light,
                                                    CoglPipeline *pipeline);

G_DEFINE_TYPE (MashDirectionalLight, mash_directional_light, MASH_TYPE_LIGHT);

#define MASH_DIRECTIONAL_LIGHT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_DIRECTIONAL_LIGHT, \
                                MashDirectionalLightPrivate))

struct _MashDirectionalLightPrivate
{
  int light_direction_uniform_location;

  /* TRUE if the shader has changed since we last called
     cogl_program_get_uniform_location for the uniforms */
  gboolean uniform_locations_dirty;
};

static const char
mash_directional_light_shader[] =
  /* Add the ambient light term */
  "  vec3 lit_color$ = mash_material.ambient.rgb * ambient_light$;\n"
  /* Calculate the diffuse factor based on the angle between the
     vertex normal and light direction */
  "  float diffuse_factor$ = max (0.0, dot (light_direction$, normal));\n"
  /* Skip the specular and diffuse terms if the vertex is not facing
     the light */
  "  if (diffuse_factor$ > 0.0)\n"
  "    {\n"
  /* Add the diffuse term */
  "      lit_color$ += (diffuse_factor$ * mash_material.diffuse.rgb\n"
  "                     * diffuse_light$);\n"
  /* Direction for maximum specular highlights is half way between the
     eye vector and the light vector. The eye vector is hard-coded to
     look down the negative z axis */
  "      vec3 half_vector$ = normalize (light_direction$\n"
  "                                     + vec3 (0.0, 0.0, 1.0));\n"
  "      float spec_factor$ = max (0.0, dot (half_vector$, normal));\n"
  "      float spec_power$ = pow (spec_factor$, mash_material.shininess);\n"
  /* Add the specular term */
  "      lit_color$ += (mash_material.specular.rgb\n"
  "                     * specular_light$ * spec_power$);\n"
  "    }\n"
  /* Add it to the total computed color value */
  "  cogl_color_out.xyz += lit_color$;\n"
  ;

static void
mash_directional_light_class_init (MashDirectionalLightClass *klass)
{
  MashLightClass *light_class = (MashLightClass *) klass;

  light_class->generate_shader = mash_directional_light_generate_shader;
  light_class->update_uniforms = mash_directional_light_update_uniforms;

  g_type_class_add_private (klass, sizeof (MashDirectionalLightPrivate));
}

static void
mash_directional_light_init (MashDirectionalLight *self)
{
  MashDirectionalLightPrivate *priv;

  priv = self->priv = MASH_DIRECTIONAL_LIGHT_GET_PRIVATE (self);

  priv->uniform_locations_dirty = TRUE;
}

/**
 * mash_directional_light_new:
 *
 * Constructs a new directional light actor.
 *
 * Return value: the new light.
 */
ClutterActor *
mash_directional_light_new (void)
{
  ClutterActor *self = g_object_new (MASH_TYPE_DIRECTIONAL_LIGHT, NULL);

  return self;
}

static void
mash_directional_light_generate_shader (MashLight *light,
                                        GString *uniform_source,
                                        GString *main_source)
{
  MashDirectionalLight *plight = MASH_DIRECTIONAL_LIGHT (light);
  MashDirectionalLightPrivate *priv = plight->priv;

  MASH_LIGHT_CLASS (mash_directional_light_parent_class)
    ->generate_shader (light, uniform_source, main_source);

  /* If the shader is being generated then the uniform locations also
     need updating */
  priv->uniform_locations_dirty = TRUE;

  mash_light_append_shader (light, uniform_source,
                            "uniform vec3 light_direction$;\n");

  mash_light_append_shader (light, main_source, mash_directional_light_shader);
}

static void
mash_directional_light_update_uniforms (MashLight *light,
                                        CoglPipeline *pipeline)
{
  MashDirectionalLight *dlight = MASH_DIRECTIONAL_LIGHT (light);
  MashDirectionalLightPrivate *priv = dlight->priv;
  /* The light is assumed to always be pointing directly down. This
     can be modified by rotating the actor */
  static const float light_direction[4] = { 0.0f, -1.0f, 0.0f, 0.0f };

  MASH_LIGHT_CLASS (mash_directional_light_parent_class)
    ->update_uniforms (light, pipeline);

  if (priv->uniform_locations_dirty)
    {
      priv->light_direction_uniform_location
        = mash_light_get_uniform_location (light, pipeline, "light_direction");
      priv->uniform_locations_dirty = FALSE;
    }

  /* I can't think of a good way to recognise when the transformation
     of the actor may have changed so this just always updates the
     light direction. Any transformations in the parent hierarchy
     could cause the transformation to change without affecting the
     allocation */

  mash_light_set_direction_uniform (light,
                                    pipeline,
                                    priv->light_direction_uniform_location,
                                    light_direction);
}
