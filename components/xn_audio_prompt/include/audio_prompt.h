/*
 * @Author: AI Assistant
 * @Description: 音效播放模块 - 基于预加载缓存的PCM音效播放
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 预定义音效类型
 */
typedef enum {
    AUDIO_PROMPT_BEEP,          ///< 短促蜂鸣（通用提示）
    AUDIO_PROMPT_SUCCESS,       ///< 成功提示音
    AUDIO_PROMPT_ERROR,         ///< 错误提示音
    AUDIO_PROMPT_WAKEUP,        ///< 唤醒提示音
    AUDIO_PROMPT_THINKING,      ///< 思考中提示音
    AUDIO_PROMPT_VERSION_UPDATE,///< 版本更新提示音
    AUDIO_PROMPT_DICE,          ///< 骰子摇动音效
    AUDIO_PROMPT_MAX            ///< 音效类型数量（不要使用）
} audio_prompt_type_t;

/**
 * @brief 初始化音效模块
 * @note 会预加载所有音效到PSRAM，建议在系统启动时调用
 * @return
 *      - ESP_OK: 成功
 *      - ESP_ERR_NO_MEM: 内存不足
 *      - ESP_ERR_NOT_FOUND: 音效文件未找到
 */
esp_err_t audio_prompt_init(void);

/**
 * @brief 反初始化音效模块
 * @note 释放所有预加载的音效内存
 */
void audio_prompt_deinit(void);

/**
 * @brief 播放预定义音效
 * @param type 音效类型
 * @return
 *      - ESP_OK: 成功
 *      - ESP_ERR_INVALID_ARG: 无效的音效类型
 *      - ESP_ERR_INVALID_STATE: 模块未初始化
 *      - ESP_ERR_NOT_FOUND: 音效未加载
 */
esp_err_t audio_prompt_play(audio_prompt_type_t type);

/**
 * @brief 播放自定义PCM文件（不使用缓存）
 * @param filename PCM文件路径（如 "/spiffs/custom.pcm"）
 * @note 此函数每次从文件读取，不使用缓存
 * @return
 *      - ESP_OK: 成功
 *      - ESP_ERR_INVALID_ARG: 文件名为空
 *      - ESP_ERR_NOT_FOUND: 文件不存在
 *      - ESP_ERR_NO_MEM: 内存不足
 */
esp_err_t audio_prompt_play_file(const char *filename);

/**
 * @brief 停止当前播放
 * @note 会清空播放缓冲区
 */
void audio_prompt_stop(void);

/**
 * @brief 检查音效是否已加载
 * @param type 音效类型
 * @return true 已加载，false 未加载
 */
bool audio_prompt_is_loaded(audio_prompt_type_t type);

/**
 * @brief 获取音效信息
 * @param type 音效类型
 * @param[out] samples 采样点数（可选，传NULL跳过）
 * @param[out] duration_ms 时长（毫秒，可选，传NULL跳过）
 * @return
 *      - ESP_OK: 成功
 *      - ESP_ERR_INVALID_ARG: 无效的音效类型
 *      - ESP_ERR_NOT_FOUND: 音效未加载
 */
esp_err_t audio_prompt_get_info(audio_prompt_type_t type, size_t *samples, uint32_t *duration_ms);

esp_err_t audio_prompt_start_loop(audio_prompt_type_t type);

void audio_prompt_stop_loop(void);

#ifdef __cplusplus
}
#endif

