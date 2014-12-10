#include "common.h"


char *airplay_devices[30];  
char *active_airplay_devices[30];  
int number_of_devices;
int number_of_active_devices;

char* p_start = NULL; /* the pointer to the start of the current fragment */
char* p_end = NULL; /* the pointer to the end of the current fragment */

static void send_request(char * command);
static void update_progress_bar_shuffle();
static void update_playing_status();
static void update_progress_bar();
static void back_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void select_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void up_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void down_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void select_long_click_handler(ClickRecognizerRef recognizer, void *context);
static void up_long_click_handler(ClickRecognizerRef recognizer, void *context);
static void up_long_click_release_handler(ClickRecognizerRef recognizer, void *context);
static void down_long_click_handler(ClickRecognizerRef recognizer, void *context);
static void down_long_click_release_handler(ClickRecognizerRef recognizer, void *context);
static void click_config_provider(void *context);
static void window_load(Window *window);
static void window_unload(Window *window);
static void window_appear(Window *window);
static void window_disappear(Window *window);
static bool is_device_active(char* device_name);

static Window *window;
static MenuLayer * menu_layer;

static TextLayer *header_text;
static TextLayer *main_text;
static TextLayer *footer_text;

static ProgressBarLayer *progress_bar;

static BitmapLayer* progress_bar_left_icon_layer;
static BitmapLayer* progress_bar_right_icon_layer;
static BitmapLayer* dotted_status_top_layer;
static BitmapLayer* dotted_status_bottom_layer;


static ActionBarLayer *action_bar;

static GBitmap *status_bar_icon;
static GBitmap *action_icon_rewind;
static GBitmap *action_icon_fast_forward;
static GBitmap *action_icon_play;
static GBitmap *action_icon_pause;
static GBitmap *action_icon_volume_up;
static GBitmap *action_icon_volume_down;
static GBitmap *action_icon_refresh;
static GBitmap *progress_bar_clock;
static GBitmap *progress_bar_volume_icon;
static GBitmap *progress_bar_shuffle;
static GBitmap *progress_bar_no_shuffle;
static GBitmap *dotted_status;

static uint8_t sys_volume;
static uint8_t app_volume;
static uint16_t position;
static uint16_t duration;

static bool controlling_volume;
static bool is_playing;
static bool is_shuffling;
static bool shuffle_icon_showing;
static bool refresh_icon_showing;
static bool has_stale_information;

static bool has_loaded = false;

char* strtok1( char* str, const char delimiter ) 
{
    if ( str )
    {
        p_start = str;
    }
    else
    {
        if ( p_end == NULL ) return NULL;
        p_start = p_end+1;
    }
    for ( p_end = p_start; true; p_end++ )
    {
        if ( *p_end == '\0' )
        {
            p_end = NULL;
            break;
        }
        if ( *p_end == delimiter )
        {
            *p_end = '\0';
            break;
        }
    }
    return p_start;
}

void airplay_deinit(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Window - Spotify deinit %p", window);
  // save some stuff
  menu_layer_destroy(menu_layer);
  window_destroy(window);
}

static int16_t menu_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  return 35;
}

static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return 1;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  switch (section_index) {
    case 0:
      // Draw title text in the section header
      return number_of_devices;
      break;
  }
  return 0;
}

// A callback is used to specify the height of the section header
static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  // This is a define provided in pebble.h that you may use for the default height
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

// Here we draw what each header is
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  // Determine which section we're working with
  switch (section_index) {
    case 0:
      // Draw title text in the section header
      menu_cell_basic_header_draw(ctx, cell_layer, "AirPlay Devices");
      break;
  }
}

// This is the menu item draw callback where you specify what each item should look like
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // Switch on section
  //menu_cell_basic_draw(ctx, cell_layer, "device1", NULL, NULL);
  
  switch (cell_index->section) {

    // Available AirPlay Devices
    case 0:
      menu_cell_basic_draw(ctx, cell_layer, airplay_devices[cell_index->row], NULL, is_device_active(airplay_devices[cell_index->row]) ? action_icon_pause : action_icon_play);
      break;
  }
  
}

// Here we capture when a user selects a menu item
void airplay_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  switch (cell_index->section) {
    case 0:
      APP_LOG( APP_LOG_LEVEL_DEBUG, "selected row %d", cell_index->row);      
      char* deviceName;
      deviceName = airplay_devices[cell_index->row];
      char request[80];
      strcpy (request, "set-");
      strcat (request, deviceName);
      APP_LOG( APP_LOG_LEVEL_DEBUG, "sending: %s", request);
      send_request(request);
//      has_stale_information = true;
      break;
  }
}

static void render_devices_menu() {
  APP_LOG( APP_LOG_LEVEL_DEBUG, "rendering %d devices", number_of_devices);
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create the menu layer
  menu_layer = menu_layer_create(bounds);

  // Set all the callbacks for the menu layer
  menu_layer_set_callbacks(menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = airplay_menu_select_callback,
  });

  // Bind the menu layer's click config provider to the window for interactivity
  menu_layer_set_click_config_onto_window(menu_layer, window);

  // Add it to the window for display
  layer_add_child(window_layer, menu_layer_get_layer(menu_layer));
  
}

void airplay_init(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Window - airplay init %p", window);

  action_icon_play = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_PLAY);
  action_icon_pause = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_PAUSE);

  
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
    .appear = window_appear,
    .disappear = window_disappear,
  });

  send_request("info");

}

void airplay_connected(bool connected) {
  wsConnected = connected;
}


void airplay_update_ui(DictionaryIterator *iter) {
  /*
  if (!has_loaded) {
    return;
  }
  */
  int i, len;
  char *tok, *text;


  Tuple* tuple = dict_read_first(iter);

  while(tuple) {
    switch(tuple->key) {
      case KEY_APDEVICES:
        // vibes_short_pulse();
        APP_LOG( APP_LOG_LEVEL_DEBUG, "AP Devices: %s",tuple->value->cstring);
        memset(airplay_devices, 0, sizeof airplay_devices); // reset array
        len = strlen( tuple->value->cstring ) + 1;
        text = malloc( len );
        strcpy( text, tuple->value->cstring );
        for ( ( i = 0, tok = strtok1( text, '|' ) ); tok != NULL && i<50; ( i++, tok = strtok1( NULL, '|' ) ) ) {
            len = strlen( tok );
            airplay_devices[i] = malloc( len+1 );
            strcpy( airplay_devices[i], tok );
        }
        number_of_devices = i;
        break;
      case KEY_ACTIVEAPDEVICES:
        // vibes_short_pulse();
        APP_LOG( APP_LOG_LEVEL_DEBUG, "Active AP Devices: %s",tuple->value->cstring);
        memset(active_airplay_devices, 0, sizeof active_airplay_devices); // reset array
        len = strlen( tuple->value->cstring ) + 1;
        text = malloc( len );
        strcpy( text, tuple->value->cstring );
        for ( ( i = 0, tok = strtok1( text, '|' ) ); tok != NULL && i<50; ( i++, tok = strtok1( NULL, '|' ) ) ) {
            len = strlen( tok );
            active_airplay_devices[i] = malloc( len+1 );
            strcpy( active_airplay_devices[i], tok );
        }
        number_of_active_devices = i;
        break;
    }
    tuple = dict_read_next(iter);
  }
  if (number_of_devices) {
    render_devices_menu();
  }
  has_stale_information = false;
  
  //update bar here since we have both position and duration
  // progress_bar_layer_set_value(seek_bar, seek_tuple->value->int16);
}

static bool is_device_active(char* device_name) {
  int i;
  for(i=0;i<number_of_active_devices;i++) {
    if (strcmp(device_name,active_airplay_devices[i])) {
      return true;
    }
  }
  return false;
}

static void update_progress_bar_shuffle() {
  bitmap_layer_set_bitmap(progress_bar_right_icon_layer, is_shuffling ? progress_bar_shuffle : progress_bar_no_shuffle);
}

static void update_playing_status() {
  action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, is_playing ? action_icon_pause : action_icon_play);
}

static void update_progress_bar() {
  progress_bar_layer_set_value(progress_bar, (controlling_volume) ? sys_volume : (position*100)/duration);
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  /*
  if (has_stale_information) {
     send_request("info");
  }
  */
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //

static void send_request(char * command) {
  send_command("AirPlay", command);
}

// Events
static void back_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  airplay_deinit();
  window_stack_pop(true);
}

static void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  send_request(is_playing ? "pause" : "play");
  is_playing = !is_playing;
  update_playing_status();
}

static void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  send_request(controlling_volume ? "volume_up" : "previous");
}

static void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  send_request(controlling_volume ? "volume_down" : "next");
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  controlling_volume = !controlling_volume;
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, controlling_volume ? action_icon_volume_up : action_icon_rewind);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, controlling_volume ? action_icon_volume_down : action_icon_fast_forward);
  bitmap_layer_set_bitmap(progress_bar_left_icon_layer, controlling_volume ? progress_bar_volume_icon : progress_bar_clock);
  update_progress_bar();
}

static void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!controlling_volume) {
    if (is_shuffling) {
      action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, progress_bar_no_shuffle);
    } else {
      action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, progress_bar_shuffle);
    }
    shuffle_icon_showing = true;
  }
}

static void up_long_click_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (shuffle_icon_showing) {
    action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, controlling_volume ? action_icon_volume_up : action_icon_rewind);

    send_request(is_shuffling ? "disable_shuffle" : "enable_shuffle");
    shuffle_icon_showing = false;
  }
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, action_icon_refresh);
  refresh_icon_showing = true;
}

static void down_long_click_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (refresh_icon_showing) {
    action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, controlling_volume ? action_icon_volume_down : action_icon_fast_forward);

    send_request("info");
    refresh_icon_showing = false;
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, LONG_CLICK_HOLD_MS, select_long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_UP, LONG_CLICK_HOLD_MS, up_long_click_handler, up_long_click_release_handler);
  window_long_click_subscribe(BUTTON_ID_DOWN, LONG_CLICK_HOLD_MS, down_long_click_handler, down_long_click_release_handler);
}

static void window_appear(Window *window) {
//  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
}

static void window_disappear(Window *window) {
//  tick_timer_service_unsubscribe();
}

static void window_load(Window *window) {

  has_loaded = true;
}

static void window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Window - Airplay window_unload %p", window);
  
  gbitmap_destroy(action_icon_play);
  gbitmap_destroy(action_icon_pause);

  
  has_loaded = false;
  menu_layer_destroy(menu_layer);
}

void airplay_control() {
  airplay_init();
  window_stack_push(window, true);
}