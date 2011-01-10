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
 * SECTION:mash-point-light
 * @short_description: An actor for a light with a position that emits
 *   light in all directions.
 *
 * A #MashPointLight models a light that has a position and emits
 * light evenly in all directions. The position of the light is taken
 * from the actor's position so it can be easily modified and even
 * animated using the #ClutterActor properties. The intensity of the
 * light can be attenuated using the attenuation properties to make
 * objects that are further from the light receive less intensity. The
 * intensity of the light is divided by ad² + bd + c, where d is the
 * distance between the light and the vertex and a, b and c are the
 * following properties which can be modified on the light:
 * quadratic-attenuation, linear-attenuation and constant-attenuation.
 *
 * By default the attenuation values are all zero except for the
 * constant attenuation. This causes the light to never be attenuated
 * so that the light intensity is not affected by the distance from
 * the light.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <clutter/clutter.h>

#include "mash-light.h"
#include "mash-point-light.h"

static void mash_point_light_get_property (GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec);
static void mash_point_light_set_property (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);

static void mash_point_light_generate_shader (MashLight *light,
                                              GString *uniform_source,
                                              GString *main_source);
static void mash_point_light_update_uniforms (MashLight *light,
                                              CoglHandle program);

G_DEFINE_TYPE (MashPointLight, mash_point_light, MASH_TYPE_LIGHT);

#define MASH_POINT_LIGHT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_POINT_LIGHT, \
                                MashPointLightPrivate))

#define MASH_POINT_LIGHT_ATTENUATION_CONSTANT  0
#define MASH_POINT_LIGHT_ATTENUATION_LINEAR    1
#define MASH_POINT_LIGHT_ATTENUATION_QUADRATIC 2
#define MASH_POINT_LIGHT_ATTENUATION_COUNT     3

struct _MashPointLightPrivate
{
  /* The three attenuation factors. These are stored in an array so we
     can upload them as one vector and use a dot product in the
     shader */
  float attenuation[MASH_POINT_LIGHT_ATTENUATION_COUNT];

  int attenuation_uniform_location;
  int light_eye_coord_uniform_location;

  /* TRUE if the attenuation factors have been modified since
     update_uniforms was last called */
  gboolean attenuation_dirty;

  /* TRUE if the shader has changed since we last called
     cogl_program_get_uniform_location for the uniforms */
  gboolean uniform_locations_dirty;
};

enum
  {
    PROP_0,

    PROP_CONSTANT_ATTENUATION,
    PROP_LINEAR_ATTENUATION,
    PROP_QUADRATIC_ATTENUATION
  };

static const char
mash_point_light_shader[] =
  /* Vector from the vertex to the light */
  "  vec3 light_vec$ = light_eye_coord$ - eye_coord;\n"
  /* Distance from the vertex to the light */
  "  float d$ = length (light_vec$);\n"
  /* Normalize the light vector */
  "  light_vec$ /= d$;\n"
  /* Add the ambient light term */
  "  vec3 lit_color$ = gl_FrontMaterial.ambient.rgb * ambient_light$;\n"
  /* Calculate the diffuse factor based on the angle between the
     vertex normal and the angle between the light and the vertex */
  "  float diffuse_factor$ = max (0.0, dot (light_vec$, normal));\n"
  /* Skip the specular and diffuse terms if the vertex is not facing
     the light */
  "  if (diffuse_factor$ > 0.0)\n"
  "    {\n"
  /* Add the diffuse term */
  "      lit_color$ += (diffuse_factor$ * gl_FrontMaterial.diffuse.rgb\n"
  "                     * diffuse_light$);\n"
  /* Direction for maximum specular highlights is half way between the
     eye vector and the light vector. The eye vector is hard-coded to
     look down the negative z axis */
  "      vec3 half_vector$ = normalize (light_vec$ + vec3 (0.0, 0.0, 1.0));\n"
  "      float spec_factor$ = max (0.0, dot (half_vector$, normal));\n"
  "      float spec_power$ = pow (spec_factor$, gl_FrontMaterial.shininess);\n"
  /* Add the specular term */
  "      lit_color$ += (gl_FrontMaterial.specular.rgb\n"
  "                     * specular_light$ * spec_power$);\n"
  "    }\n"
  /* Attenuate the lit color based on the distance to the light and
     the attenuation formula properties */
  "  lit_color$ /= dot (attenuation$, vec3 (1.0, d$, d$ * d$));\n"
  /* Add it to the total computed color value */
  "  gl_FrontColor.xyz += lit_color$;\n"
  ;

static void
mash_point_light_class_init (MashPointLightClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  MashLightClass *light_class = (MashLightClass *) klass;
  GParamSpec *pspec;

  gobject_class->get_property = mash_point_light_get_property;
  gobject_class->set_property = mash_point_light_set_property;

  light_class->generate_shader = mash_point_light_generate_shader;
  light_class->update_uniforms = mash_point_light_update_uniforms;

  pspec = g_param_spec_float ("constant-attenuation",
                              "Constant Attenuation",
                              "A constant number to add to "
                              "the attenuation value",
                              0.0f, G_MAXFLOAT,
                              1.0f, /* default */
                              G_PARAM_READABLE | G_PARAM_WRITABLE
                              | G_PARAM_STATIC_NAME
                              | G_PARAM_STATIC_NICK
                              | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class,
                                   PROP_CONSTANT_ATTENUATION,
                                   pspec);

  pspec = g_param_spec_float ("linear-attenuation",
                              "Linear Attenuation",
                              "This number is multiplied by the distance "
                              "from the vertex to the light source and "
                              "added to the attenuation factor.",
                              0.0f, G_MAXFLOAT,
                              0.0f, /* default */
                              G_PARAM_READABLE | G_PARAM_WRITABLE
                              | G_PARAM_STATIC_NAME
                              | G_PARAM_STATIC_NICK
                              | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class,
                                   PROP_LINEAR_ATTENUATION,
                                   pspec);

  pspec = g_param_spec_float ("quadratic-attenuation",
                              "Quadratic Attenuation",
                              "This number is multiplied by the square of the "
                              "distance from the vertex to the light source "
                              "and added to the attenuation factor.",
                              0.0f, G_MAXFLOAT,
                              0.0f, /* default */
                              G_PARAM_READABLE | G_PARAM_WRITABLE
                              | G_PARAM_STATIC_NAME
                              | G_PARAM_STATIC_NICK
                              | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class,
                                   PROP_QUADRATIC_ATTENUATION,
                                   pspec);

  g_type_class_add_private (klass, sizeof (MashPointLightPrivate));
}

static void
mash_point_light_init (MashPointLight *self)
{
  MashPointLightPrivate *priv;

  priv = self->priv = MASH_POINT_LIGHT_GET_PRIVATE (self);

  /* These is the default lighting parameters providing by OpenGL. This
     results in no attenuation */
  priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_CONSTANT] = 1.0f;
  priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_LINEAR] = 0.0f;
  priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_QUADRATIC] = 0.0f;

  priv->attenuation_dirty = TRUE;

  priv->uniform_locations_dirty = TRUE;
}

static void
mash_point_light_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
  MashPointLight *light = MASH_POINT_LIGHT (object);

  switch (prop_id)
    {
    case PROP_CONSTANT_ATTENUATION:
      {
        gfloat attenuation = mash_point_light_get_constant_attenuation (light);
        g_value_set_float (value, attenuation);
      }
      break;

    case PROP_LINEAR_ATTENUATION:
      {
        gfloat attenuation = mash_point_light_get_linear_attenuation (light);
        g_value_set_float (value, attenuation);
      }
      break;

    case PROP_QUADRATIC_ATTENUATION:
      {
        gfloat attenuation = mash_point_light_get_quadratic_attenuation (light);
        g_value_set_float (value, attenuation);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mash_point_light_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  MashPointLight *light = MASH_POINT_LIGHT (object);

  switch (prop_id)
    {
    case PROP_CONSTANT_ATTENUATION:
      {
        gfloat attenuation = g_value_get_float (value);
        mash_point_light_set_constant_attenuation (light, attenuation);
      }
      break;

    case PROP_LINEAR_ATTENUATION:
      {
        gfloat attenuation = g_value_get_float (value);
        mash_point_light_set_linear_attenuation (light, attenuation);
      }
      break;

    case PROP_QUADRATIC_ATTENUATION:
      {
        gfloat attenuation = g_value_get_float (value);
        mash_point_light_set_quadratic_attenuation (light, attenuation);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * mash_point_light_new:
 *
 * Constructs a new #MashPointLight.
 *
 * Return value: the new light.
 */
ClutterActor *
mash_point_light_new (void)
{
  ClutterActor *self = g_object_new (MASH_TYPE_POINT_LIGHT, NULL);

  return self;
}

/**
 * mash_point_light_set_constant_attenuation:
 * @light: The light to modify
 * @attenuation: The new value
 *
 * Sets the constant attenuation value on a light. The light intensity
 * is divided by this value. Setting a higher value will cause the
 * light to appear dimmer.
 */
void
mash_point_light_set_constant_attenuation (MashPointLight *light,
                                           gfloat attenuation)
{
  MashPointLightPrivate *priv;

  g_return_if_fail (MASH_IS_POINT_LIGHT (light));

  priv = light->priv;

  if (attenuation != priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_CONSTANT])
    {
      priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_CONSTANT] = attenuation;
      priv->attenuation_dirty = TRUE;
      g_object_notify (G_OBJECT (light), "constant-attenuation");
    }
}

/**
 * mash_point_light_get_constant_attenuation:
 * @light: The light to query
 *
 * Return value: the constant light attenuation value.
 */
gfloat
mash_point_light_get_constant_attenuation (MashPointLight *light)
{
  g_return_val_if_fail (MASH_IS_POINT_LIGHT (light), 0.0f);

  return light->priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_CONSTANT];
}

/**
 * mash_point_light_set_linear_attenuation:
 * @light: The light to modify
 * @attenuation: The new value
 *
 * Sets the linear attenuation value on a light. The light intensity
 * is divided by this value multiplied by the distance to the
 * light. Setting a higher value will cause the intensity to dim faster
 * as the vertex moves away from the light.
 */
void
mash_point_light_set_linear_attenuation (MashPointLight *light,
                                         gfloat attenuation)
{
  MashPointLightPrivate *priv;

  g_return_if_fail (MASH_IS_POINT_LIGHT (light));

  priv = light->priv;

  if (attenuation != priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_LINEAR])
    {
      priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_LINEAR] = attenuation;
      priv->attenuation_dirty = TRUE;
      g_object_notify (G_OBJECT (light), "linear-attenuation");
    }
}

/**
 * mash_point_light_get_linear_attenuation:
 * @light: The light to query
 *
 * Return value: the linear light attenuation value.
 */
gfloat
mash_point_light_get_linear_attenuation (MashPointLight *light)
{
  g_return_val_if_fail (MASH_IS_POINT_LIGHT (light), 0.0f);

  return light->priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_LINEAR];
}

/**
 * mash_point_light_set_quadratic_attenuation:
 * @light: The light to modify
 * @attenuation: The new value
 *
 * Sets the quadratic attenuation value on a light. The light
 * intensity is divided by this value multiplied by the square of the
 * distance to the light. Setting a higher value will cause the
 * intensity to dim sharply as the vertex moves away from the light.
 */
void
mash_point_light_set_quadratic_attenuation (MashPointLight *light,
                                            gfloat attenuation)
{
  MashPointLightPrivate *priv;

  g_return_if_fail (MASH_IS_POINT_LIGHT (light));

  priv = light->priv;

  if (attenuation != priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_QUADRATIC])
    {
      priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_QUADRATIC] = attenuation;
      priv->attenuation_dirty = TRUE;
      g_object_notify (G_OBJECT (light), "quadratic-attenuation");
    }
}

/**
 * mash_point_light_get_quadratic_attenuation:
 * @light: The light to query
 *
 * Return value: the quadratic light attenuation value.
 */
gfloat
mash_point_light_get_quadratic_attenuation (MashPointLight *light)
{
  g_return_val_if_fail (MASH_IS_POINT_LIGHT (light), 0.0f);

  return light->priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_QUADRATIC];
}

static void
mash_point_light_generate_shader (MashLight *light,
                                  GString *uniform_source,
                                  GString *main_source)
{
  MashPointLight *plight = MASH_POINT_LIGHT (light);
  MashPointLightPrivate *priv = plight->priv;

  MASH_LIGHT_CLASS (mash_point_light_parent_class)
    ->generate_shader (light, uniform_source, main_source);

  /* If the shader is being generated then the uniform locations also
     need updating */
  priv->uniform_locations_dirty = TRUE;
  priv->attenuation_dirty = TRUE;

  mash_light_append_shader (light, uniform_source,
                            "uniform vec3 attenuation$;\n"
                            "uniform vec3 light_eye_coord$;\n");

  mash_light_append_shader (light, main_source, mash_point_light_shader);
}

static void
mash_point_light_update_uniforms (MashLight *light,
                                  CoglHandle program)
{
  MashPointLight *plight = MASH_POINT_LIGHT (light);
  MashPointLightPrivate *priv = plight->priv;
  gfloat light_eye_coord[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
  CoglMatrix matrix;

  MASH_LIGHT_CLASS (mash_point_light_parent_class)
    ->update_uniforms (light, program);

  if (priv->uniform_locations_dirty)
    {
      priv->attenuation_uniform_location
        = mash_light_get_uniform_location (light, program, "attenuation");
      priv->light_eye_coord_uniform_location
        = mash_light_get_uniform_location (light, program, "light_eye_coord");
      priv->uniform_locations_dirty = FALSE;
    }

  if (priv->attenuation_dirty)
    {
      cogl_program_set_uniform_float (program,
                                      priv->attenuation_uniform_location,
                                      3, 1,
                                      priv->attenuation);
      priv->attenuation_dirty = FALSE;
    }

  /* I can't think of a good way to recognise when the position of the
     actor may have changed so this just always updates the light eye
     coordinates. Any transformations in the parent hierarchy could
     cause the position to change without affecting the allocation */

  mash_light_get_modelview_matrix (light,
                                   &matrix);

  cogl_matrix_transform_point (&matrix,
                               light_eye_coord + 0,
                               light_eye_coord + 1,
                               light_eye_coord + 2,
                               light_eye_coord + 3);
  light_eye_coord[0] /= light_eye_coord[3];
  light_eye_coord[1] /= light_eye_coord[3];
  light_eye_coord[2] /= light_eye_coord[3];

  cogl_program_set_uniform_float (program,
                                  priv->light_eye_coord_uniform_location,
                                  3, 1,
                                  light_eye_coord);
}
