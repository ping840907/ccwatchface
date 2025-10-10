#include <pebble.h>

// ==================== 結構定義 ====================

// 顯示圖層結構，包含點陣圖層和點陣圖本身
typedef struct {
    BitmapLayer *layer;
    GBitmap *bitmap;
    uint32_t current_resource_id;
} DisplayLayer;

// 動畫狀態結構
typedef struct {
    uint8_t *old_pixels;   // 舊圖像的像素數據副本
    uint8_t *new_pixels;   // 新圖像的像素數據副本
    int bytes_per_row;     // 每行的字節數
    GSize size;            // 圖像大小
    int step;              // 當前動畫步驟 (0-10)
    bool animating;        // 是否正在動畫
} AnimationState;

// ==================== 全域變數宣告 ====================

// UI 視窗元件
static Window *s_main_window;

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

// 動畫狀態
static AnimationState s_hour_anims[2];
static AnimationState s_minute_anims[2];
static AnimationState s_month_anims[2];
static AnimationState s_day_anims[2];
static AnimationState s_week_anim;

static AppTimer *s_animation_timer = NULL;

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

#define ANIMATION_STEPS 10
#define ANIMATION_INTERVAL_MS 10  // 10ms * 10 steps = 100ms total

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

// ==================== 動畫輔助函式 ====================

/**
 * 複製 GBitmap 的像素數據
 */
static uint8_t* copy_bitmap_data(GBitmap *bitmap, GSize size, int *bytes_per_row_out) {
    if (!bitmap) return NULL;
    
    uint8_t *data = gbitmap_get_data(bitmap);
    if (!data) return NULL;
    
    int bytes_per_row = gbitmap_get_bytes_per_row(bitmap);
    if (bytes_per_row_out) {
        *bytes_per_row_out = bytes_per_row;
    }
    
    int total_bytes = bytes_per_row * size.h;
    
    uint8_t *copy = malloc(total_bytes);
    if (copy) {
        memcpy(copy, data, total_bytes);
    }
    return copy;
}

/**
 * 初始化動畫狀態
 */
static void init_animation(AnimationState *anim, GBitmap *old_bmp, GBitmap *new_bmp, GSize size) {
    // 清理舊的動畫數據
    if (anim->old_pixels) {
        free(anim->old_pixels);
        anim->old_pixels = NULL;
    }
    if (anim->new_pixels) {
        free(anim->new_pixels);
        anim->new_pixels = NULL;
    }
    
    anim->size = size;
    anim->step = 0;
    anim->animating = false;
    anim->bytes_per_row = 0;
    
    // 如果沒有舊圖像，直接顯示新圖像，不需要動畫
    if (!old_bmp) return;
    
    int old_bpr = 0, new_bpr = 0;
    anim->old_pixels = copy_bitmap_data(old_bmp, size, &old_bpr);
    anim->new_pixels = copy_bitmap_data(new_bmp, size, &new_bpr);
    
    // 確保兩個圖像的字節數相同
    if (old_bpr != new_bpr) {
        if (anim->old_pixels) free(anim->old_pixels);
        if (anim->new_pixels) free(anim->new_pixels);
        anim->old_pixels = NULL;
        anim->new_pixels = NULL;
        return;
    }
    
    anim->bytes_per_row = old_bpr;
    
    if (anim->old_pixels && anim->new_pixels) {
        anim->animating = true;
    }
}

/**
 * 更新單個圖層的動畫幀
 */
static bool update_animation_frame(DisplayLayer *layer, AnimationState *anim) {
    if (!anim->animating || !layer->bitmap) {
        return false;
    }
    
    if (anim->step > ANIMATION_STEPS) {
        anim->animating = false;
        return false;
    }
    
    uint8_t *current = gbitmap_get_data(layer->bitmap);
    if (!current || !anim->old_pixels || !anim->new_pixels) {
        anim->animating = false;
        return false;
    }
    
    int bytes_per_row = anim->bytes_per_row;
    
    // 如果已經完成動畫，確保顯示最終狀態
    if (anim->step == ANIMATION_STEPS) {
        int total_bytes = bytes_per_row * anim->size.h;
        memcpy(current, anim->new_pixels, total_bytes);
        anim->step++;
        layer_mark_dirty(bitmap_layer_get_layer(layer->layer));
        return false;
    }
    
    // 使用固定的隨機種子來確保每個像素的抖動模式一致
    // 使用更好的偽隨機分佈
    static const uint8_t dither_matrix[8][8] = {
        { 0, 32,  8, 40,  2, 34, 10, 42},
        {48, 16, 56, 24, 50, 18, 58, 26},
        {12, 44,  4, 36, 14, 46,  6, 38},
        {60, 28, 52, 20, 62, 30, 54, 22},
        { 3, 35, 11, 43,  1, 33,  9, 41},
        {51, 19, 59, 27, 49, 17, 57, 25},
        {15, 47,  7, 39, 13, 45,  5, 37},
        {63, 31, 55, 23, 61, 29, 53, 21}
    };
    
    // 逐像素混合
    for (int y = 0; y < anim->size.h; y++) {
        for (int x = 0; x < anim->size.w; x++) {
            int byte_idx = y * bytes_per_row + (x / 8);
            int bit_idx = x % 8;
            uint8_t mask = 1 << bit_idx;
            
            bool old_set = (anim->old_pixels[byte_idx] & mask) != 0;
            bool new_set = (anim->new_pixels[byte_idx] & mask) != 0;
            
            bool show = false;
            
            if (old_set == new_set) {
                // 共同像素，保持不變
                show = new_set;
            } else {
                // 使用抖動矩陣來決定是否顯示像素
                int threshold = dither_matrix[y % 8][x % 8];
                int alpha = (anim->step * 64) / ANIMATION_STEPS;  // 0-64
                
                if (new_set && !old_set) {
                    // 淡入：當 alpha > threshold 時顯示
                    show = (alpha > threshold);
                } else {
                    // 淡出：當 (64 - alpha) > threshold 時顯示
                    show = ((64 - alpha) > threshold);
                }
            }
            
            if (show) {
                current[byte_idx] |= mask;
            } else {
                current[byte_idx] &= ~mask;
            }
        }
    }
    
    anim->step++;
    layer_mark_dirty(bitmap_layer_get_layer(layer->layer));
    return true;
}

/**
 * 動畫計時器回調
 */
static void animation_timer_callback(void *data) {
    bool any_animating = false;
    
    // 更新所有動畫
    any_animating |= update_animation_frame(&s_hour_layers[0], &s_hour_anims[0]);
    any_animating |= update_animation_frame(&s_hour_layers[1], &s_hour_anims[1]);
    any_animating |= update_animation_frame(&s_minute_layers[0], &s_minute_anims[0]);
    any_animating |= update_animation_frame(&s_minute_layers[1], &s_minute_anims[1]);
    any_animating |= update_animation_frame(&s_month_layers[0], &s_month_anims[0]);
    any_animating |= update_animation_frame(&s_month_layers[1], &s_month_anims[1]);
    any_animating |= update_animation_frame(&s_day_layers[0], &s_day_anims[0]);
    any_animating |= update_animation_frame(&s_day_layers[1], &s_day_anims[1]);
    any_animating |= update_animation_frame(&s_week_layer, &s_week_anim);
    
    // 如果還有動畫在進行，繼續計時
    if (any_animating) {
        s_animation_timer = app_timer_register(ANIMATION_INTERVAL_MS, animation_timer_callback, NULL);
    } else {
        s_animation_timer = NULL;
    }
}

/**
 * 啟動動畫計時器
 */
static void start_animation_timer() {
    if (s_animation_timer) {
        app_timer_cancel(s_animation_timer);
    }
    s_animation_timer = app_timer_register(ANIMATION_INTERVAL_MS, animation_timer_callback, NULL);
}

// ==================== 工具函式 ====================

/**
 * 設定顯示圖層的點陣圖（帶動畫）
 */
/**
 * 設定顯示圖層的點陣圖（帶動畫）- 已修正版本
 */
static void set_display_layer_bitmap_animated(DisplayLayer *display_layer, uint32_t resource_id,
                                               AnimationState *anim, GSize size) {
    // 【捷徑 1: 效率提升】
    // 如果要顯示的資源ID和目前的一樣，代表畫面無須更新，直接返回。
    if (display_layer->current_resource_id == resource_id) {
        return;
    }

    GBitmap *old_bitmap = display_layer->bitmap;
    GBitmap *new_bitmap = (resource_id != 0) ? gbitmap_create_with_resource(resource_id) : NULL;

    bool animation_will_run = false;

    // 嘗試啟動動畫
    if (anim && old_bitmap && new_bitmap) {
        init_animation(anim, old_bitmap, new_bitmap, size);

        // 【捷徑 2: 穩健性增強】
        // 檢查 init_animation 是否成功 (例如，malloc 是否成功)
        if (anim->animating) {
            // 動畫初始化成功
            animation_will_run = true;
            
            // 我們只需要 new_bitmap 的像素數據，現在可以銷毀它了
            gbitmap_destroy(new_bitmap);
            
            // 讓動畫在 old_bitmap 上進行繪製
        }
        // 如果 anim->animating 為 false，代表初始化失敗，將會走下面的非動畫路徑
    }

    // 如果動畫無法運行 (條件不滿足 或 初始化失敗)，則執行無動畫的直接替換
    if (!animation_will_run) {
        // 這是備用方案 (Fallback) 或 正常無動畫更新
        if (old_bitmap) {
            gbitmap_destroy(old_bitmap);
        }
        display_layer->bitmap = new_bitmap;
        bitmap_layer_set_bitmap(display_layer->layer, new_bitmap);
    }
    
    // 無論是哪種路徑，最後都要更新當前顯示的 resource_id
    display_layer->current_resource_id = resource_id;
}

static void set_display_layer_bitmap(DisplayLayer *display_layer, uint32_t resource_id) {
    if (display_layer->bitmap) {
        gbitmap_destroy(display_layer->bitmap);
        display_layer->bitmap = NULL;
    }

    if (resource_id != 0) {
        display_layer->bitmap = gbitmap_create_with_resource(resource_id);
        bitmap_layer_set_bitmap(display_layer->layer, display_layer->bitmap);
    } else {
        bitmap_layer_set_bitmap(display_layer->layer, NULL);
    }
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
    
    set_display_layer_bitmap_animated(&s_hour_layers[0], hour_tens_res_id, &s_hour_anims[0], TIME_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_hour_layers[1], hour_ones_res_id, &s_hour_anims[1], TIME_IMAGE_SIZE);

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
    
    set_display_layer_bitmap_animated(&s_minute_layers[0], minute_tens_res_id, &s_minute_anims[0], TIME_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_minute_layers[1], minute_ones_res_id, &s_minute_anims[1], TIME_IMAGE_SIZE);
    
    // 啟動動畫
    start_animation_timer();
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

    set_display_layer_bitmap_animated(&s_month_layers[0], month_tens_res_id, &s_month_anims[0], DATE_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_month_layers[1], month_ones_res_id, &s_month_anims[1], DATE_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_day_layers[0], day_tens_res_id, &s_day_anims[0], DATE_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_day_layers[1], day_ones_res_id, &s_day_anims[1], DATE_IMAGE_SIZE);
    set_display_layer_bitmap_animated(&s_week_layer, week_res_id, &s_week_anim, DATE_IMAGE_SIZE);
    
    // 啟動動畫
    start_animation_timer();
}

// ==================== 事件處理 ====================

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();

    if (units_changed & DAY_UNIT) {
        update_date(tick_time);
    }
}

// ==================== UI 畫面建構 ====================

static void create_display_layer(Layer *parent, GRect bounds, DisplayLayer *display_layer) {
    display_layer->layer = bitmap_layer_create(bounds);
    bitmap_layer_set_background_color(display_layer->layer, GColorClear);
    bitmap_layer_set_compositing_mode(display_layer->layer, GCompOpSet);
    layer_add_child(parent, bitmap_layer_get_layer(display_layer->layer));
    display_layer->bitmap = NULL;
    display_layer->current_resource_id = 0;
}

static void destroy_display_layer(DisplayLayer *display_layer) {
    if (display_layer) {
        if (display_layer->bitmap) {
            gbitmap_destroy(display_layer->bitmap);
        }
        if (display_layer->layer) {
            bitmap_layer_destroy(display_layer->layer);
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
    
    // 初始化動畫狀態
    memset(&s_hour_anims, 0, sizeof(s_hour_anims));
    memset(&s_minute_anims, 0, sizeof(s_minute_anims));
    memset(&s_month_anims, 0, sizeof(s_month_anims));
    memset(&s_day_anims, 0, sizeof(s_day_anims));
    memset(&s_week_anim, 0, sizeof(s_week_anim));
}

static void main_window_unload(Window *window) {
    // 取消動畫計時器
    if (s_animation_timer) {
        app_timer_cancel(s_animation_timer);
        s_animation_timer = NULL;
    }
    
    // 清理動畫數據
    AnimationState* all_anims[] = {
        &s_hour_anims[0], &s_hour_anims[1], &s_minute_anims[0], &s_minute_anims[1],
        &s_month_anims[0], &s_month_anims[1], &s_day_anims[0], &s_day_anims[1], &s_week_anim
    };
    
    for (size_t i = 0; i < ARRAY_LENGTH(all_anims); i++) {
        if (all_anims[i]->old_pixels) free(all_anims[i]->old_pixels);
        if (all_anims[i]->new_pixels) free(all_anims[i]->new_pixels);
    }
    
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
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    window_stack_push(s_main_window, true);

    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    update_time();
    update_date(tick_time);

    tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT, tick_handler);
}

static void deinit() {
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
