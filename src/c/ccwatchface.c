#include <pebble.h>

// ==================== 常數定義 ====================

// 動畫參數
#define ANIMATION_DURATION_MS 300
#define ANIMATION_OFFSET_Y 5

// 調色盤大小
#if defined(PBL_COLOR)
    #define PALETTE_SIZE 8
#else
    #define PALETTE_SIZE 4
#endif

// 特殊資源 ID 標記
#define RESOURCE_ID_NONE 0

// 平台相關佈局
#if defined(PBL_PLATFORM_EMERY)
    #define TIME_IMAGE_SIZE GSize(88, 88)
    #define DATE_IMAGE_SIZE GSize(22, 22)
    #define TIME_COL1_X 8
    #define TIME_COL2_X 104
    #define TIME_ROW1_Y 8
    #define TIME_ROW2_Y 104
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
    #define TIME_IMAGE_SIZE GSize(66, 66)
    #define DATE_IMAGE_SIZE GSize(11, 11)
    #define TIME_COL1_X 4
    #define TIME_COL2_X 74
    #define TIME_ROW1_Y 4
    #define TIME_ROW2_Y 74
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

// ==================== 列舉與類型定義 ====================

// 設定鍵值
typedef enum {
    KEY_MINUTE_COLOR = 1,
    KEY_THEME_IS_DARK = 2,
    KEY_HOUR_COLOR = 0,
    KEY_ANIMATION_ENABLED = 3,
    KEY_BACKGROUND_COLOR = 4,
    KEY_TEXT_COLOR = 5,
    KEY_BW_HOUR_ACCENT = 6,

} SettingKey;

// 圖層類型（用於主題應用）
typedef enum {
    LAYER_TYPE_HOUR,
    LAYER_TYPE_MINUTE_ACCENT,
    LAYER_TYPE_MINUTE_NORMAL,
    LAYER_TYPE_DATE,
    LAYER_TYPE_STATIC,
} LayerType;

// 動畫狀態
typedef enum {
    ANIM_STATE_IDLE,
    ANIM_STATE_FADE_OUT,
    ANIM_STATE_FADE_IN,
} AnimationState;

// 顯示圖層結構
typedef struct {
    BitmapLayer *layer;
    GBitmap *bitmap;
    uint32_t current_resource_id;
    PropertyAnimation *animation;
    AnimationState anim_state;
    GRect base_frame;
    LayerType type;
} DisplayLayer;

// 主題配置
typedef struct {
    GColor background;
    GColor text;
    GColor hour_accent;
    GColor minute_accent;
    // B&W Platform Settings
    bool is_dark;
    bool bw_hour_accent;

    // bool bw_minute_accent; // Removed
} ThemeConfig;

// 應用狀態
typedef struct {
    Window *main_window;
    ThemeConfig theme;
    bool animation_enabled;

    DisplayLayer hour_layers[2];
    DisplayLayer minute_layers[2];
    DisplayLayer month_layers[2];
    DisplayLayer day_layers[2];
    DisplayLayer week_layer;
    DisplayLayer yue_layer;
    DisplayLayer ri_layer;
    DisplayLayer zhou_layer;
} AppState;

// ==================== 全域狀態 ====================

static AppState s_app;

// ==================== 資源映射表 ====================

static const uint32_t TIME_UPPERCASE_TENS_RESOURCES[] = {
    RESOURCE_ID_NONE, RESOURCE_ID_IMG_U10, RESOURCE_ID_IMG_U2,
};

static const uint32_t TIME_UPPERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_U10, RESOURCE_ID_IMG_U1, RESOURCE_ID_IMG_U2, RESOURCE_ID_IMG_U3,
    RESOURCE_ID_IMG_U4, RESOURCE_ID_IMG_U5, RESOURCE_ID_IMG_U6, RESOURCE_ID_IMG_U7,
    RESOURCE_ID_IMG_U8, RESOURCE_ID_IMG_U9
};

static const uint32_t TIME_LOWERCASE_TENS_RESOURCES[] = {
    RESOURCE_ID_IMG_L0, RESOURCE_ID_IMG_L10, RESOURCE_ID_IMG_L20, RESOURCE_ID_IMG_L30,
    RESOURCE_ID_IMG_L4, RESOURCE_ID_IMG_L5,
};

static const uint32_t TIME_LOWERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_L10, RESOURCE_ID_IMG_L1, RESOURCE_ID_IMG_L2, RESOURCE_ID_IMG_L3,
    RESOURCE_ID_IMG_L4, RESOURCE_ID_IMG_L5, RESOURCE_ID_IMG_L6, RESOURCE_ID_IMG_L7,
    RESOURCE_ID_IMG_L8, RESOURCE_ID_IMG_L9
};

static const uint32_t DATE_UPPERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_SU10, RESOURCE_ID_IMG_SU1, RESOURCE_ID_IMG_SU2, RESOURCE_ID_IMG_SU3,
    RESOURCE_ID_IMG_SU4, RESOURCE_ID_IMG_SU5, RESOURCE_ID_IMG_SU6, RESOURCE_ID_IMG_SU7,
    RESOURCE_ID_IMG_SU8, RESOURCE_ID_IMG_SU9
};

static const uint32_t DATE_LOWERCASE_TENS_RESOURCES[] = {
    RESOURCE_ID_NONE, RESOURCE_ID_IMG_SL10, RESOURCE_ID_IMG_SL20, RESOURCE_ID_IMG_SL30,
};

static const uint32_t DATE_LOWERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_SL10, RESOURCE_ID_IMG_SL1, RESOURCE_ID_IMG_SL2, RESOURCE_ID_IMG_SL3,
    RESOURCE_ID_IMG_SL4, RESOURCE_ID_IMG_SL5, RESOURCE_ID_IMG_SL6, RESOURCE_ID_IMG_SL7,
    RESOURCE_ID_IMG_SL8, RESOURCE_ID_IMG_SL9,
};

// ==================== 主題系統 ====================

static void theme_resolve_colors(ThemeConfig *theme) {
#if defined(PBL_COLOR)
    // On Color platforms, colors are set directly via AppMessage
#else
    theme->background = theme->is_dark ? GColorBlack : GColorWhite;
    theme->text = theme->is_dark ? GColorWhite : GColorBlack;
    
    // Hour Accent: Follows Background (Invisible/Masked) or Text (Visible)
    theme->hour_accent = theme->bw_hour_accent ? theme->background : theme->text;
    
    // Minute Accent: Always Text Color (Disabled) on B&W
    theme->minute_accent = theme->text;
#endif
}

static void theme_init_defaults(ThemeConfig *theme) {
#if defined(PBL_COLOR)
    theme->background = GColorBlack;
    theme->text = GColorWhite;
    theme->hour_accent = GColorChromeYellow;
    theme->minute_accent = GColorChromeYellow;
#else
    theme->is_dark = true;
    theme->bw_hour_accent = true;
    theme_resolve_colors(theme);
#endif
}

static void theme_load_from_storage(ThemeConfig *theme) {
    theme_init_defaults(theme);

#if defined(PBL_COLOR)
    if (persist_exists(KEY_BACKGROUND_COLOR)) {
        theme->background = GColorFromHEX(persist_read_int(KEY_BACKGROUND_COLOR));
    }
    if (persist_exists(KEY_TEXT_COLOR)) {
        theme->text = GColorFromHEX(persist_read_int(KEY_TEXT_COLOR));
    }
    if (persist_exists(KEY_HOUR_COLOR)) {
        theme->hour_accent = GColorFromHEX(persist_read_int(KEY_HOUR_COLOR));
    }
    if (persist_exists(KEY_MINUTE_COLOR)) {
        theme->minute_accent = GColorFromHEX(persist_read_int(KEY_MINUTE_COLOR));
    }
#else
    if (persist_exists(KEY_THEME_IS_DARK)) {
        theme->is_dark = persist_read_bool(KEY_THEME_IS_DARK);
    }
    if (persist_exists(KEY_BW_HOUR_ACCENT)) {
        theme->bw_hour_accent = persist_read_bool(KEY_BW_HOUR_ACCENT);
    }
    theme_resolve_colors(theme);
#endif
}

static void theme_apply_to_bitmap(const ThemeConfig *theme, GBitmap *bitmap, LayerType type) {
    if (!bitmap) return;

    GColor *palette = gbitmap_get_palette(bitmap);
    if (!palette) {
        // 安全檢查：Aplite 上的 1-bit 圖可能無調色盤，避免崩潰
        return;
    }

    GColor accent_color = (type == LAYER_TYPE_HOUR) ? theme->hour_accent :
                          (type == LAYER_TYPE_MINUTE_ACCENT) ? theme->minute_accent :
                          theme->text;

    for (int i = 0; i < PALETTE_SIZE; i++) {
#if defined(PBL_COLOR)
        if (gcolor_equal(palette[i], GColorRed)) {
            palette[i] = accent_color;
        } else if (gcolor_equal(palette[i], GColorBlack)) {
            palette[i] = (type == LAYER_TYPE_MINUTE_ACCENT) ? accent_color : theme->text;
        }
#else
        if (gcolor_equal(palette[i], GColorBlack)) {
            palette[i] = theme->text;
        } else if (gcolor_equal(palette[i], GColorWhite)) {
            palette[i] = accent_color;
        }
#endif
    }
}

// ==================== 圖層管理系統 ====================

static void display_layer_init(DisplayLayer *dl, Layer *parent, GRect frame, LayerType type) {
    if (!dl || !parent) return;

    memset(dl, 0, sizeof(DisplayLayer));

    dl->base_frame = frame;
    dl->type = type;
    dl->anim_state = ANIM_STATE_IDLE;

    dl->layer = bitmap_layer_create(frame);
    if (!dl->layer) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create bitmap layer");
        return;
    }

    bitmap_layer_set_background_color(dl->layer, GColorClear);
    bitmap_layer_set_compositing_mode(dl->layer, GCompOpSet);
    layer_add_child(parent, bitmap_layer_get_layer(dl->layer));
}

static void display_layer_cleanup_animation(DisplayLayer *dl) {
    if (!dl) return;

    if (dl->animation) {
        animation_unschedule((Animation *)dl->animation);
        property_animation_destroy(dl->animation);
        dl->animation = NULL;
    }
    dl->anim_state = ANIM_STATE_IDLE;
}

static void display_layer_load_resource(DisplayLayer *dl, uint32_t resource_id) {
    if (!dl) return;

    if (dl->bitmap) {
        gbitmap_destroy(dl->bitmap);
        dl->bitmap = NULL;
    }

    if (resource_id != RESOURCE_ID_NONE) {
        dl->bitmap = gbitmap_create_with_resource(resource_id);
        if (!dl->bitmap) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load resource: %lu", resource_id);
            return;
        }
        theme_apply_to_bitmap(&s_app.theme, dl->bitmap, dl->type);
    }

    if (dl->layer) {
        bitmap_layer_set_bitmap(dl->layer, dl->bitmap);
    }
}

static void display_layer_set_position(DisplayLayer *dl, bool offset_for_animation) {
    if (!dl || !dl->layer) return;

    GRect frame = dl->base_frame;
    if (offset_for_animation && s_app.animation_enabled) {
        frame.origin.y += ANIMATION_OFFSET_Y;
    }

    layer_set_frame(bitmap_layer_get_layer(dl->layer), frame);
}

static void display_layer_deinit(DisplayLayer *dl) {
    if (!dl) return;

    display_layer_cleanup_animation(dl);

    if (dl->bitmap) {
        gbitmap_destroy(dl->bitmap);
        dl->bitmap = NULL;
    }
    
    if (dl->layer) {
        bitmap_layer_destroy(dl->layer);
        dl->layer = NULL;
    }

    dl->current_resource_id = RESOURCE_ID_NONE;
    dl->anim_state = ANIM_STATE_IDLE;
}

// ==================== 圖層遍歷系統 ====================

typedef void (*LayerIteratorCallback)(DisplayLayer *dl, void *context);

static void iterate_all_layers(LayerIteratorCallback callback, void *context) {
    if (!callback) return;

    DisplayLayer *all_layers[] = {
        &s_app.hour_layers[0], &s_app.hour_layers[1],
        &s_app.minute_layers[0], &s_app.minute_layers[1],
        &s_app.month_layers[0], &s_app.month_layers[1],
        &s_app.day_layers[0], &s_app.day_layers[1],
        &s_app.week_layer, &s_app.yue_layer, &s_app.ri_layer, &s_app.zhou_layer
    };

    for (size_t i = 0; i < ARRAY_LENGTH(all_layers); i++) {
        if (all_layers[i]) {
            callback(all_layers[i], context);
        }
    }
}

static void iterate_animated_layers(LayerIteratorCallback callback, void *context) {
    if (!callback) return;

    DisplayLayer *animated_layers[] = {
        &s_app.hour_layers[0], &s_app.hour_layers[1],
        &s_app.minute_layers[0], &s_app.minute_layers[1],
        &s_app.month_layers[0], &s_app.month_layers[1],
        &s_app.day_layers[0], &s_app.day_layers[1],
        &s_app.week_layer
    };

    for (size_t i = 0; i < ARRAY_LENGTH(animated_layers); i++) {
        if (animated_layers[i]) {
            callback(animated_layers[i], context);
        }
    }
}

// ==================== 遍歷回調函數 (Callbacks) ====================

// 用於 teardown
static void teardown_layer_cb(DisplayLayer *dl, void *context) {
    display_layer_deinit(dl);
}

// 用於主題更新
static void refresh_theme_cb(DisplayLayer *dl, void *context) {
    if (dl->bitmap) {
        theme_apply_to_bitmap(&s_app.theme, dl->bitmap, dl->type);
        if (dl->layer) {
            layer_mark_dirty(bitmap_layer_get_layer(dl->layer));
        }
    }
}

// 用於動畫開關設定
static void set_anim_pos_cb(DisplayLayer *dl, void *context) {
    display_layer_set_position(dl, s_app.animation_enabled);
}

// ==================== 動畫系統 ====================

static void anim_fade_in_stopped(Animation *anim, bool finished, void *context) {
    DisplayLayer *dl = (DisplayLayer *)context;
    if (!dl || !dl->layer) return;

    display_layer_cleanup_animation(dl);
}

static void anim_fade_out_stopped(Animation *anim, bool finished, void *context) {
    DisplayLayer *dl = (DisplayLayer *)context;
    if (!dl || !dl->layer) {
        if (dl) display_layer_cleanup_animation(dl);
        return;
    }

    if (!finished) {
        display_layer_cleanup_animation(dl);
        return;
    }

    Layer *layer = bitmap_layer_get_layer(dl->layer);
    if (!layer) return;

    display_layer_load_resource(dl, dl->current_resource_id);

    GRect from = layer_get_frame(layer);
    GRect to = dl->base_frame;

    dl->animation = property_animation_create_layer_frame(layer, &from, &to);
    if (!dl->animation) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create fade-in animation");
        return;
    }

    dl->anim_state = ANIM_STATE_FADE_IN;
    animation_set_duration((Animation *)dl->animation, ANIMATION_DURATION_MS / 2);
    animation_set_curve((Animation *)dl->animation, AnimationCurveEaseOut);
    animation_set_handlers((Animation *)dl->animation,
                           (AnimationHandlers){.stopped = anim_fade_in_stopped}, dl);
    animation_schedule((Animation *)dl->animation);
}

static void display_layer_update_animated(DisplayLayer *dl, uint32_t resource_id) {
    if (!dl || !dl->layer) return;

    display_layer_cleanup_animation(dl);

    Layer *layer = bitmap_layer_get_layer(dl->layer);
    GRect from = layer_get_frame(layer);

    if (dl->current_resource_id == RESOURCE_ID_NONE) {
        display_layer_load_resource(dl, resource_id);
        dl->current_resource_id = resource_id;
        
        GRect to = dl->base_frame;
        dl->animation = property_animation_create_layer_frame(layer, &from, &to);
        
        if (dl->animation) {
            dl->anim_state = ANIM_STATE_FADE_IN;
            animation_set_duration((Animation *)dl->animation, ANIMATION_DURATION_MS / 2);
            animation_set_handlers((Animation *)dl->animation,
                                  (AnimationHandlers){.stopped = anim_fade_in_stopped}, dl);
            animation_schedule((Animation *)dl->animation);
        }
        return;
    }
    
    dl->current_resource_id = resource_id;

    GRect to = from;
    to.origin.y += ANIMATION_OFFSET_Y;
    
    dl->animation = property_animation_create_layer_frame(layer, &from, &to);
    if (dl->animation) {
        dl->anim_state = ANIM_STATE_FADE_OUT;
        animation_set_duration((Animation *)dl->animation, ANIMATION_DURATION_MS / 2);
        animation_set_curve((Animation *)dl->animation, AnimationCurveEaseIn);
        animation_set_handlers((Animation *)dl->animation,
                              (AnimationHandlers){.stopped = anim_fade_out_stopped}, dl);
        animation_schedule((Animation *)dl->animation);
    }
}

static void display_layer_update_static(DisplayLayer *dl, uint32_t resource_id) {
    if (!dl || !dl->layer) return;

    display_layer_cleanup_animation(dl);
    display_layer_set_position(dl, false);
    display_layer_load_resource(dl, resource_id);
    dl->current_resource_id = resource_id;
}

static void display_layer_update(DisplayLayer *dl, uint32_t resource_id) {
    if (!dl || dl->current_resource_id == resource_id) return;

    if (s_app.animation_enabled) {
        display_layer_update_animated(dl, resource_id);
    } else {
        display_layer_update_static(dl, resource_id);
    }
}

// ==================== 時間更新邏輯 ====================

static void update_time_display(struct tm *tick_time) {
    if (!tick_time) return;

    int hour = tick_time->tm_hour;
    if (!clock_is_24h_style()) {
        hour = hour % 12;
        if (hour == 0) hour = 12;
    }

    uint32_t hour_tens = RESOURCE_ID_NONE;
    uint32_t hour_ones = (hour == 0) ? RESOURCE_ID_IMG_U0 :
                          TIME_UPPERCASE_ONES_RESOURCES[hour % 10];

    if (hour > 10 && hour / 10 < (int)ARRAY_LENGTH(TIME_UPPERCASE_TENS_RESOURCES)) {
        hour_tens = TIME_UPPERCASE_TENS_RESOURCES[hour / 10];
    }

    display_layer_update(&s_app.hour_layers[0], hour_tens);
    display_layer_update(&s_app.hour_layers[1], hour_ones);

    int minute = tick_time->tm_min;
    uint32_t minute_tens = RESOURCE_ID_NONE;
    uint32_t minute_ones = RESOURCE_ID_NONE;

    if (minute == 0) {
        minute_tens = RESOURCE_ID_IMG_DIAN;
        minute_ones = RESOURCE_ID_IMG_ZHENG;
    } else if (minute == 30) {
        minute_tens = RESOURCE_ID_IMG_DIAN;
        minute_ones = RESOURCE_ID_IMG_BAN;
    } else if (minute == 10) {
        minute_tens = RESOURCE_ID_IMG_L1;
        minute_ones = RESOURCE_ID_IMG_L0;
    } else {
        int m1 = minute / 10;
        int m2 = minute % 10;
        minute_tens = (m2 == 0) ? TIME_LOWERCASE_ONES_RESOURCES[m1] :
                      TIME_LOWERCASE_TENS_RESOURCES[m1];
        minute_ones = TIME_LOWERCASE_ONES_RESOURCES[m2];
    }

    display_layer_update(&s_app.minute_layers[0], minute_tens);
    display_layer_update(&s_app.minute_layers[1], minute_ones);
}

static void update_date_display(struct tm *tick_time) {
    if (!tick_time) return;

    int month = tick_time->tm_mon + 1;
    int day = tick_time->tm_mday;
    int week = tick_time->tm_wday;

    uint32_t month_tens = (month > 10) ? RESOURCE_ID_IMG_SU10 : RESOURCE_ID_NONE;
    uint32_t month_ones = DATE_UPPERCASE_ONES_RESOURCES[month % 10];

    int d1 = day / 10;
    int d2 = day % 10;
    uint32_t day_tens = RESOURCE_ID_NONE;
    uint32_t day_ones = RESOURCE_ID_NONE;

    if (d2 < (int)ARRAY_LENGTH(DATE_LOWERCASE_ONES_RESOURCES)) {
        day_ones = DATE_LOWERCASE_ONES_RESOURCES[d2];
    }

    if (day > 10) {
        if (d2 == 0 && d1 < (int)ARRAY_LENGTH(DATE_LOWERCASE_ONES_RESOURCES)) {
            day_tens = DATE_LOWERCASE_ONES_RESOURCES[d1];
        } else if (d1 < (int)ARRAY_LENGTH(DATE_LOWERCASE_TENS_RESOURCES)) {
            day_tens = DATE_LOWERCASE_TENS_RESOURCES[d1];
        }
    }

    uint32_t week_res = (week == 0) ? RESOURCE_ID_IMG_RI :
                        DATE_LOWERCASE_ONES_RESOURCES[week];

    display_layer_update(&s_app.month_layers[0], month_tens);
    display_layer_update(&s_app.month_layers[1], month_ones);
    display_layer_update(&s_app.day_layers[0], day_tens);
    display_layer_update(&s_app.day_layers[1], day_ones);
    display_layer_update(&s_app.week_layer, week_res);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time_display(tick_time);
    if (units_changed & DAY_UNIT) {
        update_date_display(tick_time);
    }
}

// ==================== UI 構建 ====================

static void setup_all_layers(Layer *parent) {
    // 這裡保留陣列定義是合理的，因為每個圖層有不同的 frame 和 type
    DisplayLayer *layers[] = {
        &s_app.hour_layers[0], &s_app.hour_layers[1],
        &s_app.minute_layers[0], &s_app.minute_layers[1],
        &s_app.month_layers[0], &s_app.month_layers[1],
        &s_app.day_layers[0], &s_app.day_layers[1],
        &s_app.week_layer, &s_app.yue_layer, &s_app.ri_layer, &s_app.zhou_layer
    };

    GRect frames[] = {
        GRect(TIME_COL1_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h),
        GRect(TIME_COL2_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h),
        GRect(TIME_COL1_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h),
        GRect(TIME_COL2_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h),
        GRect(DATE_MONTH1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_MONTH2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_DAY1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_DAY2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_WEEK_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_YUE_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_RI_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_ZHOU_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
    };

    LayerType types[] = {
        LAYER_TYPE_HOUR, LAYER_TYPE_HOUR,
        LAYER_TYPE_MINUTE_NORMAL, LAYER_TYPE_MINUTE_ACCENT,
        LAYER_TYPE_DATE, LAYER_TYPE_DATE, LAYER_TYPE_DATE, LAYER_TYPE_DATE,
        LAYER_TYPE_DATE, LAYER_TYPE_STATIC, LAYER_TYPE_STATIC, LAYER_TYPE_STATIC
    };

    uint32_t static_resources[] = {
        RESOURCE_ID_NONE, RESOURCE_ID_NONE, RESOURCE_ID_NONE, RESOURCE_ID_NONE,
        RESOURCE_ID_NONE, RESOURCE_ID_NONE, RESOURCE_ID_NONE, RESOURCE_ID_NONE,
        RESOURCE_ID_NONE, RESOURCE_ID_IMG_YUE, RESOURCE_ID_IMG_RI, RESOURCE_ID_IMG_ZHOU
    };

    for (size_t i = 0; i < ARRAY_LENGTH(layers); i++) {
        display_layer_init(layers[i], parent, frames[i], types[i]);
        display_layer_set_position(layers[i], s_app.animation_enabled && types[i] != LAYER_TYPE_STATIC);

        if (static_resources[i] != RESOURCE_ID_NONE) {
            display_layer_load_resource(layers[i], static_resources[i]);
            layers[i]->current_resource_id = static_resources[i];
        }
    }
}

static void teardown_all_layers(void) {
    // 使用 Iterator + Callback，簡潔且不重複
    iterate_all_layers(teardown_layer_cb, NULL);
}

static void refresh_all_layer_themes(void) {
    // 使用 Iterator + Callback
    iterate_all_layers(refresh_theme_cb, NULL);
}

static void apply_theme_to_window(void) {
    Layer *root = window_get_root_layer(s_app.main_window);
    layer_set_hidden(root, true);
    
    window_set_background_color(s_app.main_window, s_app.theme.background);
    refresh_all_layer_themes();
    
    layer_set_hidden(root, false);
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    setup_all_layers(window_layer);

    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    update_time_display(current_time);
    update_date_display(current_time);
}

static void main_window_unload(Window *window) {
    teardown_all_layers();
}

// ==================== AppMessage 處理 ====================

static void handle_settings_update(DictionaryIterator *iter) {
    if (!iter) return;
    
    bool theme_changed = false;

#if defined(PBL_COLOR)
    Tuple *bg = dict_find(iter, KEY_BACKGROUND_COLOR);
    if (bg) {
        s_app.theme.background = GColorFromHEX(bg->value->int32);
        persist_write_int(KEY_BACKGROUND_COLOR, bg->value->int32);
        theme_changed = true;
    }

    Tuple *text = dict_find(iter, KEY_TEXT_COLOR);
    if (text) {
        s_app.theme.text = GColorFromHEX(text->value->int32);
        persist_write_int(KEY_TEXT_COLOR, text->value->int32);
        theme_changed = true;
    }
#else

    Tuple *dark = dict_find(iter, KEY_THEME_IS_DARK);
    if (dark) {
        s_app.theme.is_dark = dark->value->int32 == 1;
        persist_write_bool(KEY_THEME_IS_DARK, s_app.theme.is_dark);
        theme_changed = true;
    }

    Tuple *hour_bg = dict_find(iter, KEY_BW_HOUR_ACCENT);
    if (hour_bg) {
        s_app.theme.bw_hour_accent = hour_bg->value->int32 == 1;
        persist_write_bool(KEY_BW_HOUR_ACCENT, s_app.theme.bw_hour_accent);
        theme_changed = true;
    }
    
    // Resolve colors based on new settings
    if (theme_changed) {
        theme_resolve_colors(&s_app.theme);
    }
#endif

    Tuple *minute_color = dict_find(iter, KEY_MINUTE_COLOR);
    if (minute_color) {
        s_app.theme.minute_accent = GColorFromHEX(minute_color->value->int32);
        persist_write_int(KEY_MINUTE_COLOR, minute_color->value->int32);
        theme_changed = true;
    }

    Tuple *hour_color = dict_find(iter, KEY_HOUR_COLOR);
    if (hour_color) {
        s_app.theme.hour_accent = GColorFromHEX(hour_color->value->int32);
        persist_write_int(KEY_HOUR_COLOR, hour_color->value->int32);
        theme_changed = true;
    }

    if (theme_changed) {
        apply_theme_to_window();
    }

    Tuple *anim = dict_find(iter, KEY_ANIMATION_ENABLED);
    if (anim) {
        s_app.animation_enabled = anim->value->int32 == 1;
        persist_write_bool(KEY_ANIMATION_ENABLED, s_app.animation_enabled);

        // 使用 Iterator + Callback，更新動畫位置
        // 僅針對會動的圖層 (exclude static resources like yue/ri/zhou)
        iterate_animated_layers(set_anim_pos_cb, NULL);
    }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    handle_settings_update(iter);
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}

// ==================== 應用程式生命週期 ====================

static void app_init(void) {
    memset(&s_app, 0, sizeof(AppState));
    
    theme_load_from_storage(&s_app.theme);
    
    if (persist_exists(KEY_ANIMATION_ENABLED)) {
        s_app.animation_enabled = persist_read_bool(KEY_ANIMATION_ENABLED);
    } else {
        s_app.animation_enabled = true;
    }

    s_app.main_window = window_create();
    if (!s_app.main_window) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create window");
        return;
    }
    
    window_set_background_color(s_app.main_window, s_app.theme.background);
    window_set_window_handlers(s_app.main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });

    window_stack_push(s_app.main_window, true);

    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    
    AppMessageResult result = app_message_open(128, 128);
    if (result != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage open failed: %d", (int)result);
    }
}

static void app_deinit(void) {
    tick_timer_service_unsubscribe();
    app_message_deregister_callbacks();
    
    if (s_app.main_window) {
        window_destroy(s_app.main_window);
    }
}

int main(void) {
    app_init();
    app_event_loop();
    app_deinit();
}
