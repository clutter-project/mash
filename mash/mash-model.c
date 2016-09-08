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
 * SECTION:mash-model
 * @short_description: An actor that can be used to render a PLY model.
 *
 * #MashModel is an actor subclass that can be used to render a
 * 3D model. The model is a normal #ClutterActor that can be animated
 * and positioned with the methods of #ClutterActor.
 *
 * By default the model will be scaled to best fit within the size of
 * the actor. Therefore it is possible to take a small model that may
 * have positions ranging between -1 and 1 and draw it at a larger
 * size just by setting the size on the actor. This behaviour can be
 * disabled with mash_model_set_fit_to_allocation().
 *
 * The actual data for the model is stored in a separate object called
 * #MashData. This can be used to share the data for a model
 * between multiple actors without having to duplicate resources of
 * the data. Alternatively mash_model_new_from_file() can be
 * used as a convenience wrapper to easily make an actor out of a PLY
 * without having to worry about #MashData. To share the data
 * with another actor, call mash_model_get_data() on an
 * existing actor then call mash_model_set_data() with the
 * return value on a new actor.
 *
 * The model can be rendered with any Cogl pipeline. By default the
 * model will use a solid white pipeline. The pipeline color is
 * blended with the model's vertex colors so the white pipeline will
 * cause the vertex colors to be used directly. #MashData is
 * able to load texture coordinates from the PLY file so it is
 * possible to render a textured model by setting a texture layer on
 * the pipeline, like so:
 *
 * |[
 *   /&ast; Create an actor out of a PLY model file &ast;/
 *   ClutterActor *model
 *     = mash_model_new_from_file ("some-model.ply", NULL);
 *   /&ast; Get a handle to the default pipeline for the actor &ast;/
 *   CoglHandle pipeline
 *     = mash_model_get_pipeline (MASH_MODEL (model));
 *   /&ast; Load a texture image from a file &ast;/
 *   CoglHandle texture
 *     = cogl_texture_new_from_file ("some-image.png", COGL_TEXTURE_NONE,
 *                                   COGL_PIXEL_FORMAT_ANY, NULL);
 *   /&ast; Set a texture layer on the pipeline &ast;/
 *   cogl_pipeline_set_layer (pipeline, 0, texture);
 *   /&ast; The texture is now referenced by the pipeline so we can
 *     drop the reference we have &ast;/
 *   cogl_handle_unref (texture);
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API
#define COGL_ENABLE_EXPERIMENTAL_API


#include <glib-object.h>
#include <string.h>
#include <cogl/cogl.h>
#include <clutter/clutter.h>

#include "mash-model.h"
#include "mash-data.h"

static void mash_model_dispose (GObject *object);

static void mash_model_get_property (GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec);
static void mash_model_set_property (GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec);

static void mash_model_paint (ClutterActor *actor);

static void mash_model_pick (ClutterActor *actor,
                             const ClutterColor *pick_color);

static void mash_model_get_preferred_width (ClutterActor *actor,
                                            gfloat for_height,
                                            gfloat *minimum_width,
                                            gfloat *natural_width);
static void mash_model_get_preferred_height (ClutterActor *actor,
                                             gfloat for_width,
                                             gfloat *minimum_height,
                                             gfloat *natural_height);

static void mash_model_allocate (ClutterActor *actor,
                                 const ClutterActorBox *box,
                                 ClutterAllocationFlags flags);

G_DEFINE_TYPE (MashModel, mash_model, CLUTTER_TYPE_ACTOR);

#define MASH_MODEL_GET_PRIVATE(obj)                     \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_MODEL, \
                                MashModelPrivate))

struct _MashModelPrivate
{
  MashData *data;
  MashLightSet *light_set;
  CoglPipeline *pipeline, *pick_pipeline;
  CoglFramebuffer *fb;
  /* Whether the model should be transformed to fill the allocation */
  gboolean fit_to_allocation;
  /* The amount to scale (on all axes) when fit_to_allocation is
     TRUE. This is calculated in the allocate method */
  gfloat scale;
  /* Translation used when fit_to_allocation is TRUE. This is
     calculated in the allocate method */
  gfloat translate_x, translate_y, translate_z;
  gfloat progress;
  gboolean pipeline_created;
};

enum
  {
    PROP_0,

    PROP_PIPELINE,
    PROP_DATA,
    PROP_LIGHT_SET,
    PROP_FIT_TO_ALLOCATION
  };

static void
mash_model_class_init (MashModelClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;
  GParamSpec *pspec;

  gobject_class->dispose = mash_model_dispose;
  gobject_class->get_property = mash_model_get_property;
  gobject_class->set_property = mash_model_set_property;

  actor_class->paint = mash_model_paint;
  //actor_class->pick = mash_model_pick;
  actor_class->get_preferred_width = mash_model_get_preferred_width;
  actor_class->get_preferred_height = mash_model_get_preferred_height;
  actor_class->allocate = mash_model_allocate;

  pspec = g_param_spec_boxed ("pipeline",
                              "pipeline",
                              "The Cogl pipeline to render with",
                              COGL_TYPE_HANDLE,
                              G_PARAM_READABLE | G_PARAM_WRITABLE
                              | G_PARAM_STATIC_NAME
                              | G_PARAM_STATIC_NICK
                              | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class, PROP_PIPELINE, pspec);

  pspec = g_param_spec_object ("data",
                               "Data",
                               "The MashData to render",
                               MASH_TYPE_DATA,
                               G_PARAM_READABLE | G_PARAM_WRITABLE
                               | G_PARAM_STATIC_NAME
                               | G_PARAM_STATIC_NICK
                               | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class, PROP_DATA, pspec);

  pspec = g_param_spec_object ("light-set",
                               "Light set",
                               "The MashLightSet to use for the lighting model",
                               MASH_TYPE_LIGHT_SET,
                               G_PARAM_READABLE | G_PARAM_WRITABLE
                               | G_PARAM_STATIC_NAME
                               | G_PARAM_STATIC_NICK
                               | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class, PROP_LIGHT_SET, pspec);

  pspec = g_param_spec_boolean ("fit-to-allocation",
                                "Fit to allocation",
                                "Whether to transform the model so that "
                                "it fills the actor's allocation while "
                                "preserving the aspect ratio",
                                TRUE,
                                G_PARAM_READABLE | G_PARAM_WRITABLE
                                | G_PARAM_STATIC_NAME
                                | G_PARAM_STATIC_NICK
                                | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class,
                                   PROP_FIT_TO_ALLOCATION, pspec);

  g_type_class_add_private (klass, sizeof (MashModelPrivate));
}

static void
mash_model_init (MashModel *self)
{
  MashModelPrivate *priv;

  priv = self->priv = MASH_MODEL_GET_PRIVATE (self);

  /* Default to a plain white pipeline */
  
  CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  priv->pipeline = cogl_pipeline_new (ctx);
  priv->pick_pipeline = cogl_pipeline_new (ctx);

  priv->fit_to_allocation = TRUE;
}

/**
 * mash_model_new:
 *
 * Constructs a new #MashModel. Nothing will be rendered by the
 * model until a #MashData is attached using
 * mash_model_set_data().
 *
 * Return value: a new #MashModel.
 */

ClutterActor *
mash_model_new (void)
{
  ClutterActor *self = g_object_new (MASH_TYPE_MODEL, NULL);

  return self;
}

/**
 * mash_model_new_from_file:
 * @flags: Flags for loading the data.
 * @filename: The name of a PLY file to load.
 * @error: Return location for a #GError or %NULL.
 *
 * This is a convenience function that creates a new #MashData
 * and immediately loads the data in @filename. If the load succeeds a
 * new #MashModel will be created for the data. The model has a
 * default white pipeline so that if vertices of the model have any
 * color attributes they will be used directly. The pipeline does not
 * have textures by default so if you want the model to be textured
 * you will need to modify the pipeline.
 *
 * Return value: a new #MashModel or %NULL if the load failed.
 */
ClutterActor *
mash_model_new_from_file (MashDataFlags flags,
                          const gchar *filename,
                          GError **error)
{
  MashData *data = mash_data_new ();
  ClutterActor *model = NULL;

  if (mash_data_load (data, flags, filename, error))
    {
      model = mash_model_new ();
      mash_model_set_data (MASH_MODEL (model), data);
    }

  g_object_unref (data);

  return model;
}

static void
mash_model_dispose (GObject *object)
{
  MashModel *self = (MashModel *) object;
  MashModelPrivate *priv = self->priv;

  mash_model_set_data (self, NULL);
  mash_model_set_pipeline (self, COGL_INVALID_HANDLE);

  if (priv->pick_pipeline)
    {
      cogl_handle_unref (priv->pick_pipeline);
      priv->pick_pipeline = COGL_INVALID_HANDLE;
    }

  mash_model_set_light_set (self, NULL);

  G_OBJECT_CLASS (mash_model_parent_class)->dispose (object);
}

/**
 * mash_model_set_pipeline:
 * @self: A #MashModel instance
 * @pipeline: A handle to a Cogl pipeline
 *
 * Replaces the pipeline that will be used to render the model with
 * the given one. By default a #MashModel will use a solid white
 * pipeline. However the color of the pipeline is still blended with
 * the vertex colors so the white pipeline will cause the vertex
 * colors to be used directly. If you want the model to be textured
 * you will need to create a pipeline that has a texture layer and set
 * it with this function.
 *
 * If a #MashLightSet is used with the model then the pipeline given
 * here will be modified to use the program generated by that light
 * set. If multiple models are expected to use the same pipeline with
 * different light sets, it would be better to use a different copy of
 * the same pipeline for each set of models so that they don't
 * repeatedly change the program on the pipeline during paint.
 */
void
mash_model_set_pipeline (MashModel *self,
                         CoglPipeline* pipeline)
{
  MashModelPrivate *priv;

  //fprintf(stderr, "mash_model_set_pipeline\n");
  
  g_return_if_fail (MASH_IS_MODEL (self));
  g_return_if_fail (pipeline == COGL_INVALID_HANDLE
                    || cogl_is_pipeline (pipeline));


  priv = self->priv;

  if (pipeline)
    cogl_handle_ref (pipeline);

  if (priv->pipeline)
    cogl_handle_unref (priv->pipeline);

  priv->pipeline = pipeline;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (self));

  g_object_notify (G_OBJECT (self), "pipeline");
}

/**
 * mash_model_get_pipeline:
 * @self: A #MashModel instance
 *
 * Gets the pipeline that will be used to render the model. The
 * pipeline can be modified to affect the appearence of the model. By
 * default the pipeline will be solid white.
 *
 * Return value: a handle to the Cogl pipeline used by the model.
 */
CoglPipeline*
mash_model_get_pipeline (MashModel *self)
{
  g_return_val_if_fail (MASH_IS_MODEL (self), COGL_INVALID_HANDLE);

  return self->priv->pipeline;
}

static void
mash_model_render_data (MashModel *self)
{
  MashModelPrivate *priv = self->priv;
  priv->fb = cogl_get_draw_framebuffer();

  if (priv->fit_to_allocation)
    {
      cogl_framebuffer_push_matrix (priv->fb);

      cogl_framebuffer_translate (priv->fb, 
                      priv->translate_x,
                      priv->translate_y,
                      priv->translate_z);
      cogl_framebuffer_scale (priv->fb, priv->scale, priv->scale, priv->scale);
    }

  mash_data_render (priv->data, (CoglPipeline *) priv->pipeline);

  if (priv->fit_to_allocation)
    cogl_framebuffer_pop_matrix (priv->fb);
}

static void
mash_model_paint (ClutterActor *actor){
  MashModel *self = MASH_MODEL (actor);
  MashModelPrivate *priv;

  g_return_if_fail (MASH_IS_MODEL (self));

  priv = self->priv;
  priv->fb = cogl_get_draw_framebuffer();

  /* Silently fail if we haven't got any data or a pipeline */
  if (priv->data == NULL || priv->pipeline == COGL_INVALID_HANDLE)
    return;

  if(!cogl_is_pipeline(priv->pipeline))
    return;

  if (priv->light_set){
    if(mash_light_set_update_layer_indices(priv->light_set, priv->pipeline)){
      // Recreate program
      priv->pipeline_created = FALSE;
    }
    if(!priv->pipeline_created){
      mash_light_set_get_pipeline (priv->light_set, priv->pipeline);
      priv->pipeline_created = TRUE;
    }
    mash_light_set_begin_paint(priv->light_set, priv->pipeline);    
  }
  
  /* Now we can render with the snippet as usual */
  mash_model_render_data (self);
}

static void
mash_model_pick (ClutterActor *actor,
                 const ClutterColor *pick_color)
{
  MashModel *self = MASH_MODEL (actor);
  MashModelPrivate *priv;
  CoglColor color;
  g_return_if_fail (MASH_IS_MODEL (self));

  priv = self->priv;

  /* Silently fail if we haven't got any data */
  if (priv->data == NULL){
    return;
  }
  
  if (priv->pick_pipeline == COGL_INVALID_HANDLE){
      GError *error = NULL;
      if (!cogl_pipeline_set_layer_combine (priv->pick_pipeline, 0,
                                            "RGBA=REPLACE(CONSTANT)",
                                            &error))
        {
          g_warning ("Error setting pick combine: %s", error->message);
          g_clear_error (&error);
        }
    }

  cogl_color_init_from_4ub (&color,
                           pick_color->red,
                           pick_color->green,
                           pick_color->blue,
                           255);
  cogl_pipeline_set_layer_combine_constant (priv->pick_pipeline, 0, &color);
  //cogl_set_source (priv->pick_pipeline);
  mash_model_render_data (self);
}

/**
 * mash_model_get_data:
 * @self: A #MashModel instance
 *
 * Gets the model data that will be used to render the actor.
 *
 * Return value: A pointer to a #MashData instance or %NULL if
 * no data has been set yet.
 */
MashData *
mash_model_get_data (MashModel *self)
{
  g_return_val_if_fail (MASH_IS_MODEL (self), NULL);

  return self->priv->data;
}

/**
 * mash_model_set_data:
 * @self: A #MashModel instance
 * @data: The new #MashData
 *
 * Replaces the data used by the actor with @data. A reference is
 * taken on @data so if you no longer need it you should unref it with
 * g_object_unref().
 */
void
mash_model_set_data (MashModel *self,
                     MashData *data)
{
  MashModelPrivate *priv;

  g_return_if_fail (MASH_IS_MODEL (self));
  g_return_if_fail (data == NULL || MASH_IS_DATA (data));

  priv = self->priv;

  if (data)
    g_object_ref (data);

  if (priv->data)
    g_object_unref (priv->data);

  priv->data = data;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

  g_object_notify (G_OBJECT (self), "data");
}

/**
 * mash_model_get_light_set:
 * @self: A #MashModel instance
 *
 * Return value: the #MashLightSet previously set with
 * mash_model_set_light_set().
 *
 * Since: 0.2
 */
MashLightSet *
mash_model_get_light_set (MashModel *self)
{
  g_return_val_if_fail (MASH_IS_MODEL (self), NULL);

  return self->priv->light_set;
}

/**
 * mash_model_set_light_set:
 * @self: A #MashModel instance
 * @light_set: A new #MashLightSet
 *
 * This sets the #MashLightSet that will be used to render the
 * model. Alternatively %NULL can be passed to disable lighting for
 * this model. The light set represents a collection of #MashLight<!--
 * -->s that will affect the appearance of the model.
 *
 * Since: 0.2
 */
void
mash_model_set_light_set (MashModel *self,
                          MashLightSet *light_set)
{
  MashModelPrivate *priv;

  g_return_if_fail (MASH_IS_MODEL (self));
  g_return_if_fail (light_set == NULL || MASH_IS_LIGHT_SET (light_set));

  priv = self->priv;

  if (light_set)
    g_object_ref (light_set);

  if (priv->light_set)
    g_object_unref (priv->light_set);

  priv->light_set = light_set;

  //if (light_set == NULL && priv->pipeline)
  //  cogl_pipeline_set_user_program (priv->pipeline, COGL_INVALID_HANDLE);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

  g_object_notify (G_OBJECT (self), "light-set");
}

/**
 * mash_model_get_fit_to_allocation:
 * @self: A #MashModel instance
 *
 * Return value: whether the actor will try to scale the model to fit
 * within the allocation.
 */
gboolean
mash_model_get_fit_to_allocation (MashModel *self)
{
  g_return_val_if_fail (MASH_IS_MODEL (self), FALSE);

  return self->priv->fit_to_allocation;
}

/**
 * mash_model_set_fit_to_allocation:
 * @self: A #MashModel instance
 * @fit_to_allocation: New value
 *
 * This sets whether the actor should scale the model to fit the
 * actor's allocation. If it's %TRUE then all of the axes of the model
 * will be scaled by the same amount to fill the allocation as much as
 * possible without distorting the aspect ratio. The model is also
 * translated so that it is at the center of the allocation and
 * centered at 0 along the z axis. The size along the z axis is not
 * considered when calculating a scale so if the model is largest
 * along that axis then the actor may appear too large. The
 * transformations are applied in addition to the actor's
 * transformations so it is still possible scale the actor further
 * using the scale-x and scale-y properties. The preferred size of the
 * actor will be the width and height of the model. If
 * width-for-height or height-for-width allocation is being used then
 * #MashModel will return whatever width or height will exactly
 * preserve the aspect ratio.
 *
 * If the value is %FALSE then the actor is not transformed so the
 * origin of the model will be the top left corner of the actor. The
 * preferred size of the actor will be maximum extents of the model
 * although the allocation is not considered during paint so if the
 * model extends past the allocated size then it will draw outside the
 * allocation.
 *
 * The default value is %TRUE.
 */
void
mash_model_set_fit_to_allocation (MashModel *self,
                                  gboolean fit_to_allocation)
{
  MashModelPrivate *priv;

  g_return_if_fail (MASH_IS_MODEL (self));

  priv = self->priv;

  if (priv->fit_to_allocation != fit_to_allocation)
    {
      priv->fit_to_allocation = fit_to_allocation;
      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
      g_object_notify (G_OBJECT (self), "fit-to-allocation");
    }
}

static void
mash_model_get_property (GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  MashModel *model = MASH_MODEL (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      g_value_set_boxed (value, mash_model_get_pipeline (model));
      break;

    case PROP_DATA:
      g_value_set_object (value, mash_model_get_data (model));
      break;

    case PROP_LIGHT_SET:
      g_value_set_object (value, mash_model_get_light_set (model));
      break;

    case PROP_FIT_TO_ALLOCATION:
      g_value_set_boolean (value,
                           mash_model_get_fit_to_allocation (model));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mash_model_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  MashModel *model = MASH_MODEL (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      mash_model_set_pipeline (model, g_value_get_boxed (value));
      break;

    case PROP_DATA:
      mash_model_set_data (model, g_value_get_object (value));
      break;

    case PROP_LIGHT_SET:
      mash_model_set_light_set (model, g_value_get_object (value));
      break;

    case PROP_FIT_TO_ALLOCATION:
      mash_model_set_fit_to_allocation (model,
                                        g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mash_model_get_preferred_width (ClutterActor *actor,
                                gfloat for_height,
                                gfloat *minimum_width_p,
                                gfloat *natural_width_p)
{
  MashModel *model = MASH_MODEL (actor);
  MashModelPrivate *priv = model->priv;
  ClutterVertex min_vertex, max_vertex;
  gfloat minimum_width, natural_width;

  if (priv->data)
    {
      mash_data_get_extents (priv->data, &min_vertex, &max_vertex);

      if (priv->fit_to_allocation)
        {
          gfloat model_width = max_vertex.x - min_vertex.x;
          gfloat model_height = max_vertex.y - min_vertex.y;

          if (for_height < 0.0f || model_height == 0.0f)
            natural_width = model_width;
          else
            /* Pick a width that would preserve the aspect ratio */
            natural_width = for_height * model_width / model_height;

          minimum_width = 0.0f;
        }
      else
        /* We can't report if the actor draws to the left of the origin so
           the best we can do is to report the extent to the right of the
           origin. If the data also contains vertices to the left of the
           origin then this won't be the actual width */
        minimum_width = natural_width = max_vertex.x;
    }
  else
    minimum_width = natural_width = 0.0f;

  if (minimum_width_p)
    *minimum_width_p = minimum_width;
  if (natural_width_p)
    *natural_width_p = natural_width;
}

static void
mash_model_get_preferred_height (ClutterActor *actor,
                                 gfloat for_width,
                                 gfloat *minimum_height_p,
                                 gfloat *natural_height_p)
{
  MashModel *model = MASH_MODEL (actor);
  MashModelPrivate *priv = model->priv;
  ClutterVertex min_vertex, max_vertex;
  gfloat minimum_height, natural_height;

  if (priv->data)
    {
      mash_data_get_extents (priv->data, &min_vertex, &max_vertex);

      if (priv->fit_to_allocation)
        {
          gfloat model_width = max_vertex.x - min_vertex.x;
          gfloat model_height = max_vertex.y - min_vertex.y;

          if (for_width < 0.0f || model_width == 0.0f)
            natural_height = model_height;
          else
            /* Pick a height that would preserve the aspect ratio */
            natural_height = for_width * model_height / model_width;

          minimum_height = 0.0f;
        }
      else
        /* We can't report if the actor draws above the origin so the
           best we can do is to report the extent below the origin. If
           the data also contains vertices above the origin then this
           won't be the actual height */
        minimum_height = natural_height = max_vertex.y;
    }
  else
    minimum_height = natural_height = 0.0f;

  if (minimum_height_p)
    *minimum_height_p = minimum_height;
  if (natural_height_p)
    *natural_height_p = natural_height;
}

static gfloat
mash_model_calculate_scale (gfloat target_extents,
                            gfloat min,
                            gfloat max)
{
  if (min == max)
    return G_MAXFLOAT;
  else
    return target_extents / (max - min);
}

static void
mash_model_allocate (ClutterActor *actor,
                     const ClutterActorBox *box,
                     ClutterAllocationFlags flags)
{
  MashModel *self = MASH_MODEL (actor);
  MashModelPrivate *priv = self->priv;

  CLUTTER_ACTOR_CLASS (mash_model_parent_class)
    ->allocate (actor, box, flags);

  if (priv->fit_to_allocation && priv->data)
    {
      ClutterVertex min_vertex, max_vertex;
      gfloat scale, min_scale;

      /* Try to scale the model uniformly so that it will fill the
         maximum amount of space without breaking the aspect
         ratio. The model is then centered in the allocation */
      mash_data_get_extents (priv->data, &min_vertex, &max_vertex);

      min_scale = mash_model_calculate_scale (box->x2 - box->x1,
                                              min_vertex.x,
                                              max_vertex.x);
      scale = mash_model_calculate_scale (box->y2 - box->y1,
                                          min_vertex.y,
                                          max_vertex.y);
      if (min_scale > scale)
        min_scale = scale;

      if (min_scale >= G_MAXFLOAT)
        min_scale = 0.0f;

      priv->scale = min_scale;

      priv->translate_x = ((box->x2 - box->x1) / 2.0f
                           - (min_vertex.x + max_vertex.x) / 2.0f * min_scale);
      priv->translate_y = ((box->y2 - box->y1) / 2.0f
                           - (min_vertex.y + max_vertex.y) / 2.0f * min_scale);
      priv->translate_z = -(min_vertex.z + max_vertex.z) / 2.0f * min_scale;
    }
}

/**
 * mash_model_get_model_depth:
 * @self: A #MashModel instance
 *
 * Return value: the depth of the actor, in pixels
 */

gfloat
mash_model_get_model_depth (ClutterActor *actor){
    MashModel *model = MASH_MODEL (actor);
    MashModelPrivate *priv = model->priv;
    ClutterVertex min_vertex, max_vertex;
    if (priv->data){
        mash_data_get_extents (priv->data, &min_vertex, &max_vertex);
        return max_vertex.z - min_vertex.z;
    }
    return 0.0;
}

/**
 * mash_model_get_model_z_min:
 * @self: A #MashModel instance
 *
 * Return value: the z_min of the actor, in pixels
 */

gfloat
mash_model_get_model_z_min (ClutterActor *actor){
    MashModel *model = MASH_MODEL (actor);
    MashModelPrivate *priv = model->priv;
    ClutterVertex min_vertex, max_vertex;
    if (priv->data){
        mash_data_get_extents (priv->data, &min_vertex, &max_vertex);
        gfloat f = min_vertex.z;
        fprintf(stderr,"F: %f\n", f);
        return f;
    }
    return 0.0;
}

/**
 * mash_model_get_model_z_max:
 * @self: A #MashModel instance
 *
 * Return value: the z_max of the actor, in pixels
 */

gfloat
mash_model_get_model_z_max (ClutterActor *actor){
    MashModel *model = MASH_MODEL (actor);
    MashModelPrivate *priv = model->priv;
    ClutterVertex min_vertex, max_vertex;
    if (priv->data){
        mash_data_get_extents (priv->data, &min_vertex, &max_vertex);
        return max_vertex.z;
    }
    return 0.0;
}
