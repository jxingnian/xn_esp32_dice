/*
 * @Author: xingnian j_xingnian@163.com
 * @Date: 2025-08-30 16:30:00
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-12-01 11:05:31
 * @FilePath: \xn_esp32_lottie\components\xn_bsp_spd2010\src\bsp_touch_spd2010.c
 * @Description: SPD2010 触摸驱动实现（参考官方开发板例程 Touch_SPD2010.c，使用自定义协议）
 */

#include "bsp_touch_spd2010.h"
#include "bsp_i2c_driver.h"
#include "bsp_exio_tca9554.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TOUCH_SPD2010";

/*********************
 * SPD2010 协议相关宏
 *********************/

// SPD2010 I2C 地址（与 temp 示例保持一致）
#define SPD2010_ADDR                    (0x53)

// 最大触摸点数（与 temp 中 CONFIG_ESP_LCD_TOUCH_MAX_POINTS 对齐）
#define SPD2010_MAX_POINTS              (5)

typedef struct {
    uint8_t id;
    uint16_t x;
    uint16_t y;
    uint8_t weight;
} tp_report_t;

typedef struct {
    tp_report_t rpt[10];
    uint8_t touch_num;     // Number of touch points
    uint8_t pack_code;
    uint8_t down;
    uint8_t up;
    uint8_t gesture;
    uint16_t down_x;
    uint16_t down_y;
    uint16_t up_x;
    uint16_t up_y;
} SPD2010_Touch;

typedef struct {
    uint8_t none0;
    uint8_t none1;
    uint8_t none2;
    uint8_t cpu_run;
    uint8_t tint_low;
    uint8_t tic_in_cpu;
    uint8_t tic_in_bios;
    uint8_t tic_busy;
} tp_status_high_t;

typedef struct {
    uint8_t pt_exist;
    uint8_t gesture;
    uint8_t key;
    uint8_t aux;
    uint8_t keep;
    uint8_t raw_or_pt;
    uint8_t none6;
    uint8_t none7;
} tp_status_low_t;

typedef struct {
    tp_status_low_t status_low;
    tp_status_high_t status_high;
    uint16_t read_len;
} tp_status_t;

typedef struct {
    uint8_t status;
    uint16_t next_packet_len;
} tp_hdp_status_t;

// 全局触摸数据（与 temp 示例保持一致）
static SPD2010_Touch s_touch_data = {0};

/*********************
 * I2C 16 位寄存器访问封装（基于当前工程 I2C 驱动）
 *********************/

static esp_err_t I2C_Read_Touch(uint8_t driver_addr, uint16_t reg_addr, uint8_t *reg_data, uint32_t length)
{
    if (!reg_data || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (I2C_Get_Bus_Handle() == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 使用临时设备配置，16 位寄存器地址通过首发 2 字节写入实现
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = driver_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev_handle;
    esp_err_t ret = i2c_master_bus_add_device(I2C_Get_Bus_Handle(), &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add touch device: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t addr_buf[2];
    addr_buf[0] = (uint8_t)(reg_addr >> 8);
    addr_buf[1] = (uint8_t)reg_addr;

    ret = i2c_master_transmit_receive(dev_handle, addr_buf, 2, reg_data, length, I2C_MASTER_TIMEOUT_MS);

    i2c_master_bus_rm_device(dev_handle);
    return ret;
}

static esp_err_t I2C_Write_Touch(uint8_t driver_addr, uint16_t reg_addr, const uint8_t *reg_data, uint32_t length)
{
    if (!reg_data || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (I2C_Get_Bus_Handle() == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = driver_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev_handle;
    esp_err_t ret = i2c_master_bus_add_device(I2C_Get_Bus_Handle(), &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add touch device: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t *buf = malloc(length + 2);
    if (!buf) {
        i2c_master_bus_rm_device(dev_handle);
        return ESP_ERR_NO_MEM;
    }

    buf[0] = (uint8_t)(reg_addr >> 8);
    buf[1] = (uint8_t)reg_addr;
    memcpy(&buf[2], reg_data, length);

    ret = i2c_master_transmit(dev_handle, buf, length + 2, I2C_MASTER_TIMEOUT_MS);

    free(buf);
    i2c_master_bus_rm_device(dev_handle);
    return ret;
}

/*********************
 * SPD2010 命令序列封装
 *********************/

static esp_err_t write_tp_point_mode_cmd(void)
{
    uint8_t data[2];
    uint16_t addr;

    data[0] = 0x00;
    data[1] = 0x00;
    addr = ((uint16_t)0x50 << 8) | 0x00;

    esp_err_t ret = I2C_Write_Touch(SPD2010_ADDR, addr, data, 2);
    esp_rom_delay_us(200);
    return ret;
}

static esp_err_t write_tp_start_cmd(void)
{
    uint8_t data[2];
    uint16_t addr;

    data[0] = 0x00;
    data[1] = 0x00;
    addr = ((uint16_t)0x46 << 8) | 0x00;

    esp_err_t ret = I2C_Write_Touch(SPD2010_ADDR, addr, data, 2);
    esp_rom_delay_us(200);
    return ret;
}

static esp_err_t write_tp_cpu_start_cmd(void)
{
    uint8_t data[2];
    uint16_t addr;

    data[0] = 0x01;
    data[1] = 0x00;
    addr = ((uint16_t)0x04 << 8) | 0x00;

    esp_err_t ret = I2C_Write_Touch(SPD2010_ADDR, addr, data, 2);
    esp_rom_delay_us(200);
    return ret;
}

static esp_err_t write_tp_clear_int_cmd(void)
{
    uint8_t data[2];
    uint16_t addr;

    data[0] = 0x01;
    data[1] = 0x00;
    addr = ((uint16_t)0x02 << 8) | 0x00;

    esp_err_t ret = I2C_Write_Touch(SPD2010_ADDR, addr, data, 2);
    esp_rom_delay_us(200);
    return ret;
}

static esp_err_t read_tp_status_length(tp_status_t *tp_status)
{
    uint8_t sample_data[4];
    uint16_t addr;

    if (!tp_status) {
        return ESP_ERR_INVALID_ARG;
    }

    addr = ((uint16_t)0x20 << 8) | 0x00;
    esp_err_t ret = I2C_Read_Touch(SPD2010_ADDR, addr, sample_data, 4);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_rom_delay_us(200);

    tp_status->status_low.pt_exist = (sample_data[0] & 0x01);
    tp_status->status_low.gesture = (sample_data[0] & 0x02);
    tp_status->status_low.aux = (sample_data[0] & 0x08);
    tp_status->status_high.tic_busy = (sample_data[1] & 0x80) >> 7;
    tp_status->status_high.tic_in_bios = (sample_data[1] & 0x40) >> 6;
    tp_status->status_high.tic_in_cpu = (sample_data[1] & 0x20) >> 5;
    tp_status->status_high.tint_low = (sample_data[1] & 0x10) >> 4;
    tp_status->status_high.cpu_run = (sample_data[1] & 0x08) >> 3;
    tp_status->read_len = (uint16_t)(sample_data[3] << 8 | sample_data[2]);

    return ESP_OK;
}

static esp_err_t read_tp_hdp(const tp_status_t *tp_status, SPD2010_Touch *touch)
{
    uint8_t sample_data[4 + (10 * 6)]; // 4 字节头 + 最多 10 个点 * 6 字节
    uint8_t i;
    uint8_t offset;
    uint8_t check_id;
    uint16_t addr;

    if (!tp_status || !touch) {
        return ESP_ERR_INVALID_ARG;
    }

    addr = ((uint16_t)0x00 << 8) | 0x03;
    esp_err_t ret = I2C_Read_Touch(SPD2010_ADDR, addr, sample_data, tp_status->read_len);
    if (ret != ESP_OK) {
        return ret;
    }

    check_id = sample_data[4];
    if ((check_id <= 0x0A) && tp_status->status_low.pt_exist) {
        touch->touch_num = (tp_status->read_len - 4) / 6;
        touch->gesture = 0x00;
        for (i = 0; i < touch->touch_num; i++) {
            offset = i * 6;
            touch->rpt[i].id = sample_data[4 + offset];
            touch->rpt[i].x = (uint16_t)(((sample_data[7 + offset] & 0xF0) << 4) | sample_data[5 + offset]);
            touch->rpt[i].y = (uint16_t)(((sample_data[7 + offset] & 0x0F) << 8) | sample_data[6 + offset]);
            touch->rpt[i].weight = sample_data[8 + offset];
        }

        if ((touch->rpt[0].weight != 0) && (touch->down != 1)) {
            touch->down = 1;
            touch->up = 0;
            touch->down_x = touch->rpt[0].x;
            touch->down_y = touch->rpt[0].y;
        } else if ((touch->rpt[0].weight == 0) && (touch->down == 1)) {
            touch->up = 1;
            touch->down = 0;
            touch->up_x = touch->rpt[0].x;
            touch->up_y = touch->rpt[0].y;
        }
    } else if ((check_id == 0xF6) && tp_status->status_low.gesture) {
        touch->touch_num = 0x00;
        touch->up = 0;
        touch->down = 0;
        touch->gesture = sample_data[6] & 0x07;
    } else {
        touch->touch_num = 0x00;
        touch->gesture = 0x00;
    }

    return ESP_OK;
}

static esp_err_t read_tp_hdp_status(tp_hdp_status_t *tp_hdp_status)
{
    uint8_t sample_data[8];
    uint16_t addr;

    if (!tp_hdp_status) {
        return ESP_ERR_INVALID_ARG;
    }

    addr = ((uint16_t)0xFC << 8) | 0x02;
    esp_err_t ret = I2C_Read_Touch(SPD2010_ADDR, addr, sample_data, 8);
    if (ret != ESP_OK) {
        return ret;
    }

    tp_hdp_status->status = sample_data[5];
    tp_hdp_status->next_packet_len = (uint16_t)(sample_data[2] | (sample_data[3] << 8));

    return ESP_OK;
}

static esp_err_t Read_HDP_REMAIN_DATA(const tp_hdp_status_t *tp_hdp_status)
{
    uint8_t sample_data[32];
    uint16_t addr;

    if (!tp_hdp_status) {
        return ESP_ERR_INVALID_ARG;
    }

    addr = ((uint16_t)0x00 << 8) | 0x03;
    return I2C_Read_Touch(SPD2010_ADDR, addr, sample_data, tp_hdp_status->next_packet_len);
}

static esp_err_t read_fw_version(void)
{
    uint8_t sample_data[18];
    uint16_t addr;

    addr = ((uint16_t)0x26 << 8) | 0x00;
    esp_err_t ret = I2C_Read_Touch(SPD2010_ADDR, addr, sample_data, 18);
    if (ret != ESP_OK) {
        return ret;
    }

    uint16_t DVer = (uint16_t)((sample_data[5] << 8) | sample_data[4]);
    uint32_t PID = (uint32_t)((sample_data[9] << 24) | (sample_data[8] << 16) |
                              (sample_data[7] << 8) | sample_data[6]);

    uint32_t ICName_L = (uint32_t)((sample_data[13] << 24) | (sample_data[12] << 16) |
                                   (sample_data[11] << 8) | sample_data[10]);
    uint32_t ICName_H = (uint32_t)((sample_data[17] << 24) | (sample_data[16] << 16) |
                                   (sample_data[15] << 8) | sample_data[14]);

    ESP_LOGI(TAG, "SPD2010 FW: DVer=%d, PID=%lu, Name=%lu-%lu",
             DVer, (unsigned long)PID, (unsigned long)ICName_H, (unsigned long)ICName_L);

    return ESP_OK;
}

static esp_err_t tp_read_data(SPD2010_Touch *touch)
{
    tp_status_t tp_status = {0};
    tp_hdp_status_t tp_hdp_status = {0};

    esp_err_t ret = read_tp_status_length(&tp_status);
    if (ret != ESP_OK) {
        return ret;
    }

    if (tp_status.status_high.tic_in_bios) {
        write_tp_clear_int_cmd();
        write_tp_cpu_start_cmd();
    } else if (tp_status.status_high.tic_in_cpu) {
        write_tp_point_mode_cmd();
        write_tp_start_cmd();
        write_tp_clear_int_cmd();
    } else if (tp_status.status_high.cpu_run && tp_status.read_len == 0) {
        write_tp_clear_int_cmd();
    } else if (tp_status.status_low.pt_exist || tp_status.status_low.gesture) {
        read_tp_hdp(&tp_status, touch);

    hdp_done_check:
        read_tp_hdp_status(&tp_hdp_status);
        if (tp_hdp_status.status == 0x82) {
            write_tp_clear_int_cmd();
        } else if (tp_hdp_status.status == 0x00) {
            Read_HDP_REMAIN_DATA(&tp_hdp_status);
            goto hdp_done_check;
        }
    } else if (tp_status.status_high.cpu_run && tp_status.status_low.aux) {
        write_tp_clear_int_cmd();
    }

    return ESP_OK;
}

/*********************
 * 对外接口实现
 *********************/

// 触摸硬复位（通过 TCA9554 EXIO1，与 temp 示例保持一致）
static void SPD2010_Touch_Reset(void)
{
    Set_EXIO(TCA9554_EXIO1, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    Set_EXIO(TCA9554_EXIO1, true);
    vTaskDelay(pdMS_TO_TICKS(50));
}

esp_err_t Touch_Init_Official(void)
{
    ESP_LOGI(TAG, "开始初始化SPD2010触摸控制器（自定义协议版本）");

    if (I2C_Get_Bus_Handle() == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    SPD2010_Touch_Reset();

    esp_err_t ret = read_fw_version();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "读取SPD2010固件信息失败: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "SPD2010触摸控制器初始化完成");
    return ESP_OK;
}

void Touch_Deinit_Official(void)
{
    ESP_LOGI(TAG, "SPD2010触摸驱动暂不需要特殊反初始化");
}

bool Touch_Get_xy_Official(uint16_t *touch_x, uint16_t *touch_y, uint16_t *strength,
                           uint8_t *touch_count, uint8_t max_points)
{
    if (!touch_x || !touch_y || !touch_count || max_points == 0) {
        return false;
    }

    if (I2C_Get_Bus_Handle() == NULL) {
        return false;
    }

    // 读取一次触摸数据
    esp_err_t ret = tp_read_data(&s_touch_data);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "读取触摸数据失败: %s", esp_err_to_name(ret));
        return false;
    }

    uint8_t cnt = s_touch_data.touch_num;
    if (cnt > max_points) {
        cnt = max_points;
    }

    *touch_count = cnt;

    for (uint8_t i = 0; i < cnt; i++) {
        touch_x[i] = s_touch_data.rpt[i].x;
        touch_y[i] = s_touch_data.rpt[i].y;
        if (strength) {
            strength[i] = s_touch_data.rpt[i].weight;
        }
    }

    // 清除已读数量
    s_touch_data.touch_num = 0;

    if (cnt > 0) {
        ESP_LOGI(TAG, "检测到 %d 个触摸点", cnt);
        for (uint8_t i = 0; i < cnt; i++) {
            ESP_LOGI(TAG, "触摸点 %d: (%d, %d) w=%d", i,
                     touch_x[i], touch_y[i], strength ? strength[i] : 0);
        }
    }

    return (cnt > 0);
}

