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
 * SECTION:mash-light-box
 * @short_description: A container which enables lighting on its children.
 *
 * #MashLightBox is a subclass of #ClutterBox with the special
 * property that it will apply a lighting model to all of its
 * children. The intention is that the children will be #MashModel<!--
 * -->s but it can apply the lighting to any actors. All of the
 * builtin light types depend on a ‘normal’ attribute being defined on
 * each vertex of the children so it would only make sense to use
 * these with #MashModel<!-- -->s containing normals.
 *
 * The box implements the Blinn-Phong lighting model which is the
 * standard model used in fixed function version of OpenGL and
 * Direct3D. The lighting calculations are performed per-vertex and
 * then interpolated across the surface of the primitives.
 *
 * Lights are positioned within the light box by adding #MashLight<!--
 * -->s to the container. The lights must be direct children of the
 * box to work (ie, they can be within another container in the
 * box). The lights are subclasses of #ClutterActor so they can be
 * positioned and animated using the usual Clutter animation
 * framework.
 *
 * The lighting implementation requires GLSL support from Clutter. If
 * the application can still work without lighting it would be worth
 * checking for shader support by passing %COGL_FEATURE_SHADERS_GLSL
 * to cogl_features_available().
 *
 * It should be possible to extend the lighting model and implement
 * application-specific lighting algorithms by subclassing #MashLight
 * and adding shader snippets by overriding
 * mash_light_generate_shader().
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <clutter/clutter.h>

#include "mash-light-box.h"
#include "mash-light.h"

static void mash_light_box_finalize (GObject *object);

static void mash_light_box_paint (ClutterActor *actor);

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
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  gobject_class->finalize = mash_light_box_finalize;

  actor_class->paint = mash_light_box_paint;

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

/**
 * mash_light_box_new:
 * @layout_manager: A #ClutterLayoutManager subclass
 *
 * Constructs a new #MashLightBox. A layout manager must be specified
 * with @layout_manager. To get similar fixed positioning semantics as
 * #ClutterGroup, a #ClutterFixedLayout instance could be used like
 * so:
 *
 * |[
 *   ClutterActor *box = mash_light_box_new (clutter_fixed_layout_new ());
 * ]|
 *
 * For details of other layouts that can be used, see
 * clutter_box_new().
 *
 * Return value: a new #MashLightBox.
 */
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

struct MashLightBoxGetProgramData
{
  GString *uniform_source;
  GString *main_source;
};

static void
mash_light_box_generate_shader_cb (ClutterActor *actor,
                                   gpointer data_p)
{
  struct MashLightBoxGetProgramData *data = data_p;

  if (MASH_IS_LIGHT (actor))
    mash_light_generate_shader (MASH_LIGHT (actor),
                                data->uniform_source,
                                data->main_source);
}

static CoglHandle
mash_light_box_get_program (MashLightBox *light_box)
{
  MashLightBoxPrivate *priv = light_box->priv;

  if (priv->program == COGL_INVALID_HANDLE)
    {
      struct MashLightBoxGetProgramData data;
      char *full_source;
      CoglHandle shader;
      char *info_log;

      data.uniform_source = g_string_new (NULL);
      data.main_source = g_string_new (NULL);

      /* Give all of the lights in the scene a chance to modify the
         shader source */
      clutter_container_foreach (CLUTTER_CONTAINER (light_box),
                                 mash_light_box_generate_shader_cb,
                                 &data);

      /* Append the shader boiler plate */
      g_string_append (data.uniform_source,
                       "\n"
                       "void\n"
                       "main ()\n"
                       "{\n"
                       /* Start with a completely unlit vertex. The
                          lights should add to this color */
                       "  gl_FrontColor = vec4 (0.0, 0.0, 0.0, 1.0);\n"
                       /* Calculate a transformed and normalized
                          vertex normal */
                       "  vec3 normal = normalize (gl_NormalMatrix\n"
                       "                           * gl_Normal);\n"
                       /* Calculate the vertex position in eye coordinates */
                       "  vec4 homogenous_eye_coord\n"
                       "    = gl_ModelViewMatrix * gl_Vertex;\n"
                       "  vec3 eye_coord = homogenous_eye_coord.xyz\n"
                       "    / homogenous_eye_coord.w;\n");
      /* Append the main source to the uniform source to get the full
         source for the shader */
      g_string_append_len (data.uniform_source,
                           data.main_source->str,
                           data.main_source->len);
      /* Perform the standard vertex transformation and copy the
         texture coordinates. FIXME: This is limited to CoglMaterials
         that only have one layer. Hopefully this could be fixed when
         Cogl has a way to insert shader snippets rather than having
         to replace the whole pipeline. */
      g_string_append (data.uniform_source,
                       "  gl_Position = ftransform ();\n"
                       "  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
                       "}\n");

      full_source = g_string_free (data.uniform_source, FALSE);
      g_string_free (data.main_source, TRUE);

      priv->program = cogl_create_program ();

      shader = cogl_create_shader (COGL_SHADER_TYPE_VERTEX);
      cogl_shader_source (shader, full_source);
      g_free (full_source);
      cogl_shader_compile (shader);

      if (!cogl_shader_is_compiled (shader))
        g_warning ("Error compiling light box shader");

      info_log = cogl_shader_get_info_log (shader);

      if (info_log)
        {
          if (*info_log)
            g_warning ("The light box shader has an info log:\n%s", info_log);

          g_free (info_log);
        }

      cogl_program_attach_shader (priv->program, shader);
      cogl_program_link (priv->program);
    }

  return priv->program;
}

static void
mash_light_box_update_uniforms_cb (ClutterActor *actor,
                                   gpointer data_p)
{
  CoglHandle program = data_p;

  if (MASH_IS_LIGHT (actor))
    mash_light_update_uniforms (MASH_LIGHT (actor), program);
}

static void
mash_light_box_paint (ClutterActor *actor)
{
  MashLightBox *light_box = MASH_LIGHT_BOX (actor);
  CoglHandle program;

  program = mash_light_box_get_program (light_box);

  cogl_program_use (program);

  /* Give all of the lights a chance to update the uniforms before we
     paint any other actors */
  clutter_container_foreach (CLUTTER_CONTAINER (light_box),
                             mash_light_box_update_uniforms_cb,
                             program);

  /* Chain up to paint the rest of the children */
  CLUTTER_ACTOR_CLASS (mash_light_box_parent_class)->paint (actor);

  cogl_program_use (COGL_INVALID_HANDLE);
}

