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
 * SECTION:mash-light-set
 * @short_description: A group of #MashLight<!-- -->s which can be
 *   used to affect the appearance of a #MashModel.
 *
 * #MashLightSet is a toplevel object that contains a list of
 * #MashLight<!-- -->s. The set which a light belongs to is separate
 * from its parent actor. For a light to be useful it needs to be
 * added to both a light set and a parent container.
 *
 * The #MashLightSet can only be used with actors that are
 * specifically designed to support it. #MashModel is one such
 * actor. It can be told to use a light set with
 * mash_model_set_light_set().
 *
 * The light set implements the Blinn-Phong lighting model which is
 * the standard model used in fixed function version of OpenGL and
 * Direct3D. The lighting calculations are performed per-vertex and
 * then interpolated across the surface of the primitives.
 *
 * Lights are positioned as normal actors by adding #MashLight<!--
 * -->s them to a container and moving them. The lights do not have to
 * be in any particular position relative to the models in the
 * hierarchy of actors, although it wouldn't make much sense if they
 * were on different stages. The lights are subclasses of
 * #ClutterActor so they can be positioned and animated using the
 * usual Clutter animation framework.
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

#define CLUTTER_ENABLE_EXPERIMENTAL_API
#define COGL_ENABLE_EXPERIMENTAL_API

#include <clutter/clutter.h>

#include "mash-light-set.h"
#include "mash-light.h"

static void mash_light_set_dispose (GObject *object);
static void mash_light_set_finalize (GObject *object);

static gboolean mash_light_set_repaint_func (gpointer data);

static float mash_light_set_get_shininess_wrapper (CoglPipeline *pipeline);

G_DEFINE_TYPE (MashLightSet, mash_light_set, G_TYPE_OBJECT);

#define MASH_LIGHT_SET_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_LIGHT_SET, \
                                MashLightSetPrivate))

typedef void (* MaterialColorGetFunc) (CoglPipeline *pipeline,
                                       CoglColor *color);

typedef float (* MaterialFloatGetFunc) (CoglPipeline *pipeline);

typedef enum
{
  MATERIAL_PROP_TYPE_COLOR,
  MATERIAL_PROP_TYPE_FLOAT
} MaterialPropType;

static struct
{
  MaterialPropType type;
  const char *uniform_name;
  void *get_func;
}
mash_light_set_material_properties[] =
  {
    {
      MATERIAL_PROP_TYPE_COLOR,
      "mash_material.emission",
      cogl_pipeline_get_emission
    },
    {
      MATERIAL_PROP_TYPE_COLOR,
      "mash_material.ambient",
      cogl_pipeline_get_ambient
    },
    {
      MATERIAL_PROP_TYPE_COLOR,
      "mash_material.diffuse",
      cogl_pipeline_get_diffuse
    },
    {
      MATERIAL_PROP_TYPE_COLOR,
      "mash_material.specular",
      cogl_pipeline_get_specular
    },
    {
      MATERIAL_PROP_TYPE_FLOAT,
      "mash_material.shininess",
      mash_light_set_get_shininess_wrapper
    }
  };

struct _MashLightSetPrivate
{
  CoglPipeline *pipeline;

  /* This is the layer indices that the pipeline contained the last
   * time the program was generated. If these change then we need to
   * regenerate the program */
  GArray *layer_indices;

  GSList *lights;

  guint repaint_func_id;

  int normal_matrix_uniform;

  int material_uniforms[G_N_ELEMENTS (mash_light_set_material_properties)];

  /* Set to TRUE at the beginning of every paint so that we know we
     need to update the uniforms on the program before painting any
     actor */
  gboolean uniforms_dirty;

  gboolean pipeline_created;
};

static void
mash_light_set_class_init (MashLightSetClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = mash_light_set_dispose;
  gobject_class->finalize = mash_light_set_finalize;

  g_type_class_add_private (klass, sizeof (MashLightSetPrivate));
}

static void
mash_light_set_init (MashLightSet *self)
{
  MashLightSetPrivate *priv;

  priv = self->priv = MASH_LIGHT_SET_GET_PRIVATE (self);

  priv->repaint_func_id =
    clutter_threads_add_repaint_func (mash_light_set_repaint_func, self, NULL);

  priv->layer_indices = g_array_new (FALSE, FALSE, sizeof (int));
  priv->pipeline_created = FALSE;
}

static void
mash_light_set_dispose (GObject *object)
{
  MashLightSet *self = (MashLightSet *) object;
  MashLightSetPrivate *priv = self->priv;

  g_slist_foreach (priv->lights, (GFunc) g_object_unref, NULL);
  g_slist_free (priv->lights);
  priv->lights = NULL;

  if (priv->repaint_func_id)
    {
      clutter_threads_remove_repaint_func (priv->repaint_func_id);
      priv->repaint_func_id = 0;
    }

  G_OBJECT_CLASS (mash_light_set_parent_class)->dispose (object);
}

static void
mash_light_set_finalize (GObject *object)
{
  MashLightSet *self = (MashLightSet *) object;
  MashLightSetPrivate *priv = self->priv;

  if (priv->pipeline)
    cogl_handle_unref (priv->pipeline);

  g_array_free (priv->layer_indices, TRUE);

  G_OBJECT_CLASS (mash_light_set_parent_class)->finalize (object);
}

/**
 * mash_light_set_new:
 *
 * Constructs a new #MashLightSet.
 *
 * Return value: a new #MashLightSet.
 */
MashLightSet *
mash_light_set_new (void)
{
  return g_object_new (MASH_TYPE_LIGHT_SET, NULL);
}

static void
add_layer_indices (MashLightSet *light_set,
                   GString *string)
{
  MashLightSetPrivate *priv = light_set->priv;
  int i;

  for (i = 0; i < priv->layer_indices->len; i++)
    {
      int layer_index = g_array_index (priv->layer_indices, int, i);

      g_string_append_printf (string,
                              "  cogl_tex_coord%i_out = cogl_tex_coord%i_in;\n",
                              layer_index, layer_index);
    }
}

void
mash_light_set_get_pipeline (MashLightSet *light_set, CoglPipeline* pipeline){
 
  MashLightSetPrivate *priv = light_set->priv;
  GString *uniform_source, *main_source;
  char *uniform_char, *main_char;
  CoglHandle shader;
  char *info_log;
  GSList *l;
  int i;

  uniform_source = g_string_new (NULL);
  main_source = g_string_new (NULL);

  /* Append the shader boiler plate */
  g_string_append (uniform_source,
                   "\n"
                   "uniform mat3 mash_normal_matrix;\n"
                   "\n"
                   "struct MashMaterialParameters {\n"
                   "  vec4 emission;\n"
                   "  vec4 ambient;\n"
                   "  vec4 diffuse;\n"
                   "  vec4 specular;\n"
                   "  float shininess;\n"
                   "};\n"
                   "\n"
                   "uniform MashMaterialParameters mash_material;\n");
  g_string_append (main_source,
                   /* Start with just the light emitted by the
                      object itself. The lights should add to this
                      color */
                   "  cogl_color_out = mash_material.emission;\n"
                   /* Calculate a transformed and normalized vertex normal */
                   "  vec3 normal = normalize (mash_normal_matrix * cogl_normal_in);\n"
                   /* Calculate the vertex position in eye coordinates */
                   "  vec4 homogenous_eye_coord = cogl_modelview_matrix * cogl_position_in;\n"
                   "  vec3 eye_coord = homogenous_eye_coord.xyz / homogenous_eye_coord.w;\n");


  /* Give all of the lights in the scene a chance to modify the
     shader source */
  for (l = priv->lights; l; l = l->next)
    mash_light_generate_shader (l->data,
                                uniform_source,
                                main_source);


  /* Perform the standard vertex transformation and copy the
     texture coordinates. */
  g_string_append (main_source,
                   "  cogl_position_out = cogl_modelview_projection_matrix * cogl_position_in;\n");
  
  add_layer_indices (light_set, main_source);

  uniform_char = g_string_free (uniform_source, FALSE);
  main_char = g_string_free (main_source, FALSE);

  CoglSnippet *snippet_vertex; 
  snippet_vertex = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
                  uniform_char, main_char);


  /* Add it to the pipeline */
  cogl_pipeline_add_snippet (pipeline, snippet_vertex);
  /* The pipeline keeps a reference to the snippet
  so we don't need to */
  cogl_object_unref (snippet_vertex);

  priv->normal_matrix_uniform = cogl_pipeline_get_uniform_location (pipeline, "mash_normal_matrix");

  for (i = 0; i < G_N_ELEMENTS (mash_light_set_material_properties); i++){
      const char *uniform_name = mash_light_set_material_properties[i].uniform_name;
      priv->material_uniforms[i] = cogl_pipeline_get_uniform_location (pipeline, uniform_name);
  }
}

void
mash_light_set_get_snippets (MashLightSet *light_set){
 
  MashLightSetPrivate *priv = light_set->priv;
  GString *uniform_source, *main_source;
  char *uniform_char, *main_char;
  CoglHandle shader;
  char *info_log;
  GSList *l;
  int i;

  // TODO: Let each light generate it's own snippet. 

  CoglSnippet *main =  cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX_GLOBALS, NULL, NULL);

  cogl_snippet_set_declarations(main, 
                   "uniform mat3 mash_normal_matrix;\n"
                   "\n"
                   "struct MashMaterialParameters {\n"
                   "  vec4 emission;\n"
                   "  vec4 ambient;\n"
                   "  vec4 diffuse;\n"
                   "  vec4 specular;\n"
                   "  float shininess;\n"
                   "};\n"
                   "\n"
                   "uniform MashMaterialParameters mash_material;\n");
}


static void
mash_light_set_dirty_program (MashLightSet *light_set)
{
  MashLightSetPrivate *priv = light_set->priv;

  /* If we've added or removed a light then we need to regenerate the
     shader */
  priv->pipeline_created = FALSE;
  // TODO: Keep a list of pipelines, so each can be recreated. 
  // Right now, pipelines can not be set to NULL, since they 
  // already have color and possibly a fragment shader. 
}

typedef struct
{
  MashLightSet *light_set;
  CoglPipeline *pipeline;
  gboolean is_matching;
  int index_num;
} UpdateLayerIndicesData;

static CoglBool
update_layer_indices_cb (CoglPipeline *pipeline,
                         int layer_index,
                         void *user_data){
  UpdateLayerIndicesData *data = user_data;
  MashLightSetPrivate *priv = data->light_set->priv;

  if (data->is_matching){
      if (data->index_num >= priv->layer_indices->len ||
          g_array_index (priv->layer_indices,
                         int,
                         data->index_num) != layer_index)
        {
          g_array_set_size (priv->layer_indices, data->index_num);
          data->is_matching = FALSE;
        }
    }

  if (!data->is_matching)
    g_array_append_val (priv->layer_indices, layer_index);
  data->index_num++;
  return TRUE;
}

gboolean
mash_light_set_update_layer_indices (MashLightSet *light_set,
                      CoglPipeline *pipeline)
{
  MashLightSetPrivate *priv = light_set->priv;
  UpdateLayerIndicesData data;

  data.light_set = light_set;
  data.pipeline = pipeline;
  data.is_matching = TRUE;
  data.index_num = 0;

  cogl_pipeline_foreach_layer (data.pipeline,
                               update_layer_indices_cb,
                               &data);
  /* If the layer indices have changed then we need to regenerate the
   * program */
  if (!data.is_matching ||
      data.index_num != priv->layer_indices->len)
    {
      g_array_set_size (priv->layer_indices, data.index_num);
      mash_light_set_dirty_program (light_set);

      return TRUE;
    }
  return FALSE;
}

/**
 * mash_light_set_begin_paint:
 * @light_set: A #MashLightSet instance
 * @material: The material that will be used to paint
 *
 * This function should only be needed by custom actors that wish to
 * use the lighting model of Mash. The function should be called every
 * time the actor is painted. The @material parameter is used to
 * specify the lighting material properties. The material is not
 * otherwise read or modified. The material properties that are used
 * are: the emission color, the ambient color, the diffuse color, the
 * specular color and the shininess.
 *
 * The return value is a CoglProgram that should be used to paint the
 * actor. The actor should attach this to its material using
 * cogl_material_set_user_program().
 *
 * #MashModel<!-- -->s are already designed to use this function when
 * a light set is passed to mash_model_set_light_set().
 *
 * Return value: a CoglProgram to use for rendering.
 *
 * Since: 0.2
 */
void
mash_light_set_begin_paint (MashLightSet *light_set,
                            CoglPipeline *pipeline){  
  MashLightSetPrivate *priv = light_set->priv;
  int i;

  priv->pipeline = pipeline;

  if(!cogl_is_pipeline(pipeline))
    return;

  //mash_light_set_update_layer_indices (light_set, pipeline);

  if (priv->uniforms_dirty){
      GSList *l;
      /* Give all of the lights a chance to update the uniforms before we
         paint the first actor using the light set */
      for (l = priv->lights; l; l = l->next)
        mash_light_update_uniforms (l->data, pipeline);

      //priv->uniforms_dirty = FALSE;
    }

  /* Calculate the normal matrix from the modelview matrix */
  if (priv->normal_matrix_uniform != -1){
      CoglMatrix modelview_matrix;
      CoglMatrix inverse_matrix;
      float transpose_matrix[3 * 3];

      /* Invert the matrix */
      cogl_get_modelview_matrix (&modelview_matrix);
      cogl_matrix_get_inverse (&modelview_matrix, &inverse_matrix);

      /* Transpose it while converting it to 3x3 */
      transpose_matrix[0] = inverse_matrix.xx;
      transpose_matrix[1] = inverse_matrix.xy;
      transpose_matrix[2] = inverse_matrix.xz;

      transpose_matrix[3] = inverse_matrix.yx;
      transpose_matrix[4] = inverse_matrix.yy;
      transpose_matrix[5] = inverse_matrix.yz;

      transpose_matrix[6] = inverse_matrix.zx;
      transpose_matrix[7] = inverse_matrix.zy;
      transpose_matrix[8] = inverse_matrix.zz;

      cogl_pipeline_set_uniform_matrix (pipeline,
                                       priv->normal_matrix_uniform,
                                       3, /* dimensions */
                                       1, /* count */
                                       FALSE, /* transpose */
                                       transpose_matrix);
    }

  for (i = 0; i < G_N_ELEMENTS (mash_light_set_material_properties); i++)
    if (priv->material_uniforms[i] != -1)
      switch (mash_light_set_material_properties[i].type)
        {
        case MATERIAL_PROP_TYPE_COLOR:
          {
            CoglColor color;
            MaterialColorGetFunc get_func =
              mash_light_set_material_properties[i].get_func;
            float vec[4];

            get_func (pipeline, &color);

            vec[0] = cogl_color_get_red_float (&color);
            vec[1] = cogl_color_get_green_float (&color);
            vec[2] = cogl_color_get_blue_float (&color);
            vec[3] = cogl_color_get_alpha_float (&color);

            cogl_pipeline_set_uniform_float (pipeline,
                                            priv->material_uniforms[i],
                                            4, /* n_components */
                                            1, /* count */
                                            vec);
          }
          break;

        case MATERIAL_PROP_TYPE_FLOAT:
          {
            MaterialFloatGetFunc get_func =
              mash_light_set_material_properties[i].get_func;
            float value;

            value = get_func (pipeline);

            cogl_pipeline_set_uniform_1f (pipeline,
                                         priv->material_uniforms[i],
                                         value);
          }
          break;
        }
}

static gboolean
mash_light_set_repaint_func (gpointer data)
{
  MashLightSet *light_set = MASH_LIGHT_SET (data);
  MashLightSetPrivate *priv = light_set->priv;

  /* Mark that we need to update the uniforms the next time an actor
     is painted. We can't just update the uniforms immediately because
     the repaint function is called before the allocation is run so
     the lights may not have the correct position yet */

  priv->uniforms_dirty = TRUE;

  return TRUE;
}

/**
 * mash_light_set_add_light:
 * @light_set: A #MashLightSet instance
 * @light: A #MashLight
 *
 * This adds a light to the set. Lights need to be added to the light
 * set as well as to a container somewhere in the Clutter actor
 * hierarchy in order to be useful.
 */
void
mash_light_set_add_light (MashLightSet *light_set,
                          MashLight *light)
{
  MashLightSetPrivate *priv;

  g_return_if_fail (MASH_IS_LIGHT_SET (light_set));
  g_return_if_fail (MASH_IS_LIGHT (light));

  priv = light_set->priv;

  priv->lights = g_slist_prepend (priv->lights, g_object_ref_sink (light));

  mash_light_set_dirty_program (light_set);
}

/**
 * mash_light_set_remove_light:
 * @light_set: A #MashLightSet instance
 * @light: A #MashLight
 *
 * Removes a light from the set.
 */
void
mash_light_set_remove_light (MashLightSet *light_set,
                             MashLight *light)
{
  MashLightSetPrivate *priv;
  GSList *l, *prev = NULL;

  g_return_if_fail (MASH_IS_LIGHT_SET (light_set));
  g_return_if_fail (MASH_IS_LIGHT (light));

  priv = light_set->priv;

  for (l = priv->lights; l; l = l->next)
    if (l->data == light)
      {
        g_object_unref (light);
        if (prev)
          prev->next = l->next;
        else
          priv->lights = l->next;
        g_slist_free_1 (l);

        mash_light_set_dirty_program (light_set);

        break;
      }
    else
      prev = l;
}

static float
mash_light_set_get_shininess_wrapper (CoglPipeline *pipeline)
{
  float shininess;

  /* The shininess is used in the GLSL code for all of the lights as a
     power to raise to. However the glsl pow function has undefined
     results when the power is zero. On nvidia this seems to mess up
     the calculations so much that even multiplying the result of the
     pow call by zero ends up in a non-zero value. Therefore we just
     want to avoid passing zero as the shininess value. I don't think
     it would make much sense to pass a value less than one
     anyway. Unfortunately the default value for the shininess on a
     CoglMaterial is 0 so it's quite likely that an application would
     hit this if they are trying not to use specular lighting */

  shininess = cogl_pipeline_get_shininess  (pipeline);

  return MAX (0.0001, shininess);
}
