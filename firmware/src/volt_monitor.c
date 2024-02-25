#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <soc.h>
#include <device.h>
#include <math.h>

#include "volt_monitor.h"
#include "analog_in.h"

//AIN05 = pin 29
#define BATTERY_V_ADC_CHANNEL 5	

#define BATTERY_ADC_READS 1

#define BATTERY_VOLTAGE_DIVIDER_RATIO 11.1f	//For example - 11k to V+ and to 1k to GND

static float battery_voltage = 0.0f;

struct k_mutex volt_monitor_mutex;


float adc_multi_sample(int samples, int adc_channel) {
	float multi_sample = 0;
    for (int loop = 0; loop < samples; loop ++) {
		multi_sample += AnalogRead(adc_channel);
	}
    return multi_sample / samples;
}


void update_battery_voltage(void) {
	float current_voltage = adc_multi_sample(BATTERY_ADC_READS, BATTERY_V_ADC_CHANNEL);
	
	k_mutex_lock(&volt_monitor_mutex, K_FOREVER);
	if (battery_voltage == 0) battery_voltage = current_voltage;
	battery_voltage = (battery_voltage * 0.8f) + (current_voltage * BATTERY_VOLTAGE_DIVIDER_RATIO) * .2f;
	k_mutex_unlock(&volt_monitor_mutex);
}

float get_battery_voltage(void) {
	k_mutex_lock(&volt_monitor_mutex, K_FOREVER);
	float voltage = battery_voltage;
	k_mutex_unlock(&volt_monitor_mutex);

	return voltage;
}
