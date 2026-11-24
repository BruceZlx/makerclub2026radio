#pragma once

// 硬件引脚模板：只列出配置项名称，不提供任何 GPIO 默认值。
//
// 使用方法：复制为 config/hardware_pins.local.h，由硬件组逐项填写实际
// 模组、Flash/PSRAM、屏幕与 IO 分配。local 文件已被 .gitignore 排除。
// 任何 RADIO_PIN_* 未定义、非数字或超出 0..48 时，硬件构建必须失败。
//
// 板 A 必填：
//   RADIO_PIN_AUDIO_BCLK
//   RADIO_PIN_AUDIO_LRCLK
//   RADIO_PIN_AUDIO_DOUT
//   RADIO_PIN_ENCODER_A
//   RADIO_PIN_ENCODER_B
//   RADIO_PIN_ENCODER_SW
//   RADIO_PIN_LINK_TX
//   RADIO_PIN_LINK_RX
//   RADIO_PIN_SD_SCK
//   RADIO_PIN_SD_MISO
//   RADIO_PIN_SD_MOSI
//   RADIO_PIN_SD_CS
//   RADIO_HARDWARE_CONFIGURED 1
//
// 板 B 必填：
//   RADIO_PIN_LINK_TX
//   RADIO_PIN_LINK_RX
//   RADIO_PIN_PANEL_R1
//   RADIO_PIN_PANEL_G1
//   RADIO_PIN_PANEL_B1
//   RADIO_PIN_PANEL_R2
//   RADIO_PIN_PANEL_G2
//   RADIO_PIN_PANEL_B2
//   RADIO_PIN_PANEL_CLK
//   RADIO_PIN_PANEL_LAT
//   RADIO_PIN_PANEL_OE
//   RADIO_PIN_PANEL_A
//   RADIO_PIN_PANEL_B
//   RADIO_PIN_PANEL_C
//   RADIO_PIN_PANEL_D
//   RADIO_PIN_PANEL_E
//   RADIO_DISPLAY_CONFIGURED 1
//
// 说明：GPIO 编号本身不是默认值；只有硬件组确认后才能写入 local 文件。
// 本模板故意不定义任何宏。
