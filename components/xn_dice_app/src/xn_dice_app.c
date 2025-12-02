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

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_card = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_value_label = NULL;
static int s_value = 1;
static bool s_inited = false;

static void dice_result_create_ui(void);
static void dice_result_update_value(void);

static void dice_result_create_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    if (!screen) {
        ESP_LOGE(TAG, "no active screen");
        return;
    }

    // 顶层容器作为结果页根节点
    s_root = lv_obj_create(screen);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_center(s_root);

    // 深色渐变背景，营造安静的氛围
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x020617), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(s_root, lv_color_hex(0x0f172a), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(s_root, LV_GRAD_DIR_VER, LV_PART_MAIN);

    // 中间卡片，突出结果
    s_card = lv_obj_create(s_root);
    lv_obj_set_size(s_card, lv_pct(70), lv_pct(55));
    lv_obj_center(s_card);
    lv_obj_set_style_bg_color(s_card, lv_color_hex(0x020617), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_card, 24, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_card, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_card, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(s_card, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(s_card, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(s_card, 12, LV_PART_MAIN);

    // 顶部标题
    s_title_label = lv_label_create(s_card);
    lv_label_set_text(s_title_label, "骰子点数");
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(0x9ca3af), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_title_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 16);

    // 中央大号数字
    s_value_label = lv_label_create(s_card);
    lv_label_set_text(s_value_label, "1");
    lv_obj_set_style_text_color(s_value_label, lv_color_hex(0xf9fafb), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_value_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(s_value_label, LV_ALIGN_CENTER, 0, 0);

    // 底部提示
    lv_obj_t *hint = lv_label_create(s_root);
    lv_label_set_text(hint, "轻触屏幕重新摇骰子");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x6b7280), LV_PART_MAIN);
    lv_obj_set_style_text_opa(hint, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);

    // 默认隐藏，等待 Lottie 骰子动画结束后再显示
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);

    dice_result_update_value();
}

static void dice_result_update_value(void)
{
    if (!s_value_label) {
        return;
    }

    if (s_value < 1) {
        s_value = 1;
    }
    if (s_value > 6) {
        s_value = 6;
    }

    lv_label_set_text_fmt(s_value_label, "%d", s_value);
}

esp_err_t xn_dice_app_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "init dice result page");

    // 在应用任务中创建界面，需要使用 LVGL 锁
    lv_lock();
    dice_result_create_ui();
    lv_unlock();

    s_inited = true;
    return ESP_OK;
}

void xn_dice_app_set_value(int value)
{
    s_value = value;

    if (!s_inited) {
        return;
    }

    dice_result_update_value();
}

void xn_dice_app_show(bool show)
{
    if (!s_root) {
        return;
    }

    if (show) {
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
}
