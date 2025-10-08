#include <pebble.h>

// ==================== 全域變數宣告 ====================

// UI 視窗元件
static Window *s_main_window;

// 時間顯示用的圖層(小時十位、個位;分鐘十位、個位)
static BitmapLayer *s_hour_tens_layer;
static BitmapLayer *s_hour_ones_layer;
static BitmapLayer *s_minute_tens_layer;
static BitmapLayer *s_minute_ones_layer;

// 日期顯示用的圖層(月份十位、個位;日期十位、個位;星期)
static BitmapLayer *s_month_tens_layer;
static BitmapLayer *s_month_ones_layer;
static BitmapLayer *s_day_tens_layer;
static BitmapLayer *s_day_ones_layer;
static BitmapLayer *s_week_layer;

// 固定文字圖層("月"、"日"、"週")
static BitmapLayer *s_yue_layer;
static BitmapLayer *s_ri_layer;
static BitmapLayer *s_zhou_layer;

// 時間顯示用的點陣圖
static GBitmap *s_hour_tens_bitmap;
static GBitmap *s_hour_ones_bitmap;
static GBitmap *s_minute_tens_bitmap;
static GBitmap *s_minute_ones_bitmap;

// 日期顯示用的點陣圖
static GBitmap *s_month_tens_bitmap;
static GBitmap *s_month_ones_bitmap;
static GBitmap *s_day_tens_bitmap;
static GBitmap *s_day_ones_bitmap;
static GBitmap *s_week_bitmap;

// 固定文字點陣圖
static GBitmap *s_yue_bitmap;
static GBitmap *s_ri_bitmap;
static GBitmap *s_zhou_bitmap;

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
static void set_bitmap_on_layer(GBitmap **bitmap_ptr, BitmapLayer *bitmap_layer, uint32_t resource_id);

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
    RESOURCE_ID_IMG_SL9         // 九
};

// ==================== 工具函式 ====================

/**
 * 設定圖層的點陣圖
 * @param bitmap_ptr 指向點陣圖指標的指標(用於釋放舊圖並儲存新圖)
 * @param bitmap_layer 要設定的圖層
 * @param resource_id 圖片資源ID(若為0則清除圖片)
 */
static void set_bitmap_on_layer(GBitmap **bitmap_ptr, BitmapLayer *bitmap_layer, uint32_t resource_id) {
    // 如果已有舊的點陣圖,先釋放記憶體
    if (*bitmap_ptr) {
        gbitmap_destroy(*bitmap_ptr);
    }
    
    // 如果資源ID有效,載入新圖片
    if (resource_id != 0) {
        *bitmap_ptr = gbitmap_create_with_resource(resource_id);
        bitmap_layer_set_bitmap(bitmap_layer, *bitmap_ptr);
    } else {
        // 資源ID為0時,清除圖層內容
        bitmap_layer_set_bitmap(bitmap_layer, NULL);
    }
}

// ==================== 時間更新邏輯 ====================

/**
 * 更新時間顯示
 * 處理小時和分鐘的中文數字顯示邏輯
 */
static void update_time() {
    // 取得當前時間
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    // --- 小時處理 ---
    int hour = tick_time->tm_hour;
    // 若為12小時制且超過12點,轉換為12小時制
    if (!clock_is_24h_style() && hour > 12) {
        hour -= 12;
    }
    
    uint32_t hour_tens_res_id = 0;  // 小時十位數的資源ID
    uint32_t hour_ones_res_id = 0;  // 小時個位數的資源ID
    
    // 特殊處理:0點顯示為"零"
    if (hour == 0) {
        hour_ones_res_id = RESOURCE_ID_IMG_U0;
    } else {
        // 設定個位數
        hour_ones_res_id = TIME_UPPERCASE_ONES_RESOURCES[hour % 10];
        // 若大於10,設定十位數
        if (hour > 10) {
            hour_tens_res_id = TIME_UPPERCASE_TENS_RESOURCES[hour / 10];
        }
    }
    // 將資源設定到對應圖層
    set_bitmap_on_layer(&s_hour_tens_bitmap, s_hour_tens_layer, hour_tens_res_id);
    set_bitmap_on_layer(&s_hour_ones_bitmap, s_hour_ones_layer, hour_ones_res_id);
    
    // --- 分鐘處理 ---
    int minute = tick_time->tm_min;
    int m1 = minute / 10;  // 十位數
    int m2 = minute % 10;  // 個位數
    
    uint32_t minute_tens_res_id = 0;  // 分鐘十位數的資源ID
    uint32_t minute_ones_res_id = 0;  // 分鐘個位數的資源ID
    
    // 特殊處理:00分顯示為"點整"
    if (minute == 0) {
        minute_tens_res_id = RESOURCE_ID_IMG_DIAN;
        minute_ones_res_id = RESOURCE_ID_IMG_ZHENG;
    } 
    // 特殊處理:30分顯示為"半"
    else if (minute == 30) {
        minute_tens_res_id = RESOURCE_ID_IMG_BAN;
        minute_ones_res_id = 0;
    }
    // 特殊處理:10分顯示為"一〇"
    else if (minute == 10) {
        minute_tens_res_id = RESOURCE_ID_IMG_L1;
        minute_ones_res_id = RESOURCE_ID_IMG_L0;
    } 
    // 一般情況:非整十的分鐘
    else if (m2 != 0) {
        minute_tens_res_id = TIME_LOWERCASE_TENS_RESOURCES[m1];
        minute_ones_res_id = TIME_LOWERCASE_ONES_RESOURCES[m2];
    } 
    // 整十分鐘
    else {
        minute_tens_res_id = TIME_LOWERCASE_ONES_RESOURCES[m1];
        minute_ones_res_id = TIME_LOWERCASE_ONES_RESOURCES[m2];
    }
    // 將資源設定到對應圖層
    set_bitmap_on_layer(&s_minute_tens_bitmap, s_minute_tens_layer, minute_tens_res_id);
    set_bitmap_on_layer(&s_minute_ones_bitmap, s_minute_ones_layer, minute_ones_res_id);
}

// ==================== 日期更新邏輯 ====================

/**
 * 更新日期顯示
 * 處理月份、日期和星期的中文數字顯示邏輯
 * @param tick_time 時間結構指標
 */
static void update_date(struct tm *tick_time) {
    int month = tick_time->tm_mon + 1;  // 月份(tm_mon為0-11需加1)
    int day = tick_time->tm_mday;       // 日期
    int week = tick_time->tm_wday;      // 星期(0=週日,1-6=週一至週六)
    
    // --- 月份處理 ---
    uint32_t month_tens_res_id = 0;  // 月份十位數的資源ID
    uint32_t month_ones_res_id = 0;  // 月份個位數的資源ID

    // 設定個位數
    month_ones_res_id = DATE_UPPERCASE_ONES_RESOURCES[month % 10];
    // 若大於10月,顯示"拾"在十位
    if (month > 10) {
        month_tens_res_id = RESOURCE_ID_IMG_SU10;
    }
    
    // --- 日期處理 ---
    int d1 = day / 10;  // 十位數
    int d2 = day % 10;  // 個位數
  
    uint32_t day_tens_res_id = 0;  // 日期十位數的資源ID
    uint32_t day_ones_res_id = 0;  // 日期個位數的資源ID
    
    // 日期顯示邏輯
    if (d1 == 0) { // 1-9日
        day_tens_res_id = 0;
        day_ones_res_id = DATE_LOWERCASE_ONES_RESOURCES[d2];
    } else { // 10日以上
        if (d2 == 0) { // 10, 20, 30日
            day_tens_res_id = DATE_LOWERCASE_TENS_RESOURCES[d1];
            day_ones_res_id = 0;
        } else { // 11-19, 21-29, 31日
            day_tens_res_id = DATE_LOWERCASE_TENS_RESOURCES[d1];
            day_ones_res_id = DATE_LOWERCASE_ONES_RESOURCES[d2];
        }
    }

    // --- 星期處理 ---
    uint32_t week_res_id = 0;  // 星期的資源ID
    
    // 週日(0)顯示"日",週一到週六(1-6)顯示對應數字
    if (week == 0) {
        week_res_id = RESOURCE_ID_IMG_RI;
    } else {
        week_res_id = DATE_LOWERCASE_ONES_RESOURCES[week];
    }
    
    // 將所有日期資源設定到對應圖層
    set_bitmap_on_layer(&s_month_tens_bitmap, s_month_tens_layer, month_tens_res_id);
    set_bitmap_on_layer(&s_month_ones_bitmap, s_month_ones_layer, month_ones_res_id);
    set_bitmap_on_layer(&s_day_tens_bitmap, s_day_tens_layer, day_tens_res_id);
    set_bitmap_on_layer(&s_day_ones_bitmap, s_day_ones_layer, day_ones_res_id);
    set_bitmap_on_layer(&s_week_bitmap, s_week_layer, week_res_id);
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
 * 建立並設定 BitmapLayer 的輔助函式
 * @param parent 父圖層
 * @param bounds 圖層的位置和大小
 * @return 建立的 BitmapLayer
 */
static BitmapLayer* create_bitmap_layer(Layer *parent, GRect bounds) {
    BitmapLayer *layer = bitmap_layer_create(bounds);
    bitmap_layer_set_background_color(layer, GColorClear);      // 設定透明背景
    bitmap_layer_set_compositing_mode(layer, GCompOpSet);       // 設定合成模式
    layer_add_child(parent, bitmap_layer_get_layer(layer));    // 加入父圖層
    return layer;
}

/**
 * 載入主視窗時的初始化函式
 * 建立所有UI元件並設定其位置
 * @param window 主視窗
 */
static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);

    // 建立時間顯示圖層(2x2格子佈局,顯示小時和分鐘)
    s_hour_tens_layer = create_bitmap_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h));
    s_hour_ones_layer = create_bitmap_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h));
    s_minute_tens_layer = create_bitmap_layer(window_layer, GRect(TIME_COL1_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h));
    s_minute_ones_layer = create_bitmap_layer(window_layer, GRect(TIME_COL2_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h));
    
    // 建立日期顯示圖層(水平排列:月月月日日日周周)
    int x_pos = 8;  // 起始X座標
    s_month_tens_layer = create_bitmap_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h));
    x_pos += DATE_IMAGE_SIZE.w + 1;
    s_month_ones_layer = create_bitmap_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h));
    x_pos += DATE_IMAGE_SIZE.w + 1;
    s_yue_layer = create_bitmap_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h));
    x_pos += DATE_IMAGE_SIZE.w + 1;
    s_day_tens_layer = create_bitmap_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h));
    x_pos += DATE_IMAGE_SIZE.w + 1;
    s_day_ones_layer = create_bitmap_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h));
    x_pos += DATE_IMAGE_SIZE.w + 1;
    s_ri_layer = create_bitmap_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h));
    x_pos += DATE_IMAGE_SIZE.w + 2;
    s_zhou_layer = create_bitmap_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h));
    x_pos += DATE_IMAGE_SIZE.w + 1;
    s_week_layer = create_bitmap_layer(window_layer, GRect(x_pos, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h));

    // 載入固定文字的圖片("月"、"日"、"周")
    set_bitmap_on_layer(&s_yue_bitmap, s_yue_layer, RESOURCE_ID_IMG_YUE);
    set_bitmap_on_layer(&s_ri_bitmap, s_ri_layer, RESOURCE_ID_IMG_RI);
    set_bitmap_on_layer(&s_zhou_bitmap, s_zhou_layer, RESOURCE_ID_IMG_ZHOU);
}

/**
 * 卸載主視窗時的清理函式
 * 釋放所有點陣圖和圖層的記憶體
 * @param window 主視窗
 */
static void main_window_unload(Window *window) {
    // 將所有點陣圖指標放入陣列便於批次處理
    GBitmap* bitmaps[] = {
        s_hour_tens_bitmap, s_hour_ones_bitmap, s_minute_tens_bitmap, s_minute_ones_bitmap,
        s_month_tens_bitmap, s_month_ones_bitmap, s_yue_bitmap, s_day_tens_bitmap,
        s_day_ones_bitmap, s_ri_bitmap, s_week_bitmap, s_zhou_bitmap
    };
    // 逐一釋放所有點陣圖記憶體
    for (int i = 0; i < 12; i++) {
        if (bitmaps[i]) {
            gbitmap_destroy(bitmaps[i]);
        }
    }

    // 釋放所有圖層記憶體
    bitmap_layer_destroy(s_hour_tens_layer);
    bitmap_layer_destroy(s_hour_ones_layer);
    bitmap_layer_destroy(s_minute_tens_layer);
    bitmap_layer_destroy(s_minute_ones_layer);
    bitmap_layer_destroy(s_month_tens_layer);
    bitmap_layer_destroy(s_month_ones_layer);
    bitmap_layer_destroy(s_day_tens_layer);
    bitmap_layer_destroy(s_day_ones_layer);
    bitmap_layer_destroy(s_week_layer);
    bitmap_layer_destroy(s_yue_layer);
    bitmap_layer_destroy(s_ri_layer);
    bitmap_layer_destroy(s_zhou_layer);
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