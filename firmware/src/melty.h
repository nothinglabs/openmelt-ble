#ifndef MELTY_CONTROL_H_

#define MELTY_CONTROL_H_

void do_melty(void);
void init_melty(void);
void motors_safe(void);
void status_led_flash(int connected);

void update_melty_stats(int rotation_interval_ms, float battery_voltage);

float get_battery_voltage(void);

typedef struct melty_parameters_t {
	u_int32_t rotation_interval_us;
	u_int32_t led_start;
	u_int32_t led_stop;
	u_int32_t motor_start1;
	u_int32_t motor_stop1;
	u_int32_t motor_start2;
	u_int32_t motor_stop2;
};

#endif
