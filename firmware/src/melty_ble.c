/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Melty Brain BLE control interface
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>

#include "melty_ble.h"

LOG_MODULE_REGISTER(bt_meltble, 3);

static bool                   notify_enabled;

static bool melty_parameters_initialized = false;

static float radius;
static u_int8_t led_offset;
static u_int8_t throttle;
static u_int8_t translate_direction;
static u_int8_t heart_beat;

static void melty_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				  uint16_t value)
{
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static ssize_t update_melty_config(struct bt_conn *conn,
			 const struct bt_gatt_attr *attr,
			 const void *buf,
			 uint16_t len, uint16_t offset, uint8_t flags)
{
	LOG_DBG("Attribute write, handle: %u, conn: %p", attr->handle,
		(void *)conn);

	if (len != 7U) {
		LOG_DBG("Write led: Incorrect data length");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if (offset != 0) {
		LOG_DBG("Write led: Incorrect data offset");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

    melty_parameters_initialized = false;

    radius = (((uint8_t *)buf)[0] + ((uint8_t *)buf)[1] * 256) / 1000.0f;
    led_offset = ((uint8_t *)buf)[2];
    throttle = ((uint8_t *)buf)[3];
    translate_direction = ((int8_t *)buf)[4];
    heart_beat = ((int8_t *)buf)[5];
    //byte 6 reserved

    melty_parameters_initialized = true;
    LOG_DBG("params updated");

	return len;
}

void clear_melty_parameters_initialized(void) {
    melty_parameters_initialized = false;
}

bool get_melty_parameters_initialized(void) {
    return melty_parameters_initialized;
}

float get_radius(void) {
    return radius;
}


u_int8_t get_heart_beat(void) {
    return heart_beat;
}

u_int8_t get_translate_direction(void) {
    return translate_direction;
}

u_int8_t get_throttle(void) {
    return throttle;
}

u_int8_t get_led_offset(void) {
    return led_offset;
}


/* MeltyBLE Service Declaration */
BT_GATT_SERVICE_DEFINE(meltyble_svc,
BT_GATT_PRIMARY_SERVICE(BT_UUID_MELTYBLE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MELTYBLE_STATS,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, NULL, NULL, NULL),
	BT_GATT_CCC(melty_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MELTYBLE_CONFIG,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL, update_melty_config, NULL),
);

int bt_melty_init(void)
{
	//nothing to init / placeholder
	return 0;
}

int bt_send_melty_stats(u_int8_t melty_stats[3])
{
	if (!notify_enabled) {
		return -EACCES;
	}

	return bt_gatt_notify(NULL, &meltyble_svc.attrs[2],
			      melty_stats,
			      3);
}
