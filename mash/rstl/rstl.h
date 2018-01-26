#ifndef STL_H
#define STL_H
/* ----------------------------------------------------------------------
 * RStl library, read/write STL files
 * Diego Nehab, Princeton University
 * http://www.cs.princeton.edu/~diego/professional/rstl
 *
 * This library is distributed under the MIT License. See notice
 * at the end of this file.
 * ---------------------------------------------------------------------- */

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RSTL_VERSION   "RStl 1.01"
#define RSTL_COPYRIGHT "Copyright (C) 2003-2005 Diego Nehab"
#define RSTL_AUTHORS   "Diego Nehab"

/* ----------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */
/* structures are opaque */
typedef struct t_stl_ *p_stl;
typedef struct t_stl_element_ *p_stl_element;
typedef struct t_stl_property_ *p_stl_property;
typedef struct t_stl_argument_ *p_stl_argument;

/* stl format mode type */
typedef enum e_stl_storage_mode_ {
    STL_BIG_ENDIAN,
    STL_LITTLE_ENDIAN,
    STL_ASCII,
    STL_DEFAULT      /* has to be the last in enum */
} e_stl_storage_mode; /* order matches stl_storage_mode_list */

/* stl data type */
typedef enum e_stl_type {
    STL_INT8, STL_UINT8, STL_INT16, STL_UINT16,
    STL_INT32, STL_UIN32, STL_FLOAT32, STL_FLOAT64,
    STL_CHAR, STL_UCHAR, STL_SHORT, STL_USHORT,
    STL_INT, STL_UINT, STL_FLOAT, STL_DOUBLE, STL_WORD, STL_LINE,
    STL_LIST    /* has to be the last in enum */
} e_stl_type;   /* order matches stl_type_list */

/* ----------------------------------------------------------------------
 * Property reading callback prototype
 *
 * message: error message
 * ---------------------------------------------------------------------- */
typedef void (*p_stl_error_cb)(const char *message, gpointer data);

/* ----------------------------------------------------------------------
 * Opens a stl file for reading (fails if file is not a stl file)
 *
 * error_cb: error callback function
 * name: file name
 *
 * Returns 1 if successful, 0 otherwise
 * ---------------------------------------------------------------------- */
p_stl stl_open(const char *name, p_stl_error_cb error_cb, gpointer cb_data);

/* ----------------------------------------------------------------------
 * Reads and parses the header of a stl file returned by stl_open
 *
 * stl: handle returned by stl_open
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_read_header(p_stl stl);

/* ----------------------------------------------------------------------
 * Property reading callback prototype
 *
 * argument: parameters for property being processed when callback is called
 *
 * Returns 1 if should continue processing file, 0 if should abort.
 * ---------------------------------------------------------------------- */
typedef int (*p_stl_read_cb)(p_stl_argument argument);

/* ----------------------------------------------------------------------
 * Sets up callbacks for property reading after header was parsed
 *
 * stl: handle returned by stl_open
 * element_name: element where property is
 * property_name: property to associate element with
 * read_cb: function to be called for each property value
 * pdata/idata: user data that will be passed to callback
 *
 * Returns 0 if no element or no property in element, returns the
 * number of element instances otherwise.
 * ---------------------------------------------------------------------- */
long stl_set_read_cb(p_stl stl, const char *element_name,
        const char *property_name, p_stl_read_cb read_cb,
        void *pdata, long idata);

/* ----------------------------------------------------------------------
 * Returns information about the element originating a callback
 *
 * argument: handle to argument
 * element: receives a the element handle (if non-null)
 * instance_index: receives the index of the current element instance
 *     (if non-null)
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_get_argument_element(p_stl_argument argument,
        p_stl_element *element, gint32 *instance_index);

/* ----------------------------------------------------------------------
 * Returns information about the property originating a callback
 *
 * argument: handle to argument
 * property: receives the property handle (if non-null)
 * length: receives the number of values in this property (if non-null)
 * value_index: receives the index of current property value (if non-null)
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_get_argument_property(p_stl_argument argument,
        p_stl_property *property, gint32 *length, gint32 *value_index);

/* ----------------------------------------------------------------------
 * Returns user data associated with callback
 *
 * pdata: receives a copy of user custom data pointer (if non-null)
 * idata: receives a copy of user custom data integer (if non-null)
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_get_argument_user_data(p_stl_argument argument, void **pdata,
        long *idata);

/* ----------------------------------------------------------------------
 * Returns the value associated with a callback
 *
 * argument: handle to argument
 *
 * Returns the current data item
 * ---------------------------------------------------------------------- */
double stl_get_argument_value(p_stl_argument argument);

/* ----------------------------------------------------------------------
 * Reads all elements and properties calling the callbacks defined with
 * calls to stl_set_read_cb
 *
 * stl: handle returned by stl_open
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_read(p_stl stl);

/* ----------------------------------------------------------------------
 * Iterates over all elements by returning the next element.
 * Call with NULL to return handle to first element.
 *
 * stl: handle returned by stl_open
 * last: handle of last element returned (NULL for first element)
 *
 * Returns element if successfull or NULL if no more elements
 * ---------------------------------------------------------------------- */
p_stl_element stl_get_next_element(p_stl stl, p_stl_element last);

/* ----------------------------------------------------------------------
 * Iterates over all comments by returning the next comment.
 * Call with NULL to return pointer to first comment.
 *
 * stl: handle returned by stl_open
 * last: pointer to last comment returned (NULL for first comment)
 *
 * Returns comment if successfull or NULL if no more comments
 * ---------------------------------------------------------------------- */
const char *stl_get_next_comment(p_stl stl, const char *last);

/* ----------------------------------------------------------------------
 * Iterates over all obj_infos by returning the next obj_info.
 * Call with NULL to return pointer to first obj_info.
 *
 * stl: handle returned by stl_open
 * last: pointer to last obj_info returned (NULL for first obj_info)
 *
 * Returns obj_info if successfull or NULL if no more obj_infos
 * ---------------------------------------------------------------------- */
const char *stl_get_next_obj_info(p_stl stl, const char *last);

/* ----------------------------------------------------------------------
 * Returns information about an element
 *
 * element: element of interest
 * name: receives a pointer to internal copy of element name (if non-null)
 * ninstances: receives the number of instances of this element (if non-null)
 *
 * Returns 1 if successfull or 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_get_element_info(p_stl_element element, const char** name,
        gint32 *ninstances);

/* ----------------------------------------------------------------------
 * Iterates over all properties by returning the next property.
 * Call with NULL to return handle to first property.
 *
 * element: handle of element with the properties of interest
 * last: handle of last property returned (NULL for first property)
 *
 * Returns element if successfull or NULL if no more properties
 * ---------------------------------------------------------------------- */
p_stl_property stl_get_next_property(p_stl_element element,
        p_stl_property last);

/* ----------------------------------------------------------------------
 * Returns information about a property
 *
 * property: handle to property of interest
 * name: receives a pointer to internal copy of property name (if non-null)
 * type: receives the property type (if non-null)
 * length_type: for list properties, receives the scalar type of
 *     the length field (if non-null)
 * value_type: for list properties, receives the scalar type of the value
 *     fields  (if non-null)
 *
 * Returns 1 if successfull or 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_get_property_info(p_stl_property property, const char** name,
        e_stl_type *type, e_stl_type *length_type, e_stl_type *value_type);

/* ----------------------------------------------------------------------
 * Creates new stl file
 *
 * name: file name
 * storage_mode: file format mode
 *
 * Returns handle to stl file if successfull, NULL otherwise
 * ---------------------------------------------------------------------- */
p_stl stl_create(const char *name, e_stl_storage_mode storage_mode,
                 p_stl_error_cb error_cb, gpointer cb_data);

/* ----------------------------------------------------------------------
 * Adds a new element to the stl file created by stl_create
 *
 * stl: handle returned by stl_create
 * name: name of new element
 * ninstances: number of element of this time in file
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_add_element(p_stl stl, const char *name, gint32 ninstances);

/* ----------------------------------------------------------------------
 * Adds a new property to the last element added by stl_add_element
 *
 * stl: handle returned by stl_create
 * name: name of new property
 * type: property type
 * length_type: scalar type of length field of a list property
 * value_type: scalar type of value fields of a list property
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_add_property(p_stl stl, const char *name, e_stl_type type,
        e_stl_type length_type, e_stl_type value_type);

/* ----------------------------------------------------------------------
 * Adds a new list property to the last element added by stl_add_element
 *
 * stl: handle returned by stl_create
 * name: name of new property
 * length_type: scalar type of length field of a list property
 * value_type: scalar type of value fields of a list property
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_add_list_property(p_stl stl, const char *name,
        e_stl_type length_type, e_stl_type value_type);

/* ----------------------------------------------------------------------
 * Adds a new property to the last element added by stl_add_element
 *
 * stl: handle returned by stl_create
 * name: name of new property
 * type: property type
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_add_scalar_property(p_stl stl, const char *name, e_stl_type type);

/* ----------------------------------------------------------------------
 * Adds a new comment item
 *
 * stl: handle returned by stl_create
 * comment: pointer to string with comment text
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_add_comment(p_stl stl, const char *comment);

/* ----------------------------------------------------------------------
 * Adds a new obj_info item
 *
 * stl: handle returned by stl_create
 * comment: pointer to string with obj_info data
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_add_obj_info(p_stl stl, const char *obj_info);

/* ----------------------------------------------------------------------
 * Writes the stl file header after all element and properties have been
 * defined by calls to stl_add_element and stl_add_property
 *
 * stl: handle returned by stl_create
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_write_header(p_stl stl);

/* ----------------------------------------------------------------------
 * Writes one property value, in the order they should be written to the
 * file. For each element type, write all elements of that type in order.
 * For each element, write all its properties in order. For scalar
 * properties, just write the value. For list properties, write the length
 * and then each of the values.
 *
 * stl: handle returned by stl_create
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_write(p_stl stl, double value);

/* ----------------------------------------------------------------------
 * Closes a stl file handle. Releases all memory used by handle
 *
 * stl: handle to be closed.
 *
 * Returns 1 if successfull, 0 otherwise
 * ---------------------------------------------------------------------- */
int stl_close(p_stl stl);

#ifdef __cplusplus
}
#endif

#endif /* RSTL_H */

/* ----------------------------------------------------------------------
 * Copyright (C) 2003-2005 Diego Nehab. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ---------------------------------------------------------------------- */
