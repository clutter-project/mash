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

static void mash_point_light_dispose (GObject *object);
static void mash_point_light_finalize (GObject *object);

G_DEFINE_TYPE (MashPointLight, mash_point_light, MASH_TYPE_LIGHT);

#define MASH_POINT_LIGHT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_POINT_LIGHT, \
                                MashPointLightPrivate))

struct _MashPointLightPrivate
{
  int stub;
};

static void
mash_point_light_class_init (MashPointLightClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = mash_point_light_dispose;
  gobject_class->finalize = mash_point_light_finalize;

  g_type_class_add_private (klass, sizeof (MashPointLightPrivate));
}

static void
mash_point_light_init (MashPointLight *self)
{
  self->priv = MASH_POINT_LIGHT_GET_PRIVATE (self);
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
