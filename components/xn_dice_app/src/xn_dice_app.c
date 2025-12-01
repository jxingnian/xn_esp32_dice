/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-11-22 13:43:50
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-11-26 21:40:08
 * @FilePath: \\xn_esp32_dice\\components\\xn_dice_app\\src\\xn_dice_app.c
 * @Description: esp32 骰子应用实现（LVGL 优雅骰子界面）
 */

#include "xn_dice_app.h"

#include "xn_lvgl.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "XN_DICE_APP";

// 骰子点位置索引
typedef enum {
    PIP_CENTER = 0,
    PIP_TOP_LEFT,
    PIP_TOP_RIGHT,
    PIP_MID_LEFT,
    PIP_MID_RIGHT,
    PIP_BOTTOM_LEFT,
    PIP_BOTTOM_RIGHT,
    PIP_COUNT
} dice_pip_index_t;

static lv_obj_t *s_dice_obj = NULL;
static lv_obj_t *s_pip_objs[PIP_COUNT] = {0};
static lv_obj_t *s_hint_label = NULL;
static bool s_is_rolling = false;
static lv_timer_t *s_roll_timer = NULL;
static uint32_t s_roll_start_tick = 0;
static const uint32_t s_roll_duration_ms = 2000;

static void dice_set_face(int value);
static void dice_roll_timer_cb(lv_timer_t *timer);
static void dice_click_event_cb(lv_event_t *e);
static void dice_create_ui(void);

static void dice_create_ui(void)
{
    lv_obj_t *screen = lv_screen_active();

    // 设置屏幕渐变背景，深色系更显优雅
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x141428), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);

    // 创建骰子主体（圆角 + 阴影）
    const int dice_size = 220;
    const int pip_size = 26;

    s_dice_obj = lv_obj_create(screen);
    lv_obj_set_size(s_dice_obj, dice_size, dice_size);
    lv_obj_center(s_dice_obj);
    lv_obj_set_style_bg_color(s_dice_obj, lv_color_hex(0xFDFDFD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_dice_obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_dice_obj, 32, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_dice_obj, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_dice_obj, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_dice_obj, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(s_dice_obj, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_shadow_spread(s_dice_obj, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(s_dice_obj, lv_color_hex(0x000000), LV_PART_MAIN);

    // 创建 7 个圆点（骰子点），统一风格
    const int center_x = dice_size / 2;
    const int center_y = dice_size / 2;
    const int offset = dice_size / 4;

    for (int i = 0; i < PIP_COUNT; i++) {
        s_pip_objs[i] = lv_obj_create(s_dice_obj);
        lv_obj_set_size(s_pip_objs[i], pip_size, pip_size);
        lv_obj_set_style_bg_color(s_pip_objs[i], lv_color_hex(0x202020), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_pip_objs[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(s_pip_objs[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_pip_objs[i], 0, LV_PART_MAIN);
    }

    // 设置每个点的位置（相对于骰子对象）
    lv_obj_set_pos(s_pip_objs[PIP_CENTER],       center_x - pip_size / 2,          center_y - pip_size / 2);
    lv_obj_set_pos(s_pip_objs[PIP_TOP_LEFT],     center_x - offset,                center_y - offset);
    lv_obj_set_pos(s_pip_objs[PIP_TOP_RIGHT],    center_x + offset - pip_size,     center_y - offset);
    lv_obj_set_pos(s_pip_objs[PIP_MID_LEFT],     center_x - offset,                center_y - pip_size / 2);
    lv_obj_set_pos(s_pip_objs[PIP_MID_RIGHT],    center_x + offset - pip_size,     center_y - pip_size / 2);
    lv_obj_set_pos(s_pip_objs[PIP_BOTTOM_LEFT],  center_x - offset,                center_y + offset - pip_size);
    lv_obj_set_pos(s_pip_objs[PIP_BOTTOM_RIGHT], center_x + offset - pip_size,     center_y + offset - pip_size);

    // 默认显示 1 点
    dice_set_face(1);

    // 底部提示文字
    s_hint_label = lv_label_create(screen);
    lv_label_set_text(s_hint_label, "轻触屏幕，摇动骰子");
    lv_obj_set_style_text_color(s_hint_label, lv_color_hex(0xE0E0FF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_hint_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(s_hint_label, LV_ALIGN_BOTTOM_MID, 0, -24);

    // 整个屏幕可点击来摇骰子
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, dice_click_event_cb, LV_EVENT_CLICKED, NULL);
}

static void dice_set_face(int value)
{
    if (!s_dice_obj) {
        return;
    }

    if (value < 1 || value > 6) {
        value = 1;
    }

    // 先隐藏所有点
    for (int i = 0; i < PIP_COUNT; i++) {
        if (s_pip_objs[i]) {
            lv_obj_add_flag(s_pip_objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 根据点数显示对应的点
    switch (value) {
    case 1:
        lv_obj_clear_flag(s_pip_objs[PIP_CENTER], LV_OBJ_FLAG_HIDDEN);
        break;
    case 2:
        lv_obj_clear_flag(s_pip_objs[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 3:
        lv_obj_clear_flag(s_pip_objs[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_CENTER], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 4:
        lv_obj_clear_flag(s_pip_objs[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_TOP_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_BOTTOM_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 5:
        lv_obj_clear_flag(s_pip_objs[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_TOP_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_CENTER], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_BOTTOM_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 6:
        lv_obj_clear_flag(s_pip_objs[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_TOP_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_MID_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_MID_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_BOTTOM_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pip_objs[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        break;
    }
}

static void dice_roll_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    uint32_t elapsed = lv_tick_elaps(s_roll_start_tick);

    // 在摇动过程中快速切换点数，模拟摇晃效果
    int face = ((int)(esp_random() % 6U)) + 1;
    dice_set_face(face);

    if (elapsed >= s_roll_duration_ms) {
        // 结束摇动，给一个最终结果
        if (s_roll_timer) {
            lv_timer_del(s_roll_timer);
            s_roll_timer = NULL;
        }

        face = ((int)(esp_random() % 6U)) + 1;
        dice_set_face(face);

        s_is_rolling = false;

        ESP_LOGI(TAG, "dice result: %d", face);
    }
}

static void dice_click_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    if (s_is_rolling) {
        // 正在摇骰子时忽略新的点击
        return;
    }

    if (!s_dice_obj) {
        return;
    }

    s_is_rolling = true;
    s_roll_start_tick = lv_tick_get();

    if (s_roll_timer) {
        lv_timer_del(s_roll_timer);
        s_roll_timer = NULL;
    }

    // 创建定时器，每 80ms 变换一次点数
    s_roll_timer = lv_timer_create(dice_roll_timer_cb, 80, NULL);
}

esp_err_t xn_dice_app_init(void)
{
    static bool s_inited = false;

    if (s_inited) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "init dice app");

    // 初始化 LVGL 驱动（屏幕 + 触摸 + LVGL 任务）
    esp_err_t ret = lvgl_driver_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_driver_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 在应用任务中创建界面，需要使用 LVGL 锁
    lv_lock();
    dice_create_ui();
    lv_unlock();

    s_inited = true;
    return ESP_OK;
}
