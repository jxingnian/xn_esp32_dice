/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-11-22 13:43:50
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-12-01 17:25:40
 * @FilePath: \xn_esp32_dice\components\xn_dice_app\src\xn_dice_app.c
 * @Description: esp32 骰子应用实现（LVGL 优雅骰子界面）
 */

#include "xn_dice_app.h"

#include "lvgl.h"
#include "xn_lvgl.h"
#include "esp_log.h"

static const char *TAG = "XN_DICE_APP";

#define DICE_COUNT 6

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

typedef struct {
    lv_obj_t *root;
    lv_obj_t *pips[PIP_COUNT];
} dice_view_t;

static lv_obj_t *s_root = NULL;
static dice_view_t s_dice_views[DICE_COUNT];
static int s_values[DICE_COUNT] = {1, 1, 1, 1, 1, 1};
static bool s_inited = false;

static void dice_result_create_ui(void);
static void dice_update_layout(lv_coord_t dice_size, lv_coord_t gap);
static void dice_set_face(int index, int value);

static void dice_result_create_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    if (!screen) {
        ESP_LOGE(TAG, "no active screen");
        return;
    }

    // 顶层容器作为结果页根节点，保持透明，只负责承载 6 颗骰子
    s_root = lv_obj_create(screen);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_center(s_root);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, LV_PART_MAIN);

    const lv_coord_t dice_size = 90;
    const lv_coord_t pip_size  = 18;
    const lv_coord_t offset    = dice_size / 3;   // 点距中心偏移

    for (int i = 0; i < DICE_COUNT; i++) {
        dice_view_t *view = &s_dice_views[i];

        view->root = lv_obj_create(s_root);
        lv_obj_remove_style_all(view->root);
        lv_obj_set_size(view->root, dice_size, dice_size);

        // 简约白色骰子方块
        lv_obj_set_style_bg_color(view->root, lv_color_hex(0xf9fafb), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(view->root, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(view->root, 16, LV_PART_MAIN);
        lv_obj_set_style_border_width(view->root, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(view->root, lv_color_hex(0xe5e7eb), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(view->root, 18, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(view->root, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(view->root, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_y(view->root, 8, LV_PART_MAIN);

        // 创建 7 个圆点
        for (int p = 0; p < PIP_COUNT; p++) {
            lv_obj_t *pip = lv_obj_create(view->root);
            view->pips[p] = pip;
            lv_obj_remove_style_all(pip);
            lv_obj_set_size(pip, pip_size, pip_size);
            lv_obj_set_style_bg_color(pip, lv_color_hex(0x111827), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(pip, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(pip, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        }

        // 计算相对位置（以方块中心为基准）
        lv_coord_t center_x = dice_size / 2 - pip_size / 2;
        lv_coord_t center_y = dice_size / 2 - pip_size / 2;

        lv_obj_set_pos(view->pips[PIP_CENTER],       center_x,                 center_y);
        lv_obj_set_pos(view->pips[PIP_TOP_LEFT],     center_x - offset,        center_y - offset);
        lv_obj_set_pos(view->pips[PIP_TOP_RIGHT],    center_x + offset,        center_y - offset);
        lv_obj_set_pos(view->pips[PIP_MID_LEFT],     center_x - offset,        center_y);
        lv_obj_set_pos(view->pips[PIP_MID_RIGHT],    center_x + offset,        center_y);
        lv_obj_set_pos(view->pips[PIP_BOTTOM_LEFT],  center_x - offset,        center_y + offset);
        lv_obj_set_pos(view->pips[PIP_BOTTOM_RIGHT], center_x + offset,        center_y + offset);

        dice_set_face(i, s_values[i]);
    }

    // 将 6 颗骰子排成 2 行 3 列，居中布局
    dice_update_layout(dice_size, 18);

    // 默认隐藏，等待 Lottie 骰子动画结束后再显示
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

static void dice_update_layout(lv_coord_t dice_size, lv_coord_t gap)
{
    lv_display_t *disp = lv_display_get_default();
    lv_coord_t scr_w = disp ? lv_display_get_horizontal_resolution(disp) : 320;
    lv_coord_t scr_h = disp ? lv_display_get_vertical_resolution(disp) : 240;

    lv_coord_t cx = scr_w / 2;
    lv_coord_t cy = scr_h / 2;

    lv_coord_t row_y[2];
    row_y[0] = cy - dice_size / 2 - gap / 2;
    row_y[1] = cy + dice_size / 2 + gap / 2;

    lv_coord_t col_x[3];
    col_x[0] = cx - dice_size - gap;
    col_x[1] = cx;
    col_x[2] = cx + dice_size + gap;

    for (int i = 0; i < DICE_COUNT; i++) {
        dice_view_t *view = &s_dice_views[i];
        if (!view->root) {
            continue;
        }

        int row = i / 3;
        int col = i % 3;

        lv_coord_t x = col_x[col] - dice_size / 2;
        lv_coord_t y = row_y[row] - dice_size / 2;

        lv_obj_set_pos(view->root, x, y);
    }
}

static void dice_set_face(int index, int value)
{
    if (index < 0 || index >= DICE_COUNT) {
        return;
    }

    if (value < 1 || value > 6) {
        value = 1;
    }

    dice_view_t *view = &s_dice_views[index];
    if (!view->root) {
        return;
    }

    // 先隐藏所有点
    for (int i = 0; i < PIP_COUNT; i++) {
        lv_obj_add_flag(view->pips[i], LV_OBJ_FLAG_HIDDEN);
    }

    switch (value) {
    case 1:
        lv_obj_clear_flag(view->pips[PIP_CENTER], LV_OBJ_FLAG_HIDDEN);
        break;
    case 2:
        lv_obj_clear_flag(view->pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 3:
        lv_obj_clear_flag(view->pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_CENTER], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 4:
        lv_obj_clear_flag(view->pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_TOP_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_BOTTOM_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 5:
        lv_obj_clear_flag(view->pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_TOP_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_CENTER], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_BOTTOM_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    case 6:
        lv_obj_clear_flag(view->pips[PIP_TOP_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_TOP_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_MID_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_MID_RIGHT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_BOTTOM_LEFT], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->pips[PIP_BOTTOM_RIGHT], LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        break;
    }
}

esp_err_t xn_dice_app_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "init dice result page (6 dice)");

    lv_lock();
    dice_result_create_ui();
    lv_unlock();

    s_inited = true;
    return ESP_OK;
}

void xn_dice_app_set_results(const int values[6])
{
    if (!values) {
        return;
    }

    for (int i = 0; i < DICE_COUNT; i++) {
        int v = values[i];
        if (v < 1 || v > 6) {
            v = 1;
        }
        s_values[i] = v;
    }

    if (!s_inited) {
        return;
    }

    lv_lock();
    for (int i = 0; i < DICE_COUNT; i++) {
        dice_set_face(i, s_values[i]);
    }
    lv_unlock();
}

void xn_dice_app_show(bool show)
{
    if (!s_root) {
        return;
    }

    lv_lock();
    if (show) {
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
    lv_unlock();
}
