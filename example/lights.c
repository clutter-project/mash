#include <mx/mx.h>
#include <mash/mash.h>
#include <math.h>

#define N_LIGHTS 3
#define N_PAGES (N_LIGHTS + 1)

/* Since version 1.3.8 of Clutter, Cogl has started using
   CoglMaterial* instead of CoglHandle to point to materials. This
   doesn't usually cause any problems because CoglHandle is typedef'd
   to a gpointer so it gets silently cast to and from
   CoglMaterial*. However in this case we are using a function pointer
   with CoglHandle as one of the arguments so GCC will give a warning
   that the function doesn't match the typedef */

#if CLUTTER_CHECK_VERSION (1, 3, 8)
typedef CoglMaterial *MaterialType;
#else
typedef CoglHandle MaterialType;
#endif

typedef void (* MaterialColorSetFunc) (MaterialType material,
                                       const CoglColor *color);

typedef void (* MaterialColorGetFunc) (MaterialType material,
                                       CoglColor *color);

typedef void (* MaterialFloatSetFunc) (MaterialType material, float value);

typedef float (* MaterialFloatGetFunc) (MaterialType material);

typedef struct
{
  ClutterActor *lights[N_LIGHTS];
  ClutterActor *model;
  CoglHandle light_marker_material;

  ClutterActor *notebook;
  ClutterActor *notebook_buttons[N_PAGES];
  ClutterActor *notebook_pages[N_PAGES];
} Data;

typedef struct
{
  char *prop_name;
  GObject *object;
  CoglHandle material;
  ClutterActor *rect;
  MaterialColorSetFunc set_func;
  MaterialColorGetFunc get_func;
} ColorProp;

typedef struct
{
  ColorProp *prop;
  gint comp_num;
  ClutterActor *label;
} ColorPropComp;

typedef struct
{
  char *prop_name;
  GObject *object;
  CoglHandle material;
  ClutterActor *label;
  float min, max;
  MaterialFloatSetFunc set_func;
  MaterialFloatGetFunc get_func;
} FloatProp;

static void
color_prop_free (ColorProp *prop)
{
  if (prop->object)
    g_object_unref (prop->object);
  if (prop->material)
    cogl_handle_unref (prop->material);
  g_free (prop->prop_name);
  g_slice_free (ColorProp, prop);
}

static void
color_prop_comp_free (ColorPropComp *prop_comp)
{
  if (prop_comp->comp_num == 0)
    color_prop_free (prop_comp->prop);

  g_slice_free (ColorPropComp, prop_comp);
}

static void
float_prop_free (FloatProp *prop)
{
  if (prop->object)
    g_object_unref (prop->object);
  if (prop->material)
    cogl_handle_unref (prop->material);
  g_free (prop->prop_name);
  g_slice_free (FloatProp, prop);
}

static void
update_prop_comp_label (ColorPropComp *prop_comp, guint8 value)
{
  char *label_text;

  label_text = g_strdup_printf ("%i", value);
  mx_label_set_text (MX_LABEL (prop_comp->label), label_text);
  g_free (label_text);
}

static void
color_prop_value_cb (MxSlider *slider,
                     GParamSpec *pspec,
                     ColorPropComp *prop_comp)
{
  float value = mx_slider_get_value (slider) * 255.0f + 0.5f;
  ColorProp *prop = prop_comp->prop;

  if (prop->object)
    {
      ClutterColor *color;
      g_object_get (prop->object, prop->prop_name, &color, NULL);
      ((guint8 *) color)[prop_comp->comp_num] = value;
      clutter_rectangle_set_color (CLUTTER_RECTANGLE (prop->rect), color);
      g_object_set (prop->object, prop->prop_name, color, NULL);
      clutter_color_free (color);
    }
  else
    {
      ClutterColor color;
      CoglColor cogl_color;

      prop->get_func (prop->material, &cogl_color);

      color.red = cogl_color_get_red_byte (&cogl_color);
      color.green = cogl_color_get_green_byte (&cogl_color);
      color.blue = cogl_color_get_blue_byte (&cogl_color);
      color.alpha = 255;

      ((guint8 *) &color)[prop_comp->comp_num] = value;

      cogl_color_set_from_4ub (&cogl_color,
                               color.red,
                               color.green,
                               color.blue,
                               255);

      clutter_rectangle_set_color (CLUTTER_RECTANGLE (prop->rect), &color);

      prop->set_func (prop->material, &cogl_color);
    }

  update_prop_comp_label (prop_comp, value);
}

static void
update_float_prop_label (FloatProp *prop, float value)
{
  char *value_text = g_strdup_printf ("%.2f", value);
  mx_label_set_text (MX_LABEL (prop->label), value_text);
  g_free (value_text);
}

static void
float_prop_value_cb (MxSlider *slider,
                     GParamSpec *pspec,
                     FloatProp *prop)
{
  float value = mx_slider_get_value (slider);

  value = value * (prop->max - prop->min) + prop->min;

  if (prop->object)
    g_object_set (prop->object, prop->prop_name, value, NULL);
  else if (prop->material)
    prop->set_func (prop->material, value);

  update_float_prop_label (prop, value);
}

static void
add_color_prop_base (ClutterActor *table,
                     const char *name,
                     ColorProp *prop,
                     const ClutterColor *value)
{
  int table_y = mx_table_get_row_count (MX_TABLE (table));
  ClutterActor *label;
  static const char *color_names[] = { "red", "green", "blue" };
  int i;

  label = mx_label_new_with_text (name);
  mx_table_add_actor (MX_TABLE (table), label, table_y, 0);
  prop->rect = clutter_rectangle_new ();
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (prop->rect), value);
  clutter_actor_set_size (prop->rect, 20, 0);
  mx_table_add_actor (MX_TABLE (table), prop->rect, table_y++, 1);

  for (i = 0; i < G_N_ELEMENTS (color_names); i++)
    {
      ClutterActor *slider;
      char *label_name = g_strconcat (name, " ", color_names[i], NULL);
      ColorPropComp *prop_comp = g_slice_new (ColorPropComp);

      label = mx_label_new_with_text (label_name);
      mx_table_add_actor (MX_TABLE (table), label, table_y, 0);
      g_free (label_name);

      slider = mx_slider_new ();
      mx_table_add_actor (MX_TABLE (table), slider, table_y, 1);

      mx_slider_set_value (MX_SLIDER (slider),
                           ((guint8 *) value)[i] / 255.0f);

      prop_comp->comp_num = i;
      prop_comp->prop = prop;

      prop_comp->label = mx_label_new ();
      mx_table_add_actor (MX_TABLE (table), prop_comp->label, table_y, 2);
      update_prop_comp_label (prop_comp, ((guint8 *) value)[i]);

      g_signal_connect_data (slider, "notify::value",
                             G_CALLBACK (color_prop_value_cb),
                             prop_comp,
                             (GClosureNotify) color_prop_comp_free,
                             0);

      table_y++;
    }
}

static void
add_color_prop (ClutterActor *table,
                const char *name,
                GObject *object,
                const char *prop_name)
{
  ColorProp *prop = g_slice_new (ColorProp);
  ClutterColor *value;

  prop->prop_name = g_strdup (prop_name);
  prop->object = g_object_ref (object);
  prop->material = COGL_INVALID_HANDLE;

  g_object_get (object, prop_name, &value, NULL);

  add_color_prop_base (table, name, prop, value);

  clutter_color_free (value);
}

static void
add_material_color_prop (ClutterActor *table,
                         const char *name,
                         CoglHandle material,
                         MaterialColorSetFunc set_func,
                         MaterialColorGetFunc get_func)
{
  ColorProp *prop = g_slice_new (ColorProp);
  ClutterColor value;
  CoglColor cogl_color;

  prop->prop_name = NULL;
  prop->material = cogl_handle_ref (material);
  prop->object = NULL;
  prop->set_func = set_func;
  prop->get_func = get_func;

  get_func (material, &cogl_color);
  value.red = cogl_color_get_red_byte (&cogl_color);
  value.green = cogl_color_get_green_byte (&cogl_color);
  value.blue = cogl_color_get_blue_byte (&cogl_color);
  value.alpha = cogl_color_get_alpha_byte (&cogl_color);

  add_color_prop_base (table, name, prop, &value);
}

static void
add_float_prop_base (ClutterActor *table,
                     const char *name,
                     FloatProp *prop,
                     float value)
{
  int table_y = mx_table_get_row_count (MX_TABLE (table));
  ClutterActor *label;
  ClutterActor *slider;

  label = mx_label_new_with_text (name);
  mx_table_add_actor (MX_TABLE (table), label, table_y, 0);

  slider = mx_slider_new ();
  mx_table_add_actor (MX_TABLE (table), slider, table_y, 1);

  prop->label = mx_label_new ();
  mx_table_add_actor (MX_TABLE (table), prop->label, table_y, 2);

  mx_slider_set_value (MX_SLIDER (slider),
                       (value - prop->min) / (prop->max - prop->min));

  update_float_prop_label (prop, value);

  g_signal_connect_data (slider, "notify::value",
                         G_CALLBACK (float_prop_value_cb),
                         prop,
                         (GClosureNotify) float_prop_free,
                         0);
}

static void
add_float_prop (ClutterActor *table,
                const char *name,
                GObject *object,
                const char *prop_name,
                float min,
                float max)
{
  FloatProp *prop = g_slice_new (FloatProp);
  float value;

  prop->prop_name = g_strdup (prop_name);
  prop->object = g_object_ref (object);
  prop->material = COGL_INVALID_HANDLE;
  prop->min = min;
  prop->max = max;

  g_object_get (object, prop_name, &value, NULL);

  add_float_prop_base (table, name, prop, value);
}

static void
add_material_float_prop (ClutterActor *table,
                         const char *name,
                         CoglHandle material,
                         float min,
                         float max,
                         MaterialFloatSetFunc set_func,
                         MaterialFloatGetFunc get_func)
{
  FloatProp *prop = g_slice_new (FloatProp);
  float value;

  prop->prop_name = NULL;
  prop->object = NULL;
  prop->material = cogl_handle_ref (material);
  prop->min = min;
  prop->max = max;
  prop->set_func = set_func;
  prop->get_func = get_func;

  value = get_func (material);

  add_float_prop_base (table, name, prop, value);
}

static void
notebook_button_cb (ClutterActor *button, GParamSpec *spec, Data *data)
{
  if (mx_button_get_toggled (MX_BUTTON (button)))
    {
      int i;

      for (i = 0; i < N_PAGES; i++)
        if (data->notebook_buttons[i] == button)
          mx_notebook_set_current_page (MX_NOTEBOOK (data->notebook),
                                        data->notebook_pages[i]);
        else
          mx_button_set_toggled (MX_BUTTON (button), FALSE);
    }
}

int
main (int argc, char **argv)
{
  ClutterActor *stage = stage;
  ClutterActor *side_box;
  ClutterActor *button_box;
  ClutterActor *light_box;
  ClutterAnimation *anim;
  MxStyle *style;
  GError *error = NULL;
  Data data;
  int i;

  clutter_init (&argc, &argv);

  style = mx_style_get_default ();
  if (!mx_style_load_from_file (style, "lights.css",
                                &error))
    {
      g_warning ("Error setting style: %s", error->message);
      g_clear_error (&error);
    }

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 800, 600);

  side_box = mx_table_new ();
  clutter_actor_set_name (side_box, "side-box");
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), side_box);
  clutter_actor_set_size (side_box, 300,
                          clutter_actor_get_height (stage));
  clutter_actor_set_x (side_box,
                       clutter_actor_get_width (stage)
                       - clutter_actor_get_width (side_box));

  button_box = mx_table_new ();
  mx_table_add_actor (MX_TABLE (side_box), button_box, 0, 0);

  data.notebook = mx_notebook_new ();

  mx_table_add_actor (MX_TABLE (side_box), data.notebook, 1, 0);

  data.model = mash_model_new_from_file (MASH_DATA_NONE,
                                         argc > 1
                                         ? argv[1]
                                         : "suzanne.ply",
                                         &error);
  if (data.model == NULL)
    {
      g_warning ("Error loading model: %s", error->message);
      g_clear_error (&error);
      return 1;
    }

  light_box = mash_light_box_new (clutter_fixed_layout_new ());

  clutter_actor_set_size (data.model, 400, 400);
  clutter_actor_set_position (data.model, 50.0, 100.0);
  clutter_container_add_actor (CLUTTER_CONTAINER (light_box), data.model);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), light_box);

  g_signal_connect_swapped (light_box, "paint",
                            G_CALLBACK (cogl_set_depth_test_enabled),
                            GINT_TO_POINTER (TRUE));
  g_signal_connect_data (light_box, "paint",
                         G_CALLBACK (cogl_set_depth_test_enabled),
                         GINT_TO_POINTER (FALSE), NULL,
                         G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  data.light_marker_material = cogl_material_new ();
  {
    CoglColor color;
    cogl_color_set_from_4ub (&color, 255, 0, 0, 255);
    /* Use the layer state to ignore the vertex color from the shader so
       that the light marker won't itself be lit */
    cogl_material_set_layer_combine_constant (data.light_marker_material, 0,
                                              &color);
    cogl_material_set_layer_combine (data.light_marker_material, 0,
                                     "RGBA = REPLACE(CONSTANT)",
                                     NULL);
  }

  clutter_actor_set_rotation (data.model, CLUTTER_Y_AXIS,
                              0.0f,
                              clutter_actor_get_width (data.model) / 2.0f,
                              0.0f,
                              0.0f);

  anim = clutter_actor_animate (data.model, CLUTTER_LINEAR, 3000,
                                "rotation-angle-y", 360.0f,
                                NULL);
  clutter_animation_set_loop (anim, TRUE);

  for (i = 0; i < N_LIGHTS; i++)
    {
      ClutterActor *table = mx_table_new ();
      ClutterActor *button;
      static ClutterActor *(* constructors[N_LIGHTS]) (void) =
        { mash_directional_light_new,
          mash_point_light_new,
          mash_spot_light_new };
      static const ClutterColor black = { 0, 0, 0, 255 };

      data.lights[i] = constructors[i] ();

      button = mx_button_new_with_label (G_OBJECT_TYPE_NAME (data.lights[i]));
      mx_table_add_actor (MX_TABLE (button_box), button, i, 0);

      /* Default to disable all of the lights */
      g_object_set (data.lights[i],
                    "ambient", &black,
                    "diffuse", &black,
                    "specular", &black,
                    NULL);

      data.notebook_buttons[i] = button;

      clutter_container_add_actor (CLUTTER_CONTAINER (light_box),
                                   data.lights[i]);

      add_color_prop (table, "ambient light",
                      G_OBJECT (data.lights[i]), "ambient");
      add_color_prop (table, "diffuse light",
                      G_OBJECT (data.lights[i]), "diffuse");
      add_color_prop (table, "specular light",
                      G_OBJECT (data.lights[i]), "specular");

      if (MASH_IS_POINT_LIGHT (data.lights[i]))
        {
          add_float_prop (table, "constant attenuation",
                          G_OBJECT (data.lights[i]), "constant-attenuation",
                          0.0f, 10.0f);
          add_float_prop (table, "linear attenuation",
                          G_OBJECT (data.lights[i]), "linear-attenuation",
                          0.0f, 10.0f);
          add_float_prop (table, "quadratic attenuation",
                          G_OBJECT (data.lights[i]), "quadratic-attenuation",
                          0.0f, 10.0f);
        }

      if (MASH_IS_SPOT_LIGHT (data.lights[i]))
        {
          clutter_actor_set_x (data.lights[i], 250);

          add_float_prop (table, "spot cutoff",
                          G_OBJECT (data.lights[i]), "spot-cutoff",
                          0.0f, 90.0f);
          add_float_prop (table, "spot exponent",
                          G_OBJECT (data.lights[i]), "spot-exponent",
                          0.0f, 128.0f);
        }

      clutter_container_add_actor (CLUTTER_CONTAINER (data.notebook), table);

      data.notebook_pages[i] = table;
    }

  {
    ClutterActor *button;
    ClutterActor *table;
    CoglHandle material;
    float maximum_shininess;

    material = mash_model_get_material (MASH_MODEL (data.model));

    /* Before version 1.3.10 on the 1.3 branch and 1.2.14 on the 1.2
       branch Cogl would remap the shininess property to the range
       [0,1]. After this it is just a value greater or equal to zero
       (but GL imposes a limit of 128.0) */
    if (clutter_check_version (1, 3, 9)
        || (clutter_major_version == 1
            && clutter_minor_version == 2
            && clutter_micro_version >= 13))
      maximum_shininess = 128.0f;
    else
      maximum_shininess = 1.0f;

    cogl_material_set_shininess (material, maximum_shininess);

    button = mx_button_new_with_label ("Material");
    data.notebook_buttons[i] = button;
    mx_table_add_actor (MX_TABLE (button_box), button, i, 0);
    table = mx_table_new ();
    data.notebook_pages[i] = table;
    clutter_container_add_actor (CLUTTER_CONTAINER (data.notebook), table);

    add_material_color_prop (table, "diffuse", material,
                             cogl_material_set_diffuse,
                             cogl_material_get_diffuse);
    add_material_color_prop (table, "ambient", material,
                             cogl_material_set_ambient,
                             cogl_material_get_ambient);
    add_material_color_prop (table, "specular", material,
                             cogl_material_set_specular,
                             cogl_material_get_specular);
    add_material_float_prop (table, "shininess", material,
                             0.0f, maximum_shininess,
                             cogl_material_set_shininess,
                             cogl_material_get_shininess);
  }

  for (i = 0; i < N_PAGES; i++)
    {
      g_signal_connect (data.notebook_buttons[i], "notify::toggled",
                        G_CALLBACK (notebook_button_cb), &data);
      mx_button_set_is_toggle (MX_BUTTON (data.notebook_buttons[i]), TRUE);
    }

  mx_button_set_toggled (MX_BUTTON (data.notebook_buttons[0]), TRUE);

  clutter_actor_show (stage);

  clutter_main ();

  cogl_handle_unref (data.light_marker_material);

  return 0;
}
