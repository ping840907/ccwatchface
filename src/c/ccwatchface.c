#include <pebble.h>

// ==================== 結構定義 ====================

// 顯示圖層結構，包含點陣圖層和點陣圖本身
typedef struct {
    BitmapLayer *layer;
    GBitmap *bitmap;
    uint32_t current_resource_id;
    PropertyAnimation *animation;
} DisplayLayer;

// ==================== 全域變數宣告 ====================

// UI 視窗元件
static Window *s_main_window;
static GColor s_accent_color;

// 時間顯示圖層 (小時十位/個位, 分鐘十位/個位)
static DisplayLayer s_hour_layers[2];
static DisplayLayer s_minute_layers[2];

// 日期顯示圖層 (月份十位/個位, 日期十位/個位, 星期)
static DisplayLayer s_month_layers[2];
static DisplayLayer s_day_layers[2];
static DisplayLayer s_week_layer;

// 固定文字圖層 ("月", "日", "週")
static DisplayLayer s_yue_layer;
static DisplayLayer s_ri_layer;
static DisplayLayer s_zhou_layer;

// ==================== 螢幕佈局常數定義 ====================

#define SCREEN_WIDTH 200
#define SCREEN_HEIGHT 228
#define TIME_IMAGE_SIZE GSize(88, 88)
#define DATE_IMAGE_SIZE GSize(22, 22)
#define TIME_COL1_X 8
#define TIME_COL2_X (TIME_COL1_X + TIME_IMAGE_SIZE.w + 8)
#define TIME_ROW1_Y 8
#define TIME_ROW2_Y (TIME_ROW1_Y + TIME_IMAGE_SIZE.h + 8)
#define DATE_ROW_Y (TIME_ROW2_Y + TIME_IMAGE_SIZE.h + 7)

#define ANIMATION_DURATION_MS 300
#define FADE_OUT_DISTANCE 5  // 淡出時向上移動的像素

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

// ==================== 工具函式 ====================

/**
 * 根據圖層類型修改點陣圖的調色盤
 */
static void customize_bitmap(DisplayLayer *display_layer, GBitmap *bitmap) {
    if (!bitmap) return;

    GColor *palette = gbitmap_get_palette(bitmap);
    if (!palette) return;

    // 如果是小時圖層，將紅色(#FF0000)替換為主題色
    if (display_layer == &s_hour_layers[0] || display_layer == &s_hour_layers[1]) {
        for (int i = 0; i < 8; i++) {
            if (gcolor_equal(palette[i], GColorRed)) {
                palette[i] = s_accent_color;
                break;
            }
        }
    }
    // 如果是分鐘第二個字圖層，將黑色換成主題色
    else if (display_layer == &s_minute_layers[1]) {
        for (int i = 0; i < 2; i++) {
            if (gcolor_equal(palette[i], GColorBlack)) {
                palette[i] = s_accent_color;
                break;
            }
        }
    }
}

/**
 * 淡入動畫停止回調 - 清理動畫對象
 */
static void fade_in_stopped_handler(Animation *animation, bool finished, void *context) {
    DisplayLayer *display_layer = (DisplayLayer *)context;
    
    // 清理動畫對象
    if (display_layer->animation) {
        property_animation_destroy(display_layer->animation);
        display_layer->animation = NULL;
    }
}

/**
 * 淡出動畫停止回調 - 替換圖片並開始淡入
 */
static void fade_out_stopped_handler(Animation *animation, bool finished, void *context) {
    if (!finished) return;
    
    DisplayLayer *dl = (DisplayLayer *)context;
    Layer *l = bitmap_layer_get_layer(dl->layer);
    
    // 銷毀舊圖並載入新圖
    if (dl->bitmap) {
        gbitmap_destroy(dl->bitmap);
    }
    
    uint32_t new_res_id = dl->current_resource_id; // 暫存在 current_resource_id
    dl->bitmap = (new_res_id != 0) ? gbitmap_create_with_resource(new_res_id) : NULL;
    customize_bitmap(dl, dl->bitmap);
    bitmap_layer_set_bitmap(dl->layer, dl->bitmap);
    
    // 階段2: 淡入新圖（從下方移動到正確位置）
    GRect current_frame = layer_get_frame(l);
    GRect target_frame = current_frame;
    target_frame.origin.y -= FADE_OUT_DISTANCE;  // 向上移動到正確位置
    
    PropertyAnimation *fade_in = property_animation_create_layer_frame(l, &current_frame, &target_frame);
    animation_set_duration((Animation *)fade_in, ANIMATION_DURATION_MS / 2);
    animation_set_curve((Animation *)fade_in, AnimationCurveEaseOut);
    
    animation_set_handlers((Animation *)fade_in, (AnimationHandlers){
        .stopped = fade_in_stopped_handler
    }, dl);
    
    dl->animation = fade_in;
    animation_schedule((Animation *)fade_in);
}

/**
 * 設定顯示圖層的點陣圖（帶淡入淡出動畫）
 */
static void set_display_layer_bitmap_animated(DisplayLayer *display_layer, uint32_t resource_id, GSize size) {
    // 如果要顯示的資源ID和目前的一樣，無須更新
    if (display_layer->current_resource_id == resource_id) {
        return;
    }

    // 取消正在進行的動畫
    if (display_layer->animation) {
        animation_unschedule((Animation *)display_layer->animation);
        property_animation_destroy(display_layer->animation);
        display_layer->animation = NULL;
    }

    Layer *layer = bitmap_layer_get_layer(display_layer->layer);
    GRect current_frame = layer_get_frame(layer);
    GRect target_frame = current_frame;
    target_frame.origin.y -= FADE_OUT_DISTANCE;  // 目標位置是向上偏移後的正確位置
    
    // 如果是第一次設定（沒有舊圖），使用淡入動畫
    if (display_layer->current_resource_id == 0) {
        if (display_layer->bitmap) {
            gbitmap_destroy(display_layer->bitmap);
        }
        
        display_layer->bitmap = (resource_id != 0) ? gbitmap_create_with_resource(resource_id) : NULL;
        customize_bitmap(display_layer, display_layer->bitmap);
        bitmap_layer_set_bitmap(display_layer->layer, display_layer->bitmap);
        display_layer->current_resource_id = resource_id;
        
        // 執行淡入動畫到正確位置
        PropertyAnimation *fade_in = property_animation_create_layer_frame(layer, &current_frame, &target_frame);
        animation_set_duration((Animation *)fade_in, ANIMATION_DURATION_MS / 2);
        animation_set_curve((Animation *)fade_in, AnimationCurveEaseOut);
        animation_set_handlers((Animation *)fade_in, (AnimationHandlers){
            .stopped = fade_in_stopped_handler
        }, display_layer);
        
        display_layer->animation = fade_in;
        animation_schedule((Animation *)fade_in);
        return;
    }

    // 將新的 resource_id 暫存在 current_resource_id 中，供淡出回調使用
    display_layer->current_resource_id = resource_id;
    
    // 階段1: 淡出舊圖（向下移動回初始位置）
    GRect fade_out_frame = current_frame;
    fade_out_frame.origin.y += FADE_OUT_DISTANCE;  // 向下移動
    
    PropertyAnimation *fade_out = property_animation_create_layer_frame(layer, &current_frame, &fade_out_frame);
    animation_set_duration((Animation *)fade_out, ANIMATION_DURATION_MS / 2);
    animation_set_curve((Animation *)fade_out, AnimationCurveEaseIn);
    animation_set_handlers((Animation *)fade_out, (AnimationHandlers){
        .stopped = fade_out_stopped_handler
    }, display_layer);
    
    display_layer->animation = fade_out;
    animation_schedule((Animation *)fade_out);
}

/**
 * 直接設定顯示圖層的點陣圖（無動畫，用於固定圖層）
 */
static void set_display_layer_bitmap(DisplayLayer *display_layer, uint32_t resource_id) {
    if (display_layer->bitmap) {
        gbitmap_destroy(display_layer->bitmap);
        display_layer->bitmap = NULL;
    }

    if (resource_id != 0) {
        display_layer->bitmap = gbitmap_create_with_resource(resource_id);
        customize_bitmap(display_layer, display_layer->bitmap);
        bitmap_layer_set_bitmap(display_layer->layer, display_layer->bitmap);
    } else {
        bitmap_layer_set_bitmap(display_layer->layer, NULL);
    }
    
    display_layer->current_resource_id = resource_id;
}

// ==================== 時間更新邏輯 ====================

static void update_time() {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    int hour = tick_time->tm_hour;
    if (!clock_is_24h_style() && hour > 12) {
        hour -= 12;
    }

    uint32_t hour_tens_res_id = 0;
    uint32_t hour_ones_res_id = 0;

    if (hour == 0) {
        hour_ones_res_id = RESOURCE_ID_IMG_U0;
    } else {
        hour_ones_res_id = TIME_UPPERCASE_ONES_RESOURCES[hour % 10];
        if (hour > 10) {
            hour_tens_res_id = TIME_UPPERCASE_TENS_RESOURCES[hour / 10];
        }
    }

    set_display_layer_bitmap_animated(&s_hour_layers[0], hour_tens_res_id, TIME_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_hour_layers[1], hour_ones_res_id, TIME_IMAGE_SIZE);

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

    set_display_layer_bitmap_animated(&s_minute_layers[0], minute_tens_res_id, TIME_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_minute_layers[1], minute_ones_res_id, TIME_IMAGE_SIZE);
}

// ==================== 日期更新邏輯 ====================

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
    uint32_t day_ones_res_id = 0;

    if (day == 10) {
        day_tens_res_id = RESOURCE_ID_IMG_SL1;
        day_ones_res_id = RESOURCE_ID_IMG_SL0;
    } else if (d2 != 0) {
        day_tens_res_id = DATE_LOWERCASE_TENS_RESOURCES[d1];
        day_ones_res_id = DATE_LOWERCASE_ONES_RESOURCES[d2];
    } else {
        day_tens_res_id = DATE_LOWERCASE_ONES_RESOURCES[d1];
        day_ones_res_id = DATE_LOWERCASE_ONES_RESOURCES[d2];
    }

    uint32_t week_res_id = (week == 0) ? RESOURCE_ID_IMG_RI : DATE_LOWERCASE_ONES_RESOURCES[week];

    set_display_layer_bitmap_animated(&s_month_layers[0], month_tens_res_id, DATE_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_month_layers[1], month_ones_res_id, DATE_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_day_layers[0], day_tens_res_id, DATE_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_day_layers[1], day_ones_res_id, DATE_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_week_layer, week_res_id, DATE_IMAGE_SIZE);
}

// ==================== 事件處理 ====================

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();

    if (units_changed & DAY_UNIT) {
        update_date(tick_time);
    }
}

// ==================== UI 畫面建構 ====================

static void create_display_layer(Layer *parent, GRect bounds, DisplayLayer *display_layer, bool animated) {
    // 如果需要動畫，調整初始位置：向下偏移以配合淡入動畫
    GRect adjusted_bounds = bounds;
    if (animated) {
        adjusted_bounds.origin.y += FADE_OUT_DISTANCE;
    }
    
    display_layer->layer = bitmap_layer_create(adjusted_bounds);
    bitmap_layer_set_background_color(display_layer->layer, GColorClear);
    bitmap_layer_set_compositing_mode(display_layer->layer, GCompOpSet);
    layer_add_child(parent, bitmap_layer_get_layer(display_layer->layer));
    display_layer->bitmap = NULL;
    display_layer->current_resource_id = 0;
    display_layer->animation = NULL;
}

static void destroy_display_layer(DisplayLayer *display_layer) {
    if (display_layer) {
        if (display_layer->animation) {
            animation_unschedule((Animation *)display_layer->animation);
            property_animation_destroy(display_layer->animation);
            display_layer->animation = NULL;
        }
        if (display_layer->bitmap) {
            gbitmap_destroy(display_layer->bitmap);
            display_layer->bitmap = NULL;
        }
        if (display_layer->layer) {
            bitmap_layer_destroy(display_layer->layer);
            display_layer->layer = NULL;
        }
    }
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);

    create_display_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[0]);
    create_display_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[1]);
    create_display_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[0]);
    create_display_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[1]);

    int x_pos = 8;
    create_display_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_month_layers[0]);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_display_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_month_layers[1]);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_display_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_yue_layer);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_display_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_day_layers[0]);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_display_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_day_layers[1]);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_display_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_ri_layer);
    x_pos += DATE_IMAGE_SIZE.w + 2;
    create_display_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_zhou_layer);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_display_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_week_layer);

    set_display_layer_bitmap(&s_yue_layer, RESOURCE_ID_IMG_YUE);
    set_display_layer_bitmap(&s_ri_layer, RESOURCE_ID_IMG_RI);
    set_display_layer_bitmap(&s_zhou_layer, RESOURCE_ID_IMG_ZHOU);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    Tuple *accent_color_t = dict_find(iter, MESSAGE_KEY_KEY_MINUTE_COLOR);
    if (accent_color_t) {
        s_accent_color = GColorFromHEX(accent_color_t->value->int32);

        // 重新繪製時間，讓顏色生效
        update_time();
    }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_handler(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
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

// ==================== 應用程式生命週期 ====================

static void init() {
    s_accent_color = GColorRed; // 預設顏色

    s_main_window = window_create();
    window_set_background_color(s_main_window, GColorWhite);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });

    window_stack_push(s_main_window, true);

    // 註冊時間更新
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    // 初始化顯示
    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    update_time();
    update_date(current_time);

    // 註冊訊息處理
    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    app_message_register_outbox_sent(outbox_sent_handler);
    app_message_open(128, 128);
}

static void deinit() {
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
