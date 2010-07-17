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
#include "mash-point-light.h"

static void mash_point_light_get_property (GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec);
static void mash_point_light_set_property (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);

static void mash_point_light_dispose (GObject *object);
static void mash_point_light_finalize (GObject *object);

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

  /* TRUE if the attenuation factors have been modified since
     update_uniforms was last called */
  gboolean attenuation_dirty;
};

enum
  {
    PROP_0,

    PROP_CONSTANT_ATTENUATION,
    PROP_LINEAR_ATTENUATION,
    PROP_QUADRATIC_ATTENUATION
  };

static void
mash_point_light_class_init (MashPointLightClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GParamSpec *pspec;

  gobject_class->dispose = mash_point_light_dispose;
  gobject_class->finalize = mash_point_light_finalize;
  gobject_class->get_property = mash_point_light_get_property;
  gobject_class->set_property = mash_point_light_set_property;

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

static void
mash_point_light_dispose (GObject *object)
{
  MashPointLight *self = (MashPointLight *) object;

  G_OBJECT_CLASS (mash_point_light_parent_class)->dispose (object);
}

static void
mash_point_light_finalize (GObject *object)
{
  MashPointLight *self = (MashPointLight *) object;

  G_OBJECT_CLASS (mash_point_light_parent_class)->finalize (object);
}

ClutterActor *
mash_point_light_new (void)
{
  ClutterActor *self = g_object_new (MASH_TYPE_POINT_LIGHT, NULL);

  return self;
}

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

gfloat
mash_point_light_get_constant_attenuation (MashPointLight *light)
{
  g_return_val_if_fail (MASH_IS_POINT_LIGHT (light), 0.0f);

  return light->priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_CONSTANT];
}

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

gfloat
mash_point_light_get_linear_attenuation (MashPointLight *light)
{
  g_return_val_if_fail (MASH_IS_POINT_LIGHT (light), 0.0f);

  return light->priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_LINEAR];
}

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

gfloat
mash_point_light_get_quadratic_attenuation (MashPointLight *light)
{
  g_return_val_if_fail (MASH_IS_POINT_LIGHT (light), 0.0f);

  return light->priv->attenuation[MASH_POINT_LIGHT_ATTENUATION_QUADRATIC];
}
