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

/** @brief RPM Characteristic UUID. */
#define BT_UUID_MELTYBLE_RPM_VAL \
	BT_UUID_128_ENCODE(0x00001524, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

/** @brief LED Characteristic UUID. */
#define BT_UUID_MELTYBLE_CONFIG_VAL \
	BT_UUID_128_ENCODE(0x00001525, 0x1212, 0xefde, 0x1523, 0x785feabcd123)


#define BT_UUID_MELTYBLE           	BT_UUID_DECLARE_128(BT_UUID_MELTYBLE_VAL)
#define BT_UUID_MELTYBLE_RPM    	BT_UUID_DECLARE_128(BT_UUID_MELTYBLE_RPM_VAL)
#define BT_UUID_MELTYBLE_CONFIG		BT_UUID_DECLARE_128(BT_UUID_MELTYBLE_CONFIG_VAL)

#define TRANSLATE_IDLE 0
#define TRANSLATE_FORWARD 1
#define TRANSLATE_REVERSE 2

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
