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

#if !defined(__MASH_H_INSIDE__) && !defined(MASH_COMPILATION)
#error "Only <mash/mash.h> can be included directly."
#endif

#ifndef __MASH_STL_LOADER_H__
#define __MASH_STL_LOADER_H__

#include "mash-data-loader.h"

G_BEGIN_DECLS

#define MASH_TYPE_STL_LOADER                          \
  (mash_stl_loader_get_type())
#define MASH_STL_LOADER(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                 \
                               MASH_TYPE_STL_LOADER,  \
                               MashStlLoader))
#define MASH_STL_LOADER_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                  \
                            MASH_TYPE_STL_LOADER,     \
                            MashStlLoaderClass))
#define MASH_IS_STL_LOADER(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                 \
                               MASH_TYPE_STL_LOADER))
#define MASH_IS_STL_LOADER_CLASS(klass)                 \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                    \
                            MASH_TYPE_STL_LOADER))
#define MASH_STL_LOADER_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                  \
                              MASH_TYPE_STL_LOADER,   \
                              MashStlLoaderClass))

typedef struct _MashStlLoader        MashStlLoader;
typedef struct _MashStlLoaderClass   MashStlLoaderClass;
typedef struct _MashStlLoaderPrivate MashStlLoaderPrivate;

struct _MashStlLoaderClass
{
  /*< private >*/
  MashDataLoaderClass parent_class;
};

struct _MashStlLoader
{
  /*< private >*/
  GObject parent;

  MashStlLoaderPrivate *priv;
};

GType mash_stl_loader_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __MASH_STL_LOADER_H__ */
