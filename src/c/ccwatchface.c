#include <pebble.h>

// ==================== 結構定義 ====================

// 顯示圖層結構
typedef struct {
    BitmapLayer *layer;
    GBitmap *bitmap;
    uint32_t current_resource_id;
    PropertyAnimation *animation;
} DisplayLayer;

// 圖層配置結構（用於減少重複程式碼）
typedef struct {
    int x;
    int y;
    int w;
    int h;
    DisplayLayer *display_layer;
    bool animated;
    uint32_t fixed_resource_id; // 0 表示非固定圖層
} LayerConfig;

// ==================== 全域變數 & 設定 Key ====================

// UI
static Window *s_main_window;
static GColor s_accent_color;
static bool s_is_dark_theme;
static bool s_bw_accent_off;

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
    KEY_BW_ACCENT_OFF = 2,
};

// ==================== 平台專屬佈局常數 ====================

#define ANIMATION_DURATION_MS 300
#define FADE_OUT_DISTANCE 5

// 定義 palette 大小常數（避免魔數）
#if defined(PBL_COLOR)
    #define PALETTE_SIZE 8
#else
    #define PALETTE_SIZE 4
#endif

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
static void update_time(struct tm *tick_time);
static void update_date(struct tm *tick_time);

// ==================== 主題 & 顏色邏輯 ====================

/**
 * 根據平台和主題設定，套用點陣圖的顏色
 */
static void apply_theme_to_layer(DisplayLayer *display_layer, GBitmap *bitmap) {
    if (!display_layer || !bitmap) return;

    GColor *palette = gbitmap_get_palette(bitmap);
    if (!palette) return;

#if defined(PBL_COLOR)
    // --- 彩色平台邏輯 ---
    GColor text_color = s_is_dark_theme ? GColorWhite : GColorBlack;

    if (display_layer == &s_hour_layers[0] || display_layer == &s_hour_layers[1]) {
        for (int i = 0; i < PALETTE_SIZE; i++) {
            if (gcolor_equal(palette[i], GColorRed)) {
                palette[i] = s_accent_color;
            } else if (gcolor_equal(palette[i], GColorBlack)) {
                palette[i] = text_color;
            }
        }
    } else if (display_layer == &s_minute_layers[1]) {
        for (int i = 0; i < PALETTE_SIZE; i++) {
            if (gcolor_equal(palette[i], GColorBlack)) {
                palette[i] = s_accent_color;
                break;
            }
        }
    } else {
        for (int i = 0; i < PALETTE_SIZE; i++) {
            if (gcolor_equal(palette[i], GColorBlack)) {
                palette[i] = text_color;
                break;
            }
        }
    }
#else
    // --- 黑白平台邏輯 ---
    GColor main_color = s_is_dark_theme ? GColorWhite : GColorBlack;
    GColor accent_color = s_is_dark_theme ? GColorBlack : GColorWhite;

    if (s_bw_accent_off) {
        accent_color = main_color;
    }

    for (int i = 0; i < PALETTE_SIZE; i++) {
        if (gcolor_equal(palette[i], GColorBlack)) {
            palette[i] = main_color;
        } else if (gcolor_equal(palette[i], GColorWhite)) {
            palette[i] = accent_color;
        }
    }
#endif
}

// ==================== 動畫 & 圖層管理 ====================

static void fade_in_stopped_handler(Animation *animation, bool finished, void *context) {
    DisplayLayer *dl = (DisplayLayer *)context;
    if (!dl) return;
    
    if (dl->animation) {
        property_animation_destroy(dl->animation);
        dl->animation = NULL;
    }
}

static void fade_out_stopped_handler(Animation *animation, bool finished, void *context) {
    DisplayLayer *dl = (DisplayLayer *)context;
    if (!dl) return;
    
    // 無論動畫是否完成，都要清理動畫物件
    if (dl->animation) {
        property_animation_destroy(dl->animation);
        dl->animation = NULL;
    }
    
    // 只有在動畫正常完成時才繼續淡入
    if (!finished) return;
    
    Layer *l = bitmap_layer_get_layer(dl->layer);
    if (!l) return;
    
    if (dl->bitmap) {
        gbitmap_destroy(dl->bitmap);
        dl->bitmap = NULL;
    }
    
    uint32_t new_res_id = dl->current_resource_id;
    if (new_res_id != 0) {
        dl->bitmap = gbitmap_create_with_resource(new_res_id);
        if (!dl->bitmap) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create bitmap for resource: %lu", new_res_id);
            return;
        }
    }
    
    apply_theme_to_layer(dl, dl->bitmap);
    bitmap_layer_set_bitmap(dl->layer, dl->bitmap);
    
    GRect current_frame = layer_get_frame(l);
    GRect target_frame = current_frame;
    target_frame.origin.y -= FADE_OUT_DISTANCE;
    
    dl->animation = property_animation_create_layer_frame(l, &current_frame, &target_frame);
    if (!dl->animation) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create fade-in animation");
        return;
    }
    
    animation_set_duration((Animation *)dl->animation, ANIMATION_DURATION_MS / 2);
    animation_set_curve((Animation *)dl->animation, AnimationCurveEaseOut);
    animation_set_handlers((Animation *)dl->animation, (AnimationHandlers){.stopped = fade_in_stopped_handler}, dl);
    animation_schedule((Animation *)dl->animation);
}

// 安全取消動畫（避免競態條件）
static void cancel_animation(DisplayLayer *display_layer) {
    if (!display_layer) return;
    
    if (display_layer->animation) {
        Animation *anim = (Animation *)display_layer->animation;
        display_layer->animation = NULL;  // 先清空，避免 callback 重複處理
        animation_unschedule(anim);
        property_animation_destroy(anim);
    }
}

static void set_display_layer_bitmap_animated(DisplayLayer *display_layer, uint32_t resource_id) {
    if (!display_layer || !display_layer->layer) return;
    if (display_layer->current_resource_id == resource_id) return;

    cancel_animation(display_layer);

    Layer *layer = bitmap_layer_get_layer(display_layer->layer);
    GRect current_frame = layer_get_frame(layer);
    
    if (display_layer->current_resource_id == 0) { // First time setup
        if (display_layer->bitmap) {
            gbitmap_destroy(display_layer->bitmap);
            display_layer->bitmap = NULL;
        }
        
        if (resource_id != 0) {
            display_layer->bitmap = gbitmap_create_with_resource(resource_id);
            if (!display_layer->bitmap) {
                APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load resource: %lu", resource_id);
                return;
            }
        }
        
        apply_theme_to_layer(display_layer, display_layer->bitmap);
        bitmap_layer_set_bitmap(display_layer->layer, display_layer->bitmap);
        display_layer->current_resource_id = resource_id;
        
        GRect target_frame = current_frame;
        target_frame.origin.y -= FADE_OUT_DISTANCE;
        display_layer->animation = property_animation_create_layer_frame(layer, &current_frame, &target_frame);
        
        if (display_layer->animation) {
            animation_set_duration((Animation *)display_layer->animation, ANIMATION_DURATION_MS / 2);
            animation_set_handlers((Animation *)display_layer->animation, (AnimationHandlers){.stopped = fade_in_stopped_handler}, display_layer);
            animation_schedule((Animation *)display_layer->animation);
        }
        return;
    }

    display_layer->current_resource_id = resource_id;
    
    GRect fade_out_frame = current_frame;
    fade_out_frame.origin.y += FADE_OUT_DISTANCE;
    
    display_layer->animation = property_animation_create_layer_frame(layer, &current_frame, &fade_out_frame);
    if (display_layer->animation) {
        animation_set_duration((Animation *)display_layer->animation, ANIMATION_DURATION_MS / 2);
        animation_set_curve((Animation *)display_layer->animation, AnimationCurveEaseIn);
        animation_set_handlers((Animation *)display_layer->animation, (AnimationHandlers){.stopped = fade_out_stopped_handler}, display_layer);
        animation_schedule((Animation *)display_layer->animation);
    }
}

static void set_display_layer_bitmap(DisplayLayer *display_layer, uint32_t resource_id) {
    if (!display_layer) return;
    
    if (display_layer->bitmap) {
        gbitmap_destroy(display_layer->bitmap);
        display_layer->bitmap = NULL;
    }
    
    if (resource_id != 0) {
        display_layer->bitmap = gbitmap_create_with_resource(resource_id);
        if (!display_layer->bitmap) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load fixed resource: %lu", resource_id);
            return;
        }
    }
    
    apply_theme_to_layer(display_layer, display_layer->bitmap);
    bitmap_layer_set_bitmap(display_layer->layer, display_layer->bitmap);
    display_layer->current_resource_id = resource_id;
}

// ==================== 時間 & 日期更新 ====================

static void update_time(struct tm *tick_time) {
    if (!tick_time) return;

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
    if (!tick_time) return;

    int month = tick_time->tm_mon + 1;
    int day = tick_time->tm_mday;
    int week = tick_time->tm_wday;

    uint32_t month_tens_res_id = 0;
    uint32_t month_ones_res_id = DATE_UPPERCASE_ONES_RESOURCES[month % 10];
    if (month >= 10) {
        month_tens_res_id = RESOURCE_ID_IMG_SU10;
    }

    int d1 = day / 10;
    int d2 = day % 10;

    uint32_t day_tens_res_id = 0;
    uint32_t day_ones_res_id = DATE_LOWERCASE_ONES_RESOURCES[d2];

    if (day >= 10) {
        day_tens_res_id = (d2 == 0) ? DATE_LOWERCASE_ONES_RESOURCES[d1] : DATE_LOWERCASE_TENS_RESOURCES[d1];
    }
            
    uint32_t week_res_id = (week == 0) ? RESOURCE_ID_IMG_RI : DATE_LOWERCASE_ONES_RESOURCES[week];
    set_display_layer_bitmap_animated(&s_month_layers[0], month_tens_res_id);
    set_display_layer_bitmap_animated(&s_month_layers[1], month_ones_res_id);
    set_display_layer_bitmap_animated(&s_day_layers[0], day_tens_res_id);
    set_display_layer_bitmap_animated(&s_day_layers[1], day_ones_res_id);
    set_display_layer_bitmap_animated(&s_week_layer, week_res_id);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time(tick_time);
    if (units_changed & DAY_UNIT) {
        update_date(tick_time);
    }
}

// ==================== UI 畫面建構 & 銷毀 ====================

static void create_display_layer(Layer *parent, GRect bounds, DisplayLayer *dl, bool animated) {
    if (!parent || !dl) return;
    
    if (animated) {
        bounds.origin.y += FADE_OUT_DISTANCE;
    }
    
    dl->layer = bitmap_layer_create(bounds);
    if (!dl->layer) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create bitmap layer");
        return;
    }
    
    bitmap_layer_set_background_color(dl->layer, GColorClear);
    bitmap_layer_set_compositing_mode(dl->layer, GCompOpSet);
    layer_add_child(parent, bitmap_layer_get_layer(dl->layer));
    
    dl->bitmap = NULL;
    dl->current_resource_id = 0;
    dl->animation = NULL;
}

static void destroy_display_layer(DisplayLayer *dl) {
    if (!dl) return;
    
    cancel_animation(dl);
    
    if (dl->bitmap) {
        gbitmap_destroy(dl->bitmap);
        dl->bitmap = NULL;
    }
    
    if (dl->layer) {
        bitmap_layer_destroy(dl->layer);
        dl->layer = NULL;
    }
    
    dl->current_resource_id = 0;
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);

    // 定義所有圖層配置（減少重複程式碼）
    LayerConfig layer_configs[] = {
        // 時間圖層
        { TIME_COL1_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h, &s_hour_layers[0], true, 0 },
        { TIME_COL2_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h, &s_hour_layers[1], true, 0 },
        { TIME_COL1_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h, &s_minute_layers[0], true, 0 },
        { TIME_COL2_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h, &s_minute_layers[1], true, 0 },
        // 日期圖層
        { DATE_MONTH1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h, &s_month_layers[0], true, 0 },
        { DATE_MONTH2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h, &s_month_layers[1], true, 0 },
        { DATE_YUE_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h, &s_yue_layer, false, RESOURCE_ID_IMG_YUE },
        { DATE_DAY1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h, &s_day_layers[0], true, 0 },
        { DATE_DAY2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h, &s_day_layers[1], true, 0 },
        { DATE_RI_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h, &s_ri_layer, false, RESOURCE_ID_IMG_RI },
        { DATE_ZHOU_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h, &s_zhou_layer, false, RESOURCE_ID_IMG_ZHOU },
        { DATE_WEEK_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h, &s_week_layer, true, 0 },
    };

    // 統一建立所有圖層
    for (size_t i = 0; i < ARRAY_LENGTH(layer_configs); i++) {
        LayerConfig *config = &layer_configs[i];
        create_display_layer(window_layer, 
                           GRect(config->x, config->y, config->w, config->h),
                           config->display_layer,
                           config->animated);
        
        // 如果是固定圖層，立即載入 bitmap（記憶體效率優化）
        if (config->fixed_resource_id != 0) {
            set_display_layer_bitmap(config->display_layer, config->fixed_resource_id);
        }
    }

    // 初次更新時間和日期
    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    update_time(current_time);
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
    if (!dl || !dl->layer || !dl->bitmap) return;
    
    apply_theme_to_layer(dl, dl->bitmap);
    layer_mark_dirty(bitmap_layer_get_layer(dl->layer));
}

static void update_theme() {
    // 隱藏窗口以避免視覺閃爍
    Layer *root_layer = window_get_root_layer(s_main_window);
    layer_set_hidden(root_layer, true);
    
    window_set_background_color(s_main_window, s_is_dark_theme ? GColorBlack : GColorWhite);

    DisplayLayer* all_layers[] = {
        &s_hour_layers[0], &s_hour_layers[1], &s_minute_layers[0], &s_minute_layers[1],
        &s_month_layers[0], &s_month_layers[1], &s_day_layers[0], &s_day_layers[1],
        &s_week_layer, &s_yue_layer, &s_ri_layer, &s_zhou_layer
    };
    for (size_t i = 0; i < ARRAY_LENGTH(all_layers); i++) {
        refresh_layer_theme(all_layers[i]);
    }
    
    // 重新顯示窗口
    layer_set_hidden(root_layer, false);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    if (!iter) return;
    
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

    #if defined(PBL_BW)
    Tuple *bw_accent_off_t = dict_find(iter, KEY_BW_ACCENT_OFF);
    if (bw_accent_off_t) {
        s_bw_accent_off = bw_accent_off_t->value->int32 == 1;
        persist_write_bool(KEY_BW_ACCENT_OFF, s_bw_accent_off);
        theme_changed = true;
    }
    #endif

    if (theme_changed) {
        update_theme();
    }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! Reason: %d", (int)reason);
}

static void outbox_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed! Reason: %d", (int)reason);
}

// ==================== 應用程式生命週期 ====================

static void init() {
    // 讀取儲存的設定（優化 persist_exists 檢查）
    int stored_color = persist_read_int(KEY_MINUTE_COLOR);
    s_accent_color = GColorFromHEX(stored_color != 0 ? stored_color : 0xFFAA00);
    
    s_is_dark_theme = persist_read_bool(KEY_THEME_IS_DARK);
    if (!persist_exists(KEY_THEME_IS_DARK)) {
        s_is_dark_theme = true; // 預設為深色主題
    }
    
    #if defined(PBL_BW)
    s_bw_accent_off = persist_read_bool(KEY_BW_ACCENT_OFF);
    #endif

    s_main_window = window_create();
    if (!s_main_window) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create main window");
        return;
    }
    
    window_set_background_color(s_main_window, s_is_dark_theme ? GColorBlack : GColorWhite);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });

    window_stack_push(s_main_window, true);

    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    // 註冊 AppMessage handlers（包含錯誤處理）
    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    
    AppMessageResult open_result = app_message_open(128, 128);
    if (open_result != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to open AppMessage: %d", (int)open_result);
    }
}

static void deinit() {
    tick_timer_service_unsubscribe();
    app_message_deregister_callbacks();
    
    if (s_main_window) {
        window_destroy(s_main_window);
        s_main_window = NULL;
    }
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
