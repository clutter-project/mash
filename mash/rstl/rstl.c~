/* ----------------------------------------------------------------------
 * RStl library, read/write STL files
 * Diego Nehab, Princeton University
 * http://www.cs.princeton.edu/~diego/professional/rstl
 *
 * This library is distributed under the MIT License. See notice
 * at the end of this file.
 * ---------------------------------------------------------------------- */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>

#include "rstl.h"

/* ----------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */
#define WORDSIZE 256
#define LINESIZE 1024
#define BUFFERSIZE (8*1024)

typedef enum e_stl_io_mode_ {
    STL_READ,
    STL_WRITE
} e_stl_io_mode;

static const char *const stl_storage_mode_list[] = {
    "binary_big_endian", "binary_little_endian", "ascii", NULL
};     /* order matches e_stl_storage_mode enum */

static const char *const stl_type_list[] = {
    "int8", "uint8", "int16", "uint16",
    "int32", "uint32", "float32", "float64",
    "char", "uchar", "short", "ushort",
    "int", "uint", "float", "double",
    "list", NULL
};     /* order matches e_stl_type enum */

/* ----------------------------------------------------------------------
 * Property reading callback argument
 *
 * element: name of element being processed
 * property: name of property being processed
 * nelements: number of elements of this kind in file
 * instance_index: index current element of this kind being processed
 * length: number of values in current list (or 1 for scalars)
 * value_index: index of current value int this list (or 0 for scalars)
 * value: value of property
 * pdata/idata: user data defined with stl_set_cb
 *
 * Returns handle to stl file if succesful, NULL otherwise.
 * ---------------------------------------------------------------------- */
typedef struct t_stl_argument_ {
    p_stl_element element;
    gint32 instance_index;
    p_stl_property property;
    gint32 length, value_index;
    double value;
    void *pdata;
    long idata;
} t_stl_argument;

/* ----------------------------------------------------------------------
 * Property information
 *
 * name: name of this property
 * type: type of this property (list or type of scalar value)
 * length_type, value_type: type of list property count and values
 * read_cb: function to be called when this property is called
 *
 * Returns 1 if should continue processing file, 0 if should abort.
 * ---------------------------------------------------------------------- */
typedef struct t_stl_property_ {
    char name[WORDSIZE];
    e_stl_type type, value_type, length_type;
    p_stl_read_cb read_cb;
    void *pdata;
    long idata;
} t_stl_property;

/* ----------------------------------------------------------------------
 * Element information
 *
 * name: name of this property
 * ninstances: number of elements of this type in file
 * property: property descriptions for this element
 * nproperty: number of properties in this element
 *
 * Returns 1 if should continue processing file, 0 if should abort.
 * ---------------------------------------------------------------------- */
typedef struct t_stl_element_ {
    char name[WORDSIZE];
    gint32 ninstances;
    p_stl_property property;
    gint32 nproperties;
} t_stl_element;

/* ----------------------------------------------------------------------
 * Input/output driver
 *
 * Depending on file mode, different functions are used to read/write
 * property fields. The drivers make it transparent to read/write in ascii,
 * big endian or little endian cases.
 * ---------------------------------------------------------------------- */
typedef int (*p_stl_ihandler)(p_stl stl, double *value);
typedef int (*p_stl_ichunk)(p_stl stl, void *anydata, size_t size);
typedef struct t_stl_idriver_ {
    p_stl_ihandler ihandler[18];
    p_stl_ichunk ichunk;
    const char *name;
} t_stl_idriver;
typedef t_stl_idriver *p_stl_idriver;

typedef int (*p_stl_ohandler)(p_stl stl, double value);
typedef int (*p_stl_ochunk)(p_stl stl, void *anydata, size_t size);
typedef struct t_stl_odriver_ {
    p_stl_ohandler ohandler[16];
    p_stl_ochunk ochunk;
    const char *name;
} t_stl_odriver;
typedef t_stl_odriver *p_stl_odriver;

/* ----------------------------------------------------------------------
 * Stl file handle.
 *
 * io_mode: read or write (from e_stl_io_mode)
 * storage_mode: mode of file associated with handle (from e_stl_storage_mode)
 * element: elements description for this file
 * nelement: number of different elements in file
 * comment: comments for this file
 * ncomments: number of comments in file
 * obj_info: obj_info items for this file
 * nobj_infos: number of obj_info items in file
 * fp: file pointer associated with stl file
 * c: last character read from stl file
 * buffer: last word/chunck of data read from stl file
 * buffer_first, buffer_last: interval of untouched good data in buffer
 * buffer_token: start of parsed token (line or word) in buffer
 * idriver, odriver: input driver used to get property fields from file
 * argument: storage space for callback arguments
 * welement, wproperty: element/property type being written
 * winstance_index: index of instance of current element being written
 * wvalue_index: index of list property value being written
 * wlength: number of values in list property being written
 * error_cb: callback for error messages
 * ---------------------------------------------------------------------- */
typedef struct t_stl_ {
    e_stl_io_mode io_mode;
    e_stl_storage_mode storage_mode;
    p_stl_element element;
    gint32 nelements;
    char *comment;
    gint32 ncomments;
    char *obj_info;
    gint32 nobj_infos;
    FILE *fp;
    int c;
    char buffer[BUFFERSIZE];
    size_t buffer_first, buffer_token, buffer_last;
    p_stl_idriver idriver;
    p_stl_odriver odriver;
    t_stl_argument argument;
    gint32 welement, wproperty;
    gint32 winstance_index, wvalue_index, wlength;
    p_stl_error_cb error_cb;
    gpointer cb_data;
} t_stl;

/* ----------------------------------------------------------------------
 * I/O functions and drivers
 * ---------------------------------------------------------------------- */
static t_stl_idriver stl_idriver_ascii;
static t_stl_idriver stl_idriver_binary;
static t_stl_idriver stl_idriver_binary_reverse;
static t_stl_odriver stl_odriver_ascii;
static t_stl_odriver stl_odriver_binary;
static t_stl_odriver stl_odriver_binary_reverse;

static int stl_read_word(p_stl stl);
static int stl_check_word(p_stl stl);
static int stl_read_line(p_stl stl);
static int stl_check_line(p_stl stl);
static int stl_read_chunk(p_stl stl, void *anybuffer, size_t size);
static int stl_read_chunk_reverse(p_stl stl, void *anybuffer, size_t size);
static int stl_write_chunk(p_stl stl, void *anybuffer, size_t size);
static int stl_write_chunk_reverse(p_stl stl, void *anybuffer, size_t size);
static void stl_reverse(void *anydata, size_t size);

/* ----------------------------------------------------------------------
 * String functions
 * ---------------------------------------------------------------------- */
static int stl_find_string(const char *item, const char* const list[]);
static p_stl_element stl_find_element(p_stl stl, const char *name);
static p_stl_property stl_find_property(p_stl_element element,
        const char *name);

/* ----------------------------------------------------------------------
 * Header parsing
 * ---------------------------------------------------------------------- */
static int stl_read_header_format(p_stl stl);
static int stl_read_header_comment(p_stl stl);
static int stl_read_header_obj_info(p_stl stl);
static int stl_read_header_property(p_stl stl);
static int stl_set_header_property(p_stl stl, char* name, int type);
static int stl_read_header_element(p_stl stl);
static int stl_set_header_element(p_stl stl, char* name, int val);

/* ----------------------------------------------------------------------
 * Error handling
 * ---------------------------------------------------------------------- */
static void stl_error_cb(const char *message, gpointer data);
static void stl_error(p_stl stl, const char *fmt, ...);

/* ----------------------------------------------------------------------
 * Memory allocation and initialization
 * ---------------------------------------------------------------------- */
static void stl_init(p_stl stl);
static void stl_element_init(p_stl_element element);
static void stl_property_init(p_stl_property property);
static p_stl stl_alloc(void);
static p_stl_element stl_grow_element(p_stl stl);
static p_stl_property stl_grow_property(p_stl stl, p_stl_element element);
static void *stl_grow_array(p_stl stl, void **pointer, gint32 *nmemb, gint32 size);

/* ----------------------------------------------------------------------
 * Special functions
 * ---------------------------------------------------------------------- */
static e_stl_storage_mode stl_arch_endian(void);
static int stl_type_check(void);

/* ----------------------------------------------------------------------
 * Auxiliary read functions
 * ---------------------------------------------------------------------- */
static int stl_read_element(p_stl stl, p_stl_element element,
        p_stl_argument argument);
static int stl_read_property(p_stl stl, p_stl_element element,
        p_stl_property property, p_stl_argument argument);
static int stl_read_list_property(p_stl stl, p_stl_element element,
        p_stl_property property, p_stl_argument argument);
static int stl_read_scalar_property(p_stl stl, p_stl_element element,
        p_stl_property property, p_stl_argument argument);


/* ----------------------------------------------------------------------
 * Buffer support functions
 * ---------------------------------------------------------------------- */
/* pointers to tokenized word and line in buffer */
#define BWORD(p) (p->buffer + p->buffer_token)
#define BLINE(p) (p->buffer + p->buffer_token)

/* pointer to start of untouched bytes in buffer */
#define BFIRST(p) (p->buffer + p->buffer_first)

/* number of bytes untouched in buffer */
#define BSIZE(p) (p->buffer_last - p->buffer_first)

/* consumes data from buffer */
#define BSKIP(p, s) (p->buffer_first += s)

/* refills the buffer */
static int BREFILL(p_stl stl) {
    /* move untouched data to beginning of buffer */
    size_t size = BSIZE(stl);
    memmove(stl->buffer, BFIRST(stl), size);
    stl->buffer_last = size;
    stl->buffer_first = stl->buffer_token = 0;
    /* fill remaining with new data */
    size = fread(stl->buffer+size, 1, BUFFERSIZE-size-1, stl->fp);
    /* place sentinel so we can use str* functions with buffer */
    stl->buffer[BUFFERSIZE-1] = '\0';
    /* check if read failed */
    if (size <= 0) return 0;
    /* increase size to account for new data */
    stl->buffer_last += size;
    return 1;
}

/* ----------------------------------------------------------------------
 * Exported functions
 * ---------------------------------------------------------------------- */
/* ----------------------------------------------------------------------
 * Read support functions
 * ---------------------------------------------------------------------- */
p_stl stl_open(const char *name, p_stl_error_cb error_cb, gpointer cb_data) {
    char magic[6] = "     ";
    FILE *fp = NULL;
    p_stl stl = NULL;
    if (error_cb == NULL) error_cb = stl_error_cb;
    if (!stl_type_check()) {
        error_cb("Incompatible type system", cb_data);
        return NULL;
    }
    assert(name);
    fp = fopen(name, "rb");
    if (!fp) {
        error_cb("Unable to open file", cb_data);
        return NULL;
    }
    if (fread(magic, 1, 5, fp) < 5) {
        error_cb("Error reading from file", cb_data);
        fclose(fp);
        return NULL;
    }
    if (strcmp(magic, "solid")) {
        fclose(fp);
        error_cb("Not a STL file. Expected magic number 'solid'", cb_data);
        return NULL;
    }
    stl = stl_alloc();
    if (!stl) {
        error_cb("Out of memory", cb_data);
        fclose(fp);
        return NULL;
    }
    stl->fp = fp;
    stl->io_mode = STL_READ;
    stl->error_cb = error_cb;
    stl->cb_data = cb_data;
    return stl;
}

int stl_read_header(p_stl stl) {
    int i, nr_facets = 0;
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    // STL files do not have the number of vertices 
    // and normals, so we fake it.
    while(1){
        if(!stl_read_word(stl)) 
            return 0;
        if(strcmp(BWORD(stl), "facet") == 0)
            nr_facets++;
        else if(strcmp(BWORD(stl), "endsolid") == 0)
            break;
    }
    // reinit to reset buffer pointers.
    stl_init(stl);
    rewind (stl->fp);

    if (!stl_read_header_format(stl)) return 0;
    if (!stl_read_line(stl)) return 0;

    stl_set_header_element(stl, "facet", nr_facets);
    stl_set_header_property(stl, "facet", STL_WORD);    
    stl_set_header_property(stl, "normal", STL_WORD);    
    stl_set_header_property(stl, "nx", STL_FLOAT);
    stl_set_header_property(stl, "ny", STL_FLOAT);
    stl_set_header_property(stl, "nz", STL_FLOAT);
    stl_set_header_property(stl, "outer loop", STL_LINE);    
    stl_set_header_property(stl, "vertex", STL_WORD);    
    stl_set_header_property(stl, "x0", STL_FLOAT);
    stl_set_header_property(stl, "y0", STL_FLOAT);
    stl_set_header_property(stl, "z0", STL_FLOAT);
    stl_set_header_property(stl, "vertex", STL_WORD);    
    stl_set_header_property(stl, "x1", STL_FLOAT);
    stl_set_header_property(stl, "y1", STL_FLOAT);
    stl_set_header_property(stl, "z1", STL_FLOAT);
    stl_set_header_property(stl, "vertex", STL_WORD);    
    stl_set_header_property(stl, "x2", STL_FLOAT);
    stl_set_header_property(stl, "y2", STL_FLOAT);
    stl_set_header_property(stl, "z2", STL_FLOAT);
    stl_set_header_property(stl, "endloop", STL_LINE);    
    stl_set_header_property(stl, "endfacet", STL_LINE);    

    return 1;
}



long stl_set_read_cb(p_stl stl, const char *element_name,
        const char* property_name, p_stl_read_cb read_cb,
        void *pdata, long idata) {
    p_stl_element element = NULL;
    p_stl_property property = NULL;
    assert(stl && element_name && property_name);

    element = stl_find_element(stl, element_name);
    if (!element) return 0;
    property = stl_find_property(element, property_name);
    if (!property) return 0;
    property->read_cb = read_cb;
    property->pdata = pdata;
    property->idata = idata;
    return (int) element->ninstances;
}


int stl_read(p_stl stl) {
    gint32 i;
    p_stl_argument argument;
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    argument = &stl->argument;
    for (i = 0; i < stl->nelements; i++) {
        p_stl_element element = &stl->element[i];
        argument->element = element;
        if (!stl_read_element(stl, element, argument))
            return 0;
    }
    return 1;
}

/* ----------------------------------------------------------------------
 * Write support functions
 * ---------------------------------------------------------------------- */
p_stl stl_create(const char *name, e_stl_storage_mode storage_mode,
        p_stl_error_cb error_cb, gpointer cb_data) {
    FILE *fp = NULL;
    p_stl stl = NULL;
    if (error_cb == NULL) error_cb = stl_error_cb;
    if (!stl_type_check()) {
        error_cb("Incompatible type system", cb_data);
        return NULL;
    }
    assert(name && storage_mode <= STL_DEFAULT);
    fp = fopen(name, "wb");
    if (!fp) {
        error_cb("Unable to create file", cb_data);
        return NULL;
    }
    stl = stl_alloc();
    if (!stl) {
        fclose(fp);
        error_cb("Out of memory", cb_data);
        return NULL;
    }
    stl->io_mode = STL_WRITE;
    if (storage_mode == STL_DEFAULT) storage_mode = stl_arch_endian();
    if (storage_mode == STL_ASCII) stl->odriver = &stl_odriver_ascii;
    else if (storage_mode == stl_arch_endian())
        stl->odriver = &stl_odriver_binary;
    else stl->odriver = &stl_odriver_binary_reverse;
    stl->storage_mode = storage_mode;
    stl->fp = fp;
    stl->error_cb = error_cb;
    stl->cb_data = cb_data;
    return stl;
}

int stl_add_element(p_stl stl, const char *name, gint32 ninstances) {
    p_stl_element element = NULL;
    assert(stl && stl->fp && stl->io_mode == STL_WRITE);
    assert(name && strlen(name) < WORDSIZE && ninstances >= 0);
    if (strlen(name) >= WORDSIZE || ninstances < 0) {
        stl_error(stl, "Invalid arguments");
        return 0;
    }
    element = stl_grow_element(stl);
    if (!element) return 0;
    strcpy(element->name, name);
    element->ninstances = ninstances;
    return 1;
}

int stl_add_scalar_property(p_stl stl, const char *name, e_stl_type type) {
    p_stl_element element = NULL;
    p_stl_property property = NULL;
    assert(stl && stl->fp && stl->io_mode == STL_WRITE);
    assert(name && strlen(name) < WORDSIZE);
    assert(type < STL_LIST);
    if (strlen(name) >= WORDSIZE || type >= STL_LIST) {
        stl_error(stl, "Invalid arguments");
        return 0;
    }
    element = &stl->element[stl->nelements-1];
    property = stl_grow_property(stl, element);
    if (!property) return 0;
    strcpy(property->name, name);
    property->type = type;
    return 1;
}

int stl_add_list_property(p_stl stl, const char *name,
        e_stl_type length_type, e_stl_type value_type) {
    p_stl_element element = NULL;
    p_stl_property property = NULL;
    assert(stl && stl->fp && stl->io_mode == STL_WRITE);
    assert(name && strlen(name) < WORDSIZE);
    if (strlen(name) >= WORDSIZE) {
        stl_error(stl, "Invalid arguments");
        return 0;
    }
    assert(length_type < STL_LIST);
    assert(value_type < STL_LIST);
    if (length_type >= STL_LIST || value_type >= STL_LIST) {
        stl_error(stl, "Invalid arguments");
        return 0;
    }
    element = &stl->element[stl->nelements-1];
    property = stl_grow_property(stl, element);
    if (!property) return 0;
    strcpy(property->name, name);
    property->type = STL_LIST;
    property->length_type = length_type;
    property->value_type = value_type;
    return 1;
}

int stl_add_property(p_stl stl, const char *name, e_stl_type type,
        e_stl_type length_type, e_stl_type value_type) {
    if (type == STL_LIST)
        return stl_add_list_property(stl, name, length_type, value_type);
    else
        return stl_add_scalar_property(stl, name, type);
}

int stl_add_comment(p_stl stl, const char *comment) {
    char *new_comment = NULL;
    assert(stl && comment && strlen(comment) < LINESIZE);
    if (!comment || strlen(comment) >= LINESIZE) {
        stl_error(stl, "Invalid arguments");
        return 0;
    }
    new_comment = (char *) stl_grow_array(stl, (void **) &stl->comment,
            &stl->ncomments, LINESIZE);
    if (!new_comment) return 0;
    strcpy(new_comment, comment);
    return 1;
}

int stl_add_obj_info(p_stl stl, const char *obj_info) {
    char *new_obj_info = NULL;
    assert(stl && obj_info && strlen(obj_info) < LINESIZE);
    if (!obj_info || strlen(obj_info) >= LINESIZE) {
        stl_error(stl, "Invalid arguments");
        return 0;
    }
    new_obj_info = (char *) stl_grow_array(stl, (void **) &stl->obj_info,
            &stl->nobj_infos, LINESIZE);
    if (!new_obj_info) return 0;
    strcpy(new_obj_info, obj_info);
    return 1;
}

int stl_write_header(p_stl stl) {
    gint32 i, j;
    assert(stl && stl->fp && stl->io_mode == STL_WRITE);
    assert(stl->element || stl->nelements == 0);
    assert(!stl->element || stl->nelements > 0);
    if (fprintf(stl->fp, "stl\nformat %s 1.0\n",
                stl_storage_mode_list[stl->storage_mode]) <= 0) goto error;
    for (i = 0; i < stl->ncomments; i++)
        if (fprintf(stl->fp, "comment %s\n", stl->comment + LINESIZE*i) <= 0)
            goto error;
    for (i = 0; i < stl->nobj_infos; i++)
        if (fprintf(stl->fp, "obj_info %s\n", stl->obj_info + LINESIZE*i) <= 0)
            goto error;
    for (i = 0; i < stl->nelements; i++) {
        p_stl_element element = &stl->element[i];
        assert(element->property || element->nproperties == 0);
        assert(!element->property || element->nproperties > 0);
        if (fprintf(stl->fp, "element %s %" G_GINT32_FORMAT " \n", element->name,
                    element->ninstances) <= 0) goto error;
        for (j = 0; j < element->nproperties; j++) {
            p_stl_property property = &element->property[j];
            if (property->type == STL_LIST) {
                if (fprintf(stl->fp, "property list %s %s %s\n",
                            stl_type_list[property->length_type],
                            stl_type_list[property->value_type],
                            property->name) <= 0) goto error;
            } else {
                if (fprintf(stl->fp, "property %s %s\n",
                            stl_type_list[property->type],
                            property->name) <= 0) goto error;
            }
        }
    }
    return fprintf(stl->fp, "end_header\n") > 0;
error:
    stl_error(stl, "Error writing to file");
    return 0;
}

int stl_write(p_stl stl, double value) {
    p_stl_element element = NULL;
    p_stl_property property = NULL;
    int type = -1;
    int breakafter = 0;
    if (stl->welement > stl->nelements) return 0;
    element = &stl->element[stl->welement];
    if (stl->wproperty > element->nproperties) return 0;
    property = &element->property[stl->wproperty];
    if (property->type == STL_LIST) {
        if (stl->wvalue_index == 0) {
            type = property->length_type;
            stl->wlength = (gint32) value;
        } else type = property->value_type;
    } else {
        type = property->type;
        stl->wlength = 0;
    }
    if (!stl->odriver->ohandler[type](stl, value)) {
        stl_error(stl, "Failed writing %s of %s %d (%s: %s)",
                    property->name, element->name,
                    stl->winstance_index,
                    stl->odriver->name, stl_type_list[type]);
        return 0;
    }
    stl->wvalue_index++;
    if (stl->wvalue_index > stl->wlength) {
        stl->wvalue_index = 0;
        stl->wproperty++;
    }
    if (stl->wproperty >= element->nproperties) {
        stl->wproperty = 0;
        stl->winstance_index++;
        if (stl->storage_mode == STL_ASCII) breakafter = 1;
    }
    if (stl->winstance_index >= element->ninstances) {
        stl->winstance_index = 0;
        stl->welement++;
    }
    return !breakafter || putc('\n', stl->fp) > 0;
}

int stl_close(p_stl stl) {
    gint32 i;
    assert(stl && stl->fp);
    assert(stl->element || stl->nelements == 0);
    assert(!stl->element || stl->nelements > 0);
    /* write last chunk to file */
    if (stl->io_mode == STL_WRITE &&
      fwrite(stl->buffer, 1, stl->buffer_last, stl->fp) < stl->buffer_last) {
        stl_error(stl, "Error closing up");
        return 0;
    }
    fclose(stl->fp);
    /* free all memory used by handle */
    if (stl->element) {
        for (i = 0; i < stl->nelements; i++) {
            p_stl_element element = &stl->element[i];
            if (element->property) free(element->property);
        }
        free(stl->element);
    }
    if (stl->obj_info) free(stl->obj_info);
    if (stl->comment) free(stl->comment);
    free(stl);
    return 1;
}

/* ----------------------------------------------------------------------
 * Query support functions
 * ---------------------------------------------------------------------- */
p_stl_element stl_get_next_element(p_stl stl,
        p_stl_element last) {
    assert(stl);
    if (!last) return stl->element;
    last++;
    if (last < stl->element + stl->nelements) return last;
    else return NULL;
}

int stl_get_element_info(p_stl_element element, const char** name,
        gint32 *ninstances) {
    assert(element);
    if (name) *name = element->name;
    if (ninstances) *ninstances = (gint32) element->ninstances;
    return 1;
}

p_stl_property stl_get_next_property(p_stl_element element,
        p_stl_property last) {
    assert(element);
    if (!last) return element->property;
    last++;
    if (last < element->property + element->nproperties) return last;
    else return NULL;
}

int stl_get_property_info(p_stl_property property, const char** name,
        e_stl_type *type, e_stl_type *length_type, e_stl_type *value_type) {
    assert(property);
    if (name) *name = property->name;
    if (type) *type = property->type;
    if (length_type) *length_type = property->length_type;
    if (value_type) *value_type = property->value_type;
    return 1;

}

const char *stl_get_next_comment(p_stl stl, const char *last) {
    assert(stl);
    if (!last) return stl->comment;
    last += LINESIZE;
    if (last < stl->comment + LINESIZE*stl->ncomments) return last;
    else return NULL;
}

const char *stl_get_next_obj_info(p_stl stl, const char *last) {
    assert(stl);
    if (!last) return stl->obj_info;
    last += LINESIZE;
    if (last < stl->obj_info + LINESIZE*stl->nobj_infos) return last;
    else return NULL;
}

/* ----------------------------------------------------------------------
 * Callback argument support functions
 * ---------------------------------------------------------------------- */
int stl_get_argument_element(p_stl_argument argument,
        p_stl_element *element, gint32 *instance_index) {
    assert(argument);
    if (!argument) return 0;
    if (element) *element = argument->element;
    if (instance_index) *instance_index = argument->instance_index;
    return 1;
}

int stl_get_argument_property(p_stl_argument argument,
        p_stl_property *property, gint32 *length, gint32 *value_index) {
    assert(argument);
    if (!argument) return 0;
    if (property) *property = argument->property;
    if (length) *length = argument->length;
    if (value_index) *value_index = argument->value_index;
    return 1;
}

int stl_get_argument_user_data(p_stl_argument argument, void **pdata,
        long *idata) {
    assert(argument);
    if (!argument) return 0;
    if (pdata) *pdata = argument->pdata;
    if (idata) *idata = argument->idata;
    return 1;
}

double stl_get_argument_value(p_stl_argument argument) {
    assert(argument);
    if (!argument) return 0.0;
    return argument->value;
}

/* ----------------------------------------------------------------------
 * Internal functions
 * ---------------------------------------------------------------------- */
static int stl_read_list_property(p_stl stl, p_stl_element element,
        p_stl_property property, p_stl_argument argument) {
    int l;
    p_stl_read_cb read_cb = property->read_cb;
    p_stl_ihandler *driver = stl->idriver->ihandler;
    /* get list length */
    p_stl_ihandler handler = driver[property->length_type];
    double length;
    if (!handler(stl, &length)) {
        stl_error(stl, "Error reading '%s' of '%s' number %d",
                property->name, element->name, argument->instance_index);
        return 0;
    }
    /* invoke callback to pass length in value field */
    argument->length = (gint32) length;
    argument->value_index = -1;
    argument->value = length;
    if (read_cb && !read_cb(argument)) {
        stl_error(stl, "Aborted by user");
        return 0;
    }
    /* read list values */
    handler = driver[property->value_type];
    /* for each value in list */
    for (l = 0; l < (gint32) length; l++) {
        /* read value from file */
        argument->value_index = l;
        if (!handler(stl, &argument->value)) {
            stl_error(stl, "Error reading value number %d of '%s' of "
                    "'%s' number %d", l+1, property->name,
                    element->name, argument->instance_index);
            return 0;
        }
        /* invoke callback to pass value */
        if (read_cb && !read_cb(argument)) {
            stl_error(stl, "Aborted by user");
            return 0;
        }
    }
    return 1;
}

static int stl_read_scalar_property(p_stl stl, p_stl_element element,
        p_stl_property property, p_stl_argument argument) {
    p_stl_read_cb read_cb = property->read_cb;
    p_stl_ihandler *driver = stl->idriver->ihandler;
    p_stl_ihandler handler = driver[property->type];
    argument->length = 1;
    argument->value_index = 0;
    //fprintf(stderr, "stl_read_scalar_property\n");
    if (!handler(stl, &argument->value)) {
        stl_error(stl, "Error reading '%s' of '%s' number %d",
                property->name, element->name, argument->instance_index);
        return 0;
    }
    if (read_cb && !read_cb(argument)) {
        stl_error(stl, "Aborted by user");
        return 0;
    }
    return 1;
}

static int stl_read_property(p_stl stl, p_stl_element element,
        p_stl_property property, p_stl_argument argument) {
    if (property->type == STL_LIST)
        return stl_read_list_property(stl, element, property, argument);
    else
        return stl_read_scalar_property(stl, element, property, argument);
}

static int stl_read_element(p_stl stl, p_stl_element element,
        p_stl_argument argument) {
    gint32 j, k;
    /* for each element of this type */
    for (j = 0; j < element->ninstances; j++) {
        //fprintf(stderr, "reading element %d of %d\n", j, element->ninstances);
        argument->instance_index = j;
        /* for each property */
        for (k = 0; k < element->nproperties; k++) {
            //fprintf(stderr, "reading property %d of %d\n", k, element->nproperties);
            p_stl_property property = &element->property[k];
            argument->property = property;
            argument->pdata = property->pdata;
            argument->idata = property->idata;
            if (!stl_read_property(stl, element, property, argument))
                return 0;
        }
    }
    return 1;
}

static int stl_find_string(const char *item, const char* const list[]) {
    int i;
    assert(item && list);
    for (i = 0; list[i]; i++)
        if (!strcmp(list[i], item)) return i;
    return -1;
}

static p_stl_element stl_find_element(p_stl stl, const char *name) {
    p_stl_element element;
    int i, nelements;

    assert(stl && name);
    element = stl->element;
    nelements = stl->nelements;
    //fprintf(stderr, "stl_find_element %s, nelements: %d\n", name, nelements);
    assert(element || nelements == 0);
    assert(!element || nelements > 0);
    for (i = 0; i < nelements; i++)
        if (!strcmp(element[i].name, name)) return &element[i];
    return NULL;
}

static p_stl_property stl_find_property(p_stl_element element,
        const char *name) {
    p_stl_property property;
    int i, nproperties;
    assert(element && name);
    property = element->property;
    nproperties = element->nproperties;
    assert(property || nproperties == 0);
    assert(!property || nproperties > 0);
    for (i = 0; i < nproperties; i++)
        if (!strcmp(property[i].name, name)) return &property[i];
    return NULL;
}

static int stl_check_word(p_stl stl) {
    if (strlen(BLINE(stl)) >= WORDSIZE) {
        stl_error(stl, "Word too gint32");
        return 0;
    }
    return 1;
}

static int stl_read_word(p_stl stl) {
    size_t t = 0;
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    /* skip leading blanks */
    while (1) {
        t = strspn(BFIRST(stl), " \n\r\t");
        /* check if all buffer was made of blanks */
        if (t >= BSIZE(stl)) {
            if (!BREFILL(stl)) {
                stl_error(stl, "Unexpected end of file");
                return 0;
            }
        } else break;
    }
    BSKIP(stl, t);
    /* look for a space after the current word */
    t = strcspn(BFIRST(stl), " \n\r\t");
    /* if we didn't reach the end of the buffer, we are done */
    if (t < BSIZE(stl)) {
        stl->buffer_token = stl->buffer_first;
        BSKIP(stl, t);
        *BFIRST(stl) = '\0';
        BSKIP(stl, 1);
        return stl_check_word(stl);
    }
    /* otherwise, try to refill buffer */
    if (!BREFILL(stl)) {
        stl_error(stl, "Unexpected end of file");
        return 0;
    }
    /* keep looking from where we left */
    t += strcspn(BFIRST(stl) + t, " \n\r\t");
    /* check if the token is too large for our buffer */
    if (t >= BSIZE(stl)) {
        stl_error(stl, "Token too large");
        return 0;
    }
    /* we are done */
    stl->buffer_token = stl->buffer_first;
    BSKIP(stl, t);
    *BFIRST(stl) = '\0';
    BSKIP(stl, 1);
    return stl_check_word(stl);
}

static int stl_check_line(p_stl stl) {
    if (strlen(BLINE(stl)) >= LINESIZE) {
        stl_error(stl, "Line too gint32");
        return 0;
    }
    return 1;
}

static int stl_read_line(p_stl stl) {
    const char *end = NULL;
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    /* look for a end of line */
    end = strchr(BFIRST(stl), '\n');
    /* if we didn't reach the end of the buffer, we are done */
    if (end) {
        stl->buffer_token = stl->buffer_first;
        BSKIP(stl, end - BFIRST(stl));
        *BFIRST(stl) = '\0';
        BSKIP(stl, 1);
        return stl_check_line(stl);
    } else {
        end = stl->buffer + BSIZE(stl);
        /* otherwise, try to refill buffer */
        if (!BREFILL(stl)) {
            stl_error(stl, "Unexpected end of file");
            return 0;
        }
    }
    /* keep looking from where we left */
    end = strchr(end, '\n');
    /* check if the token is too large for our buffer */
    if (!end) {
        stl_error(stl, "Token too large");
        return 0;
    }
    /* we are done */
    stl->buffer_token = stl->buffer_first;
    BSKIP(stl, end - BFIRST(stl));
    *BFIRST(stl) = '\0';
    BSKIP(stl, 1);
    return stl_check_line(stl);
}

static int stl_read_chunk(p_stl stl, void *anybuffer, size_t size) {
    char *buffer = (char *) anybuffer;
    size_t i = 0;
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    assert(stl->buffer_first <= stl->buffer_last);
    while (i < size) {
        if (stl->buffer_first < stl->buffer_last) {
            buffer[i] = stl->buffer[stl->buffer_first];
            stl->buffer_first++;
            i++;
        } else {
            stl->buffer_first = 0;
            stl->buffer_last = fread(stl->buffer, 1, BUFFERSIZE, stl->fp);
            if (stl->buffer_last <= 0) return 0;
        }
    }
    return 1;
}

static int stl_write_chunk(p_stl stl, void *anybuffer, size_t size) {
    char *buffer = (char *) anybuffer;
    size_t i = 0;
    assert(stl && stl->fp && stl->io_mode == STL_WRITE);
    assert(stl->buffer_last <= BUFFERSIZE);
    while (i < size) {
        if (stl->buffer_last < BUFFERSIZE) {
            stl->buffer[stl->buffer_last] = buffer[i];
            stl->buffer_last++;
            i++;
        } else {
            stl->buffer_last = 0;
            if (fwrite(stl->buffer, 1, BUFFERSIZE, stl->fp) < BUFFERSIZE)
                return 0;
        }
    }
    return 1;
}

static int stl_write_chunk_reverse(p_stl stl, void *anybuffer, size_t size) {
    int ret = 0;
    stl_reverse(anybuffer, size);
    ret = stl_write_chunk(stl, anybuffer, size);
    stl_reverse(anybuffer, size);
    return ret;
}

static int stl_read_chunk_reverse(p_stl stl, void *anybuffer, size_t size) {
    if (!stl_read_chunk(stl, anybuffer, size)) return 0;
    stl_reverse(anybuffer, size);
    return 1;
}

static void stl_reverse(void *anydata, size_t size) {
    char *data = (char *) anydata;
    char temp;
    size_t i;
    for (i = 0; i < size/2; i++) {
        temp = data[i];
        data[i] = data[size-i-1];
        data[size-i-1] = temp;
    }
}

static void stl_init(p_stl stl) {
    stl->c = ' ';
    stl->element = NULL;
    stl->nelements = 0;
    stl->comment = NULL;
    stl->ncomments = 0;
    stl->obj_info = NULL;
    stl->nobj_infos = 0;
    stl->idriver = NULL;
    stl->odriver = NULL;
    stl->buffer[0] = '\0';
    stl->buffer_first = stl->buffer_last = stl->buffer_token = 0;
    stl->welement = 0;
    stl->wproperty = 0;
    stl->winstance_index = 0;
    stl->wlength = 0;
    stl->wvalue_index = 0;
}

static void stl_element_init(p_stl_element element) {
    element->name[0] = '\0';
    element->ninstances = 0;
    element->property = NULL;
    element->nproperties = 0;
}

static void stl_property_init(p_stl_property property) {
    property->name[0] = '\0';
    property->type = -1;
    property->length_type = -1;
    property->value_type = -1;
    property->read_cb = (p_stl_read_cb) NULL;
    property->pdata = NULL;
    property->idata = 0;
}

static p_stl stl_alloc(void) {
    p_stl stl = (p_stl) malloc(sizeof(t_stl));
    if (!stl) return NULL;
    stl_init(stl);
    return stl;
}

static void *stl_grow_array(p_stl stl, void **pointer,
        gint32 *nmemb, gint32 size) {
    void *temp = *pointer;
    gint32 count = *nmemb + 1;
    if (!temp) temp = malloc(count*size);
    else temp = realloc(temp, count*size);
    if (!temp) {
        stl_error(stl, "Out of memory");
        return NULL;
    }
    *pointer = temp;
    *nmemb = count;
    return (char *) temp + (count-1) * size;
}

static p_stl_element stl_grow_element(p_stl stl) {
    p_stl_element element = NULL;
    assert(stl);
    assert(stl->element || stl->nelements == 0);
    assert(!stl->element || stl->nelements > 0);
    element = (p_stl_element) stl_grow_array(stl, (void **) &stl->element,
            &stl->nelements, sizeof(t_stl_element));
    if (!element) return NULL;
    stl_element_init(element);
    return element;
}

static p_stl_property stl_grow_property(p_stl stl, p_stl_element element) {
    p_stl_property property = NULL;
    assert(stl);
    assert(element);
    assert(element->property || element->nproperties == 0);
    assert(!element->property || element->nproperties > 0);
    property = (p_stl_property) stl_grow_array(stl,
            (void **) &element->property,
            &element->nproperties, sizeof(t_stl_property));
    if (!property) return NULL;
    stl_property_init(property);
    return property;
}

static int stl_read_header_format(p_stl stl) {
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    stl->idriver = &stl_idriver_ascii;
    //TODO: find out format

    

    return 1;
}

static int stl_read_header_comment(p_stl stl) {
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    if (strcmp(BWORD(stl), "comment")) return 0;
    if (!stl_read_line(stl)) return 0;
    if (!stl_add_comment(stl, BLINE(stl))) return 0;
    if (!stl_read_word(stl)) return 0;
    return 1;
}

static int stl_read_header_obj_info(p_stl stl) {
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    if (strcmp(BWORD(stl), "obj_info")) return 0;
    if (!stl_read_line(stl)) return 0;
    if (!stl_add_obj_info(stl, BLINE(stl))) return 0;
    if (!stl_read_word(stl)) return 0;
    return 1;
}

static int stl_read_header_property(p_stl stl) {
    p_stl_element element = NULL;
    p_stl_property property = NULL;
    /* make sure it is a property */
    if (strcmp(BWORD(stl), "property")) return 0;
    element = &stl->element[stl->nelements-1];
    property = stl_grow_property(stl, element);
    if (!property) return 0;
    /* get property type */
    if (!stl_read_word(stl)) return 0;
    property->type = stl_find_string(BWORD(stl), stl_type_list);
    if (property->type == (e_stl_type) (-1)) return 0;
    if (property->type == STL_LIST) {
        /* if it's a list, we need the base types */
        if (!stl_read_word(stl)) return 0;
        property->length_type = stl_find_string(BWORD(stl), stl_type_list);
        if (property->length_type == (e_stl_type) (-1)) return 0;
        if (!stl_read_word(stl)) return 0;
        property->value_type = stl_find_string(BWORD(stl), stl_type_list);
        if (property->value_type == (e_stl_type) (-1)) return 0;
    }
    /* get property name */
    if (!stl_read_word(stl)) return 0;
    strcpy(property->name, BWORD(stl));
    if (!stl_read_word(stl)) return 0;
    return 1;
}

static int stl_set_header_property(p_stl stl, char* name, int type) {
    p_stl_element element = NULL;
    p_stl_property property = NULL;
    /* make sure it is a property */
    element = &stl->element[stl->nelements-1];
    property = stl_grow_property(stl, element);
    if (!property) return 0;
    /* get property type */
    property->type = type;
    if (property->type == (e_stl_type) (-1)) return 0;
    if (property->type == STL_LIST) {
        /* if it's a list, we need the base types */
        property->length_type = STL_UCHAR;
        property->value_type = STL_INT;
    }
    /* get property name */
    strcpy(property->name, name);
    return 1;
}

static int stl_read_header_element(p_stl stl) {
    p_stl_element element = NULL;
    gint32 dummy;
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    if (strcmp(BWORD(stl), "element")) return 0;
    /* allocate room for new element */
    element = stl_grow_element(stl);
    if (!element) return 0;
    /* get element name */
    if (!stl_read_word(stl)) return 0;
    strcpy(element->name, BWORD(stl));
    /* get number of elements of this type */
    if (!stl_read_word(stl)) return 0;
    if (sscanf(BWORD(stl), "%" G_GINT32_FORMAT, &dummy) != 1) {
        stl_error(stl, "Expected number got '%s'", BWORD(stl));
        return 0;
    }
    element->ninstances = dummy;
    /* get all properties for this element */
    if (!stl_read_word(stl)) return 0;
    while (stl_read_header_property(stl) ||
        stl_read_header_comment(stl) || stl_read_header_obj_info(stl))
        /* do nothing */;
    return 1;
}


static int stl_set_header_element(p_stl stl, char* name, int val){
    p_stl_element element = NULL;
    gint32 dummy;
    assert(stl && stl->fp && stl->io_mode == STL_READ);
    /* allocate room for new element */
    element = stl_grow_element(stl);
    if (!element) return 0;
    /* get element name */
    strcpy(element->name, name);
    /* get number of elements of this type */
    element->ninstances = val;
}

static void stl_error_cb(const char *message, gpointer data) {
    fprintf(stderr, "RStl: %s\n", message);
}

static void stl_error(p_stl stl, const char *fmt, ...) {
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);
    stl->error_cb(buffer, stl->cb_data);
}

static e_stl_storage_mode stl_arch_endian(void) {
    guint32 i = 1;
    unsigned char *s = (unsigned char *) &i;
    if (*s == 1) return STL_LITTLE_ENDIAN;
    else return STL_BIG_ENDIAN;
}

static int stl_type_check(void) {
    assert(sizeof(char) == 1);
    assert(sizeof(unsigned char) == 1);
    assert(sizeof(gint16) == 2);
    assert(sizeof(guint16) == 2);
    assert(sizeof(gint32) == 4);
    assert(sizeof(guint32) == 4);
    assert(sizeof(float) == 4);
    assert(sizeof(double) == 8);
    if (sizeof(char) != 1) return 0;
    if (sizeof(unsigned char) != 1) return 0;
    if (sizeof(gint16) != 2) return 0;
    if (sizeof(guint16) != 2) return 0;
    if (sizeof(gint32) != 4) return 0;
    if (sizeof(guint32) != 4) return 0;
    if (sizeof(float) != 4) return 0;
    if (sizeof(double) != 8) return 0;
    return 1;
}

/* ----------------------------------------------------------------------
 * Output handlers
 * ---------------------------------------------------------------------- */
static int oascii_int8(p_stl stl, double value) {
    if (value > CHAR_MAX || value < CHAR_MIN) return 0;
    return fprintf(stl->fp, "%d ", (char) value) > 0;
}

static int oascii_uint8(p_stl stl, double value) {
    if (value > UCHAR_MAX || value < 0) return 0;
    return fprintf(stl->fp, "%d ", (unsigned char) value) > 0;
}

static int oascii_int16(p_stl stl, double value) {
    if (value > G_MAXINT16 || value < G_MININT16) return 0;
    return fprintf(stl->fp, "%d ", (gint16) value) > 0;
}

static int oascii_uint16(p_stl stl, double value) {
    if (value > G_MAXUINT16 || value < 0) return 0;
    return fprintf(stl->fp, "%d ", (guint16) value) > 0;
}

static int oascii_int32(p_stl stl, double value) {
    if (value > G_MAXINT32 || value < G_MININT32) return 0;
    return fprintf(stl->fp, "%d ", (int) value) > 0;
}

static int oascii_uint32(p_stl stl, double value) {
    if (value > G_MAXUINT32 || value < 0) return 0;
    return fprintf(stl->fp, "%d ", (unsigned int) value) > 0;
}

static int oascii_float32(p_stl stl, double value) {
    char buf[G_ASCII_DTOSTR_BUF_SIZE];
    if (value < -FLT_MAX || value > FLT_MAX) return 0;
    return fprintf(stl->fp, "%s ",
                   g_ascii_formatd(buf, sizeof (buf), "%g", (float) value)) > 0;
}

static int oascii_float64(p_stl stl, double value) {
    char buf[G_ASCII_DTOSTR_BUF_SIZE];
    if (value < -DBL_MAX || value > DBL_MAX) return 0;
    return fprintf(stl->fp, "%s ",
                   g_ascii_formatd(buf, sizeof (buf), "%g", value)) > 0;
}

static int obinary_int8(p_stl stl, double value) {
    char int8 = (char) value;
    if (value > CHAR_MAX || value < CHAR_MIN) return 0;
    return stl->odriver->ochunk(stl, &int8, sizeof(int8));
}

static int obinary_uint8(p_stl stl, double value) {
    unsigned char uint8 = (unsigned char) value;
    if (value > UCHAR_MAX || value < 0) return 0;
    return stl->odriver->ochunk(stl, &uint8, sizeof(uint8));
}

static int obinary_int16(p_stl stl, double value) {
    gint16 int16 = (gint16) value;
    if (value > G_MAXINT16 || value < G_MININT16) return 0;
    return stl->odriver->ochunk(stl, &int16, sizeof(int16));
}

static int obinary_uint16(p_stl stl, double value) {
    guint16 uint16 = (guint16) value;
    if (value > G_MAXUINT16 || value < 0) return 0;
    return stl->odriver->ochunk(stl, &uint16, sizeof(uint16));
}

static int obinary_int32(p_stl stl, double value) {
    gint32 int32 = (gint32) value;
    if (value > G_MAXINT32 || value < G_MININT32) return 0;
    return stl->odriver->ochunk(stl, &int32, sizeof(int32));
}

static int obinary_uint32(p_stl stl, double value) {
    guint32 uint32 = (guint32) value;
    if (value > G_MAXUINT32 || value < 0) return 0;
    return stl->odriver->ochunk(stl, &uint32, sizeof(uint32));
}

static int obinary_float32(p_stl stl, double value) {
    float float32 = (float) value;
    if (value > FLT_MAX || value < -FLT_MAX) return 0;
    return stl->odriver->ochunk(stl, &float32, sizeof(float32));
}

static int obinary_float64(p_stl stl, double value) {
    return stl->odriver->ochunk(stl, &value, sizeof(value));
}

/* ----------------------------------------------------------------------
 * Input  handlers
 * ---------------------------------------------------------------------- */
static int iascii_int8(p_stl stl, double *value) {
    char *end;
    if (!stl_read_word(stl)) return 0;
    *value = strtol(BWORD(stl), &end, 10);
    if (*end || *value > CHAR_MAX || *value < CHAR_MIN) return 0;
    return 1;
}

static int iascii_uint8(p_stl stl, double *value) {
    char *end;
    if (!stl_read_word(stl)) return 0;
    *value = strtol(BWORD(stl), &end, 10);
    if (*end || *value > UCHAR_MAX || *value < 0) return 0;
    return 1;
}

static int iascii_int16(p_stl stl, double *value) {
    char *end;
    if (!stl_read_word(stl)) return 0;
    *value = strtol(BWORD(stl), &end, 10);
    if (*end || *value > G_MAXINT16 || *value < G_MININT16) return 0;
    return 1;
}

static int iascii_uint16(p_stl stl, double *value) {
    char *end;
    if (!stl_read_word(stl)) return 0;
    *value = strtol(BWORD(stl), &end, 10);
    if (*end || *value > G_MAXUINT16 || *value < 0) return 0;
    return 1;
}

static int iascii_int32(p_stl stl, double *value) {
    char *end;
    if (!stl_read_word(stl)) return 0;
    *value = strtol(BWORD(stl), &end, 10);
    if (*end || *value > G_MAXINT32 || *value < G_MININT32) return 0;
    return 1;
}

static int iascii_uint32(p_stl stl, double *value) {
    char *end;
    if (!stl_read_word(stl)) return 0;
    *value = strtol(BWORD(stl), &end, 10);
    if (*end || *value < 0) return 0;
    return 1;
}

static int iascii_float32(p_stl stl, double *value) {
    char *end;
    if (!stl_read_word(stl)) return 0;
    //fprintf(stderr, "iascii_float32: %s\n", BWORD(stl));
    *value = g_ascii_strtod(BWORD(stl), &end);
    if (*end || *value < -FLT_MAX || *value > FLT_MAX) return 0;
    return 1;
}

static int iascii_word(p_stl stl, double *value) {
    char *end;
    if (!stl_read_word(stl)) return 0;
    //fprintf(stderr, "iascii_word: %s\n", BWORD(stl));
    return 1;
}

static int iascii_line(p_stl stl, double *value) {
    char *end;
    if (!stl_read_line(stl)) return 0;
    //fprintf(stderr, "iascii_line: %s\n", BLINE(stl));
    return 1;
}


static int iascii_float64(p_stl stl, double *value) {
    char *end;
    if (!stl_read_word(stl)) return 0;
    *value = g_ascii_strtod(BWORD(stl), &end);
    if (*end || *value < -DBL_MAX || *value > DBL_MAX) return 0;
    return 1;
}



static int ibinary_int8(p_stl stl, double *value) {
    char int8;
    if (!stl->idriver->ichunk(stl, &int8, 1)) return 0;
    *value = int8;
    return 1;
}

static int ibinary_uint8(p_stl stl, double *value) {
    unsigned char uint8;
    if (!stl->idriver->ichunk(stl, &uint8, 1)) return 0;
    *value = uint8;
    return 1;
}

static int ibinary_int16(p_stl stl, double *value) {
    gint16 int16;
    if (!stl->idriver->ichunk(stl, &int16, sizeof(int16))) return 0;
    *value = int16;
    return 1;
}

static int ibinary_uint16(p_stl stl, double *value) {
    guint16 uint16;
    if (!stl->idriver->ichunk(stl, &uint16, sizeof(uint16))) return 0;
    *value = uint16;
    return 1;
}

static int ibinary_int32(p_stl stl, double *value) {
    gint32 int32;
    if (!stl->idriver->ichunk(stl, &int32, sizeof(int32))) return 0;
    *value = int32;
    return 1;
}

static int ibinary_uint32(p_stl stl, double *value) {
    guint32 uint32;
    if (!stl->idriver->ichunk(stl, &uint32, sizeof(uint32))) return 0;
    *value = uint32;
    return 1;
}

static int ibinary_float32(p_stl stl, double *value) {
    float float32;
    if (!stl->idriver->ichunk(stl, &float32, sizeof(float32))) return 0;
    *value = float32;
    stl_reverse(&float32, sizeof(float32));
    return 1;
}

static int ibinary_float64(p_stl stl, double *value) {
    return stl->idriver->ichunk(stl, value, sizeof(double));
}

/* ----------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */
static t_stl_idriver stl_idriver_ascii = {
    {   iascii_int8, iascii_uint8, iascii_int16, iascii_uint16,
        iascii_int32, iascii_uint32, iascii_float32, iascii_float64,
        iascii_int8, iascii_uint8, iascii_int16, iascii_uint16,
        iascii_int32, iascii_uint32, iascii_float32, iascii_float64, iascii_word, iascii_line
    }, /* order matches e_stl_type enum */
    NULL,
    "ascii input"
};

static t_stl_idriver stl_idriver_binary = {
    {   ibinary_int8, ibinary_uint8, ibinary_int16, ibinary_uint16,
        ibinary_int32, ibinary_uint32, ibinary_float32, ibinary_float64,
        ibinary_int8, ibinary_uint8, ibinary_int16, ibinary_uint16,
        ibinary_int32, ibinary_uint32, ibinary_float32, ibinary_float64
    }, /* order matches e_stl_type enum */
    stl_read_chunk,
    "binary input"
};

static t_stl_idriver stl_idriver_binary_reverse = {
    {   ibinary_int8, ibinary_uint8, ibinary_int16, ibinary_uint16,
        ibinary_int32, ibinary_uint32, ibinary_float32, ibinary_float64,
        ibinary_int8, ibinary_uint8, ibinary_int16, ibinary_uint16,
        ibinary_int32, ibinary_uint32, ibinary_float32, ibinary_float64
    }, /* order matches e_stl_type enum */
    stl_read_chunk_reverse,
    "reverse binary input"
};

static t_stl_odriver stl_odriver_ascii = {
    {   oascii_int8, oascii_uint8, oascii_int16, oascii_uint16,
        oascii_int32, oascii_uint32, oascii_float32, oascii_float64,
        oascii_int8, oascii_uint8, oascii_int16, oascii_uint16,
        oascii_int32, oascii_uint32, oascii_float32, oascii_float64
    }, /* order matches e_stl_type enum */
    NULL,
    "ascii output"
};

static t_stl_odriver stl_odriver_binary = {
    {   obinary_int8, obinary_uint8, obinary_int16, obinary_uint16,
        obinary_int32, obinary_uint32, obinary_float32, obinary_float64,
        obinary_int8, obinary_uint8, obinary_int16, obinary_uint16,
        obinary_int32, obinary_uint32, obinary_float32, obinary_float64
    }, /* order matches e_stl_type enum */
    stl_write_chunk,
    "binary output"
};

static t_stl_odriver stl_odriver_binary_reverse = {
    {   obinary_int8, obinary_uint8, obinary_int16, obinary_uint16,
        obinary_int32, obinary_uint32, obinary_float32, obinary_float64,
        obinary_int8, obinary_uint8, obinary_int16, obinary_uint16,
        obinary_int32, obinary_uint32, obinary_float32, obinary_float64
    }, /* order matches e_stl_type enum */
    stl_write_chunk_reverse,
    "reverse binary output"
};

/* ----------------------------------------------------------------------
 * Copyright (C) 2003 Diego Nehab.  All rights reserved.
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
