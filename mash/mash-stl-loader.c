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

#define CLUTTER_ENABLE_EXPERIMENTAL_API
#define COGL_ENABLE_EXPERIMENTAL_API

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
  { "nx", sizeof (gfloat) },
  { "ny", sizeof (gfloat) },
  { "nz", sizeof (gfloat) },
  { "x0", sizeof (gfloat) },
  { "y0", sizeof (gfloat) },
  { "z0", sizeof (gfloat) },
  { "x1", sizeof (gfloat) },
  { "y1", sizeof (gfloat) },
  { "z1", sizeof (gfloat) },
  { "x2", sizeof (gfloat) },
  { "y2", sizeof (gfloat) },
  { "z2", sizeof (gfloat) },
};

#define MASH_STL_LOADER_VERTEX_PROPS    511
//#define MASH_STL_LOADER_NORMAL_PROPS    (7 << 3)
//#define MASH_STL_LOADER_TEX_COORD_PROPS (3 << 6)
//#define MASH_STL_LOADER_COLOR_PROPS     (7 << 8)

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
      if(   (*(gfloat *) (data->current_vertex + data->prop_map[0]) == 0.0) && 
            (*(gfloat *) (data->current_vertex + data->prop_map[1]) == 0.0) &&
            (*(gfloat *) (data->current_vertex + data->prop_map[2]) == 0.0)){
         // No normal data in the STL. Calculate it
         //fprintf(stderr, "Missing normal data\n");
        
        float u1[3], u2[3], cross[3];
        float v1[3], v2[3], v3[3];

        cogl_vector3_init (v1, 
            *(gfloat *) (data->current_vertex + data->prop_map[3]), 
            *(gfloat *) (data->current_vertex + data->prop_map[4]), 
            *(gfloat *) (data->current_vertex + data->prop_map[5]));
        cogl_vector3_init (v2, 
            *(gfloat *) (data->current_vertex + data->prop_map[6]), 
            *(gfloat *) (data->current_vertex + data->prop_map[7]), 
            *(gfloat *) (data->current_vertex + data->prop_map[8]));
        cogl_vector3_init (v3, 
            *(gfloat *) (data->current_vertex + data->prop_map[9]), 
            *(gfloat *) (data->current_vertex + data->prop_map[10]), 
            *(gfloat *) (data->current_vertex + data->prop_map[11]));

        //fprintf(stderr, "V1: %f, %f, %f\n", v1[0], v1[1], v1[2]);
        //fprintf(stderr, "V2: %f, %f, %f\n", v2[0], v2[1], v2[2]);
        //fprintf(stderr, "V3: %f, %f, %f\n", v3[0], v3[1], v3[2]);

        cogl_vector3_init_zero(u1);
        cogl_vector3_init_zero(u2);

        cogl_vector3_subtract(u1, v1, v2);
        cogl_vector3_subtract(u2, v2, v3);

        //fprintf(stderr, "U1: %f, %f, %f\n", u1[0], u1[1], u1[2]);
        //fprintf(stderr, "U2: %f, %f, %f\n", u2[0], u2[1], u2[2]);

        cogl_vector3_subtract(u2, v2, v3);
        cogl_vector3_cross_product(cross, u1, u2);
        cogl_vector3_normalize(cross);
        //fprintf(stderr, "New normal: %f, %f, %f\n", cross[0], cross[1], cross[2]);
        *(gfloat *) (data->current_vertex + data->prop_map[0]) = cross[0];
        *(gfloat *) (data->current_vertex + data->prop_map[1]) = cross[1];
        *(gfloat *) (data->current_vertex + data->prop_map[2]) = cross[2];
      }

      for(i=0; i<3; i++){
          // Append first vertex data
          g_byte_array_append (data->vertices, data->current_vertex + data->prop_map[3+i*3],
                               mash_stl_loader_properties[3].size*3);
          // Append the normal data. This is the same for all vertices
          g_byte_array_append (data->vertices, data->current_vertex + data->prop_map[0],
                               mash_stl_loader_properties[0].size*3);
      }
      data->got_props = 0;
      /* Update the bounding box for the data 
        TODO: This should only have to be done once, not for every vertex.*/
      for (i = 0; i < 3; i++){
            for(j=0; j<3; j++){
                gfloat *min = &data->min_vertex.x + j;
                gfloat *max = &data->max_vertex.x + j;
                gfloat value = *(gfloat *) (data->current_vertex + data->prop_map[3+i*3+j]);
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


static gboolean
mash_stl_loader_get_indices_type (MashStlLoaderData *data,
                                  GError **error){
    p_stl_element elem = NULL;

    ClutterBackend      *be         = clutter_get_default_backend ();
    CoglContext         *ctx        = (CoglContext*) clutter_backend_get_cogl_context (be);

    /* Look for the 'vertices' element */
    while ((elem = stl_get_next_element (data->stl, elem))){
        const char *name;
        gint32 n_instances;
        if (stl_get_element_info (elem, &name, &n_instances)){
            if (!strcmp (name, "facet")){
                if (n_instances <= 0x100){
                    data->indices_type = COGL_INDICES_TYPE_UNSIGNED_BYTE;
                    data->faces = g_array_new (FALSE, FALSE, sizeof (guint8));
                }
                else if (n_instances <= 0x10000){
                    data->indices_type = COGL_INDICES_TYPE_UNSIGNED_SHORT;
                    data->faces = g_array_new (FALSE, FALSE, sizeof (guint16));
                }
                else if (cogl_has_feature(ctx, COGL_FEATURE_UNSIGNED_INT_INDICES)){
                    data->indices_type = COGL_INDICES_TYPE_UNSIGNED_INT;
                    data->faces = g_array_new (FALSE, FALSE, sizeof (guint32));
                }
                else{
                    g_set_error (error, MASH_DATA_ERROR,
                               MASH_DATA_ERROR_UNSUPPORTED,
                               "The STL file requires unsigned int indices "
                               "but this is not supported by your GL driver");
                    return FALSE;
                }
                return TRUE;
            }
        }
        else{
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
                      GError **error){
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
        //fprintf(stderr, "STL open OK\n");
        if (!stl_read_header (data.stl))
            mash_stl_loader_check_unknown_error (&data);
        else{
            //fprintf(stderr, "STL header read OK\n");
            int i;
            for (i = 0; i < G_N_ELEMENTS (mash_stl_loader_properties); i++){
                if (stl_set_read_cb (data.stl, "facet",
                                 mash_stl_loader_properties[i].name,
                                 mash_stl_loader_vertex_read_cb,
                                 &data, i)){
                    data.prop_map[i] = data.n_vertex_bytes;
                    data.n_vertex_bytes += mash_stl_loader_properties[i].size;
                    data.available_props |= 1 << i;
                }
            }
            //fprintf(stderr, "STL aligning\n");
            /* Align the size of a vertex to 32 bits */
            data.n_vertex_bytes = (data.n_vertex_bytes + 3) & ~(guint) 3;
            if ((data.available_props & MASH_STL_LOADER_VERTEX_PROPS)
              != MASH_STL_LOADER_VERTEX_PROPS)
                g_set_error (&data.error, MASH_DATA_ERROR,
                         MASH_DATA_ERROR_MISSING_PROPERTY,
                         "STL file %s is missing the vertex properties",
                        display_name);
            //fprintf(stderr, "STL getting indices\n");
            if (mash_stl_loader_get_indices_type (&data, &data.error)
                   && !stl_read (data.stl))
                mash_stl_loader_check_unknown_error (&data);
        }
        stl_close (data.stl);
    }
  if (data.error){
      g_propagate_error (error, data.error);
      ret = FALSE;
    }
  else{
      /* Make sure all of the indices are valid */
      if (data.max_index >= data.vertices->len / data.n_vertex_bytes){
          g_set_error (error, MASH_DATA_ERROR,
                       MASH_DATA_ERROR_INVALID,
                       "Index out of range in %s",
                       display_name);
          ret = FALSE;
        }
      else{
            ClutterBackend      *be         = clutter_get_default_backend ();
            CoglContext         *ctx        = (CoglContext*) clutter_backend_get_cogl_context (be);

            data.n_vertex_bytes = sizeof (float) *6;

            int nr_vertices = data.vertices->len/(sizeof (float)*6);

            CoglAttributeBuffer *buffer = cogl_attribute_buffer_new (ctx, data.vertices->len, data.vertices->data);
            CoglAttribute *attributes[2];
            attributes[0] = cogl_attribute_new (buffer,
                                    "cogl_position_in",
                                    /* Stride */
                                    sizeof (float) * 6,
                                    /* Offset */
                                    0,
                                    /* n_components */
                                    3,
                                    COGL_ATTRIBUTE_TYPE_FLOAT);
            attributes[1] = cogl_attribute_new (buffer,
                                    "cogl_normal_in",
                                    /* Stride */
                                    sizeof (float) * 6,
                                    /* Offset */
                                    sizeof (float) * 3,
                                    /* n_components */
                                    3,
                                    COGL_ATTRIBUTE_TYPE_FLOAT);
            priv->prim = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                      nr_vertices, /* n_vertices */
                                      attributes,
                                      2 /* n_attributes */);
            cogl_object_unref (attributes[0]);
            cogl_object_unref (attributes[1]);
            cogl_object_unref (buffer);

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
mash_stl_loader_get_data (MashDataLoader *data_loader, MashDataLoaderData *loader_data){
  MashStlLoader *self = MASH_STL_LOADER (data_loader);
  MashStlLoaderPrivate *priv = self->priv;

  loader_data->min_vertex = priv->min_vertex;
  loader_data->max_vertex = priv->max_vertex;

  loader_data->prim = priv->prim;
}
