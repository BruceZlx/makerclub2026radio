#pragma once

// ESP32S3_1 = board A. User-supplied wiring, received 2026-11-25.
// UART TX/RX and the exact module are still unknown; do not enable yet.
#define RADIO_HARDWARE_CONFIGURED 0

#define RADIO_PIN_AUDIO_BCLK 4
#define RADIO_PIN_AUDIO_LRCLK 5
#define RADIO_PIN_AUDIO_DOUT 6

#define RADIO_PIN_ENCODER_A 7
#define RADIO_PIN_ENCODER_B 8
#define RADIO_PIN_ENCODER_SW 9

#define RADIO_PIN_SD_CS 10
#define RADIO_PIN_SD_MOSI 11
#define RADIO_PIN_SD_SCK 12
#define RADIO_PIN_SD_MISO 13

#define RADIO_PIN_USB_DM 19
#define RADIO_PIN_USB_DP 20
#define RADIO_PIN_BOOT 0
// EN/CHIP_PU is a dedicated reset input, not a GPIO.
// BOOT and EN: button to GND, pull-up to 3V3, 100 nF to GND (as supplied).
// Pull-up resistance and EN power-up timing remain to be verified.

// RADIO_PIN_LINK_TX / RADIO_PIN_LINK_RX: not supplied, intentionally undefined.
