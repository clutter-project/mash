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
 * SECTION:mash-light
 * @short_description: An object for the common state of all light types
 *
 * #MashLight is the abstract base class of all lights in Mash. It can
 * not be instantiated directly. Instead one of its subclasses should
 * be used such as #MashPointLight, #MashSpotLight or
 * #MashDirectionalLight.
 *
 * #MashLight<!-- -->s must be added to a #MashLightSet and a parent
 * container before they will have any effect.
 *
 * #MashLight contains three light colors that are common to all
 * three light types that Mash supports. These are ambient, diffuse
 * and specular. The colors are of the lights are combined with the
 * corresponding colors of the #CoglMaterial to give a final fragment
 * color. The material colors can be changed for a #MashModel by
 * extracting the #CoglMaterial with mash_model_get_material() and
 * then calling functions such as cogl_material_set_diffuse().
 *
 * #MashLight can be subclassed in an application to provide custom
 * lighting algorithms.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <clutter/clutter.h>
#include <string.h>
#include <math.h>

#include "mash-light.h"
#include "mash-light-set.h"

static void mash_light_get_property (GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec);
static void mash_light_set_property (GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec);

static void mash_light_real_generate_shader (MashLight *light,
                                             GString *uniform_source,
                                             GString *main_source);
static void mash_light_real_update_uniforms (MashLight *light,
                                             CoglHandle program);

/* Length in characters not including any terminating NULL of the
   unique string that we append to uniform symbols */
#define MASH_LIGHT_UNIQUE_SYMBOL_SIZE (1 + 8)

static const ClutterColor
mash_light_default_color = { 0xff, 0xff, 0xff, 0xff };

G_DEFINE_ABSTRACT_TYPE (MashLight, mash_light, CLUTTER_TYPE_ACTOR);

#define MASH_LIGHT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_LIGHT, \
                                MashLightPrivate))

#define MASH_LIGHT_COLOR_AMBIENT  0
#define MASH_LIGHT_COLOR_DIFFUSE  1
#define MASH_LIGHT_COLOR_SPECULAR 2
#define MASH_LIGHT_COLOR_COUNT    3

static const char *
mash_light_color_names[MASH_LIGHT_COLOR_COUNT] =
  {
    "ambient_light",
    "diffuse_light",
    "specular_light"
  };

struct _MashLightPrivate
{
  /* This string gets appended to symbols that we want to be unique to
     this light in the shader snippets */
  char unique_str[MASH_LIGHT_UNIQUE_SYMBOL_SIZE + 1];

  /* Light colors for the different lighting effects that are shared
     by all light types */
  ClutterColor light_colors[MASH_LIGHT_COLOR_COUNT];

  int uniform_locations[MASH_LIGHT_COLOR_COUNT];

  /* TRUE if the shader has changed since we last called
     cogl_program_get_uniform_location for the color uniforms */
  gboolean uniform_locations_dirty;

  /* Contains a bit for each light color. The bit is set if the color
     value has changed since we last copied the values to the
     uniforms */
  guint dirty_uniforms;

  /* This contains the modelview matrix for the light including all of
     its parent's transformations. It is probably expensive to
     calculate and the matrix is used to update the uniforms for both
     spot lights and point lights so it is cached to avoid calculating
     it twice */
  gboolean modelview_matrix_dirty;
  CoglMatrix modelview_matrix;
};

enum
  {
    PROP_0,

    PROP_AMBIENT,
    PROP_DIFFUSE,
    PROP_SPECULAR
  };

static void
mash_light_class_init (MashLightClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GParamSpec *pspec;

  gobject_class->get_property = mash_light_get_property;
  gobject_class->set_property = mash_light_set_property;

  klass->generate_shader = mash_light_real_generate_shader;
  klass->update_uniforms = mash_light_real_update_uniforms;

  pspec = clutter_param_spec_color ("ambient",
                                    "Ambient",
                                    "The ambient color emitted by the light",
                                    &mash_light_default_color,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE
                                    | G_PARAM_STATIC_NAME
                                    | G_PARAM_STATIC_NICK
                                    | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class, PROP_AMBIENT, pspec);

  pspec = clutter_param_spec_color ("diffuse",
                                    "Diffuse",
                                    "The diffuse color emitted by the light",
                                    &mash_light_default_color,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE
                                    | G_PARAM_STATIC_NAME
                                    | G_PARAM_STATIC_NICK
                                    | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class, PROP_DIFFUSE, pspec);

  pspec = clutter_param_spec_color ("specular",
                                    "Specular",
                                    "The specular color emitted by the light",
                                    &mash_light_default_color,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE
                                    | G_PARAM_STATIC_NAME
                                    | G_PARAM_STATIC_NICK
                                    | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class, PROP_SPECULAR, pspec);

  g_type_class_add_private (klass, sizeof (MashLightPrivate));
}

static void
mash_light_init (MashLight *self)
{
  MashLightPrivate *priv;
  guint32 gid;
  int i;

  priv = self->priv = MASH_LIGHT_GET_PRIVATE (self);

  /* We append the gid of the actor to any symbols that need to be
     unique to this */
  gid = clutter_actor_get_gid (CLUTTER_ACTOR (self));
  g_snprintf (priv->unique_str, MASH_LIGHT_UNIQUE_SYMBOL_SIZE + 1,
              "g%08" G_GUINT32_FORMAT, gid);

  for (i = 0; i < MASH_LIGHT_COLOR_COUNT; i++)
    priv->light_colors[i] = mash_light_default_color;

  priv->uniform_locations_dirty = TRUE;
  priv->dirty_uniforms = (1 << MASH_LIGHT_COLOR_COUNT) - 1;

  priv->modelview_matrix_dirty = TRUE;
}

static void
mash_light_get_property (GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  MashLight *light = MASH_LIGHT (object);

  switch (prop_id)
    {
    case PROP_AMBIENT:
      {
        ClutterColor ambient;
        mash_light_get_ambient (light, &ambient);
        clutter_value_set_color (value, &ambient);
      }
      break;

    case PROP_DIFFUSE:
      {
        ClutterColor diffuse;
        mash_light_get_diffuse (light, &diffuse);
        clutter_value_set_color (value, &diffuse);
      }
      break;

    case PROP_SPECULAR:
      {
        ClutterColor specular;
        mash_light_get_specular (light, &specular);
        clutter_value_set_color (value, &specular);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mash_light_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  MashLight *light = MASH_LIGHT (object);

  switch (prop_id)
    {
    case PROP_AMBIENT:
      mash_light_set_ambient (light, clutter_value_get_color (value));
      break;

    case PROP_DIFFUSE:
      mash_light_set_diffuse (light, clutter_value_get_color (value));
      break;

    case PROP_SPECULAR:
      mash_light_set_specular (light, clutter_value_get_color (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * mash_light_set_ambient:
 * @light: The #MashLight to modify
 * @ambient: The new color value
 *
 * Sets the ‘ambient’ color emitted by the light. If the light reaches
 * a vertex at all then the ambient color affects the vertex
 * regardless of its orientation or distance from the light. In
 * real-world lighting, even if an object isn't in a direct line of
 * sight to a light it can still be partially lit due to the fact that
 * light can bounce off other objects to reach it. The Mash lighting
 * model doesn't simulate this bouncing so the ambient color is often
 * used to give an approximation of the effect.
 */
void
mash_light_set_ambient (MashLight *light, const ClutterColor *ambient)
{
  MashLightPrivate *priv;

  g_return_if_fail (MASH_IS_LIGHT (light));

  priv = light->priv;

  if (!clutter_color_equal (ambient,
                            &priv->light_colors + MASH_LIGHT_COLOR_AMBIENT))
    {
      priv->light_colors[MASH_LIGHT_COLOR_AMBIENT] = *ambient;
      priv->dirty_uniforms |= 1 << MASH_LIGHT_COLOR_AMBIENT;
      g_object_notify (G_OBJECT (light), "ambient");
    }
}

/**
 * mash_light_get_ambient:
 * @light: The #MashLight to query
 * @ambient: A return location for the color
 *
 * Retrieves the ‘ambient’ color emitted by the light.
 */
void
mash_light_get_ambient (MashLight *light, ClutterColor *ambient)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  *ambient = light->priv->light_colors[MASH_LIGHT_COLOR_AMBIENT];
}

/**
 * mash_light_set_diffuse:
 * @light: The #MashLight to modify
 * @diffuse: The new color value
 *
 * Sets the ‘diffuse’ color emitted by the light. The diffuse color is
 * only visible on an object if is facing the light. The orientation
 * of the object is determined per-vertex using the vertex's
 * normal. The diffuse color will be darkened depending on how
 * directly the object faces the light.
 */
void
mash_light_set_diffuse (MashLight *light, const ClutterColor *diffuse)
{
  MashLightPrivate *priv;

  g_return_if_fail (MASH_IS_LIGHT (light));

  priv = light->priv;

  if (!clutter_color_equal (diffuse,
                            &priv->light_colors + MASH_LIGHT_COLOR_DIFFUSE))
    {
      priv->light_colors[MASH_LIGHT_COLOR_DIFFUSE] = *diffuse;
      priv->dirty_uniforms |= 1 << MASH_LIGHT_COLOR_DIFFUSE;
      g_object_notify (G_OBJECT (light), "diffuse");
    }
}

/**
 * mash_light_get_diffuse:
 * @light: The #MashLight to query
 * @diffuse: A return location for the color
 *
 * Retrieves the ‘diffuse’ color emitted by the light.
 */
void
mash_light_get_diffuse (MashLight *light, ClutterColor *diffuse)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  *diffuse = light->priv->light_colors[MASH_LIGHT_COLOR_DIFFUSE];
}

/**
 * mash_light_set_specular:
 * @light: The #MashLight to modify
 * @specular: The new color value
 *
 * Sets the ‘specular’ color emitted by the light. The specular color
 * is used to add highlights to an object wherever the angle to the
 * light is close to the angle that the object is being viewed
 * from. For example, if you were modelling a snooker ball with a
 * bright light above it, this property will allow you add a bright
 * part where the light can directly reflect off the ball into the
 * eye. It is common to set this to a bright white value.
 */
void
mash_light_set_specular (MashLight *light, const ClutterColor *specular)
{
  MashLightPrivate *priv;

  g_return_if_fail (MASH_IS_LIGHT (light));

  priv = light->priv;

  if (!clutter_color_equal (specular,
                            &priv->light_colors + MASH_LIGHT_COLOR_SPECULAR))
    {
      priv->light_colors[MASH_LIGHT_COLOR_SPECULAR] = *specular;
      priv->dirty_uniforms |= 1 << MASH_LIGHT_COLOR_SPECULAR;
      g_object_notify (G_OBJECT (light), "specular");
    }
}

/**
 * mash_light_get_specular:
 * @light: The #MashLight to query
 * @specular: A return location for the color
 *
 * Retrieves the ‘specular’ color emitted by the light.
 */
void
mash_light_get_specular (MashLight *light, ClutterColor *specular)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  *specular = light->priv->light_colors[MASH_LIGHT_COLOR_SPECULAR];
}

/**
 * mash_light_generate_shader:
 * @light: A #MashLight
 * @uniform_source: A location to append uniforms declarations to
 * @main_source: A location to append lighting algorithm snippets to
 *
 * This function is used to generate the shader code required to
 * implement a paraticular. It would not usually need to be called
 * from an application. Instead it is called automatically by
 * #MashLightSet.
 *
 * This function can be overriden in subclasses of #MashLight to
 * implement custom lighting algorithms. The function will be called
 * before the first actor that is using the light set is painted
 * whenever it deems that the shader needs to be regenerated. It
 * currently will do this whenever a light is added or removed from
 * the box. The implementation should append any GLSL code to
 * @uniform_source and @main_source needed to implement the algorithm.
 *
 * The implementation should use mash_light_append_shader() to append
 * code to either of the shader strings so that it can declare
 * variables that are unique to the individual actor.
 *
 * The code in @uniform_source is inserted at the global level of a
 * vertex shader. It is expected that the light will add uniform
 * declarations here. For example, if the light depends on the light's
 * position it could define a uniform for the position like so:
 *
 * |[
 *   mash_light_append_shader (light, uniform_source,
 *                             "uniform vec3 position$;\n");
 * ]|
 *
 * The code in @main_source is inserted with the main function of a
 * vertex shader. The snippet added by a light is expected to modify
 * the gl_FrontColor attribute according to its algorithm. The snippet
 * can also use the following variables which will be initialized
 * before the snippet is run:
 *
 * normal: This will be a vec3 which is initialized to the transformed
 * and normalized vertex normal.
 *
 * eye_coord: This will be a vec3 containing the vertex coordinates in
 * eye-space.
 *
 * ambient_light: A vec3 uniform containing the ambient light color.
 *
 * diffuse_light: A vec3 uniform containing the diffuse light color.
 *
 * specular_light: A vec3 uniform containing the specular light color.
 *
 * mash_material.ambient: A vec4 containing the current material's
 *   ambient color.
 *
 * mash_material.diffuse: A vec4 containing the current material's
 *   diffuse color.
 *
 * mash_material.specular: A vec4 containing the current material's
 *   specular color.
 *
 * mash_material.emission: A vec4 containing the current material's
 *   emission color.
 *
 * mash_material.shininess: A float containing the current material's
 *   shininess value.
 *
 * mash_normal_matrix: A version of the modelview matrix used to
 * transform normals.
 *
 * In addition to these variables the shader can use all of the
 * built-in Cogl uniforms. Please see a future version of the Cogl
 * documentation for a description of these.
 *
 * The implementation should always chain up to the #MashLight
 * implementation so that it can declare the built-in uniforms.
 */
void
mash_light_generate_shader (MashLight *light,
                            GString *uniform_source,
                            GString *main_source)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  MASH_LIGHT_GET_CLASS (light)->generate_shader (light,
                                                 uniform_source,
                                                 main_source);
}

/**
 * mash_light_update_uniforms:
 * @light: The #MashLight that needs updating
 * @program: A #CoglProgram containing the uniforms
 *
 * This function is used by #MashLightSet to implement the lights. It
 * should not need to be called by an application directly.
 *
 * This function is virtual and can be overriden by subclasses to
 * implement custom lighting algorithms. The function is called during
 * the paint sequence of #MashLightSet on every light before any other
 * actors are painted. This gives the light implementation a chance to
 * update any uniforms it may have declared in the override of
 * mash_light_generate_shader().
 *
 * The program is always made current with cogl_program_use() before
 * this method is called so it is safe to directly call
 * cogl_program_uniform_1f() and friends to update the uniforms. The
 * @program handle is passed in so that the program can also be
 * queried to the locations of named
 * uniforms. mash_light_get_uniform_location() can be used to make
 * this easier when a uniform is named uniquely using the ‘$’ symbol
 * in mash_light_append_shader().
 */
void
mash_light_update_uniforms (MashLight *light, CoglHandle program)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  MASH_LIGHT_GET_CLASS (light)->update_uniforms (light, program);
}

/**
 * mash_light_append_shader:
 * @light: The #MashLight which is generating the shader
 * @shader_source: The string to append to
 * @snippet: A snippet of GLSL
 *
 * This is a convenience intended to be used within
 * mash_light_generate_shader() to generate shader snippets with
 * actor-specific variable names. It should not generally need to be
 * called by an application unless it is implementing its own lighting
 * algorithms.
 *
 * The code in @snippet is appended to @shader_source but all
 * occurences of the ‘$’ symbol are replaced with a string that is
 * unique to @light object. This is useful when multiple lights of the
 * same type are added to a single light box. For example, if a light
 * needs to have a position uniform it could make a call like the
 * following:
 *
 * |[
 *   mash_light_append_shader (light, uniform_source,
 *                             "uniform vec3 position$;\n");
 * ]|
 *
 * The ‘position’ will get translated to something like
 * ‘positiong00000002’.
 */
void
mash_light_append_shader (MashLight *light,
                          GString *shader_source,
                          const char *snippet)
{
  MashLightPrivate *priv;
  const char *dollar_pos;

  g_return_if_fail (MASH_IS_LIGHT (light));

  priv = light->priv;

  /* Replace any '$' symbols in the shader with the unique
     identifier */
  while ((dollar_pos = strchr (snippet, '$')))
    {
      g_string_append_len (shader_source, snippet, dollar_pos - snippet);
      g_string_append_len (shader_source, priv->unique_str,
                           MASH_LIGHT_UNIQUE_SYMBOL_SIZE);
      snippet = dollar_pos + 1;
    }

  g_string_append (shader_source, snippet);
}

/**
 * mash_light_get_uniform_location:
 * @light: The #MashLight which is generating the shader
 * @program: The program passed in from mash_light_update_uniforms().
 * @uniform_name: The name of a uniform
 *
 * This is a convenience intended to be used within
 * mash_light_update_uniforms() to help query uniform locations. It
 * should not generally need to be called by an application unless it
 * is implementing its own lighting algorithms.
 *
 * This is a wrapper around cogl_program_get_uniform_location() which
 * appends an actor specific string to the uniform name. This is
 * useful when uniforms have been declared like ‘position$’ within
 * mash_light_append_shader().
 */
int
mash_light_get_uniform_location (MashLight *light,
                                 CoglHandle program,
                                 const char *uniform_name)
{
  MashLightPrivate *priv;
  char *unique_name;
  int location;

  g_return_val_if_fail (MASH_IS_LIGHT (light), -1);

  priv = light->priv;

  /* Append this light's unique identifier to the uniform name */
  unique_name = g_strconcat (uniform_name, priv->unique_str, NULL);

  location = cogl_program_get_uniform_location (program, unique_name);

  g_free (unique_name);

  return location;
}

static void
transpose_matrix (const CoglMatrix *matrix,
                  CoglMatrix *transpose)
{
  const float *matrix_p = cogl_matrix_get_array (matrix);
  float matrix_array[16];
  int i, j;

  /* This should probably be in Cogl */
  for (j = 0; j < 4; j++)
    for (i = 0; i < 4; i++)
      matrix_array[i * 4 + j] = matrix_p[j * 4 + i];

  cogl_matrix_init_from_array (transpose, matrix_array);
}

/**
 * mash_light_get_modelview_matrix:
 * @light: A #MashLight
 * @matrix: The return location for the matrix
 *
 * Gets the modelview matrix for the light including all of the
 * transformations for its parent actors. This should be used for
 * updating uniforms that depend on the actor's transformation or
 * position.
 */
void
mash_light_get_modelview_matrix (MashLight *light,
                                 CoglMatrix *matrix)
{
  MashLightPrivate *priv = light->priv;

  if (priv->modelview_matrix_dirty)
    {
      ClutterActor *actor;
      GSList *parents = NULL, *l;

      cogl_matrix_init_identity (&priv->modelview_matrix);

      /* Get the complete modelview matrix for light by applying all of
         its parent transformations as well as its own in reverse */
      for (actor = CLUTTER_ACTOR (light);
           actor;
           actor = clutter_actor_get_parent (actor))
        parents = g_slist_prepend (parents, actor);

      for (l = parents; l; l = l->next)
        {
          CoglMatrix actor_matrix;

          cogl_matrix_init_identity (&actor_matrix);
          clutter_actor_get_transformation_matrix (CLUTTER_ACTOR (l->data),
                                                   &actor_matrix);

          cogl_matrix_multiply (&priv->modelview_matrix,
                                &priv->modelview_matrix,
                                &actor_matrix);
        }

      g_slist_free (parents);

      priv->modelview_matrix_dirty = FALSE;
    }

  *matrix = priv->modelview_matrix;
}

/**
 * mash_light_set_direction_uniform:
 * @light: The #MashLight which is generating the shader
 * @uniform_location: The location of the uniform
 * @direction_in: The untransformed direction uniform
 *
 * This is a convenience intended to be used within
 * mash_light_update_uniforms() to help set uniforms. It
 * should not generally need to be called by an application unless it
 * is implementing its own lighting algorithms.
 *
 * This is intended to help when setting a direction
 * uniform. @direction_in should be an untransformed array of 3 floats
 * representing a vector. The vector will be transformed into eye
 * space according to the inverse transposed matrix of @light so that
 * it won't change direction for non-uniform scaling transformations.
 */
void
mash_light_set_direction_uniform (MashLight *light,
                                  CoglHandle program,
                                  int uniform_location,
                                  const float *direction_in)
{
  float light_direction[4];
  CoglMatrix matrix, inverse_matrix;
  float magnitude;

  memcpy (light_direction, direction_in, sizeof (light_direction));

  mash_light_get_modelview_matrix (light, &matrix);

  /* To safely transform the direction when the matrix might not be
     orthogonal we need the transposed inverse matrix */

  cogl_matrix_get_inverse (&matrix, &inverse_matrix);
  transpose_matrix (&inverse_matrix, &matrix);

  cogl_matrix_transform_point (&matrix,
                               light_direction + 0,
                               light_direction + 1,
                               light_direction + 2,
                               light_direction + 3);

  /* Normalize the light direction */
  magnitude = sqrtf ((light_direction[0] * light_direction[0])
                     + (light_direction[1] * light_direction[1])
                     + (light_direction[2] * light_direction[2]));
  light_direction[0] /= magnitude;
  light_direction[1] /= magnitude;
  light_direction[2] /= magnitude;

  cogl_program_set_uniform_float (program,
                                  uniform_location,
                                  3, 1,
                                  light_direction);
}

static void
mash_light_real_generate_shader (MashLight *light,
                                 GString *uniform_source,
                                 GString *main_source)
{
  MashLightPrivate *priv = light->priv;

  /* If the shader is being regenerated then we know that the uniform
     locations are dirty */
  priv->uniform_locations_dirty = TRUE;
  priv->dirty_uniforms = (1 << MASH_LIGHT_COLOR_COUNT) - 1;

  /* Add the uniform definitions for the colors of this light */
  mash_light_append_shader (light,
                            uniform_source,
                            "uniform vec3 ambient_light$;\n"
                            "uniform vec3 diffuse_light$;\n"
                            "uniform vec3 specular_light$;\n");
}

static void
mash_light_real_update_uniforms (MashLight *light,
                                 CoglHandle program)
{
  MashLightPrivate *priv = light->priv;
  int i;

  /* We need to recalculate the light's transformation matrix. It is
     assumed the subclasses will chain-up first before calling
     mash_light_get_transformation_matrix(), otherwise this won't work
     correctly */
  priv->modelview_matrix_dirty = TRUE;

  if (priv->uniform_locations_dirty)
    {
      for (i = 0; i < MASH_LIGHT_COLOR_COUNT; i++)
        priv->uniform_locations[i]
          = mash_light_get_uniform_location (light, program,
                                             mash_light_color_names[i]);

      priv->uniform_locations_dirty = FALSE;
    }

  for (i = 0; i < MASH_LIGHT_COLOR_COUNT; i++)
    if (priv->dirty_uniforms & (1 << i))
      {
        const ClutterColor *color = priv->light_colors + i;
        float vec[3];

        vec[0] = color->red / 255.0f;
        vec[1] = color->green / 255.0f;
        vec[2] = color->blue / 255.0f;

        cogl_program_set_uniform_float (program,
                                        priv->uniform_locations[i],
                                        3, 1, vec);
      }

  priv->dirty_uniforms = 0;
}
