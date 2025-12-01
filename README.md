# ESP32 Lottie 动画播放器

基于 ESP32-S3 的 Lottie 动画播放器，支持在 SPD2010 显示屏上流畅播放 Lottie 动画。

## ✨ 特性

- 🎨 **Lottie 动画支持** - 播放 JSON 格式的 Lottie 动画
- 📱 **高性能显示** - 412x412 分辨率，QSPI 接口，60MHz 时钟
- 👆 **触摸支持** - 支持 SPD2010 触摸屏，最多 5 点触控
- 🎯 **模块化架构** - 分层设计，易于移植和维护
- 💾 **SPIFFS 存储** - 动画资源存储在 SPIFFS 分区
- ⚡ **硬件加速** - SPI DMA 传输，硬件完成回调

## 🏗️ 架构设计

```
┌─────────────────────────────────────┐
│      应用层 (main)                   │
│  - 初始化管理                        │
│  - 动画播放控制                      │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│  Lottie管理层 (xn_lottie_manager)   │
│  - 动画资源管理                      │
│  - 动画播放控制                      │
│  - SPIFFS文件系统                    │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│  LVGL驱动层 (xn_lvgl_driver)        │
│  - LVGL适配                          │
│  - 显示/触摸驱动                     │
│  - 任务管理                          │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│  BSP硬件层 (xn_bsp_spd2010)         │
│  - I2C驱动                           │
│  - LCD面板驱动                       │
│  - 触摸屏驱动                        │
│  - IO扩展驱动                        │
└─────────────────────────────────────┘
```

详细架构说明请查看 [组件架构文档](components/README.md)

## 📋 硬件要求

### 主控
- **MCU**: ESP32-S3
- **Flash**: 至少 4MB
- **PSRAM**: 8MB (必需)

### 显示屏
- **型号**: SPD2010
- **分辨率**: 412x412
- **接口**: QSPI (4线SPI)
- **颜色**: RGB565

### 触摸屏
- **型号**: SPD2010
- **接口**: I2C
- **触控点**: 最多5点

### 引脚连接

| 功能 | 引脚 | 说明 |
|------|------|------|
| SPI SCK | GPIO40 | SPI时钟 |
| SPI D0 | GPIO46 | QSPI数据线0 |
| SPI D1 | GPIO45 | QSPI数据线1 |
| SPI D2 | GPIO42 | QSPI数据线2 |
| SPI D3 | GPIO41 | QSPI数据线3 |
| SPI CS | GPIO21 | SPI片选 |
| I2C SDA | GPIO11 | I2C数据线 |
| I2C SCL | GPIO10 | I2C时钟线 |
| 背光PWM | GPIO5 | 背光控制 |

## 🚀 快速开始

### 环境准备

1. 安装 ESP-IDF v5.5
```bash
# 参考 ESP-IDF 官方文档安装
https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/
```

2. 克隆项目
```bash
git clone https://github.com/yourusername/xn_esp32_lottie.git
cd xn_esp32_lottie
```

### 编译和烧录

1. 配置项目
```bash
idf.py menuconfig
```

2. 编译项目
```bash
idf.py build
```

3. 烧录到设备
```bash
idf.py flash
```

4. 查看日志
```bash
idf.py monitor
```

或者一步完成：
```bash
idf.py build flash monitor
```

### 添加自定义动画

1. 将 Lottie JSON 文件放到 `components/xn_lottie_manager/lottie_spiffs/` 目录

2. 在 `xn_lottie_manager.h` 中定义动画类型：
```c
#define LOTTIE_ANIM_MY_ANIM  7
```

3. 在 `xn_lottie_manager.c` 中配置动画：
```c
static const lottie_anim_config_t anim_configs[] = {
    [LOTTIE_ANIM_MY_ANIM] = {"/lottie/my_anim.json", 200, 200},
};
```

4. 播放动画：
```c
lottie_manager_play_anim(LOTTIE_ANIM_MY_ANIM);
```

## 📖 API 使用

### 初始化

```c
#include "xn_lottie_manager.h"

void app_main(void)
{
    // 初始化 Lottie 管理器（包括 LVGL、显示、触摸）
    esp_err_t ret = xn_lottie_manager_init(NULL);
    if (ret == ESP_OK) {
        // 初始化成功
    }
}
```

### 播放动画

```c
// 播放预定义的动画
lottie_manager_play_anim(LOTTIE_ANIM_LOADING);

// 播放自定义路径的动画
lottie_manager_play("/lottie/custom.json", 200, 200);

// 播放动画并设置位置
lottie_manager_play_anim_at_pos(LOTTIE_ANIM_LOADING, 100, 100);
```

### 控制动画

```c
// 停止动画
lottie_manager_stop();

// 隐藏动画
lottie_manager_hide();

// 显示动画
lottie_manager_show();

// 设置动画位置
lottie_manager_set_pos(100, 100);

// 居中显示
lottie_manager_center();
```

### 显示图片

```c
// 显示 PNG/JPG 图片
lottie_manager_show_image("/images/logo.png", 200, 200);

// 隐藏图片
lottie_manager_hide_image();
```

## 🔧 配置说明

### LVGL 配置

在 `components/xn_lvgl_driver/include/xn_lvgl.h` 中：

```c
// LVGL Tick 周期 (ms)
#define LVGL_TICK_PERIOD_MS     100

// 显示缓冲区大小（像素数）
#define LVGL_BUFFER_SIZE        (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT / 20)
```

### 显示配置

在 `components/xn_bsp_spd2010/include/bsp_panel_spd2010.h` 中：

```c
// 显示屏分辨率
#define EXAMPLE_LCD_WIDTH       (412)
#define EXAMPLE_LCD_HEIGHT      (412)

// SPI 时钟频率
#define ESP_PANEL_LCD_SPI_CLK_HZ    (60 * 1000 * 1000)  // 60MHz
```

## 🔄 硬件移植

如果要移植到其他显示屏（如 ST7789、ILI9341），请参考：

1. [组件架构文档](components/README.md)
2. [BSP 驱动说明](components/xn_bsp_spd2010/README.md)
3. [LVGL 驱动说明](components/xn_lvgl_driver/README.md)

### 移植步骤

1. 复制 BSP 组件
```bash
cp -r components/xn_bsp_spd2010 components/xn_bsp_st7789
```

2. 修改驱动实现（适配新的显示屏）

3. 更新 LVGL 驱动依赖
```cmake
# components/xn_lvgl_driver/CMakeLists.txt
REQUIRES
    lvgl
    xn_bsp_st7789  # 改为新的 BSP 组件
```

4. 重新编译测试

## 📊 性能指标

- **显示刷新率**: 约 30-60 FPS（取决于动画复杂度）
- **SPI 传输速度**: 60MHz QSPI
- **内存使用**: 
  - LVGL 缓冲区: ~34KB (PSRAM)
  - LVGL 任务栈: 64KB (PSRAM)
  - Lottie 任务栈: 350KB (PSRAM)
- **CPU 占用**: 
  - LVGL 任务: Core 1, 优先级 7
  - Lottie 任务: Core 0, 优先级 5

## 🐛 故障排除

### 显示花屏

1. 检查 RGB 字节序交换是否正确
2. 检查颜色顺序（RGB vs BGR）
3. 检查 SPI 时钟频率是否过高

### 触摸不响应

1. 检查 I2C 总线频率
2. 检查触摸屏初始化日志
3. 启用触摸调试日志

### 编译错误

1. 确保 ESP-IDF 版本为 v5.5
2. 运行 `idf.py reconfigure` 下载依赖
3. 清理构建：`idf.py fullclean`

## 📝 依赖组件

- **ESP-IDF**: v5.5
- **LVGL**: ^9.2.0
- **esp_lcd_spd2010**: ^1.0.0
- **esp_lcd_touch_spd2010**: ^1.0.0

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

MIT License

## 👨‍💻 作者

星年 (jixingnian@gmail.com)

## 🙏 致谢

- ESP-IDF 团队
- LVGL 项目
- Lottie 动画库

---

**注意**: 本项目仅供学习和参考使用。
