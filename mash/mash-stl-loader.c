/*
 * Mash - A library for displaying STL models in a Clutter scene
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <string.h>
#include <cogl/cogl.h>
#include <clutter/clutter.h>

#include "mash-stl-loader.h"
#include "rstl/rstl.h"

static void mash_stl_loader_finalize (GObject *object);
static gboolean mash_stl_loader_load (MashDataLoader *data_loader,
                                      MashDataFlags flags,
                                      const gchar *filename,
                                      GError **error);
static void mash_stl_loader_get_data (MashDataLoader *data_loader,
                                      MashDataLoaderData *loader_data);

G_DEFINE_TYPE (MashStlLoader, mash_stl_loader, MASH_TYPE_DATA_LOADER);

#define MASH_STL_LOADER_GET_PRIVATE(obj)                      \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_STL_LOADER,  \
                                MashStlLoaderPrivate))

static const struct
{
  const gchar *name;
  int size;
}
mash_stl_loader_properties[] =
{
  /* These should be sorted in descending order of size so that it
     never ends doing an unaligned write */
  { "x0", sizeof (gfloat) },
  { "y0", sizeof (gfloat) },
  { "z0", sizeof (gfloat) },
  { "x1", sizeof (gfloat) },
  { "y1", sizeof (gfloat) },
  { "z1", sizeof (gfloat) },
  { "x2", sizeof (gfloat) },
  { "y2", sizeof (gfloat) },
  { "z2", sizeof (gfloat) },
  //{ "nx", sizeof (gfloat) },
  //{ "ny", sizeof (gfloat) },
  //{ "nz", sizeof (gfloat) },
  //{ "s", sizeof (gfloat) },
  //{ "t", sizeof (gfloat) },
  //{ "red", sizeof (guint8) },
  //{ "green", sizeof (guint8) },
  //{ "blue", sizeof (guint8) }
};

#define MASH_STL_LOADER_VERTEX_PROPS    511
#define MASH_STL_LOADER_NORMAL_PROPS    (7 << 3)
#define MASH_STL_LOADER_TEX_COORD_PROPS (3 << 6)
#define MASH_STL_LOADER_COLOR_PROPS     (7 << 8)

typedef struct _MashStlLoaderData MashStlLoaderData;

struct _MashStlLoaderData{
  p_stl stl;
  GError *error;
  /* Data for the current vertex */
  guint8 current_vertex[G_N_ELEMENTS (mash_stl_loader_properties) * 4];
  /* Map from property number to byte offset in the current_vertex array */
  gint prop_map[G_N_ELEMENTS (mash_stl_loader_properties)];
  /* Number of bytes for a vertex */
  guint n_vertex_bytes;
  gint available_props, got_props;
  guint first_vertex, last_vertex;
  GByteArray *vertices;
  GArray *faces;
  CoglIndicesType indices_type;
  MashDataFlags flags;

  /* Bounding cuboid of the data */
  ClutterVertex min_vertex, max_vertex;

  /* Range of indices used */
  guint min_index, max_index;

  CoglContext *ctx;
  CoglPrimitive   *prim;    
};

struct _MashStlLoaderPrivate{
  CoglHandle vertices_vbo;
  CoglHandle indices;
  guint min_index, max_index;
  guint n_triangles;

  /* Bounding cuboid of the data */
  ClutterVertex min_vertex, max_vertex;
  CoglPrimitive   *prim;    
};

static void
mash_stl_loader_class_init (MashStlLoaderClass *klass){
  GObjectClass *gobject_class = (GObjectClass *) klass;
  MashDataLoaderClass *data_loader_class = (MashDataLoaderClass *) klass;

  gobject_class->finalize = mash_stl_loader_finalize;

  data_loader_class->load = mash_stl_loader_load;
  data_loader_class->get_data = mash_stl_loader_get_data;

  g_type_class_add_private (klass, sizeof (MashStlLoaderPrivate));
}

static void
mash_stl_loader_init (MashStlLoader *self){
  self->priv = MASH_STL_LOADER_GET_PRIVATE (self);
}

static void
mash_stl_loader_free_vbos (MashStlLoader *self){
  MashStlLoaderPrivate *priv = self->priv;

  if (priv->vertices_vbo)
    {
      cogl_handle_unref (priv->vertices_vbo);
      priv->vertices_vbo = NULL;
    }

  if (priv->indices)
    {
      cogl_handle_unref (priv->indices);
      priv->indices = NULL;
    }
}

static void
mash_stl_loader_finalize (GObject *object){
  MashStlLoader *self = (MashStlLoader *) object;

  mash_stl_loader_free_vbos (self);

  G_OBJECT_CLASS (mash_stl_loader_parent_class)->finalize (object);
}

static void
mash_stl_loader_error_cb (const char *message, gpointer data){
  MashStlLoaderData *load_stl_loader = data;

  if (load_stl_loader->error == NULL)
    g_set_error_literal (&load_stl_loader->error, MASH_DATA_ERROR,
                         MASH_DATA_ERROR_UNKNOWN, message);
}

static void
mash_stl_loader_check_unknown_error (MashStlLoaderData *data){
  if (data->error == NULL)
    g_set_error_literal (&data->error,
                         MASH_DATA_ERROR,
                         MASH_DATA_ERROR_UNKNOWN,
                         "Unknown error loading STL file");
}

static int
mash_stl_loader_vertex_read_cb (p_stl_argument argument){
  long prop_num;
  MashStlLoaderData *data;
  gint32 length, index;
  double value;

  stl_get_argument_user_data (argument, (void **) &data, &prop_num);
  stl_get_argument_property (argument, NULL, &length, &index);


  if (length != 1 || index != 0){
      g_set_error (&data->error, MASH_DATA_ERROR,
                   MASH_DATA_ERROR_INVALID,
                   "List type property not supported for vertex element '%s'",
                   mash_stl_loader_properties[prop_num].name);

      return 0;
    }

    
  value = stl_get_argument_value (argument);
  //fprintf(stderr, "mash_stl_loader_vertex_read_cb(), value = %f\n", value);

  *(gfloat *) (data->current_vertex + data->prop_map[prop_num]) = value;

  data->got_props |= 1 << prop_num;

  /* If we've got enough properties for a complete vertex then add it
     to the array */
  if (data->got_props == data->available_props){
      int i, j;
        //fprintf(stderr, "data->got_props\n");

      g_byte_array_append (data->vertices, data->current_vertex,
                           data->n_vertex_bytes);
      data->got_props = 0;

      /* Update the bounding box for the data */
      for (i = 0; i < 3; i++){
            for(j=0; j<3; j++){
                gfloat *min = &data->min_vertex.x + j;
                gfloat *max = &data->max_vertex.x + j;
                gfloat value = *(gfloat *) (data->current_vertex + data->prop_map[i*3+j]);
                //fprintf(stderr, "value = %f, min_x = %f, max_x = %f\n", value, *min, *max);
                if (value < *min)
                    *min = value;
                if (value > *max)
                    *max = value;
            }
        }
    }

  return 1;
}



static void
mash_stl_loader_add_face_index (MashStlLoaderData *data,
                                guint index)
{
  if (index > data->max_index)
    data->max_index = index;
  if (index < data->min_index)
    data->min_index = index;

  switch (data->indices_type)
    {
    case COGL_INDICES_TYPE_UNSIGNED_BYTE:
      {
        guint8 value = index;
        g_array_append_val (data->faces, value);
      }
      break;
    case COGL_INDICES_TYPE_UNSIGNED_SHORT:
      {
        guint16 value = index;
        g_array_append_val (data->faces, value);
      }
      break;
    case COGL_INDICES_TYPE_UNSIGNED_INT:
      {
        guint32 value = index;
        g_array_append_val (data->faces, value);
      }
      break;
    }
}

static gboolean
mash_stl_loader_get_indices_type (MashStlLoaderData *data,
                                  GError **error)
{
  p_stl_element elem = NULL;

  /* Look for the 'vertices' element */
  while ((elem = stl_get_next_element (data->stl, elem)))
    {
      const char *name;
      gint32 n_instances;

      if (stl_get_element_info (elem, &name, &n_instances))
        {
          if (!strcmp (name, "facet"))
            {
              if (n_instances <= 0x100)
                {
                  data->indices_type = COGL_INDICES_TYPE_UNSIGNED_BYTE;
                  data->faces = g_array_new (FALSE, FALSE, sizeof (guint8));
                }
              else if (n_instances <= 0x10000)
                {
                  data->indices_type = COGL_INDICES_TYPE_UNSIGNED_SHORT;
                  data->faces = g_array_new (FALSE, FALSE, sizeof (guint16));
                }
              else if (cogl_has_feature(COGL_FEATURE_UNSIGNED_INT_INDICES))
                {
                  data->indices_type = COGL_INDICES_TYPE_UNSIGNED_INT;
                  data->faces = g_array_new (FALSE, FALSE, sizeof (guint32));
                }
              else
                {
                  g_set_error (error, MASH_DATA_ERROR,
                               MASH_DATA_ERROR_UNSUPPORTED,
                               "The STL file requires unsigned int indices "
                               "but this is not supported by your GL driver");
                  return FALSE;
                }

              return TRUE;
            }
        }
      else
        {
          g_set_error (error, MASH_DATA_ERROR,
                       MASH_DATA_ERROR_UNKNOWN,
                       "Error getting element info");
          return FALSE;
        }
    }

  g_set_error (error, MASH_DATA_ERROR,
               MASH_DATA_ERROR_MISSING_PROPERTY,
               "STL file is missing the vertex element");

  return FALSE;
}

static gboolean
mash_stl_loader_load (MashDataLoader *data_loader,
                      MashDataFlags flags,
                      const gchar *filename,
                      GError **error)
{
  MashStlLoader *self = MASH_STL_LOADER (data_loader);
  MashStlLoaderPrivate *priv;
  MashStlLoaderData data;
  gchar *display_name;
  gboolean ret;

    int i,j;

  priv = self->priv;

  data.error = NULL;
  data.n_vertex_bytes = 0;
  data.available_props = 0;
  data.got_props = 0;
  data.vertices = g_byte_array_new ();
  data.faces = NULL;
  data.min_vertex.x = G_MAXFLOAT;
  data.min_vertex.y = G_MAXFLOAT;
  data.min_vertex.z = G_MAXFLOAT;
  data.max_vertex.x = -G_MAXFLOAT;
  data.max_vertex.y = -G_MAXFLOAT;
  data.max_vertex.z = -G_MAXFLOAT;
  data.min_index = G_MAXUINT;
  data.max_index = 0;
  data.flags = flags;

  display_name = g_filename_display_name (filename);



  if ((data.stl = stl_open (filename,
                            mash_stl_loader_error_cb,
                            &data)) == NULL)
    mash_stl_loader_check_unknown_error (&data);
  else{
      if (!stl_read_header (data.stl))
        mash_stl_loader_check_unknown_error (&data);
      else{
          int i;
          fprintf(stderr, "mash_stl_loader_load\n");

          for (i = 0; i < G_N_ELEMENTS (mash_stl_loader_properties); i++)
            if (stl_set_read_cb (data.stl, "facet",
                                 mash_stl_loader_properties[i].name,
                                 mash_stl_loader_vertex_read_cb,
                                 &data, i))
              {
                data.prop_map[i] = data.n_vertex_bytes;
                data.n_vertex_bytes += mash_stl_loader_properties[i].size;
                data.available_props |= 1 << i;
              }
          fprintf(stderr, "data.available_props = %d\n", data.available_props);
          /* Align the size of a vertex to 32 bits */
          data.n_vertex_bytes = (data.n_vertex_bytes + 3) & ~(guint) 3;

          if ((data.available_props & MASH_STL_LOADER_VERTEX_PROPS)
              != MASH_STL_LOADER_VERTEX_PROPS)
            g_set_error (&data.error, MASH_DATA_ERROR,
                         MASH_DATA_ERROR_MISSING_PROPERTY,
                         "STL file %s is missing the vertex properties",
                        display_name);
          if (mash_stl_loader_get_indices_type (&data, &data.error)
                   && !stl_read (data.stl))
            mash_stl_loader_check_unknown_error (&data);
        }
        stl_close (data.stl);
    }

  if (data.error)
    {
      g_propagate_error (error, data.error);
      ret = FALSE;
    }
  /*else if (data.faces->len < 3)
    {
      g_set_error (error, MASH_DATA_ERROR,
                   MASH_DATA_ERROR_INVALID,
                   "No faces found in %s",
                   display_name);
      ret = FALSE;
    }*/
  else
    {
      /* Make sure all of the indices are valid */
      if (data.max_index >= data.vertices->len / data.n_vertex_bytes)
        {
          g_set_error (error, MASH_DATA_ERROR,
                       MASH_DATA_ERROR_INVALID,
                       "Index out of range in %s",
                       display_name);
          ret = FALSE;
        }
      else
        {


    CoglVertexP3C4 triangle[] =
    {
      { 0,   300, 0 , 0xFF, 0x00, 0x00, 0xFF},
      { 150, 0,   0 , 0xFF, 0x00, 0x00, 0xFF},
      { 300, 300, 0 , 0xFF, 0x00, 0x00, 0xFF}
    };



CoglVertexP3T2 vertices[] =
{
/* Front face */
{ /* pos = */ -1.0f, -1.0f, 1.0f, /* tex coords = */ 0.0f, 1.0f},
{ /* pos = */ 1.0f, -1.0f, 1.0f, /* tex coords = */ 1.0f, 1.0f},
{ /* pos = */ 1.0f, 1.0f, 1.0f, /* tex coords = */ 1.0f, 0.0f},
{ /* pos = */ -1.0f, 1.0f, 1.0f, /* tex coords = */ 0.0f, 0.0f},
/* Back face */
{ /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
{ /* pos = */ -1.0f, 1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
{ /* pos = */ 1.0f, 1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
{ /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f},
/* Top face */
{ /* pos = */ -1.0f, 1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
{ /* pos = */ -1.0f, 1.0f, 1.0f, /* tex coords = */ 0.0f, 0.0f},
{ /* pos = */ 1.0f, 1.0f, 1.0f, /* tex coords = */ 1.0f, 0.0f},
{ /* pos = */ 1.0f, 1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
/* Bottom face */
{ /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
{ /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
{ /* pos = */ 1.0f, -1.0f, 1.0f, /* tex coords = */ 0.0f, 0.0f},
{ /* pos = */ -1.0f, -1.0f, 1.0f, /* tex coords = */ 1.0f, 0.0f},
/* Right face */
{ /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
{ /* pos = */ 1.0f, 1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
{ /* pos = */ 1.0f, 1.0f, 1.0f, /* tex coords = */ 0.0f, 1.0f},
{ /* pos = */ 1.0f, -1.0f, 1.0f, /* tex coords = */ 0.0f, 0.0f},
/* Left face */
{ /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f},
{ /* pos = */ -1.0f, -1.0f, 1.0f, /* tex coords = */ 1.0f, 0.0f},
{ /* pos = */ -1.0f, 1.0f, 1.0f, /* tex coords = */ 1.0f, 1.0f},
{ /* pos = */ -1.0f, 1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f}
};


CoglVertexP3C4 vertices2[] =
{
/* Front face */
{ /* pos = */ -1.0f, -1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ 1.0f, -1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ 1.0f, 1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ -1.0f, 1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF},
/* Back face */
{ /* pos = */ -1.0f, -1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ -1.0f, 1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ 1.0f, 1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ 1.0f, -1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF},
/* Top face */
{ /* pos = */ -1.0f, 1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ -1.0f, 1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ 1.0f, 1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ 1.0f, 1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF },
/* Bottom face */
{ /* pos = */ -1.0f, -1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF },
{ /* pos = */ 1.0f, -1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF },
{ /* pos = */ 1.0f, -1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF },
{ /* pos = */ -1.0f, -1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF },
/* Right face */
{ /* pos = */ 1.0f, -1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF },
{ /* pos = */ 1.0f, 1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF },
{ /* pos = */ 1.0f, 1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ 1.0f, -1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF },
/* Left face */
{ /* pos = */ -1.0f, -1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF},
{ /* pos = */ -1.0f, -1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF },
{ /* pos = */ -1.0f, 1.0f, 1.0f, 0x00, 0xFF, 0x00, 0xFF },
{ /* pos = */ -1.0f, 1.0f, -1.0f, 0x00, 0xFF, 0x00, 0xFF }
};


        ClutterBackend      *be         = clutter_get_default_backend ();
        CoglContext         *ctx        = (CoglContext*) clutter_backend_get_cogl_context (be);


            CoglVertexP3C4 test[data.vertices->len/data.n_vertex_bytes*3];
            fprintf(stderr, "Making %d vertices\n", data.vertices->len/data.n_vertex_bytes*3); 
            fprintf(stderr, "Each vertex has %d bytes\n", data.n_vertex_bytes); 
            for(i=0; i<data.vertices->len/data.n_vertex_bytes; i++){     
                for(j=0; j<3; j++){            
                    gfloat *x = (gfloat *) (data.vertices->data + data.n_vertex_bytes*i + data.prop_map[j*3]);
                    gfloat *y = (gfloat *) (data.vertices->data + data.n_vertex_bytes*i + data.prop_map[j*3+1]);
                    gfloat *z = (gfloat *) (data.vertices->data + data.n_vertex_bytes*i + data.prop_map[j*3+2]);
                    fprintf(stderr, "Adding v %d: (%f, %f, %f)\n",i*3+j, *x, *y, *z);
                    CoglVertexP3C4 v = {*x, *y, *z, 0x00, 0x00, 0xFF, 0xFF};
                    test[i*3+j] = v;
                }
            }

            

            //priv->prim = cogl_primitive_new_p3t2 (ctx, COGL_VERTICES_MODE_TRIANGLES, G_N_ELEMENTS (vertices), vertices);
            //priv->prim = cogl_primitive_new_p3c4 (ctx, COGL_VERTICES_MODE_TRIANGLES, G_N_ELEMENTS (vertices2), vertices2);  
            priv->prim = cogl_primitive_new_p3c4 (ctx, COGL_VERTICES_MODE_TRIANGLES, data.vertices->len/data.n_vertex_bytes*3, test);
            fprintf(stderr, "Made a primitive with %d indices\n", cogl_primitive_get_n_vertices (priv->prim));


            fprintf(stderr, "Bounding box is (%f, %f, %f), (%f, %f, %f)\n", 
                data.min_vertex.x,
                data.min_vertex.y,
                data.min_vertex.z, 
                data.max_vertex.x, 
                data.max_vertex.y, 
                data.max_vertex.z);

          /* Get rid of the old VBOs (if any) */
          mash_stl_loader_free_vbos (self);

          /* Create a new VBO for the vertices */
          priv->vertices_vbo = cogl_vertex_buffer_new (data.vertices->len
                                                       / data.n_vertex_bytes);

            
     

          /* Upload the data */
          if ((data.available_props & MASH_STL_LOADER_VERTEX_PROPS)
              == MASH_STL_LOADER_VERTEX_PROPS){
            cogl_vertex_buffer_add (priv->vertices_vbo,
                                    "gl_Vertex",
                                    3, COGL_ATTRIBUTE_TYPE_FLOAT,
                                    FALSE, data.n_vertex_bytes,
                                    data.vertices->data
                                    + data.prop_map[0]);
            fprintf(stderr, "uploading gl_vertex\n");
          }

          if ((data.available_props & MASH_STL_LOADER_NORMAL_PROPS) == MASH_STL_LOADER_NORMAL_PROPS){
            cogl_vertex_buffer_add (priv->vertices_vbo,
                                    "gl_Normal",
                                    3, COGL_ATTRIBUTE_TYPE_FLOAT,
                                    FALSE, data.n_vertex_bytes,
                                    data.vertices->data
                                    + data.prop_map[3]);
            fprintf(stderr, "uploading gl_normal\n");
           }

          if ((data.available_props & MASH_STL_LOADER_TEX_COORD_PROPS) == MASH_STL_LOADER_TEX_COORD_PROPS){
            cogl_vertex_buffer_add (priv->vertices_vbo,
                                    "gl_MultiTexCoord0",
                                    2, COGL_ATTRIBUTE_TYPE_FLOAT,
                                    FALSE, data.n_vertex_bytes,
                                    data.vertices->data
                                    + data.prop_map[6]);
            fprintf(stderr, "uploading gl_mutitexcorrds\n");
         }

          if ((data.available_props & MASH_STL_LOADER_COLOR_PROPS) == MASH_STL_LOADER_COLOR_PROPS){
            cogl_vertex_buffer_add (priv->vertices_vbo,
                                    "gl_Color",
                                    3, COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE,
                                    FALSE, data.n_vertex_bytes,
                                    data.vertices->data
                                    + data.prop_map[8]);
            fprintf(stderr, "uploading gl_color\n");
          }

          cogl_vertex_buffer_submit (priv->vertices_vbo);


          /* Create a VBO for the indices */
          priv->indices
            = cogl_vertex_buffer_indices_new (data.indices_type,
                                              data.faces->data,
                                              data.faces->len);
          
          priv->min_index = data.min_index;
          priv->max_index = data.max_index;
          priv->n_triangles = data.faces->len / 3;

          priv->min_vertex = data.min_vertex;
          priv->max_vertex = data.max_vertex;

          ret = TRUE;
        }
    }

  g_free (display_name);
  g_byte_array_free (data.vertices, TRUE);
  if (data.faces)
    g_array_free (data.faces, TRUE);

  return ret;
}

static void
mash_stl_loader_get_data (MashDataLoader *data_loader, MashDataLoaderData *loader_data)
{
  MashStlLoader *self = MASH_STL_LOADER (data_loader);
  MashStlLoaderPrivate *priv = self->priv;

  loader_data->vertices_vbo = cogl_handle_ref (priv->vertices_vbo);
  loader_data->indices = cogl_handle_ref (priv->indices);

  loader_data->min_index = priv->min_index;
  loader_data->max_index = priv->max_index;
  loader_data->n_triangles = priv->n_triangles;

  loader_data->min_vertex = priv->min_vertex;
  loader_data->max_vertex = priv->max_vertex;

    loader_data->prim = priv->prim;
    fprintf(stderr, "loader_data->prim = priv->prim\n");
}
