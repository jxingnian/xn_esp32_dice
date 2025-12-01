/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-11-22 13:43:50
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-11-26 21:40:08
 * @FilePath: \xn_esp32_lottie\main\main.c
 * @Description: esp32 lottie动画播放 By.星年
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "xn_lottie_manager.h"

void app_main(void)
{
    printf("esp32 lottie动画播放 By.星年\n");
    // 初始化 Lottie 管理器（内部完成 SPIFFS / LVGL / 屏幕等初始化）
    esp_err_t ret_lottie = xn_lottie_manager_init(NULL);
    if (ret_lottie == ESP_OK) {
        // 播放一个测试动画（加载动画）
        printf("播放测试动画...\n");
        lottie_manager_play_anim(LOTTIE_ANIM_COOL);
    }
}
