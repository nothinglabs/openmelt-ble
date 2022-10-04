/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MELTY_BLE_H_
#define MELTY_BLE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/types.h>

/** @brief MELTYBLE Service UUID. */
#define BT_UUID_MELTYBLE_VAL \
	BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

/** @brief Melty Stats Characteristic UUID. */
#define BT_UUID_MELTYBLE_STATS_VAL \
	BT_UUID_128_ENCODE(0x00001524, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

//BT_UUID_MELTYBLE_STATS
//3 bytes of data sent to client to indicate RPM and battery voltage
//[0] Current rotation interval in MS (convert to RPM on client) - value of 0 should be handled as 0 RPM
//[1] Reserved
//[2] Current battery voltage * 10

/** @brief Melty Config Characteristic UUID. */
#define BT_UUID_MELTYBLE_CONFIG_VAL \
	BT_UUID_128_ENCODE(0x00001525, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

//BT_UUID_MELTYBLE_CONFIG
//7 bytes of data sent from BLE client to configure melty bot
//2-byte (unsigned) radius value is provided as radius in centimeters * 1000
// Dynamic adjustment of radius is used to control steering
// [0] Radius Least Signicant Byte
// [1] Radius Most Signficant Byte
// [2] LED_offset 0-99 value corresponding to offset % LED beacon is located (adjusts heading)
// [3] Throttle 0-100 value corresponding % of each rotation wheel(s) are powered
	//0 = off, 100 = fully on (no translation)
// [4] Translate direction (idle, forward or reverse)
// [5] Heartbeat value
// [6] Reserved 

#define TRANSLATE_IDLE 0
#define TRANSLATE_FORWARD 1
#define TRANSLATE_REVERSE 2

#define BT_UUID_MELTYBLE           	BT_UUID_DECLARE_128(BT_UUID_MELTYBLE_VAL)
#define BT_UUID_MELTYBLE_STATS    	BT_UUID_DECLARE_128(BT_UUID_MELTYBLE_STATS_VAL)
#define BT_UUID_MELTYBLE_CONFIG		BT_UUID_DECLARE_128(BT_UUID_MELTYBLE_CONFIG_VAL)


int bt_melty_init(void);

int bt_send_melty_stats(u_int8_t melty_stats[3]);

float get_radius(void);

bool get_melty_parameters_initialized(void);

void clear_melty_parameters_initialized(void);

u_int8_t get_translate_direction(void);

u_int8_t get_heart_beat(void);

u_int8_t get_throttle(void);

u_int8_t get_led_offset(void);

#ifdef __cplusplus
}
#endif



#endif 
