/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-11-22 13:43:50
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-11-26 21:40:08
 * @FilePath: \xn_esp32_dice\main\main.c
 * @Description: esp32 骰子演示 By.星年
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "xn_dice_app.h"

void app_main(void)
{
    printf("esp32 dice demo By.星年\n");

    // 初始化骰子应用（内部完成 LVGL / 屏幕 / 触摸初始化，并创建骰子界面）
    esp_err_t ret = xn_dice_app_init();
    if (ret != ESP_OK) {
        printf("xn_dice_app_init failed: 0x%08x\n", (unsigned int)ret);
    }

    // 所有 LVGL 任务和界面更新在后台任务中运行，这里不用再创建额外循环
}
