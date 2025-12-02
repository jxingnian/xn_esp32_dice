/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-11-22 13:43:50
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-12-02 11:22:31
 * @FilePath: \xn_esp32_dice\main\main.c
 * @Description: esp32 骰子演示 By.星年
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_random.h"
#include "esp_log.h"
#include "lvgl.h"
#include "xn_lvgl.h"
#include "xn_lottie_manager.h"
#include "xn_dice_app.h"

typedef enum {
    APP_STATE_IDLE_COOL = 0,
    APP_STATE_ROLLING_DICE,
    APP_STATE_SHOW_RESULT,
} app_state_t;

static const char *TAG = "APP_MAIN";

static app_state_t s_app_state = APP_STATE_IDLE_COOL;
static lv_timer_t *s_dice_timer = NULL;
static int s_results[6] = {1, 1, 1, 1, 1, 1};

#define DICE_RESULT_COUNT      6
#define DICE_ANIM_DURATION_MS  4200

static void app_on_screen_click(lv_event_t *e);
static void app_on_dice_anim_done(lv_timer_t *timer);

void app_main(void)
{
    printf("esp32 dice demo By.星年\n");
    ESP_LOGI(TAG, "app_main start");

    // 初始化 Lottie 管理器（内部完成 LVGL 驱动、屏幕和触摸初始化）
    esp_err_t ret = xn_lottie_manager_init(NULL);
    if (ret != ESP_OK) {
        printf("xn_lottie_manager_init failed: 0x%08" PRIx32 "\n", (uint32_t)ret);
        ESP_LOGE(TAG, "xn_lottie_manager_init failed: 0x%08" PRIx32, (uint32_t)ret);
        return;
    }

    // 初始化骰子结果页（仅创建 UI，不处理逻辑）
    ret = xn_dice_app_init();
    if (ret != ESP_OK) {
        printf("xn_dice_app_init failed: 0x%08" PRIx32 "\n", (uint32_t)ret);
        ESP_LOGE(TAG, "xn_dice_app_init failed: 0x%08" PRIx32, (uint32_t)ret);
    }

    // 整个屏幕响应点击，用于触发摇骰
    lv_lock();
    lv_obj_t *screen = lv_screen_active();
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, app_on_screen_click, LV_EVENT_CLICKED, NULL);
    lv_unlock();

    // 进入待机状态：播放 COOL 动画
    s_app_state = APP_STATE_IDLE_COOL;
    lottie_manager_play_anim(LOTTIE_ANIM_COOL);

    ESP_LOGI(TAG, "enter idle state, play COOL anim");
}

static void app_on_screen_click(lv_event_t *e)
{
    LV_UNUSED(e);

    if (s_app_state == APP_STATE_ROLLING_DICE) {
        // 动画进行中，忽略新的点击
        return;
    }

    ESP_LOGI(TAG, "screen clicked, state=%d", s_app_state);

    // 生成 6 个 1~6 的随机点数
    for (int i = 0; i < DICE_RESULT_COUNT; i++) {
        s_results[i] = (int)(esp_random() % 6U) + 1;
    }

    ESP_LOGI(TAG, "dice results: %d %d %d %d %d %d",
             s_results[0], s_results[1], s_results[2],
             s_results[3], s_results[4], s_results[5]);

    // 隐藏结果页，播放骰子动画
    ESP_LOGI(TAG, "call xn_dice_app_show(false)");
    xn_dice_app_show(false);
    ESP_LOGI(TAG, "xn_dice_app_show(false) returned");

    ESP_LOGI(TAG, "start DICE anim");
    lottie_manager_play_anim(LOTTIE_ANIM_DICE);

    // 为骰子动画创建一个一次性的结束定时器
    if (s_dice_timer) {
        lv_timer_del(s_dice_timer);
        s_dice_timer = NULL;
    }
    s_dice_timer = lv_timer_create(app_on_dice_anim_done, DICE_ANIM_DURATION_MS, NULL);

    ESP_LOGI(TAG, "dice timer created (%d ms)", DICE_ANIM_DURATION_MS);

    s_app_state = APP_STATE_ROLLING_DICE;
}

static void app_on_dice_anim_done(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    ESP_LOGI(TAG, "dice anim done callback, state=%d", s_app_state);

    if (s_dice_timer) {
        lv_timer_del(s_dice_timer);
        s_dice_timer = NULL;
    }

    // 停止当前动画
    lottie_manager_stop_anim(-1);

    // 显示结果页并更新 6 颗骰子的点数
    xn_dice_app_set_results(s_results);
    xn_dice_app_show(true);

    ESP_LOGI(TAG, "show dice result page");

    s_app_state = APP_STATE_SHOW_RESULT;
}
