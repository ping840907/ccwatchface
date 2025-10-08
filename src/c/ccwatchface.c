#include <pebble.h>

// ==================== 結構定義 ====================

// 顯示圖層結構，包含點陣圖層和點陣圖本身
typedef struct {
    BitmapLayer *layer;
    GBitmap *bitmap;
} DisplayLayer;

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

static void update_time();
static void update_date(struct tm *tick_time);
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);
static void set_display_layer_bitmap(DisplayLayer *display_layer, uint32_t resource_id);
static void create_display_layer(Layer *parent, GRect bounds, DisplayLayer *display_layer);
static void destroy_display_layer(DisplayLayer *display_layer);

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

/**
 * 設定顯示圖層的點陣圖
 * @param display_layer 指向 DisplayLayer 結構的指標
 * @param resource_id 圖片資源ID (若為0則清除圖片)
 */
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
    set_display_layer_bitmap(&s_hour_layers[0], hour_tens_res_id);
    set_display_layer_bitmap(&s_hour_layers[1], hour_ones_res_id);
    
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
    set_display_layer_bitmap(&s_minute_layers[0], minute_tens_res_id);
    set_display_layer_bitmap(&s_minute_layers[1], minute_ones_res_id);
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
    
    set_display_layer_bitmap(&s_month_layers[0], month_tens_res_id);
    set_display_layer_bitmap(&s_month_layers[1], month_ones_res_id);
    set_display_layer_bitmap(&s_day_layers[0], day_tens_res_id);
    set_display_layer_bitmap(&s_day_layers[1], day_ones_res_id);
    set_display_layer_bitmap(&s_week_layer, week_res_id);
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

/**
 * 建立並設定 DisplayLayer
 * @param parent 父圖層
 * @param bounds 圖層的位置和大小
 * @param display_layer 要初始化的 DisplayLayer 結構指標
 */
static void create_display_layer(Layer *parent, GRect bounds, DisplayLayer *display_layer) {
    display_layer->layer = bitmap_layer_create(bounds);
    bitmap_layer_set_background_color(display_layer->layer, GColorClear);
    bitmap_layer_set_compositing_mode(display_layer->layer, GCompOpSet);
    layer_add_child(parent, bitmap_layer_get_layer(display_layer->layer));
    display_layer->bitmap = NULL; // 初始化點陣圖指標
}

/**
 * 銷毀 DisplayLayer
 * @param display_layer 要銷毀的 DisplayLayer 結構指標
 */
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

/**
 * 載入主視窗時的初始化函式
 * 建立所有UI元件並設定其位置
 * @param window 主視窗
 */
static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);

    // 建立時間顯示圖層 (2x2 格子)
    create_display_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[0]);
    create_display_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_hour_layers[1]);
    create_display_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[0]);
    create_display_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h), &s_minute_layers[1]);

    // 建立日期顯示圖層 (水平排列)
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

    // 載入固定文字圖片
    set_display_layer_bitmap(&s_yue_layer, RESOURCE_ID_IMG_YUE);
    set_display_layer_bitmap(&s_ri_layer, RESOURCE_ID_IMG_RI);
    set_display_layer_bitmap(&s_zhou_layer, RESOURCE_ID_IMG_ZHOU);
}

/**
 * 卸載主視窗時的清理函式
 * 釋放所有點陣圖和圖層的記憶體
 * @param window 主視窗
 */
static void main_window_unload(Window *window) {
    // 收集所有 DisplayLayer 指標
    DisplayLayer* all_layers[] = {
        &s_hour_layers[0], &s_hour_layers[1], &s_minute_layers[0], &s_minute_layers[1],
        &s_month_layers[0], &s_month_layers[1], &s_day_layers[0], &s_day_layers[1],
        &s_week_layer, &s_yue_layer, &s_ri_layer, &s_zhou_layer
    };

    // 迭代並銷毀所有圖層和點陣圖
    for (size_t i = 0; i < ARRAY_LENGTH(all_layers); i++) {
        destroy_display_layer(all_layers[i]);
    }
}

// ==================== 應用程式生命週期 ====================

/**
 * 應用程式初始化函式
 * 建立視窗、訂閱時間服務,並進行首次時間更新
 */
static void init() {
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
