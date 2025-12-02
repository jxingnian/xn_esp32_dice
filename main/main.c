/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-11-22 13:43:50
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-12-02 10:13:43
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
#include "lvgl.h"
#include "xn_lvgl.h"
#include "xn_lottie_manager.h"
#include "xn_dice_app.h"

typedef enum {
    APP_STATE_IDLE_COOL = 0,
    APP_STATE_ROLLING_DICE,
    APP_STATE_SHOW_RESULT,
} app_state_t;

static app_state_t s_app_state = APP_STATE_IDLE_COOL;
static lv_timer_t *s_dice_timer = NULL;
static lv_timer_t *s_idle_timer = NULL;
static uint32_t s_last_interaction_tick = 0;
static int s_results[6] = {1, 1, 1, 1, 1, 1};

#define DICE_RESULT_COUNT      6
#define DICE_ANIM_DURATION_MS  4200
#define RESULT_IDLE_TIMEOUT_MS 15000

static void app_on_screen_click(lv_event_t *e);
static void app_on_dice_anim_done(lv_timer_t *timer);
static void app_on_idle_timer(lv_timer_t *timer);

void app_main(void)
{
    printf("esp32 dice demo By.星年\n");

    // 初始化 Lottie 管理器（内部完成 LVGL 驱动、屏幕和触摸初始化）
    esp_err_t ret = xn_lottie_manager_init(NULL);
    if (ret != ESP_OK) {
        printf("xn_lottie_manager_init failed: 0x%08" PRIx32 "\n", (uint32_t)ret);
        return;
    }

    // 初始化骰子结果页（仅创建 UI，不处理逻辑）
    ret = xn_dice_app_init();
    if (ret != ESP_OK) {
        printf("xn_dice_app_init failed: 0x%08" PRIx32 "\n", (uint32_t)ret);
    }

    // 整个屏幕响应点击，用于触发摇骰
    lv_lock();
    lv_obj_t *screen = lv_screen_active();
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, app_on_screen_click, LV_EVENT_CLICKED, NULL);
    lv_unlock();

    // 进入待机状态：播放 COOL 动画
    s_app_state = APP_STATE_IDLE_COOL;
    s_last_interaction_tick = lv_tick_get();
    lottie_manager_play_anim(LOTTIE_ANIM_COOL);

    // 周期检查空闲时间，在结果页长时间无操作时回到 cool 动画
    s_idle_timer = lv_timer_create(app_on_idle_timer, 1000, NULL);
}

static void app_on_screen_click(lv_event_t *e)
{
    LV_UNUSED(e);

    s_last_interaction_tick = lv_tick_get();

    if (s_app_state == APP_STATE_ROLLING_DICE) {
        // 动画进行中，忽略新的点击
        return;
    }

    // 生成 6 个 1~6 的随机点数
    for (int i = 0; i < DICE_RESULT_COUNT; i++) {
        s_results[i] = (int)(esp_random() % 6U) + 1;
    }

    // 隐藏结果页，播放骰子动画
    xn_dice_app_show(false);
    lottie_manager_play_anim(LOTTIE_ANIM_DICE);

    // 为骰子动画创建一个一次性的结束定时器
    if (s_dice_timer) {
        lv_timer_del(s_dice_timer);
        s_dice_timer = NULL;
    }
    s_dice_timer = lv_timer_create(app_on_dice_anim_done, DICE_ANIM_DURATION_MS, NULL);

    s_app_state = APP_STATE_ROLLING_DICE;
}

static void app_on_dice_anim_done(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (s_dice_timer) {
        lv_timer_del(s_dice_timer);
        s_dice_timer = NULL;
    }

    // 停止当前动画
    lottie_manager_stop_anim(-1);

    // 显示结果页并更新 6 颗骰子的点数
    xn_dice_app_set_results(s_results);
    xn_dice_app_show(true);

    s_app_state = APP_STATE_SHOW_RESULT;
    s_last_interaction_tick = lv_tick_get();
}

static void app_on_idle_timer(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (s_app_state != APP_STATE_SHOW_RESULT) {
        return;
    }

    uint32_t elapsed = lv_tick_elaps(s_last_interaction_tick);
    if (elapsed < RESULT_IDLE_TIMEOUT_MS) {
        return;
    }

    // 结果页长时间无操作，回到待机 COOL 动画
    xn_dice_app_show(false);
    lottie_manager_play_anim(LOTTIE_ANIM_COOL);
    s_app_state = APP_STATE_IDLE_COOL;
}
