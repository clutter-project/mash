/*
 * Mash - A library for displaying PLY models in a Clutter scene
 * Copyright (C) 2010  Intel Corporation
 * Copyright (C) 2010  Luca Bruno <lethalman88@gmail.com>
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
 * SECTION:mash-data
 * @short_description: An object that contains the data for a model.
 *
 * #MashData is an object that can represent the data contained
 * in a 3D model file. The data is internally converted to a
 * Cogl vertex buffer so that it can be rendered efficiently.
 *
 * The #MashData object is usually associated with a
 * #MashModel so that it can be animated as a regular actor. The
 * data is separated from the actor in this way to make it easy to
 * share data with multiple actors without having to keep two copies
 * of the data.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <string.h>
#include <cogl/cogl.h>
#include <clutter/clutter.h>

#include <GLES2/gl2.h>
#include <cogl/cogl.h>
#include <cogl/cogl-gles2.h>
#include <glib.h>

#include "mash-data.h"
#include "mash-data-loader.h"
#include "mash-data-loaders.h"

#define COGL_ENABLE_EXPERIMENTAL_API 1

static void mash_data_finalize (GObject *object);

G_DEFINE_TYPE (MashData, mash_data, G_TYPE_OBJECT);

#define MASH_DATA_GET_PRIVATE(obj)                      \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_DATA,  \
                                MashDataPrivate))

struct _MashDataPrivate{
  MashDataLoaderData loaded_data;
};

static void
mash_data_class_init (MashDataClass *klass){
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = mash_data_finalize;

  g_type_class_add_private (klass, sizeof (MashDataPrivate));
}

static void
mash_data_init (MashData *self){
  self->priv = MASH_DATA_GET_PRIVATE (self);
}

static void
mash_data_free_vbos (MashData *self){
  MashDataPrivate *priv = self->priv;

  if (priv->loaded_data.vertices_vbo){
      cogl_handle_unref (priv->loaded_data.vertices_vbo);
      priv->loaded_data.vertices_vbo = NULL;
    }

  if (priv->loaded_data.indices)
    {
      cogl_handle_unref (priv->loaded_data.indices);
      priv->loaded_data.indices = NULL;
    }
}

static void
mash_data_finalize (GObject *object){
  MashData *self = (MashData *) object;

  mash_data_free_vbos (self);

  G_OBJECT_CLASS (mash_data_parent_class)->finalize (object);
}

/**
 * mash_data_new:
 *
 * Constructs a new #MashData instance. The object initially has
 * no data so nothing will be drawn when mash_data_render() is
 * called. To load data into the object, call mash_data_load().
 *
 * Return value: a new #MashData.
 */
MashData *
mash_data_new (void){
  MashData *self = g_object_new (MASH_TYPE_DATA, NULL);

  return self;
}

/**
 * mash_data_load:
 * @self: The #MashData instance
 * @flags: Flags used to specify load-time modifications to the data
 * @filename: The name of a file to load
 * @error: Return location for an error or %NULL
 *
 * Loads the data from the file called @filename into @self. The
 * model can then be rendered using mash_data_render(). If
 * there is an error loading the file it will return %FALSE and @error
 * will be set to a GError instance.
 *
 * Return value: %TRUE if the load succeeded or %FALSE otherwise.
 */
gboolean
mash_data_load (MashData *self,
                MashDataFlags flags,
                const gchar *filename,
                GError **error){
  MashDataPrivate *priv;
  MashDataLoader *loader;
  gchar *display_name;
  gboolean ret;

  g_return_val_if_fail (MASH_IS_DATA (self), FALSE);

  priv = self->priv;

  loader = NULL;
  display_name = g_filename_display_name (filename);

  if (g_str_has_suffix (filename, ".ply"))
    loader = g_object_new (MASH_TYPE_PLY_LOADER, NULL);
  else if (g_str_has_suffix (filename, ".stl"))
    loader = g_object_new (MASH_TYPE_STL_LOADER, NULL);

  if (loader != NULL){
      if (!mash_data_loader_load (loader, flags, filename, error))
        ret = FALSE;
      else{
          /* Get rid of the old VBOs (if any) */
          mash_data_free_vbos (self);

          mash_data_loader_get_data (loader, &priv->loaded_data);
          ret = TRUE;
        }
    }
  else{
      /* Unknown file format */
      g_set_error (error, MASH_DATA_ERROR,
                   MASH_DATA_ERROR_UNKNOWN_FORMAT,
                   "Unknown format for file %s",
                   display_name);
      ret = FALSE;
    }

  g_free (display_name);
  if (loader)
    g_object_unref (loader);

  return ret;
}


static const char vertex_shader[] =
"attribute vec3 position;\n"
"attribute vec3 normal;\n"
"\n"
"uniform mat4 ModelViewProjectionMatrix;\n"
"uniform mat4 NormalMatrix;\n"
"uniform vec4 LightSourcePosition;\n"
"uniform vec4 MaterialColor;\n"
"\n"
"varying vec4 Color;\n"
"\n"
"void main(void)\n"
"{\n"
" // Transform the normal to eye coordinates\n"
" vec3 N = normalize(vec3(NormalMatrix * vec4(normal, 1.0)));\n"
"\n"
" // The LightSourcePosition is actually its direction for directional light\n"
" vec3 L = normalize(LightSourcePosition.xyz);\n"
"\n"
" // Multiply the diffuse value by the vertex color (which is fixed in this case)\n"
" // to get the actual color that we will use to draw this vertex with\n"
" float diffuse = max(dot(N, L), 0.0);\n"
" Color = diffuse * MaterialColor;\n"
"\n"
" // Transform the position to clip coordinates\n"
" gl_Position = ModelViewProjectionMatrix * vec4(position, 1.0);\n"
"}";
static const char fragment_shader[] =
"precision mediump float;\n"
"varying vec4 Color;\n"
"\n"
"void main(void)\n"
"{\n"
" gl_FragColor = Color;\n"
"}";



/**
 * mash_data_render:
 * @self: A #MashData instance
 *
 * Renders the data contained in the model to the Clutter
 * scene. The current Cogl source material will be used to affect the
 * appearance of the model. This function is not usually called
 * directly but instead the #MashData instance is added to a
 * #MashModel and this function will be automatically called by
 * the paint method of the model.
 */
void
mash_data_render (MashData *self){
  MashDataPrivate *priv;
  g_return_if_fail (MASH_IS_DATA (self));
  priv = self->priv;
    CoglError *error = NULL;

#define COGL_PIPELINE_FILTER_NEAREST 0x2600
#define COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE 0x812F

  /* Silently fail if we didn't load any data */
    if (priv->loaded_data.prim != NULL){
        ClutterBackend  *be  = clutter_get_default_backend ();
        CoglContext     *ctx = (CoglContext*) clutter_backend_get_cogl_context (be);
        CoglFramebuffer *fb  = cogl_get_draw_framebuffer();
        CoglPipeline    *pl  = cogl_pipeline_new (ctx);  

        CoglTexture *texture = cogl_texture_2d_new_from_file (ctx, "crate.jpg", COGL_PIXEL_FORMAT_ANY, &error);
        if (!texture)
            g_error ("Failed to load texture: %s", error->message);
        cogl_pipeline_set_layer_texture (pl, 0, texture);
        CoglIndices *indices = cogl_get_rectangle_indices (ctx, 6 /* n_rectangles */);
        cogl_primitive_set_indices (priv->loaded_data.prim, indices, 6 * 6);

        GLuint v, f, program;
        const char *p;
        char msg[512];
         /* Compile the vertex shader */
        /*p = vertex_shader;
        v = glCreateShader (GL_VERTEX_SHADER);
        glShaderSource (v, 1, &p, NULL);
        glCompileShader (v);
        glGetShaderInfoLog (v, sizeof msg, NULL, msg);
        printf ("vertex shader info: %s\n", msg);*/
        /* Compile the fragment shader */
        /*p = fragment_shader;
        f = glCreateShader (GL_FRAGMENT_SHADER);
        glShaderSource (f, 1, &p, NULL);
        glCompileShader (f);
        glGetShaderInfoLog (f, sizeof msg, NULL, msg);
        printf ("fragment shader info: %s\n", msg);
        */

        CoglTexture2D *tex;
        static const uint8_t tex_data[] = { 0x00, 0x44, 0x88, 0xcc };
        tex = cogl_texture_2d_new_from_data (ctx, 2, 2, COGL_PIXEL_FORMAT_A_8, COGL_PIXEL_FORMAT_A_8, 2, tex_data, &error);
        cogl_pipeline_set_layer_filters (pl,
            0, /* layer */
            COGL_PIPELINE_FILTER_NEAREST,
            COGL_PIPELINE_FILTER_NEAREST);
        cogl_pipeline_set_layer_wrap_mode (pl,
            0, /* layer */
            COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
        /* This is the layer combine used by cogl-pango */
        cogl_pipeline_set_layer_combine (pl,
            0, /* layer */
            "RGBA = MODULATE (PREVIOUS, TEXTURE[A])",
            NULL);
        cogl_pipeline_set_layer_texture (pl,
            0, /* layer */
            tex);

        uint8_t replacement_data[1] = { 0xff };

        cogl_texture_set_region (tex,
            0, 0, /* src_x/y */
            1, 1, /* dst_x/y */
            1, 1, /* dst_width / dst_height */
            1, 1, /* width / height */
            COGL_PIXEL_FORMAT_A_8,
            1, /* rowstride */
            replacement_data);

        cogl_primitive_draw (priv->loaded_data.prim, fb, pl);  
    }
    else if(priv->loaded_data.vertices_vbo != NULL && priv->loaded_data.indices != NULL){
        cogl_vertex_buffer_draw_elements (priv->loaded_data.vertices_vbo,
                                    COGL_VERTICES_MODE_TRIANGLES,
                                    priv->loaded_data.indices,
                                    priv->loaded_data.min_index,
                                    priv->loaded_data.max_index,
                                    0, priv->loaded_data.n_triangles * 3);
    }
}

/**
 * mash_data_get_extents:
 * @self: A #MashData instance
 * @min_vertex: A location to return the minimum vertex
 * @max_vertex: A location to return the maximum vertex
 *
 * Gets the bounding cuboid of the vertices in @self. The cuboid is
 * represented by two vertices representing the minimum and maximum
 * extents. The x, y and z components of @min_vertex will contain the
 * minimum x, y and z values of all the vertices and @max_vertex will
 * contain the maximum. The extents of the model are cached so it is
 * cheap to call this function.
 */
void
mash_data_get_extents (MashData *self,
                       ClutterVertex *min_vertex,
                       ClutterVertex *max_vertex)
{
  MashDataPrivate *priv = self->priv;

  *min_vertex = priv->loaded_data.min_vertex;
  *max_vertex = priv->loaded_data.max_vertex;
}

GQuark
mash_data_error_quark (void)
{
  return g_quark_from_static_string ("mash-data-error-quark");
}
