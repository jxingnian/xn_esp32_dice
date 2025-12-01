/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-11-22 13:43:50
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-12-01 17:25:40
 * @FilePath: \xn_esp32_dice\components\xn_dice_app\src\xn_dice_app.c
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

#define MAX_DICE 3

typedef struct {
    lv_obj_t *root;
    lv_obj_t *pips[PIP_COUNT];
} dice_view_t;

static dice_view_t s_dice_views[MAX_DICE];
static uint8_t s_dice_count = 1;
static lv_obj_t *s_hint_label = NULL;
static bool s_is_rolling = false;
static lv_timer_t *s_roll_timer = NULL;
static uint32_t s_roll_start_tick = 0;
static const uint32_t s_roll_duration_ms = 2000;
static lv_coord_t s_touch_start_x = 0;
static lv_coord_t s_touch_start_y = 0;

static lv_coord_t dice_abs(lv_coord_t v)
{
    return v < 0 ? -v : v;
}

static void dice_create_ui(void);
static void dice_set_face(uint8_t index, int value);
static void dice_update_layout(void);
static void dice_change_count(int delta);
static void dice_start_roll(void);
static void dice_roll_timer_cb(lv_timer_t *timer);
static void dice_click_event_cb(lv_event_t *e);

static void dice_create_ui(void)
{
    lv_obj_t *screen = lv_screen_active();

    // 设置屏幕渐变背景，深色系更显优雅（方案A）
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1F2933), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x111827), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);

    // 中央柔和高光，突出骰子区域
    lv_obj_t *highlight = lv_obj_create(screen);
    lv_obj_set_size(highlight, lv_pct(80), lv_pct(80));
    lv_obj_center(highlight);
    lv_obj_set_style_bg_color(highlight, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(highlight, LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_radius(highlight, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(highlight, 0, LV_PART_MAIN);

    // 创建多颗骰子视图（最多 MAX_DICE），初始只显示 1 颗
    for (int i = 0; i < MAX_DICE; i++) {
        dice_view_t *view = &s_dice_views[i];

        view->root = lv_obj_create(screen);
        lv_obj_set_style_bg_color(view->root, lv_color_hex(0xFAFAFA), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(view->root, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(view->root, 32, LV_PART_MAIN);
        lv_obj_set_style_border_color(view->root, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
        lv_obj_set_style_border_width(view->root, 4, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(view->root, 24, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(view->root, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(view->root, 4, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(view->root, lv_color_hex(0x000000), LV_PART_MAIN);

        const int pip_size = 26;
        const int center_x = 110;
        const int center_y = 110;
        const int offset = 110 / 2;

        for (int p = 0; p < PIP_COUNT; p++) {
            view->pips[p] = lv_obj_create(view->root);
            lv_obj_set_size(view->pips[p], pip_size, pip_size);
            lv_obj_set_style_bg_color(view->pips[p], lv_color_hex(0x111827), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(view->pips[p], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(view->pips[p], LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_set_style_border_width(view->pips[p], 0, LV_PART_MAIN);
        }

        lv_obj_set_pos(view->pips[PIP_CENTER],       center_x - pip_size / 2,          center_y - pip_size / 2);
        lv_obj_set_pos(view->pips[PIP_TOP_LEFT],     center_x - offset,                center_y - offset);
        lv_obj_set_pos(view->pips[PIP_TOP_RIGHT],    center_x + offset - pip_size,     center_y - offset);
        lv_obj_set_pos(view->pips[PIP_MID_LEFT],     center_x - offset,                center_y - pip_size / 2);
        lv_obj_set_pos(view->pips[PIP_MID_RIGHT],    center_x + offset - pip_size,     center_y - pip_size / 2);
        lv_obj_set_pos(view->pips[PIP_BOTTOM_LEFT],  center_x - offset,                center_y + offset - pip_size);
        lv_obj_set_pos(view->pips[PIP_BOTTOM_RIGHT], center_x + offset - pip_size,     center_y + offset - pip_size);

        dice_set_face((uint8_t)i, 1);
    }

    s_dice_count = 1;
    dice_update_layout();

    // 底部提示文字
    s_hint_label = lv_label_create(screen);
    lv_label_set_text(s_hint_label, "轻触屏幕，摇动骰子");
    lv_obj_set_style_text_color(s_hint_label, lv_color_hex(0xE0E0FF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_hint_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(s_hint_label, LV_ALIGN_BOTTOM_MID, 0, -24);

    // 整个屏幕可点击/滑动：
    // - 轻触：摇当前所有骰子
    // - 左右滑动：改变骰子数量
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, dice_click_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen, dice_click_event_cb, LV_EVENT_RELEASED, NULL);
}

static void dice_set_face(uint8_t index, int value)
{
    if (index >= MAX_DICE) {
        return;
    }

    if (value < 1 || value > 6) {
        value = 1;
    }

    dice_view_t *view = &s_dice_views[index];
    if (!view->root) {
        return;
    }

    for (int i = 0; i < PIP_COUNT; i++) {
        if (view->pips[i]) {
            lv_obj_add_flag(view->pips[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 根据点数显示对应的点
    switch (value) {
    case 1:
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_CENTER], LV_OBJ_FLAG_HIDDEN);
        break;
    case 2:
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 3:
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_CENTER], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 4:
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_TOP_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_BOTTOM_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 5:
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_TOP_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_CENTER], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_BOTTOM_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 6:
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_TOP_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_MID_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_MID_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_BOTTOM_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dice_views[index].pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        break;
    }
}

static void dice_update_layout(void)
{
    if (s_dice_count < 1) {
        s_dice_count = 1;
    }
    if (s_dice_count > MAX_DICE) {
        s_dice_count = MAX_DICE;
    }

    lv_display_t *disp = g_lvgl_display;
    if (!disp) {
        return;
    }

    lv_coord_t scr_w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t scr_h = lv_display_get_vertical_resolution(disp);
    lv_coord_t base = scr_w < scr_h ? scr_w : scr_h;

    lv_coord_t dice_size;
    if (s_dice_count == 1) {
        dice_size = (base * 3) / 5;
    } else if (s_dice_count == 2) {
        dice_size = (base * 2) / 5;
    } else {
        dice_size = (base * 3) / 10;
    }

    lv_coord_t gap = dice_size / 5;
    lv_coord_t cx = scr_w / 2;
    lv_coord_t cy = scr_h / 2;

    for (uint8_t i = 0; i < MAX_DICE; i++) {
        dice_view_t *view = &s_dice_views[i];
        if (!view->root) {
            continue;
        }

        if (i >= s_dice_count) {
            lv_obj_add_flag(view->root, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(view->root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(view->root, dice_size, dice_size);

        lv_coord_t x;
        if (s_dice_count == 1) {
            x = cx;
        } else if (s_dice_count == 2) {
            if (i == 0) {
                x = cx - (dice_size / 2 + gap / 2);
            } else {
                x = cx + (dice_size / 2 + gap / 2);
            }
        } else {
            if (i == 0) {
                x = cx - (dice_size + gap);
            } else if (i == 1) {
                x = cx;
            } else {
                x = cx + (dice_size + gap);
            }
        }

        lv_coord_t y = cy;
        lv_obj_set_pos(view->root, x - dice_size / 2, y - dice_size / 2);
    }
}

static void dice_roll_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    uint32_t elapsed = lv_tick_elaps(s_roll_start_tick);

    // 在摇动过程中快速切换点数，模拟摇晃效果
    for (uint8_t i = 0; i < s_dice_count; i++) {
        int face = ((int)(esp_random() % 6U)) + 1;
        dice_set_face(i, face);
    }

    if (elapsed >= s_roll_duration_ms) {
        // 结束摇动，给一个最终结果
        if (s_roll_timer) {
            lv_timer_del(s_roll_timer);
            s_roll_timer = NULL;
        }
        for (uint8_t i = 0; i < s_dice_count; i++) {
            int face = ((int)(esp_random() % 6U)) + 1;
            dice_set_face(i, face);
            ESP_LOGI(TAG, "dice %d result: %d", (int)i + 1, face);
        }

        s_is_rolling = false;
    }
}

static void dice_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_indev_get_act();
        if (indev) {
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            s_touch_start_x = p.x;
            s_touch_start_y = p.y;
        }
        return;
    }

    if (code != LV_EVENT_RELEASED) {
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) {
        return;
    }

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    lv_coord_t dx = p.x - s_touch_start_x;
    lv_coord_t dy = p.y - s_touch_start_y;

    if ((dx > 30) && (dx > dice_abs(dy))) {
        dice_change_count(1);
        return;
    }

    if ((dx < -30) && (-dx > dice_abs(dy))) {
        dice_change_count(-1);
        return;
    }

    dice_start_roll();
}

static void dice_change_count(int delta)
{
    int new_count = (int)s_dice_count + delta;
    if (new_count < 1) {
        new_count = 1;
    }
    if (new_count > MAX_DICE) {
        new_count = MAX_DICE;
    }

    if (new_count == (int)s_dice_count) {
        return;
    }

    s_dice_count = (uint8_t)new_count;
    dice_update_layout();
}

static void dice_start_roll(void)
{
    if (s_is_rolling) {
        return;
    }

    s_is_rolling = true;
    s_roll_start_tick = lv_tick_get();

    if (s_roll_timer) {
        lv_timer_del(s_roll_timer);
        s_roll_timer = NULL;
    }

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
