#include <pebble.h>

// ==================== 結構定義 ====================

// 顯示圖層結構
typedef struct {
    BitmapLayer *layer;
    GBitmap *bitmap;
    uint32_t current_resource_id;
    PropertyAnimation *animation;
} DisplayLayer;

// ==================== 全域變數 & 設定 Key ====================

// UI
static Window *s_main_window;
static GColor s_accent_color;
static bool s_is_dark_theme;

// Layers
static DisplayLayer s_hour_layers[2];
static DisplayLayer s_minute_layers[2];
static DisplayLayer s_month_layers[2];
static DisplayLayer s_day_layers[2];
static DisplayLayer s_week_layer;
static DisplayLayer s_yue_layer;
static DisplayLayer s_ri_layer;
static DisplayLayer s_zhou_layer;

// 設定儲存的 Key
enum AppMessageKey {
    KEY_MINUTE_COLOR = 0,
    KEY_THEME_IS_DARK = 1,
};

// ==================== 平台專屬佈局常數 ====================

#define ANIMATION_DURATION_MS 300
#define FADE_OUT_DISTANCE 5

#if defined(PBL_PLATFORM_EMERY)
    // Emery 方形大螢幕 (200x228)
    #define TIME_IMAGE_SIZE GSize(88, 88)
    #define DATE_IMAGE_SIZE GSize(22, 22)
    // 時間位置
    #define TIME_COL1_X 8
    #define TIME_COL2_X 104
    #define TIME_ROW1_Y 8
    #define TIME_ROW2_Y 104
    // 日期位置
    #define DATE_ROW_Y 199
    #define DATE_MONTH1_X 8
    #define DATE_MONTH2_X 31
    #define DATE_YUE_X 54
    #define DATE_DAY1_X 77
    #define DATE_DAY2_X 100
    #define DATE_RI_X 123
    #define DATE_ZHOU_X 146
    #define DATE_WEEK_X 169
#else
    // 方形小螢幕 (Aplite, Basalt, Diorite: 144x168)
    #define TIME_IMAGE_SIZE GSize(66, 66)
    #define DATE_IMAGE_SIZE GSize(11, 11)
    // 時間位置
    #define TIME_COL1_X 4
    #define TIME_COL2_X 74
    #define TIME_ROW1_Y 4
    #define TIME_ROW2_Y 74
    // 日期位置
    #define DATE_ROW_Y 149
    #define DATE_MONTH1_X 4
    #define DATE_MONTH2_X 16
    #define DATE_YUE_X 28
    #define DATE_DAY1_X 50
    #define DATE_DAY2_X 62
    #define DATE_RI_X 74
    #define DATE_ZHOU_X 117
    #define DATE_WEEK_X 129
#endif

// ==================== 圖片資源映射表 ====================

const uint32_t TIME_UPPERCASE_TENS_RESOURCES[] = {
    0, RESOURCE_ID_IMG_U10, RESOURCE_ID_IMG_U2,
};
const uint32_t TIME_UPPERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_U10, RESOURCE_ID_IMG_U1, RESOURCE_ID_IMG_U2, RESOURCE_ID_IMG_U3,
    RESOURCE_ID_IMG_U4, RESOURCE_ID_IMG_U5, RESOURCE_ID_IMG_U6, RESOURCE_ID_IMG_U7,
    RESOURCE_ID_IMG_U8, RESOURCE_ID_IMG_U9
};
const uint32_t TIME_LOWERCASE_TENS_RESOURCES[] = {
    RESOURCE_ID_IMG_L0, RESOURCE_ID_IMG_L10, RESOURCE_ID_IMG_L20, RESOURCE_ID_IMG_L30,
    RESOURCE_ID_IMG_L4, RESOURCE_ID_IMG_L5,
};
const uint32_t TIME_LOWERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_L10, RESOURCE_ID_IMG_L1, RESOURCE_ID_IMG_L2, RESOURCE_ID_IMG_L3,
    RESOURCE_ID_IMG_L4, RESOURCE_ID_IMG_L5, RESOURCE_ID_IMG_L6, RESOURCE_ID_IMG_L7,
    RESOURCE_ID_IMG_L8, RESOURCE_ID_IMG_L9
};
const uint32_t DATE_UPPERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_SU10, RESOURCE_ID_IMG_SU1, RESOURCE_ID_IMG_SU2, RESOURCE_ID_IMG_SU3,
    RESOURCE_ID_IMG_SU4, RESOURCE_ID_IMG_SU5, RESOURCE_ID_IMG_SU6, RESOURCE_ID_IMG_SU7,
    RESOURCE_ID_IMG_SU8, RESOURCE_ID_IMG_SU9
};
const uint32_t DATE_LOWERCASE_TENS_RESOURCES[] = {
    0, RESOURCE_ID_IMG_SL10, RESOURCE_ID_IMG_SL20, RESOURCE_ID_IMG_SL30,
};
const uint32_t DATE_LOWERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_SL10, RESOURCE_ID_IMG_SL1, RESOURCE_ID_IMG_SL2, RESOURCE_ID_IMG_SL3,
    RESOURCE_ID_IMG_SL4, RESOURCE_ID_IMG_SL5, RESOURCE_ID_IMG_SL6, RESOURCE_ID_IMG_SL7,
    RESOURCE_ID_IMG_SL8, RESOURCE_ID_IMG_SL9,
};

// Forward declaration
static void update_time();
static void update_date(struct tm* tick_time);

// ==================== 主題 & 顏色邏輯 ====================

/**
 * 根據平台和主題設定，套用點陣圖的顏色
 */
static void apply_theme_to_layer(DisplayLayer *display_layer, GBitmap *bitmap) {
    if (!bitmap) return;

    GColor *palette = gbitmap_get_palette(bitmap);
    if (!palette) return;

#if defined(PBL_COLOR)
    // --- 彩色平台邏輯 ---
    GColor text_color = s_is_dark_theme ? GColorWhite : GColorBlack;

    if (display_layer == &s_hour_layers[0] || display_layer == &s_hour_layers[1]) {
        for (int i = 0; i < 8; i++) {
            if (gcolor_equal(palette[i], GColorRed)) {
                palette[i] = s_accent_color;
            } else if (gcolor_equal(palette[i], GColorBlack)) {
                palette[i] = text_color;
            }
        }
    } else if (display_layer == &s_minute_layers[1]) {
        for (int i = 0; i < 2; i++) {
            if (gcolor_equal(palette[i], GColorBlack)) {
                palette[i] = s_accent_color;
                break;
            }
        }
    } else {
        for (int i = 0; i < 4; i++) {
            if (gcolor_equal(palette[i], GColorBlack)) {
                palette[i] = text_color;
                break;
            }
        }
    }
#else
    // --- 黑白平台邏輯 ---
    // 對於 1-bit 圖片，我們只改變黑色到白色，透明保持不變
    if (s_is_dark_theme) {
        // 尋找黑色並將其改為白色
        for (int i = 0; i < 2; i++) {
            if (gcolor_equal(palette[i], GColorBlack)) {
                palette[i] = GColorWhite;
                break;
            }
        }
    }
    // 如果是淺色主題，圖片本身就是黑色的，所以無需操作
#endif
}

// ==================== 動畫 & 圖層管理 ====================

static void fade_in_stopped_handler(Animation *animation, bool finished, void *context) {
    DisplayLayer *dl = (DisplayLayer *)context;
    if (dl && dl->animation) {
        property_animation_destroy(dl->animation);
        dl->animation = NULL;
    }
}

static void fade_out_stopped_handler(Animation *animation, bool finished, void *context) {
    if (!finished) return;
    
    DisplayLayer *dl = (DisplayLayer *)context;
    Layer *l = bitmap_layer_get_layer(dl->layer);
    
    if (dl->bitmap) {
        gbitmap_destroy(dl->bitmap);
    }
    
    uint32_t new_res_id = dl->current_resource_id;
    dl->bitmap = (new_res_id != 0) ? gbitmap_create_with_resource(new_res_id) : NULL;
    apply_theme_to_layer(dl, dl->bitmap);
    bitmap_layer_set_bitmap(dl->layer, dl->bitmap);
    
    GRect current_frame = layer_get_frame(l);
    GRect target_frame = current_frame;
    target_frame.origin.y -= FADE_OUT_DISTANCE;
    
    dl->animation = property_animation_create_layer_frame(l, &current_frame, &target_frame);
    animation_set_duration((Animation *)dl->animation, ANIMATION_DURATION_MS / 2);
    animation_set_curve((Animation *)dl->animation, AnimationCurveEaseOut);
    animation_set_handlers((Animation *)dl->animation, (AnimationHandlers){.stopped = fade_in_stopped_handler}, dl);
    animation_schedule((Animation *)dl->animation);
}

static void set_display_layer_bitmap_animated(DisplayLayer *display_layer, uint32_t resource_id) {
    if (display_layer->current_resource_id == resource_id) return;

    if (display_layer->animation) {
        animation_unschedule((Animation *)display_layer->animation);
        property_animation_destroy(display_layer->animation);
        display_layer->animation = NULL;
    }

    Layer *layer = bitmap_layer_get_layer(display_layer->layer);
    GRect current_frame = layer_get_frame(layer);
    
    if (display_layer->current_resource_id == 0) { // First time setup
        if (display_layer->bitmap) gbitmap_destroy(display_layer->bitmap);
        
        display_layer->bitmap = (resource_id != 0) ? gbitmap_create_with_resource(resource_id) : NULL;
        apply_theme_to_layer(display_layer, display_layer->bitmap);
        bitmap_layer_set_bitmap(display_layer->layer, display_layer->bitmap);
        display_layer->current_resource_id = resource_id;
        
        GRect target_frame = current_frame;
        target_frame.origin.y -= FADE_OUT_DISTANCE;
        display_layer->animation = property_animation_create_layer_frame(layer, &current_frame, &target_frame);
        animation_set_duration((Animation *)display_layer->animation, ANIMATION_DURATION_MS / 2);
        animation_set_handlers((Animation *)display_layer->animation, (AnimationHandlers){.stopped = fade_in_stopped_handler}, display_layer);
        animation_schedule((Animation *)display_layer->animation);
        return;
    }

    display_layer->current_resource_id = resource_id;
    
    GRect fade_out_frame = current_frame;
    fade_out_frame.origin.y += FADE_OUT_DISTANCE;
    
    display_layer->animation = property_animation_create_layer_frame(layer, &current_frame, &fade_out_frame);
    animation_set_duration((Animation *)display_layer->animation, ANIMATION_DURATION_MS / 2);
    animation_set_curve((Animation *)display_layer->animation, AnimationCurveEaseIn);
    animation_set_handlers((Animation *)display_layer->animation, (AnimationHandlers){.stopped = fade_out_stopped_handler}, display_layer);
    animation_schedule((Animation *)display_layer->animation);
}

static void set_display_layer_bitmap(DisplayLayer *display_layer, uint32_t resource_id) {
    if (display_layer->bitmap) {
        gbitmap_destroy(display_layer->bitmap);
    }
    display_layer->bitmap = (resource_id != 0) ? gbitmap_create_with_resource(resource_id) : NULL;
    apply_theme_to_layer(display_layer, display_layer->bitmap);
    bitmap_layer_set_bitmap(display_layer->layer, display_layer->bitmap);
    display_layer->current_resource_id = resource_id;
}

// ==================== 時間 & 日期更新 ====================

static void update_time() {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    int hour = tick_time->tm_hour;
    if (!clock_is_24h_style()) {
        if (hour == 0) {
            hour = 12;
        } else if (hour > 12) {
            hour -= 12;
        }
    }

    uint32_t hour_tens_res_id = 0;
    uint32_t hour_ones_res_id = 0;

    if (hour == 0) { // For 24h format 00:xx
        hour_ones_res_id = RESOURCE_ID_IMG_U0;
    } else {
        hour_ones_res_id = TIME_UPPERCASE_ONES_RESOURCES[hour % 10];
        if (hour >= 10) {
            hour_tens_res_id = TIME_UPPERCASE_TENS_RESOURCES[hour / 10];
        }
    }

    set_display_layer_bitmap_animated(&s_hour_layers[0], hour_tens_res_id);
    set_display_layer_bitmap_animated(&s_hour_layers[1], hour_ones_res_id);

    int minute = tick_time->tm_min;
    int m1 = minute / 10;
    int m2 = minute % 10;

    uint32_t minute_tens_res_id = 0;
    uint32_t minute_ones_res_id = 0;

    if (minute == 0) {
        minute_tens_res_id = RESOURCE_ID_IMG_DIAN;
        minute_ones_res_id = RESOURCE_ID_IMG_ZHENG;
    } else if (minute == 30) {
        minute_tens_res_id = RESOURCE_ID_IMG_DIAN;
        minute_ones_res_id = RESOURCE_ID_IMG_BAN;
    } else if (minute == 10) {
        minute_tens_res_id = RESOURCE_ID_IMG_L1;
        minute_ones_res_id = RESOURCE_ID_IMG_L0;
    } else if (m2 != 0) {
        minute_tens_res_id = TIME_LOWERCASE_TENS_RESOURCES[m1];
        minute_ones_res_id = TIME_LOWERCASE_ONES_RESOURCES[m2];
    } else {
        minute_tens_res_id = TIME_LOWERCASE_ONES_RESOURCES[m1];
        minute_ones_res_id = TIME_LOWERCASE_ONES_RESOURCES[m2];
    }

    set_display_layer_bitmap_animated(&s_minute_layers[0], minute_tens_res_id);
    set_display_layer_bitmap_animated(&s_minute_layers[1], minute_ones_res_id);
}

static void update_date(struct tm *tick_time) {
    int month = tick_time->tm_mon + 1;
    int day = tick_time->tm_mday;
    int week = tick_time->tm_wday;

    uint32_t month_tens_res_id = 0;
    uint32_t month_ones_res_id = DATE_UPPERCASE_ONES_RESOURCES[month % 10];
    if (month > 10) {
        month_tens_res_id = RESOURCE_ID_IMG_SU10;
    }

    int d1 = day / 10;
    int d2 = day % 10;

    uint32_t day_tens_res_id = 0;
    uint32_t day_ones_res_id = DATE_LOWERCASE_ONES_RESOURCES[d2];

    if (day > 10) {
        day_tens_res_id = (d2 == 0) ? DATE_LOWERCASE_ONES_RESOURCES[d1] : DATE_LOWERCASE_TENS_RESOURCES[d1];
    }
            
    uint32_t week_res_id = (week = 0) ? RESOURCE_ID_IMG_RI : DATE_LOWERCASE_ONES_RESOURCES[week];
    set_display_layer_bitmap_animated(&s_month_layers[0], month_tens_res_id);
    set_display_layer_bitmap_animated(&s_month_layers[1], month_ones_res_id);
    set_display_layer_bitmap_animated(&s_day_layers[0], day_tens_res_id);
    set_display_layer_bitmap_animated(&s_day_layers[1], day_ones_res_id);
    set_display_layer_bitmap_animated(&s_week_layer, week_res_id);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
    if (units_changed & DAY_UNIT) {
        update_date(tick_time);
    }
}

// ==================== UI 畫面建構 & 銷毀 ====================

static void create_display_layer(Layer *parent, GRect bounds, DisplayLayer *dl, bool animated) {
    if (animated) {
        bounds.origin.y += FADE_OUT_DISTANCE;
    }
    dl->layer = bitmap_layer_create(bounds);
    bitmap_layer_set_background_color(dl->layer, GColorClear);

    // 現在所有平台都使用 GCompOpSet，因為顏色由調色盤控制
    bitmap_layer_set_compositing_mode(dl->layer, GCompOpSet);

    layer_add_child(parent, bitmap_layer_get_layer(dl->layer));
    dl->bitmap = NULL;
    dl->current_resource_id = 0;
    dl->animation = NULL;
}

static void destroy_display_layer(DisplayLayer *dl) {
    if (!dl) return;
    if (dl->animation) {
        animation_unschedule((Animation *)dl->animation);
        property_animation_destroy(dl->animation);
    }
    gbitmap_destroy(dl->bitmap);
    bitmap_layer_destroy(dl->layer);
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);

#if defined(PBL_PLATFORM_EMERY)
    // --- Emery Rectangular Layout ---
    create_display_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[0], true);
    create_display_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[1], true);
    create_display_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[0], true);
    create_display_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[1], true);

    create_display_layer(window_layer, GRect(DATE_MONTH1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_month_layers[0], true);
    create_display_layer(window_layer, GRect(DATE_MONTH2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_month_layers[1], true);
    create_display_layer(window_layer, GRect(DATE_YUE_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_yue_layer, false);
    create_display_layer(window_layer, GRect(DATE_DAY1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_day_layers[0], true);
    create_display_layer(window_layer, GRect(DATE_DAY2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_day_layers[1], true);
    create_display_layer(window_layer, GRect(DATE_RI_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_ri_layer, false);
    create_display_layer(window_layer, GRect(DATE_ZHOU_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_zhou_layer, false);
    create_display_layer(window_layer, GRect(DATE_WEEK_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_week_layer, true);
#else
    // --- Small Rectangular Layout (Aplite, Basalt, Diorite) ---
    create_display_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[0], true);
    create_display_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[1], true);
    create_display_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[0], true);
    create_display_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[1], true);

    create_display_layer(window_layer, GRect(DATE_MONTH1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_month_layers[0], true);
    create_display_layer(window_layer, GRect(DATE_MONTH2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_month_layers[1], true);
    create_display_layer(window_layer, GRect(DATE_YUE_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_yue_layer, false);
    create_display_layer(window_layer, GRect(DATE_DAY1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_day_layers[0], true);
    create_display_layer(window_layer, GRect(DATE_DAY2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_day_layers[1], true);
    create_display_layer(window_layer, GRect(DATE_RI_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_ri_layer, false);
    create_display_layer(window_layer, GRect(DATE_ZHOU_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_zhou_layer, false);
    create_display_layer(window_layer, GRect(DATE_WEEK_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_week_layer, true);
#endif

    // Set bitmaps for fixed layers
    set_display_layer_bitmap(&s_yue_layer, RESOURCE_ID_IMG_YUE);
    set_display_layer_bitmap(&s_ri_layer, RESOURCE_ID_IMG_RI);
    set_display_layer_bitmap(&s_zhou_layer, RESOURCE_ID_IMG_ZHOU);

    // Manually update time and date for the first time
    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    update_time();
    update_date(current_time);
}

static void main_window_unload(Window *window) {
    DisplayLayer* all_layers[] = {
        &s_hour_layers[0], &s_hour_layers[1], &s_minute_layers[0], &s_minute_layers[1],
        &s_month_layers[0], &s_month_layers[1], &s_day_layers[0], &s_day_layers[1],
        &s_week_layer, &s_yue_layer, &s_ri_layer, &s_zhou_layer
    };
    for (size_t i = 0; i < ARRAY_LENGTH(all_layers); i++) {
        destroy_display_layer(all_layers[i]);
    }
}

// ==================== AppMessage & 設定處理 ====================

static void refresh_layer_theme(DisplayLayer *dl) {
    if (dl && dl->layer && dl->bitmap) {
        apply_theme_to_layer(dl, dl->bitmap);
        layer_mark_dirty(bitmap_layer_get_layer(dl->layer));
    }
}

static void update_theme() {
    window_set_background_color(s_main_window, s_is_dark_theme ? GColorBlack : GColorWhite);

    DisplayLayer* all_layers[] = {
        &s_hour_layers[0], &s_hour_layers[1], &s_minute_layers[0], &s_minute_layers[1],
        &s_month_layers[0], &s_month_layers[1], &s_day_layers[0], &s_day_layers[1],
        &s_week_layer, &s_yue_layer, &s_ri_layer, &s_zhou_layer
    };
    for (size_t i = 0; i < ARRAY_LENGTH(all_layers); i++) {
        refresh_layer_theme(all_layers[i]);
    }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    bool theme_changed = false;

    Tuple *theme_t = dict_find(iter, KEY_THEME_IS_DARK);
    if (theme_t) {
        s_is_dark_theme = theme_t->value->int32 == 1;
        persist_write_bool(KEY_THEME_IS_DARK, s_is_dark_theme);
        theme_changed = true;
    }

    Tuple *accent_color_t = dict_find(iter, KEY_MINUTE_COLOR);
    if (accent_color_t) {
        s_accent_color = GColorFromHEX(accent_color_t->value->int32);
        persist_write_int(KEY_MINUTE_COLOR, accent_color_t->value->int32);
        theme_changed = true;
    }

    if (theme_changed) {
        update_theme();
    }
}

// ==================== 應用程式生命週期 ====================

static void init() {
    // 讀取儲存的設定
    s_accent_color = GColorFromHEX(persist_exists(KEY_MINUTE_COLOR) ? persist_read_int(KEY_MINUTE_COLOR) : 0xFFAA00);
    
    // 所有平台預設使用深色主題（黑色背景）
    s_is_dark_theme = persist_exists(KEY_THEME_IS_DARK) ? persist_read_bool(KEY_THEME_IS_DARK) : true;

    s_main_window = window_create();
    window_set_background_color(s_main_window, s_is_dark_theme ? GColorBlack : GColorWhite);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });

    window_stack_push(s_main_window, true);

    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    app_message_register_inbox_received(inbox_received_handler);
    app_message_open(128, 128);
}

static void deinit() {
    tick_timer_service_unsubscribe();
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
