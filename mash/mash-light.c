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

static void mash_light_dispose (GObject *object);
static void mash_light_finalize (GObject *object);

static const ClutterColor
mash_light_default_color = { 0xff, 0xff, 0xff, 0xff };

G_DEFINE_ABSTRACT_TYPE (MashLight, mash_light, CLUTTER_TYPE_ACTOR);

#define MASH_LIGHT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_LIGHT, \
                                MashLightPrivate))

struct _MashLightPrivate
{
  /* Light colors for the three different lighting effects that are
     shared by all light types */
  ClutterColor ambient;
  ClutterColor diffuse;
  ClutterColor specular;
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

  priv = self->priv = MASH_LIGHT_GET_PRIVATE (self);

  priv->ambient = mash_light_default_color;
  priv->diffuse = mash_light_default_color;
  priv->specular = mash_light_default_color;
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

  if (!clutter_color_equal (ambient, &priv->ambient))
    {
      priv->ambient = *ambient;
      g_object_notify (G_OBJECT (light), "ambient");
    }
}

void
mash_light_get_ambient (MashLight *light, ClutterColor *ambient)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  *ambient = light->priv->ambient;
}

void
mash_light_set_diffuse (MashLight *light, const ClutterColor *diffuse)
{
  MashLightPrivate *priv;

  g_return_if_fail (MASH_IS_LIGHT (light));

  priv = light->priv;

  if (!clutter_color_equal (diffuse, &priv->diffuse))
    {
      priv->diffuse = *diffuse;
      g_object_notify (G_OBJECT (light), "diffuse");
    }
}

void
mash_light_get_diffuse (MashLight *light, ClutterColor *diffuse)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  *diffuse = light->priv->diffuse;
}

void
mash_light_set_specular (MashLight *light, const ClutterColor *specular)
{
  MashLightPrivate *priv;

  g_return_if_fail (MASH_IS_LIGHT (light));

  priv = light->priv;

  if (!clutter_color_equal (specular, &priv->specular))
    {
      priv->specular = *specular;
      g_object_notify (G_OBJECT (light), "specular");
    }
}

void
mash_light_get_specular (MashLight *light, ClutterColor *specular)
{
  g_return_if_fail (MASH_IS_LIGHT (light));

  *specular = light->priv->specular;
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
