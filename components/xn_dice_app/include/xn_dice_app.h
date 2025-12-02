/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-11-22 13:43:50
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-12-01 16:56:00
 * @FilePath: \xn_esp32_dice\components\xn_dice_app\include\xn_dice_app.h
 * @Description: esp32 骰子应用接口
 */

#pragma once

#include "esp_err.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化骰子应用
 *
 * 完成 LVGL 驱动、屏幕和触摸初始化，并创建骰子界面。
 *
 * @return ESP_OK 成功，其他为错误码
 */
esp_err_t xn_dice_app_init(void);

void xn_dice_app_set_results(const int values[6]);
void xn_dice_app_show(bool show);

#ifdef __cplusplus
}
#endif
