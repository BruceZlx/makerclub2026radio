#pragma once

// 配置方法（2026-11-25）：不要再让两板共用 hardware_pins.local.h。
// 板 A：复制 hardware_pins_a.h 为 hardware_pins_a.local.h。
// 板 B：复制 hardware_pins_b.h 为 hardware_pins_b.local.h。
// 每份 local 文件是完整的单板配置，不是 #include 或增量覆盖。
// 也可用 build_firmware.py / check_hardware.py 的 --pins 指定完整文件。
//
// 已收到的外设接线在对应共享头文件中，不是猜测的默认引脚。
// 两板仍缺 RADIO_PIN_LINK_TX / RADIO_PIN_LINK_RX，不要凭空填写。
// 板 B 使用 GPIO35/36/37：核对实物后填写 RADIO_OCTAL_MEMORY_PRESENT 0 或 1。
// 如果为 1，必须重新分配与八线存储器冲突的屏幕引脚，不能仅关闭 PSRAM 选项。
// GPIO45 用作屏幕 A：核对 VDD_SPI 启动电平后填写 RADIO_GPIO45_STRAP_VERIFIED 1。
// 完成模组、UART 与电气核对后，才能将该板 CONFIGURED 标记设为 1。
//
// 文件只支持 #pragma once 和十进制整数 #define，允许行注释与块注释。
// 不支持表达式、别名、#if 或 #include，避免校验值与编译值不一致。
// EN 是专用复位端，不定义 RADIO_PIN_RESET，也不为其编造 GPIO。
// 完整接线与评审结论见 docs/hardware-io-review.md。
