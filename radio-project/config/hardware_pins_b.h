#pragma once

// ESP32S3_2 = board B. User-supplied wiring, received 2026-11-25.
// UART, GPIO35..37 memory use and GPIO45 strapping still require verification.
#define RADIO_DISPLAY_CONFIGURED 0

#define RADIO_PIN_SD_CS 10
#define RADIO_PIN_SD_MOSI 11
#define RADIO_PIN_SD_SCK 12
#define RADIO_PIN_SD_MISO 13

#define RADIO_PIN_PANEL_R1 42
#define RADIO_PIN_PANEL_G1 41
#define RADIO_PIN_PANEL_B1 40
#define RADIO_PIN_PANEL_R2 38
#define RADIO_PIN_PANEL_G2 39
#define RADIO_PIN_PANEL_B2 37
#define RADIO_PIN_PANEL_E 21
#define RADIO_PIN_PANEL_A 45
#define RADIO_PIN_PANEL_B 36
#define RADIO_PIN_PANEL_C 48
#define RADIO_PIN_PANEL_D 35
#define RADIO_PIN_PANEL_CLK 2
#define RADIO_PIN_PANEL_LAT 47
#define RADIO_PIN_PANEL_OE 14

#define RADIO_PIN_USB_DM 19
#define RADIO_PIN_USB_DP 20
#define RADIO_PIN_BOOT 0
// EN/CHIP_PU is a dedicated reset input, not a GPIO.
// BOOT and EN: button to GND, pull-up to 3V3, 100 nF to GND (as supplied).
// Pull-up resistance and EN power-up timing remain to be verified.

// RADIO_PIN_LINK_TX / RADIO_PIN_LINK_RX: not supplied, intentionally undefined.
// RADIO_OCTAL_MEMORY_PRESENT: 0 or 1, only after checking the actual module.
// RADIO_GPIO45_STRAP_VERIFIED: 1 only after checking reset-time voltage/loading.
