#include <pebble.h>

#define USE_FIXED_POINT 1

#ifdef USE_FIXED_POINT
  #include "math-sll.h"
#else
  #define CONST_PI M_PI
#endif

static Window *window;
static BitmapLayer *render_layer = NULL;
static GBitmap *bitmap = NULL;


GFont lcd_date_font = NULL;
GFont lcd_time_font = NULL;
GBitmap *mask = NULL;

//Digital Time Display
char time_string[] = "00:00";  // Make this longer to show AM/PM
Layer *digital_layer = NULL;

//Digital Date Display
char date_wday_string[] = "WED";
char date_mday_string[] = "30";
Layer *date_layer = NULL;
static const char *const dname[7] =
{"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

static GPath *minute_arrow;
static GPath *minute_fill;

static GPath *hour_arrow;
static GPath *hour_fill;
static GPath *peg_fill;

static GPath *tick_path_1;
static GPath *tick_path_3;
static GPath *tick_path_5;
static GPath *tick_path_7;
static GPath *tick_path_9;
static GPath *tick_path_11;

static GPoint tick_point_2;
static GPoint tick_point_4;
static GPoint tick_point_6;
static GPoint tick_point_8;
static GPoint tick_point_10;
static GPoint tick_point_12;

#define CAT(a, b) a##b
#define STR(s) #s
#define draw_hour(hour) \
  graphics_context_set_text_color(ctx, GColorBlack); \
  graphics_draw_text(ctx, STR(hour), custom_font_outline, (GRect){.origin = \
      (GPoint){CAT(tick_point_, hour).x - 17, CAT(tick_point_, hour).y - 11}, .size = {32,32}}, \
      GTextOverflowModeFill, GTextAlignmentCenter, NULL); \
  graphics_context_set_text_color(ctx, GColorWhite); \
  graphics_draw_text(ctx, STR(hour), custom_font_text, (GRect){.origin = \
      (GPoint){CAT(tick_point_, hour).x - 16, CAT(tick_point_, hour).y - 10}, .size = {32,32}}, \
      GTextOverflowModeFill, GTextAlignmentCenter, NULL)

static GFont custom_font_text = NULL;
static GFont custom_font_outline = NULL;

static Layer *analog_layer;

static const GPathInfo MINUTE_HAND_POINTS = {6, (GPoint []) {
    { -6,  15 },
    {  0,  20 },
    {  6,  15 },
    {  4, -56 },
    {  0, -66 },
    { -4, -56 }
  }
};

static const GPathInfo MINUTE_FILL_POINTS = {5, (GPoint []) {
    { -6,  15 },
    {  0,  20 },
    {  6,  15 },
    {  4, -8 },
    { -4, -8 }
  }
};

static const GPathInfo HOUR_HAND_POINTS = {6, (GPoint []) {
    { -7,  12},
    { -0,  15},
    {  7,  12},
    {  5, -42},
    {  0, -52},
    { -5, -42}
  }
};

static const GPathInfo HOUR_FILL_POINTS = {5, (GPoint []) {
    { -7,  12},
    {  0,  15},
    {  7,  12},
    {  7, -8},
    { -7, -8},
  }
};

static const GPathInfo PEG_POINTS = {6, (GPoint []) {
  // Start at the top point, clockwise
    {  0,  5},
    {  5,  2},
    {  5, -2},
    {  0, -5},
    { -5, -2},
    { -5,  2},
  }
};

#define TICK_DISTANCE 72
static const GPathInfo TICK_POINTS = {4, (GPoint []) {
    {  3,  7},
    { -3,  7},
    { -5, -9},
    {  5, -9},
  }
};

// local prototypes
static void draw_ticks(Layer *layer, GContext *ctx);

static void digital_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  GPoint box_point = grect_center_point(&bounds);

  // Size box to width of wday
  GSize box_size = graphics_text_layout_get_content_size(
      time_string, lcd_time_font, bounds,
      GTextOverflowModeWordWrap, GTextAlignmentCenter);
  box_size.w += 2; // Padding

  graphics_draw_bitmap_in_rect(ctx, mask, 
      GRect(box_point.x - box_size.w / 2, bounds.origin.y, box_size.w, bounds.size.h));

  graphics_context_set_text_color(ctx, GColorCyan);
  graphics_draw_text(ctx, time_string, lcd_time_font, 
      GRect( 
        bounds.origin.x + 2, bounds.origin.y - 2,
        bounds.size.w, bounds.size.h - 2),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  GPoint box_point = grect_center_point(&bounds);

  // Size box to width of wday
  GSize box_size = graphics_text_layout_get_content_size(
      date_wday_string, lcd_date_font, bounds,
      GTextOverflowModeWordWrap, GTextAlignmentCenter);
  box_size.w += 4; // Padding

  graphics_draw_bitmap_in_rect(ctx, mask, 
      GRect(box_point.x - box_size.w / 2, bounds.origin.y, box_size.w, bounds.size.h));

  graphics_context_set_text_color(ctx, GColorCyan);
  graphics_draw_text(ctx, date_wday_string, lcd_date_font,
      GRect( 
        bounds.origin.x + 2, bounds.origin.y - 2,
        bounds.size.w, bounds.size.h),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  graphics_draw_text(ctx, date_mday_string, lcd_date_font,
      GRect( 
        bounds.origin.x + 2, bounds.origin.y + 20 - 2,
        bounds.size.w, bounds.size.h),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void analog_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  // Draw ticks behind
  draw_ticks(layer, ctx);

  // minute/hour hand
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_context_set_antialiased(ctx, true);

  gpath_rotate_to(minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_rotate_to(minute_fill, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_rotate_to(hour_arrow, 
      (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10)))/(12 * 6));
  gpath_rotate_to(hour_fill, 
      (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10)))/(12 * 6));

  // Draw minute hand background
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 6);
  gpath_draw_outline(ctx, minute_arrow);

  // Draw minute hand foreground
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 3);
  gpath_draw_outline(ctx, minute_arrow);
  gpath_draw_filled(ctx, minute_fill);

  // Draw hour hand background
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 6);
  gpath_draw_outline(ctx, hour_arrow);

  // Draw hour hand foreground
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 3);
  gpath_draw_outline(ctx, hour_arrow);
  gpath_draw_filled(ctx, hour_fill);

  // Draw a black peg in the center
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  gpath_draw_filled(ctx, peg_fill);
}

static void draw_ticks(Layer *layer, GContext *ctx) {
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 5);
  
  gpath_draw_outline(ctx, tick_path_1);
  gpath_draw_filled(ctx, tick_path_1);

  gpath_draw_outline(ctx, tick_path_3);
  gpath_draw_filled(ctx, tick_path_3);
  
  gpath_draw_outline(ctx, tick_path_5);
  gpath_draw_filled(ctx, tick_path_5);

  gpath_draw_outline(ctx, tick_path_7);
  gpath_draw_filled(ctx, tick_path_7);
  
  gpath_draw_outline(ctx, tick_path_9);
  gpath_draw_filled(ctx, tick_path_9);
  
  gpath_draw_outline(ctx, tick_path_11);
  gpath_draw_filled(ctx, tick_path_11);

  draw_hour(2);
  draw_hour(4);
  draw_hour(6);
  draw_hour(8);
  draw_hour(10);
  draw_hour(12);
}

GPoint tick_point(GPoint center, uint16_t hour) {
  return (GPoint){
      center.x + TICK_DISTANCE * sin_lookup(TRIG_MAX_ANGLE * hour / 12) / TRIG_MAX_RATIO, 
      center.y - TICK_DISTANCE * cos_lookup(TRIG_MAX_ANGLE * hour / 12) / TRIG_MAX_RATIO};
}

void tick_setup(GPath **path, GPoint center, uint16_t hour) {
  *path = gpath_create(&TICK_POINTS);
  gpath_rotate_to(*path, TRIG_MAX_ANGLE * hour / 12);
  gpath_move_to(*path, tick_point(center, hour));
}


void tick_init(Window* window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  const GPoint center = grect_center_point(&bounds);

  tick_setup(&tick_path_1, center, 1);
  tick_setup(&tick_path_3, center, 3);
  tick_setup(&tick_path_5, center, 5);
  tick_setup(&tick_path_7, center, 7);
  tick_setup(&tick_path_9, center, 9);
  tick_setup(&tick_path_11, center, 11);

  tick_point_2 = tick_point(center, 2);
  tick_point_4 = tick_point(center, 4);
  tick_point_6 = tick_point(center, 6);
  tick_point_8 = tick_point(center, 8);
  tick_point_10 = tick_point(center, 10);
  tick_point_12 = tick_point(center, 12);
}

void analog_init(Window* window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  const GPoint center = grect_center_point(&bounds);

  analog_layer = layer_create(bounds);
  layer_set_update_proc(analog_layer, analog_update_proc);
  layer_add_child(window_layer, analog_layer);

  minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  minute_fill = gpath_create(&MINUTE_FILL_POINTS);
  hour_arrow = gpath_create(&HOUR_HAND_POINTS);
  hour_fill = gpath_create(&HOUR_FILL_POINTS);
  peg_fill = gpath_create(&PEG_POINTS);

  gpath_move_to(minute_arrow, center);
  gpath_move_to(minute_fill, center);
  gpath_move_to(hour_arrow, center);
  gpath_move_to(hour_fill, center);
  gpath_move_to(peg_fill, center);

  tick_init(window);
}

void analog_destroy(void) {
  layer_destroy(analog_layer);
  gpath_destroy(minute_arrow);
  gpath_destroy(hour_arrow);
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed){
  if (units_changed & DAY_UNIT) {
    time_t current_time = time(NULL);
    struct tm *current_tm = localtime(&current_time);
    snprintf(date_wday_string, sizeof(date_wday_string), "%s", dname[current_tm->tm_wday]);
    snprintf(date_mday_string, sizeof(date_mday_string), "%d", current_tm->tm_mday);
    layer_mark_dirty(date_layer);
  }
  clock_copy_time_string(time_string,sizeof(time_string));
  // Remove the space on the end of the string in AM/PM mode
  if (strchr(time_string, ' ')) {
    time_string[strlen(time_string) - 1] = '\0';
  }
  layer_mark_dirty(digital_layer);
}


#define COLOR(x) {.argb=GColor ## x ## ARGB8}

typedef struct Pattern {
  int R;
  int r;
  int d;
} Pattern;

Pattern patterns[]= {
  {65, 15, 24},  // big lobes
  {95, 55, 12},  // ball
  {65, -15, 24}, // offscreen
  {95, 55, 42},  // offspiral
  {15, 55, 45},  // tight ball
  {25, 55, 25},  // lopsided ball
  {35, 55, 35},  // very cool 
};

GColor background_colors[] = {
  COLOR(Black), 
  COLOR(Blue), 
  COLOR(DukeBlue),
  COLOR(OxfordBlue),
  COLOR(DarkGreen),
  COLOR(ImperialPurple),
};

bool draw_spirograph(GContext* ctx, int x, int y, int outer, int inner, int dist) {
  //int t = 144 / 2; // (width / 2)

  static GPoint point = {0,0};
  static GPoint first_point = {0,0};
  static GColor color;
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_antialiased(ctx, true);

  sll R = int2sll(outer);
  sll r = int2sll(inner);
  sll d = int2sll(dist);
  sll q = sllsub(R, r);
  sll xip = slldiv(CONST_PI, sllmul(CONST_2, sllexp(CONST_4))); // PI / 2e^4
  sll R_pi = sllmul(R, CONST_PI);

  static sll index = CONST_0;

  int segments = 0;

  while(index < R_pi) {
    index = slladd(index, xip);
    sll iq_over_r = slldiv(sllmul(index, q), r); // i * (q / r)
    int xval = 
      sll2int(sllmul(q, sllsin(index))) - sll2int(sllmul(d, sllsin(iq_over_r)));
    int yval = 
      sll2int(sllmul(q, sllcos(index))) + sll2int(sllmul(d, sllcos(iq_over_r)));

    // If we are back to the starting point, quit
    if (gpoint_equal(&(GPoint){xval, yval}, &first_point)) {
      index = CONST_0;
      point = GPointZero;
      return false;
    }

    if (!gpoint_equal(&point, &GPointZero)) {
      graphics_draw_line(ctx, (GPoint){point.x + x, point.y + y}, (GPoint){xval + x, yval + y});
    } else {
      first_point = (GPoint){xval, yval};
    }

    point = (GPoint){xval, yval};

    if (segments++ >= 10) {
      color.argb = (color.argb + 1 ) | 0xc0;
      break;
    }
  }

  if (index > R_pi) {
    index = CONST_0;
    point = GPointZero;
    return false;
  }

  return true;
}

// GBitmap and GContext are opaque types, so provide just enough here to allow
// offscreen rendering into a bitmap
typedef struct BitmapInfo {
  bool is_bitmap_heap_allocated:1;
  GBitmapFormat format:3;
  bool is_palette_heap_allocated:1;
  uint16_t reserved:7;
  uint8_t version:4;
} BitmapInfo;

typedef struct {
  uint16_t offset;
  uint8_t min_x;
  uint8_t max_x;
} GBitmapDataRowInfoInternal;

static GBitmapDataRowInfoInternal row_info[180] = {};
static bool info_setup = false;

typedef struct MyGBitmap {
  void *addr;
  uint16_t row_size_bytes;
  union {
    //! Bitfields of metadata flags.
    uint16_t info_flags;
    BitmapInfo info;
  };
  GRect bounds;
  GBitmapDataRowInfoInternal *data_row_infos;
} MyGBitmap;

typedef struct MyGContext {
  MyGBitmap dest_bitmap;
} MyGContext;

static void update_display(Layer* layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const GPoint center = grect_center_point(&bounds);
  bool retval = true;
  static int idx = 0;
  int num_patterns = sizeof(patterns) / sizeof(Pattern);

  if (!info_setup) {
    info_setup = true;
    for (int i = 0; i < 180; i++) {
      row_info[i].offset = i * 180;
      row_info[i].min_x = 0;
      row_info[i].max_x = 180;
    }
  }

  graphics_context_set_compositing_mode(ctx, GCompOpAssign);

  //backup old dest_bitmap addr
  uint8_t *orig_addr = gbitmap_get_data((GBitmap*)(&((MyGContext*)ctx)->dest_bitmap));
  GBitmapFormat orig_format = gbitmap_get_format((GBitmap*)(&((MyGContext*)ctx)->dest_bitmap));
  GBitmapDataRowInfoInternal *orig_row_info = ((MyGContext*)ctx)->dest_bitmap.data_row_infos;

  //replace screen bitmap with our offscreen render bitmap
  gbitmap_set_data((GBitmap*)(&((MyGContext*)ctx)->dest_bitmap), gbitmap_get_data(bitmap),
    GBitmapFormat8BitCircular, gbitmap_get_bytes_per_row(bitmap), false);
    //gbitmap_get_format(bitmap), gbitmap_get_bytes_per_row(bitmap), false);
  ((MyGContext*)ctx)->dest_bitmap.data_row_infos = row_info;

  retval = draw_spirograph(ctx, center.x, center.y, 
      patterns[idx].R, patterns[idx].r, patterns[idx].d);
  
  if (!retval) {
    //reset the background color
    //window_set_background_color(window_stack_get_top_window(), 
    //    background_colors[rand() % (sizeof(background_colors) / sizeof(GColor))]);
    graphics_context_set_fill_color(ctx, 
        background_colors[rand() % (sizeof(background_colors) / sizeof(GColor))]);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    //change the pattern randomly
    //while (last_idx == idx) {
    //  idx = rand() % num_patterns;
    //}
    idx = (idx + 1) % num_patterns;
  }

  //restore original context bitmap
  gbitmap_set_data((GBitmap*)(&((MyGContext*)ctx)->dest_bitmap), orig_addr, orig_format, 0, false);
  ((MyGContext*)ctx)->dest_bitmap.data_row_infos = orig_row_info;

  //draw the bitmap to the screen
  graphics_draw_bitmap_in_rect(ctx, bitmap, bounds);
}

static bool looking = false;
static void register_timer(void* data) {
  if (looking) {
    app_timer_register(50, register_timer, data);
    layer_mark_dirty(bitmap_layer_get_layer(render_layer));
  }
}

#define ACCEL_DEADZONE 100
#define WITHIN(n, min, max) (((n)>=(min) && (n) <= (max)) ? true : false)

void accel_handler(AccelData *data, uint32_t num_samples) {
  for (uint32_t i = 0; i < num_samples; i++) {
    if(
        WITHIN(data[i].x, -400, 400) &&
        WITHIN(data[i].y, -900, 0) &&
        WITHIN(data[i].z, -1100, -300)) {
      if (!looking) {
        looking = true;
        light_enable(true);
        register_timer(NULL);
      }
      return;
    }
  }
  looking = false;
  light_enable(false);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  const GPoint center = grect_center_point(&bounds);

  render_layer = bitmap_layer_create(bounds);
  bitmap = gbitmap_create_blank(bounds.size, GBitmapFormat8Bit);
  bitmap_layer_set_bitmap(render_layer, bitmap);
  layer_set_update_proc(bitmap_layer_get_layer(render_layer), update_display);
  layer_add_child(window_layer, bitmap_layer_get_layer(render_layer));
  //register_timer(NULL);

  custom_font_text = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_BOXY_TEXT_20));
  custom_font_outline = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_BOXY_OUTLINE_20));
  mask = gbitmap_create_with_resource(RESOURCE_ID_MASK);
  
  //Add layers from back to front (background first)

  //Load the lcd font
  lcd_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCD_24));
  lcd_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCD_20));

  //Setup background layer for digital time display
  digital_layer = layer_create(GRect(center.x - 32, center.y + 22, 32 * 2, 24));
  layer_set_update_proc(digital_layer, digital_update_proc);
  layer_add_child(window_layer, digital_layer);

  //Setup background layer for digital date display
  date_layer = layer_create(GRect(center.x - 20, center.y - 56, 20 * 2, 40));
  layer_set_update_proc(date_layer, date_update_proc);
  layer_add_child(window_layer, date_layer);

  //Add analog hands
  analog_init(window);

  //Force time update
  time_t current_time = time(NULL);
  struct tm *current_tm = localtime(&current_time);
  tick_handler(current_tm, MINUTE_UNIT | DAY_UNIT);

  //Setup tick time handler
  tick_timer_service_subscribe((MINUTE_UNIT), tick_handler);
  
  //Setup magic motion accel handler
  //accel_data_service_subscribe(5, accel_handler);
  //accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  light_enable(true);
  looking = true;
  register_timer(NULL);
}

static void window_unload(Window *window) {
}

static void init(void) {
  window = window_create();
  //window_set_fullscreen(window, true);
  window_set_background_color(window, GColorBlack);
  window_set_window_handlers(window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload
      });
  window_stack_push(window, true);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
