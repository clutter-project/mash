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
#include "mash-directional-light.h"

static void mash_directional_light_dispose (GObject *object);
static void mash_directional_light_finalize (GObject *object);

G_DEFINE_TYPE (MashDirectionalLight, mash_directional_light, MASH_TYPE_LIGHT);

#define MASH_DIRECTIONAL_LIGHT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_DIRECTIONAL_LIGHT, \
                                MashDirectionalLightPrivate))

struct _MashDirectionalLightPrivate
{
  int stub;
};

static void
mash_directional_light_class_init (MashDirectionalLightClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = mash_directional_light_dispose;
  gobject_class->finalize = mash_directional_light_finalize;

  g_type_class_add_private (klass, sizeof (MashDirectionalLightPrivate));
}

static void
mash_directional_light_init (MashDirectionalLight *self)
{
  self->priv = MASH_DIRECTIONAL_LIGHT_GET_PRIVATE (self);
}

static void
mash_directional_light_dispose (GObject *object)
{
  MashDirectionalLight *self = (MashDirectionalLight *) object;

  G_OBJECT_CLASS (mash_directional_light_parent_class)->dispose (object);
}

static void
mash_directional_light_finalize (GObject *object)
{
  MashDirectionalLight *self = (MashDirectionalLight *) object;

  G_OBJECT_CLASS (mash_directional_light_parent_class)->finalize (object);
}

ClutterActor *
mash_directional_light_new (void)
{
  ClutterActor *self = g_object_new (MASH_TYPE_DIRECTIONAL_LIGHT, NULL);

  return self;
}
