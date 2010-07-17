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

#include "mash-light-box.h"

static void mash_light_box_finalize (GObject *object);

G_DEFINE_TYPE (MashLightBox, mash_light_box, CLUTTER_TYPE_BOX);

#define MASH_LIGHT_BOX_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_LIGHT_BOX, \
                                MashLightBoxPrivate))

struct _MashLightBoxPrivate
{
  int stub;
};

static void
mash_light_box_class_init (MashLightBoxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = mash_light_box_finalize;

  g_type_class_add_private (klass, sizeof (MashLightBoxPrivate));
}

static void
mash_light_box_init (MashLightBox *self)
{
  self->priv = MASH_LIGHT_BOX_GET_PRIVATE (self);
}

static void
mash_light_box_finalize (GObject *object)
{
  MashLightBox *self = (MashLightBox *) object;

  G_OBJECT_CLASS (mash_light_box_parent_class)->finalize (object);
}

ClutterActor *
mash_light_box_new (void)
{
  ClutterActor *self = g_object_new (MASH_TYPE_LIGHT_BOX, NULL);

  return self;
}
