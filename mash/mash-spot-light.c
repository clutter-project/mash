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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <clutter/clutter.h>
#include <math.h>

#include "mash-light.h"
#include "mash-spot-light.h"

static void mash_spot_light_get_property (GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec);
static void mash_spot_light_set_property (GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec);

static void mash_spot_light_generate_shader (MashLight *light,
                                             GString *uniform_source,
                                             GString *main_source);
static void mash_spot_light_update_uniforms (MashLight *light,
                                             CoglHandle program);

G_DEFINE_TYPE (MashSpotLight, mash_spot_light, MASH_TYPE_POINT_LIGHT);

#define MASH_SPOT_LIGHT_GET_PRIVATE(obj)                        \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_SPOT_LIGHT,    \
                                MashSpotLightPrivate))

struct _MashSpotLightPrivate
{
  int light_direction_uniform_location;
  int spot_cos_cutoff_uniform_location;
  int spot_exponent_uniform_location;

  float spot_cutoff;
  float spot_exponent;

  /* TRUE if the spot parameters have been modified since
     update_uniforms was last called */
  gboolean spot_params_dirty;

  /* TRUE if the shader has changed since we last called
     cogl_program_get_uniform_location for the uniforms */
  gboolean uniform_locations_dirty;
};

enum
  {
    PROP_0,

    PROP_SPOT_CUTOFF,
    PROP_SPOT_EXPONENT
  };

static const char
mash_spot_light_shader[] =
  /* Vector from the vertex to the light */
  "  vec3 light_vec$ = light_eye_coord$ - eye_coord;\n"
  /* Distance from the vertex to the light */
  "  float d$ = length (light_vec$);\n"
  /* Normalize the light vector */
  "  light_vec$ /= d$;\n"
  /* Check if the point on the surface is inside the cone of
     illumination */
  "  float spot_cos$ = dot (-light_vec$, spot_direction$);\n"
  "  if (spot_cos$ > spot_cos_cutoff$)\n"
  "    {\n"
  /* Add the ambient light term */
  "      vec3 lit_color$ = gl_FrontMaterial.ambient.rgb * ambient_light$;\n"
  /* Calculate the diffuse factor based on the angle between the
     vertex normal and the angle between the light and the vertex */
  "      float diffuse_factor$ = max (0.0, dot (light_vec$, normal));\n"
  /* Skip the specular and diffuse terms if the vertex is not facing
     the light */
  "      if (diffuse_factor$ > 0.0)\n"
  "        {\n"
  /* Add the diffuse term */
  "          lit_color$ += (diffuse_factor$ * gl_FrontMaterial.diffuse.rgb\n"
  "                         * diffuse_light$);\n"
  /* Direction for maximum specular highlights is half way between the
     eye vector and the light vector. The eye vector is hard-coded to
     look down the negative z axis */
  "          vec3 half_vector$ = normalize (light_vec$\n"
  "                                         + vec3 (0.0, 0.0, 1.0));\n"
  "          float spec_factor$ = max (0.0, dot (half_vector$, normal));\n"
  "          float spec_power$ = pow (spec_factor$,\n"
  "                                   gl_FrontMaterial.shininess);\n"
  /* Add the specular term */
  "          lit_color$ += (gl_FrontMaterial.specular.rgb\n"
  "                         * specular_light$ * spec_power$);\n"
  "        }\n"
  /* Attenuate the lit color based on the distance to the light and
     the attenuation formula properties */
  "      float att = dot (attenuation$, vec3 (1.0, d$, d$ * d$));\n"
  /* Also attenuate based on the angle to the light and the spot exponent */
  "      att *= pow (spot_cos$, spot_exponent$);\n"
  /* Add it to the total computed color value */
  "      gl_FrontColor.xyz += lit_color$ * att;\n"
  "    }\n"
  ;

static void
mash_spot_light_class_init (MashSpotLightClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  MashLightClass *light_class = (MashLightClass *) klass;
  GParamSpec *pspec;

  gobject_class->get_property = mash_spot_light_get_property;
  gobject_class->set_property = mash_spot_light_set_property;

  light_class->generate_shader = mash_spot_light_generate_shader;
  light_class->update_uniforms = mash_spot_light_update_uniforms;

  pspec = g_param_spec_float ("spot-cutoff",
                              "Spot Cutoff",
                              "The cut off angle where the spot light emits "
                              "no light",
                              0.0f, 90.0f,
                              45.0f, /* default */
                              G_PARAM_READABLE | G_PARAM_WRITABLE
                              | G_PARAM_STATIC_NAME
                              | G_PARAM_STATIC_NICK
                              | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class,
                                   PROP_SPOT_CUTOFF,
                                   pspec);

  pspec = g_param_spec_float ("spot-exponent",
                              "Spot Exponent",
                              "A value used to set the decay of the light "
                              "as the angle increases from the light "
                              "direction.",
                              0.0f, 128.0f,
                              0.0f, /* default */
                              G_PARAM_READABLE | G_PARAM_WRITABLE
                              | G_PARAM_STATIC_NAME
                              | G_PARAM_STATIC_NICK
                              | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class,
                                   PROP_SPOT_EXPONENT,
                                   pspec);

  g_type_class_add_private (klass, sizeof (MashSpotLightPrivate));
}

static void
mash_spot_light_init (MashSpotLight *self)
{
  MashSpotLightPrivate *priv;

  priv = self->priv = MASH_SPOT_LIGHT_GET_PRIVATE (self);

  /* Default to no attenuation based on the angle */
  priv->spot_exponent = 0.0f;
  /* Default to a 45° cone. This isn't the same as the default for
     OpenGL which uses 180°. However 180° results in a point light
     which doesn't make sense here */
  priv->spot_cutoff = 45.0f;

  priv->spot_params_dirty = TRUE;
  priv->uniform_locations_dirty = TRUE;
}

static void
mash_spot_light_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  MashSpotLight *light = MASH_SPOT_LIGHT (object);

  switch (prop_id)
    {
    case PROP_SPOT_CUTOFF:
      {
        gfloat cutoff = mash_spot_light_get_spot_cutoff (light);
        g_value_set_float (value, cutoff);
      }
      break;

    case PROP_SPOT_EXPONENT:
      {
        gfloat exponent = mash_spot_light_get_spot_exponent (light);
        g_value_set_float (value, exponent);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mash_spot_light_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  MashSpotLight *light = MASH_SPOT_LIGHT (object);

  switch (prop_id)
    {
    case PROP_SPOT_CUTOFF:
      {
        gfloat cutoff = g_value_get_float (value);
        mash_spot_light_set_spot_cutoff (light, cutoff);
      }
      break;

    case PROP_SPOT_EXPONENT:
      {
        gfloat exponent = g_value_get_float (value);
        mash_spot_light_set_spot_exponent (light, exponent);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

ClutterActor *
mash_spot_light_new (void)
{
  ClutterActor *self = g_object_new (MASH_TYPE_SPOT_LIGHT, NULL);

  return self;
}

void
mash_spot_light_set_spot_cutoff (MashSpotLight *light,
                                 gfloat cutoff)
{
  MashSpotLightPrivate *priv;

  g_return_if_fail (MASH_IS_SPOT_LIGHT (light));

  priv = light->priv;

  if (cutoff != priv->spot_cutoff)
    {
      priv->spot_cutoff = cutoff;
      priv->spot_params_dirty = TRUE;
      g_object_notify (G_OBJECT (light), "spot-cutoff");
    }
}

gfloat
mash_spot_light_get_spot_cutoff (MashSpotLight *light)
{
  g_return_val_if_fail (MASH_IS_SPOT_LIGHT (light), 0.0f);

  return light->priv->spot_cutoff;
}

void
mash_spot_light_set_spot_exponent (MashSpotLight *light,
                                   gfloat exponent)
{
  MashSpotLightPrivate *priv;

  g_return_if_fail (MASH_IS_SPOT_LIGHT (light));

  priv = light->priv;

  if (exponent != priv->spot_exponent)
    {
      priv->spot_exponent = exponent;
      priv->spot_params_dirty = TRUE;
      g_object_notify (G_OBJECT (light), "spot-exponent");
    }
}

gfloat
mash_spot_light_get_spot_exponent (MashSpotLight *light)
{
  g_return_val_if_fail (MASH_IS_SPOT_LIGHT (light), 0.0f);

  return light->priv->spot_exponent;
}

static void
mash_spot_light_generate_shader (MashLight *light,
                                 GString *uniform_source,
                                 GString *main_source)
{
  MashSpotLight *plight = MASH_SPOT_LIGHT (light);
  MashSpotLightPrivate *priv = plight->priv;
  guint old_len;

  /* We need to ignore the shader generation of MashPointLight so we
     keep track of the size of the main source before it ran and trim
     back to that */
  old_len = main_source->len;

  MASH_LIGHT_CLASS (mash_spot_light_parent_class)
    ->generate_shader (light, uniform_source, main_source);

  g_string_set_size (main_source, old_len);

  /* If the shader is being generated then the uniform locations also
     need updating */
  priv->uniform_locations_dirty = TRUE;
  priv->spot_params_dirty = TRUE;

  mash_light_append_shader (light, uniform_source,
                            "uniform float spot_cos_cutoff$;\n"
                            "uniform float spot_exponent$;\n"
                            "uniform vec3 spot_direction$;\n");

  mash_light_append_shader (light, main_source, mash_spot_light_shader);
}

static void
mash_spot_light_update_uniforms (MashLight *light,
                                 CoglHandle program)
{
  MashSpotLight *slight = MASH_SPOT_LIGHT (light);
  MashSpotLightPrivate *priv = slight->priv;
  /* The light is assumed to always be pointing directly down. This
     can be modified by rotating the actor */
  static const float light_direction[4] = { 0.0f, 1.0f, 0.0f, 0.0f };

  MASH_LIGHT_CLASS (mash_spot_light_parent_class)
    ->update_uniforms (light, program);

  if (priv->uniform_locations_dirty)
    {
      priv->spot_cos_cutoff_uniform_location
        = mash_light_get_uniform_location (light, program, "spot_cos_cutoff");
      priv->spot_exponent_uniform_location
        = mash_light_get_uniform_location (light, program, "spot_exponent");
      priv->light_direction_uniform_location
        = mash_light_get_uniform_location (light, program, "spot_direction");
      priv->uniform_locations_dirty = FALSE;
    }

  if (priv->spot_params_dirty)
    {
      cogl_program_uniform_1f (priv->spot_cos_cutoff_uniform_location,
                               cosf (priv->spot_cutoff * G_PI / 180.0));
      cogl_program_uniform_1f (priv->spot_exponent_uniform_location,
                               priv->spot_exponent);
      priv->spot_params_dirty = FALSE;
    }

  /* I can't think of a good way to recognise when the transformation
     of the actor may have changed so this just always updates the
     light direction. Any transformations in the parent hierarchy
     could cause the transformation to change without affecting the
     allocation */

  mash_light_set_direction_uniform (light,
                                    priv->light_direction_uniform_location,
                                    light_direction);
}
