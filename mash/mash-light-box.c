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
#include "mash-light.h"

static void mash_light_box_finalize (GObject *object);

static void mash_light_box_add (ClutterContainer *container,
                                ClutterActor *actor);
static void mash_light_box_remove (ClutterContainer *container,
                                   ClutterActor *actor);

static void mash_light_box_container_iface_init (ClutterContainerIface *iface);

static ClutterContainerIface *mash_light_box_container_parent_iface;

G_DEFINE_TYPE_WITH_CODE
(MashLightBox, mash_light_box, CLUTTER_TYPE_BOX,
 G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                        mash_light_box_container_iface_init));

#define MASH_LIGHT_BOX_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_LIGHT_BOX, \
                                MashLightBoxPrivate))

struct _MashLightBoxPrivate
{
  CoglHandle program;
};

static void
mash_light_box_class_init (MashLightBoxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = mash_light_box_finalize;

  g_type_class_add_private (klass, sizeof (MashLightBoxPrivate));
}

static void
mash_light_box_container_iface_init (ClutterContainerIface *iface)
{
  mash_light_box_container_parent_iface = g_type_interface_peek_parent (iface);

  /* We need to override the add and remove methods of the container
     interface so that we can detect when the number of lights is
     modified */
  iface->add = mash_light_box_add;
  iface->remove = mash_light_box_remove;
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
  MashLightBoxPrivate *priv = self->priv;

  if (priv->program)
    cogl_handle_unref (priv->program);

  G_OBJECT_CLASS (mash_light_box_parent_class)->finalize (object);
}

ClutterActor *
mash_light_box_new (ClutterLayoutManager *layout_manager)
{
  return g_object_new (MASH_TYPE_LIGHT_BOX,
                       "layout-manager", layout_manager,
                       NULL);
}

static void
mash_light_box_check_light_changed (MashLightBox *light_box,
                                    ClutterActor *actor)
{
  MashLightBoxPrivate *priv = light_box->priv;

  /* If we've added or removed a light then we need to regenerate the
     shader */
  if (MASH_IS_LIGHT (actor) && priv->program != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->program);
      priv->program = COGL_INVALID_HANDLE;
    }
}

static void
mash_light_box_add (ClutterContainer *container,
                    ClutterActor *actor)
{
  MashLightBox *light_box = MASH_LIGHT_BOX (container);

  mash_light_box_container_parent_iface->add (container, actor);

  mash_light_box_check_light_changed (light_box, actor);
}

static void
mash_light_box_remove (ClutterContainer *container,
                       ClutterActor *actor)
{
  MashLightBox *light_box = MASH_LIGHT_BOX (container);

  mash_light_box_container_parent_iface->remove (container, actor);

  mash_light_box_check_light_changed (light_box, actor);
}
