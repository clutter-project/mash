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

static void mash_light_dispose (GObject *object);
static void mash_light_finalize (GObject *object);

G_DEFINE_ABSTRACT_TYPE (MashLight, mash_light, CLUTTER_TYPE_ACTOR);

#define MASH_LIGHT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_LIGHT, \
                                MashLightPrivate))

struct _MashLightPrivate
{
  int stub;
};

static void
mash_light_class_init (MashLightClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = mash_light_dispose;
  gobject_class->finalize = mash_light_finalize;

  g_type_class_add_private (klass, sizeof (MashLightPrivate));
}

static void
mash_light_init (MashLight *self)
{
  self->priv = MASH_LIGHT_GET_PRIVATE (self);
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
