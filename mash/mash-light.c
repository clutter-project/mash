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
#include <string.h>

#include "mash-light.h"
#include "mash-light-box.h"

static void mash_light_get_property (GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec);
static void mash_light_set_property (GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec);

static void mash_light_parent_set (ClutterActor *self,
                                   ClutterActor *old_parent);

static void mash_light_real_generate_shader (MashLight *light,
                                             GString *uniform_source,
                                             GString *main_source);
static void mash_light_real_update_uniforms (MashLight *light,
                                             CoglHandle program);

static void mash_light_dispose (GObject *object);
static void mash_light_finalize (GObject *object);

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
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;
  GParamSpec *pspec;

  gobject_class->dispose = mash_light_dispose;
  gobject_class->finalize = mash_light_finalize;
  gobject_class->get_property = mash_light_get_property;
  gobject_class->set_property = mash_light_set_property;

  actor_class->parent_set = mash_light_parent_set;

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
}

static void
mash_light_parent_set (ClutterActor *self,
                       ClutterActor *old_parent)
{
  ClutterActor *new_parent = clutter_actor_get_parent (self);

  if (new_parent && !MASH_IS_LIGHT_BOX (new_parent))
    g_warning ("A MashLight is being added to a %s. The light will only "
               "have any effect if it is added directly to a MashLightBox",
               G_OBJECT_TYPE_NAME (new_parent));

  if (CLUTTER_ACTOR_CLASS (mash_light_parent_class)->parent_set)
    CLUTTER_ACTOR_CLASS (mash_light_parent_class)
      ->parent_set (self, old_parent);
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

void
mash_light_get_ambient (MashLight *light, ClutterColor *ambient)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  *ambient = light->priv->light_colors[MASH_LIGHT_COLOR_AMBIENT];
}

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

void
mash_light_get_diffuse (MashLight *light, ClutterColor *diffuse)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  *diffuse = light->priv->light_colors[MASH_LIGHT_COLOR_DIFFUSE];
}

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

void
mash_light_get_specular (MashLight *light, ClutterColor *specular)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  *specular = light->priv->light_colors[MASH_LIGHT_COLOR_SPECULAR];
}

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

void
mash_light_update_uniforms (MashLight *light, CoglHandle program)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  MASH_LIGHT_GET_CLASS (light)->update_uniforms (light, program);
}

static void
mash_light_dispose (GObject *object)
{
  MashLight *self = (MashLight *) object;

  G_OBJECT_CLASS (mash_light_parent_class)->dispose (object);
}

static void
mash_light_finalize (GObject *object)
{
  MashLight *self = (MashLight *) object;

  G_OBJECT_CLASS (mash_light_parent_class)->finalize (object);
}

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

        cogl_program_uniform_float (priv->uniform_locations[i],
                                    3, 1, vec);
      }

  priv->dirty_uniforms = 0;
}
