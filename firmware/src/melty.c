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

#include "melty.h"
#include "melty_ble.h"
#include "analog_in.h"

#define MELTY_LED_PIN			13
#define MOTOR_PIN1				4
#define MOTOR_PIN2				3

//AIN04 = pin 28
#define ACCEL_ADC_CHANNEL 4	

//AIN05 = pin 29
#define BATTERY_V_ADC_CHANNEL 5	

#define BATTERY_VOLTAGE_DIVIDER_RATIO 11.0f	//For example - 11k to V+ and to 1k to GND

//full power spin in below this number
#define MIN_TRANSLATION_RPM                    250
#define MAX_TRANSLATION_ROTATION_INTERVAL_US   (1.0f / MIN_TRANSLATION_RPM) * 60 * 1000 * 1000

//don't even try to do heading track if we are this slow
//limits max time spent in do_melty (helps assure heartbeat is checked at safe interval)
#define MAX_TRACKING_ROTATION_INTERVAL_US   MAX_TRANSLATION_ROTATION_INTERVAL_US * 2

//moving average for accel (.98f = 98% of prior value is kept for new reading)
#define ACCEL_SMOOTHING_FACTOR     .98f

//use negative if need to invert (to accout for accel orientation)
#define G_PER_ADC   -.008f

//number of reads per ADC sample during translation (in addition to any oversampling)
#define ADC_READS   1

//number of reads for battery voltage
#define BATTERY_ADC_READS   1

//times to sample ADC for initial "0g" read
#define INIT_ADC_READS   200


static const struct device *dev;

static float zero_g_accel;

float adc_multi_sample(int samples, int adc_channel) {
	float multi_sample = 0;
    for (int loop = 0; loop < samples; loop ++) {
		multi_sample += AnalogRead(adc_channel);
	}
    return multi_sample / samples;
}

void init_melty(void){

	dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

	gpio_pin_configure(dev, MELTY_LED_PIN, GPIO_OUTPUT); 
	gpio_pin_configure(dev, MOTOR_PIN1, GPIO_OUTPUT); 
	gpio_pin_configure(dev, MOTOR_PIN2, GPIO_OUTPUT); 

	zero_g_accel = adc_multi_sample(INIT_ADC_READS, ACCEL_ADC_CHANNEL);
}

void motors_safe(void) {
    //motor off!
	gpio_pin_set(dev, MOTOR_PIN1, 0);
	gpio_pin_set(dev, MOTOR_PIN2, 0);
}


static float get_accel_force(void){

	float relative_adc_read = adc_multi_sample(ADC_READS, ACCEL_ADC_CHANNEL) - zero_g_accel;
	
	float g_force = relative_adc_read / G_PER_ADC;
	if (g_force < 0) g_force = 0;

	//test!
	return 15;
	return g_force;
}

static float get_smoothed_accel_force(void){

    static float smoothed_accel = 0;

    if (smoothed_accel == 0) {
        smoothed_accel = get_accel_force();
    } else {
        smoothed_accel = (smoothed_accel * ACCEL_SMOOTHING_FACTOR) + (get_accel_force() * (1.0f - ACCEL_SMOOTHING_FACTOR));
    }

	return smoothed_accel;
}

static float get_rotation_interval_ms(void){

	//increasing causes tracking speed to decrease
	float radius_in_cm = get_radius();

 	//calculate RPM from g's - derived from "G = 0.00001118 * r * RPM^2"
	float rpm;
	rpm = get_smoothed_accel_force() * 89445.0f;                               
	rpm = rpm / radius_in_cm;
	rpm = sqrt(rpm);	

	float rotation_interval = (1.0f / rpm) * 60 * 1000;
	if (rotation_interval > 250) rotation_interval = 250;
	if (rotation_interval < 0) rotation_interval = 250;
	return rotation_interval;
}


float get_battery_voltage(void) {
	float voltage = adc_multi_sample(BATTERY_ADC_READS, BATTERY_V_ADC_CHANNEL);
	return voltage * BATTERY_VOLTAGE_DIVIDER_RATIO;
}

static struct melty_parameters_t get_melty_parameters(void) {

	float led_offset_portion = get_led_offset() / 100.0f;
	float motor_on_portion = get_throttle() / 100.0f;	
	float led_on_portion = .4f * (1.1f - motor_on_portion);     //LED width changed with throttle

	struct melty_parameters_t melty_parameters;
	melty_parameters.rotation_interval_us = get_rotation_interval_ms() * 1000;

	//if under defined RPM - just try to spin up
    if (melty_parameters.rotation_interval_us > MAX_TRANSLATION_ROTATION_INTERVAL_US) motor_on_portion = 1;

    //if we are too slow - don't even try to track heading
	if (melty_parameters.rotation_interval_us > MAX_TRACKING_ROTATION_INTERVAL_US) {
        melty_parameters.rotation_interval_us = MAX_TRACKING_ROTATION_INTERVAL_US;
    }

	u_int32_t motor_on_us = motor_on_portion * melty_parameters.rotation_interval_us;
	
    u_int32_t led_on_us = led_on_portion * melty_parameters.rotation_interval_us;
	u_int32_t led_offset_us = led_offset_portion * melty_parameters.rotation_interval_us;

    //center LED on offset
	if (led_on_us / 2 <= led_offset_us) {
        melty_parameters.led_start = led_offset_us - (led_on_us / 2);
    } else {
       melty_parameters.led_start =  (melty_parameters.rotation_interval_us + led_offset_us) - (led_on_us / 2); 
    }

 	melty_parameters.led_stop = melty_parameters.led_start + led_on_us;
	
	if (melty_parameters.led_stop > melty_parameters.rotation_interval_us)
		melty_parameters.led_stop = melty_parameters.led_stop - melty_parameters.rotation_interval_us;

	melty_parameters.motor_start1 = (melty_parameters.rotation_interval_us - motor_on_us) / 2;
	melty_parameters.motor_stop1 = melty_parameters.motor_start1 + motor_on_us;

	melty_parameters.motor_start2 = melty_parameters.rotation_interval_us - (motor_on_us / 2);
	melty_parameters.motor_stop2 = motor_on_us / 2;

	return melty_parameters;

}

void update_melty_stats(int rotation_interval_ms, float battery_voltage) {
	u_int8_t melty_stats[3] = {0, 0, 0};
	melty_stats[0] = rotation_interval_ms;
	melty_stats[2] = battery_voltage * 10.0f;
	bt_send_melty_stats(melty_stats);
}

void do_melty(void){
		
	u_int32_t time_spent_this_rotation_us = 0;

	int sleep_time_us = 10;

	static u_int32_t cycle_count = 0;

	/* capture initial time stamp */
	u_int32_t start_time;
	start_time = k_cycle_get_32();

	struct melty_parameters_t melty_parameters = get_melty_parameters();

	update_melty_stats(melty_parameters.rotation_interval_us / 1000, get_battery_voltage());

	cycle_count++;

	while(time_spent_this_rotation_us < melty_parameters.rotation_interval_us) {
		u_int32_t stop_time;
		int64_t cycles_spent;

        melty_parameters = get_melty_parameters();

		//assures BLE gets time to do it's thing
		k_sleep(K_USEC(sleep_time_us));

		if (get_translate_direction() == TRANSLATE_FORWARD || (get_translate_direction() == TRANSLATE_IDLE && cycle_count % 2 == 0)) {
			if (time_spent_this_rotation_us >= melty_parameters.motor_start1 && 
				time_spent_this_rotation_us <= melty_parameters.motor_stop1) {
					gpio_pin_set(dev, MOTOR_PIN1, 1);
			} else {
					gpio_pin_set(dev, MOTOR_PIN1, 0);
			}

			if (time_spent_this_rotation_us >= melty_parameters.motor_start2 || 
				time_spent_this_rotation_us <= melty_parameters.motor_stop2) {
					gpio_pin_set(dev, MOTOR_PIN2, 1);
			} else {
					gpio_pin_set(dev, MOTOR_PIN2, 0);
			}

		}

		if (get_translate_direction() == TRANSLATE_REVERSE || (get_translate_direction() == TRANSLATE_IDLE && cycle_count %2 == 1)) {
			if (time_spent_this_rotation_us >= melty_parameters.motor_start2 || 
				time_spent_this_rotation_us <= melty_parameters.motor_stop2) {
					gpio_pin_set(dev, MOTOR_PIN1, 1);
			} else {
					gpio_pin_set(dev, MOTOR_PIN1, 0);
			}

		if (time_spent_this_rotation_us >= melty_parameters.motor_start1 && 
				time_spent_this_rotation_us <= melty_parameters.motor_stop1) {
					gpio_pin_set(dev, MOTOR_PIN2, 1);
			} else {
					gpio_pin_set(dev, MOTOR_PIN2, 0);
			}

		}
		
		if (melty_parameters.led_start > melty_parameters.led_stop) {
    		if (time_spent_this_rotation_us >= melty_parameters.led_start || time_spent_this_rotation_us <= melty_parameters.led_stop) {
				gpio_pin_set(dev, MELTY_LED_PIN, 1);
			} else {
				gpio_pin_set(dev, MELTY_LED_PIN, 0);
			}
		} else {
			if (time_spent_this_rotation_us >= melty_parameters.led_start && time_spent_this_rotation_us <= melty_parameters.led_stop) {
				gpio_pin_set(dev, MELTY_LED_PIN, 1);
			} else {
				gpio_pin_set(dev, MELTY_LED_PIN, 0);
			}
		}
		
		/* capture final time stamp */
		stop_time = k_cycle_get_32();

		cycles_spent = stop_time - start_time;
		if (cycles_spent < 0) cycles_spent = cycles_spent + UINT32_MAX;
		time_spent_this_rotation_us += SYS_CLOCK_HW_CYCLES_TO_NS_AVG(cycles_spent, 1) / 1000;
		start_time = k_cycle_get_32();
	}

}

void status_led_flash(int connected) {

	gpio_pin_set(dev, MELTY_LED_PIN, 0);
	
    //do accel dependent flash if not connected (provides easy way to verify accelerometer is working)
	//fast flash if connected
    if (connected == 0) {
        k_sleep(K_MSEC(200));
        int on_time = 1 + (int)(get_accel_force() * 50.0f);

        if (on_time > 0) {
            gpio_pin_set(dev, MELTY_LED_PIN, 1);
            k_sleep(K_MSEC(on_time));
        }
    } else {
        k_sleep(K_MSEC(50));
        gpio_pin_set(dev, MELTY_LED_PIN, 1);
        k_sleep(K_MSEC(50));
    }

}
