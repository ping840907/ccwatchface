#include <pebble.h>

// ==================== 結構定義 ====================

// AnimationLayer 結構定義
typedef struct {
    Layer *layer;
    GBitmap *old_bitmap;
    GBitmap *new_bitmap;
    Animation *animation;
    AnimationImplementation anim_impl;
    AnimationProgress anim_progress;
    uint32_t current_resource_id;
} AnimationLayer;

// ==================== 全域變數宣告 ====================

// UI 視窗元件
static Window *s_main_window;

// 時間顯示圖層 (小時十位/個位, 分鐘十位/個位)
static AnimationLayer s_hour_layers[2];
static AnimationLayer s_minute_layers[2];

// 日期顯示圖層 (月份十位/個位, 日期十位/個位, 星期)
static AnimationLayer s_month_layers[2];
static AnimationLayer s_day_layers[2];
static AnimationLayer s_week_layer;

// 固定文字圖層 ("月", "日", "週")
static AnimationLayer s_yue_layer;
static AnimationLayer s_ri_layer;
static AnimationLayer s_zhou_layer;

// 全域 AnimationLayer 指標陣列
static AnimationLayer* s_all_layers[12];
static int s_all_layers_count = 0;

static AnimationLayer* get_anim_layer_from_layer(Layer *layer) {
    for (int i = 0; i < s_all_layers_count; i++) {
        if (s_all_layers[i] && s_all_layers[i]->layer == layer) {
            return s_all_layers[i];
        }
    }
    return NULL;
}

// Helper to find AnimationLayer from Animation
static AnimationLayer* get_anim_layer_from_animation(Animation *animation) {
    for (int i = 0; i < s_all_layers_count; i++) {
        if (s_all_layers[i] && s_all_layers[i]->animation == animation) {
            return s_all_layers[i];
        }
    }
    return NULL;
}

// ==================== 螢幕佈局常數定義 ====================

#define SCREEN_WIDTH 200                              // 螢幕寬度
#define SCREEN_HEIGHT 228                             // 螢幕高度
#define TIME_IMAGE_SIZE GSize(88, 88)                 // 時間數字圖片大小
#define DATE_IMAGE_SIZE GSize(22, 22)                 // 日期數字圖片大小
#define TIME_COL1_X 8                                 // 時間第一列的X座標
#define TIME_COL2_X (TIME_COL1_X + TIME_IMAGE_SIZE.w + 8)  // 時間第二列的X座標
#define TIME_ROW1_Y 8                                 // 時間第一行的Y座標
#define TIME_ROW2_Y (TIME_ROW1_Y + TIME_IMAGE_SIZE.h + 8)  // 時間第二行的Y座標
#define DATE_ROW_Y (TIME_ROW2_Y + TIME_IMAGE_SIZE.h + 7)   // 日期行的Y座標

// ==================== 函式原型宣告 ====================

static void layer_update_proc(Layer *layer, GContext *ctx);
static void animation_update(Animation *animation, const AnimationProgress progress);
static void animation_teardown(Animation *animation);

static void update_time();
static void update_date(struct tm *tick_time);
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);
static void animation_layer_update(AnimationLayer *anim_layer, uint32_t new_resource_id);
static void create_animation_layer(Layer *parent, GRect bounds, AnimationLayer *anim_layer);
static void destroy_animation_layer(AnimationLayer *anim_layer);

// ==================== 圖片資源映射表 ====================

// 時間顯示 - 大寫數字十位數資源
const uint32_t TIME_UPPERCASE_TENS_RESOURCES[] = {
    0,                          // 0-10點不顯示十位
    RESOURCE_ID_IMG_U10,        // 11-19點顯示"拾"
    RESOURCE_ID_IMG_U2,         // 20-23點顯示"貳"
};

// 時間顯示 - 大寫數字個位數資源
const uint32_t TIME_UPPERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_U10,        // 拾
    RESOURCE_ID_IMG_U1,         // 壹
    RESOURCE_ID_IMG_U2,         // 貳
    RESOURCE_ID_IMG_U3,         // 參
    RESOURCE_ID_IMG_U4,         // 肆
    RESOURCE_ID_IMG_U5,         // 伍
    RESOURCE_ID_IMG_U6,         // 陸
    RESOURCE_ID_IMG_U7,         // 柒
    RESOURCE_ID_IMG_U8,         // 捌
    RESOURCE_ID_IMG_U9          // 玖
};

// 時間顯示 - 小寫數字十位數資源
const uint32_t TIME_LOWERCASE_TENS_RESOURCES[] = {
    RESOURCE_ID_IMG_L0,         // 〇
    RESOURCE_ID_IMG_L10,        // 十
    RESOURCE_ID_IMG_L20,        // 廿
    RESOURCE_ID_IMG_L30,        // 卅
    RESOURCE_ID_IMG_L4,         // 四
    RESOURCE_ID_IMG_L5,         // 五
};

// 時間顯示 - 小寫數字個位數資源
const uint32_t TIME_LOWERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_L10,        // 十
    RESOURCE_ID_IMG_L1,         // 一
    RESOURCE_ID_IMG_L2,         // 二
    RESOURCE_ID_IMG_L3,         // 三
    RESOURCE_ID_IMG_L4,         // 四
    RESOURCE_ID_IMG_L5,         // 五
    RESOURCE_ID_IMG_L6,         // 六
    RESOURCE_ID_IMG_L7,         // 七
    RESOURCE_ID_IMG_L8,         // 八
    RESOURCE_ID_IMG_L9          // 九
};

// 日期顯示 - 大寫數字十位數:因只有不顯示和"拾"，故省略

// 日期顯示 - 大寫數字個位數資源
const uint32_t DATE_UPPERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_SU10,       // 拾
    RESOURCE_ID_IMG_SU1,        // 壹
    RESOURCE_ID_IMG_SU2,        // 貳
    RESOURCE_ID_IMG_SU3,        // 參
    RESOURCE_ID_IMG_SU4,        // 肆
    RESOURCE_ID_IMG_SU5,        // 伍
    RESOURCE_ID_IMG_SU6,        // 陸
    RESOURCE_ID_IMG_SU7,        // 柒
    RESOURCE_ID_IMG_SU8,        // 捌
    RESOURCE_ID_IMG_SU9         // 玖
};

// 日期顯示 - 小寫數字十位數資源
const uint32_t DATE_LOWERCASE_TENS_RESOURCES[] = {
    0,                          // 1-10日不顯示十位
    RESOURCE_ID_IMG_SL10,       // 十
    RESOURCE_ID_IMG_SL20,       // 廿
    RESOURCE_ID_IMG_SL30,       // 卅
};

// 日期顯示 - 小寫數字個位數資源
const uint32_t DATE_LOWERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_SL10,       // 十
    RESOURCE_ID_IMG_SL1,        // 一
    RESOURCE_ID_IMG_SL2,        // 二
    RESOURCE_ID_IMG_SL3,        // 三
    RESOURCE_ID_IMG_SL4,        // 四
    RESOURCE_ID_IMG_SL5,        // 五
    RESOURCE_ID_IMG_SL6,        // 六
    RESOURCE_ID_IMG_SL7,        // 七
    RESOURCE_ID_IMG_SL8,        // 八
    RESOURCE_ID_IMG_SL9,        // 九
};

// ==================== 工具函式 ====================

static void animation_update(Animation *animation, const AnimationProgress progress) {
    AnimationLayer *anim_layer = get_anim_layer_from_animation(animation);
    if (!anim_layer) return;

    anim_layer->anim_progress = progress;
    layer_mark_dirty(anim_layer->layer);
}

static void animation_teardown(Animation *animation) {
    AnimationLayer *anim_layer = get_anim_layer_from_animation(animation);
    if (!anim_layer) return;

    if (anim_layer->old_bitmap) {
        gbitmap_destroy(anim_layer->old_bitmap);
    }
    anim_layer->old_bitmap = anim_layer->new_bitmap;
    anim_layer->new_bitmap = NULL;
    anim_layer->animation = NULL; // The animation is destroyed automatically
}

static void animation_layer_update(AnimationLayer *anim_layer, uint32_t new_resource_id) {
    if (anim_layer->current_resource_id == new_resource_id) {
        return;
    }

    if (anim_layer->animation && animation_is_scheduled(anim_layer->animation)) {
        animation_unschedule(anim_layer->animation);
    }

    if (anim_layer->new_bitmap) {
        gbitmap_destroy(anim_layer->new_bitmap);
        anim_layer->new_bitmap = NULL;
    }

    if (new_resource_id != 0) {
        anim_layer->new_bitmap = gbitmap_create_with_resource(new_resource_id);
    }

    anim_layer->current_resource_id = new_resource_id;

    if (!anim_layer->old_bitmap && anim_layer->new_bitmap) {
         if (anim_layer->old_bitmap) gbitmap_destroy(anim_layer->old_bitmap);
         anim_layer->old_bitmap = anim_layer->new_bitmap;
         anim_layer->new_bitmap = NULL;
         layer_mark_dirty(anim_layer->layer);
         return;
    }

    anim_layer->anim_impl = (AnimationImplementation){
        .update = animation_update,
        .teardown = animation_teardown
    };

    if (anim_layer->animation) {
        animation_destroy(anim_layer->animation);
    }
    anim_layer->animation = animation_create();
    animation_set_duration(anim_layer->animation, 100);
    animation_set_curve(anim_layer->animation, AnimationCurveLinear);
    animation_set_implementation(anim_layer->animation, &anim_layer->anim_impl);
    animation_schedule(anim_layer->animation);
}

static void layer_update_proc(Layer *layer, GContext *ctx) {
    AnimationLayer *anim_layer = get_anim_layer_from_layer(layer);
    if (!anim_layer) return;

    GBitmap *old_b = anim_layer->old_bitmap;
    GBitmap *new_b = anim_layer->new_bitmap;
    GRect bounds = layer_get_bounds(layer);

    // If animation is done, just draw the current bitmap
    if (anim_layer->anim_progress == ANIMATION_NORMALIZED_MAX) {
        if (old_b) {
            graphics_context_set_compositing_mode(ctx, GCompOpSet);
            graphics_draw_bitmap_in_rect(ctx, old_b, (GRect){ .origin = {0,0}, .size = bounds.size });
        }
        return;
    }

    // Capture framebuffer
    GBitmap *fb = graphics_capture_frame_buffer(ctx);
    if (!fb) {
        // Fallback for when framebuffer is not available (e.g. screenshot)
        if (anim_layer->old_bitmap) {
            graphics_draw_bitmap_in_rect(ctx, anim_layer->old_bitmap, (GRect){ .origin = {0,0}, .size = bounds.size });
        }
        return;
    }

    for (uint16_t y = 0; y < bounds.size.h; y++) {
        GBitmapDataRowInfo old_row = old_b ? gbitmap_get_data_row_info(old_b, y) : (GBitmapDataRowInfo){ .min_x = 255 };
        GBitmapDataRowInfo new_row = new_b ? gbitmap_get_data_row_info(new_b, y) : (GBitmapDataRowInfo){ .min_x = 255 };
        GBitmapDataRowInfo fb_row = gbitmap_get_data_row_info(fb, y + bounds.origin.y);

        for (int x = 0; x < bounds.size.w; x++) {
            bool old_pixel = false;
            if (old_b) {
                int min_x = old_row.min_x;
                int max_x = old_row.max_x;
                if (x >= min_x && x <= max_x) {
                    if ((old_row.data[x / 8] >> (x % 8)) & 1) {
                        old_pixel = true;
                    }
                }
            }

            bool new_pixel = false;
            if (new_b) {
                int min_x = new_row.min_x;
                int max_x = new_row.max_x;
                if (x >= min_x && x <= max_x) {
                    if ((new_row.data[x / 8] >> (x % 8)) & 1) {
                        new_pixel = true;
                    }
                }
            }

            bool set_pixel = false;
            if (old_pixel && new_pixel) {
                set_pixel = true;
            } else if (old_pixel) {
                set_pixel = ((uint32_t)rand() % ANIMATION_NORMALIZED_MAX > anim_layer->anim_progress);
            } else if (new_pixel) {
                set_pixel = ((uint32_t)rand() % ANIMATION_NORMALIZED_MAX < anim_layer->anim_progress);
            }

            int byte = (x + bounds.origin.x) / 8;
            int bit = (x + bounds.origin.x) % 8;
            if (set_pixel) {
                fb_row.data[byte] |= (1 << bit);
            } else {
                fb_row.data[byte] &= ~(1 << bit);
            }
        }
    }

    graphics_release_frame_buffer(ctx, fb);
}

// ==================== 時間更新邏輯 ====================

/**
 * 更新時間顯示
 * 處理小時和分鐘的中文數字顯示邏輯
 */
static void update_time() {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    // --- 小時處理 ---
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
    animation_layer_update(&s_hour_layers[0], hour_tens_res_id);
    animation_layer_update(&s_hour_layers[1], hour_ones_res_id);
    
    // --- 分鐘處理 ---
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
    animation_layer_update(&s_minute_layers[0], minute_tens_res_id);
    animation_layer_update(&s_minute_layers[1], minute_ones_res_id);
}

// ==================== 日期更新邏輯 ====================

/**
 * 更新日期顯示
 * 處理月份、日期和星期的中文數字顯示邏輯
 * @param tick_time 時間結構指標
 */
static void update_date(struct tm *tick_time) {
    int month = tick_time->tm_mon + 1;
    int day = tick_time->tm_mday;
    int week = tick_time->tm_wday;
    
    // --- 月份處理 ---
    uint32_t month_tens_res_id = 0;
    uint32_t month_ones_res_id = DATE_UPPERCASE_ONES_RESOURCES[month % 10];
    if (month > 10) {
        month_tens_res_id = RESOURCE_ID_IMG_SU10;
    }
    
    // --- 日期處理 ---
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

    // --- 星期處理 ---
    uint32_t week_res_id = (week == 0) ? RESOURCE_ID_IMG_RI : DATE_LOWERCASE_ONES_RESOURCES[week];
    
    animation_layer_update(&s_month_layers[0], month_tens_res_id);
    animation_layer_update(&s_month_layers[1], month_ones_res_id);
    animation_layer_update(&s_day_layers[0], day_tens_res_id);
    animation_layer_update(&s_day_layers[1], day_ones_res_id);
    animation_layer_update(&s_week_layer, week_res_id);
}

// ==================== 事件處理 ====================

/**
 * 時間變動的回呼函式
 * 系統每分鐘或每天變動時會呼叫此函式
 * @param tick_time 當前時間
 * @param units_changed 變動的時間單位(分鐘或日期)
 */
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    // 每分鐘更新時間顯示
    update_time();
    
    // 只有在日期改變時才更新日期顯示(節省資源)
    if (units_changed & DAY_UNIT) {
        update_date(tick_time);
    }
}

// ==================== UI 畫面建構 ====================

static void create_animation_layer(Layer *parent, GRect bounds, AnimationLayer *anim_layer) {
    anim_layer->layer = layer_create(bounds);
    layer_set_update_proc(anim_layer->layer, layer_update_proc);
    layer_add_child(parent, anim_layer->layer);

    anim_layer->old_bitmap = NULL;
    anim_layer->new_bitmap = NULL;
    anim_layer->animation = NULL;
    anim_layer->current_resource_id = 0;
    anim_layer->anim_progress = ANIMATION_NORMALIZED_MAX;

    // Add to the global array
    if (s_all_layers_count < (int)ARRAY_LENGTH(s_all_layers)) {
        s_all_layers[s_all_layers_count++] = anim_layer;
    }
}

static void destroy_animation_layer(AnimationLayer *anim_layer) {
    if (anim_layer) {
        if (anim_layer->animation) {
            animation_unschedule(anim_layer->animation);
            animation_destroy(anim_layer->animation);
        }
        if (anim_layer->old_bitmap) gbitmap_destroy(anim_layer->old_bitmap);
        if (anim_layer->new_bitmap) gbitmap_destroy(anim_layer->new_bitmap);
        if (anim_layer->layer) layer_destroy(anim_layer->layer);
    }
}

/**
 * 載入主視窗時的初始化函式
 * 建立所有UI元件並設定其位置
 * @param window 主視窗
 */
static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);

    // 建立時間顯示圖層 (2x2 格子)
    create_animation_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[0]);
    create_animation_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[1]);
    create_animation_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[0]);
    create_animation_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[1]);

    // 建立日期顯示圖層 (水平排列)
    int x_pos = 8;
    create_animation_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_month_layers[0]);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_animation_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_month_layers[1]);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_animation_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_yue_layer);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_animation_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_day_layers[0]);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_animation_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_day_layers[1]);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_animation_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_ri_layer);
    x_pos += DATE_IMAGE_SIZE.w + 2;
    create_animation_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_zhou_layer);
    x_pos += DATE_IMAGE_SIZE.w + 1;
    create_animation_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h), &s_week_layer);

    // 載入固定文字圖片 (這些不需要動畫)
    animation_layer_update(&s_yue_layer, RESOURCE_ID_IMG_YUE);
    animation_layer_update(&s_ri_layer, RESOURCE_ID_IMG_RI);
    animation_layer_update(&s_zhou_layer, RESOURCE_ID_IMG_ZHOU);
}

/**
 * 卸載主視窗時的清理函式
 * 釋放所有點陣圖和圖層的記憶體
 * @param window 主視窗
 */
static void main_window_unload(Window *window) {
    // 收集所有 AnimationLayer 指標
    AnimationLayer* all_layers[] = {
        &s_hour_layers[0], &s_hour_layers[1], &s_minute_layers[0], &s_minute_layers[1],
        &s_month_layers[0], &s_month_layers[1], &s_day_layers[0], &s_day_layers[1],
        &s_week_layer, &s_yue_layer, &s_ri_layer, &s_zhou_layer
    };

    // 迭代並銷毀所有圖層和點陣圖
    for (size_t i = 0; i < ARRAY_LENGTH(all_layers); i++) {
        destroy_animation_layer(all_layers[i]);
    }
}

// ==================== 應用程式生命週期 ====================

/**
 * 應用程式初始化函式
 * 建立視窗、訂閱時間服務,並進行首次時間更新
 */
static void init() {
    // 初始化隨機數生成器
    srand(time(NULL));

    // 建立主視窗
    s_main_window = window_create();
    // 設定視窗的載入與卸載處理函式
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    // 將視窗推入視窗堆疊並顯示(true表示有動畫效果)
    window_stack_push(s_main_window, true);
   
    // 首次載入時手動更新一次時間和日期,避免空白顯示
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    update_time();
    update_date(tick_time);
    
    // 訂閱時間服務,每分鐘和每天變動時會呼叫 tick_handler
    tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT, tick_handler);
}

/**
 * 應用程式反初始化函式
 * 清理並釋放所有資源
 */
static void deinit() {
    window_destroy(s_main_window);
}

/**
 * 主程式進入點
 * 初始化應用程式,進入事件迴圈,最後清理資源
 */
int main(void) {
    init();                // 初始化
    app_event_loop();      // 進入事件迴圈(持續運行直到應用程式結束)
    deinit();              // 清理資源
}
