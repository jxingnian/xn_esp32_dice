/*
 * @Author: AI Assistant
 * @Description: 音效播放模块实现 - 预加载缓存方案
 */

#include "audio_prompt.h"
#include "audio_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "AUDIO_PROMPT";

// ============ 音效文件定义 ============

typedef struct {
    const char *filename;       // PCM文件路径
    int16_t *data;              // 预加载的音频数据（PSRAM）
    size_t samples;             // 采样点数
    bool loaded;                // 是否已加载
} prompt_info_t;

// 音效文件路径映射表
static prompt_info_t s_prompts[AUDIO_PROMPT_MAX] = {
    {"/prompt_spiffs/beep.pcm", NULL, 0, false},
    {"/prompt_spiffs/success.pcm", NULL, 0, false},
    {"/prompt_spiffs/error.pcm", NULL, 0, false},
    {"/prompt_spiffs/wakeup.pcm", NULL, 0, false},
    {"/prompt_spiffs/thinking.pcm", NULL, 0, false},
    {"/prompt_spiffs/version_update.pcm", NULL, 0, false},
    {"/prompt_spiffs/dice.pcm", NULL, 0, false},
};

static bool s_initialized = false;

static TaskHandle_t s_loop_task = NULL;
static bool s_loop_running = false;
static audio_prompt_type_t s_loop_type = AUDIO_PROMPT_BEEP;

// ============ 内部函数 ============

static esp_err_t audio_prompt_mount_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/prompt_spiffs",
        .partition_label = "prompt_spiffs",
        .max_files = 8,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spiffs register failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief 加载单个音效到内存
 */
static esp_err_t load_prompt(audio_prompt_type_t type)
{
    if (type >= AUDIO_PROMPT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    prompt_info_t *prompt = &s_prompts[type];

    // 检查文件是否存在
    struct stat st;
    if (stat(prompt->filename, &st) != 0) {
        ESP_LOGW(TAG, "音效文件不存在: %s", prompt->filename);
        return ESP_ERR_NOT_FOUND;
    }

    size_t file_size = st.st_size;
    if (file_size == 0 || file_size % 2 != 0) {
        ESP_LOGE(TAG, "无效的PCM文件大小: %d bytes", (int)file_size);
        return ESP_ERR_INVALID_SIZE;
    }

    // 打开文件
    FILE *fp = fopen(prompt->filename, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "无法打开音效文件: %s", prompt->filename);
        return ESP_ERR_NOT_FOUND;
    }

    // 分配PSRAM内存
    prompt->samples = file_size / sizeof(int16_t);
    prompt->data = (int16_t *)heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (!prompt->data) {
        ESP_LOGE(TAG, "音效内存分配失败: %d bytes", (int)file_size);
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    // 读取文件
    size_t read_bytes = fread(prompt->data, 1, file_size, fp);
    fclose(fp);

    if (read_bytes != file_size) {
        ESP_LOGE(TAG, "音效文件读取失败: %s", prompt->filename);
        heap_caps_free(prompt->data);
        prompt->data = NULL;
        return ESP_FAIL;
    }

    prompt->loaded = true;

    float duration_ms = (prompt->samples * 1000.0f) / 16000.0f;
    ESP_LOGI(TAG, "✅ 音效已加载: %s (%d samples, %.1f ms, %.1f KB)",
             prompt->filename,
             (int)prompt->samples,
             duration_ms,
             file_size / 1024.0f);

    return ESP_OK;
}

/**
 * @brief 卸载单个音效
 */
static void unload_prompt(audio_prompt_type_t type)
{
    if (type >= AUDIO_PROMPT_MAX) return;

    prompt_info_t *prompt = &s_prompts[type];
    if (prompt->data) {
        heap_caps_free(prompt->data);
        prompt->data = NULL;
    }
    prompt->samples = 0;
    prompt->loaded = false;
}

static void audio_prompt_loop_task(void *arg)
{
    (void)arg;

    for (;;) {
        if (!s_loop_running) {
            break;
        }

        if (!s_initialized) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (s_loop_type >= AUDIO_PROMPT_MAX) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        prompt_info_t *prompt = &s_prompts[s_loop_type];
        if (!prompt->loaded || !prompt->data || prompt->samples == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t free_samples = audio_manager_get_playback_free_space();
        if (free_samples >= prompt->samples) {
            esp_err_t ret = audio_manager_play_audio(prompt->data, prompt->samples);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "loop play failed: %s", esp_err_to_name(ret));
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    s_loop_task = NULL;
    s_loop_running = false;
    vTaskDelete(NULL);
}

// ============ 公共API实现 ============

esp_err_t audio_prompt_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "音效模块已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "======== 初始化音效模块 ========");

    esp_err_t ret = audio_prompt_mount_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "挂载音效 SPIFFS 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    int loaded_count = 0;
    int failed_count = 0;

    // 预加载所有音效
    for (int i = 0; i < AUDIO_PROMPT_MAX; i++) {
        esp_err_t load_ret = load_prompt(i);
        if (load_ret == ESP_OK) {
            loaded_count++;
        } else {
            failed_count++;
            // 音效加载失败不是致命错误，继续加载其他音效
            ESP_LOGW(TAG, "音效 %d 加载失败，将跳过", i);
        }
    }

    if (loaded_count == 0) {
        ESP_LOGE(TAG, "❌ 所有音效加载失败");
        return ESP_ERR_NOT_FOUND;
    }

    s_initialized = true;

    ESP_LOGI(TAG, "✅ 音效模块初始化完成: 成功加载 %d/%d 个音效",
             loaded_count, AUDIO_PROMPT_MAX);

    return ESP_OK;
}

void audio_prompt_deinit(void)
{
    if (!s_initialized) return;

    ESP_LOGI(TAG, "卸载音效模块...");

    // 卸载所有音效
    for (int i = 0; i < AUDIO_PROMPT_MAX; i++) {
        unload_prompt(i);
    }

    s_initialized = false;
    ESP_LOGI(TAG, "音效模块已卸载");
}

esp_err_t audio_prompt_play(audio_prompt_type_t type)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "音效模块未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (type >= AUDIO_PROMPT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    prompt_info_t *prompt = &s_prompts[type];

    if (!prompt->loaded || !prompt->data) {
        ESP_LOGW(TAG, "音效 %d 未加载", type);
        return ESP_ERR_NOT_FOUND;
    }

    // 确保播放任务运行
    audio_manager_start_playback();

    // 播放音效
    esp_err_t ret = audio_manager_play_audio(prompt->data, prompt->samples);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "播放音效 %d (%s)", type, prompt->filename);
    } else {
        ESP_LOGW(TAG, "音效播放失败: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t audio_prompt_play_file(const char *filename)
{
    if (!filename) {
        return ESP_ERR_INVALID_ARG;
    }

    // 检查文件是否存在
    struct stat st;
    if (stat(filename, &st) != 0) {
        ESP_LOGE(TAG, "文件不存在: %s", filename);
        return ESP_ERR_NOT_FOUND;
    }

    size_t file_size = st.st_size;
    if (file_size == 0 || file_size % 2 != 0) {
        ESP_LOGE(TAG, "无效的PCM文件大小: %d bytes", (int)file_size);
        return ESP_ERR_INVALID_SIZE;
    }

    // 打开文件
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "无法打开文件: %s", filename);
        return ESP_ERR_NOT_FOUND;
    }

    // 分配临时缓冲区
    int16_t *buffer = (int16_t *)malloc(file_size);
    if (!buffer) {
        ESP_LOGE(TAG, "内存分配失败: %d bytes", (int)file_size);
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    // 读取文件
    size_t read_bytes = fread(buffer, 1, file_size, fp);
    fclose(fp);

    if (read_bytes != file_size) {
        ESP_LOGE(TAG, "文件读取失败: %s", filename);
        free(buffer);
        return ESP_FAIL;
    }

    // 确保播放任务运行
    audio_manager_start_playback();

    // 播放
    size_t samples = file_size / sizeof(int16_t);
    esp_err_t ret = audio_manager_play_audio(buffer, samples);

    free(buffer);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "播放文件: %s (%d samples)", filename, (int)samples);
    }

    return ret;
}

void audio_prompt_stop(void)
{
    // 停止播放任务（会清空播放缓冲区）
    audio_manager_stop_playback();
}

bool audio_prompt_is_loaded(audio_prompt_type_t type)
{
    if (type >= AUDIO_PROMPT_MAX) {
        return false;
    }

    return s_prompts[type].loaded;
}

esp_err_t audio_prompt_get_info(audio_prompt_type_t type, size_t *samples, uint32_t *duration_ms)
{
    if (type >= AUDIO_PROMPT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    prompt_info_t *prompt = &s_prompts[type];

    if (!prompt->loaded) {
        return ESP_ERR_NOT_FOUND;
    }

    if (samples) {
        *samples = prompt->samples;
    }

    if (duration_ms) {
        *duration_ms = (prompt->samples * 1000) / 16000;
    }

    return ESP_OK;
}

esp_err_t audio_prompt_start_loop(audio_prompt_type_t type)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "音效模块未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (type >= AUDIO_PROMPT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    prompt_info_t *prompt = &s_prompts[type];
    if (!prompt->loaded || !prompt->data || prompt->samples == 0) {
        ESP_LOGW(TAG, "音效 %d 未加载，无法循环播放", type);
        return ESP_ERR_NOT_FOUND;
    }

    s_loop_type = type;

    if (s_loop_running && s_loop_task != NULL) {
        // 已经在循环播放，直接返回
        return ESP_OK;
    }

    // 确保播放任务运行
    audio_manager_start_playback();

    s_loop_running = true;

    if (xTaskCreatePinnedToCore(audio_prompt_loop_task,
                                "dice_snd_loop",
                                3 * 1024,
                                NULL,
                                5,
                                &s_loop_task,
                                1) != pdPASS) {
        ESP_LOGE(TAG, "创建音效循环任务失败");
        s_loop_running = false;
        s_loop_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void audio_prompt_stop_loop(void)
{
    if (!s_loop_running) {
        return;
    }

    s_loop_running = false;

    // 清空播放缓冲区，避免残余音频继续播放
    audio_manager_clear_playback_buffer();
}

